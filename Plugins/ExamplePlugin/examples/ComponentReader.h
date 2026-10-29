#pragma once
// ============================================================================
// ComponentReader.h — v6 SDK
// ============================================================================
// Demonstrates calling ctx->Components.ReadXxx() on the addresses already
// available in Snapshot::Entities[i].Components. Replaces the v5 demo that
// went through ComponentCache.HasLife() / ReadLifeComponent() etc.
// ============================================================================

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "SdkStatus.h"
#include "Text.h"
#include <algorithm>
#include <string>

namespace Examples {

inline void ShowComponentReaderDemo(const PluginSDK::Context* ctx,
                                    const PluginSDK::Snapshot& snapshot) {
    if (!ctx) return;
    if (snapshot.State != PluginSDK::GameState::InGame) {
        ImGui::TextDisabled("%s", I18n::Tr("common_not_in_game"));
        return;
    }

    ImGui::TextUnformatted(I18n::Tr("components_title"));
    ImGui::Separator();

    if (ImGui::CollapsingHeader(I18n::TrId("components_entity_health"))) {
        int shown = 0;
        for (const auto& entity : snapshot.Entities) {
            if (!entity.Components.HasLife()) continue;

            auto life = ctx->Components.ReadLife(entity.Components.Life);
            if (!life.Valid) continue;

            float hpPct = ctx->Components.GetHealthPercent(entity.Components.Life);
            Show(I18n::TrF("components_entity_hp", { { "id", Num("%u", entity.Id) },
                                                     { "current", Num("%d", life.Health.Current) },
                                                     { "max", Num("%d", life.Health.Total) },
                                                     { "percent", Num("%.0f", hpPct) } }));

            if (++shown >= 20) {
                ImGui::TextDisabled("%s", I18n::TrF("components_more_with",
                    { { "count", Num("%d", static_cast<int>(snapshot.Entities.size()) - shown) },
                      { "component", "Life" } }).c_str());
                break;
            }
        }
        if (shown == 0) ImGui::TextDisabled("%s", I18n::Tr("components_no_life"));
    }

    if (ImGui::CollapsingHeader(I18n::TrId("components_entity_positions"))) {
        int shown = 0;
        for (const auto& entity : snapshot.Entities) {
            if (!entity.Components.HasRender()) continue;

            auto render = ctx->Components.ReadRender(entity.Components.Render);
            if (!render.Valid) continue;

            Show(I18n::TrF("components_entity_world", { { "id", Num("%u", entity.Id) },
                                                        { "x", Num("%.1f", render.WorldX) },
                                                        { "y", Num("%.1f", render.WorldY) },
                                                        { "z", Num("%.1f", render.WorldZ) } }));

            if (++shown >= 20) {
                ImGui::TextDisabled("%s", I18n::TrF("components_more_with",
                    { { "count", Num("%d", static_cast<int>(snapshot.Entities.size()) - shown) },
                      { "component", "Render" } }).c_str());
                break;
            }
        }
        if (shown == 0) ImGui::TextDisabled("%s", I18n::Tr("components_no_render"));
    }

    if (ImGui::CollapsingHeader(I18n::TrId("components_ground_items"))) {
        // Ground items are WorldItem container entities. The inner item
        // entity (with Mods/Base/Stack/Sockets components) is reached via
        // ctx->Entities.GetWorldItemInner(containerAddr).

        int total = 0;
        int withMods = 0;
        for (const auto& e : snapshot.Entities) {
            if (e.EntityType != PluginSDK::EntityType::Item) continue;
            ++total;
            auto inner = ctx->Entities.GetWorldItemInner(e.Address);
            if (inner && inner->Components.HasMods()) ++withMods;
        }
        Show(I18n::TrF("components_world_items", { { "total", Num("%d", total) },
                                                   { "mods", Num("%d", withMods) } }));
        if (total == 0) {
            ImGui::TextDisabled("%s", I18n::Tr("components_walk_near_item"));
        } else {
            ImGui::Separator();
            int shown = 0;
            for (const auto& entity : snapshot.Entities) {
                if (entity.EntityType != PluginSDK::EntityType::Item) continue;

                auto inner = ctx->Entities.GetWorldItemInner(entity.Address);
                if (!inner) {
                    ImGui::TextDisabled("id=%u", entity.Id);   // a field dump (code), then the note (a text)
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", I18n::Tr("components_inner_unresolved"));
                    if (++shown >= 20) break;
                    continue;
                }

                // Truncate inner path to last 2 segments for readability.
                std::string narrowPath;
                narrowPath.reserve(inner->Path.size());
                for (wchar_t c : inner->Path)
                    narrowPath += (c < 128) ? static_cast<char>(c) : '?';
                size_t lastSlash = narrowPath.rfind('/');
                size_t prevSlash = (lastSlash != std::string::npos && lastSlash > 0)
                                       ? narrowPath.rfind('/', lastSlash - 1)
                                       : std::string::npos;
                std::string shortPath = (prevSlash != std::string::npos)
                                            ? narrowPath.substr(prevSlash + 1)
                                            : narrowPath;

                // Rarity colour: 0=Normal (white), 1=Magic (blue),
                // 2=Rare (yellow), 3=Unique (orange).
                int rarity = 0;
                int itemLevel = 0;
                bool isIdentified = false;
                bool isCorrupted = false;
                int implicitCount = 0;
                int explicitCount = 0;
                if (inner->Components.HasMods()) {
                    auto mods = ctx->Components.ReadMods(inner->Components.Mods);
                    if (mods.Valid) {
                        rarity        = mods.Rarity;
                        itemLevel     = mods.ItemLevel;
                        isIdentified  = mods.IsIdentified;
                        isCorrupted   = mods.IsCorrupted;
                    }
                    // InventoryService::ReadItemMods returns a default-constructed
                    // ItemMods on failure; empty vectors mean no mods.
                    PluginSDK::ItemMods modList =
                        ctx->Inventory.ReadItemMods(entity.Address);
                    implicitCount = static_cast<int>(modList.ImplicitMods.size());
                    explicitCount = static_cast<int>(modList.ExplicitMods.size());
                }

                // Item names — both auto-resolve WorldItem containers (Task 4),
                // so passing the container address directly works. They are the
                // game's own names: shown as the game gives them.
                std::string baseTypeName = ctx->Inventory.ReadItemBaseTypeName(entity.Address);
                std::string uniqueName   = ctx->Inventory.ReadItemUniqueName(entity.Address);

                ImVec4 colour =
                    rarity == 1 ? ImVec4(0.55f, 0.65f, 1.00f, 1.0f)  // magic
                  : rarity == 2 ? ImVec4(1.00f, 0.95f, 0.45f, 1.0f)  // rare
                  : rarity == 3 ? ImVec4(1.00f, 0.60f, 0.20f, 1.0f)  // unique
                  :               ImVec4(0.95f, 0.95f, 0.95f, 1.0f); // normal
                ImGui::PushStyleColor(ImGuiCol_Text, colour);
                // Line 1: rarity-coloured display name.
                //   Unique items get "Unique Name (Base Type)".
                //   Non-uniques just show the base type name.
                //   Fallback to truncated metadata path if names are empty
                //   (e.g. very early mid-spawn before names are streamed in).
                if (!uniqueName.empty()) {
                    ImGui::Text("%s %s", uniqueName.c_str(), baseTypeName.c_str());
                } else if (!baseTypeName.empty()) {
                    ImGui::TextUnformatted(baseTypeName.c_str());
                } else {
                    ImGui::TextUnformatted(shortPath.c_str());
                }
                ImGui::PopStyleColor();
                // Line 2: indented diagnostic row (uncoloured): a field dump, as in code.
                ImGui::TextDisabled(
                    "    id=%u  iLvl=%d  rarity=%d  ident=%s  corr=%s  +%d/%dM",
                    entity.Id, itemLevel, rarity,
                    isIdentified ? "y" : "n",
                    isCorrupted  ? "y" : "n",
                    implicitCount, explicitCount);

                if (++shown >= 20) {
                    ImGui::TextDisabled("%s",
                        I18n::TrF("components_more_items", { { "count", Num("%d", total - shown) } }).c_str());
                    break;
                }
            }
        }
    }

    if (ImGui::CollapsingHeader(I18n::TrId("components_ground_effects"))) {
        // GroundEffect lives on VisibleServerGroundEffect entities, which all
        // share the same metadata path — TypeId is what tells the variants apart
        // (ShockedGround / IgnitedGround / CausticCloud / ChilledGround / ...).
        // NOTE: ReadGroundEffect takes the ENTITY address, NOT a component
        // address — the host resolves the "GroundEffect" component itself (it is
        // not part of Entity::Components, so there is no Components.GroundEffect).
        // Radius is in world units (0 = unset by that variant); to draw the area
        // on the ground, project a ring with ctx->Render.WorldToScreen (see
        // ExampleRender).
        int shown = 0;
        for (const auto& entity : snapshot.Entities) {
            if (entity.Path.find(L"VisibleServerGroundEffect") == std::wstring::npos)
                continue;

            auto ge = ctx->Components.ReadGroundEffect(entity.Address);
            if (!ge.Valid) continue;

            Show(I18n::TrF("components_ground_effect", {
                { "id", Num("%u", entity.Id) },
                { "type", ge.TypeId.empty() ? std::string_view(I18n::Tr("components_type_unknown")) : std::string_view(ge.TypeId) } }));
            ImGui::SameLine();
            Show(I18n::TrF("components_ground_effect_radius", { { "radius", Num("%.0f", ge.Radius) } }));
            if (!ge.BuffVisual1.empty() || !ge.AoFile.empty() || !ge.EndEffect.empty()) {
                ImGui::TextDisabled("    visual=%s  ao=%s  end=%s",
                    ge.BuffVisual1.empty() ? "-" : ge.BuffVisual1.c_str(),
                    ge.AoFile.empty()      ? "-" : ge.AoFile.c_str(),
                    ge.EndEffect.empty()   ? "-" : ge.EndEffect.c_str());
            }

            if (++shown >= 20) {
                ImGui::TextDisabled("%s", I18n::Tr("components_more_ground_effects"));
                break;
            }
        }
        if (shown == 0)
            ImGui::TextDisabled("%s", I18n::Tr("components_no_ground_effects"));
    }

    if (ImGui::CollapsingHeader(I18n::TrId("components_ui_navigation"))) {
        uintptr_t gameUi = ctx->Ui.GetGameUiRoot();
        if (gameUi != 0) {
            auto children = ctx->Ui.GetChildren(gameUi);
            Show(I18n::TrF("components_ui_root_children",
                           { { "count", Num("%d", static_cast<int>(children.size())) } }));

            int limit = (static_cast<int>(children.size()) < 10) ? static_cast<int>(children.size()) : 10;
            for (int i = 0; i < limit; i++) {
                auto elem = ctx->Ui.Read(children[i]);
                if (!elem.Valid) continue;
                std::string sid = ctx->Ui.GetStringId(children[i]);

                // A field dump, as in code.
                ImGui::Text("  [%d] Type=0x%04X Visible=%s StringId=\"%s\"",
                    i, elem.ElementType,
                    elem.IsVisible ? "yes" : "no",
                    sid.c_str());
            }
            if (static_cast<int>(children.size()) > 10) {
                ImGui::Indent();   // under the list: the indent is code, never spaces in the value
                ImGui::TextDisabled("%s", I18n::TrF("components_more_children",
                    { { "count", Num("%d", static_cast<int>(children.size()) - 10) } }).c_str());
                ImGui::Unindent();
            }
        } else {
            ImGui::TextDisabled("%s", I18n::Tr("components_ui_root_missing"));
        }
    }

    if (ImGui::CollapsingHeader(I18n::TrId("components_all_readers"))) {
        const auto& player = snapshot.Player;

        auto reportComponent = [&ctx](const char* name, uintptr_t addr,
                                        bool (*tryRead)(const PluginSDK::Context*,
                                                         uintptr_t)) {
            if (addr == 0) {
                Examples::StatusBadge(Examples::StatusLevel::Warn,
                    I18n::TrF("components_reader_unset", { { "component", name } }).c_str());
                return;
            }
            if (tryRead(ctx, addr))
                Examples::StatusBadge(Examples::StatusLevel::Ok,
                    I18n::TrF("components_reader_valid", { { "component", name } }).c_str());
            else
                Examples::StatusBadge(Examples::StatusLevel::Fail,
                    I18n::TrF("components_reader_invalid", { { "component", name } }).c_str());
        };

        reportComponent("Life",        player.Components.Life,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadLife(a).Valid; });
        reportComponent("Render",      player.Components.Render,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadRender(a).Valid; });
        reportComponent("Positioned",  player.Components.Positioned,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadPositioned(a).Valid; });
        reportComponent("Targetable",  player.Components.Targetable,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadTargetable(a).Valid; });
        reportComponent("Chest",       player.Components.Chest,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadChest(a).Valid; });
        reportComponent("Shrine",      player.Components.Shrine,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadShrine(a).Valid; });
        reportComponent("Stack",       player.Components.Stack,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadStack(a).Valid; });
        reportComponent("Charges",     player.Components.Charges,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadCharges(a).Valid; });
        reportComponent("Player",      player.Components.Player,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadPlayer(a).Valid; });
        reportComponent("Animated",    player.Components.Animated,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadAnimated(a).Valid; });
        reportComponent("Transitionable", player.Components.Transitionable,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadTransitionable(a).Valid; });
        reportComponent("TriggerableBlockage", player.Components.TriggerableBlockage,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadTriggerableBlockage(a).Valid; });
        reportComponent("MinimapIcon", player.Components.MinimapIcon,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadMinimapIcon(a).Valid; });
        reportComponent("StateMachine", player.Components.StateMachine,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadStateMachine(a).Valid; });
        reportComponent("Base",        player.Components.Base,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadBase(a).Valid; });
        reportComponent("Mods",        player.Components.Mods,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadMods(a).Valid; });
        reportComponent("Stats",       player.Components.Stats,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadStats(a).Valid; });
        reportComponent("Buffs",       player.Components.Buffs,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadBuffs(a).Valid; });
        reportComponent("Actor",       player.Components.Actor,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadActor(a).Valid; });
        reportComponent("Npc",         player.Components.Npc,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadNpc(a).Valid; });
        reportComponent("DiesAfterTime", player.Components.DiesAfterTime,
            [](const PluginSDK::Context* c, uintptr_t a){ return c->Components.ReadDiesAfterTime(a).Valid; });
    }

    if (ImGui::CollapsingHeader(I18n::TrId("components_enumerators"))) {
        const auto& player = snapshot.Player;

        if (player.Components.HasBuffs()) {
            auto buffs = ctx->Components.EnumerateBuffs(player.Components.Buffs);
            Show(I18n::TrF("components_enum_buffs", { { "count", Num("%zu", buffs.size()) } }));
            Examples::StatusBadge(Examples::StatusLevel::Ok,
                I18n::TrF("components_enum_returned", { { "function", "EnumerateBuffs" } }).c_str());
        } else {
            Examples::StatusBadge(Examples::StatusLevel::Warn,
                I18n::TrF("components_address_zero", { { "component", "Buffs" } }).c_str());
        }

        if (player.Components.HasActor()) {
            auto skills = ctx->Components.EnumerateActiveSkills(player.Components.Actor);
            Show(I18n::TrF("components_enum_skills", { { "count", Num("%zu", skills.size()) } }));
            Examples::StatusBadge(Examples::StatusLevel::Ok,
                I18n::TrF("components_enum_returned", { { "function", "EnumerateActiveSkills" } }).c_str());
        } else {
            Examples::StatusBadge(Examples::StatusLevel::Warn,
                I18n::TrF("components_address_zero", { { "component", "Actor" } }).c_str());
        }

        if (player.Components.HasStats()) {
            auto stats = ctx->Components.EnumerateStats(player.Components.Stats);
            Show(I18n::TrF("components_enum_stats", { { "count", Num("%zu", stats.size()) } }));
            Examples::StatusBadge(Examples::StatusLevel::Ok,
                I18n::TrF("components_enum_returned", { { "function", "EnumerateStats" } }).c_str());
        } else {
            Examples::StatusBadge(Examples::StatusLevel::Warn,
                I18n::TrF("components_address_zero", { { "component", "Stats" } }).c_str());
        }

        // Item mods enumeration requires an item entity; skipped here unless a
        // nearby item has Mods. The Item Mods (Nearby Items) header above covers it.
        ImGui::TextDisabled("%s", I18n::Tr("components_enum_item_mods"));

        // Monster mods live on monster entities (ObjectMagicProperties / OMP),
        // not the player. See the per-entity "Monster Mods" node in the Entities
        // tab for a live ctx->Components.EnumerateMonsterMods(OMP) demo.
        ImGui::TextDisabled("%s", I18n::Tr("components_enum_monster_mods"));
    }

    if (ImGui::CollapsingHeader(I18n::TrId("components_helpers"))) {
        const auto& player = snapshot.Player;
        if (player.Components.HasLife()) {
            Show(I18n::TrF("components_player_alive",
                           { { "value", YesNo(ctx->Components.IsAlive(player.Components.Life)) } }));
            Show(I18n::TrF("components_hp_percent",
                           { { "percent", Num("%.1f", ctx->Components.GetHealthPercent(player.Components.Life)) } }));
            Show(I18n::TrF("components_es_percent",
                           { { "percent", Num("%.1f", ctx->Components.GetEsPercent(player.Components.Life)) } }));
            Show(I18n::TrF("components_mp_percent",
                           { { "percent", Num("%.1f", ctx->Components.GetManaPercent(player.Components.Life)) } }));
        } else {
            ImGui::TextDisabled("%s", I18n::Tr("components_no_player_life"));
        }
        if (player.Components.HasPlayer()) {
            Show(I18n::TrF("components_player_name",
                           { { "name", ctx->Components.GetPlayerName(player.Components.Player) } }));
        }
        if (player.Components.HasRender()) {
            float wx, wy, wz;
            if (ctx->Components.GetWorldPosition(player.Components.Render, wx, wy, wz)) {
                Show(I18n::TrF("components_position", { { "x", Num("%.1f", wx) },
                                                        { "y", Num("%.1f", wy) },
                                                        { "z", Num("%.1f", wz) } }));
            }
        }
        if (player.Components.HasChest()) {
            Show(I18n::TrF("components_chest_opened",
                           { { "value", YesNo(ctx->Components.IsChestOpened(player.Components.Chest)) } }));
        }

        // Item helpers - only meaningful for items, not the player. Demonstrated
        // for the first nearby item entity that has Mods component.
        for (const auto& entity : snapshot.Entities) {
            if (!entity.Components.HasMods()) continue;
            if (!entity.Components.HasStack() && !entity.Components.HasBase()) continue;
            ImGui::Separator();
            Show(I18n::TrF("components_item_helpers", { { "id", Num("%u", entity.Id) } }));
            if (entity.Components.HasMods()) {
                int rarity = ctx->Components.GetItemRarity(entity.Components.Mods);
                bool ident = ctx->Components.IsItemIdentified(entity.Components.Mods);
                ImGui::Indent();
                Show(I18n::TrF("components_item_rarity", { { "rarity", Num("%d", rarity) } }));
                ImGui::SameLine(0.0f, ImGui::GetFontSize());
                Show(I18n::TrF("components_item_identified", { { "identified", YesNo(ident) } }));
                ImGui::Unindent();
                Examples::StatusBadge(Examples::StatusLevel::Ok,
                    "GetItemRarity + IsItemIdentified");
            }
            if (entity.Components.HasStack()) {
                int stack = ctx->Components.GetStackCount(entity.Components.Stack);
                ImGui::Indent();
                Show(I18n::TrF("components_item_stack", { { "count", Num("%d", stack) } }));
                ImGui::Unindent();
                Examples::StatusBadge(Examples::StatusLevel::Ok, "GetStackCount");
            }
            break;  // first matching item is enough
        }
    }
}

} // namespace Examples
