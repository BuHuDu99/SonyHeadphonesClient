#pragma once
#include "imgui/imgui.h"

namespace UiTheme {
inline constexpr ImVec4 canvas{.965f, .965f, .976f, 1};
inline constexpr ImVec4 surface{1, 1, 1, 1};
inline constexpr ImVec4 subtle{.945f, .941f, .961f, 1};
inline constexpr ImVec4 ink{.15f, .14f, .19f, 1};
inline constexpr ImVec4 muted{.38f, .36f, .43f, 1};
inline constexpr ImVec4 accent{.37f, .28f, .61f, 1};
inline constexpr ImVec4 accentHover{.30f, .22f, .52f, 1};
inline constexpr ImVec4 accentTint{.91f, .89f, .96f, 1};
inline constexpr ImVec4 border{.85f, .84f, .88f, 1};
inline constexpr ImVec4 success{.16f, .40f, .31f, 1};
inline constexpr ImVec4 player{.16f, .15f, .20f, 1};
inline constexpr ImVec4 onPlayer{.97f, .96f, .99f, 1};
inline constexpr ImVec4 playerMuted{.78f, .76f, .83f, 1};
inline constexpr float radius = 12;
inline bool reduceMotion = false;

inline void apply() {
    auto& s = ImGui::GetStyle();
    s.WindowPadding = {20, 18}; s.FramePadding = {12, 8};
    s.ItemSpacing = {12, 10}; s.CellPadding = {6, 4};
    s.ChildRounding = radius; s.FrameRounding = 8; s.PopupRounding = 10;
    s.WindowBorderSize = 0; s.ChildBorderSize = 1; s.FrameBorderSize = 0;
    s.ScrollbarSize = 8; s.GrabRounding = 8; s.DisabledAlpha = .48f;
    auto& c = s.Colors;
    c[ImGuiCol_WindowBg] = canvas; c[ImGuiCol_ChildBg] = surface;
    c[ImGuiCol_PopupBg] = surface; c[ImGuiCol_Text] = ink;
    c[ImGuiCol_TextDisabled] = muted; c[ImGuiCol_Border] = border;
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_Button] = subtle; c[ImGuiCol_ButtonHovered] = accentTint;
    c[ImGuiCol_ButtonActive] = { .84f, .81f, .92f, 1 };
    c[ImGuiCol_Header] = accentTint; c[ImGuiCol_HeaderHovered] = subtle;
    c[ImGuiCol_HeaderActive] = accentTint;
    c[ImGuiCol_FrameBg] = subtle; c[ImGuiCol_FrameBgHovered] = accentTint;
    c[ImGuiCol_FrameBgActive] = accentTint;
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accentHover;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_TextSelectedBg] = {.76f, .70f, .89f, .55f};
}
}
