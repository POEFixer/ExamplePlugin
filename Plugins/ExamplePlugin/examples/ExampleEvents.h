// Plugins/ExamplePlugin/examples/ExampleEvents.h
// Demonstrates every EventsService method.
//
// Owned state (counters + tokens) lives in EventsDemoState; the plugin owns
// one instance and passes it by ref to the draw function. The lifetime of
// the subscriptions follows the lifetime of the state (cleaned up via
// ~EventsService when the plugin disables).

#pragma once
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "SdkStatus.h"
#include "Text.h"
#include <atomic>

namespace Examples {

struct EventsDemoState {
    std::atomic<int> frameCount{0};
    std::atomic<int> areaChangeCount{0};
    std::atomic<int> gameAttachedCount{0};
    std::atomic<int> gameDetachedCount{0};
    PluginSDK::EventsService::Token frameTok{};
    PluginSDK::EventsService::Token areaTok{};
    PluginSDK::EventsService::Token attachedTok{};
    PluginSDK::EventsService::Token detachedTok{};
    bool subscribed = false;
};

inline void SubscribeAll(const PluginSDK::Context* ctx, EventsDemoState& s) {
    if (s.subscribed || !ctx) return;
    // ctx() returns const Context*; service mutation goes through const_cast,
    // identical pattern to what the Radar plugin uses.
    auto& events = const_cast<PluginSDK::EventsService&>(ctx->Events);
    s.frameTok    = events.OnFrame       ([&s]{ s.frameCount.fetch_add(1); });
    s.areaTok     = events.OnAreaChange  ([&s]{ s.areaChangeCount.fetch_add(1); });
    s.attachedTok = events.OnGameAttached([&s]{ s.gameAttachedCount.fetch_add(1); });
    s.detachedTok = events.OnGameDetached([&s]{ s.gameDetachedCount.fetch_add(1); });
    s.subscribed = true;
}

inline void UnsubscribeAll(const PluginSDK::Context* ctx, EventsDemoState& s) {
    if (!s.subscribed || !ctx) return;
    auto& events = const_cast<PluginSDK::EventsService&>(ctx->Events);
    events.Unsubscribe(s.frameTok);
    events.Unsubscribe(s.areaTok);
    events.Unsubscribe(s.attachedTok);
    events.Unsubscribe(s.detachedTok);
    s.frameTok = {};
    s.areaTok = {};
    s.attachedTok = {};
    s.detachedTok = {};
    s.subscribed = false;
}

inline void DrawEventsPanel(const PluginSDK::Context* ctx, EventsDemoState& s) {
    if (!ctx) { StatusBadge(StatusLevel::Fail, I18n::Tr("common_context_missing")); return; }

    if (s.subscribed) StatusBadge(StatusLevel::Ok, I18n::TrF("events_subscribed", { { "count", "4" } }).c_str());
    else              StatusBadge(StatusLevel::Warn, I18n::Tr("events_not_subscribed"));
    ImGui::Separator();

    if (!s.subscribed) {
        if (ImGui::Button(I18n::TrId("events_subscribe"))) SubscribeAll(ctx, s);
    } else {
        if (ImGui::Button(I18n::TrId("events_unsubscribe"))) UnsubscribeAll(ctx, s);
    }

    // The event names are the EventsService methods' (SDK names).
    ImGui::Separator();
    Show(I18n::TrF("events_fired", { { "event", "OnFrame" }, { "count", Num("%d", s.frameCount.load()) } }));
    Show(I18n::TrF("events_fired", { { "event", "OnAreaChange" }, { "count", Num("%d", s.areaChangeCount.load()) } }));
    Show(I18n::TrF("events_fired", { { "event", "OnGameAttached" }, { "count", Num("%d", s.gameAttachedCount.load()) } }));
    Show(I18n::TrF("events_fired", { { "event", "OnGameDetached" }, { "count", Num("%d", s.gameDetachedCount.load()) } }));

    if (ImGui::Button(I18n::TrId("events_reset"))) {
        s.frameCount = 0;
        s.areaChangeCount = 0;
        s.gameAttachedCount = 0;
        s.gameDetachedCount = 0;
    }
}

} // namespace Examples
