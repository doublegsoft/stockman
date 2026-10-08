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
#include <string>

namespace blueprint {
  namespace detail {
    // 供 std::string 动态扩容使用的内部回调载体
    struct InputTextStringCallbackData {
      std::string* str;
      ImGuiInputTextCallback chain_callback;
      void* chain_callback_user_data;
    };

    inline int InputTextStringCallback(ImGuiInputTextCallbackData* data) {
      auto* user_data = static_cast<InputTextStringCallbackData*>(data->UserData);
      if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        std::string* str = user_data->str;
        str->resize(data->BufTextLen);
        data->Buf = const_cast<char*>(str->c_str());
      } else if (user_data->chain_callback) {
        data->UserData = user_data->chain_callback_user_data;
        return user_data->chain_callback(data);
      }
      return 0;
    }
  }

  /**
   * @brief 渲染蓝图线框风格的单行文本输入框（C 字符数组底层实现）。
   *
   * 视觉特性与逻辑流程 (Logic Flow):
   * 1. 样式隔离：推入蓝图专属的 0 圆角、1px 纯白描边、深蓝半透明底板及选中文本高亮色。
   * 2. 宽度响应：支持 -1.0f 自动撑满可用父容器宽度，或传像素绝对值。
   * 3. 渲染完成后立即弹出样式栈，避免污染后续普通控件。
   *
   * @param label    控件唯一标识（如 "##detail"）。
   * @param buf      指向字符缓冲区的指针。
   * @param buf_size 缓冲区最大容量（字节数）。
   * @param width    输入框像素宽度（默认 -1.0f 自动撑满）。
   * @param flags    ImGui 原生输入框配置标记（如 ImGuiInputTextFlags_Password 等）。
   *
   * @return bool - 若用户输入导致文本内容变更则返回 true，否则返回 false。
   */
  inline bool TextInput(const char* label, char* buf, size_t buf_size, float width = -1.0f, ImGuiInputTextFlags flags = 0) {
    if (!buf || buf_size == 0) return false;

    if (width != 0.0f) {
      ImGui::SetNextItemWidth(width);
    }

    // 注入蓝图线框主题
    ImGui::PushStyleColor(ImGuiCol_FrameBg, Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Color::Hovered);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Color::Active);
    ImGui::PushStyleColor(ImGuiCol_Border, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_Text, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, Color::Selected);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

    const bool changed = ImGui::InputText(label, buf, buf_size, flags);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(6);

    return changed;
  }

  /**
   * @brief 渲染蓝图线框风格的单行文本输入框（std::string 动态字符串便捷重载）。
   *
   * 内部自动处理动态扩容回调，调用方无需指定固定长度 buffer。
   *
   * @param label 控件唯一标识。
   * @param str   可变引用的 std::string 实例。
   * @param width 输入框像素宽度（默认 -1.0f 自动撑满）。
   * @param flags ImGui 原生输入框配置标记。
   *
   * @return bool - 若内容发生变化则返回 true，否则返回 false。
   */
  inline bool TextInput(const char* label, std::string& str, float width = -1.0f, ImGuiInputTextFlags flags = 0) {
    if (width != 0.0f) {
      ImGui::SetNextItemWidth(width);
    }

    detail::InputTextStringCallbackData user_data;
    user_data.str = &str;
    user_data.chain_callback = nullptr;
    user_data.chain_callback_user_data = nullptr;

    flags |= ImGuiInputTextFlags_CallbackResize;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Color::Hovered);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Color::Active);
    ImGui::PushStyleColor(ImGuiCol_Border, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_Text, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, Color::Selected);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

    const bool changed = ImGui::InputText(
      label, 
      const_cast<char*>(str.c_str()), 
      str.capacity() + 1, 
      flags, 
      detail::InputTextStringCallback, 
      &user_data
    );

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(6);

    return changed;
  }

  /**
   * @brief 渲染蓝图线框风格的多行文本输入框（C 字符数组实现，对应参考图中带缩放手柄的 LABEL 框）。
   *
   * @param label    控件唯一标识。
   * @param buf      指向字符缓冲区的指针。
   * @param buf_size 缓冲区最大容量。
   * @param size     多行文本框的尺寸（宽, 高），宽为 -1.0f 时撑满父级容器。
   * @param flags    ImGui 原生输入框标记。
   *
   * @return bool - 若内容改变返回 true。
   */
  inline bool TextInputMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(-1.0f, 64.0f), ImGuiInputTextFlags flags = 0) {
    if (!buf || buf_size == 0) return false;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Color::Hovered);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Color::Active);
    ImGui::PushStyleColor(ImGuiCol_Border, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_Text, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, Color::Selected);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

    const bool changed = ImGui::InputTextMultiline(label, buf, buf_size, size, flags);

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(6);

    return changed;
  }

  /**
   * @brief 渲染蓝图线框风格的多行文本输入框（std::string 便捷重载）。
   *
   * @param label 控件唯一标识。
   * @param str   可变引用的 std::string 实例。
   * @param size  多行文本框尺寸（宽, 高）。
   * @param flags ImGui 原生输入框标记。
   *
   * @return bool - 若内容改变返回 true。
   */
  inline bool TextInputMultiline(const char* label, std::string& str, const ImVec2& size = ImVec2(-1.0f, 64.0f), ImGuiInputTextFlags flags = 0) {
    detail::InputTextStringCallbackData user_data;
    user_data.str = &str;
    user_data.chain_callback = nullptr;
    user_data.chain_callback_user_data = nullptr;

    flags |= ImGuiInputTextFlags_CallbackResize;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Color::PanelBg);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Color::Hovered);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, Color::Active);
    ImGui::PushStyleColor(ImGuiCol_Border, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_Text, Color::InkWhite);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, Color::Selected);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));

    const bool changed = ImGui::InputTextMultiline(
      label, 
      const_cast<char*>(str.c_str()), 
      str.capacity() + 1, 
      size, 
      flags, 
      detail::InputTextStringCallback, 
      &user_data
    );

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(6);

    return changed;
  }
}