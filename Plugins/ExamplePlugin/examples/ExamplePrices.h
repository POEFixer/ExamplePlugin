#pragma once
// ============================================================================
// ExamplePrices.h — v6 SDK
// ============================================================================
// Demonstrates the v6 Prices service: ctx->Prices exposes market prices the
// host loads ONCE per session from poe2scout on a background thread. Every
// plugin (and the built-in radar/overlays) shares this single price database,
// so plugins never fetch prices themselves — they just look them up.
//
//   - GetStatus()  : gate UI on .loaded before trusting prices; per-category
//                    counts show how far the background loader got.
//   - GetRates()   : Divine / Exalted -> Chaos conversion rates.
//   - LookupPrice(): fuzzy host-side match by display name; returns chaos +
//                    divine + exalt + the poe2scout category that matched.
//
// All prices are Chaos-denominated. The panel below shows the load status, an
// interactive name lookup, a live common-currency table, and the real-world
// workflow: scan an inventory and value every item in chaos.
// ============================================================================

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "Text.h"
#include <vector>
#include <string>
#include <utility>
#include <cstdio>

namespace Examples {

// Render one PriceResult inline: chaos | divine | exalt [category]. The currency
// abbreviations (c, div, ex) and the category are kept as the price source spells them.
inline void DrawPriceResultRow(const PluginSDK::PriceResult& p) {
    if (!p.found) {
        ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.60f, 1), "%s", I18n::Tr("prices_not_found"));
        return;
    }
    ImGui::Text("%.1fc", p.chaos);
    ImGui::SameLine(); ImGui::TextDisabled("|");
    ImGui::SameLine(); ImGui::Text("%.2f div", p.divine);
    ImGui::SameLine(); ImGui::TextDisabled("|");
    ImGui::SameLine(); ImGui::Text("%.1f ex", p.exalt);
    if (!p.category.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.50f, 0.80f, 1.00f, 1), "[%s]", p.category.c_str());
    }
}

inline void DrawPricesPanel(const PluginSDK::Context* ctx) {
    if (!ctx) return;

    const ImVec4 cHead(0.40f, 0.80f, 1.00f, 1.0f);
    const ImVec4 cDim (0.55f, 0.55f, 0.55f, 1.0f);
    const ImVec4 cGood(0.30f, 0.90f, 0.30f, 1.0f);
    const ImVec4 cWarn(0.95f, 0.75f, 0.25f, 1.0f);

    // ---- Load status -----------------------------------------------------
    // Always gate on GetStatus().loaded before trusting prices: the host loads
    // them on a background thread, so they may not be ready on the first frame.
    PluginSDK::PriceStatus st = ctx->Prices.GetStatus();
    ImGui::TextColored(cHead, "%s", I18n::Tr("prices_status"));
    if (st.loaded) {
        ImGui::TextColored(cGood, "%s", I18n::Tr("prices_loaded"));
        ImGui::SameLine();
        Show(I18n::TrF("prices_item_count", { { "count", Num("%d", st.totalItems) } }));
    } else {
        ImGui::TextColored(cWarn, "%s", I18n::Tr("prices_loading"));
    }
    Show(I18n::TrF("prices_categories", { { "ok", Num("%d", st.catsOk) },
                                          { "pending", Num("%d", st.catsPending) },
                                          { "failed", Num("%d", st.catsFailed) } }));

    // ---- Conversion rates ------------------------------------------------
    PluginSDK::PriceRates rates = ctx->Prices.GetRates();
    // Two rates on one line: the separator and its gaps are code (SameLine), not part of a text.
    Show(I18n::TrF("prices_rate_divine", { { "divine", Num("%.0f", rates.divineInChaos) } }));
    ImGui::SameLine(0.0f, ImGui::GetFontSize());
    ImGui::TextDisabled("|");
    ImGui::SameLine(0.0f, ImGui::GetFontSize());
    Show(I18n::TrF("prices_rate_exalted", { { "exalted", Num("%.1f", rates.exaltedInChaos) } }));

    ImGui::Separator();

    // ---- Interactive lookup ----------------------------------------------
    // The query is a name as the price source knows it (English), so its default
    // and the presets below stay English.
    ImGui::TextColored(cHead, "%s", I18n::Tr("prices_lookup_title"));
    static char query[128] = "Divine Orb";
    bool submitted = ImGui::InputText("##exPriceQuery", query, sizeof(query),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    submitted |= ImGui::Button(I18n::TrId("prices_lookup"));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", I18n::Tr("prices_lookup_hint"));

    static PluginSDK::PriceResult lastResult;
    static std::string            lastQuery;
    if (submitted && query[0]) {
        lastResult = ctx->Prices.LookupPrice(query);
        lastQuery  = query;
    }
    if (!lastQuery.empty()) {
        ImGui::Text("\"%s\":", lastQuery.c_str());
        ImGui::SameLine();
        DrawPriceResultRow(lastResult);
    }

    ImGui::Separator();

    // ---- Common currency (looked up live each frame; cheap) --------------
    ImGui::TextColored(cHead, "%s", I18n::Tr("prices_common_currency"));
    static const char* kPresets[] = {
        "Divine Orb", "Exalted Orb", "Chaos Orb",
        "Orb of Annulment", "Mirror of Kalandra", "Vaal Orb",
    };
    if (ImGui::BeginTable("##exPrices", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn(I18n::TrId("prices_col_item"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(I18n::TrId("prices_col_price"));
        ImGui::TableHeadersRow();
        for (const char* name : kPresets) {
            PluginSDK::PriceResult p = ctx->Prices.LookupPrice(name);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1); DrawPriceResultRow(p);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    // ---- Inventory valuation (on demand) ---------------------------------
    // The real-world workflow (this is what NinjaPricer does): scan an
    // inventory, price each item by its unique-or-base name, sum in chaos.
    // Triggered by a button so the per-item LookupPrice loop runs once, not
    // every frame.
    ImGui::TextColored(cHead, "%s", I18n::Tr("prices_value_backpack"));
    static std::vector<std::pair<std::string, float>> invPriced;  // name -> chaos
    static float invTotalChaos = 0.f;
    static bool  invScanned    = false;
    if (ImGui::Button(I18n::TrId("prices_scan_inventory"))) {
        ctx->Inventory.Scan(0);          // 0 = backpack
        invPriced.clear();
        invTotalChaos = 0.f;
        for (const auto& item : ctx->Inventory.GetItems(0)) {
            std::string name = ctx->Inventory.ReadItemUniqueName(item.Address);
            if (name.empty()) name = ctx->Inventory.ReadItemBaseTypeName(item.Address);
            if (name.empty()) continue;
            PluginSDK::PriceResult p = ctx->Prices.LookupPrice(name);
            if (p.found) {
                float stackChaos = p.chaos * (item.StackCount > 0 ? item.StackCount : 1);
                invPriced.emplace_back(name, stackChaos);
                invTotalChaos += stackChaos;
            }
        }
        invScanned = true;
    }
    if (invScanned) {
        ImGui::SameLine();
        float div = rates.divineInChaos > 0 ? invTotalChaos / rates.divineInChaos : 0.f;
        Show(I18n::TrF("prices_total", { { "chaos", Num("%.1f", invTotalChaos) },
                                         { "divine", Num("%.2f", div) } }));
        if (invPriced.empty()) {
            ImGui::TextColored(cDim, "%s", I18n::Tr("prices_no_priced_items"));
        } else if (ImGui::BeginTable("##exInvPrices", 2,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                ImVec2(0, 160))) {
            ImGui::TableSetupColumn(I18n::TrId("prices_col_item"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(I18n::TrId("prices_col_chaos"));
            ImGui::TableHeadersRow();
            for (const auto& row : invPriced) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(row.first.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", row.second);
            }
            ImGui::EndTable();
        }
    }
}

} // namespace Examples
