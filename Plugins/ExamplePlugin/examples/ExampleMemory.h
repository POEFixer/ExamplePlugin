#pragma once
// ============================================================================
// ExampleMemory.h — v6 SDK
// ============================================================================
// Demonstrates the v6 MemoryService: base address, module size, pattern
// scanning, raw memory read, and the new RAII string/wstring read paths.
// ============================================================================

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "Text.h"
#include <vector>
#include <cstdio>

namespace Examples {

inline void DrawMemoryPanel(const PluginSDK::Context* ctx) {
    if (!ctx) return;

    uintptr_t base = ctx->Memory.GetBaseAddress();
    uintptr_t modSize = ctx->Memory.GetModuleSize();

    Show(I18n::TrF("memory_base_address", { { "address", Num("0x%llX", static_cast<unsigned long long>(base)) } }));
    Show(I18n::TrF("memory_module_size", { { "size", Num("0x%llX", static_cast<unsigned long long>(modSize)) },
                                           { "kb", Num("%llu", static_cast<unsigned long long>(modSize / 1024)) } }));
    Show(I18n::TrF("memory_pid", { { "pid", Num("%u", ctx->Game.GetProcessId()) } }));

    // --- PE Header Verification ---
    if (base != 0) {
        uint16_t dosSignature = 0;
        ctx->Memory.Read(base, &dosSignature, sizeof(dosSignature));
        Show(I18n::TrF("memory_dos_signature",
                       { { "value", Num("0x%04X", dosSignature) },
                         { "verdict", dosSignature == 0x5A4D ? I18n::Tr("memory_dos_valid")
                                                             : I18n::Tr("common_unknown") } }));

        uint32_t peOffset = 0;
        ctx->Memory.Read(base + 0x3C, &peOffset, sizeof(peOffset));
        if (peOffset > 0 && peOffset < modSize) {
            uint32_t peSignature = 0;
            ctx->Memory.Read(base + peOffset, &peSignature, sizeof(peSignature));
            Show(I18n::TrF("memory_pe_signature",
                           { { "value", Num("0x%08X", peSignature) },
                             { "verdict", peSignature == 0x00004550 ? I18n::Tr("memory_pe_valid")
                                                                    : I18n::Tr("memory_invalid") } }));

            uint16_t machine = 0;
            ctx->Memory.Read(base + peOffset + 4, &machine, sizeof(machine));
            Show(I18n::TrF("memory_machine",
                           { { "value", Num("0x%04X", machine) },
                             { "name", machine == 0x8664 ? "x64"
                                       : machine == 0x14C ? "x86" : I18n::Tr("memory_other") } }));
        }
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader(I18n::TrId("memory_patterns"), ImGuiTreeNodeFlags_DefaultOpen)) {
        // The host's pattern names (GetPatternAddress arguments): never translated.
        const char* patterns[] = {
            "Game States", "File Root", "AreaChangeCounter",
            "Terrain Rotator Helper", "Terrain Rotation Selector", "GameCullSize"
        };
        if (ImGui::BeginTable("PatternTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn(I18n::TrId("memory_col_pattern"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(I18n::TrId("common_address"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableHeadersRow();

            for (auto* name : patterns) {
                uintptr_t addr = ctx->Memory.GetPatternAddress(name);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", name);
                ImGui::TableNextColumn();
                if (addr != 0) {
                    char buf[20]; snprintf(buf, sizeof(buf), "0x%llX",
                                           static_cast<unsigned long long>(addr));
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", buf);
                    if (ImGui::IsItemHovered() && ImGui::IsItemClicked())
                        ImGui::SetClipboardText(buf);
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", I18n::Tr("memory_not_found"));
                }
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader(I18n::TrId("memory_hex_viewer"))) {
        static char addrInput[20] = "";
        static uintptr_t viewAddr = 0;
        static int viewSize = 256;
        ImGui::InputText(I18n::TrId("memory_address_hex"), addrInput, sizeof(addrInput));
        ImGui::SameLine();
        if (ImGui::Button(I18n::TrId("memory_go"))) {
            viewAddr = strtoull(addrInput, nullptr, 16);
        }
        ImGui::SameLine();
        if (ImGui::Button(I18n::TrId("memory_base"))) {
            viewAddr = base;
            snprintf(addrInput, sizeof(addrInput), "%llX",
                     static_cast<unsigned long long>(base));
        }
        ImGui::SliderInt(I18n::TrId("memory_bytes"), &viewSize, 16, 512);

        if (viewAddr != 0) {
            std::vector<uint8_t> data(viewSize);
            if (ctx->Memory.Read(viewAddr, data.data(), viewSize)) {
                for (int i = 0; i < viewSize; i += 16) {
                    ImGui::Text("%llX: ", static_cast<unsigned long long>(viewAddr + i));
                    ImGui::SameLine();
                    for (int j = 0; j < 16 && (i + j) < viewSize; j++) {
                        if (j == 8) { ImGui::SameLine(); ImGui::TextDisabled("|"); }
                        ImGui::SameLine();
                        ImGui::Text("%02X", data[i + j]);
                    }
                    ImGui::SameLine(0.0f, ImGui::GetFontSize());   // gaps scale with the font, never pixels
                    char ascii[17] = {};
                    for (int j = 0; j < 16 && (i + j) < viewSize; j++) {
                        uint8_t c = data[i + j];
                        ascii[j] = (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
                    }
                    ImGui::TextDisabled("%s", ascii);
                }
            } else {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s",
                    I18n::TrF("memory_read_failed",
                              { { "address", Num("0x%llX", static_cast<unsigned long long>(viewAddr)) } }).c_str());
            }
        }
    }

    // The header and the call list name the SDK (English); the sentence is translated.
    if (ImGui::CollapsingHeader("MemoryService API")) {
        ImGui::TextWrapped("%s", I18n::Tr("memory_api_intro"));
        ImGui::TextUnformatted(
            "  ctx->Memory.Read(addr, buf, size)\n"
            "  ctx->Memory.ReadString(addr)           - null-terminated ASCII\n"
            "  ctx->Memory.ReadWString(addr)          - null-terminated wide\n"
            "  ctx->Memory.ReadStdWString(addr)       - StdWString container\n"
            "  ctx->Memory.ReadStdVector(addr, sz, n) - StdVector<T> raw bytes\n"
            "  ctx->Memory.GetBaseAddress()\n"
            "  ctx->Memory.GetModuleSize()\n"
            "  ctx->Memory.GetPatternAddress(name)\n");
    }
}

} // namespace Examples
