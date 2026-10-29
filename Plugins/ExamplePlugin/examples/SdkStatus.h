// Plugins/ExamplePlugin/examples/SdkStatus.h
// Shared helpers for SDK-coverage status badges + Coverage Summary banner.

#pragma once
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "Text.h"
#include <string>

namespace Examples {

enum class StatusLevel { Ok, Warn, Fail };

inline void StatusBadge(StatusLevel level, const char* label) {
    ImVec4 color{0.7f, 0.7f, 0.7f, 1};
    const char* glyph = "?";
    switch (level) {
        case StatusLevel::Ok:   color = {0.2f, 0.9f, 0.2f, 1}; glyph = "\xE2\x9C\x93"; break;  // ✓
        case StatusLevel::Warn: color = {0.9f, 0.7f, 0.2f, 1}; glyph = "\xE2\x9A\xA0"; break;  // ⚠
        case StatusLevel::Fail: color = {0.9f, 0.2f, 0.2f, 1}; glyph = "\xE2\x9C\x97"; break;  // ✗
    }
    ImGui::TextColored(color, "%s %s", glyph, label);
}

// Name: the service's SDK name (ctx->Game, ctx->Entities, ...), never translated.
// Detail: a translated text, built per frame.
struct ServiceStatus {
    const char* Name;
    StatusLevel Level;
    std::string Detail;
};

// Computed per-frame; passed to the Coverage Summary banner above the tab bar.
inline std::vector<ServiceStatus> ComputeCoverage(
        const PluginSDK::Context* ctx,
        const PluginSDK::Snapshot& snap)
{
    std::vector<ServiceStatus> out;
    auto add = [&](const char* name, StatusLevel l, std::string d = {}) {
        out.push_back({name, l, std::move(d)});
    };

    const bool inGame = snap.State == PluginSDK::GameState::InGame;

    // Game
    if (snap.IsAttached && snap.GameWindow != nullptr && snap.ScreenWidth > 0)
        add("Game", StatusLevel::Ok);
    else
        add("Game", StatusLevel::Warn, I18n::Tr("coverage_not_attached"));

    // Entities
    add("Entities",
        (inGame && snap.Player.IsValid)
            ? StatusLevel::Ok
            : StatusLevel::Warn,
        inGame ? std::string{} : I18n::Tr("coverage_enter_game"));

    // Components
    add("Components",
        (inGame && snap.Player.Components.HasLife())
            ? StatusLevel::Ok
            : StatusLevel::Warn,
        inGame ? std::string{} : I18n::Tr("coverage_enter_game"));

    // Inventory
    if (!ctx) { add("Inventory", StatusLevel::Fail, I18n::Tr("coverage_abi_missing")); }
    else add("Inventory", inGame ? StatusLevel::Ok : StatusLevel::Warn);

    // Ui
    add("Ui",
        (ctx && ctx->Ui.GetGameUiRoot() != 0)
            ? StatusLevel::Ok
            : StatusLevel::Warn,
        I18n::Tr("coverage_ui_needs_game"));

    // Render
    add("Render",
        (snap.LargeMap.IsVisible || snap.MiniMap.IsVisible)
            ? StatusLevel::Ok
            : StatusLevel::Warn,
        I18n::Tr("coverage_open_map"));

    // Terrain
    {
        auto wg = ctx ? ctx->Terrain.GetWalkableGrid() : PluginSDK::WalkableGridHandle{};
        add("Terrain", wg.Valid() ? StatusLevel::Ok : StatusLevel::Warn,
            wg.Valid() ? std::string{} : I18n::Tr("coverage_no_walkable"));
    }

    // Memory
    add("Memory",
        (ctx && ctx->Memory.GetBaseAddress() != 0)
            ? StatusLevel::Ok
            : StatusLevel::Fail);

    // Log
    add("Log", StatusLevel::Ok);  // Always available; use button in Log tab to test

    // Events
    add("Events", StatusLevel::Ok);  // Counter values shown in Events tab

    // Prices (host-loaded poe2scout market data; shared by all plugins)
    if (!ctx) {
        add("Prices", StatusLevel::Fail, I18n::Tr("coverage_abi_missing"));
    } else {
        PluginSDK::PriceStatus ps = ctx->Prices.GetStatus();
        if (ps.loaded)
            add("Prices", StatusLevel::Ok,
                I18n::TrF("coverage_price_items", { { "count", Num("%d", ps.totalItems) } }));
        else if (ps.catsFailed > 0 && ps.catsPending == 0)
            add("Prices", StatusLevel::Fail, I18n::Tr("coverage_prices_failed"));
        else
            add("Prices", StatusLevel::Warn, I18n::Tr("coverage_prices_loading"));
    }

    return out;
}

inline void DrawCoverageSummary(const std::vector<ServiceStatus>& services) {
    int okCount = 0;
    for (const auto& s : services) if (s.Level == StatusLevel::Ok) ++okCount;
    const int total = static_cast<int>(services.size());

    ImVec4 color = (okCount == total) ? ImVec4(0.2f, 0.9f, 0.2f, 1)
                  : (okCount >= total - 2) ? ImVec4(0.9f, 0.7f, 0.2f, 1)
                  : ImVec4(0.9f, 0.4f, 0.2f, 1);

    ImGui::TextColored(color, "%s", I18n::TrF("coverage_summary", { { "ok", Num("%d", okCount) },
                                                                    { "total", Num("%d", total) } }).c_str());

    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        for (const auto& s : services) {
            StatusBadge(s.Level, s.Name);
            if (!s.Detail.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", s.Detail.c_str());
            }
        }
        ImGui::EndTooltip();
    }
}

} // namespace Examples
