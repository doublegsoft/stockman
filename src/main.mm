/*                                                                                                             
**        ██               ▄▄                          
** ▄█▀▀▀ ▀██▀▀ ▄███▄ ▄████ ██ ▄█▀ ███▄███▄  ▀▀█▄ ████▄ 
** ▀███▄  ██   ██ ██ ██    ████   ██ ██ ██ ▄█▀██ ██ ██ 
** ▄▄▄█▀  ██   ▀███▀ ▀████ ██ ▀█▄ ██ ██ ██ ▀█▄██ ██ ██    
** 
** Copyright 2026 doublegsoft.me
** 
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
** 
**     http://www.apache.org/licenses/LICENSE-2.0
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
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
#include <string>
#include <algorithm>
#include <cmath>
#include <stdio.h>
#include <unistd.h>

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

// =================================================================
// 引入 Blueprint 抽象组件系统及数据加载器
// =================================================================
#include "dataloader.hpp"
#include "blueprint/color.hpp"
#include "blueprint/button.hpp"
#include "blueprint/kchart.hpp"
#include "blueprint/canvas.hpp"
#include "blueprint/dropdown.hpp"
#include "blueprint/segments.hpp"
#include "blueprint/textinput.hpp"

#define FRAMES_IN_FLIGHT 2

// 本地行情数据绝对根目录
static NSString* const kStockDataBaseDir = @"/Users/christian/export/local/works/doublegsoft.me/stockman/03.Development/stockman-data/hsjday";

/**
  * @brief 自动检测并载入 macOS 原生中文字体
  */
inline void SetupChineseFont(ImGuiIO& io, float font_size = 15.0f) {
  const char* font_paths[] = {
    "/System/Library/Fonts/PingFang.ttc",          // 苹方 (macOS 默认最佳)
    "/System/Library/Fonts/Hiragino Sans GB.ttc", // 冬青黑体
    "/System/Library/Fonts/STHeiti Light.ttc",    // 华文黑体
    "/Library/Fonts/Arial Unicode.ttf"            // 通用备选
  };

  for (const char* path : font_paths) {
    if (access(path, R_OK) == 0) {
      ImFontConfig config;
      config.FontNo = 0; // TTC 首个子字体
      config.OversampleH = 2;
      config.OversampleV = 2;

      // 载入常用简体中文字符集 (含 ASCII 及 ~2500 常用汉字)
      io.Fonts->AddFontFromFileTTF(path, font_size, &config, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
      return;
    }
  }

  // 兜底英文字体
  io.Fonts->AddFontDefault();
}

// 绘制 CAD 底层坐标网格
inline void DrawGrid(ImDrawList* dl, ImVec2 min, ImVec2 max) {
  const float step_fine = 20.0f;
  const float step_bold = 100.0f;

  for (float x = min.x; x <= max.x; x += step_fine) {
    dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), blueprint::Color::GridFine, 1.0f);
  }
  for (float y = min.y; y <= max.y; y += step_fine) {
    dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), blueprint::Color::GridFine, 1.0f);
  }
  for (float x = min.x; x <= max.x; x += step_bold) {
    dl->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), blueprint::Color::GridBold, 1.2f);
  }
  for (float y = min.y; y <= max.y; y += step_bold) {
    dl->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), blueprint::Color::GridBold, 1.2f);
  }
}

// 绘制工程图纸图框与边界坐标刻度
inline void DrawSheetBorder(ImDrawList* dl, ImVec2 min, ImVec2 max) {
  const float pad = 16.0f;
  const ImVec2 b_min = ImVec2(min.x + pad, min.y + pad);
  const ImVec2 b_max = ImVec2(max.x - pad, max.y - pad);

  dl->AddRect(b_min, b_max, blueprint::Color::InkWhite, 0.0f, 0, 2.0f);
  dl->AddRect(ImVec2(b_min.x + 4.0f, b_min.y + 4.0f), ImVec2(b_max.x - 4.0f, b_max.y - 4.0f), blueprint::Color::InkCyan, 0.0f, 0, 1.0f);

  const char* cols[] = { "A", "B", "C", "D", "E" };
  float col_w = (b_max.x - b_min.x) / 5.0f;
  for (int i = 0; i < 5; ++i) {
    float cx = b_min.x + (i + 0.5f) * col_w;
    dl->AddText(ImVec2(cx - 4.0f, b_min.y + 6.0f), blueprint::Color::InkCyan, cols[i]);
    dl->AddText(ImVec2(cx - 4.0f, b_max.y - 18.0f), blueprint::Color::InkCyan, cols[i]);
  }

  const char* rows[] = { "1", "2", "3" };
  float row_h = (b_max.y - b_min.y) / 3.0f;
  for (int i = 0; i < 3; ++i) {
    float cy = b_min.y + (i + 0.5f) * row_h;
    dl->AddText(ImVec2(b_min.x + 8.0f, cy - 6.0f), blueprint::Color::InkCyan, rows[i]);
    dl->AddText(ImVec2(b_max.x - 18.0f, cy - 6.0f), blueprint::Color::InkCyan, rows[i]);
  }
}

//-----------------------------------------------------------------------------------
// AppViewController
//-----------------------------------------------------------------------------------

@interface AppViewController () <MTKViewDelegate> {
  std::vector<blueprint::Candle> _candleData;
  std::vector<std::string> _candleDateStrings; // 保证 K 线日期字符串常驻内存生命周期
  blueprint::CanvasState _canvasState;
  blueprint::KChartState _kchartState;         // 图表视口状态
  std::string _currentDataName;                // 当前数据集显示名
  std::string _statusMessage;                  // 搜索状态反馈信息
}
@property (nonatomic, readonly) MTKView* mtkView;
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTL4CommandQueue> commandQueue;
@property (nonatomic, strong) id<MTL4CommandAllocator> commandAllocator;
@end

@implementation AppViewController

/**
 * @brief 将 KChart 视口设定为「在 20 根基准上 Zoom-Out 远眺两度」的状态
 */
- (void)applyDefaultZoomOutState {
  if (_candleData.empty()) {
    _kchartState.visible_count = 20.0f;
    _kchartState.offset = 0.0f;
    _kchartState.velocity = 0.0f;
    _kchartState.initialized = false;
    return;
  }

  const float base_visible = 20.0f;
  const float zoom_factor = 1.15f;
  const float zoomed_visible = base_visible * (zoom_factor * zoom_factor); // 约 26.45 根

  const float total = static_cast<float>(_candleData.size());
  _kchartState.visible_count = std::min(total, zoomed_visible);
  _kchartState.offset = std::max(0.0f, total - _kchartState.visible_count);
  _kchartState.velocity = 0.0f;
  _kchartState.initialized = true;
}

/**
 * @brief 在本地行情多层目录中快速检索并定位 6 位代码对应的 .day 文件路径
 */
- (NSString*)findDayFilePathForCode:(NSString*)code {
  if (code.length != 6) return nil;

  NSFileManager* fm = [NSFileManager defaultManager];
  if (![fm fileExistsAtPath:kStockDataBaseDir]) {
    NSLog(@"[Error] 行情根目录不存在: %@", kStockDataBaseDir);
    return nil;
  }

  // 1. 规则预测优先匹配 (O(1) 极速直达)
  NSArray<NSString*>* marketPrefixes;
  if ([code hasPrefix:@"6"]) {
    marketPrefixes = @[@"sh", @"sz", @"bj"];
  } else if ([code hasPrefix:@"0"] || [code hasPrefix:@"3"]) {
    marketPrefixes = @[@"sz", @"sh", @"bj"];
  } else {
    marketPrefixes = @[@"bj", @"sz", @"sh"];
  }

  for (NSString* pfx in marketPrefixes) {
    // 典型结构：hsjday/sz/lday/sz300531.day
    NSString* direct1 = [NSString stringWithFormat:@"%@/%@/lday/%@%@.day", kStockDataBaseDir, pfx, pfx, code];
    if ([fm fileExistsAtPath:direct1]) return direct1;

    // 结构：hsjday/sz/sz300531.day
    NSString* direct2 = [NSString stringWithFormat:@"%@/%@/%@%@.day", kStockDataBaseDir, pfx, pfx, code];
    if ([fm fileExistsAtPath:direct2]) return direct2;

    // 结构：hsjday/lday/sz300531.day
    NSString* direct3 = [NSString stringWithFormat:@"%@/lday/%@%@.day", kStockDataBaseDir, pfx, code];
    if ([fm fileExistsAtPath:direct3]) return direct3;
  }

  // 2. 多级子目录递归遍历兜底
  NSDirectoryEnumerator* enumerator = [fm enumeratorAtPath:kStockDataBaseDir];
  NSString* targetFileSuffix = [NSString stringWithFormat:@"%@.day", code];

  NSString* subpath;
  while ((subpath = [enumerator nextObject])) {
    if ([subpath hasSuffix:targetFileSuffix]) {
      return [kStockDataBaseDir stringByAppendingPathComponent:subpath];
    }
  }

  return nil;
}

/**
 * @brief 根据 6 位股票代码搜索并加载行情
 */
- (BOOL)searchAndLoadStockCode:(NSString*)code {
  NSString* foundPath = [self findDayFilePathForCode:code];
  if (!foundPath) {
    _statusMessage = "未找到代码: " + std::string([code UTF8String]);
    NSLog(@"[Warning] 未在根目录下搜索到股票代码 %@ 的数据文件", code);
    return NO;
  }

  BOOL ok = [self loadDataFromFile:foundPath];
  if (ok) {
    _statusMessage = "已载入: " + std::string([foundPath.lastPathComponent UTF8String]);
  }
  return ok;
}

/**
 * @brief 使用 dataloader 解析二进制行情文件
 */
- (BOOL)loadDataFromFile:(NSString*)filePath {
  OhlcvParser parser;
  if (!parser.parse([filePath UTF8String])) {
    NSLog(@"[Error] 读取二进制行情文件失败: %@", filePath);
    return NO;
  }

  const auto& records = parser.records();
  if (records.empty()) return NO;

  _candleData.clear();
  _candleDateStrings.clear();
  _candleData.reserve(records.size());
  _candleDateStrings.reserve(records.size());

  for (const auto& r : records) {
    uint32_t y = r.date / 10000;
    uint32_t m = (r.date % 10000) / 100;
    uint32_t d = r.date % 100;
    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%04u-%02u-%02u", y, m, d);
    _candleDateStrings.emplace_back(dateBuf);

    blueprint::Candle c;
    c.open  = static_cast<float>(r.open);
    c.high  = static_cast<float>(r.high);
    c.low   = static_cast<float>(r.low);
    c.close = static_cast<float>(r.close);
    c.volume = static_cast<float>(r.volume); // ★ 装载成交量 (手)
    c.amount = r.amount;                     // ★ 装载成交金额 (元)
    c.time  = _candleDateStrings.back().c_str();

    _candleData.push_back(c);
  }

  // 默认应用 Zoom-Out 远眺两度视角
  [self applyDefaultZoomOutState];
  _currentDataName = [filePath.lastPathComponent UTF8String];

  NSLog(@"[Success] 装载 %zu 根 K 线: %@", _candleData.size(), filePath);
  return YES;
}

- (instancetype)initWithNibName:(nullable NSString*)nibNameOrNil bundle:(nullable NSBundle*)nibBundleOrNil {
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];

  _device = MTLCreateSystemDefaultDevice();
  _commandQueue = [_device newMTL4CommandQueue];
  _commandAllocator = [_device newCommandAllocator];

  if (!self.device) {
    NSLog(@"Metal is not supported");
    abort();
  }

  _currentDataName = "未载入数据";
  _statusMessage = "就绪";
  
  // 启动即直接检索并载入本地 300531 真实行情
  [self searchAndLoadStockCode:@"300531"];

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  SetupChineseFont(io, 12.0f);
  ImGui::StyleColorsDark();

  ImGui_ImplMetal4_Init(_device, _commandQueue, FRAMES_IN_FLIGHT);
  return self;
}

- (MTKView*)mtkView {
  return (MTKView*)self.view;
}

- (void)loadView {
  self.view = [[MTKView alloc] initWithFrame:CGRectMake(0, 0, 1380, 860)];
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

  // === 注入蓝图风格主视口 ===
  ImGui::PushStyleColor(ImGuiCol_WindowBg, blueprint::Color::Bg);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, blueprint::Color::PanelBg);
  ImGui::PushStyleColor(ImGuiCol_Border, blueprint::Color::InkWhite);
  ImGui::PushStyleColor(ImGuiCol_Text, blueprint::Color::InkWhite);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("BlueprintCanvas", nullptr, windowFlags);

  ImDrawList* dl = ImGui::GetWindowDrawList();

  // 1. 底层 CAD 网格与图框
  DrawGrid(dl, ImVec2(0, 0), io.DisplaySize);
  DrawSheetBorder(dl, ImVec2(0, 0), io.DisplaySize);

  // 2. 顶部工具栏 (中英文)
  ImGui::SetCursorPos(ImVec2(32, 32));
  ImGui::BeginGroup();
  ImGui::TextColored(blueprint::Color::Vec4White, "FERROPRINT 蓝图");
  ImGui::SameLine();
  ImGui::TextColored(blueprint::Color::Vec4Cyan, " [%s]", _currentDataName.c_str());
  ImGui::SameLine(0, 24);

  static int active_mode = 0;
  if (blueprint::Button("吸附 (SNAP)", ImVec2(90, 28), active_mode == 0)) active_mode = 0;
  ImGui::SameLine(0, 6);
  if (blueprint::Button("标尺 (DIMS)", ImVec2(90, 28), active_mode == 1)) active_mode = 1;
  ImGui::SameLine(0, 6);
  if (blueprint::Button("蓝图 (BLUE)", ImVec2(90, 28), active_mode == 2)) active_mode = 2;
  ImGui::SameLine(0, 6);
  if (blueprint::Button("白图 (WHITE)", ImVec2(90, 28), active_mode == 3)) active_mode = 3;
  ImGui::EndGroup();

  // =================================================================
  // 右侧列布局参数定义：表单与 Canvas 严格对齐 (340px 宽，32px 边距)
  // =================================================================
  const float right_col_w = 340.0f;
  const float right_col_margin_r = 32.0f;
  const float right_col_margin_top = 32.0f;
  const float right_col_margin_bottom = 32.0f;
  const float right_col_x = io.DisplaySize.x - right_col_w - right_col_margin_r;

  const float form_h = 490.0f;
  const float gap = 12.0f;
  const float canvas_y = right_col_margin_top + form_h + gap;
  const float canvas_h = io.DisplaySize.y - canvas_y - right_col_margin_bottom;

  // =================================================================
  // 3. 极大化蓝图 K 线图
  // =================================================================
  const float chart_x = 32.0f;
  const float chart_y = 78.0f;
  const float chart_w = right_col_x - chart_x - 16.0f;
  const float chart_h = io.DisplaySize.y - chart_y - 32.0f;

  ImGui::SetCursorPos(ImVec2(chart_x, chart_y));
  blueprint::KChart("MainBlueprintKChart", _candleData, ImVec2(chart_w, chart_h), &_kchartState);

  // =================================================================
  // 4. 右侧属性检查器输入表单 (BOX INSPECTOR)
  // =================================================================
  ImGui::SetCursorPos(ImVec2(right_col_x, right_col_margin_top));
  ImGui::BeginChild("InspectorPanel", ImVec2(right_col_w, form_h), true, ImGuiWindowFlags_None);

  ImGui::TextColored(blueprint::Color::Vec4White, "元件属性 (BOX)");
  ImGui::SameLine(ImGui::GetWindowWidth() - 84);
  ImGui::TextColored(blueprint::Color::Vec4Cyan, "160 x 80");
  ImGui::Separator();
  ImGui::Dummy(ImVec2(0, 3));

  // 标签名称 LABEL
  ImGui::TextColored(blueprint::Color::Vec4Cyan, "标签名称 (LABEL)");
  static std::string label_text = "核心交易标的\n行情追踪";
  blueprint::TextInputMultiline("##label", label_text, ImVec2(-1.0f, 36.0f));

  ImGui::Dummy(ImVec2(0, 3));

  // 股票代码 (SYMBOL) - 回车载入
  ImGui::TextColored(blueprint::Color::Vec4Cyan, "股票代码 (SYMBOL - 回车载入)");
  static char symbol_buf[16] = "300531";

  ImGui::PushStyleColor(ImGuiCol_FrameBg, blueprint::Color::PanelBg);
  ImGui::PushStyleColor(ImGuiCol_Border, blueprint::Color::InkWhite);
  ImGui::SetNextItemWidth(-1.0f);

  if (ImGui::InputText("##symbol", symbol_buf, sizeof(symbol_buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal)) {
    NSString* code = [[NSString stringWithUTF8String:symbol_buf] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (code.length == 6) {
      [self searchAndLoadStockCode:code];
    } else {
      _statusMessage = "代码须为6位数字";
    }
  }
  ImGui::PopStyleColor(2);

  // 状态反馈及已载入信息
  ImGui::TextColored(blueprint::Color::Vec4Dim, "状态: %s (%zu 根K线)", _statusMessage.c_str(), _candleData.size());

  ImGui::Dummy(ImVec2(0, 3));

  // 下拉框 TIMEFRAME
  static int current_tf = 3;
  const char* timeframes[] = { "1分钟 · 实时分时", "5分钟 · 日内趋势", "1小时 · 小时线", "1天 · 日K线" };
  ImGui::TextColored(blueprint::Color::Vec4Cyan, "行情周期 (TIMEFRAME)");
  blueprint::Dropdown("##timeframe", &current_tf, timeframes, IM_ARRAYSIZE(timeframes), -1.0f);

  ImGui::Dummy(ImVec2(0, 4));

  // 坐标数值输入
  static float pos_x = 460.0f, pos_y = 260.0f;
  static float size_w = 160.0f, size_h = 80.0f;
  ImGui::TextColored(blueprint::Color::Vec4Cyan, "位置与尺寸 (PX)");
  ImGui::SetNextItemWidth(140);
  ImGui::DragFloat("##x", &pos_x, 1.0f, 0.0f, 2000.0f, "X: %.0f");
  ImGui::SameLine(0, 12);
  ImGui::SetNextItemWidth(140);
  ImGui::DragFloat("##y", &pos_y, 1.0f, 0.0f, 2000.0f, "Y: %.0f");

  ImGui::SetNextItemWidth(140);
  ImGui::DragFloat("##w", &size_w, 1.0f, 10.0f, 1000.0f, "W: %.0f");
  ImGui::SameLine(0, 12);
  ImGui::SetNextItemWidth(140);
  ImGui::DragFloat("##h", &size_h, 1.0f, 10.0f, 1000.0f, "H: %.0f");

  ImGui::Dummy(ImVec2(0, 4));

  // 分段选择器 LINE (与 Canvas 画布联动)
  ImGui::TextColored(blueprint::Color::Vec4Cyan, "线型 (LINE)");
  static int line_idx = 0;
  const char* line_opts[] = { "实线", "虚线" };
  if (blueprint::Segments("##line_style", &line_idx, line_opts, 2, -1.0f, 22.0f)) {
    _canvasState.is_dashed = (line_idx == 1);
  } else {
    _canvasState.is_dashed = (line_idx == 1);
  }

  ImGui::Dummy(ImVec2(0, 3));

  // 分段选择器 FILL
  ImGui::TextColored(blueprint::Color::Vec4Cyan, "填充样式 (FILL)");
  static int fill_idx = 2;
  const char* fill_opts[] = { "空心", "半透明", "剖面线" };
  blueprint::Segments("##fill_style", &fill_idx, fill_opts, 3, -1.0f, 22.0f);

  ImGui::Dummy(ImVec2(0, 5));

  // 操作按钮组第一行
  if (blueprint::Button("置于顶层", ImVec2(140, 24))) {}
  ImGui::SameLine(0, 12);
  if (blueprint::Button("置于底层", ImVec2(140, 24))) {}

  ImGui::Dummy(ImVec2(0, 3));

  // 操作按钮组第二行
  if (blueprint::Button("锁定元件", ImVec2(140, 24))) {}
  ImGui::SameLine(0, 12);
  if (blueprint::Button("创建副本", ImVec2(140, 24))) {}

  ImGui::EndChild();

  // =================================================================
  // 5. 底部手绘曲线画布 (CANVAS)：严格对齐 Entry Form
  // =================================================================
  ImGui::SetCursorPos(ImVec2(right_col_x, canvas_y));
  blueprint::Canvas("AlignedCanvas", ImVec2(right_col_w, canvas_h), &_canvasState);

  ImGui::End();

  ImGui::PopStyleVar(4);
  ImGui::PopStyleColor(4);

  // === 提交 Metal4 渲染 ===
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();

  renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(
    16.0 / 255.0, 
    54.0 / 255.0, 
    115.0 / 255.0, 
    1.0
  );

  id<MTL4RenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
  [renderEncoder pushDebugGroup:@"Blueprint rendering"];
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
    [self.window setContentSize:NSMakeSize(1380, 860)];
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