// Plugins/ExamplePlugin/examples/ExampleTerrain.h
// Demonstrates every TerrainService method.

#pragma once
#include <cmath>
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "SdkStatus.h"
#include "Text.h"

namespace Examples {

inline void DrawTerrainPanel(const PluginSDK::Context* ctx,
                              const PluginSDK::Snapshot& snap)
{
    if (!ctx) { StatusBadge(StatusLevel::Fail, I18n::Tr("common_context_missing")); return; }

    if (ImGui::CollapsingHeader(I18n::TrId("terrain_walkable_grid"), ImGuiTreeNodeFlags_DefaultOpen)) {
        PluginSDK::WalkableGridHandle h = ctx->Terrain.GetWalkableGrid();
        if (h.Valid()) {
            Show(I18n::TrF("terrain_width", { { "value", Num("%d", h.Width()) } }));
            Show(I18n::TrF("terrain_height", { { "value", Num("%d", h.Height()) } }));
            Show(I18n::TrF("terrain_size_bytes", { { "value", Num("%zu", h.SizeBytes()) } }));
            Show(I18n::TrF("terrain_data_pointer", { { "pointer", Num("0x%p", static_cast<const void*>(h.Data())) } }));
            StatusBadge(StatusLevel::Ok, I18n::Tr("terrain_grid_loaded"));
        } else {
            StatusBadge(StatusLevel::Warn, I18n::Tr("terrain_walkable_not_loaded"));
        }
    }

    if (ImGui::CollapsingHeader(I18n::TrId("terrain_height_grid"))) {
        PluginSDK::HeightGridHandle h = ctx->Terrain.GetHeightGrid();
        if (h.Valid()) {
            Show(I18n::TrF("terrain_width", { { "value", Num("%d", h.Width()) } }));
            Show(I18n::TrF("terrain_height", { { "value", Num("%d", h.Height()) } }));
            Show(I18n::TrF("terrain_element_count", { { "value", Num("%zu", h.ElementCount()) } }));
            Show(I18n::TrF("terrain_data_pointer", { { "pointer", Num("0x%p", static_cast<const void*>(h.Data())) } }));
            StatusBadge(StatusLevel::Ok, I18n::Tr("terrain_height_loaded"));
        } else {
            StatusBadge(StatusLevel::Warn, I18n::Tr("terrain_height_not_loaded"));
        }
    }

    if (ImGui::CollapsingHeader(I18n::TrId("terrain_per_tile"))) {
        int gx = static_cast<int>(snap.Player.GridPositionX);
        int gy = static_cast<int>(snap.Player.GridPositionY);
        bool walkable = ctx->Terrain.IsWalkable(gx, gy);
        float terrainH = ctx->Terrain.GetTerrainHeight(gx, gy);
        float conv = ctx->Terrain.GetWorldToGridConvertor();
        Show(I18n::TrF("terrain_player_tile", { { "x", Num("%d", gx) },
                                                { "y", Num("%d", gy) },
                                                { "value", YesNo(walkable) } }));
        StatusBadge(walkable ? StatusLevel::Ok : StatusLevel::Warn,
            walkable ? I18n::Tr("terrain_on_walkable") : I18n::Tr("terrain_on_blocked"));
        Show(I18n::TrF("terrain_height_at_player", { { "height", Num("%.2f", terrainH) },
                                                     { "player", Num("%.2f", snap.Player.TerrainHeight) } }));
        ImGui::Text("WorldToGridConvertor: %.4f", conv);   // an SDK getter's value
    }

    if (ImGui::CollapsingHeader(I18n::TrId("terrain_tgt_locations"))) {
        int count = 0;
        struct Ctx { int* count; int firstTen; std::vector<std::string>* names; };
        std::vector<std::string> names;
        Ctx c{&count, 10, &names};
        ctx->Terrain.EnumerateTgtLocations(
            [&c](const PluginSDK::TgtLocation& loc) -> bool {
                ++(*c.count);
                if ((int)c.names->size() < c.firstTen) c.names->push_back(loc.Path);
                return true;
            });
        Show(I18n::TrF("terrain_tgt_total", { { "count", Num("%d", count) } }));
        for (const auto& n : names) {
            ImGui::BulletText("%s", n.c_str());
        }
        if (count == 0) StatusBadge(StatusLevel::Warn, I18n::Tr("terrain_no_tgt"));
        else StatusBadge(StatusLevel::Ok, I18n::Tr("terrain_tgt_working"));
    }

    // The area's generated rooms (.arm), as the host Radar matches its room
    // targets. The copy is refreshed only when GetAreaRoomsRevision() changes
    // (it is 0 while the list is not ready), so an open panel does not copy
    // every room each frame.
    if (ImGui::CollapsingHeader(I18n::TrId("terrain_area_rooms"))) {
        static PluginSDK::AreaRooms rooms;
        if (!rooms.Supported || ctx->Terrain.GetAreaRoomsRevision() != rooms.Revision)
            rooms = ctx->Terrain.GetAreaRooms();
        if (!rooms.Supported) {
            StatusBadge(StatusLevel::Warn, I18n::Tr("terrain_host_predates_rooms"));
        } else if (!rooms.Ready) {
            StatusBadge(StatusLevel::Warn, I18n::Tr("terrain_rooms_not_ready"));
        } else {
            static char pattern[160] = "*";
            ImGui::InputText(I18n::TrId("terrain_room_pattern"), pattern, sizeof(pattern));
            Show(I18n::TrF("terrain_rooms_count", { { "count", Num("%zu", rooms.Rooms.size()) } }));
            ImGui::SameLine(0.0f, ImGui::GetFontSize());
            Show(I18n::TrF("terrain_rooms_revision",
                           { { "revision", Num("%llu", static_cast<unsigned long long>(rooms.Revision)) } }));
            int shown = 0;
            for (const auto& room : rooms.Rooms) {
                if (!room.Matches(pattern)) continue;
                if (++shown > 40) { ImGui::TextDisabled("..."); break; }
                const float dx = room.GridX - snap.Player.GridPositionX;
                const float dy = room.GridY - snap.Player.GridPositionY;
                // room.Name is the room's .arm path (game data), then its place (a text).
                ImGui::BulletText("%s", room.Name.c_str());
                ImGui::SameLine(0.0f, ImGui::GetFontSize());
                Show(I18n::TrF("terrain_room_line",
                    { { "x", Num("%.0f", room.GridX) },
                      { "y", Num("%.0f", room.GridY) },
                      { "distance", Num("%.0f", std::sqrt(dx * dx + dy * dy)) } }));
            }
            StatusBadge(StatusLevel::Ok, I18n::Tr("terrain_rooms_working"));
        }
    }
}

} // namespace Examples
