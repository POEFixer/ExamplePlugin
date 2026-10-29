// Plugins/ExamplePlugin/examples/ExampleRender.h
// Demonstrates every RenderService method.

#pragma once
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "SdkStatus.h"
#include "Text.h"

namespace Examples {

// One MapTransform's fields (the large map's or the mini map's).
inline void DrawMapTransform(const PluginSDK::MapTransform& t) {
    Show(I18n::TrF("render_visible", { { "value", YesNo(t.IsVisible) } }));
    Show(I18n::TrF("render_center", { { "x", Num("%.2f", t.CenterX) }, { "y", Num("%.2f", t.CenterY) } }));
    Show(I18n::TrF("render_scale", { { "x", Num("%.4f", t.ScaleX) }, { "y", Num("%.4f", t.ScaleY) } }));
    Show(I18n::TrF("render_player_origin", { { "x", Num("%.2f", t.PlayerGridX) },
                                             { "y", Num("%.2f", t.PlayerGridY) } }));
}

// The headers name the RenderService methods (SDK names).
inline void DrawRenderPanel(const PluginSDK::Context* ctx,
                             const PluginSDK::Snapshot& snap)
{
    if (!ctx) { StatusBadge(StatusLevel::Fail, I18n::Tr("common_context_missing")); return; }

    if (snap.State != PluginSDK::GameState::InGame || !snap.Player.IsValid) {
        StatusBadge(StatusLevel::Warn, I18n::Tr("render_enter_game"));
        return;
    }
    StatusBadge(StatusLevel::Ok, I18n::Tr("render_ready"));
    ImGui::Separator();

    if (ImGui::CollapsingHeader("WorldToScreen", ImGuiTreeNodeFlags_DefaultOpen)) {
        float sx = 0, sy = 0;
        bool ok = ctx->Render.WorldToScreen(
            snap.Player.WorldX, snap.Player.WorldY,
            snap.Player.WorldZ, sx, sy);
        if (ok) {
            Show(I18n::TrF("render_world_to_screen", { { "x", Num("%.1f", snap.Player.WorldX) },
                                                       { "y", Num("%.1f", snap.Player.WorldY) },
                                                       { "z", Num("%.1f", snap.Player.WorldZ) },
                                                       { "sx", Num("%.0f", sx) },
                                                       { "sy", Num("%.0f", sy) } }));
            bool onScreen = sx >= 0 && sx < snap.ScreenWidth
                         && sy >= 0 && sy < snap.ScreenHeight;
            StatusBadge(onScreen ? StatusLevel::Ok : StatusLevel::Warn,
                onScreen ? I18n::Tr("render_on_screen") : I18n::Tr("render_off_screen"));
        } else {
            StatusBadge(StatusLevel::Warn, I18n::Tr("render_projection_failed"));
        }
    }

    if (ImGui::CollapsingHeader("GridToLargeMap")) {
        if (!snap.LargeMap.IsVisible) {
            StatusBadge(StatusLevel::Warn, I18n::Tr("render_open_large_map"));
        } else {
            float sx = 0, sy = 0;
            bool ok = ctx->Render.GridToLargeMap(
                snap.Player.GridPositionX, snap.Player.GridPositionY,
                snap.Player.TerrainHeight, sx, sy);
            if (ok) {
                Show(I18n::TrF("render_grid_to_large_map", { { "x", Num("%.1f", snap.Player.GridPositionX) },
                                                             { "y", Num("%.1f", snap.Player.GridPositionY) },
                                                             { "sx", Num("%.0f", sx) },
                                                             { "sy", Num("%.0f", sy) } }));
                StatusBadge(StatusLevel::Ok, I18n::Tr("render_projected"));
            } else {
                StatusBadge(StatusLevel::Fail,
                    I18n::TrF("render_returned_false", { { "function", "GridToLargeMap" } }).c_str());
            }
        }
    }

    if (ImGui::CollapsingHeader("GridToMiniMap")) {
        if (!snap.MiniMap.IsVisible) {
            StatusBadge(StatusLevel::Warn, I18n::Tr("render_mini_map_hidden"));
        } else {
            float sx = 0, sy = 0;
            bool ok = ctx->Render.GridToMiniMap(
                snap.Player.GridPositionX, snap.Player.GridPositionY,
                snap.Player.TerrainHeight, sx, sy);
            if (ok) {
                Show(I18n::TrF("render_grid_to_mini_map", { { "x", Num("%.1f", snap.Player.GridPositionX) },
                                                            { "y", Num("%.1f", snap.Player.GridPositionY) },
                                                            { "sx", Num("%.0f", sx) },
                                                            { "sy", Num("%.0f", sy) } }));
                StatusBadge(StatusLevel::Ok, I18n::Tr("render_projected"));
            } else {
                StatusBadge(StatusLevel::Fail,
                    I18n::TrF("render_returned_false", { { "function", "GridToMiniMap" } }).c_str());
            }
        }
    }

    if (ImGui::CollapsingHeader(I18n::TrId("render_large_map_transform"))) {
        DrawMapTransform(ctx->Render.GetLargeMapTransform());
    }

    if (ImGui::CollapsingHeader(I18n::TrId("render_mini_map_transform"))) {
        DrawMapTransform(ctx->Render.GetMiniMapTransform());
    }
}

} // namespace Examples
