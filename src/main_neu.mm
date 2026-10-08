#import <Foundation/Foundation.h>

#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>
#else
#import <UIKit/UIKit.h>
#endif

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <GameController/GameController.h>

#include <vector>
#include <algorithm>
#include <stdio.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_metal4.h"

#if TARGET_OS_OSX
#include "imgui_impl_osx.h"
@interface AppViewController : NSViewController <NSWindowDelegate>
@end
#else
@interface AppViewController : UIViewController
@end
#endif

#define FRAMES_IN_FLIGHT 2

//-----------------------------------------------------------------------------------
// K-Line Data & Neumorphism Helpers
//-----------------------------------------------------------------------------------

struct Candle {
  float open;
  float high;
  float low;
  float close;
};

namespace Neumorphism {
  // ImU32 色彩定义 (用于 ImDrawList 绘制几何体)
  const ImU32 COLOR_BG       = IM_COL32(230, 235, 240, 255); // #E6EBF0
  const ImU32 COLOR_LIGHT    = IM_COL32(255, 255, 255, 255);
  const ImU32 COLOR_SHADOW   = IM_COL32(180, 190, 205, 255);
  const ImU32 COLOR_TEXT     = IM_COL32(80, 90, 105, 255);
  const ImU32 COLOR_TEXT_DIM = IM_COL32(140, 150, 165, 255);
  const ImU32 COLOR_BULL     = IM_COL32(38, 166, 154, 255);  // 薄荷绿 #26A69A
  const ImU32 COLOR_BEAR     = IM_COL32(239, 83, 80, 255);   // 珊瑚红 #EF5350

  // ImVec4 色彩定义 (用于 ImGui::TextColored 等控件)
  const ImVec4 VEC4_TEXT     = ImVec4(80.0f / 255.0f, 90.0f / 255.0f, 105.0f / 255.0f, 1.0f);
  const ImVec4 VEC4_TEXT_DIM = ImVec4(140.0f / 255.0f, 150.0f / 255.0f, 165.0f / 255.0f, 1.0f);
  const ImVec4 VEC4_BULL     = ImVec4(38.0f / 255.0f, 166.0f / 255.0f, 154.0f / 255.0f, 1.0f);
  const ImVec4 VEC4_BEAR     = ImVec4(239.0f / 255.0f, 83.0f / 255.0f, 80.0f / 255.0f, 1.0f);

  // 模拟外投影 (多层 Alpha 衰减)
  void DrawSoftShadow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding,
                      ImVec2 offset, ImU32 shadow_col, float blur_radius, int passes = 6) {
    float alpha_base = ImGui::ColorConvertU32ToFloat4(shadow_col).w;

    for (int i = passes; i >= 1; --i) {
      float progress = (float)i / (float)passes;
      float current_spread = blur_radius * progress;
      float current_alpha = (alpha_base / passes) * (1.0f - progress * 0.4f);

      ImVec4 col_vec = ImGui::ColorConvertU32ToFloat4(shadow_col);
      col_vec.w = current_alpha;
      ImU32 pass_col = ImGui::ColorConvertFloat4ToU32(col_vec);

      ImVec2 p_min = ImVec2(min.x + offset.x - current_spread, min.y + offset.y - current_spread);
      ImVec2 p_max = ImVec2(max.x + offset.x + current_spread, max.y + offset.y + current_spread);

      dl->AddRectFilled(p_min, p_max, pass_col, rounding + current_spread);
    }
  }

  // 绘制拟物表面 (支持凸起 Elevated 与 凹陷 Inset)
  void DrawNeumorphicSurface(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, bool is_pressed) {
    float shadow_dist = 6.0f;
    float blur = 8.0f;

    if (!is_pressed) {
      // 凸起态 (外阴影)
      DrawSoftShadow(dl, min, max, rounding, ImVec2(shadow_dist, shadow_dist), 
                     IM_COL32(170, 182, 198, 140), blur);
      DrawSoftShadow(dl, min, max, rounding, ImVec2(-shadow_dist, -shadow_dist), 
                     IM_COL32(255, 255, 255, 220), blur);
      dl->AddRectFilled(min, max, COLOR_BG, rounding);
    } else {
      // 凹陷态 (内阴影底槽)
      ImU32 inset_bg = IM_COL32(222, 227, 234, 255);
      dl->AddRectFilled(min, max, inset_bg, rounding);

      dl->PushClipRect(min, max, true);
      dl->AddRect(ImVec2(min.x - 2, min.y - 2), ImVec2(max.x + 2, max.y + 2), 
                  IM_COL32(160, 175, 190, 180), rounding, 0, 4.0f);
      dl->AddRect(ImVec2(min.x + 2, min.y + 2), ImVec2(max.x + 4, max.y + 4), 
                  IM_COL32(255, 255, 255, 160), rounding, 0, 4.0f);
      dl->PopClipRect();
    }
  }

  // 拟物风格按钮
  bool Button(const char* label, ImVec2 size, float rounding = 12.0f) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    const ImVec2 pos = window->DC.CursorPos;

    if (size.x <= 0) size.x = ImGui::CalcTextSize(label).x + 40.0f;
    if (size.y <= 0) size.y = ImGui::GetFrameHeight() * 1.5f;

    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;
    DrawNeumorphicSurface(dl, bb.Min, bb.Max, rounding, held);

    ImVec2 text_size = ImGui::CalcTextSize(label);
    ImVec2 text_pos = ImVec2(
      bb.Min.x + (size.x - text_size.x) * 0.5f,
      bb.Min.y + (size.y - text_size.y) * 0.5f + (held ? 1.0f : 0.0f)
    );
    dl->AddText(text_pos, COLOR_TEXT, label);

    return pressed;
  }

  // 拟物风格 K 线图绘制组件
  void DrawKChart(const char* label, const std::vector<Candle>& data, ImVec2 size) {
    if (data.empty()) return;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    // 占位
    ImGui::InvisibleButton(label, size);
    const bool is_hovered = ImGui::IsItemHovered();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. 绘制内嵌凹陷屏显底盘
    DrawNeumorphicSurface(dl, p0, p1, 16.0f, true);

    // 内部边距
    const float pad_x = 24.0f;
    const float pad_y = 20.0f;
    const ImVec2 chart_min = ImVec2(p0.x + pad_x, p0.y + pad_y);
    const ImVec2 chart_max = ImVec2(p1.x - pad_x, p1.y - pad_y);

    // 2. 价格极值计算与自适应纵轴
    float min_price = data[0].low;
    float max_price = data[0].high;
    for (const auto& c : data) {
      min_price = std::min(min_price, c.low);
      max_price = std::max(max_price, c.high);
    }
    float price_range = max_price - min_price;
    if (price_range <= 0.0001f) price_range = 1.0f;
    min_price -= price_range * 0.08f;
    max_price += price_range * 0.08f;
    price_range = max_price - min_price;

    auto PriceToY = [&](float p) -> float {
      float normalized = (p - min_price) / price_range;
      return chart_max.y - normalized * (chart_max.y - chart_min.y);
    };

    // 3. 绘制拟态网格横线
    const int grid_rows = 4;
    for (int i = 0; i <= grid_rows; ++i) {
      float t = (float)i / (float)grid_rows;
      float y = chart_min.y + t * (chart_max.y - chart_min.y);
      dl->AddLine(ImVec2(chart_min.x, y), ImVec2(chart_max.x, y), IM_COL32(200, 208, 218, 120), 1.0f);
    }

    // 4. 计算柱体宽度
    const int count = (int)data.size();
    const float slot_width = (chart_max.x - chart_min.x) / count;
    const float candle_width = std::max(2.0f, slot_width * 0.65f);

    int hovered_idx = -1;
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;

    // 5. 遍历绘制触感 K 线
    for (int i = 0; i < count; ++i) {
      const Candle& c = data[i];
      const float center_x = chart_min.x + (i + 0.5f) * slot_width;
      const bool is_bull = c.close >= c.open;
      const ImU32 col = is_bull ? COLOR_BULL : COLOR_BEAR;

      if (is_hovered && mouse_pos.x >= (center_x - slot_width * 0.5f) && mouse_pos.x < (center_x + slot_width * 0.5f)) {
        hovered_idx = i;
        dl->AddLine(ImVec2(center_x, chart_min.y), ImVec2(center_x, chart_max.y), IM_COL32(160, 175, 190, 150), 1.0f);
      }

      const float y_high = PriceToY(c.high);
      const float y_low  = PriceToY(c.low);
      float y_open = PriceToY(c.open);
      float y_close = PriceToY(c.close);
      float y_top = std::min(y_open, y_close);
      float y_bot = std::max(y_open, y_close);
      if (y_bot - y_top < 2.0f) y_bot = y_top + 2.0f;

      ImVec2 b_min = ImVec2(center_x - candle_width * 0.5f, y_top);
      ImVec2 b_max = ImVec2(center_x + candle_width * 0.5f, y_bot);

      // 触感软阴影（悬浮效果）
      dl->AddRectFilled(ImVec2(b_min.x + 1.5f, b_min.y + 1.5f), 
                        ImVec2(b_max.x + 1.5f, b_max.y + 1.5f), 
                        IM_COL32(180, 190, 205, 110), 2.5f);

      // 上下影线
      dl->AddLine(ImVec2(center_x, y_high), ImVec2(center_x, y_low), col, 1.5f);

      // 柱体实体
      dl->AddRectFilled(b_min, b_max, col, 2.5f);
    }

    // 6. 拟态悬停提示卡 (使用 VEC4 常量，类型匹配)
    if (hovered_idx != -1) {
      const Candle& c = data[hovered_idx];
      ImGui::BeginTooltip();
      ImGui::PushStyleColor(ImGuiCol_PopupBg, COLOR_BG);
      ImGui::TextColored(VEC4_TEXT_DIM, "K-LINE NODE #%02d", hovered_idx + 1);
      ImGui::Separator();
      ImGui::TextColored(VEC4_TEXT, "Open:  %.2f", c.open);
      ImGui::TextColored(VEC4_BULL, "High:  %.2f", c.high);
      ImGui::TextColored(VEC4_BEAR, "Low:   %.2f", c.low);
      ImGui::TextColored(VEC4_TEXT, "Close: %.2f", c.close);
      ImGui::PopStyleColor();
      ImGui::EndTooltip();
    }
  }
}

//-----------------------------------------------------------------------------------
// AppViewController
//-----------------------------------------------------------------------------------

@interface AppViewController () <MTKViewDelegate>
@property (nonatomic, readonly) MTKView* mtkView;
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTL4CommandQueue> commandQueue;
@property (nonatomic, strong) id<MTL4CommandAllocator> commandAllocator;
@property (nonatomic, assign) std::vector<Candle> candleData;
@end

@implementation AppViewController

- (instancetype)initWithNibName:(nullable NSString*)nibNameOrNil bundle:(nullable NSBundle*)nibBundleOrNil {
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];

  _device = MTLCreateSystemDefaultDevice();
  _commandQueue = [_device newMTL4CommandQueue];
  _commandAllocator = [_device newCommandAllocator];

  if (!self.device) {
    NSLog(@"Metal is not supported");
    abort();
  }

  // 构造模拟市场 K 线走势数据
  _candleData = {
    { 120.0f, 125.0f, 118.5f, 124.0f },
    { 124.0f, 128.5f, 122.0f, 127.5f },
    { 127.5f, 131.0f, 125.0f, 126.0f },
    { 126.0f, 126.5f, 121.0f, 122.0f },
    { 122.0f, 128.0f, 121.5f, 127.0f },
    { 127.0f, 134.0f, 126.0f, 133.5f },
    { 133.5f, 135.0f, 129.0f, 130.0f },
    { 130.0f, 132.5f, 128.0f, 131.0f },
    { 131.0f, 138.0f, 130.5f, 137.5f },
    { 137.5f, 142.0f, 136.0f, 140.0f },
    { 140.0f, 141.0f, 133.0f, 134.5f },
    { 134.5f, 136.0f, 129.5f, 131.0f },
    { 131.0f, 135.5f, 130.0f, 135.0f },
    { 135.0f, 144.0f, 134.5f, 143.0f },
    { 143.0f, 146.5f, 141.0f, 145.5f },
    { 145.5f, 148.0f, 142.0f, 144.0f }
  };

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplMetal4_Init(_device, _commandQueue, FRAMES_IN_FLIGHT);

  return self;
}

- (MTKView*)mtkView {
  return (MTKView*)self.view;
}

- (void)loadView {
  self.view = [[MTKView alloc] initWithFrame:CGRectMake(0, 0, 1200, 800)];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.mtkView.device = self.device;
  self.mtkView.delegate = self;

#if TARGET_OS_OSX
  ImGui_ImplOSX_Init(self.view);
  [NSApp activateIgnoringOtherApps:YES];
#endif
}

- (void)drawInMTKView:(MTKView*)view {
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize.x = view.bounds.size.width;
  io.DisplaySize.y = view.bounds.size.height;

#if TARGET_OS_OSX
  CGFloat framebufferScale = view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
#else
  CGFloat framebufferScale = view.window.screen.scale ?: UIScreen.mainScreen.scale;
#endif
  io.DisplayFramebufferScale = ImVec2(framebufferScale, framebufferScale);
  [self.commandAllocator reset];

  id<MTL4CommandBuffer> commandBuffer = [self.device newCommandBuffer];
  [commandBuffer beginCommandBufferWithAllocator:self.commandAllocator];

  MTL4RenderPassDescriptor* renderPassDescriptor = view.currentMTL4RenderPassDescriptor;
  if (renderPassDescriptor == nil) {
    [commandBuffer endCommandBuffer];
    [self.commandQueue commit:&commandBuffer count:1];
    return;
  }

  static int frameIndex = 0;
  ImGui_ImplMetal4_NewFrame(renderPassDescriptor, frameIndex);
  frameIndex = (frameIndex + 1) % FRAMES_IN_FLIGHT;

#if TARGET_OS_OSX
  ImGui_ImplOSX_NewFrame(view);
#endif
  ImGui::NewFrame();

  // === 拟态全视口主窗口 ===
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, Neumorphism::COLOR_BG);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::Begin("Canvas", nullptr, windowFlags);

  ImGui::Dummy(ImVec2(0, 24));
  ImGui::Indent(36);

  ImGui::TextColored(Neumorphism::VEC4_TEXT, "NEUMORPHIC TRADING WORKSTATION");
  ImGui::Dummy(ImVec2(0, 16));

  if (Neumorphism::Button("REALTIME", ImVec2(130, 44), 14.0f)) {
  }

  ImGui::SameLine(0, 20);

  if (Neumorphism::Button("HISTORY", ImVec2(130, 44), 14.0f)) {
  }

  ImGui::Dummy(ImVec2(0, 24));

  // 拟态 K 线图表
  Neumorphism::DrawKChart("MainKChart", self.candleData, ImVec2(800, 360));

  ImGui::Dummy(ImVec2(0, 24));

  // 底部容器卡片
  ImVec2 card_pos = ImGui::GetCursorScreenPos();
  ImVec2 card_size = ImVec2(800, 90);
  ImDrawList* dl = ImGui::GetWindowDrawList();

  Neumorphism::DrawNeumorphicSurface(
    dl,
    card_pos,
    ImVec2(card_pos.x + card_size.x, card_pos.y + card_size.y),
    16.0f,
    false
  );

  ImGui::SetCursorScreenPos(ImVec2(card_pos.x + 24, card_pos.y + 22));
  ImGui::BeginGroup();
  ImGui::TextColored(Neumorphism::VEC4_TEXT, "Active Market: DOXY-HCL (Index)");
  ImGui::TextColored(Neumorphism::VEC4_TEXT_DIM, "Hardware accelerated candles with soft tactile drop shadows.");
  ImGui::EndGroup();

  ImGui::Unindent(36);
  ImGui::End();

  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(1);

  // === 提交渲染 ===
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();

  renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
    230.0 / 255.0, 
    235.0 / 255.0, 
    240.0 / 255.0, 
    1.0
  );

  id<MTL4RenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
  [renderEncoder pushDebugGroup:@"Dear ImGui rendering"];
  ImGui_ImplMetal4_RenderDrawData(draw_data, commandBuffer, renderEncoder);
  [renderEncoder popDebugGroup];
  [renderEncoder endEncoding];

  [commandBuffer endCommandBuffer];
  [self.commandQueue waitForDrawable:view.currentDrawable];
  [self.commandQueue commit:&commandBuffer count:1];
  [self.commandQueue signalDrawable:view.currentDrawable];
  [view.currentDrawable present];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
}

#if TARGET_OS_OSX

- (void)viewWillAppear {
  [super viewWillAppear];
  self.view.window.delegate = self;
}

- (void)windowWillClose:(NSNotification*)notification {
  ImGui_ImplMetal4_Shutdown();
  ImGui_ImplOSX_Shutdown();
  ImGui::DestroyContext();
}

#endif

@end

//-----------------------------------------------------------------------------------
// AppDelegate & main()
//-----------------------------------------------------------------------------------

#if TARGET_OS_OSX

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSWindow* window;
@end

@implementation AppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return YES;
}

- (instancetype)init {
  if (self = [super init]) {
    NSViewController* rootViewController = [[AppViewController alloc] initWithNibName:nil bundle:nil];
    self.window = [[NSWindow alloc] initWithContentRect:NSZeroRect
                                              styleMask:NSWindowStyleMaskTitled |
                                                        NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskResizable |
                                                        NSWindowStyleMaskMiniaturizable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.contentViewController = rootViewController;
    [self.window setContentSize:NSMakeSize(920, 680)];
    [self.window center];
    [self.window makeKeyAndOrderFront:self];
  }
  return self;
}

@end

int main(int, const char**) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    AppDelegate* appDelegate = [[AppDelegate alloc] init];
    [NSApp setDelegate:appDelegate];

    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
  }
  return 0;
}

#endif