#pragma once

#include <imgui.h>

namespace Hachiko
{
    class Component;

    namespace ImGuiUtils
    {
        constexpr float TOOLTIP_TEXT_SIZE = 35.0f;

        bool IconButton(const char* icon, const char* tooltip);

        void VSplitter(const char* str_id, ImVec2* size);

        void Rect(float w, float h, ImU32 color);

        bool CompactColorPicker(const std::string& name, float* color, float width = 150.0f);

        bool CompactOpaqueColorPicker(const std::string& name, float* color, float width = 150.0f);

        bool CollapsingHeader(Component* component, const char* header_name);

        bool ToolbarButton(ImFont* const font, const char* font_icon, bool active, const char* tooltip_desc, const bool enabled = true);

        void DisplayTooltip(const char* desc);

        void Combo(const char* label, std::vector<std::string> list, unsigned& index);

    }; // namespace ImGuiUtils
}
