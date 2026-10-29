#pragma once
// ============================================================================
// ExampleInventory.h — v6 SDK
// ============================================================================
// Demonstrates the v6 Inventory service: scan, enumerate, read item mods.
// Replaces the v5 "WatchInventory/GetWatchedInventoryData" demo with the
// simpler v6 model where InventoryService::GetAll() / Get(id) returns
// full inventory snapshots on demand.
// ============================================================================

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "Text.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <string>

namespace Examples {

inline void DrawInventoryPanel(
    const PluginSDK::Context* ctx,
    std::chrono::steady_clock::time_point& lastScan) {

    if (!ctx) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastScan).count();
    if (elapsed > 2000) {
        ctx->Inventory.Scan(-1);
        lastScan = now;
    }

    std::vector<PluginSDK::Inventory> inventories = ctx->Inventory.GetAll();

    Show(I18n::TrF("inventory_count", { { "count", Num("%d", static_cast<int>(inventories.size())) } }));

    if (inventories.empty()) {
        ImGui::TextDisabled("%s", I18n::Tr("inventory_none"));
        return;
    }

    static int selectedIdx = 0;
    int invCount = static_cast<int>(inventories.size());
    if (selectedIdx >= invCount) selectedIdx = 0;

    // The entries are "[id] <the host's inventory name>" (an identifier, not translated).
    char previewLabel[64];
    {
        const auto& inv = inventories[selectedIdx];
        const char* name = ctx->Inventory.GetName(inv.InventoryId);
        snprintf(previewLabel, sizeof(previewLabel), "[%d] %s",
                 inv.InventoryId, name ? name : "");
    }

    if (ImGui::BeginCombo(I18n::TrId("inventory_inspect"), previewLabel)) {
        for (int ci = 0; ci < invCount; ci++) {
            const auto& inv = inventories[ci];
            const char* name = ctx->Inventory.GetName(inv.InventoryId);
            char itemLabel[64];
            snprintf(itemLabel, sizeof(itemLabel), "[%d] %s",
                     inv.InventoryId, name ? name : "");
            bool isSelected = (ci == selectedIdx);
            if (ImGui::Selectable(itemLabel, isSelected))
                selectedIdx = ci;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const auto& inv = inventories[selectedIdx];

    // One "label | value" row; the label is a translated text or an SDK field name.
    auto labelCell = [](const char* label) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();
    };

    if (ImGui::BeginTable("InvDetail", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        // Width 0: the label column fits its widest translated label (see SetupNameValueColumns).
        ImGui::TableSetupColumn(I18n::TrId("common_name"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
        ImGui::TableSetupColumn(I18n::TrId("common_value"), ImGuiTableColumnFlags_WidthStretch);

        labelCell(I18n::Tr("common_address"));
        char ab[20]; snprintf(ab, sizeof(ab), "0x%llX",
                              static_cast<unsigned long long>(inv.Address));
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", ab);
        if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(ab);

        labelCell(I18n::Tr("inventory_boxes_x")); ImGui::Text("%d", inv.TotalBoxesX);
        labelCell(I18n::Tr("inventory_boxes_y")); ImGui::Text("%d", inv.TotalBoxesY);
        labelCell("ServerRequestCounter");        ImGui::Text("%d", inv.ServerRequestCounter);
        labelCell(I18n::Tr("inventory_items"));   ImGui::Text("%d", static_cast<int>(inv.Items.size()));

        labelCell(I18n::Tr("inventory_grid_valid"));
        ImGui::TextColored(inv.Grid.Valid ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                           "%s", TrueFalse(inv.Grid.Valid));

        if (inv.Grid.Valid) {
            labelCell(I18n::Tr("inventory_grid_screen"));
            ImGui::Text("%.1f, %.1f", inv.Grid.GridScreenX, inv.Grid.GridScreenY);

            labelCell(I18n::Tr("inventory_cell_size")); ImGui::Text("%.2f", inv.Grid.CellSize);
        }
        ImGui::EndTable();
    }

    // --- Visual slot grid -------------------------------------------------
    if (inv.TotalBoxesX > 0 && inv.TotalBoxesY > 0 && ImGui::TreeNode(I18n::TrId("inventory_slots"))) {
        // Defensive bound: TotalBoxesX/Y originate from a live game-memory read and
        // can momentarily be garbage during an area transition (the inventory
        // object is mid-free/relocate). NEVER size an allocation off raw memory
        // dimensions without bounding them first — an unbounded
        // std::vector(TotalBoxesX*TotalBoxesY) on garbage dims throws std::bad_alloc
        // mid-ImGui-render, which the host's SEH guard catches but cannot use to
        // restore the shared ImGui context → the host crashes a frame later.
        // Special stash tabs (currency/gems/sockets/essence) are genuinely wide —
        // up to ~250 a side / ~1.3k slots — so bound generously (matching the host
        // scan) rather than at 64: reject only the absurd mid-transition garbage
        // reads (millions/billions a side) while still rendering real wide tabs.
        const long long exTotalSlots =
            static_cast<long long>(inv.TotalBoxesX) * inv.TotalBoxesY;
        if (inv.TotalBoxesX <= 1024 && inv.TotalBoxesY <= 1024 && exTotalSlots <= 65536) {
            std::vector<uint8_t> occupied(
                static_cast<size_t>(inv.TotalBoxesX) * inv.TotalBoxesY, 0);
            for (const auto& it : inv.Items) {
                for (int yy = 0; yy < it.Height; ++yy)
                    for (int xx = 0; xx < it.Width; ++xx) {
                        int gx = it.SlotX + xx;
                        int gy = it.SlotY + yy;
                        if (gx >= 0 && gx < inv.TotalBoxesX
                            && gy >= 0 && gy < inv.TotalBoxesY) {
                            occupied[static_cast<size_t>(gy) * inv.TotalBoxesX + gx] = 1;
                        }
                    }
            }

            float cellSize = 18.0f;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            for (int y = 0; y < inv.TotalBoxesY; y++) {
                for (int x = 0; x < inv.TotalBoxesX; x++) {
                    int idx = y * inv.TotalBoxesX + x;
                    bool occ = (idx < static_cast<int>(occupied.size()))
                        && occupied[static_cast<size_t>(idx)];
                    ImVec2 p0(origin.x + x * cellSize, origin.y + y * cellSize);
                    ImVec2 p1(p0.x + cellSize - 1, p0.y + cellSize - 1);
                    ImU32 col = occ
                        ? IM_COL32(80, 200, 80, 255)
                        : IM_COL32(60, 60, 60, 255);
                    drawList->AddRectFilled(p0, p1, col);
                    drawList->AddRect(p0, p1, IM_COL32(100, 100, 100, 255));
                }
            }
            ImGui::Dummy(ImVec2(inv.TotalBoxesX * cellSize, inv.TotalBoxesY * cellSize));
        } else {
            ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "%s",
                I18n::TrF("inventory_invalid_grid", { { "width", Num("%d", inv.TotalBoxesX) },
                                                      { "height", Num("%d", inv.TotalBoxesY) } }).c_str());
        }
        ImGui::TreePop();
    }

    // --- Items list with mods --------------------------------------------
    if (!inv.Items.empty() && ImGui::TreeNode(I18n::TrId("inventory_items"))) {
        ImVec4 rarityColors[] = {
            ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
            ImVec4(0.5f, 0.5f, 1.0f, 1.0f),
            ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
            ImVec4(0.8f, 0.5f, 0.2f, 1.0f),
        };
        // Built each frame (not static): the language can change between frames.
        const char* rarityNames[] = { I18n::Tr("rarity_normal"), I18n::Tr("rarity_magic"),
                                      I18n::Tr("rarity_rare"), I18n::Tr("rarity_unique") };

        // treeLabel: I18n::TrId(...). Mod names and stat lines are the game's own text.
        auto renderMods = [ctx](const char* treeLabel, const std::vector<PluginSDK::Mod>& mods) {
            if (mods.empty()) return;
            if (ImGui::TreeNode(treeLabel)) {
                for (const auto& mod : mods) {
                    // Affix-type tag + display name (additive — raw line follows).
                    const char* genTag = (mod.GenerationType == 1) ? I18n::Tr("mods_tag_prefix")
                                       : (mod.GenerationType == 2) ? I18n::Tr("mods_tag_suffix")
                                       : (mod.GenerationType == 3) ? I18n::Tr("mods_tag_implicit") : "";
                    if (genTag[0]) {
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s ", genTag);
                        ImGui::SameLine(0, 0);
                    }
                    if (!mod.AffixName.empty()) {
                        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.9f, 1.0f), "%s ", mod.AffixName.c_str());
                        ImGui::SameLine(0, 0);
                    }
                    bool hasV0 = !std::isnan(mod.Value0);
                    bool hasV1 = !std::isnan(mod.Value1);
                    if (hasV0 && hasV1)
                        ImGui::Text("%s: %.0f - %.0f", mod.Name.c_str(), mod.Value0, mod.Value1);
                    else if (hasV0)
                        ImGui::Text("%s: %.0f", mod.Name.c_str(), mod.Value0);
                    else
                        ImGui::Text("%s", mod.Name.c_str());
                    if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                        ImGui::SetClipboardText(mod.Name.c_str());
                    // In-game-style line via the host .csd formatter.
                    float v0 = std::isnan(mod.Value0) ? 0.0f : mod.Value0;
                    float v1 = std::isnan(mod.Value1) ? 0.0f : mod.Value1;
                    std::string ig = ctx->Inventory.FormatStat(mod.StatKey, v0, v1);
                    if (!ig.empty())
                        ImGui::TextColored(ImVec4(0.55f, 0.65f, 1.0f, 1.0f), "    %s", ig.c_str());
                }
                ImGui::TreePop();
            }
        };

        for (size_t i = 0; i < inv.Items.size(); i++) {
            const auto& item = inv.Items[i];
            int ri = (item.Rarity < 0) ? 0 : (item.Rarity > 3 ? 3 : item.Rarity);

            std::string label = !item.UniqueName.empty()
                ? item.UniqueName
                : (!item.BaseTypeName.empty() ? item.BaseTypeName : item.Path);

            char itemNodeLabel[256];
            snprintf(itemNodeLabel, sizeof(itemNodeLabel), "%s##item%zu", label.c_str(), i);
            ImGui::PushStyleColor(ImGuiCol_Text, rarityColors[ri]);
            bool open = ImGui::TreeNode(itemNodeLabel);
            ImGui::PopStyleColor();

            if (open) {
                if (ImGui::BeginTable("ItemT", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn(I18n::TrId("common_name"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
                    ImGui::TableSetupColumn(I18n::TrId("common_value"), ImGuiTableColumnFlags_WidthStretch);

                    labelCell(I18n::Tr("common_address"));
                    char ab3[20]; snprintf(ab3, sizeof(ab3), "0x%llX",
                                           static_cast<unsigned long long>(item.Address));
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", ab3);

                    labelCell(I18n::Tr("inventory_slot"));
                    ImGui::Text("(%d, %d) %dx%d", item.SlotX, item.SlotY, item.Width, item.Height);

                    labelCell(I18n::Tr("common_rarity"));   ImGui::Text("%s", rarityNames[ri]);
                    labelCell(I18n::Tr("inventory_stack")); ImGui::Text("%d", item.StackCount);
                    labelCell(I18n::Tr("common_path"));     ImGui::TextWrapped("%s", item.Path.c_str());

                    ImGui::EndTable();
                }

                PluginSDK::ItemMods mods = ctx->Inventory.ReadItemMods(item.Address);
                if (mods.Valid) {
                    renderMods(I18n::TrId("mods_implicit_mods"), mods.ImplicitMods);
                    renderMods(I18n::TrId("mods_explicit_mods"), mods.ExplicitMods);
                    renderMods(I18n::TrId("mods_enchant_mods"), mods.EnchantMods);
                    renderMods(I18n::TrId("mods_hellscape_mods"), mods.HellscapeMods);
                    renderMods(I18n::TrId("mods_crucible_mods"), mods.CrucibleMods);
                    int total = static_cast<int>(
                        mods.ImplicitMods.size() + mods.ExplicitMods.size() +
                        mods.EnchantMods.size() + mods.HellscapeMods.size() +
                        mods.CrucibleMods.size());
                    if (total == 0)
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", I18n::Tr("common_no_mods"));
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", I18n::Tr("common_mods_unavailable"));
                }

                // Item base defensive stats. Energy Shield is the in-game
                // (computed) value: round((baseES + flat131) * (1 + %132/100)).
                PluginSDK::ItemBaseStats bs = ctx->Inventory.ReadItemBaseStats(item.Address);
                auto agg = ctx->Inventory.ReadItemAggregatedStats(item.Address);
                int flatES = 0, pctES = 0;
                for (const auto& kv : agg) {
                    if (kv.first == 131) flatES = kv.second;
                    else if (kv.first == 132) pctES = kv.second;
                }
                int es = (bs.EnergyShield > 0)
                    ? static_cast<int>(std::lround((bs.EnergyShield + flatES) * (1.0 + pctES / 100.0)))
                    : 0;
                if (bs.Valid && (es || bs.Ward || bs.Armour || bs.Evasion)
                    && ImGui::TreeNode(I18n::TrId("inventory_base_stats"))) {
                    if (es)         Show(I18n::TrF("inventory_energy_shield", { { "value", Num("%d", es) } }));
                    if (bs.Ward)    Show(I18n::TrF("inventory_ward", { { "value", Num("%d", bs.Ward) } }));
                    if (bs.Armour)  Show(I18n::TrF("inventory_armour", { { "value", Num("%d", bs.Armour) } }));
                    if (bs.Evasion) Show(I18n::TrF("inventory_evasion", { { "value", Num("%d", bs.Evasion) } }));
                    ImGui::TreePop();
                }

                // Aggregated stats — only the known waystone/map header props.
                bool anyKnown = false;
                for (const auto& kv : agg)
                    if (kv.first >= 8206 && kv.first <= 8210) { anyKnown = true; break; }
                if (anyKnown && ImGui::TreeNode(I18n::TrId("inventory_aggregated_stats"))) {
                    for (const auto& kv : agg) {
                        const char* lbl = (kv.first == 8206) ? I18n::Tr("stat_item_rarity")
                                        : (kv.first == 8207) ? I18n::Tr("stat_pack_size")
                                        : (kv.first == 8208) ? I18n::Tr("stat_monster_rarity")
                                        : (kv.first == 8209) ? I18n::Tr("stat_monster_effectiveness")
                                        : (kv.first == 8210) ? I18n::Tr("stat_waystone_drop_chance") : nullptr;
                        if (lbl)
                            Show(I18n::TrF("inventory_stat_line", { { "stat", lbl },
                                                                    { "value", Num("%+d", kv.second) } }));
                    }
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

} // namespace Examples
