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
   * @brief 单根 K 线（蜡烛图）行情数据结构体（含时间、成交量、成交额）。
   */
  struct Candle {
    float open = 0.0f;          ///< 开盘价 (Open)
    float high = 0.0f;          ///< 最高价 (High)
    float low = 0.0f;           ///< 最低价 (Low)
    float close = 0.0f;         ///< 收盘价 (Close)
    float volume = 0.0f;        ///< 成交量 (Volume，单位：手/股)
    float amount = 0.0f;        ///< 成交额 (Amount，单位：元)
    const char* time = nullptr; ///< 时间标签 (如 "2026-10-07")
  };

  /**
   * @brief K 线图表视口状态控制器。
   */
  struct KChartState {
    float offset = 0.0f;         ///< 视口起始数据索引偏移
    float visible_count = 26.45f;///< 视口当前可见蜡烛总数 (默认 Zoom-Out 2级)
    float velocity = 0.0f;      ///< 惯性滑动速度
    bool initialized = false;    ///< 初始化标志
  };

  /**
   * @brief 格式化单根 K 线的时间字符串。
   */
  inline void FormatCandleTime(int index, const Candle& c, char* out_buf, size_t out_size) {
    if (c.time && c.time[0] != '\0') {
      snprintf(out_buf, out_size, "%s", c.time);
    } else {
      int total_mins = 9 * 60 + 30 + index * 5;
      if (total_mins > 11 * 60 + 30) total_mins += 90;
      int h = (total_mins / 60) % 24;
      int m = total_mins % 60;
      snprintf(out_buf, out_size, "%02d:%02d", h, m);
    }
  }

  /**
   * @brief 格式化成交量大数显示 (万手 / 亿)
   */
  inline void FormatVolumeText(float vol, char* out_buf, size_t out_size) {
    if (vol >= 100000000.0f) {
      snprintf(out_buf, out_size, "%.2f亿", vol / 100000000.0f);
    } else if (vol >= 10000.0f) {
      snprintf(out_buf, out_size, "%.1f万", vol / 10000.0f);
    } else {
      snprintf(out_buf, out_size, "%.0f", vol);
    }
  }

  /**
   * @brief 绘制 CAD 经典 45° 剖面斜线填充。
   */
  inline void DrawHatch(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 col, float spacing = 5.0f) {
    if (!dl) return;
    dl->PushClipRect(min, max, true);
    const float total = (max.x - min.x) + (max.y - min.y);
    for (float d = 0.0f; d <= total; d += spacing) {
      dl->AddLine(ImVec2(min.x + d, min.y), ImVec2(min.x, min.y + d), col, 1.0f);
    }
    dl->PopClipRect();
  }

  /**
   * @brief 渲染含【K线价格主图】与【成交量副图】分屏联动的蓝图 K 线图表。
   */
  inline void KChart(const char* label, const std::vector<Candle>& data, ImVec2 size, KChartState* state = nullptr) {
    if (data.empty()) return;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    static KChartState default_internal_state;
    KChartState* current_state = state ? state : &default_internal_state;

    const int total_count = static_cast<int>(data.size());

    // 默认初始视角
    if (!current_state->initialized) {
      current_state->visible_count = std::min(static_cast<float>(total_count), 26.45f);
      current_state->offset = std::max(0.0f, static_cast<float>(total_count) - current_state->visible_count);
      current_state->velocity = 0.0f;
      current_state->initialized = true;
    }

    // 注册整个视口
    ImGui::InvisibleButton(label, size, ImGuiButtonFlags_MouseButtonLeft);
    const bool is_hovered = ImGui::IsItemHovered();
    const bool is_active = ImGui::IsItemActive();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    ImDrawList* dl = window->DrawList;
    ImGuiIO& io = ImGui::GetIO();

    // 边距规划
    const float pad_left = 68.0f;   // 左侧价格/量能标尺留出 68px
    const float pad_right = 16.0f;
    const float pad_top = 16.0f;
    const float pad_bottom = 26.0f; // 底部时间轴留出 26px

    const float total_w = p1.x - p0.x;
    const float total_h = p1.y - p0.y;
    const float chart_width = (p1.x - pad_right) - (p0.x + pad_left);

    // =================================================================
    // 分屏几何计算：主图 (72% 价格) + 分割槽 (12px) + 副图 (28% 成交量)
    // =================================================================
    const float avail_h = total_h - pad_top - pad_bottom;
    const float split_ratio = 0.72f; 
    const float gap = 12.0f;

    const float price_h = (avail_h - gap) * split_ratio;
    const float volume_h = (avail_h - gap) * (1.0f - split_ratio);

    // 主图 (Price) 视口范围
    const ImVec2 price_min(p0.x + pad_left, p0.y + pad_top);
    const ImVec2 price_max(p1.x - pad_right, price_min.y + price_h);

    // 副图 (Volume) 视口范围
    const ImVec2 vol_min(p0.x + pad_left, price_max.y + gap);
    const ImVec2 vol_max(p1.x - pad_right, vol_min.y + volume_h);

    // =================================================================
    // 鼠标拖拽、平移、阻尼与滚轮缩放响应
    // =================================================================
    const float slot_width = chart_width / current_state->visible_count;

    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
      const float delta_x = io.MouseDelta.x;
      const float frame_velocity = delta_x / slot_width;
      current_state->offset -= frame_velocity;
      current_state->velocity = frame_velocity;
    } else {
      if (std::abs(current_state->velocity) > 0.001f) {
        current_state->offset -= current_state->velocity;
        current_state->velocity *= 0.88f;
      } else {
        current_state->velocity = 0.0f;
      }
    }

    if (is_hovered && io.MouseWheelH != 0.0f) {
      current_state->offset -= io.MouseWheelH * 2.0f;
      current_state->velocity = 0.0f;
    }

    if (is_hovered && io.MouseWheel != 0.0f) {
      const float zoom_factor = 1.15f;
      float new_visible = (io.MouseWheel > 0.0f) 
        ? (current_state->visible_count / zoom_factor) 
        : (current_state->visible_count * zoom_factor);

      new_visible = std::clamp(new_visible, 5.0f, std::max(60.0f, static_cast<float>(total_count) * 2.5f));
      const float cursor_ratio = std::clamp((io.MousePos.x - price_min.x) / chart_width, 0.0f, 1.0f);
      const float current_anchor = current_state->offset + cursor_ratio * current_state->visible_count;

      current_state->offset = current_anchor - cursor_ratio * new_visible;
      current_state->visible_count = new_visible;
      current_state->velocity = 0.0f;
    }

    const float min_offset = -current_state->visible_count * 0.4f;
    const float max_offset = static_cast<float>(total_count) - current_state->visible_count * 0.6f;
    current_state->offset = std::clamp(current_state->offset, min_offset, max_offset);

    // =================================================================
    // 视口数据切片：极值极小/极大计算 (价格 & 成交量)
    // =================================================================
    const int start_idx = std::clamp(static_cast<int>(std::floor(current_state->offset)), 0, total_count - 1);
    const int end_idx   = std::clamp(static_cast<int>(std::ceil(current_state->offset + current_state->visible_count)), 0, total_count - 1);

    float min_price = data[start_idx].low;
    float max_price = data[start_idx].high;
    float max_vol   = std::max(1.0f, data[start_idx].volume);

    for (int i = start_idx; i <= end_idx; ++i) {
      min_price = std::min(min_price, data[i].low);
      max_price = std::max(max_price, data[i].high);
      max_vol   = std::max(max_vol, data[i].volume);
    }

    // 价格留白 8%
    float price_range = max_price - min_price;
    if (price_range <= 0.0001f) price_range = 1.0f;
    min_price -= price_range * 0.08f;
    max_price += price_range * 0.08f;
    price_range = max_price - min_price;

    auto PriceToY = [&](float p) -> float {
      float normalized = (p - min_price) / price_range;
      return price_max.y - normalized * (price_max.y - price_min.y);
    };

    auto VolToY = [&](float v) -> float {
      float normalized = std::clamp(v / max_vol, 0.0f, 1.0f);
      return vol_max.y - normalized * (vol_max.y - vol_min.y);
    };

    // =================================================================
    // 底板、框架及主副图网格标尺绘制
    // =================================================================
    dl->AddRectFilled(p0, p1, Color::PanelBg);
    dl->AddRect(p0, p1, Color::InkWhite, 0.0f, 0, 1.5f);

    // 主副图边框与分割虚线
    dl->AddRect(price_min, price_max, Color::DimLine, 0.0f, 0, 1.0f);
    dl->AddRect(vol_min, vol_max, Color::DimLine, 0.0f, 0, 1.0f);

    // 主图：价格横线标尺 (4 行横线)
    const int price_rows = 4;
    for (int i = 0; i <= price_rows; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(price_rows);
      float y = price_min.y + t * price_h;
      float p_val = max_price - t * price_range;
      dl->AddLine(ImVec2(price_min.x, y), ImVec2(price_max.x, y), Color::GridFine, 0.6f);

      char buf[32];
      snprintf(buf, sizeof(buf), "%.2f", p_val);
      dl->AddText(ImVec2(p0.x + 8.0f, y - 7.0f), Color::InkCyan, buf);
    }

    // 副图：成交量标尺及顶部数值标签 (VOL 顶格与半格线)
    dl->AddLine(ImVec2(vol_min.x, vol_min.y), ImVec2(vol_max.x, vol_min.y), Color::GridFine, 0.6f);
    dl->AddLine(ImVec2(vol_min.x, vol_min.y + volume_h * 0.5f), ImVec2(vol_max.x, vol_min.y + volume_h * 0.5f), Color::GridFine, 0.4f);

    char vol_max_str[32];
    FormatVolumeText(max_vol, vol_max_str, sizeof(vol_max_str));
    dl->AddText(ImVec2(p0.x + 8.0f, vol_min.y - 4.0f), Color::InkCyan, vol_max_str);

    char vol_mid_str[32];
    FormatVolumeText(max_vol * 0.5f, vol_mid_str, sizeof(vol_mid_str));
    dl->AddText(ImVec2(p0.x + 8.0f, vol_min.y + volume_h * 0.5f - 6.0f), Color::InkCyan, vol_mid_str);

    dl->AddText(ImVec2(vol_min.x + 6.0f, vol_min.y + 4.0f), Color::InkCyan, "成交量 VOL");

    // 动态 X 轴时间刻度线 (贯穿主副图)
    const int time_ticks = 5;
    for (int i = 0; i < time_ticks; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(time_ticks - 1);
      float tick_x = price_min.x + t * chart_width;

      int tick_candle_idx = static_cast<int>(std::round(current_state->offset + t * current_state->visible_count));
      if (tick_candle_idx >= 0 && tick_candle_idx < total_count) {
        dl->AddLine(ImVec2(tick_x, price_min.y), ImVec2(tick_x, vol_max.y), Color::DimLine, 0.6f);

        char time_str[32];
        FormatCandleTime(tick_candle_idx, data[tick_candle_idx], time_str, sizeof(time_str));
        ImVec2 text_sz = ImGui::CalcTextSize(time_str);
        dl->AddText(ImVec2(tick_x - text_sz.x * 0.5f, vol_max.y + 5.0f), Color::InkCyan, time_str);
      }
    }

    // =================================================================
    // 裁剪渲染：K 线实体主图 + 成交量柱状副图
    // =================================================================
    const float candle_width = std::max(2.0f, slot_width * 0.68f);
    int hovered_idx = -1;
    float hovered_center_x = 0.0f;

    // A. 绘制主图 (Price Candles)
    dl->PushClipRect(price_min, price_max, true);
    for (int i = 0; i < total_count; ++i) {
      const float center_x = price_min.x + (static_cast<float>(i) - current_state->offset + 0.5f) * slot_width;
      if (center_x < price_min.x - candle_width || center_x > price_max.x + candle_width) continue;

      const Candle& c = data[i];
      const bool is_bull = c.close >= c.open;

      if (is_hovered && io.MousePos.x >= (center_x - slot_width * 0.5f) && io.MousePos.x < (center_x + slot_width * 0.5f)) {
        hovered_idx = i;
        hovered_center_x = center_x;
      }

      const float y_high = PriceToY(c.high);
      const float y_low  = PriceToY(c.low);
      float y_open = PriceToY(c.open);
      float y_close = PriceToY(c.close);
      float y_top = std::min(y_open, y_close);
      float y_bot = std::max(y_open, y_close);
      if (y_bot - y_top < 2.0f) y_bot = y_top + 2.0f;

      const ImVec2 b_min(center_x - candle_width * 0.5f, y_top);
      const ImVec2 b_max(center_x + candle_width * 0.5f, y_bot);

      dl->AddLine(ImVec2(center_x, y_high), ImVec2(center_x, y_low), Color::InkWhite, 1.0f);

      if (is_bull) {
        dl->AddRectFilled(b_min, b_max, Color::CandleBull);
        dl->AddRect(b_min, b_max, Color::InkWhite, 0.0f, 0, 1.0f);
      } else {
        dl->AddRectFilled(b_min, b_max, Color::PanelBg);
        DrawHatch(dl, b_min, b_max, Color::InkWhite, 4.0f);
        dl->AddRect(b_min, b_max, Color::InkWhite, 0.0f, 0, 1.0f);
      }
    }
    dl->PopClipRect();

    // B. ★ 绘制副图 (Volume Bars 成交量柱状图)
    dl->PushClipRect(vol_min, vol_max, true);
    for (int i = 0; i < total_count; ++i) {
      const float center_x = vol_min.x + (static_cast<float>(i) - current_state->offset + 0.5f) * slot_width;
      if (center_x < vol_min.x - candle_width || center_x > vol_max.x + candle_width) continue;

      const Candle& c = data[i];
      const bool is_bull = c.close >= c.open;

      float v_top = VolToY(c.volume);
      float v_bot = vol_max.y;
      if (v_bot - v_top < 1.0f) v_top = v_bot - 1.0f; // 最低保底 1 像素高

      const ImVec2 vb_min(center_x - candle_width * 0.5f, v_top);
      const ImVec2 vb_max(center_x + candle_width * 0.5f, v_bot);

      if (is_bull) {
        // 阳线放量：蓝图实心青/白色描边
        dl->AddRectFilled(vb_min, vb_max, Color::CandleBull);
        dl->AddRect(vb_min, vb_max, Color::InkWhite, 0.0f, 0, 1.0f);
      } else {
        // 阴线缩/放量：蓝图剖面线 + 白色描边
        dl->AddRectFilled(vb_min, vb_max, Color::PanelBg);
        DrawHatch(dl, vb_min, vb_max, Color::InkWhite, 4.0f);
        dl->AddRect(vb_min, vb_max, Color::InkWhite, 0.0f, 0, 1.0f);
      }
    }
    dl->PopClipRect();

    // =================================================================
    // 十字光标贯穿线、时间悬浮标牌与行情卡 (Tooltip)
    // =================================================================
    if (hovered_idx != -1) {
      const Candle& c = data[hovered_idx];

      // 十字光标竖线同时贯穿主副图
      dl->AddLine(ImVec2(hovered_center_x, price_min.y), ImVec2(hovered_center_x, vol_max.y), Color::InkWhite, 1.0f);

      // 底部时间 Badge
      char hover_time[32];
      FormatCandleTime(hovered_idx, c, hover_time, sizeof(hover_time));
      ImVec2 time_sz = ImGui::CalcTextSize(hover_time);
      const float badge_w = time_sz.x + 12.0f;
      const float badge_h = time_sz.y + 4.0f;
      const ImVec2 badge_min(hovered_center_x - badge_w * 0.5f, vol_max.y + 2.0f);
      const ImVec2 badge_max(hovered_center_x + badge_w * 0.5f, vol_max.y + 2.0f + badge_h);

      dl->AddRectFilled(badge_min, badge_max, Color::InkWhite);
      dl->AddText(ImVec2(badge_min.x + 6.0f, badge_min.y + 2.0f), Color::Bg, hover_time);

      // 详细浮窗 (含价格、成交量与成交额)
      char vol_fmt[32];
      FormatVolumeText(c.volume, vol_fmt, sizeof(vol_fmt));

      ImGui::BeginTooltip();
      ImGui::PushStyleColor(ImGuiCol_PopupBg, Color::PanelBg);
      ImGui::PushStyleColor(ImGuiCol_Border, Color::InkWhite);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);

      ImGui::TextColored(Color::Vec4Cyan, "时间节点: [%s]  序号: #%04d", hover_time, hovered_idx + 1);
      ImGui::Separator();
      ImGui::TextColored(Color::Vec4White, "开盘价 (OPEN):  %.2f", c.open);
      ImGui::TextColored(Color::Vec4White, "最高价 (HIGH):  %.2f", c.high);
      ImGui::TextColored(Color::Vec4White, "最低价 (LOW):   %.2f", c.low);
      ImGui::TextColored(Color::Vec4Cyan,  "收盘价 (CLOSE): %.2f", c.close);
      ImGui::TextColored(Color::Vec4Dim,   "涨跌幅 (CHG):   %+.2f%%", ((c.close - c.open) / (c.open <= 0 ? 1 : c.open)) * 100.0f);
      ImGui::Separator();
      ImGui::TextColored(Color::Vec4White, "成交量 (VOL):   %s 手 (%.0f)", vol_fmt, c.volume);
      if (c.amount > 0) {
        ImGui::TextColored(Color::Vec4Dim, "成交额 (AMT):   %.2f 万元", c.amount / 10000.0f);
      }

      ImGui::PopStyleVar();
      ImGui::PopStyleColor(2);
      ImGui::EndTooltip();
    }
  }

} // namespace blueprint