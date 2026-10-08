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
#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "color.hpp"
#include <vector>
#include <string>
#include <cstdio>

namespace blueprint {
  /**
   * @brief 渲染蓝图工程线框风格的分段选择器（Segmented Control / 一体化单选组）。
   *
   * 视觉特性与逻辑流程 (Logic Flow):
   * 1. 尺寸自适应：支持根据父容器可用区域宽度自动等分每个分段（Segment）。
   * 2. 连续线框结构：外围绘制 1px 纯白无缝闭合矩形，内部各段之间绘制 1px 垂直工程分割线。
   * 3. 激活状态高亮：选中项采用纯白填充底色 + 深蓝文字的反色设计，未选中项采用深蓝半透明底板。
   * 4. 局部 ID 隔离：内部推入 PushID(label)，防止多个同名分段选择器产生事件冒泡与哈希碰撞。
   *
   * @param label          控件唯一标识与分组名（如 "##line_style"）。
   * @param selected_index 指向当前激活选项索引的指针（用户切换后被就地更新）。
   * @param items          选项文本的 C 字符串指针数组。
   * @param items_count    分段选项总数。
   * @param width          选择器总像素宽度（默认 -1.0f 自动填满当前行可用区域）。
   * @param height         选择器像素高度（默认 28.0f）。
   *
   * @return bool - 若用户点击并切换了当前激活项则返回 true，否则返回 false。
   */
  inline bool Segments(const char* label, int* selected_index, const char* const items[], int items_count, float width = -1.0f, float height = 28.0f) {
    if (!selected_index || !items || items_count <= 0) return false;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    // 1. 确定控件实际宽度与高度
    float total_width = width;
    if (total_width <= 0.0f) {
      total_width = ImGui::GetContentRegionAvail().x;
    }
    if (total_width <= 0.0f) {
      total_width = 120.0f; // 保护兜底宽度
    }

    const ImVec2 pos = window->DC.CursorPos;
    const ImVec2 total_size(total_width, height);
    const ImRect total_bb(pos, ImVec2(pos.x + total_size.x, pos.y + total_size.y));

    // 2. 在 ImGui 布局中占位
    ImGui::ItemSize(total_bb);
    if (!ImGui::ItemAdd(total_bb, window->GetID(label))) return false;

    ImDrawList* dl = window->DrawList;
    bool value_changed = false;
    const float seg_width = total_width / static_cast<float>(items_count);

    ImGui::PushID(label);

    // 3. 逐个渲染每个分段的交互热区与视觉状态
    for (int i = 0; i < items_count; ++i) {
      const ImVec2 seg_min(pos.x + i * seg_width, pos.y);
      const ImVec2 seg_max(pos.x + (i + 1) * seg_width, pos.y + height);
      const ImRect seg_bb(seg_min, seg_max);

      char seg_id[32];
      snprintf(seg_id, sizeof(seg_id), "seg_%d", i);
      const ImGuiID btn_id = window->GetID(seg_id);

      bool hovered = false;
      bool held = false;
      bool pressed = ImGui::ButtonBehavior(seg_bb, btn_id, &hovered, &held);

      if (pressed && *selected_index != i) {
        *selected_index = i;
        value_changed = true;
      }

      const bool is_active = (*selected_index == i);

      // A. 绘制分段背景
      if (is_active) {
        // 激活态：白色实心填充
        dl->AddRectFilled(seg_min, seg_max, Color::InkWhite);
      } else if (hovered) {
        // 悬停态：浅蓝悬停色
        dl->AddRectFilled(seg_min, seg_max, Color::Hovered);
      } else {
        // 常态：半透明底板
        dl->AddRectFilled(seg_min, seg_max, Color::PanelBg);
      }

      // B. 绘制居中文字
      const ImVec2 text_size = ImGui::CalcTextSize(items[i]);
      const ImVec2 text_pos(
        seg_min.x + (seg_width - text_size.x) * 0.5f,
        seg_min.y + (height - text_size.y) * 0.5f
      );
      dl->AddText(text_pos, is_active ? Color::Bg : Color::InkWhite, items[i]);

      // C. 绘制分段之间的垂直分割线 (最后一截不画)
      if (i < items_count - 1) {
        dl->AddLine(ImVec2(seg_max.x, seg_min.y), ImVec2(seg_max.x, seg_max.y), Color::InkWhite, 1.0f);
      }
    }

    ImGui::PopID();

    // 4. 绘制最外层闭合矩形线框 (1.2px)
    dl->AddRect(total_bb.Min, total_bb.Max, Color::InkWhite, 0.0f, 0, 1.2f);

    return value_changed;
  }

  /**
   * @brief 渲染蓝图工程线框风格的分段选择器（std::vector<const char*> 便捷重载）。
   *
   * @param label          控件唯一标识符。
   * @param selected_index 指向当前激活选项索引的指针。
   * @param items          选项文本的 const char* 向量容器。
   * @param width          选择器总像素宽度（默认 -1.0f 自动填满可用区域）。
   * @param height         选择器像素高度（默认 28.0f）。
   *
   * @return bool - 若用户切换了选中项则返回 true，否则返回 false。
   */
  inline bool Segments(const char* label, int* selected_index, const std::vector<const char*>& items, float width = -1.0f, float height = 28.0f) {
    return Segments(label, selected_index, items.data(), static_cast<int>(items.size()), width, height);
  }

  /**
   * @brief 渲染蓝图工程线框风格的分段选择器（std::vector<std::string> 便捷重载）。
   *
   * @param label          控件唯一标识符。
   * @param selected_index 指向当前激活选项索引的指针。
   * @param items          选项文本的 std::string 动态向量容器。
   * @param width          选择器总像素宽度（默认 -1.0f 自动填满可用区域）。
   * @param height         选择器像素高度（默认 28.0f）。
   *
   * @return bool - 若用户切换了选中项则返回 true，否则返回 false。
   */
  inline bool Segments(const char* label, int* selected_index, const std::vector<std::string>& items, float width = -1.0f, float height = 28.0f) {
    if (!selected_index || items.empty()) return false;

    std::vector<const char*> item_ptrs;
    item_ptrs.reserve(items.size());
    for (const auto& item : items) {
      item_ptrs.push_back(item.c_str());
    }

    return Segments(label, selected_index, item_ptrs.data(), static_cast<int>(item_ptrs.size()), width, height);
  }
}