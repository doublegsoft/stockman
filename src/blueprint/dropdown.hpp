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
#include "color.hpp"
#include <vector>
#include <string>

namespace blueprint {
  /**
   * @brief 渲染蓝图线框风格的组合下拉框（C 原生数组底层核心实现）。
   *
   * 自动注入并隔离蓝图设计系统规范（直角 0 圆角、1px 纯白工程线框、半透明深蓝底板、选中高亮态），
   * 绘制结束后立即弹出样式状态栈，不影响外部其他 ImGui 控件。
   *
   * @param label        控件标签与唯一标识符（若不需要显式文字可传 "##id"）。
   * @param current_item 指向当前选中项索引的指针（用户选择后会被就地更新）。
   * @param items        指向选项文本的 C 字符串指针数组。
   * @param items_count  选项总项数。
   * @param width        控件宽度（默认 -1.0f 自动撑满父容器可用宽度；传 > 0 为固定像素宽度）。
   *
   * @return bool - 若用户在本帧点击并更改了选中项则返回 true，否则返回 false。
   */
  inline bool Dropdown(const char* label, int* current_item, const char* const items[], int items_count, float width = -1.0f) {
    if (!current_item || !items || items_count <= 0) return false;

    bool value_changed = false;

    // 1. 设置宽度 (-1.0f 自动撑满父容器)
    if (width != 0.0f) {
      ImGui::SetNextItemWidth(width);
    }

    // 2. 注入蓝图线框主题（统一读取 Blueprint::Color 类静态常量）
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Color::Hovered);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Color::Active);
    ImGui::PushStyleColor(ImGuiCol_Border, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_Text, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_Header, Color::Selected);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Color::SelectedHover);
    ImGui::PushStyleColor(ImGuiCol_Button, Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Color::Hovered);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.2f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

    // 3. 计算当前预览文案
    const char* preview_value = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "";

    // 4. 渲染组合下拉框与弹出项
    if (ImGui::BeginCombo(label, preview_value, ImGuiComboFlags_None)) {
      for (int i = 0; i < items_count; ++i) {
        const bool is_selected = (*current_item == i);
        if (ImGui::Selectable(items[i], is_selected)) {
          *current_item = i;
          value_changed = true;
        }

        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    // 5. 弹出样式状态栈，恢复上下文环境
    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(10);

    return value_changed;
  }

  /**
   * @brief 渲染蓝图线框风格的组合下拉框（std::vector<const char*> 便捷重载）。
   *
   * @param label        控件标签与唯一标识符。
   * @param current_item 指向当前选中项索引的指针。
   * @param items        包含选项文本的 const char* 向量容器。
   * @param width        控件宽度（默认 -1.0f 自动撑满父容器）。
   *
   * @return bool - 若用户点击切换了选中项则返回 true，否则返回 false。
   */
  inline bool Dropdown(const char* label, int* current_item, const std::vector<const char*>& items, float width = -1.0f) {
    return Dropdown(label, current_item, items.data(), static_cast<int>(items.size()), width);
  }

  /**
   * @brief 渲染蓝图线框风格的组合下拉框（std::vector<std::string> 便捷重载）。
   *
   * 自动在内部提取各 std::string 的底层字符指针传递给底层绘制逻辑，避免调用方手动构造指针数组。
   *
   * @param label        控件标签与唯一标识符。
   * @param current_item 指向当前选中项索引的指针。
   * @param items        包含动态字符串选项的 std::string 向量容器。
   * @param width        控件宽度（默认 -1.0f 自动撑满父容器）。
   *
   * @return bool - 若用户点击切换了选中项则返回 true，否则返回 false。
   */
  inline bool Dropdown(const char* label, int* current_item, const std::vector<std::string>& items, float width = -1.0f) {
    if (!current_item || items.empty()) return false;

    std::vector<const char*> item_ptrs;
    item_ptrs.reserve(items.size());
    for (const auto& item : items) {
      item_ptrs.push_back(item.c_str());
    }

    return Dropdown(label, current_item, item_ptrs.data(), static_cast<int>(item_ptrs.size()), width);
  }
}