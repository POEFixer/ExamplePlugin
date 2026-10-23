# Changelog

## 1.0.3

- Works with the separate menu window (POEFixer 366): the host hands the plugin the right drawing context for its settings tab and for its overlay.
- The example sets its drawing context at every entry point (OnEnable, DrawSettings, DrawUI), and the README explains why: with a separate menu window the settings tab and the overlay are drawn in two different ImGui contexts.
- Its window now has the POEFixer gear menu (POEFixer 366): lock it, take it out of the game onto any screen, reset its position or close it; its place and size are remembered separately in and out of the game.
- Shows the informational window helper (PluginInfoWindow.h) in the code and in a new README section; closing the window now saves "Show the info window", and the UI Explorer draws its on-game highlights after the window ended.

## 1.0.2

- Rebuilt with the updated plugin SDK headers (POEFixer 365 adds optional calls for UI control names, stash objects and tabs, wider entity scans and the host automation flag). No change in what the plugin does.

## 1.0.1

- The "?" next to the Demo tabs title now sits in its own gap instead of on the section's line (Heading takes the help text).

## 1.0.0

- First versioned release.
- Settings rebuilt with the shared plugin UI kit (PluginUi.h): cards, measured label columns and tooltips.
