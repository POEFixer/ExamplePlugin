#pragma once
// ============================================================================
// ExampleUiExplorer.h — v6 SDK
// ============================================================================
// Navigates the game's UI element tree using ctx->Ui.* services. No raw
// offsets — the v6 UiService::Read returns a populated UiElement struct
// already containing parent, child count, position, size, scale, flags,
// visibility, string-id address, etc.
// ============================================================================

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "Text.h"
#include <deque>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace Examples {

class PluginUiExplorer {
public:
    void Draw(const PluginSDK::Context* ctx) {
        if (!ctx) {
            ImGui::TextDisabled("%s", I18n::Tr("uiexp_no_context"));
            return;
        }
        m_Ctx = ctx;
        m_Highlights.clear();

        if (m_CurrentAddress == 0) {
            uintptr_t gameUi = ctx->Ui.GetGameUiRoot();
            if (gameUi != 0) {
                m_CurrentAddress = gameUi;
                m_History.clear();
                RefreshElement();
            }
        }

        if (m_CurrentAddress == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", I18n::Tr("uiexp_no_game_ui"));
            return;
        }

        RefreshElement();

        DrawBreadcrumbs();
        ImGui::Spacing();
        DrawSearchPanel();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        std::string currentSid = ctx->Ui.GetStringId(m_CurrentAddress);

        ImGui::BeginGroup();
        // {element}: the element's StringId (game data) in quotes, else its address.
        const std::string currentShown = !currentSid.empty()
            ? "\"" + currentSid + "\""
            : Num("0x%llX", static_cast<unsigned long long>(m_CurrentAddress));
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s",
            I18n::TrF("uiexp_current", { { "element", currentShown } }).c_str());
        ImGui::EndGroup();
        if (ImGui::IsItemHovered()) {
            DrawHighlighter(m_CurrentAddress, IM_COL32(255, 255, 0, 255),
                currentSid.empty() ? std::string(I18n::Tr("uiexp_current_label")) : currentSid);
            ImGui::SetTooltip("%s", I18n::Tr("uiexp_click_to_copy"));
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                       static_cast<unsigned long long>(m_CurrentAddress));
                ImGui::SetClipboardText(buf);
            }
        }
        ImGui::Spacing();

        // Navigation buttons
        PluginSDK::UiElement currentElem = ctx->Ui.Read(m_CurrentAddress);
        bool hasParent = (currentElem.ParentAddress != 0);
        bool hasSelectedChild = (m_CurrentChildIndex >= 0 &&
            m_CurrentChildIndex < static_cast<int>(m_ChildAddresses.size()));

        if (!hasParent) ImGui::BeginDisabled();
        if (ImGui::Button(I18n::TrId("uiexp_parent"))) NavigateTo(currentElem.ParentAddress);
        if (!hasParent) ImGui::EndDisabled();

        ImGui::SameLine();
        if (!hasSelectedChild) ImGui::BeginDisabled();
        if (ImGui::Button(I18n::TrId("uiexp_child"))) NavigateTo(m_ChildAddresses[m_CurrentChildIndex]);
        if (!hasSelectedChild) ImGui::EndDisabled();

        ImGui::SameLine();
        if (m_History.empty()) ImGui::BeginDisabled();
        if (ImGui::Button(I18n::TrId("uiexp_back"))) {
            if (!m_History.empty()) {
                uintptr_t prevAddr = m_History.back().Address;
                m_History.pop_back();
                m_CurrentAddress = prevAddr;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }
        if (m_History.empty()) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(I18n::TrId("uiexp_root"))) {
            uintptr_t gameUi = ctx->Ui.GetGameUiRoot();
            if (gameUi != 0) {
                m_History.clear();
                m_CurrentAddress = gameUi;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("UiRoot")) {   // the SDK's GetUiRoot (an SDK name)
            uintptr_t ui = ctx->Ui.GetUiRoot();
            if (ui != 0) {
                m_History.clear();
                m_CurrentAddress = ui;
                m_CurrentChildIndex = -1;
                RefreshElement();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(I18n::TrId("uiexp_copy_info")))
            CopyElementInfo(currentSid);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader(I18n::TrId("uiexp_children"), ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawChildrenList();
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader(I18n::TrId("uiexp_properties"), ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawElementPropertiesFor(m_CurrentAddress);
        }

        if (hasSelectedChild) {
            ImGui::Spacing();
            if (ImGui::CollapsingHeader(I18n::TrId("uiexp_selected_child"))) {
                ImGui::Indent();
                DrawElementPropertiesFor(m_ChildAddresses[m_CurrentChildIndex]);
                ImGui::Unindent();
            }
        }
    }

    // Draws the highlights Draw() recorded this frame over the game, then forgets
    // them. Call it AFTER the window that holds the explorer has ended: that window
    // is an informational window (PluginInfoWindow.h), which the host may draw out
    // of the game in another ImGui context, where a foreground draw list would show
    // nothing. After the window the plugin is back in the game overlay's context.
    void DrawHighlights() {
        if (m_Ctx) {
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            for (const Highlight& hl : m_Highlights) {
                float x, y, w, h;
                if (!m_Ctx->Ui.ComputeScreenRect(hl.Address, x, y, w, h)) continue;
                if (w <= 0 || h <= 0) continue;
                const ImVec2 pos(x, y);
                const ImVec2 maxPos(x + w, y + h);
                dl->AddRect(pos, maxPos, hl.Color, 0.0f, 2.0f, ImDrawFlags_StrokeCenter);
                const ImU32 fillColor = (hl.Color & 0x00FFFFFF) | 0x18000000;
                dl->AddRectFilled(pos, maxPos, fillColor);
                if (!hl.Label.empty()) {
                    const ImVec2 textSize = ImGui::CalcTextSize(hl.Label.c_str());
                    const ImVec2 textBgMax(pos.x + textSize.x + 4, pos.y + textSize.y + 2);
                    dl->AddRectFilled(pos, textBgMax, IM_COL32(0, 0, 0, 180));
                    dl->AddText(ImVec2(pos.x + 2, pos.y + 1), hl.Color, hl.Label.c_str());
                }
            }
        }
        m_Highlights.clear();
    }

private:
    // Sanity-check an address before treating it as a UI element. The bridge
    // already filters obviously bad pointers (kernel range, < 0x10000), but
    // we also reject addresses that don't produce a valid UiElement to avoid
    // navigating into corrupted memory after a parent-pointer scribble.
    bool IsNavigable(uintptr_t addr) const {
        if (addr == 0 || addr < 0x10000ull) return false;
        if (addr > 0x00007FFFFFFFFFFFull) return false;  // kernel space
        if (!m_Ctx) return false;
        PluginSDK::UiElement probe = m_Ctx->Ui.Read(addr);
        return probe.Valid;
    }

    void NavigateTo(uintptr_t addr) {
        if (!IsNavigable(addr)) return;
        if (m_CurrentAddress != 0) {
            BreadcrumbEntry entry;
            entry.Address = m_CurrentAddress;
            entry.Label = m_Ctx->Ui.GetStringId(m_CurrentAddress);
            if (entry.Label.empty()) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                       static_cast<unsigned long long>(m_CurrentAddress));
                entry.Label = buf;
            }
            if (m_History.empty() || m_History.back().Address != entry.Address) {
                m_History.push_back(entry);
                if (m_History.size() > 32) m_History.pop_front();
            }
        }
        m_CurrentAddress = addr;
        m_CurrentChildIndex = -1;
        RefreshElement();
    }

    void RefreshElement() {
        m_ChildAddresses.clear();
        if (m_CurrentAddress == 0 || !m_Ctx) return;
        m_ChildAddresses = m_Ctx->Ui.GetChildren(m_CurrentAddress);
    }

    // Records a highlight of a game UI element; DrawHighlights() draws it over the
    // game once the window has ended (see there).
    void DrawHighlighter(uintptr_t addr, ImU32 color, const std::string& label = "") {
        m_Highlights.push_back({ addr, color, label });
    }

    void DrawBreadcrumbs() {
        if (m_History.empty()) return;

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", I18n::Tr("uiexp_path"));
        ImGui::SameLine();

        int startIdx = 0;
        if (m_History.size() > 6) {
            startIdx = static_cast<int>(m_History.size()) - 6;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "...");
            ImGui::SameLine();
        }

        for (int i = startIdx; i < static_cast<int>(m_History.size()); i++) {
            auto& entry = m_History[i];
            std::string displayLabel = entry.Label;
            displayLabel = Shorten(displayLabel, 20);   // a localized game string: cut by characters

            ImGui::PushID(i);
            if (ImGui::SmallButton(displayLabel.c_str())) {
                uintptr_t targetAddr = entry.Address;
                while (m_History.size() > static_cast<size_t>(i))
                    m_History.pop_back();
                m_CurrentAddress = targetAddr;
                m_CurrentChildIndex = -1;
                RefreshElement();
                ImGui::PopID();
                return;
            }
            ImGui::PopID();

            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), ">");
            ImGui::SameLine();
        }

        std::string currentLabel = m_Ctx->Ui.GetStringId(m_CurrentAddress);
        if (currentLabel.empty()) {
            char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                   static_cast<unsigned long long>(m_CurrentAddress));
            currentLabel = buf;
        }
        currentLabel = Shorten(currentLabel, 20);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", currentLabel.c_str());
    }

    void DrawSearchPanel() {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15.0f);   // in em: follows the font, not pixels
        bool searchTriggered = ImGui::InputTextWithHint("##UiSearch", I18n::Tr("uiexp_search_hint"),
            m_SearchBuffer, sizeof(m_SearchBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        if (ImGui::Button(I18n::TrId("uiexp_search")) || searchTriggered) {
            if (std::strlen(m_SearchBuffer) > 0) {
                m_SearchResults.clear();
                std::string query(m_SearchBuffer);
                std::transform(query.begin(), query.end(), query.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                uintptr_t searchRoot = m_Ctx->Ui.GetGameUiRoot();
                if (searchRoot != 0) {
                    SearchTree(searchRoot, query, 0, 15);
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(I18n::TrId("uiexp_clear"))) {
            m_SearchBuffer[0] = '\0';
            m_SearchResults.clear();
        }

        if (!m_SearchResults.empty()) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s",
                I18n::TrF("uiexp_found", { { "count", Num("%d", static_cast<int>(m_SearchResults.size())) } }).c_str());

            float listHeight = (std::min)(120.0f,
                static_cast<float>(m_SearchResults.size()) * 20.0f + 4.0f);
            if (ImGui::BeginListBox("##SearchResults", ImVec2(-1, listHeight))) {
                for (auto& [addr, name] : m_SearchResults) {
                    char label[256];
                    snprintf(label, sizeof(label), "0x%llX - %s",
                             static_cast<unsigned long long>(addr), name.c_str());
                    if (ImGui::Selectable(label)) NavigateTo(addr);
                    if (ImGui::IsItemHovered())
                        DrawHighlighter(addr, IM_COL32(0, 255, 255, 255), name);
                }
                ImGui::EndListBox();
            }
        }
    }

    void SearchTree(uintptr_t addr, const std::string& query, int depth, int maxDepth) {
        if (depth > maxDepth || addr == 0 || m_SearchResults.size() >= 100) return;
        // Filter clearly-bogus addresses (kernel range, low pointers) before
        // trusting the bridge to RPM through them.
        if (addr < 0x10000ull || addr > 0x00007FFFFFFFFFFFull) return;

        std::string stringId = m_Ctx->Ui.GetStringId(addr);
        if (!stringId.empty()) {
            std::string lower = stringId;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower.find(query) != std::string::npos) {
                m_SearchResults.emplace_back(addr, stringId);
            }
        }

        auto kids = m_Ctx->Ui.GetChildren(addr);
        for (uintptr_t childAddr : kids) {
            if (m_SearchResults.size() >= 100) break;
            SearchTree(childAddr, query, depth + 1, maxDepth);
        }
    }

    void DrawChildrenList() {
        int childCount = static_cast<int>(m_ChildAddresses.size());
        Show(I18n::TrF("uiexp_children_count", { { "count", Num("%d", childCount) } }));
        if (childCount == 0) return;

        ImGui::SameLine(0.0f, ImGui::GetFontSize());   // after the (translated) count, whatever its width

        if (ImGui::SmallButton("|<")) m_CurrentChildIndex = 0;
        ImGui::SameLine();
        if (ImGui::SmallButton("<")) {
            if (m_CurrentChildIndex > 0) m_CurrentChildIndex--;
            else m_CurrentChildIndex = childCount - 1;
        }
        ImGui::SameLine();
        ImGui::Text("%d/%d", m_CurrentChildIndex + 1, childCount);
        ImGui::SameLine();
        if (ImGui::SmallButton(">")) {
            if (m_CurrentChildIndex < childCount - 1) m_CurrentChildIndex++;
            else m_CurrentChildIndex = 0;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(">|")) m_CurrentChildIndex = childCount - 1;

        std::string preview = (m_CurrentChildIndex >= 0)
            ? std::to_string(m_CurrentChildIndex) : std::string(I18n::Tr("uiexp_no_child"));

        if (ImGui::BeginCombo("##Children", preview.c_str())) {
            for (int i = 0; i < childCount; ++i) {
                uintptr_t childAddr = m_ChildAddresses[i];
                bool isSelected = (m_CurrentChildIndex == i);
                bool isVis = m_Ctx->Ui.IsVisible(childAddr);
                PluginSDK::UiElement elem = m_Ctx->Ui.Read(childAddr);

                std::stringstream ss;
                ss << i;
                std::string childSid = m_Ctx->Ui.GetStringId(childAddr);
                if (!childSid.empty()) {
                    childSid = Shorten(childSid, 30);
                    ss << " \"" << childSid << "\"";
                }
                if (elem.ChildCount > 0) ss << " [" << elem.ChildCount << "]";

                float gx, gy, gw, gh;
                if (m_Ctx->Ui.ComputeScreenRect(childAddr, gx, gy, gw, gh) && gw > 0 && gh > 0) {
                    ss << " (" << static_cast<int>(gw) << "x" << static_cast<int>(gh) << ")";
                }

                ImVec4 textColor = isVis
                    ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f)
                    : ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

                ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                if (ImGui::Selectable(ss.str().c_str(), isSelected))
                    m_CurrentChildIndex = i;
                ImGui::PopStyleColor();

                if (isSelected) ImGui::SetItemDefaultFocus();

                if (ImGui::IsItemHovered()) {
                    std::string hoverLabel = std::to_string(i);
                    if (!childSid.empty()) hoverLabel += " " + childSid;
                    DrawHighlighter(childAddr, IM_COL32(255, 255, 0, 255), hoverLabel);
                }
            }
            ImGui::EndCombo();
        }

        if (m_CurrentChildIndex >= 0 && m_CurrentChildIndex < childCount) {
            uintptr_t childAddr = m_ChildAddresses[m_CurrentChildIndex];
            std::string label = std::to_string(m_CurrentChildIndex);
            std::string cs = m_Ctx->Ui.GetStringId(childAddr);
            if (!cs.empty()) label += " " + cs;
            DrawHighlighter(childAddr, IM_COL32(255, 80, 80, 255), label);
        }
    }

    void DrawElementPropertiesFor(uintptr_t addr) {
        if (addr == 0) return;

        std::string stringId = m_Ctx->Ui.GetStringId(addr);
        PluginSDK::UiElement elem = m_Ctx->Ui.Read(addr);

        if (!stringId.empty()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f),
                "StringId: \"%s\"", stringId.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                ImGui::SetClipboardText(stringId.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "StringId: %s", I18n::Tr("common_empty"));
        }
        ImGui::Spacing();

        // One "label | value" row with a translated label.
        auto labelCell = [](const char* label) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
        };
        // A section row: "--- <title> ---" in the accent colour.
        auto sectionRow = [](const char* title) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "--- %s ---", title);
            ImGui::TableNextColumn();
        };

        if (ImGui::BeginTable("UiProps", 2,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn(I18n::TrId("uiexp_property"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn(I18n::TrId("common_value"), ImGuiTableColumnFlags_WidthStretch);

            // Address
            labelCell(I18n::Tr("common_address"));
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "0x%llX",
                               static_cast<unsigned long long>(addr));
            if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) {
                char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                       static_cast<unsigned long long>(addr));
                ImGui::SetClipboardText(buf);
            }

            // Parent
            labelCell(I18n::Tr("uiexp_parent"));
            if (elem.ParentAddress != 0)
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "0x%llX",
                                   static_cast<unsigned long long>(elem.ParentAddress));
            else
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", I18n::Tr("uiexp_is_root"));

            // Visible
            bool vis = elem.IsVisible;
            labelCell(I18n::Tr("uiexp_visible_local"));
            ImGui::TextColored(vis ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1), "%s", TrueFalse(vis));

            // Children
            labelCell(I18n::Tr("uiexp_children"));
            ImGui::Text("%d", elem.ChildCount);

            sectionRow(I18n::Tr("uiexp_section_position"));

            float sx, sy, sw, sh;
            if (m_Ctx->Ui.ComputeScreenRect(addr, sx, sy, sw, sh)) {
                labelCell(I18n::Tr("uiexp_screen_pos"));  ImGui::Text("%.1f, %.1f", sx, sy);
                labelCell(I18n::Tr("uiexp_screen_size")); ImGui::Text("%.1f x %.1f", sw, sh);
            }

            labelCell(I18n::Tr("uiexp_relative_pos"));
            ImGui::Text("%.1f, %.1f", elem.RelativeX, elem.RelativeY);

            labelCell(I18n::Tr("uiexp_pos_modifier"));
            if (elem.HasPositionModifier)
                ImGui::Text("%.1f, %.1f", elem.PositionModX, elem.PositionModY);
            else
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "%s", I18n::Tr("uiexp_modifier_off"));

            labelCell(I18n::Tr("uiexp_unscaled_size"));
            ImGui::Text("%.1f x %.1f", elem.UnscaledWidth, elem.UnscaledHeight);

            sectionRow(I18n::Tr("uiexp_section_scale"));

            // Built each frame (not static): the language can change between frames.
            const char* scaleNames[] = { I18n::Tr("uiexp_scale_multiply"), I18n::Tr("uiexp_scale_width_width"),
                                         I18n::Tr("uiexp_scale_height_height"), I18n::Tr("uiexp_scale_width_height") };
            labelCell(I18n::Tr("uiexp_scale_index"));
            ImGui::Text("%u (%s)", elem.ScaleIndex,
                        (elem.ScaleIndex <= 3) ? scaleNames[elem.ScaleIndex] : I18n::Tr("common_unknown"));

            labelCell(I18n::Tr("uiexp_local_scale"));
            ImGui::Text("%.4f", elem.LocalScaleMultiplier);

            sectionRow(I18n::Tr("uiexp_section_flags"));

            labelCell(I18n::Tr("uiexp_flags_raw")); ImGui::Text("0x%08X", elem.Flags);

            labelCell(I18n::Tr("uiexp_element_type"));
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "0x%04X (%u)",
                               elem.ElementType, elem.ElementType);

            // Text from UI element (if any): the game's own text.
            std::string text = m_Ctx->Ui.GetText(addr);
            if (!text.empty()) {
                labelCell(I18n::Tr("uiexp_text"));
                ImGui::TextColored(ImVec4(0.8f, 0.9f, 1, 1), "\"%s\"", text.c_str());
            }

            ImGui::EndTable();
        }
    }

    // A debug dump for pasting into a report: English on purpose, like a log.
    void CopyElementInfo(const std::string& currentSid) {
        PluginSDK::UiElement elem = m_Ctx->Ui.Read(m_CurrentAddress);
        float sx = 0, sy = 0, sw = 0, sh = 0;
        m_Ctx->Ui.ComputeScreenRect(m_CurrentAddress, sx, sy, sw, sh);

        std::ostringstream ss;
        ss << "=== UI Element Debug ===\n";
        ss << "Address: 0x" << std::hex << m_CurrentAddress << "\n";
        ss << "StringId: " << currentSid << "\n";
        ss << std::dec << std::fixed << std::setprecision(1);
        ss << "Screen Pos: " << sx << ", " << sy << "\n";
        ss << "Screen Size: " << sw << " x " << sh << "\n";
        ss << "Unscaled Size: " << elem.UnscaledWidth << " x " << elem.UnscaledHeight << "\n";
        ss << "Visible: " << (elem.IsVisible ? "true" : "false") << "\n";
        ss << "Children: " << m_ChildAddresses.size() << "\n";
        ss << "Flags: 0x" << std::hex << elem.Flags << "\n";
        ss << "ScaleIndex: " << std::dec << static_cast<int>(elem.ScaleIndex) << "\n";
        ss << "ScaleMultiplier: " << elem.LocalScaleMultiplier << "\n";
        ss << "RelPos: " << elem.RelativeX << ", " << elem.RelativeY << "\n";
        ss << "Parent: 0x" << std::hex << elem.ParentAddress << "\n";
        ss << "ElementType: 0x" << std::hex << elem.ElementType
           << " (" << std::dec << elem.ElementType << ")\n";
        ImGui::SetClipboardText(ss.str().c_str());
    }

    const PluginSDK::Context* m_Ctx = nullptr;
    uintptr_t m_CurrentAddress = 0;
    int m_CurrentChildIndex = -1;

    struct BreadcrumbEntry {
        uintptr_t Address = 0;
        std::string Label;
    };
    std::deque<BreadcrumbEntry> m_History;
    std::vector<uintptr_t> m_ChildAddresses;

    char m_SearchBuffer[128] = {};
    std::vector<std::pair<uintptr_t, std::string>> m_SearchResults;

    struct Highlight {
        uintptr_t Address = 0;
        ImU32 Color = 0;
        std::string Label;
    };
    std::vector<Highlight> m_Highlights;   // recorded by Draw(), drawn by DrawHighlights()
};

} // namespace Examples
