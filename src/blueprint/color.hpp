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

namespace blueprint {
  class Color {
  public:
    // 纯静态类，禁用实例化与拷贝
    Color() = delete;
    ~Color() = delete;
    Color(const Color&) = delete;
    Color& operator=(const Color&) = delete;

    // =================================================================
    // 32 位整型颜色 (ImU32) - 供 ImDrawList 矢量图元绘制使用
    // =================================================================
    static constexpr ImU32 Bg            = IM_COL32(16, 54, 115, 255);   // #103673 经典蓝图图纸底色
    static constexpr ImU32 PanelBg       = IM_COL32(12, 42, 92, 230);    // 容器与弹出框半透明底板
    static constexpr ImU32 GridFine      = IM_COL32(28, 76, 148, 160);   // 细刻度网格线 (20px)
    static constexpr ImU32 GridBold      = IM_COL32(42, 102, 185, 220);  // 粗定位网格线 (100px)
    static constexpr ImU32 InkWhite      = IM_COL32(245, 250, 255, 255); // 纯白工程墨线
    static constexpr ImU32 InkCyan       = IM_COL32(145, 205, 255, 255); // 标注、尺寸辅助青线
    static constexpr ImU32 DimLine       = IM_COL32(75, 135, 210, 160);  // 弱参考标尺线
    static constexpr ImU32 Hovered       = IM_COL32(22, 68, 140, 255);   // 控件悬停交互色
    static constexpr ImU32 Active        = IM_COL32(32, 88, 175, 255);   // 控件按下激活色
    static constexpr ImU32 Selected      = IM_COL32(42, 102, 185, 220);  // 列表/菜单选中高亮
    static constexpr ImU32 SelectedHover = IM_COL32(65, 135, 230, 240);  // 选中项悬停色
    static constexpr ImU32 CandleBull    = IM_COL32(60, 130, 220, 180);  // 阳线半透明填充
    static constexpr ImU32 Transparent   = IM_COL32(0, 0, 0, 0);

    // =================================================================
    // 浮点四元颜色 (ImVec4) - 供 ImGui 控件、文本、PushStyleColor 使用
    // =================================================================
    static inline const ImVec4 Vec4Bg      = ImVec4(16.0f / 255.0f, 54.0f / 255.0f, 115.0f / 255.0f, 1.0f);
    static inline const ImVec4 Vec4PanelBg = ImVec4(12.0f / 255.0f, 42.0f / 255.0f, 92.0f / 255.0f, 230.0f / 255.0f);
    static inline const ImVec4 Vec4White   = ImVec4(245.0f / 255.0f, 250.0f / 255.0f, 255.0f / 255.0f, 1.0f);
    static inline const ImVec4 Vec4Cyan    = ImVec4(145.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f);
    static inline const ImVec4 Vec4Dim     = ImVec4(75.0f / 255.0f, 135.0f / 255.0f, 210.0f / 255.0f, 1.0f);

    // =================================================================
    // 便捷类型转换工具
    // =================================================================
    static inline ImVec4 ToVec4(ImU32 col) {
      return ImGui::ColorConvertU32ToFloat4(col);
    }

    static inline ImU32 ToU32(const ImVec4& col) {
      return ImGui::ColorConvertFloat4ToU32(col);
    }
  };
}