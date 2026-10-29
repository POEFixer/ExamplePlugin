// ============================================================================
// ExamplePlugin — v6 SDK showcase
// ============================================================================
// PLUGIN_EXPORTS is defined in the project preprocessor definitions, which
// makes PluginSDK.h emit the PluginSDK_AttachHost export and define the
// PLUGIN_API decorator as __declspec(dllexport).
//
// Features:
//   - Area Info, Player Vitals, WorldToScreen
//   - Player Buffs with filtering and progress bars
//   - Nearby Entities with components (via v6 Components service)
//   - Inventories with names, grid view, items, and per-entity mods
//   - Memory Info with hex viewer
//   - Game UI Explorer (via v6 Ui service — no raw offsets)
//   - Overlay mode support
//   - The info window as a host informational window (PluginInfoWindow.h):
//     gear menu, in / out of game, remembered place (see DrawUI)
//   - Texts in the program language (PluginI18n.h): lang/<code>.json next to
//     the DLL, English compiled in (lang_en.generated.h); how: examples/Text.h
// ============================================================================

#include "../../POEFixer/plugin_sdk/PluginSDK.h"
#include "lang_en.generated.h"   // the embedded English: once per DLL, after PluginSDK.h
#include "../../POEFixer/imgui/imgui.h"
#include "../../POEFixer/imgui/IconsFontAwesome6.h"
#include "../../POEFixer/plugin_sdk/PluginUi.h"
#include "../../POEFixer/plugin_sdk/PluginInfoWindow.h"   // informational windows (see DrawUI)

#include "examples/Text.h"
#include "examples/ExampleBuffs.h"
#include "examples/ExampleEntities.h"
#include "examples/ExampleInventory.h"
#include "examples/ExampleFlasks.h"
#include "examples/ExampleMemory.h"
#include "examples/ExampleUiExplorer.h"
#include "examples/ComponentReader.h"
#include "examples/SdkStatus.h"
#include "examples/ExampleRender.h"
#include "examples/ExampleSkills.h"
#include "examples/ExampleTerrain.h"
#include "examples/ExampleEvents.h"
#include "examples/ExampleLog.h"
#include "examples/ExamplePrices.h"

#include <fstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <cfloat>
#include <algorithm>

namespace I18n = PluginSDK::I18n;
using Examples::Num;
using Examples::Show;

class ExamplePlugin : public PluginSDK::Plugin {
public:
    ExamplePlugin() {
        // Avoid the "first frame elapsed = since 1970" bug from the v5 plugin.
        m_LastInventoryScan = std::chrono::steady_clock::now();
    }

    const char* GetName() const override { return "Example Plugin"; }

    void OnEnable(bool /*isGameAttached*/) override {
        LoadSettings();
        BindImGui();
        Examples::SubscribeAll(ctx(), m_eventsState);
        ctx()->Log.Info("ExamplePlugin v6 enabled");
    }

    void OnDisable() override {
        Examples::UnsubscribeAll(ctx(), m_eventsState);
        ctx()->Log.Info("ExamplePlugin v6 disabled");
    }

    // Settings use the plugin UI kit (PluginUi.h): one card per section, one
    // measured label column per block, a tip on every non-obvious control,
    // dependents dimmed rather than hidden. This is the reference layout for
    // plugin settings; tools/plugin_ui_preview renders it to PNG.
    void DrawSettings() override {
        BindImGui();   // the menu's context (see BindImGui)
        using namespace PluginUi;
        bool changed = false;

        // --- Window ----------------------------------------------------------
        // Every text is a key of lang/en.json (see examples/Text.h). Check() keeps its
        // own ImGui id ("###showWindow"), so only the label is translated.
        BeginCard("##exampleWindow");
        Heading(ICON_FA_WINDOW_MAXIMIZE, I18n::Tr("settings_info_window"));
        changed |= Check(I18n::Tr("settings_show_window"), "showWindow", &m_ShowWindow,
                         I18n::Tr("settings_show_window_tip"));
        ImGui::SameLine();
        StatusPill(HostCompatible() ? I18n::Tr("settings_host_ok") : I18n::Tr("settings_host_too_old"),
                   ImGui::ColorConvertFloat4ToU32(HostCompatible() ? Colors().Success : Colors().Error));
        Tip(I18n::Tr("settings_host_tip"));
        changed |= Check(I18n::Tr("settings_overlay_mode"), "overlay", &m_WantsOverlay,
                         I18n::Tr("settings_overlay_mode_tip"));
        {
            // The label column is measured on the translated labels, every frame.
            const char* opacityLabel = I18n::Tr("settings_window_opacity");
            const char* hotkeyLabel = I18n::Tr("settings_toggle_hotkey");
            const LabelColumn col({ opacityLabel, hotkeyLabel });
            ImGui::BeginDisabled(!m_ShowWindow);
            changed |= SliderFloatRow(col, opacityLabel, "##opacity", &m_WindowAlpha, 0.3f, 1.0f, "%.1f",
                                      I18n::Tr("settings_window_opacity_tip"));
            ImGui::EndDisabled();
            col.RowNoWidth(hotkeyLabel);
            changed |= HotkeyCapture("##toggleKey", &m_ToggleKey, I18n::Tr("settings_toggle_hotkey_tip"));
        }
        EndCard();
        ImGui::Spacing();

        // --- Demo tabs -------------------------------------------------------
        BeginCard("##examplePanels");
        Heading(ICON_FA_TABLE_COLUMNS, I18n::Tr("settings_demo_tabs"), I18n::Tr("settings_demo_tabs_help"));
        ImGui::BeginDisabled(!m_ShowWindow);
        // Groups 0-1 and 2-3 side by side when the card is wide enough, else stacked.
        // kPanelGroups / kPanels hold keys: the texts are looked up here, each frame.
        auto drawGroups = [&](int firstGroup, int lastGroup) {
            int shown = -1;
            for (const PanelSwitch& s : kPanels) {
                if (s.group < firstGroup || s.group > lastGroup) continue;
                if (s.group != shown) {
                    if (shown >= 0) ImGui::Spacing();
                    ImGui::TextDisabled("%s", I18n::Tr(kPanelGroups[s.group]));
                    shown = s.group;
                }
                changed |= Check(I18n::Tr(s.labelKey), s.id, &(this->*s.member), I18n::Tr(s.tipKey));
            }
        };
        if (WideEnoughForTwoColumns()) {
            // Explicit positions rather than SameLine: a text line placed after a
            // group of framed rows would inherit their baseline offset.
            const ImVec2 start = ImGui::GetCursorPos();
            const float half = (ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            ImGui::BeginGroup();
            drawGroups(0, 1);
            ImGui::EndGroup();
            const float leftBottom = ImGui::GetCursorPosY();
            ImGui::SetCursorPos(ImVec2(start.x + half, start.y));
            ImGui::BeginGroup();
            drawGroups(2, 3);
            ImGui::EndGroup();
            const float rightBottom = ImGui::GetCursorPosY();
            ImGui::SetCursorPos(ImVec2(start.x, (std::max)(leftBottom, rightBottom)));
        } else {
            drawGroups(0, 1);
            ImGui::Spacing();
            drawGroups(2, 3);
        }
        ImGui::EndDisabled();

        ImGui::Spacing();
        const std::string resetLabel =
            std::string(ICON_FA_ROTATE_LEFT "  ") + I18n::Tr("settings_reset_tabs") + "###resetTabs";
        AlignRight(ButtonWidth(resetLabel.c_str()));
        if (SubtleButton(resetLabel.c_str())) ImGui::OpenPopup("##confirmReset");
        Tip(I18n::Tr("settings_reset_tabs_tip"));
        // No "no" text: the kit's own "Cancel" in the program language.
        if (ConfirmPopup("##confirmReset", I18n::Tr("settings_reset_tabs_confirm"), I18n::Tr("settings_reset"))) {
            for (const PanelSwitch& s : kPanels) this->*s.member = s.defaultOn;
            changed = true;
        }
        EndCard();

        if (changed) SaveSettings();
    }

    void DrawUI() override {
        BindImGui();   // the in-game overlay's context (see BindImGui)
        // Toggle hotkey: one cheap key-state read per frame, edge-triggered.
        if (m_ToggleKey != 0) {
            const bool down = (::GetAsyncKeyState(m_ToggleKey) & 0x8000) != 0;
            if (down && !m_ToggleKeyDown) m_ShowWindow = !m_ShowWindow;
            m_ToggleKeyDown = down;
        }
        if (!m_ShowWindow) return;

        PluginSDK::Snapshot snapshot = ctx()->Game.GetSnapshot();

        // --- Informational window (PluginInfoWindow.h, POEFixer 366+) ---------
        // PluginSDK::InfoWindow replaces ImGui::Begin / End for a window that shows
        // information (a counter, a list, this demo). The host then:
        //   * draws a gear button (in the title bar; top-right corner on hover for a
        //     title-less window) with Lock window, Game overlay mode, Reset position
        //     and Close (Close only when an `open` flag is passed);
        //   * remembers the window's position (in game and out of game separately),
        //     size and collapsed state in its own file, keyed by plugin id + o.id;
        //   * with Game overlay mode off, draws the window outside the game, anywhere
        //     on the screens, in another ImGui context it binds this plugin to while
        //     the window is open.
        // So: never SetNextWindowPos / Collapsed (nor Size, unless the window is
        // AlwaysAutoResize / NoResize) for it; push window-level style and call
        // SetNextWindowBgAlpha / SetNextWindowSizeConstraints before it and pop only
        // after it ended; inside it draw only into the window (no
        // GetForegroundDrawList: the UI Explorer's on-game highlights are drawn after
        // the window, below); after Close save the flag like any setting. On an older
        // host it is a plain ImGui window at legacyPos / defaultPos.
        PluginSDK::InfoWindowOptions o;
        o.id = "main";                            // stable, 1-48 of [A-Za-z0-9_.-]
        o.title = "Example Plugin v6";
        o.flags = 0;                              // resizable: the host keeps its size
        o.defaultPos = ImVec2(60.0f, 60.0f);
        o.defaultSize = ImVec2(480.0f, 420.0f);

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowBgAlpha(m_WindowAlpha);
        {
            PluginSDK::InfoWindow window(ctx()->Overlay, o, &m_ShowWindow);   // Close clears m_ShowWindow
            if (window) DrawWindowContents(snapshot);
        }   // the window ends here
        if (!m_ShowWindow) SaveSettings();       // closed from its title-bar X or gear menu
        m_UiExplorer.DrawHighlights();           // over the game, now that the window ended
    }

    // The info window's contents (the window is open; see DrawUI).
    void DrawWindowContents(const PluginSDK::Snapshot& snapshot) {
        if (!snapshot.IsAttached) {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", I18n::Tr("window_not_attached"));
            return;
        }

        bool inGame = (snapshot.State == PluginSDK::GameState::InGame);

        if (ctx()->Game.IsOverlayMode()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1), "%s", I18n::Tr("window_overlay_mode"));
        }

        if (!inGame) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s",
                I18n::TrF("window_not_in_game", { { "state", Num("%d", static_cast<int>(snapshot.State)) } }).c_str());
            return;
        }

        {
            auto status = Examples::ComputeCoverage(ctx(), snapshot);
            Examples::DrawCoverageSummary(status);
        }
        ImGui::Separator();

        // TrId: a tab keeps its ImGui id (and so its selection) when the language changes.
        if (ImGui::BeginTabBar("##ExampleTabs")) {
            if (ImGui::BeginTabItem(I18n::TrId("tab_area_vitals"))) {
                DrawAreaAndVitals(snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowBuffs && ImGui::BeginTabItem(I18n::TrId("tab_buffs"))) {
                Examples::DrawBuffsPanel(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowEntities && ImGui::BeginTabItem(I18n::TrId("tab_entities"))) {
                Examples::DrawEntitiesPanel(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowInventory && ImGui::BeginTabItem(I18n::TrId("tab_inventories"))) {
                Examples::DrawInventoryPanel(ctx(), m_LastInventoryScan);
                ImGui::EndTabItem();
            }

            if (m_ShowFlasks && ImGui::BeginTabItem(I18n::TrId("tab_flasks"))) {
                Examples::DrawFlasksPanel(ctx());
                ImGui::EndTabItem();
            }

            if (m_ShowMemoryInfo && ImGui::BeginTabItem(I18n::TrId("tab_memory"))) {
                Examples::DrawMemoryPanel(ctx());
                ImGui::EndTabItem();
            }

            if (m_ShowUiExplorer && ImGui::BeginTabItem(I18n::TrId("tab_ui_explorer"))) {
                m_UiExplorer.Draw(ctx());
                ImGui::EndTabItem();
            }

            if (m_ShowComponentReader && ImGui::BeginTabItem(I18n::TrId("tab_components"))) {
                Examples::ShowComponentReaderDemo(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowRender && ImGui::BeginTabItem(I18n::TrId("tab_render"))) {
                Examples::DrawRenderPanel(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowTerrain && ImGui::BeginTabItem(I18n::TrId("tab_terrain"))) {
                Examples::DrawTerrainPanel(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowEvents && ImGui::BeginTabItem(I18n::TrId("tab_events"))) {
                Examples::DrawEventsPanel(ctx(), m_eventsState);
                ImGui::EndTabItem();
            }

            if (m_ShowLog && ImGui::BeginTabItem(I18n::TrId("tab_log"))) {
                Examples::DrawLogPanel(ctx(), m_logSentCount);
                ImGui::EndTabItem();
            }

            if (m_ShowSkills && ImGui::BeginTabItem(I18n::TrId("tab_skills"))) {
                Examples::DrawSkillsPanel(ctx(), snapshot, m_skillsState);
                ImGui::EndTabItem();
            }

            if (m_ShowPrices && ImGui::BeginTabItem(I18n::TrId("tab_prices"))) {
                Examples::DrawPricesPanel(ctx());
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    bool WantsOverlay() const override { return m_WantsOverlay; }

    void SaveSettings() override {
        namespace fs = std::filesystem;
        fs::path dir = DirectoryPath() / "config";
        std::error_code ec;
        fs::create_directories(dir, ec);
        std::ofstream file(dir / "settings.txt");
        if (!file.is_open()) return;
        file << "ShowWindow=" << (m_ShowWindow ? 1 : 0) << "\n";
        file << "ShowEntities=" << (m_ShowEntities ? 1 : 0) << "\n";
        file << "ShowBuffs=" << (m_ShowBuffs ? 1 : 0) << "\n";
        file << "ShowInventory=" << (m_ShowInventory ? 1 : 0) << "\n";
        file << "ShowFlasks=" << (m_ShowFlasks ? 1 : 0) << "\n";
        file << "ShowMemoryInfo=" << (m_ShowMemoryInfo ? 1 : 0) << "\n";
        file << "ShowUiExplorer=" << (m_ShowUiExplorer ? 1 : 0) << "\n";
        file << "ShowComponentReader=" << (m_ShowComponentReader ? 1 : 0) << "\n";
        file << "ShowRender=" << (m_ShowRender ? 1 : 0) << "\n";
        file << "ShowTerrain=" << (m_ShowTerrain ? 1 : 0) << "\n";
        file << "ShowEvents=" << (m_ShowEvents ? 1 : 0) << "\n";
        file << "ShowLog=" << (m_ShowLog ? 1 : 0) << "\n";
        file << "ShowSkills=" << (m_ShowSkills ? 1 : 0) << "\n";
        file << "ShowPrices=" << (m_ShowPrices ? 1 : 0) << "\n";
        file << "WantsOverlay=" << (m_WantsOverlay ? 1 : 0) << "\n";
        file << "WindowAlpha=" << m_WindowAlpha << "\n";
        file << "ToggleKey=" << m_ToggleKey << "\n";
    }

private:
    // Bind this DLL's own ImGui copy to the host's context at EVERY entry point
    // that draws (OnEnable, DrawSettings, DrawUI). With Settings -> "Separate menu
    // window" DrawSettings runs in the menu window's context and DrawUI in the
    // in-game overlay's: two contexts, each with its own frame count, time,
    // display size and windows. The host hands over the right one before each
    // call (ctx()->ImGuiContext); never compare ImGui::GetFrameCount(), ImGui
    // ids or windows between DrawSettings and DrawUI, and take the game's size
    // from ctx()->Game.GetScreenSize(), not io.DisplaySize, in DrawSettings.
    void BindImGui() const {
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
    }

    void DrawAreaAndVitals(const PluginSDK::Snapshot& snapshot) {
        // A printf line becomes a TrF text with named placeholders ("Level: {level}");
        // Num formats the number, the translation places it.
        if (ImGui::CollapsingHeader(I18n::TrId("area_info"), ImGuiTreeNodeFlags_DefaultOpen)) {
            Show(I18n::TrF("area_name", { { "name", snapshot.CurrentAreaName } }));
            Show(I18n::TrF("area_level", { { "level", Num("%d", snapshot.CurrentAreaLevel) } }));
            Show(I18n::TrF("area_hash", { { "hash", snapshot.CurrentAreaHash } }));
            // Two texts on one line: the gap is layout, so it is code (SameLine), never spaces in a value.
            Show(I18n::TrF("area_town", { { "town", Examples::YesNo(snapshot.IsTown) } }));
            ImGui::SameLine(0.0f, ImGui::GetFontSize());
            Show(I18n::TrF("area_hideout", { { "hideout", Examples::YesNo(snapshot.IsHideout) } }));
            Show(I18n::TrF("area_change_count",
                           { { "count", Num("%llu", static_cast<unsigned long long>(snapshot.AreaChangeCounter)) } }));
            Show(I18n::TrF("area_screen_size", { { "width", Num("%d", snapshot.ScreenWidth) },
                                                 { "height", Num("%d", snapshot.ScreenHeight) } }));
        }

        // Map modifiers: match on StatId (the Stats.dat Id), not on statRowIndex,
        // which moves when a patch inserts Stats.dat rows. mod.text is the game's own
        // text and stays as the game gives it.
        if (ImGui::CollapsingHeader(I18n::TrId("area_mods"))) {
            const auto mods = ctx()->Game.GetAreaMods();
            if (mods.empty()) ImGui::TextDisabled("%s", I18n::Tr("area_mods_none"));
            for (const auto& mod : mods) {
                ImGui::BulletText("%s", mod.text.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s = %d", mod.statId.empty() ? I18n::Tr("area_mod_no_id") : mod.statId.c_str(), mod.value);
            }
        }

        if (ImGui::CollapsingHeader(I18n::TrId("vitals_header"), ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& v = snapshot.Vitals;

            float hpFrac = v.MaxHP > 0 ? (float)v.CurrentHP / v.MaxHP : 0;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            const std::string hpOv = I18n::TrF("vitals_hp", { { "current", Num("%d", v.CurrentHP) },
                                                              { "max", Num("%d", v.MaxHP) },
                                                              { "percent", Num("%d", v.HPPercent) } });
            // Height 0: the frame height, so the overlay text fits at every font size and script.
            ImGui::ProgressBar(hpFrac, ImVec2(-FLT_MIN, 0.0f), hpOv.c_str());
            ImGui::PopStyleColor();

            if (v.MaxES > 0) {
                float esFrac = (float)v.CurrentES / v.MaxES;
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
                const std::string esOv = I18n::TrF("vitals_es", { { "current", Num("%d", v.CurrentES) },
                                                                  { "max", Num("%d", v.MaxES) },
                                                                  { "percent", Num("%d", v.ESPercent) } });
                ImGui::ProgressBar(esFrac, ImVec2(-FLT_MIN, 0.0f), esOv.c_str());
                ImGui::PopStyleColor();
            }

            float mpFrac = v.MaxMP > 0 ? (float)v.CurrentMP / v.MaxMP : 0;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.2f, 0.8f, 1.0f));
            const std::string mpOv = I18n::TrF("vitals_mp", { { "current", Num("%d", v.CurrentMP) },
                                                              { "max", Num("%d", v.MaxMP) },
                                                              { "percent", Num("%d", v.MPPercent) } });
            ImGui::ProgressBar(mpFrac, ImVec2(-FLT_MIN, 0.0f), mpOv.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();

            Show(I18n::TrF("vitals_world", { { "x", Num("%.0f", snapshot.Player.WorldX) },
                                             { "y", Num("%.0f", snapshot.Player.WorldY) },
                                             { "z", Num("%.0f", snapshot.Player.WorldZ) } }));
            Show(I18n::TrF("vitals_grid", { { "x", Num("%.1f", snapshot.Player.GridPositionX) },
                                            { "y", Num("%.1f", snapshot.Player.GridPositionY) } }));

            float sx, sy;
            if (ctx()->Render.WorldToScreen(
                    snapshot.Player.WorldX, snapshot.Player.WorldY,
                    snapshot.Player.WorldZ, sx, sy)) {
                Show(I18n::TrF("vitals_screen", { { "x", Num("%.0f", sx) }, { "y", Num("%.0f", sy) } }));
            }

            // Player path (wchar → narrow display)
            const auto& p = snapshot.Player;
            if (!p.Path.empty()) {
                std::string narrowPath;
                narrowPath.reserve(p.Path.size());
                for (wchar_t c : p.Path)
                    narrowPath += (c < 128) ? static_cast<char>(c) : '?';
                Show(I18n::TrF("vitals_path", { { "path", narrowPath } }));
            }
            // NearbyZone is a distance-from-player classification; for the Player
            // entity itself it has no meaning (always None). Shown per-entity
            // in the Entities tab where it's actually useful.
            Show(I18n::TrF("vitals_entity_id", { { "id", Num("%u", p.Id) } }));
        }
    }

    void LoadSettings() {
        namespace fs = std::filesystem;
        fs::path settingsPath = DirectoryPath() / "config" / "settings.txt";
        if (!fs::exists(settingsPath)) return;

        std::ifstream file(settingsPath);
        std::string line;
        while (std::getline(file, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);

            if      (key == "ShowWindow")          m_ShowWindow = (val == "1");
            else if (key == "ShowEntities")        m_ShowEntities = (val == "1");
            else if (key == "ShowBuffs")           m_ShowBuffs = (val == "1");
            else if (key == "ShowInventory")       m_ShowInventory = (val == "1");
            else if (key == "ShowFlasks")          m_ShowFlasks = (val == "1");
            else if (key == "ShowMemoryInfo")      m_ShowMemoryInfo = (val == "1");
            else if (key == "ShowUiExplorer")      m_ShowUiExplorer = (val == "1");
            else if (key == "ShowComponentReader") m_ShowComponentReader = (val == "1");
            else if (key == "ShowRender")          m_ShowRender = (val == "1");
            else if (key == "ShowTerrain")         m_ShowTerrain = (val == "1");
            else if (key == "ShowEvents")          m_ShowEvents = (val == "1");
            else if (key == "ShowLog")             m_ShowLog = (val == "1");
            else if (key == "ShowSkills")          m_ShowSkills = (val == "1");
            else if (key == "ShowPrices")          m_ShowPrices = (val == "1");
            else if (key == "WantsOverlay")        m_WantsOverlay = (val == "1");
            else if (key == "WindowAlpha") {
                try { m_WindowAlpha = std::clamp(std::stof(val), 0.3f, 1.0f); } catch (...) {}
            }
            else if (key == "ToggleKey") {
                try { m_ToggleKey = std::clamp(std::stoi(val), 0, 0xFE); } catch (...) {}
            }
        }
    }

    bool m_ShowWindow = true;
    bool m_ShowEntities = true;
    bool m_ShowBuffs = true;
    bool m_ShowInventory = false;
    bool m_ShowFlasks = true;
    bool m_ShowMemoryInfo = false;
    bool m_ShowUiExplorer = false;
    bool m_ShowComponentReader = false;
    bool m_ShowRender = true;
    bool m_ShowTerrain = true;
    bool m_ShowEvents = true;
    bool m_ShowLog = true;
    bool m_ShowSkills = true;
    bool m_ShowPrices = true;
    Examples::SkillsTabState m_skillsState;
    bool m_WantsOverlay = false;
    float m_WindowAlpha = 0.9f;
    int m_ToggleKey = 0;            // Windows virtual-key code, 0 = none
    bool m_ToggleKeyDown = false;

    // The demo-tab switches, drawn by DrawSettings in groups. The table holds KEYS of
    // lang/en.json, never texts: a static is built once, the language can change later.
    // I18n::Key marks each one for tools/plugin_i18n/gen.py --gate, which checks that the key is
    // in lang/en.json (a bare "panel_buffs" literal would go unchecked, and en.json would
    // report it as never used).
    struct PanelSwitch {
        const char* labelKey;
        const char* id;
        bool ExamplePlugin::* member;
        bool defaultOn;
        int group;
        const char* tipKey;
    };
    static constexpr const char* kPanelGroups[] = {
        I18n::Key("panel_group_player"), I18n::Key("panel_group_prices"),
        I18n::Key("panel_group_inspectors"), I18n::Key("panel_group_services") };
    static constexpr PanelSwitch kPanels[] = {
        { I18n::Key("panel_buffs"), "buffs", &ExamplePlugin::m_ShowBuffs, true, 0, I18n::Key("panel_buffs_tip") },
        { I18n::Key("panel_flasks"), "flasks", &ExamplePlugin::m_ShowFlasks, true, 0, I18n::Key("panel_flasks_tip") },
        { I18n::Key("panel_inventory"), "inventory", &ExamplePlugin::m_ShowInventory, false, 0, I18n::Key("panel_inventory_tip") },
        { I18n::Key("panel_skills"), "skills", &ExamplePlugin::m_ShowSkills, true, 0, I18n::Key("panel_skills_tip") },
        { I18n::Key("panel_prices"), "prices", &ExamplePlugin::m_ShowPrices, true, 1, I18n::Key("panel_prices_tip") },
        { I18n::Key("panel_entities"), "entities", &ExamplePlugin::m_ShowEntities, true, 2, I18n::Key("panel_entities_tip") },
        { I18n::Key("panel_components"), "components", &ExamplePlugin::m_ShowComponentReader, false, 2, I18n::Key("panel_components_tip") },
        { I18n::Key("panel_ui_explorer"), "uiExplorer", &ExamplePlugin::m_ShowUiExplorer, false, 2, I18n::Key("panel_ui_explorer_tip") },
        { I18n::Key("panel_memory"), "memory", &ExamplePlugin::m_ShowMemoryInfo, false, 2, I18n::Key("panel_memory_tip") },
        { I18n::Key("panel_render"), "render", &ExamplePlugin::m_ShowRender, true, 3, I18n::Key("panel_render_tip") },
        { I18n::Key("panel_terrain"), "terrain", &ExamplePlugin::m_ShowTerrain, true, 3, I18n::Key("panel_terrain_tip") },
        { I18n::Key("panel_events"), "events", &ExamplePlugin::m_ShowEvents, true, 3, I18n::Key("panel_events_tip") },
        { I18n::Key("panel_log"), "log", &ExamplePlugin::m_ShowLog, true, 3, I18n::Key("panel_log_tip") },
    };
    std::chrono::steady_clock::time_point m_LastInventoryScan;
    Examples::PluginUiExplorer m_UiExplorer;
    Examples::EventsDemoState m_eventsState;
    int m_logSentCount = 0;
};

// ============================================================================
// v6 factory exports
// ============================================================================

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() {
    return new ExamplePlugin();
}

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) {
    delete plugin;
}

// Plugin info: the host checks it before CreatePlugin; the publish tool reads it.
POEFIXER_PLUGIN_INFO(
    "ExamplePlugin",
    "Example Plugin",
    "1.0.4",
    "POEFixer",
    "Reference plugin for developers: live demos of every SDK service, from entities, buffs and inventories to the game UI tree, memory and prices.")
