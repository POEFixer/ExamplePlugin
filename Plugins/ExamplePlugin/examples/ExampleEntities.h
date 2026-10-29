#pragma once
// ============================================================================
// ExampleEntities.h — v6 SDK
// ============================================================================
// Walks the snapshot's entity list with filtering. For each entity, lets the
// user toggle a "watch" so the host can keep its component data hot, then
// reads component data through ctx->Components.ReadXxx().
//
// Row labels are texts of lang/en.json; a label that is an SDK field's member
// name (ReservedFlat, IsFriendly, ...) stays as written in code (Text.h).
// ============================================================================

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "Text.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>

namespace Examples {

// --- Per-component renderers (v6 PluginSDK:: types) -------------------------

// label: a translated text (I18n::Tr) or an SDK name.
inline void DrawAddressRow(const char* label, uintptr_t addr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    char buf[20]; snprintf(buf, sizeof(buf), "0x%llX", static_cast<unsigned long long>(addr));
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", buf);
    if (ImGui::IsItemHovered() && ImGui::IsItemClicked()) ImGui::SetClipboardText(buf);
}

// One "label | value" row of a two-column table; label as for DrawAddressRow.
inline void DrawLabelCell(const char* label) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
}

// The name column fits its widest (translated) label every frame: width 0 in a table that is not
// ImGuiTableFlags_Resizable. Never a pixel width sized for the English labels.
inline void SetupNameValueColumns() {
    ImGui::TableSetupColumn(I18n::TrId("common_name"), ImGuiTableColumnFlags_WidthFixed, 0.0f);
    ImGui::TableSetupColumn(I18n::TrId("common_value"), ImGuiTableColumnFlags_WidthStretch);
}

// treeLabel: the TreeNode label, I18n::TrId(...) so its id is the key's.
inline void DrawVital(const char* treeLabel, const PluginSDK::Vital& v) {
    if (ImGui::TreeNode(treeLabel)) {
        if (ImGui::BeginTable("VitalT", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            SetupNameValueColumns();

            DrawLabelCell(I18n::Tr("vital_regeneration")); ImGui::Text("%.4f", v.Regeneration);
            DrawLabelCell(I18n::Tr("vital_total"));        ImGui::Text("%d", v.Total);
            DrawLabelCell("ReservedFlat");                 ImGui::Text("%d", v.ReservedFlat);
            DrawLabelCell(I18n::Tr("vital_current"));      ImGui::Text("%d", v.Current);
            DrawLabelCell(I18n::Tr("vital_reserved_percent")); ImGui::Text("%d", v.ReservedPercent);

            if (v.Total > 0) {
                float pct = static_cast<float>(v.Current) / v.Total * 100.0f;
                DrawLabelCell(I18n::Tr("vital_current_percent")); ImGui::Text("%.1f%%", pct);
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
}

inline void DrawLifeComp(const PluginSDK::Life& l) {
    if (ImGui::BeginTable("LifeT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        SetupNameValueColumns();
        DrawAddressRow(I18n::Tr("common_address"), l.Address);
        DrawAddressRow(I18n::Tr("common_owner_address"), l.OwnerAddress);
        ImGui::EndTable();
    }
    DrawVital(I18n::TrId("life_health"), l.Health);
    DrawVital(I18n::TrId("life_energy_shield"), l.EnergyShield);
    DrawVital(I18n::TrId("life_mana"), l.Mana);
}

inline void DrawRenderComp(const PluginSDK::Render& r) {
    if (ImGui::BeginTable("RenderT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        SetupNameValueColumns();
        DrawAddressRow(I18n::Tr("common_address"), r.Address);
        DrawAddressRow(I18n::Tr("common_owner_address"), r.OwnerAddress);
        ImGui::EndTable();
    }
    Show(I18n::TrF("render_world_position", { { "x", Num("%.2f", r.WorldX) },
                                              { "y", Num("%.2f", r.WorldY) },
                                              { "z", Num("%.2f", r.WorldZ) } }));
    Show(I18n::TrF("render_terrain_height", { { "height", Num("%.4f", r.TerrainHeight) } }));
    Show(I18n::TrF("render_model_bounds", { { "x", Num("%.2f", r.ModelBoundsX) },
                                            { "y", Num("%.2f", r.ModelBoundsY) },
                                            { "z", Num("%.2f", r.ModelBoundsZ) } }));
}

inline void DrawPositionedComp(const PluginSDK::Positioned& p) {
    if (ImGui::BeginTable("PosT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        SetupNameValueColumns();
        DrawAddressRow(I18n::Tr("common_address"), p.Address);
        DrawAddressRow(I18n::Tr("common_owner_address"), p.OwnerAddress);

        DrawLabelCell(I18n::Tr("positioned_reaction")); ImGui::Text("0x%02X", p.Reaction);

        DrawLabelCell("IsFriendly");
        ImGui::TextColored(p.IsFriendly ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                           "%s", TrueFalse(p.IsFriendly));
        ImGui::EndTable();
    }
}

inline void DrawTargetableComp(const PluginSDK::Targetable& t) {
    if (ImGui::BeginTable("TgtT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        SetupNameValueColumns();
        // The rows are the Targetable fields' member names (SDK names).
        auto boolRow = [](const char* name, bool val) {
            DrawLabelCell(name);
            ImGui::TextColored(val ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                               "%s", TrueFalse(val));
        };
        boolRow("IsTargetable", t.IsTargetable);
        boolRow("IsHighlightable", t.IsHighlightable);
        boolRow("IsTargetedByPlayer", t.IsTargetedByPlayer);
        boolRow("HiddenFromPlayer", t.HiddenFromPlayer);
        boolRow("MeetsQuestState", t.MeetsQuestState);
        boolRow("MeetsItemRequirements", t.MeetsItemRequirements);
        ImGui::EndTable();
    }
}

inline void DrawAnimatedComp(const PluginSDK::Animated& a) {
    if (ImGui::BeginTable("AnimT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        SetupNameValueColumns();
        DrawLabelCell(I18n::Tr("common_path")); ImGui::TextWrapped("%s", a.Path.c_str());
        DrawLabelCell(I18n::Tr("common_id"));   ImGui::Text("%u", a.Id);
        ImGui::EndTable();
    }
}

inline void DrawActorComp(const PluginSDK::Actor& a,
                          const std::vector<PluginSDK::ActiveSkill>& skills,
                          uint32_t entityId) {
    if (ImGui::BeginTable("ActorT", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        SetupNameValueColumns();
        DrawLabelCell(I18n::Tr("actor_animation")); ImGui::Text("%s", a.AnimationName.c_str());
        DrawLabelCell("AnimationId");               ImGui::Text("%d", a.AnimationId);
        ImGui::EndTable();
    }
    // A label with a per-entity id: the text, then "###" and an id that never changes.
    const std::string treeId =
        I18n::TrF("actor_skills", { { "count", Num("%zu", skills.size()) } }) + "###as" + std::to_string(entityId);
    if (!skills.empty() && ImGui::TreeNode(treeId.c_str())) {
        for (const auto& skill : skills) {
            if (ImGui::TreeNode(skill.Name.c_str())) {   // the game's skill name
                if (ImGui::BeginTable("SkillT", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    SetupNameValueColumns();

                    // Existing fields
                    DrawLabelCell(I18n::Tr("skill_use_stage"));   ImGui::Text("%d", skill.UseStage);
                    DrawLabelCell(I18n::Tr("skill_cast_type"));   ImGui::Text("%d", skill.CastType);
                    DrawLabelCell(I18n::Tr("skill_total_uses"));  ImGui::Text("%d", skill.TotalUses);
                    DrawLabelCell(I18n::Tr("skill_cooldown_ms")); ImGui::Text("%d", skill.TotalCooldownMs);

                    // New cooldown fields
                    DrawLabelCell(I18n::Tr("skill_max_uses"));         ImGui::Text("%d", skill.MaxUses);
                    DrawLabelCell(I18n::Tr("skill_active_cooldowns")); ImGui::Text("%d", skill.TotalActiveCooldowns);
                    DrawLabelCell(I18n::Tr("skill_remaining_uses"));
                    if (skill.MaxUses > 0) {
                        int remaining = skill.MaxUses - skill.TotalActiveCooldowns;
                        if (remaining < 0) remaining = 0;
                        ImGui::Text("%d / %d", remaining, skill.MaxUses);
                    } else {
                        ImGui::TextDisabled("%s", I18n::Tr("skill_no_cooldown"));
                    }

                    DrawLabelCell(I18n::Tr("skill_can_be_used"));
                    ImGui::TextColored(skill.CanBeUsed ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.3f,0.3f,1),
                                       "%s", TrueFalse(skill.CanBeUsed));

                    // Equipment subtree
                    DrawLabelCell(I18n::Tr("skill_gem_name_hash"));  ImGui::Text("0x%08X", skill.Equipment.GemNameHash);
                    DrawLabelCell(I18n::Tr("skill_inventory_slot")); ImGui::Text("%d", skill.Equipment.InventorySlot);
                    DrawLabelCell(I18n::Tr("skill_socket_index"));   ImGui::Text("%d", skill.Equipment.SocketIndex);
                    DrawLabelCell(I18n::Tr("skill_link_index"));     ImGui::Text("%d", skill.Equipment.LinkIndex);
                    DrawLabelCell(I18n::Tr("skill_on_player_item")); ImGui::Text("%s", TrueFalse(skill.Equipment.CanBeOnPlayerItem));
                    DrawLabelCell(I18n::Tr("skill_unknown_flag"));   ImGui::Text("%d", skill.Equipment.UnknownFlag);

                    // DAT pointers — click any row to copy to clipboard for Memory.Read use.
                    // The labels name the DAT tables / SDK fields (SDK names).
                    DrawAddressRow("GrantedEffectsPerLevel",      skill.GrantedEffectsPerLevelAddr);
                    DrawAddressRow("ActiveSkillsDat",             skill.ActiveSkillsDatAddr);
                    DrawAddressRow("GrantedEffectStatSetsPerLvl", skill.GrantedEffectStatSetsPerLevelAddr);
                    DrawAddressRow("SkillDetailsAddr (raw)",      skill.SkillDetailsAddr);

                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

inline void DrawBuffsComp(const std::vector<PluginSDK::Buff>& buffs, uint32_t entityId) {
    Show(I18n::TrF("buffs_effects_count", { { "count", Num("%zu", buffs.size()) } }));
    const std::string treeId = std::string(I18n::Tr("buffs_effects")) + "###bf" + std::to_string(entityId);
    if (!buffs.empty() && ImGui::TreeNode(treeId.c_str())) {
        for (const auto& buff : buffs) {
            if (ImGui::TreeNode(buff.Name.c_str())) {   // the game's buff id
                if (ImGui::BeginTable("BuffT", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                    SetupNameValueColumns();
                    DrawLabelCell(I18n::Tr("common_name")); ImGui::Text("%s", buff.Name.c_str());
                    DrawLabelCell(I18n::Tr("buffs_total_time"));
                    ImGui::Text("%.2f", buff.TotalTime > 0
                        ? buff.TotalTime : std::numeric_limits<float>::infinity());
                    DrawLabelCell(I18n::Tr("buffs_time_left"));
                    ImGui::Text("%.2f", buff.TimeLeft > 0
                        ? buff.TimeLeft : std::numeric_limits<float>::infinity());
                    DrawLabelCell(I18n::Tr("buffs_source_entity")); ImGui::Text("%u", buff.SourceEntityId);
                    DrawLabelCell(I18n::Tr("common_charges"));       ImGui::Text("%d", buff.Charges);
                    DrawLabelCell(I18n::Tr("buffs_flask_slot"));    ImGui::Text("%d", buff.FlaskSlot);
                    DrawLabelCell(I18n::Tr("buffs_effectiveness")); ImGui::Text("%d", 100 + buff.Effectiveness);
                    ImGui::EndTable();
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

// ----------------------------------------------------------------------------
// Type-name helpers: the SDK enum identifiers, exactly as PluginSDK.h spells them
// (SDK names stay English and untranslated, and one spelling everywhere: the
// entity rows and the type filter both use EntityTypeName)
// ----------------------------------------------------------------------------

inline constexpr int kEntityTypeCount = static_cast<int>(PluginSDK::EntityType::AzmeriWisp) + 1;

inline const char* EntityTypeName(PluginSDK::EntityType t) {
    using E = PluginSDK::EntityType;
    switch (t) {
        case E::Unidentified:      return "Unidentified";
        case E::Chest:             return "Chest";
        case E::NPC:               return "NPC";
        case E::Player:            return "Player";
        case E::Shrine:            return "Shrine";
        case E::Monster:           return "Monster";
        case E::DeliriumBomb:      return "DeliriumBomb";
        case E::DeliriumSpawner:   return "DeliriumSpawner";
        case E::OtherImportant:    return "OtherImportant";
        case E::Item:              return "Item";
        case E::Renderable:        return "Renderable";
        case E::AreaTransition:    return "AreaTransition";
        case E::ExpeditionMarker:  return "ExpeditionMarker";
        case E::ExpeditionRemnant: return "ExpeditionRemnant";
        case E::AzmeriWisp:        return "AzmeriWisp";
    }
    return "?";   // a value this example predates (a newer host)
}

inline const char* NearbyZoneName(PluginSDK::NearbyZone z) {
    using Z = PluginSDK::NearbyZone;
    switch (z) {
        case Z::None:        return "None";
        case Z::InnerCircle: return "InnerCircle";
        case Z::OuterCircle: return "OuterCircle";
        case Z::Far:         return "Far";
    }
    return "?";
}

// ----------------------------------------------------------------------------
// Main entity panel — walks snapshot.Entities directly (no v5 Watch list)
// ----------------------------------------------------------------------------

inline void DrawEntitiesPanel(const PluginSDK::Context* ctx,
                              const PluginSDK::Snapshot& snapshot) {
    if (!ctx) return;

    Show(I18n::TrF("entities_total", { { "count", Num("%zu", snapshot.Entities.size()) } }));

    static char idFilter[32] = "";
    static char pathFilter[128] = "";
    static int typeFilter = -1;
    ImGui::InputText(I18n::TrId("entities_filter_id"), idFilter, sizeof(idFilter));
    ImGui::InputText(I18n::TrId("entities_filter_path"), pathFilter, sizeof(pathFilter));

    // Built each frame (not static): the first entry is a translated text, the rest
    // are the EntityType enum identifiers in order, the same names the rows show.
    const char* typeNames[1 + kEntityTypeCount];
    typeNames[0] = I18n::Tr("entities_type_all");
    for (int t = 0; t < kEntityTypeCount; ++t)
        typeNames[1 + t] = EntityTypeName(static_cast<PluginSDK::EntityType>(t));
    ImGui::Combo(I18n::TrId("entities_filter_type"), &typeFilter, typeNames, IM_COUNTOF(typeNames));

    constexpr int kMaxShown = 500;
    int shown = 0;
    for (const auto& e : snapshot.Entities) {
        std::string pathNarrow;
        pathNarrow.reserve(e.Path.size());
        for (wchar_t c : e.Path) pathNarrow += (c < 128) ? static_cast<char>(c) : '?';

        if (idFilter[0] != '\0') {
            char idStr[16]; snprintf(idStr, sizeof(idStr), "%u", e.Id);
            if (std::strstr(idStr, idFilter) == nullptr) continue;
        }
        if (pathFilter[0] != '\0') {
            if (pathNarrow.find(pathFilter) == std::string::npos) continue;
        }
        if (typeFilter > 0 && static_cast<int>(e.EntityType) != (typeFilter - 1)) continue;

        shown++;
        if (shown > kMaxShown) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s",
                I18n::TrF("entities_truncated", { { "count", Num("%d", kMaxShown) } }).c_str());
            break;
        }

        char nodeLabel[256];
        snprintf(nodeLabel, sizeof(nodeLabel), "%u %s##ent%u",
                 e.Id, pathNarrow.c_str(), e.Id);

        if (ImGui::TreeNode(nodeLabel)) {
            // Entity properties
            if (ImGui::BeginTable("EntProps", 2,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                SetupNameValueColumns();
                ImGui::TableHeadersRow();

                DrawAddressRow(I18n::Tr("common_address"), e.Address);
                DrawLabelCell(I18n::Tr("common_id"));             ImGui::Text("%u", e.Id);
                DrawLabelCell(I18n::Tr("common_path"));           ImGui::TextWrapped("%s", pathNarrow.c_str());
                DrawLabelCell(I18n::Tr("entities_type"));         ImGui::Text("%s", EntityTypeName(e.EntityType));
                DrawLabelCell(I18n::Tr("entities_subtype"));      ImGui::Text("%d", static_cast<int>(e.EntitySubtype));
                DrawLabelCell(I18n::Tr("entities_state"));        ImGui::Text("%d", static_cast<int>(e.EntityState));
                DrawLabelCell(I18n::Tr("common_rarity"));         ImGui::Text("%d", e.Rarity);
                DrawLabelCell(I18n::Tr("entities_nearby_zone"));  ImGui::Text("%s", NearbyZoneName(e.Zone));
                DrawLabelCell(I18n::Tr("entities_world"));
                ImGui::Text("%.0f, %.0f, %.0f", e.WorldX, e.WorldY, e.WorldZ);

                ImGui::EndTable();
            }

            // Component readers — only call if the component exists on the entity.
            // The component nodes carry the components' SDK names.
            const std::string compLabel =
                std::string(I18n::Tr("entities_components")) + "###comp" + std::to_string(e.Id);
            if (ImGui::TreeNode(compLabel.c_str())) {
                const auto& c = e.Components;

                if (c.HasLife() && ImGui::TreeNode("Life")) {
                    DrawLifeComp(ctx->Components.ReadLife(c.Life));
                    ImGui::TreePop();
                }
                if (c.HasRender() && ImGui::TreeNode("Render")) {
                    DrawRenderComp(ctx->Components.ReadRender(c.Render));
                    ImGui::TreePop();
                }
                if (c.HasPositioned() && ImGui::TreeNode("Positioned")) {
                    DrawPositionedComp(ctx->Components.ReadPositioned(c.Positioned));
                    ImGui::TreePop();
                }
                if (c.HasTargetable() && ImGui::TreeNode("Targetable")) {
                    DrawTargetableComp(ctx->Components.ReadTargetable(c.Targetable));
                    ImGui::TreePop();
                }
                if (c.HasAnimated() && ImGui::TreeNode("Animated")) {
                    DrawAnimatedComp(ctx->Components.ReadAnimated(c.Animated));
                    ImGui::TreePop();
                }
                if (c.HasActor() && ImGui::TreeNode("Actor")) {
                    DrawActorComp(ctx->Components.ReadActor(c.Actor),
                                  ctx->Components.EnumerateActiveSkills(c.Actor),
                                  e.Id);
                    ImGui::TreePop();
                }
                if (c.HasBuffs() && ImGui::TreeNode("Buffs")) {
                    DrawBuffsComp(ctx->Components.EnumerateBuffs(c.Buffs), e.Id);
                    ImGui::TreePop();
                }
                // Monster mods (ObjectMagicProperties) — present on monster
                // entities; readable at spawn, before any related buff. Match on
                // MonsterMod::Id (most stable), Metadata, or Hash16/Hash32.
                if (c.HasOMP() && ImGui::TreeNode(I18n::TrId("entities_monster_mods"))) {
                    auto mmods = ctx->Components.EnumerateMonsterMods(c.OMP);
                    ImGui::Text("EnumerateMonsterMods: %zu", mmods.size());
                    for (const auto& m : mmods) {
                        ImGui::BulletText("%s | %s | 0x%04X / 0x%08X",
                            m.Id.c_str(),
                            m.Name.empty() ? "-" : m.Name.c_str(),
                            static_cast<unsigned>(m.Hash16),
                            static_cast<unsigned>(m.Hash32));
                        if (!m.Metadata.empty() && ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", m.Metadata.c_str());
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }
    if (shown == 0 && !snapshot.Entities.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", I18n::Tr("entities_no_match"));
    }
}

} // namespace Examples
