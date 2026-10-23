# ExamplePlugin — POE2Fixer Plugin SDK Example

Current version: 1.0.3

Requires POE2Fixer 366 or newer.

A reference plugin demonstrating all features of the POE2Fixer Plugin SDK (v6). Use this as a starting point for building your own plugins.

## What It Does

ExamplePlugin provides interactive demo tabs that showcase every SDK capability. When enabled in POE2Fixer, it adds a settings panel with the following example tabs:

### Buff Inspector
Displays the player's active buffs in real-time. Shows buff name, remaining time, charges, flask slot, and effectiveness. Includes text filtering and color-coded progress bars for buff duration.

### Entity Explorer
Lists all game entities (monsters, NPCs, chests, players, items) with their type, rarity, and nearby zone classification. Supports the entity watch mechanism — select any entity to inspect its full component data (Life, Render, Positioned, Targetable, Animated, Stats, Actor, Buffs) in a detailed tree view.

### Inventory Inspector
Accesses the ServerData debug interface to browse player inventories. Displays inventory slot grids with item details: base type, unique name, rarity, item level, mods (implicit, explicit, enchant, hellscape), identified/corrupted status, and stack sizes.

### Memory Viewer
Demonstrates direct game memory reading through the SDK. Includes a hex dump viewer at arbitrary addresses, typed `Read<T>` examples, pattern scan address resolution, and StdVector/StdList/StdMap container reading.

### UI Explorer
Navigates the game's full UI element tree. Shows element properties (position, size, flags, type, scale, StringId, text content), recursive parent chain, and child traversal. Includes search by StringId and visual highlighting of selected elements on screen.

### Component Reader (SDK v6)
Demonstrates the SDK v6 Component Reader API and UI Element API. Exercises all 22 component readers, 4 enumerators, and 10 convenience helpers directly from entity addresses without hardcoded offsets. Shows UI tree navigation using `GetUiChildren`, `ReadUiElement`, and `ComputeUiScreenRect`. Demonstrates helpers like `GetHealthPercent`, `IsAlive`, `GetPlayerName`, `GetItemRarity`, `IsItemIdentified`, `GetStackCount`.

### Render, Terrain, Events, Log tabs
Exercise the remaining service surface: WorldToScreen + map projection, walkable/height grid + TgtLocation enumeration, the area's generated rooms (`.arm` path, grid centre and distance, filtered with the Radar's room pattern rule), OnFrame/OnAreaChange/OnGameAttached/OnGameDetached event hooks with live counters, and Log.Debug/Info/Warn/Error fire buttons. The Area & Vitals tab lists the current map's area modifiers with their Stats.dat Ids.

### SDK Coverage Summary
The settings tab shows a `SDK Coverage: N/10 services responding` banner above the tab bar. Hover for per-service status (Game, Entities, Components, Inventory, Ui, Render, Terrain, Memory, Log, Events).

## Settings

The settings tab is the reference layout for plugin settings, built with the plugin UI
kit `POEFixer/plugin_sdk/PluginUi.h`: one card per section, one measured label column
per block, a wrapped tooltip on every non-obvious control, dependent controls dimmed
rather than hidden. It has the info-window switch, overlay mode, window opacity, a
toggle hotkey (a key, or the right, middle or a side mouse button; it shows or hides the info window in game)
and one switch per demo tab.

`tools/plugin_ui_preview` renders a plugin's settings tab to PNG at several widths and
font scales without the game (see its README).

## Informational windows (PluginInfoWindow.h)

The info window is drawn through `PluginSDK::InfoWindow` (`POEFixer/plugin_sdk/PluginInfoWindow.h`)
instead of `ImGui::Begin` / `End`. With POE2Fixer 366 or later the host gives such a window a gear
menu (Lock window, Game overlay mode, Reset position, Close) and remembers its position (separately
in and out of the game), size and collapsed state; with Game overlay mode off the window is drawn
outside the game, anywhere on the screens, in another ImGui context. On an older host it is a plain
window at `legacyPos` / `defaultPos`. Rules:

- Never `SetNextWindowPos` / `SetNextWindowCollapsed` for it (`SetNextWindowSize` only for an `AlwaysAutoResize` or `NoResize` window). `legacyPos` / `legacyCollapsed` seed the host's store once from a position the plugin saved before.
- Push window-level style and call `SetNextWindowBgAlpha` / `SetNextWindowSizeConstraints` before it; pop only after it ended (wrap it in its own `{ }` block).
- Inside it draw only into the window: no `GetForegroundDrawList`. Draw over the game after the window ended (the UI Explorer's highlights do). `Detached()` tells whether it is out of the game: an in-game hover test or `Overlay.SetWantsOverlayInput` applies only in game.
- Close clears the flag passed as `open`; save it then like any setting.

```cpp
PluginSDK::InfoWindowOptions o;
o.id = "main";                      // stable, 1-48 of [A-Za-z0-9_.-]
o.title = "My window";              // nullptr with ImGuiWindowFlags_NoTitleBar
o.defaultPos = ImVec2(60, 60);
o.defaultSize = ImVec2(480, 420);   // resizable windows only
ImGui::SetNextWindowBgAlpha(m_alpha);
{
    PluginSDK::InfoWindow window(ctx()->Overlay, o, &m_showWindow);   // Close clears m_showWindow
    if (window) ImGui::Text("...");
}                                   // the window ends here
if (!m_showWindow) SaveSettings();
```

## Creating Your Own Plugin

1. Copy `Plugins/ExamplePlugin/` to `Plugins/YourPlugin/` (keep the `POEFixer/` folder next to `Plugins/`: the project compiles against `POEFixer/plugin_sdk` and `POEFixer/imgui` by relative path)
2. Rename the .vcxproj and main .cpp, give the project a new `ProjectGuid`, and add it to a solution (or copy `ExamplePlugin.sln` and point it at the new project)
3. Subclass `PluginSDK::Plugin` and override the lifecycle hooks (see `ExamplePlugin.cpp`)
4. Describe it once with `POEFIXER_PLUGIN_INFO(...)` after including `imgui.h` (id = folder and DLL name, display name, version `X.Y.Z`, author, one short description; see the end of `ExamplePlugin.cpp`). POE2Fixer refuses a DLL without it
5. Use `ctx()->Service.Method(...)` to access game data (Game, Entities, Components, Inventory, Ui, Render, Terrain, Memory, Log, Events, Overlay, Flasks, Prices, Runeshape, Atlas, Sekhema)
6. Start `OnEnable()`, `DrawSettings()` and `DrawUI()` with `ImGui::SetCurrentContext(ctx()->ImGuiContext)` (`BindImGui()` in `ExamplePlugin.cpp`). With Settings -> "Separate menu window" `DrawSettings()` runs in the menu window's ImGui context and `DrawUI()` in the in-game overlay's, each with its own frame count, time, display size and windows: never compare `ImGui::GetFrameCount()`, ids or windows between the two, and in `DrawSettings()` take the game's size from `ctx()->Game.GetScreenSize()`, not `io.DisplaySize`
7. Lay out `DrawSettings()` with `PluginUi.h` (cards, label columns, tips)
8. Build and copy the DLL to `Plugins/YourPlugin/` next to the POEFixer executable

## Install

Install it from POE2Fixer → Plugins → Plugin Manager (one click, updates included), or download `ExamplePlugin.zip` from [Releases](https://github.com/POEFixer/ExamplePlugin/releases/latest) and extract it into the POE2Fixer folder (it creates `Plugins\ExamplePlugin\`). Then enable the plugin in the Plugins tab.

## Build from source

This repository is the plugin folder from the POE2Fixer source tree (`Plugins/ExamplePlugin/`) together with the host files it compiles against, kept at the same relative paths:

- `POEFixer/plugin_sdk/` - the POE2Fixer Plugin SDK v6 headers (`PluginAbi.h`, `PluginSDK.h`)
- `POEFixer/imgui/` - the Dear ImGui sources the DLL compiles against

1. Install Visual Studio 2022 with the "Desktop development with C++" workload (MSVC v143, Windows 10/11 SDK).
2. Open `ExamplePlugin.sln`, select **Release | x64** and build the solution.
3. The plugin is written to `x64\Release\Plugins\ExamplePlugin\ExamplePlugin.dll` (Release builds carry no debug information, see `Directory.Build.targets`).
4. Copy `ExamplePlugin.dll` into `Plugins\ExamplePlugin\` next to the POE2Fixer executable.
