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
// ============================================================================

#include "../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../POEFixer/imgui/imgui.h"
#include "../../POEFixer/imgui/IconsFontAwesome6.h"
#include "../../POEFixer/plugin_sdk/PluginUi.h"

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

class ExamplePlugin : public PluginSDK::Plugin {
public:
    ExamplePlugin() {
        // Avoid the "first frame elapsed = since 1970" bug from the v5 plugin.
        m_LastInventoryScan = std::chrono::steady_clock::now();
    }

    const char* GetName() const override { return "Example Plugin"; }

    void OnEnable(bool /*isGameAttached*/) override {
        LoadSettings();
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
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
        using namespace PluginUi;
        bool changed = false;

        // --- Window ----------------------------------------------------------
        BeginCard("##exampleWindow");
        Heading(ICON_FA_WINDOW_MAXIMIZE, "Info window");
        changed |= Check("Show the info window", "showWindow", &m_ShowWindow,
                         "Opens the Example Plugin window with one tab per demo.\n"
                         "Closing the window with its X turns this off.");
        ImGui::SameLine();
        StatusPill(HostCompatible() ? "Host SDK v6" : "Host too old",
                   ImGui::ColorConvertFloat4ToU32(HostCompatible() ? Colors().Success : Colors().Error));
        Tip("Whether this POE2Fixer offers every service the plugin was built for.\n"
            "When it does not, the services answer empty values.");
        changed |= Check("Overlay mode", "overlay", &m_WantsOverlay,
                         "Asks the host to draw over the game (the overlay window).\n"
                         "Off: the window only shows while the POE2Fixer menu is open.");
        {
            const LabelColumn col({ "Window opacity", "Toggle hotkey" });
            ImGui::BeginDisabled(!m_ShowWindow);
            changed |= SliderFloatRow(col, "Window opacity", "##opacity", &m_WindowAlpha, 0.3f, 1.0f, "%.1f",
                                      "How see-through the window background is.\n1.0 is fully opaque.");
            ImGui::EndDisabled();
            col.RowNoWidth("Toggle hotkey");
            changed |= HotkeyCapture("##toggleKey", &m_ToggleKey,
                                     "A key that shows or hides the info window while you play.\n"
                                     "Click, then press a key or a mouse button; Esc cancels.");
        }
        EndCard();
        ImGui::Spacing();

        // --- Demo tabs -------------------------------------------------------
        BeginCard("##examplePanels");
        Heading(ICON_FA_TABLE_COLUMNS, "Demo tabs");
        HelpMarker("Each switch adds one tab to the info window.\n"
                   "Every tab demonstrates one SDK service; its source is in examples/.");
        ImGui::BeginDisabled(!m_ShowWindow);
        // Groups 0-1 and 2-3 side by side when the card is wide enough, else stacked.
        auto drawGroups = [&](int firstGroup, int lastGroup) {
            int shown = -1;
            for (const PanelSwitch& s : kPanels) {
                if (s.group < firstGroup || s.group > lastGroup) continue;
                if (s.group != shown) {
                    if (shown >= 0) ImGui::Spacing();
                    ImGui::TextDisabled("%s", kPanelGroups[s.group]);
                    shown = s.group;
                }
                changed |= Check(s.label, s.id, &(this->*s.member), s.tip);
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
        const char* resetLabel = ICON_FA_ROTATE_LEFT "  Reset tabs###resetTabs";
        AlignRight(ButtonWidth(resetLabel));
        if (SubtleButton(resetLabel)) ImGui::OpenPopup("##confirmReset");
        Tip("Turns every demo tab back to its default.");
        if (ConfirmPopup("##confirmReset", "Turn every demo tab back to its default?", "Reset", "Cancel")) {
            for (const PanelSwitch& s : kPanels) this->*s.member = s.defaultOn;
            changed = true;
        }
        EndCard();

        if (changed) SaveSettings();
    }

    void DrawUI() override {
        // Toggle hotkey: one cheap key-state read per frame, edge-triggered.
        if (m_ToggleKey != 0) {
            const bool down = (::GetAsyncKeyState(m_ToggleKey) & 0x8000) != 0;
            if (down && !m_ToggleKeyDown) m_ShowWindow = !m_ShowWindow;
            m_ToggleKeyDown = down;
        }
        if (!m_ShowWindow) return;
        if (ctx()->ImGuiContext)
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

        PluginSDK::Snapshot snapshot = ctx()->Game.GetSnapshot();

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowBgAlpha(m_WindowAlpha);

        if (!ImGui::Begin("Example Plugin v6##ExamplePlugin", &m_ShowWindow)) {
            ImGui::End();
            return;
        }

        if (!snapshot.IsAttached) {
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Game not attached");
            ImGui::End();
            return;
        }

        bool inGame = (snapshot.State == PluginSDK::GameState::InGame);

        if (ctx()->Game.IsOverlayMode()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1), "[OVERLAY MODE]");
        }

        if (!inGame) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Not in game (state: %d)",
                static_cast<int>(snapshot.State));
            ImGui::End();
            return;
        }

        {
            auto status = Examples::ComputeCoverage(ctx(), snapshot);
            Examples::DrawCoverageSummary(status);
        }
        ImGui::Separator();

        if (ImGui::BeginTabBar("##ExampleTabs")) {
            if (ImGui::BeginTabItem("Area & Vitals")) {
                DrawAreaAndVitals(snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowBuffs && ImGui::BeginTabItem("Buffs")) {
                Examples::DrawBuffsPanel(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowEntities && ImGui::BeginTabItem("Entities")) {
                Examples::DrawEntitiesPanel(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowInventory && ImGui::BeginTabItem("Inventories")) {
                Examples::DrawInventoryPanel(ctx(), m_LastInventoryScan);
                ImGui::EndTabItem();
            }

            if (m_ShowFlasks && ImGui::BeginTabItem("Flasks & Charms")) {
                Examples::DrawFlasksPanel(ctx());
                ImGui::EndTabItem();
            }

            if (m_ShowMemoryInfo && ImGui::BeginTabItem("Memory")) {
                Examples::DrawMemoryPanel(ctx());
                ImGui::EndTabItem();
            }

            if (m_ShowUiExplorer && ImGui::BeginTabItem("UI Explorer")) {
                m_UiExplorer.Draw(ctx());
                ImGui::EndTabItem();
            }

            if (m_ShowComponentReader && ImGui::BeginTabItem("Components")) {
                Examples::ShowComponentReaderDemo(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowRender && ImGui::BeginTabItem("Render")) {
                Examples::DrawRenderPanel(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowTerrain && ImGui::BeginTabItem("Terrain")) {
                Examples::DrawTerrainPanel(ctx(), snapshot);
                ImGui::EndTabItem();
            }

            if (m_ShowEvents && ImGui::BeginTabItem("Events")) {
                Examples::DrawEventsPanel(ctx(), m_eventsState);
                ImGui::EndTabItem();
            }

            if (m_ShowLog && ImGui::BeginTabItem("Log")) {
                Examples::DrawLogPanel(ctx(), m_logSentCount);
                ImGui::EndTabItem();
            }

            if (m_ShowSkills && ImGui::BeginTabItem("Skills & Timing")) {
                Examples::DrawSkillsPanel(ctx(), snapshot, m_skillsState);
                ImGui::EndTabItem();
            }

            if (m_ShowPrices && ImGui::BeginTabItem("Prices")) {
                Examples::DrawPricesPanel(ctx());
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
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
    void DrawAreaAndVitals(const PluginSDK::Snapshot& snapshot) {
        if (ImGui::CollapsingHeader("Area Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Area: %s", snapshot.CurrentAreaName.c_str());
            ImGui::Text("Level: %d", snapshot.CurrentAreaLevel);
            ImGui::Text("Hash: %s", snapshot.CurrentAreaHash.c_str());
            ImGui::Text("Town: %s  Hideout: %s",
                snapshot.IsTown ? "Yes" : "No",
                snapshot.IsHideout ? "Yes" : "No");
            ImGui::Text("Area Change #: %llu",
                static_cast<unsigned long long>(snapshot.AreaChangeCounter));
            ImGui::Text("Screen: %dx%d", snapshot.ScreenWidth, snapshot.ScreenHeight);
        }

        // Map modifiers: match on StatId (the Stats.dat Id), not on statRowIndex,
        // which moves when a patch inserts Stats.dat rows.
        if (ImGui::CollapsingHeader("Area Modifiers")) {
            const auto mods = ctx()->Game.GetAreaMods();
            if (mods.empty()) ImGui::TextDisabled("No area modifiers (not in a map?)");
            for (const auto& mod : mods) {
                ImGui::BulletText("%s", mod.text.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s = %d", mod.statId.empty() ? "(older host: no id)" : mod.statId.c_str(), mod.value);
            }
        }

        if (ImGui::CollapsingHeader("Player Vitals", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& v = snapshot.Vitals;

            float hpFrac = v.MaxHP > 0 ? (float)v.CurrentHP / v.MaxHP : 0;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            char hpOv[64]; snprintf(hpOv, sizeof(hpOv), "HP: %d / %d (%d%%)",
                v.CurrentHP, v.MaxHP, v.HPPercent);
            ImGui::ProgressBar(hpFrac, ImVec2(-1, 18), hpOv);
            ImGui::PopStyleColor();

            if (v.MaxES > 0) {
                float esFrac = (float)v.CurrentES / v.MaxES;
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
                char esOv[64]; snprintf(esOv, sizeof(esOv), "ES: %d / %d (%d%%)",
                    v.CurrentES, v.MaxES, v.ESPercent);
                ImGui::ProgressBar(esFrac, ImVec2(-1, 18), esOv);
                ImGui::PopStyleColor();
            }

            float mpFrac = v.MaxMP > 0 ? (float)v.CurrentMP / v.MaxMP : 0;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.2f, 0.8f, 1.0f));
            char mpOv[64]; snprintf(mpOv, sizeof(mpOv), "MP: %d / %d (%d%%)",
                v.CurrentMP, v.MaxMP, v.MPPercent);
            ImGui::ProgressBar(mpFrac, ImVec2(-1, 18), mpOv);
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::Text("World: %.0f, %.0f, %.0f",
                snapshot.Player.WorldX, snapshot.Player.WorldY, snapshot.Player.WorldZ);
            ImGui::Text("Grid: %.1f, %.1f",
                snapshot.Player.GridPositionX, snapshot.Player.GridPositionY);

            float sx, sy;
            if (ctx()->Render.WorldToScreen(
                    snapshot.Player.WorldX, snapshot.Player.WorldY,
                    snapshot.Player.WorldZ, sx, sy)) {
                ImGui::Text("Screen: %.0f, %.0f", sx, sy);
            }

            // Player path (wchar → narrow display)
            const auto& p = snapshot.Player;
            if (!p.Path.empty()) {
                std::string narrowPath;
                narrowPath.reserve(p.Path.size());
                for (wchar_t c : p.Path)
                    narrowPath += (c < 128) ? static_cast<char>(c) : '?';
                ImGui::Text("Path: %s", narrowPath.c_str());
            }
            // NearbyZone is a distance-from-player classification; for the Player
            // entity itself it has no meaning (always None). Shown per-entity
            // in the Entities tab where it's actually useful.
            ImGui::Text("Entity ID: %u", p.Id);
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

    // The demo-tab switches, drawn by DrawSettings in groups.
    struct PanelSwitch {
        const char* label;
        const char* id;
        bool ExamplePlugin::* member;
        bool defaultOn;
        int group;
        const char* tip;
    };
    static constexpr const char* kPanelGroups[] = { "Player and items", "Prices", "Inspectors", "Services" };
    static constexpr PanelSwitch kPanels[] = {
        { "Player buffs", "buffs", &ExamplePlugin::m_ShowBuffs, true, 0,
          "Active buffs with remaining time, charges and a text filter." },
        { "Flasks and charms", "flasks", &ExamplePlugin::m_ShowFlasks, true, 0,
          "Flask and charm slots, charges and active state." },
        { "Inventories", "inventory", &ExamplePlugin::m_ShowInventory, false, 0,
          "Inventory grids with item details and mods.\nScans at most once a second." },
        { "Skills and timing", "skills", &ExamplePlugin::m_ShowSkills, true, 0,
          "The skill bar, cooldowns and cast timing." },
        { "Prices", "prices", &ExamplePlugin::m_ShowPrices, true, 1,
          "Item prices through the host's price service\n(source and league: POE2Fixer Settings -> Prices)." },
        { "Nearby entities", "entities", &ExamplePlugin::m_ShowEntities, true, 2,
          "Monsters, NPCs, chests and items around you,\nwith their components." },
        { "Component reader", "components", &ExamplePlugin::m_ShowComponentReader, false, 2,
          "Every component reader and UI element helper of the SDK." },
        { "Game UI explorer", "uiExplorer", &ExamplePlugin::m_ShowUiExplorer, false, 2,
          "Browse the game's UI tree; highlights the selected element." },
        { "Memory viewer", "memory", &ExamplePlugin::m_ShowMemoryInfo, false, 2,
          "Hex view and typed reads through the memory service." },
        { "Render demo", "render", &ExamplePlugin::m_ShowRender, true, 3,
          "World-to-screen projection and overlay drawing." },
        { "Terrain demo", "terrain", &ExamplePlugin::m_ShowTerrain, true, 3,
          "Walkable grid and terrain height queries." },
        { "Events demo", "events", &ExamplePlugin::m_ShowEvents, true, 3,
          "Area change, frame and attach events as they arrive." },
        { "Log demo", "log", &ExamplePlugin::m_ShowLog, true, 3,
          "Writes lines to the POE2Fixer log at each level." },
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
    "1.0.0",
    "POEFixer",
    "Reference plugin for developers: live demos of every SDK service, from entities, buffs and inventories to the game UI tree, memory and prices.")
