// Plugins/ExamplePlugin/examples/Text.h
// How the example's texts follow the program language (PluginI18n.h, included by PluginSDK.h).
//
//   * Every text a user reads is a key of lang/en.json (lang/ru.json, ... next to the DLL):
//         ImGui::TextUnformatted(I18n::Tr("buffs_none"));                   // a text
//         if (ImGui::Button(I18n::TrId("events_reset"))) ...                // "<text>###<key>"
//         Show(I18n::TrF("buffs_active", { { "count", Num("%d", n) } }));   // "Active buffs: {count}"
//     Widgets whose label is their ImGui id (Button, Checkbox, TreeNode, CollapsingHeader,
//     BeginTabItem, Selectable, Combo, InputText, TableSetupColumn, ...) take TrId, so the id
//     stays the key's in every language. A label with a per-item id appends "###" itself:
//         std::string(I18n::Tr("entities_effects")) + "###bf" + std::to_string(id)
//   * A text is never a printf format: Num formats numbers here, in code, into named TrF
//     arguments (exactly the text's {placeholders}); the translation decides where they go.
//   * Nothing translated is kept in a static: a static table holds keys, written
//     I18n::Key("panel_buffs") so tools/plugin_i18n/gen.py --gate checks them, looked up per frame.
//   * Layout is code: no leading, trailing or doubled spaces in a text (two values on one line
//     are two keys and ImGui::SameLine), no pixel width sized for the English text (width 0 for
//     a fixed table column, em for a field), and a localized text is cut by characters (Shorten).
//   * English on purpose: SDK names (services, components, fields shown under their member
//     name, functions, enum values, pattern names), the game's own strings (item, skill, buff,
//     area names), log lines and the clipboard's debug dump.
//   * After editing lang/en.json run python tools/plugin_i18n/gen.py Plugins/ExamplePlugin
//     (it writes lang_en.generated.h, included once by ExamplePlugin.cpp).
#pragma once
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"

#include <sal.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace Examples {

namespace I18n = PluginSDK::I18n;

// printf for a TrF argument: Num("%.1f", seconds). The format is code, never a translation.
// _Printf_format_string_ makes MSVC's code analysis (/analyze) check every call's arguments
// against the format, as for printf itself.
inline std::string Num(_In_z_ _Printf_format_string_ const char* format, ...) {
    char buf[128];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}

// At most `maxChars` characters of the UTF-8 `text`: a longer one keeps its first maxChars - 3
// and ends with "...". Counted in characters, never bytes: a game's UI strings are localized, and
// a byte count would cut a Russian or Thai character in half.
inline std::string Shorten(const std::string& text, size_t maxChars) {
    const auto starts = [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; };
    size_t count = 0;
    for (char c : text) count += starts(c) ? 1 : 0;
    if (count <= maxChars) return text;
    const size_t keep = maxChars > 3 ? maxChars - 3 : 0;
    size_t chars = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (!starts(text[i])) continue;
        if (chars == keep) return text.substr(0, i) + "...";
        ++chars;
    }
    return text;
}

// One line of a text TrF built (TextUnformatted: a translation is no format).
inline void Show(const std::string& text) { ImGui::TextUnformatted(text.c_str()); }

inline const char* YesNo(bool value) { return value ? I18n::Tr("common_yes") : I18n::Tr("common_no"); }
inline const char* TrueFalse(bool value) { return value ? I18n::Tr("common_true") : I18n::Tr("common_false"); }

} // namespace Examples
