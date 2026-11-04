#pragma once

#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationSystem.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationState.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/ui/CharacterCustomizationPreset.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/Item_Record.hpp>
#include <RED4ext/Scripting/Natives/Generated/game/data/AttachmentSlot_Record.hpp>
#include <RED4ext/Scripting/Natives/Generated/ink/GameSettingsResource.hpp>
#include <RED4ext/Scripting/Natives/CResource.hpp>
#include <RED4ext/Scripting/Natives/Generated/world/RuntimeSystemEntityAppearanceChanger.hpp>
#include "Game/Utils.h"

using namespace Red;

// ── Portage patch 2.5 ─────────────────────────────────────────────────────────────
// Types que l'ancien fork du SDK définissait et que le SDK 2.5 ne fournit pas.
namespace RED4ext::world::EntityAppearanceChangeParameter
{
// Identique à AppearanceDescriptor d'ArchiveXL 1.28 (patch 2.5) : { ResourcePath, CName }.
struct Key
{
    ResourcePath resource; // 00
    CName definition;      // 08
};
static_assert(sizeof(Key) == 0x10);
}

namespace RED4ext::game::data
{
// Valeur TweakDB encodée (sert seulement de type "this" pour EvalCName / EvalArrayTweakDBID).
struct Val
{
    uint32_t hash;          // 00
    uint8_t length;         // 04
    uint8_t tbdOffsetBE[3]; // 05
    uint8_t unk08;          // 08
    uint8_t unk09;          // 09
    uint8_t unk0A;          // 0A
    uint8_t unk0B;          // 0B
};
}

// Get{Head,Body,Arms}Customization : appelées par identifiant stable (mêmes hash qu'ArchiveXL
// 1.28 : GetHeadAppearances1, etc.) au lieu des index de vtable 0x190/0x1A0/0x1B0.
// Vérifié sur l'exécutable 2.5 : les variantes 1 et 2 résolvent à la même fonction, présente
// dans la vtable de gameuiCharacterCustomizationState aux mêmes positions qu'en 2.2.
using AppearanceKeys = DynArray<world::EntityAppearanceChangeParameter::Key>;

constexpr auto CCState_GetHeadCustomization = Core::RawFunc<4051123539UL,
    void (*)(game::ui::CharacterCustomizationState*, CName aGroup, bool, AppearanceKeys&)>();
constexpr auto CCState_GetBodyCustomization = Core::RawFunc<4014159024UL,
    void (*)(game::ui::CharacterCustomizationState*, CName aGroup, bool, AppearanceKeys&)>();
constexpr auto CCState_GetArmsCustomization = Core::RawFunc<110771572UL,
    void (*)(game::ui::CharacterCustomizationState*, CName aGroup, bool, AppearanceKeys&)>();

// isBodyGenderMale reste à 0x44 en 2.5 : la zone 0x40-0xA0 est inchangée (groupes tête/corps/bras
// à 0x70/0x80/0x90 en 2.2 comme dans ArchiveXL 2.5) ; la croissance 0xF0 -> 0x110 est après.
inline bool CcIsBodyGenderMale(const game::ui::CharacterCustomizationState* aState)
{
    return *reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(aState) + 0x44) != 0;
}
// ──────────────────────────────────────────────────────────────────────────────────

constexpr auto CreateHandle_CharacterCustomizationState = Core::RawFunc<2710710832UL, 
    Handle<game::ui::CharacterCustomizationState> * (*)(Handle<game::ui::CharacterCustomizationState> *)>();


constexpr auto CharacterCustomizationState_InitFromPreset = Core::RawFunc<1993027039UL, 
    Handle<game::ui::CharacterCustomizationState> * (*)(game::ui::CharacterCustomizationState *, Handle<game::ui::CharacterCustomizationPreset> *)>();

constexpr auto CharacterCustomizationState_Serialize = Core::RawFunc<108403442UL, 
    void (*)(game::ui::CharacterCustomizationState *, CMPStream *)>();

// constexpr auto gameTransactionSystem_AddItemToSlot = Core::RawFunc<56564470UL, 
//     void (*)(game::TransactionSystem *, const AddItemToSlotContext *)>();

constexpr auto GetGameSettings = Core::RawFunc<394336209UL, 
    ink::GameSettingsResource const * (*)(void)>();

// constexpr auto ItemID_SetUniqueCounterValue = Core::RawFunc<1713639304UL, void (*)(ItemID *)>();

constexpr auto ItemID_Create = Core::RawFunc<3020231866UL, ItemID * (*)(ItemID *, TweakDBID, int64_t seed)>();

constexpr auto GetItemRecord = Core::RawFunc<3236045910UL, 
    Handle<game::data::Item_Record> * (Handle<game::data::Item_Record>::*)(TweakDBID)>();

constexpr auto GetPlacementSlot = Core::RawFunc<1934369954UL, 
    Handle<game::data::AttachmentSlot_Record> * (game::data::Item_Record::*)(Handle<game::data::AttachmentSlot_Record> *, int32_t)>();

constexpr auto EvalArrayTweakDBID = Core::RawFunc<523315084UL, 
    DynArray<TweakDBID> const & (game::data::Val::*)(void)>();

constexpr auto EvalCName = Core::RawFunc<3648983380UL, 
    CName const & (game::data::Val::*)(void)>();

constexpr auto ResolveSuffix = Core::RawFunc<4124714348UL, 
    CString * (*)(void *, CString *, Handle<game::data::Item_Record> &, bool isMale)>();

constexpr auto ResolveVisualTags = Core::RawFunc<1337337421UL, 
    CString * (*)(CName, CString *, Handle<game::data::Item_Record> &)>();

constexpr auto GetItemAppearanceName = Core::RawFunc<3029088864UL, 
    CString * (*)(CString *, Handle<game::Object> const &, Handle<game::Object> const &, Handle<game::data::Item_Record> const &, ItemID const &)>();

template <class T>
struct CpSpan 
{
    T * start;
    T * end;
};

constexpr auto ScheduleSynchronizedAppearanceChanges = Core::RawFunc<386815609UL, 
    void (*)(world::RuntimeSystemEntityAppearanceChanger *, 
        WeakHandle<game::Object> &, 
        CpSpan<world::EntityAppearanceChangeParameter::Key> * old_keys,
        CpSpan<world::EntityAppearanceChangeParameter::Key> * new_keys,
        std::function<void (void)> const &,
        uint32_t //io::EAsyncPriority
    )>();

// CharacterCustomizationSystem
// THandle<game::ui::CharacterCustomizationState> unk78

// could use game::ui::CharacterCustomizationState::Serialize to pass data to server

inline Handle<game::ui::CharacterCustomizationState> * GetCustomizationState(game::ui::CharacterCustomizationSystem * self) 
{
    return reinterpret_cast<Handle<game::ui::CharacterCustomizationState> *>((uintptr_t)self + 0x78);
}
