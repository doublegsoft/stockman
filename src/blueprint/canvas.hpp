#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "color.hpp"
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace blueprint {

  /**
   * @brief 单条绘制笔画数据结构体。
   */
  struct CanvasStroke {
    std::vector<ImVec2> points;         ///< 采样路径原始坐标集合
    ImU32 color = Color::InkWhite;      ///< 笔画颜色
    float thickness = 2.0f;             ///< 笔画线宽
    bool is_dashed = false;             ///< 是否为虚线
  };

  /**
   * @brief 画布持久化状态控制器。
   */
  struct CanvasState {
    std::vector<CanvasStroke> strokes;  ///< 已固化的历史笔画列表
    CanvasStroke current_stroke;        ///< 当前正在绘制中的笔画
    bool is_drawing = false;            ///< 绘制状态触发标志
    float stroke_thickness = 2.0f;      ///< 默认笔画粗细
    ImU32 stroke_color = Color::InkWhite;///< 当前画笔颜色
    bool is_dashed = false;             ///< 当前线型是否为虚线 (实线=false, 虚线=true)
    bool initialized = false;           ///< 状态初始化标记

    /**
     * @brief 撤销上一笔 (Undo)
     */
    void Undo() {
      if (!strokes.empty()) {
        strokes.pop_back();
      }
    }

    /**
     * @brief 清空画布所有笔画 (Clear)
     */
    void Clear() {
      strokes.clear();
      current_stroke.points.clear();
      is_drawing = false;
    }
  };

  /**
   * @brief 绘制虚线段
   */
  inline void DrawDashedLine(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col, float thickness, float dash_len = 6.0f, float gap_len = 4.0f) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= 0.0001f) return;

    float nx = dx / dist;
    float ny = dy / dist;
    float curr = 0.0f;

    while (curr < dist) {
      float next = std::min(curr + dash_len, dist);
      dl->AddLine(
        ImVec2(p1.x + nx * curr, p1.y + ny * curr),
        ImVec2(p1.x + nx * next, p1.y + ny * next),
        col, thickness
      );
      curr += (dash_len + gap_len);
    }
  }

  /**
   * @brief 修复后的高保真二次贝塞尔曲线平滑渲染（严格保证数学连续性与实线/虚线支持）。
   */
  inline void DrawSmoothSpline(ImDrawList* dl, const CanvasStroke& stroke) {
    if (!dl || stroke.points.empty()) return;

    const auto& pts = stroke.points;
    const size_t n = pts.size();
    const ImU32 col = stroke.color;
    const float th = stroke.thickness;

    // 单点墨滴
    if (n == 1) {
      dl->AddCircleFilled(pts[0], th * 0.6f, col);
      return;
    }

    // 两点直线
    if (n == 2) {
      if (stroke.is_dashed) {
        DrawDashedLine(dl, pts[0], pts[1], col, th);
      } else {
        dl->AddLine(pts[0], pts[1], col, th);
      }
      return;
    }

    // 连续多点二次贝塞尔中点平滑（彻底修复 Y 轴坐标取错的致命 Bug）
    ImVec2 p_mid_prev((pts[0].x + pts[1].x) * 0.5f, (pts[0].y + pts[1].y) * 0.5f);

    // 第一段：起点到首中点
    if (stroke.is_dashed) {
      DrawDashedLine(dl, pts[0], p_mid_prev, col, th);
    } else {
      dl->AddLine(pts[0], p_mid_prev, col, th);
    }

    // 中间段进行连续贝塞尔插值
    for (size_t i = 1; i < n - 1; ++i) {
      // ★ 严格修正：Y 坐标必须由 .y 计算！
      ImVec2 p_mid_curr((pts[i].x + pts[i + 1].x) * 0.5f, (pts[i].y + pts[i + 1].y) * 0.5f);

      if (stroke.is_dashed) {
        // 虚线采样曲线离散化绘制
        const int steps = 8;
        ImVec2 last_p = p_mid_prev;
        for (int s = 1; s <= steps; ++s) {
          float t = (float)s / (float)steps;
          float it = 1.0f - t;
          ImVec2 curr_p(
            it * it * p_mid_prev.x + 2.0f * it * t * pts[i].x + t * t * p_mid_curr.x,
            it * it * p_mid_prev.y + 2.0f * it * t * pts[i].y + t * t * p_mid_curr.y
          );
          if ((s % 2) != 0) {
            dl->AddLine(last_p, curr_p, col, th);
          }
          last_p = curr_p;
        }
      } else {
        dl->AddBezierQuadratic(p_mid_prev, pts[i], p_mid_curr, col, th, 16);
      }

      p_mid_prev = p_mid_curr;
    }

    // 尾段：末中点到终点
    if (stroke.is_dashed) {
      DrawDashedLine(dl, p_mid_prev, pts[n - 1], col, th);
    } else {
      dl->AddLine(p_mid_prev, pts[n - 1], col, th);
    }
  }

  /**
   * @brief 蓝图风格手绘曲线画布组件。
   */
  inline void Canvas(const char* label, ImVec2 size, CanvasState* state = nullptr) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    static CanvasState default_internal_state;
    CanvasState* current_state = state ? state : &default_internal_state;

    if (!current_state->initialized) {
      current_state->stroke_thickness = 2.0f;
      current_state->stroke_color = Color::InkWhite;
      current_state->is_dashed = false;
      current_state->initialized = true;
    }

    ImGui::InvisibleButton(label, size, ImGuiButtonFlags_MouseButtonLeft);
    const bool is_hovered = ImGui::IsItemHovered();
    const bool is_active = ImGui::IsItemActive();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = window->DrawList;
    ImGuiIO& io = ImGui::GetIO();

    const float header_h = 28.0f;
    const ImVec2 header_p1(p1.x, p0.y + header_h);
    const ImVec2 draw_min(p0.x + 8.0f, p0.y + header_h + 6.0f);
    const ImVec2 draw_max(p1.x - 8.0f, p1.y - 8.0f);

    // 顶栏操作按钮
    const float btn_w = 46.0f;
    const float btn_h = 18.0f;
    const float btn_y = p0.y + 5.0f;
    const ImVec2 clr_btn_min(p1.x - 8.0f - btn_w, btn_y);
    const ImVec2 clr_btn_max(p1.x - 8.0f, btn_y + btn_h);
    const ImVec2 undo_btn_min(clr_btn_min.x - 6.0f - btn_w, btn_y);
    const ImVec2 undo_btn_max(clr_btn_min.x - 6.0f, btn_y + btn_h);

    const bool is_over_clr = is_hovered && ImGui::IsMouseHoveringRect(clr_btn_min, clr_btn_max);
    const bool is_over_undo = is_hovered && ImGui::IsMouseHoveringRect(undo_btn_min, undo_btn_max);
    const bool is_over_buttons = is_over_clr || is_over_undo;

    // 清除 / 撤销交互
    if (is_over_clr && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      current_state->Clear();
    }
    if (is_over_undo && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      current_state->Undo();
    }

    // =================================================================
    // 换线条与连续采样逻辑 (重置机制加固)
    // =================================================================
    if (is_hovered || is_active) {
      if ((io.KeyCtrl || (io.ConfigMacOSXBehaviors && io.KeySuper)) && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        current_state->Undo();
      }

      // 起笔：创建全新独立的线条对象
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !is_over_buttons) {
        if (io.MousePos.x >= draw_min.x && io.MousePos.x <= draw_max.x &&
            io.MousePos.y >= draw_min.y && io.MousePos.y <= draw_max.y) {
          current_state->is_drawing = true;
          current_state->current_stroke = CanvasStroke(); // 换线条彻底初始化
          current_state->current_stroke.color = current_state->stroke_color;
          current_state->current_stroke.thickness = current_state->stroke_thickness;
          current_state->current_stroke.is_dashed = current_state->is_dashed;
          current_state->current_stroke.points.push_back(io.MousePos);
        }
      }

      // 拖拽采样
      if (current_state->is_drawing && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 clamped_pos(
          std::clamp(io.MousePos.x, draw_min.x, draw_max.x),
          std::clamp(io.MousePos.y, draw_min.y, draw_max.y)
        );

        const auto& pts = current_state->current_stroke.points;
        if (!pts.empty()) {
          const ImVec2& last_pt = pts.back();
          float dx = clamped_pos.x - last_pt.x;
          float dy = clamped_pos.y - last_pt.y;
          if ((dx * dx + dy * dy) >= 9.0f) { // 3px 滤波阈值
            current_state->current_stroke.points.push_back(clamped_pos);
          }
        }
      }
    }

    // 松开鼠标：固化当前线条进历史列表
    if (current_state->is_drawing && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      if (!current_state->current_stroke.points.empty()) {
        current_state->strokes.push_back(current_state->current_stroke);
      }
      current_state->current_stroke.points.clear();
      current_state->is_drawing = false;
    }

    // =================================================================
    // 底板、框架及 CAD 网格
    // =================================================================
    dl->AddRectFilled(p0, p1, Color::PanelBg);
    dl->AddRect(p0, p1, Color::InkWhite, 0.0f, 0, 1.2f);
    dl->AddLine(ImVec2(p0.x, header_p1.y), header_p1, Color::DimLine, 1.0f);
    dl->AddRect(draw_min, draw_max, Color::DimLine, 0.0f, 0, 1.0f);

    const float grid_step = 28.0f;
    dl->PushClipRect(draw_min, draw_max, true);
    for (float x = draw_min.x; x <= draw_max.x; x += grid_step) {
      dl->AddLine(ImVec2(x, draw_min.y), ImVec2(x, draw_max.y), Color::GridFine, 0.5f);
    }
    for (float y = draw_min.y; y <= draw_max.y; y += grid_step) {
      dl->AddLine(ImVec2(draw_min.x, y), ImVec2(draw_max.x, y), Color::GridFine, 0.5f);
    }

    // 渲染所有已固化的历史线条
    for (const auto& s : current_state->strokes) {
      DrawSmoothSpline(dl, s);
    }

    // 渲染当前正在画的活跃线条
    if (current_state->is_drawing && !current_state->current_stroke.points.empty()) {
      DrawSmoothSpline(dl, current_state->current_stroke);
    }

    // CAD 十字丝与光标坐标
    if (is_hovered && !is_over_buttons &&
        io.MousePos.x >= draw_min.x && io.MousePos.x <= draw_max.x &&
        io.MousePos.y >= draw_min.y && io.MousePos.y <= draw_max.y) {
      const ImVec2 mpos = io.MousePos;
      dl->AddLine(ImVec2(draw_min.x, mpos.y), ImVec2(draw_max.x, mpos.y), Color::InkCyan, 0.5f);
      dl->AddLine(ImVec2(mpos.x, draw_min.y), ImVec2(mpos.x, draw_max.y), Color::InkCyan, 0.5f);

      char coord[32];
      snprintf(coord, sizeof(coord), "%.0f, %.0f", mpos.x - draw_min.x, mpos.y - draw_min.y);
      ImVec2 t_sz = ImGui::CalcTextSize(coord);
      ImVec2 b0(mpos.x + 8.0f, mpos.y + 8.0f);
      ImVec2 b1(b0.x + t_sz.x + 6.0f, b0.y + t_sz.y + 2.0f);

      dl->AddRectFilled(b0, b1, Color::PanelBg);
      dl->AddRect(b0, b1, Color::InkCyan, 0.0f, 0, 1.0f);
      dl->AddText(ImVec2(b0.x + 3.0f, b0.y + 1.0f), Color::InkCyan, coord);
    }
    dl->PopClipRect();

    // 顶栏信息与按钮渲染
    char info[48];
    snprintf(info, sizeof(info), "草图画布 [%s / %02zu条]", current_state->is_dashed ? "虚线" : "实线", current_state->strokes.size());
    dl->AddText(ImVec2(p0.x + 10.0f, p0.y + 6.0f), Color::InkCyan, info);

    // 撤销按钮外观
    if (is_over_undo) {
      const bool is_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
      dl->AddRectFilled(undo_btn_min, undo_btn_max, is_down ? Color::InkCyan : Color::GridBold);
      dl->AddRect(undo_btn_min, undo_btn_max, Color::InkCyan, 0.0f, 0, 1.2f);
      dl->AddText(ImVec2(undo_btn_min.x + 10.0f, undo_btn_min.y + 2.0f), is_down ? Color::Bg : Color::InkWhite, "撤销");
    } else {
      dl->AddRectFilled(undo_btn_min, undo_btn_max, Color::PanelBg);
      dl->AddRect(undo_btn_min, undo_btn_max, Color::DimLine, 0.0f, 0, 1.0f);
      dl->AddText(ImVec2(undo_btn_min.x + 10.0f, undo_btn_min.y + 2.0f), Color::InkCyan, "撤销");
    }

    // 清除按钮外观
    if (is_over_clr) {
      const bool is_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
      dl->AddRectFilled(clr_btn_min, clr_btn_max, is_down ? Color::InkCyan : Color::GridBold);
      dl->AddRect(clr_btn_min, clr_btn_max, Color::InkCyan, 0.0f, 0, 1.2f);
      dl->AddText(ImVec2(clr_btn_min.x + 10.0f, clr_btn_min.y + 2.0f), is_down ? Color::Bg : Color::InkWhite, "清除");
    } else {
      dl->AddRectFilled(clr_btn_min, clr_btn_max, Color::PanelBg);
      dl->AddRect(clr_btn_min, clr_btn_max, Color::InkCyan, 0.0f, 0, 1.0f);
      dl->AddText(ImVec2(clr_btn_min.x + 10.0f, clr_btn_min.y + 2.0f), Color::InkCyan, "清除");
    }
  }

} // namespace blueprint