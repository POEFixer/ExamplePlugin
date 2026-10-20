// Plugins/ExamplePlugin/examples/ExampleTerrain.h
// Demonstrates every TerrainService method.

#pragma once
#include <cmath>
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "SdkStatus.h"

namespace Examples {

inline void DrawTerrainPanel(const PluginSDK::Context* ctx,
                              const PluginSDK::Snapshot& snap)
{
    if (!ctx) { StatusBadge(StatusLevel::Fail, "Context missing"); return; }

    if (ImGui::CollapsingHeader("Walkable Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        PluginSDK::WalkableGridHandle h = ctx->Terrain.GetWalkableGrid();
        if (h.Valid()) {
            ImGui::Text("Width:     %d", h.Width());
            ImGui::Text("Height:    %d", h.Height());
            ImGui::Text("SizeBytes: %zu", h.SizeBytes());
            ImGui::Text("Data ptr:  0x%p", static_cast<const void*>(h.Data()));
            StatusBadge(StatusLevel::Ok, "Grid loaded");
        } else {
            StatusBadge(StatusLevel::Warn, "Walkable grid not loaded (not in game?)");
        }
    }

    if (ImGui::CollapsingHeader("Height Grid")) {
        PluginSDK::HeightGridHandle h = ctx->Terrain.GetHeightGrid();
        if (h.Valid()) {
            ImGui::Text("Width:     %d", h.Width());
            ImGui::Text("Height:    %d", h.Height());
            ImGui::Text("Element count: %zu", h.ElementCount());
            ImGui::Text("Data ptr:  0x%p", static_cast<const void*>(h.Data()));
            StatusBadge(StatusLevel::Ok, "Height grid loaded");
        } else {
            StatusBadge(StatusLevel::Warn, "Height grid not loaded");
        }
    }

    if (ImGui::CollapsingHeader("Per-tile queries")) {
        int gx = static_cast<int>(snap.Player.GridPositionX);
        int gy = static_cast<int>(snap.Player.GridPositionY);
        bool walkable = ctx->Terrain.IsWalkable(gx, gy);
        float terrainH = ctx->Terrain.GetTerrainHeight(gx, gy);
        float conv = ctx->Terrain.GetWorldToGridConvertor();
        ImGui::Text("Player tile (%d, %d) walkable: %s",
            gx, gy, walkable ? "yes" : "no");
        StatusBadge(walkable ? StatusLevel::Ok : StatusLevel::Warn,
            walkable ? "Player on walkable tile" : "Player on blocked tile (unusual)");
        ImGui::Text("Terrain height at player tile: %.2f (player.TerrainHeight: %.2f)",
            terrainH, snap.Player.TerrainHeight);
        ImGui::Text("WorldToGridConvertor: %.4f", conv);
    }

    if (ImGui::CollapsingHeader("Tgt Locations")) {
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
        ImGui::Text("Total TgtLocations: %d", count);
        for (const auto& n : names) {
            ImGui::BulletText("%s", n.c_str());
        }
        if (count == 0) StatusBadge(StatusLevel::Warn, "No tgt tiles (likely not in dungeon area)");
        else StatusBadge(StatusLevel::Ok, "Tgt enumeration working");
    }

    // The area's generated rooms (.arm), as the host Radar matches its room
    // targets. The copy is refreshed only when GetAreaRoomsRevision() changes
    // (it is 0 while the list is not ready), so an open panel does not copy
    // every room each frame.
    if (ImGui::CollapsingHeader("Area Rooms")) {
        static PluginSDK::AreaRooms rooms;
        if (!rooms.Supported || ctx->Terrain.GetAreaRoomsRevision() != rooms.Revision)
            rooms = ctx->Terrain.GetAreaRooms();
        if (!rooms.Supported) {
            StatusBadge(StatusLevel::Warn, "Host predates GetAreaRooms");
        } else if (!rooms.Ready) {
            StatusBadge(StatusLevel::Warn, "Room list not ready (loading or not in game)");
        } else {
            static char pattern[160] = "*";
            ImGui::InputText("Pattern (* only, case ignored)", pattern, sizeof(pattern));
            ImGui::Text("Rooms: %zu  revision: %llu", rooms.Rooms.size(),
                static_cast<unsigned long long>(rooms.Revision));
            int shown = 0;
            for (const auto& room : rooms.Rooms) {
                if (!room.Matches(pattern)) continue;
                if (++shown > 40) { ImGui::TextDisabled("..."); break; }
                const float dx = room.GridX - snap.Player.GridPositionX;
                const float dy = room.GridY - snap.Player.GridPositionY;
                ImGui::BulletText("%s  centre (%.0f, %.0f), %.0f cells away", room.Name.c_str(),
                    room.GridX, room.GridY, std::sqrt(dx * dx + dy * dy));
            }
            StatusBadge(StatusLevel::Ok, "Room list working");
        }
    }
}

} // namespace Examples
