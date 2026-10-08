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

namespace blueprint {
  /**
   * @brief 渲染具有经典工程蓝图（CAD Blueprint）线框风格的操作按钮。
   *
   * 视觉特性与逻辑流程 (Logic Flow):
   * 1. 尺寸自适应：支持传 ImVec2(0, 0)，此时组件会根据文本内容长度自适应计算尺寸（水平增加 24px 内边距，默认高度 30px）。
   * 2. 状态渲染：
   *    - 激活态 / 按住态 (Active / Held)：纯白实心填充矩形，文字呈深蓝底色（反相高亮设计）。
   *    - 悬停态 (Hovered)：浅蓝半透明底色，1.2px 纯白线框描边。
   *    - 常态 (Normal)：深蓝半透明工程底板，1.2px 纯白线框描边，纯白居中文本。
   *
   * @param label     按钮文本标签与唯一标识符。
   * @param size      按钮物理尺寸 (宽, 高)；传 ImVec2(0, 0) 时自动根据文字自适应。
   * @param is_active 是否作为常驻激活/选中项渲染（true 时呈现反白高亮外观）。
   *
   * @return bool - 若用户在本帧点击了该按钮则返回 true，否则返回 false。
   */
  inline bool Button(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f), bool is_active = false) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    const ImVec2 pos = window->DC.CursorPos;

    // 1. 尺寸计算与自适应测量
    ImVec2 actual_size = size;
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

    if (actual_size.x <= 0.0f) {
      actual_size.x = label_size.x + 24.0f;
    }
    if (actual_size.y <= 0.0f) {
      actual_size.y = 30.0f;
    }

    const ImRect bb(pos, ImVec2(pos.x + actual_size.x, pos.y + actual_size.y));

    // 2. 注册布局并监听交互行为
    ImGui::ItemSize(actual_size);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered = false;
    bool held = false;
    const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    // 3. 蓝图风格渲染绘制
    if (is_active || held) {
      // 激活态 / 按下态：白色实心填充，深蓝字
      dl->AddRectFilled(bb.Min, bb.Max, Color::InkWhite);
      const ImVec2 text_pos(
        bb.Min.x + (actual_size.x - label_size.x) * 0.5f,
        bb.Min.y + (actual_size.y - label_size.y) * 0.5f
      );
      dl->AddText(text_pos, Color::Bg, label);
    } else {
      // 常态 / 悬停态：线框描边
      const ImU32 fill_col = hovered ? Color::Hovered : Color::PanelBg;
      dl->AddRectFilled(bb.Min, bb.Max, fill_col);
      dl->AddRect(bb.Min, bb.Max, Color::InkWhite, 0.0f, 0, 1.2f);

      const ImVec2 text_pos(
        bb.Min.x + (actual_size.x - label_size.x) * 0.5f,
        bb.Min.y + (actual_size.y - label_size.y) * 0.5f
      );
      dl->AddText(text_pos, Color::InkWhite, label);
    }

    return pressed;
  }
}