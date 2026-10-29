#pragma once
// ============================================================================
// ExampleBuffs.h — v6 SDK
// ============================================================================
// Demonstrates reading the player's Buffs component and rendering a list with
// progress bars and filtering. Uses ctx->Components.EnumerateAggregatedBuffs()
// so multi-instance charge stacks (e.g. Chayula breach charges, stored as N
// separate StatusEffect instances) show their summed count in the Charges
// column — matching the in-game charge-stack icon. Swap in EnumerateBuffs()
// for the raw per-instance list instead.
// ============================================================================

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "Text.h"

namespace Examples {

inline void DrawBuffsPanel(const PluginSDK::Context* ctx,
                           const PluginSDK::Snapshot& snapshot) {
    if (!ctx) return;

    if (!snapshot.Player.Components.HasBuffs()) {
        ImGui::TextDisabled("%s", I18n::Tr("buffs_no_component"));
        return;
    }

    std::vector<PluginSDK::Buff> buffs =
        ctx->Components.EnumerateAggregatedBuffs(snapshot.Player.Components.Buffs);

    Show(I18n::TrF("buffs_active", { { "count", Num("%d", static_cast<int>(buffs.size())) } }));

    if (buffs.empty()) {
        ImGui::TextDisabled("%s", I18n::Tr("buffs_none"));
        return;
    }

    static char buffFilter[64] = "";
    ImGui::InputText(I18n::TrId("buffs_filter"), buffFilter, sizeof(buffFilter));

    if (ImGui::BeginTable("BuffTable", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Sortable,
        ImVec2(0, 250))) {
        // No pixel widths: a width sized for the English header clips a translated one. A fixed
        // column with width 0 in a table without ImGuiTableFlags_Resizable fits its header and its
        // cells every frame, so it follows a language switch too. TrId keeps each column's id.
        ImGui::TableSetupColumn(I18n::TrId("common_name"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(I18n::TrId("buffs_time_left"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
        ImGui::TableSetupColumn(I18n::TrId("buffs_total_time"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
        ImGui::TableSetupColumn(I18n::TrId("common_charges"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
        ImGui::TableSetupColumn(I18n::TrId("buffs_source_entity"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
        ImGui::TableSetupColumn(I18n::TrId("buffs_progress"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
        ImGui::TableHeadersRow();

        for (const auto& b : buffs) {
            if (buffFilter[0] != '\0') {
                if (b.Name.find(buffFilter) == std::string::npos) continue;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (b.TotalTime <= 0) {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", b.Name.c_str());
            } else {
                ImGui::TextUnformatted(b.Name.c_str());
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", b.Name.c_str());
                if (ImGui::IsItemClicked()) ImGui::SetClipboardText(b.Name.c_str());
            }

            ImGui::TableNextColumn();
            if (b.TimeLeft > 0)
                Show(I18n::TrF("common_seconds", { { "value", Num("%.1f", b.TimeLeft) } }));
            else
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", I18n::Tr("buffs_permanent"));

            ImGui::TableNextColumn();
            if (b.TotalTime > 0)
                Show(I18n::TrF("common_seconds", { { "value", Num("%.1f", b.TotalTime) } }));
            else
                ImGui::TextDisabled("--");

            ImGui::TableNextColumn();
            if (b.Charges > 0)
                ImGui::Text("%d", b.Charges);
            else
                ImGui::TextDisabled("0");

            ImGui::TableNextColumn();
            if (b.SourceEntityId != 0)
                ImGui::Text("%u", b.SourceEntityId);
            else
                ImGui::TextDisabled("--");

            ImGui::TableNextColumn();
            if (b.TotalTime > 0 && b.TimeLeft > 0) {
                float frac = b.TimeLeft / b.TotalTime;
                ImVec4 barColor = (frac > 0.5f) ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f)
                    : (frac > 0.2f) ? ImVec4(0.8f, 0.8f, 0.3f, 1.0f)
                    : ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
                // A bar of its own width (6 em): the column fits it or its header, whichever is wider.
                ImGui::ProgressBar(frac, ImVec2(ImGui::GetFontSize() * 6.0f, ImGui::GetTextLineHeight()), "");
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("--");
            }
        }
        ImGui::EndTable();
    }
}

} // namespace Examples
