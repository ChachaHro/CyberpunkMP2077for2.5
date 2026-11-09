#include "App/World/AppearanceSystem.h"

#include "RED4ext/Scripting/Natives/gameIEntityStubSystem.hpp"
#include "RED4ext/Scripting/Natives/gameITransactionSystem.hpp"
#include "RED4ext/Scripting/Natives/gameItemModParams.hpp"
#include "RED4ext/Scripting/Natives/gameAddItemToSlotContext.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/EntityStubComponentPS.hpp"
#include "RED4ext/Scripting/Natives/Generated/game/data/Item_Record.hpp"
#include "RED4ext/NativeTypes.hpp"
#include "RED4ext/ResourceLoader.hpp"


#include "NetworkWorldSystem.h"
#include "Game/Utils.h"
#include "Game/CharacterCustomizationSystem.h"
#include "RED4ext/Scripting/Natives/Generated/game/PuppetPS.hpp"

AppearanceSystem::AppearanceSystem()
{

}

void AppearanceSystem::OnInitialize(const RED4ext::JobHandle& aJob)
{
    spdlog::info("[AppearanceSystem] OnInitialize");
}

void AppearanceSystem::OnWorldAttached(RED4ext::world::RuntimeScene* aScene)
{    
    spdlog::info("[AppearanceSystem] OnWorldAttached");
    Red::CallVirtual(this, "OnWorldAttached");
}

void AppearanceSystem::OnBeforeWorldDetach(RED4ext::world::RuntimeScene* aScene)
{
    spdlog::info("[AppearanceSystem] OnBeforeWorldDetach");
    Red::CallVirtual(this, "OnBeforeWorldDetach");
}

Red::DynArray<Red::TweakDBID> AppearanceSystem::GetEntityItems(Red::EntityID & entityID)
{
    return m_playerEquipment[entityID];
}

Vector<String> AppearanceSystem::GetPlayerItems(Red::Handle<Red::GameObject> player)
{
    auto equipment = Vector<String>();
    Red::DynArray<Red::CString> items;
    Red::CallVirtual(this, "GetPlayerItems", items);
    for (auto item : items)
    {
        spdlog::info("Getting: {}", item.c_str());
        equipment.push_back(item.c_str());
    }
    return equipment;
}

bool AppearanceSystem::CaptureLocalAppearance(Vector<String>& aEquipment, Vector<uint8_t>& aCcstate)
{
    aEquipment.clear();
    aCcstate.clear();

    const auto playerSystem = Red::GetGameSystem<Game::PlayerSystem>();
    Red::Handle<Red::GameObject> player;
    if (playerSystem)
        playerSystem->GetLocalPlayerControlledGameObject(player);
    if (!player)
        return false;

    aEquipment = GetPlayerItems(player);

    auto ccSystem = Red::GetGameSystem<Red::game::ui::CharacterCustomizationSystem>();
    if (!ccSystem)
        return false;

    // GetCustomizationState() returns (ccSystem + 0x78), which can never be null - the instance behind it
    // can be, and is during normal gameplay. Serializing a null instance crashes the game.
    auto stateHandle = GetCustomizationState(ccSystem);
    if (!stateHandle || !stateHandle->instance)
        return false;

    auto writer = CMPWriter();
    CharacterCustomizationState_Serialize(stateHandle->instance, &writer);
    aCcstate = writer.bytes;
    return !aCcstate.empty();
}

Red::CString AppearanceSystem::GetLocalAppearance()
{
    Vector<String> equipment;
    Vector<uint8_t> ccstate;
    if (!CaptureLocalAppearance(equipment, ccstate))
    {
        spdlog::info("[Appearance] local appearance not available");
        return "";
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(ccstate.size() * 2);
    for (const auto byte : ccstate)
    {
        hex.push_back(kHex[byte >> 4]);
        hex.push_back(kHex[byte & 0x0F]);
    }

    nlohmann::json data;
    data["v"] = 1;
    data["ccstate"] = hex;
    data["equipment"] = nlohmann::json::array();
    for (const auto& item : equipment)
        data["equipment"].push_back(std::string(item.c_str()));

    const auto text = data.dump();
    spdlog::info("[Appearance] local appearance: {} ccstate bytes, {} item(s)", ccstate.size(), equipment.size());
    return text.c_str();
}

void AppearanceSystem::AddEntity(const Red::EntityID entityID, const Red::DynArray<Red::TweakDBID>& items, const Vector<uint8_t> ccstate)
{
    spdlog::info("Logging Entity Appearance: {}", entityID.hash);
    m_playerEquipment[entityID] = items;
    m_playerCcstate[entityID] = ccstate;
}

void AddItems(Red::Handle<Red::game::Object> & object, Red::DynArray<Red::TweakDBID> const & items, game::ui::CharacterCustomizationState const * state)
{
    auto system = Red::GetGameSystem<Red::game::ITransactionSystem>();
    for (auto item : items)
    {
        auto itemID = Red::ItemID();
        ItemID_Create(&itemID, item, -1);
        if (!itemID.IsValid())
        {
            spdlog::warn("ItemID creation failed");
            continue;
        }

        auto item_record = Red::Handle<Red::game::data::Item_Record>();
        GetItemRecord(&item_record, item);
        if (!item_record)
        {
            spdlog::warn("Item record not found");
            continue;
        }
        
        auto placementSlotHandle = Red::Handle<Red::game::data::AttachmentSlot_Record>();
        GetPlacementSlot(item_record.instance, &placementSlotHandle, 0);
        if (!placementSlotHandle)
        {
            spdlog::warn("Placement slot not found... kinda weird");
            continue;
        }

        auto placementSlot = placementSlotHandle.instance->recordID;

        Red::CString str;
        Red::CallStatic("gamedataTDBIDHelper", "ToStringDEBUG", str, item);
        Red::CString placementStr;
        Red::CallStatic("gamedataTDBIDHelper", "ToStringDEBUG", placementStr, placementSlot);

        // 2.5 : lecture du flat TweakDB "<record>.appearanceName" via l'interface script
        // (le champ ajouté au record par l'ancien fork du SDK n'existe plus).
        Red::CName appearanceName;
        Red::CallStatic("gamedataTweakDBInterface", "GetCName", appearanceName,
                        Red::TweakDBID(item_record.instance->recordID, ".appearanceName"), Red::CName());
        std::string appearance = appearanceName.ToString();
        // auto suffixes = EvalArrayTweakDBID(&item_record.instance->appearanceSuffixes);
        // for (auto const suffix : suffixes) {
        //     if (suffix == Red::TweakDBID("itemsFactoryAppearanceSuffix.Gender")) {
        //         if (state->isBodyGenderMale) {
        //             appearance.append("&Male");
        //         } else {
        //             appearance.append("&Female");
        //         }
        //     } else if (suffix == Red::TweakDBID("itemsFactoryAppearanceSuffix.Camera")) {
        //         appearance.append("&TPP");
        //     } else if (suffix == Red::TweakDBID("itemsFactoryAppearanceSuffix.Partial")) {
        //         appearance.append("&Full");
        //     } else if (suffix == Red::TweakDBID("itemsFactoryAppearanceSuffix.HairType")) {
        //         if (state->tags.Contains(Red::CName("Short"))) {
        //             appearance.append("&Short");
        //         } else if (state->tags.Contains(Red::CName("Long"))) {
        //             appearance.append("&Long");
        //         } else if (state->tags.Contains(Red::CName("Dreads"))) {
        //             appearance.append("&Dreads");
        //         } else if (state->tags.Contains(Red::CName("Buzz"))) {
        //             appearance.append("&Buzz");
        //         } else {
        //             appearance.append("&Bald");
        //         }
        //     }
        // }
        spdlog::info("Adding {} to {} with \"{}\"", str.c_str(), placementStr.c_str(), appearance);
        Red::CString redAppStr;
        GetItemAppearanceName(&redAppStr, object, object, item_record, itemID);
        spdlog::info("* computed suffix: {}", redAppStr.c_str());
        appearance.append(redAppStr.c_str());

        appearanceName = appearance.c_str();

        // 2.5 : TransactionSystem.GiveItem / AddItemToSlot appelées par nom, paramètres remplis
        // par nom (obj, itemID, quantity / slotID, plane, garmentAppearanceName). Remplace les
        // appels par vtable et les structures ItemModParams / AddItemToSlotContext du fork 2.2.
        int32_t quantity = 1;
        bool given = false;
        Red::CStackType givenRet(Red::ResolveType<bool>(), &given);
        Red::CallByParamNames(system, "GiveItem",
                              {{"obj", &object}, {"itemID", &itemID}, {"quantity", &quantity}}, &givenRet);
        if (given)
        {
            auto plane = Red::ERenderingPlane::RPl_Scene;
            bool added = false;
            Red::CStackType addedRet(Red::ResolveType<bool>(), &added);
            Red::CallByParamNames(system, "AddItemToSlot",
                                  {{"obj", &object},
                                   {"slotID", &placementSlot},
                                   {"itemID", &itemID},
                                   {"plane", &plane},
                                   {"garmentAppearanceName", &appearanceName}},
                                  &addedRet);
            if (!added)
            {
                spdlog::info("AddItemToSlot failed");
            }
        } else
        {
            spdlog::info("GiveItem failed");
        }
    }
}

bool AppearanceSystem::ApplyAppearance(Red::Handle<Red::game::Object> object)
{
    if (!object.instance)
    {
        spdlog::error("[Appearance] ApplyAppearance called with a null object - aborting");
        return false;
    }

    if (m_playerCcstate.find(object.instance->entityID) == m_playerCcstate.end())
    {
        // not our entity
        return false;
    }

    auto bytes = m_playerCcstate[object.instance->entityID];
    if (bytes.size() == 0)
    {
        spdlog::info("no bytes for {}", object->entityID.hash);
        return false;
    }

    // NOTE: leftover upstream debug code - sets every remote player's display name to
    // "Test" by writing into a hand-mapped field. Verified NOT to be the spawn crash
    // (execution reaches here fine), but it serves no purpose either.
    object.instance->displayName.unk08 = Red::CString("Test");

    spdlog::info("Loaded bytes: {}", bytes.size());

    Red::Handle<game::ui::CharacterCustomizationState> stateHandle;
    CreateHandle_CharacterCustomizationState(&stateHandle);

    if (!stateHandle.instance)
    {
        spdlog::error("[Appearance] CreateHandle_CharacterCustomizationState returned null - aborting");
        return false;
    }

    if (bytes.empty())
    {
        spdlog::warn("[Appearance] no ccstate bytes for this entity - appearance will be default");
    }
    else
    {
        auto reader = CMPReader(bytes);
        CharacterCustomizationState_Serialize(stateHandle.instance, &reader);
    }

    // shouldn't be needed
    // Red::CallVirtual(this, "AddBodyParts", object, CcIsBodyGenderMale(stateHandle.instance));

    auto ps = reinterpret_cast<Red::game::PuppetPS*>(object.instance->persistentState.instance);
    if (!ps)
    {
        spdlog::error("[Appearance] puppet persistentState is null - aborting before writing unk72");
        return false;
    }
    // checked during item adding process for &TPP
    ps->unk72[0] = 1;

    // old method
    // Red::CallVirtual(this, "AddItems", object);

    // c++ method
    auto items = m_playerEquipment[object.instance->entityID];
    // if (CcIsBodyGenderMale(stateHandle.instance)) {
    //     // items.PushBack("Items.PlayerMaTppHead");
    //     items.PushBack("Items.MuppetMaHead");
    // } else {
    //     // items.PushBack("Items.PlayerWaTppHead");
    //     items.PushBack("Items.MuppetWaHead");
    // }
    // items.PushBack("Items.MuppetArms");
    AddItems(object, items, stateHandle.instance);

    // spdlog::info("head groups read:");
    // for (auto &group : stateHandle.instance->unk70) 
    // {
    //     spdlog::info(group.name.ToString());
    // }
    // ma
    //  TPP
    //  FPP
    //  hairs
    //  character_customization
    //  TPP_proxy
    //  FPP_proxy
    //  FPP_hairs
    //  TPP_photomode
    //  beards
    //  face
    //  finalSceneBruises
    // wa
    //  TPP
    //  FPP
    //  hairs
    //  character_customization
    //  TPP_proxy
    //  FPP_proxy
    //  FPP_hairs
    //  TPP_photomode
    //  face
    //  finalSceneBruises

    // spdlog::info("body groups read:");
    // for (auto &group : stateHandle.instance->unk80) 
    // {
    //     spdlog::info(group.name.ToString());
    // }
    // FPP_Body
    // TPP_Body
    // character_creation
    // genitals
    // breast

    // FPP_Body
    // TPP_Body
    // character_creation
    // genitals
    // breast
    // lifted_feet
    // flat_feet

    // spdlog::info("arm groups read:");
    // for (auto &group : stateHandle.instance->unk90) 
    // {
    //     spdlog::info(group.name.ToString());
    // }
    // holstered_default
    // holstered_strong
    // unholstered_strong
    // holstered_nanowire
    // unholstered_nanowire
    // character_customization
    // personal_link_simple
    // personal_link_advanced      
    // holstered_launcher
    // unholstered_launcher
    // holstered_mantis
    // unholstered_mantis
    // nails

    // holstered_default_tpp
    // holstered_default_fpp
    // holstered_strong_tpp
    // holstered_strong_fpp
    // unholstered_strong
    // holstered_nanowire_tpp
    // holstered_nanowire_fpp
    // unholstered_nanowire
    // character_customization
    // personal_link_simple
    // personal_link_advanced
    // holstered_launcher_tpp
    // holstered_launcher_fpp
    // unholstered_launcher
    // holstered_mantis_tpp
    // holstered_mantis_fpp
    // unholstered_mantis
    // nails

    if (stateHandle)
    {
        spdlog::info("Scheduling change");

        Red::DynArray<Red::world::EntityAppearanceChangeParameter::Key> keys;
        CCState_GetHeadCustomization(stateHandle.instance, "TPP", true, keys);
        CCState_GetHeadCustomization(stateHandle.instance, "face", true, keys);
        CCState_GetHeadCustomization(stateHandle.instance, "hairs", true, keys);
        if (CcIsBodyGenderMale(stateHandle.instance)) 
        {
            CCState_GetHeadCustomization(stateHandle.instance, "beards", true, keys);
        }

        // does all of them
        // CCState_GetHeadCustomization(stateHandle.instance, "character_customization", true, keys);

        CCState_GetBodyCustomization(stateHandle.instance, "TPP_Body", true, keys);
        // make genitals appear as well
        // CCState_GetBodyCustomization(stateHandle.instance, "character_creation", true, keys);
        // pops through clothes lol
        // CCState_GetBodyCustomization(stateHandle.instance, "genitals", true, keys);
        CCState_GetBodyCustomization(stateHandle.instance, "breast", true, keys);
        CCState_GetBodyCustomization(stateHandle.instance, "lifted_feet", true, keys);
        CCState_GetBodyCustomization(stateHandle.instance, "flat_feet", true, keys);

        // CCState_GetArmsCustomization(stateHandle.instance, "character_customization", true, keys);
        CCState_GetArmsCustomization(stateHandle.instance, "holstered_default", true, keys);
        CCState_GetArmsCustomization(stateHandle.instance, "nails", true, keys);

        auto changer = Red::GetRuntimeSystem<Red::world::RuntimeSystemEntityAppearanceChanger>();
        Red::WeakHandle<Red::game::Object> weakHandle = object;
        CpSpan<Red::world::EntityAppearanceChangeParameter::Key> old_keys = {
            .start = nullptr,
            .end = nullptr
        };
        CpSpan<Red::world::EntityAppearanceChangeParameter::Key> new_keys = {
            .start = keys.Data(),
            .end = &keys.Data()[keys.size()]
        };
        const std::function<void (void)> callback = [stateHandle = std::move(stateHandle), object = std::move(object)](void) 
        {
            // AddItems(object, items, stateHandle.instance);

            // this might not be needed? - JACK
            // also makes wa faces disappear :/

            // spdlog::info("Changed callback");
            // // TransactionSystem::sub_440(object, std::function);
            // // hide some components based on Get*Customization
            // // MorphHead;
            
            // stateHandle.instance->MorphHead("TPP", true, object, true);
            // // stateHandle.instance->MorphHead("face", true, object, true);
            // stateHandle.instance->MorphHead("hairs", true, object, true);
            
            // if (CcIsBodyGenderMale(stateHandle.instance)) 
            // {
            //     stateHandle.instance->MorphHead("beards", true, object, true);
            // }

            // stateHandle.instance->MorphBody("TPP_Body", true, object, true);
            // // stateHandle.instance->MorphBody("genitals", true, object, true);
            // // stateHandle.instance->MorphBody("breast", true, object, true);

            // stateHandle.instance->MorphArms("holstered_default", true, object, true);
            // // stateHandle.instance->MorphArms("nails", true, object, true);

            // stateHandle.instance->ApplyHead("TPP", true, object);
            // // stateHandle.instance->ApplyHead("face", true, object);
            // stateHandle.instance->ApplyHead("hairs", true, object);
        
            // if (CcIsBodyGenderMale(stateHandle.instance)) 
            // {
            //     stateHandle.instance->ApplyHead("beards", true, object);
            // }

            // stateHandle.instance->ApplyBody("TPP_Body", true, object);
            // // stateHandle.instance->ApplyBody("genitals", true, object);
            // // stateHandle.instance->ApplyBody("breast", true, object);

            // stateHandle.instance->ApplyArms("holstered_default", true, object);
            // // stateHandle.instance->ApplyArms("nails", true, object);
        
            // // auto apprSystem = Red::GetGameSystem<AppearanceSystem>();
            // // Red::CallVirtual(apprSystem, "AddItems", object);
            // // Red::CallVirtual(apprSystem, "AddBodyParts", object, CcIsBodyGenderMale(stateHandle.instance));
            
        };
        ScheduleSynchronizedAppearanceChanges(changer, weakHandle, &old_keys, &new_keys, callback, 0);
    }
    else
    {
        spdlog::warn("CustomizationState was null");
    }

    return true;
}
