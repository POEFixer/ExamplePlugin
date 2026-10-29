// Plugins/ExamplePlugin/examples/ExampleLog.h
// Demonstrates every LogService method.

#pragma once
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "SdkStatus.h"
#include "Text.h"

namespace Examples {

// The buttons name LogService calls and the log lines stay English (SDK names, log).
inline void DrawLogPanel(const PluginSDK::Context* ctx, int& sentCount) {
    if (!ctx) { StatusBadge(StatusLevel::Fail, I18n::Tr("common_context_missing")); return; }

    StatusBadge(StatusLevel::Ok, I18n::Tr("log_always_available"));
    ImGui::Separator();

    ImGui::TextWrapped("%s", I18n::Tr("log_intro"));
    ImGui::Spacing();

    if (ImGui::Button("Log.Debug")) {
        ctx->Log.Debug("ExamplePlugin: Debug test message");
        ++sentCount;
    }
    ImGui::SameLine();
    if (ImGui::Button("Log.Info")) {
        ctx->Log.Info("ExamplePlugin: Info test message");
        ++sentCount;
    }
    ImGui::SameLine();
    if (ImGui::Button("Log.Warn")) {
        ctx->Log.Warn("ExamplePlugin: Warn test message");
        ++sentCount;
    }
    ImGui::SameLine();
    if (ImGui::Button("Log.Error")) {
        ctx->Log.Error("ExamplePlugin: Error test message");
        ++sentCount;
    }
    ImGui::Separator();

    if (ImGui::Button("Log.Log(\"Info\", ...)")) {
        ctx->Log.Log("Info", "ExamplePlugin: direct Log() call with explicit level");
        ++sentCount;
    }

    ImGui::Spacing();
    Show(I18n::TrF("log_total_fired", { { "count", Num("%d", sentCount) } }));
}

} // namespace Examples
