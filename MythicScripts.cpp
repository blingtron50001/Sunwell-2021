#include "CreatureOutfits.h"
#include "GameObjectAI.h"
#include "Group.h"
#include "Guild.h"
#include "InstanceSaveMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "MythicAI.h"
#include "Player.h"
#include "AuctionHouseMgr.h"
#include "Transmogrification.h"
#include "utf8.h"

#include <cctype>
#include <array>

enum MythicHandler
{
    GOSSIP_MYTHIC_MYTHIC_ENABLED                = 1,
    GOSSIP_MYTHIC_SELECTED_DUNGEON,
    GOSSIP_MYTHIC_CHANGE_EVENT
};

enum MythicGossipStep
{
    MYTHIC_GOSSIP_DEFAULT                       = 1,
    MYTHIC_GOSSIP_PRESENT_DUNGEONS,
    MYTHIC_GOSSIP_WAIT_FOR_MYTHIC,
    MYTHIC_GOSSIP_GO_START_MYTHIC,
    MYTHIC_GOSSIP_PRESENT_EVENTS,

    MYTHIC_GOSSIP_MAX
};

enum MythicGossipTextIds
{
    MYTHIC_TEXT_ID_ENABLE_MYTHIC_FOR_GROUP      = 91528,
    MYTHIC_TEXT_ID_SELECT_DUNGEON               = 91529
};

struct MythicHandlerMapIdName
{
    uint32 instanceId = 0;
    std::string name = "Invalid";
};

static std::vector<MythicHandlerMapIdName> _mythicHandlerMapIdName =
{
    { 209, "Zul'farrak" },
    { 542, "Hellfire Citadel: The Blood Furnace" },
    { 576, "The Nexus" }
};

enum MythicHandlerGossips
{
    MYTHIC_HANDLER_GOSSIP_NOT_LEADER = 91531
};

std::string const InvalidKeyError = "Failed to initialize key level.";
class npc_mythic_handler : public CreatureScript
{
private:
    std::unordered_map<uint32, MythicHandlerMapIdName> _dungeonGossips;
public:
    npc_mythic_handler() : CreatureScript("npc_mythic_handler")
    {
        std::vector<uint32> const& currentMythics = sMythicMgr.GetCurrentMythicMaps();
        uint32 gossipId = GOSSIP_MYTHIC_SELECTED_DUNGEON;
        for (auto const& id : currentMythics)
        {
            auto it = std::find_if(_mythicHandlerMapIdName.begin(), _mythicHandlerMapIdName.end(), [&id](MythicHandlerMapIdName const& ref)
            {
                return ref.instanceId == id;
            });

            if (it == _mythicHandlerMapIdName.end())
                continue;

            _dungeonGossips[gossipId] = { it->instanceId, it->name };
            ++gossipId;
        }
    }

    void PrepareGossipFor(Player* player, Creature* creature, MythicGossipStep step)
    {
        if (!player || !creature || step >= MYTHIC_GOSSIP_MAX)
            return;

        player->PlayerTalkClass->ClearMenus();
        uint32 textId = MYTHIC_HANDLER_GOSSIP_NOT_LEADER;

        switch (step)
        {
            case MYTHIC_GOSSIP_DEFAULT:
            {
                if ( player->GetGroup() && player->GetGroup()->GetLeaderGUID() == player->GetGUID() )
                {
                    textId = MYTHIC_TEXT_ID_ENABLE_MYTHIC_FOR_GROUP;
                    player->ADD_GOSSIP_ITEM( GOSSIP_ICON_CHAT, "Enable Mythic difficulty for my group.", GOSSIP_SENDER_MAIN, GOSSIP_MYTHIC_MYTHIC_ENABLED );
                    /*if ( player->IsGameMaster() )
                        player->ADD_GOSSIP_ITEM( GOSSIP_ICON_CHAT, "*Debug* Change Mythic Event", GOSSIP_SENDER_MAIN, MYTHIC_GOSSIP_PRESENT_EVENTS );
                    */
                }

                break;
            }
            case MYTHIC_GOSSIP_PRESENT_DUNGEONS:
            {
                textId = MYTHIC_TEXT_ID_SELECT_DUNGEON;
                for (auto const& gossipId : _dungeonGossips)
                {
                    std::string mapName = "Teleport me to " + gossipId.second.name;
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, mapName, GOSSIP_SENDER_MAIN, gossipId.first);
                }
                break;
            }
            case MYTHIC_GOSSIP_PRESENT_EVENTS:
            {
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Crystals", GOSSIP_SENDER_MAIN, 1000);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Lost Npcs", GOSSIP_SENDER_MAIN, 1001);
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Alchemist", GOSSIP_SENDER_MAIN, 1002);
                break;
            }
            default:
                break;
        }

        player->SEND_GOSSIP_MENU(textId, creature->GetGUID());
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player)
            return true;

        auto CloseGossipAndReturn = [player, creature](std::string const& str)
        {
            creature->MonsterWhisper(str.c_str(), player);
            player->PlayerTalkClass->SendCloseGossip();
            return true;
        };

        if (sWorld->getBoolConfig(CONFIG_MYTHIC_ENABLE) != true)
        {
            CloseGossipAndReturn("Mythic is currently disabled.");
            return true;
        }

        player->PlayerTalkClass->ClearMenus();
        //! Prepare repeatable quest that rewards keys
        if (creature->IsQuestGiver())
            player->PrepareQuestMenu(creature->GetGUID());

        if (!player->GetGroup())
        {
            CloseGossipAndReturn("You need to be in a group to use mythic.");
            return true;
        }

        uint64 const leaderGUID = player->GetGroup()->GetLeaderGUID();
        bool const IsKeySet = player->GetGroup()->GetGroupMythicKeyLevel() != 0;
        if (IsKeySet)
        {
            PrepareGossipFor(player, creature, MYTHIC_GOSSIP_PRESENT_DUNGEONS);
            return true;
        }
        else if (leaderGUID != player->GetGUID())
            //! else if we're not leader and group is not mythic yet, close the window
        {
            return true;
        }

        PrepareGossipFor(player, creature, MYTHIC_GOSSIP_DEFAULT);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*uiSender*/, uint32 actionId) override
    {
        if (!player)
            return true;

        auto CloseGossipAndReturn = [player, creature](std::string const& str)
        {
            creature->MonsterWhisper(str.c_str(), player);
            player->PlayerTalkClass->SendCloseGossip();
            return true;
        };

        // for debug
        {
            if (actionId == MYTHIC_GOSSIP_PRESENT_EVENTS)
            {
                PrepareGossipFor(player, creature, MYTHIC_GOSSIP_PRESENT_EVENTS);
                return true;
            }
            else if (actionId == 1000)
            {
                sMythicMgr.SetCurrentMythicEventId(Mythic::MythicEventType::MYTHIC_EVENT_CRYSTALS);
                CloseGossipAndReturn("Event changed to crystals");
                return true;
            }
            else if (actionId == 1001)
            {
                sMythicMgr.SetCurrentMythicEventId(Mythic::MythicEventType::MYTHIC_EVENT_LOST_NPCS);
                CloseGossipAndReturn("Event changed to lost npcs");
                return true;
            }
            else if (actionId == 1002)
            {
                sMythicMgr.SetCurrentMythicEventId(Mythic::MythicEventType::MYTHIC_EVENT_ALCHEMIST);
                CloseGossipAndReturn("Event changed to alchemist");
                return true;
            }
        }

        if (actionId >= GOSSIP_MYTHIC_SELECTED_DUNGEON)
        {
            sMythicMgr.TeleportToMythicDestination(_dungeonGossips[actionId].instanceId, player);
            return CloseGossipAndReturn("Teleporting to selected dungeon.");
        }
        else if (actionId == GOSSIP_MYTHIC_MYTHIC_ENABLED)
        {
            uint32 _keyLevel = player->GetMythicKeyLevel();
            if (!player->GetGroup() || player->GetGroup()->GetLeaderGUID() != player->GetGUID())
                return CloseGossipAndReturn("Failed to fetch a group or you're not a leader of a group.");

            if (!_keyLevel)
                return CloseGossipAndReturn("Internal mythic error, key level set to 0.");

            if (player->GetGroup())
            {
                for (const Group::MemberSlot& slot : player->GetGroup()->GetMemberSlots())
                {
                    Player* gpPlayer = ObjectAccessor::GetPlayer(*creature, slot.guid);
                    if (!gpPlayer || gpPlayer == player || gpPlayer->IsDuringRemoveFromWorld() ||
                        !gpPlayer->IsInWorld())
                        continue;

                    if (gpPlayer->InBattlegroundQueue() || gpPlayer->isUsingLfg())
                        return CloseGossipAndReturn("At least one member of the group is in LFG or Battleground queue.");
                }
            }

            auto result = sMythicMgr.ConvertGroupToMythic(player->GetGroup());
            if (result != Mythic::MythicGroupConvertErrors::MYTHIC_GROUP_ERROR_OK)
            {
                return CloseGossipAndReturn("Mythic internal error.");
            }

            player->GetGroup()->SetGroupMythicKeyLevel(_keyLevel);
            PrepareGossipFor(player, creature, MYTHIC_GOSSIP_PRESENT_DUNGEONS);
        }

        return true;
    }

    struct npc_mythic_handlerAI : public ScriptedAI
    {
        npc_mythic_handlerAI(Creature* creature) : ScriptedAI(creature) { }

        void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*type*/, SpellSchoolMask /*mask*/) override
        {
            damage = 0;
        }

        void Reset() override
        {
            std::shared_ptr<CreatureOutfit> outfit(new CreatureOutfit(RACE_GNOME, GENDER_MALE));
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_CHEST, 31057);
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_FEET, 34574);
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_HANDS, 31055);
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_HEAD, 31056);
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_LEGS, 31058);
            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, 45170);
            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1, 0);
            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 2, 0);
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_SHOULDERS, 31059);
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_WAIST, 34557);
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_WRISTS, 34447);

            me->SetOutfit(outfit);

            me->SetFlag(UNIT_NPC_FLAGS, (UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER));
            me->setFaction(35);
            me->SetImmuneToAll(true);
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_mythic_handlerAI(creature);
    }
};

constexpr uint32 MYTHIC_CRYSTAL_HELLO_TEXT{ 91527 };
struct go_mythic_crystal : GameObjectAI
{
    go_mythic_crystal(GameObject* go) : GameObjectAI(go)
    {
        instance = go->GetInstanceScript();
        ASSERT(instance);
        keySetter = 0;
        keyLevel = 0;
    }

    bool QuestReward(Player* player, Quest const* quest, uint32 opt) override
    {
        if (instance->IsMythicTimerRunning() || keySetter)
            return false;

        std::string _msg = player->GetName() + " has unlocked the obelisk, talk to me to set the level!";
        go->MonsterTextEmote(_msg.c_str(), nullptr, true);
        keySetter = player->GetGUID();
        player->PlayerTalkClass->ClearMenus();
        GossipHello(player, false);
        return true;
    }

    bool GossipHello(Player* player, bool /*reportUse*/) override
    {
        if (sWorld->getBoolConfig(CONFIG_MYTHIC_ENABLE) != true)
            return true;

        player->PlayerTalkClass->ClearMenus();
        if (!keySetter)
            player->PrepareQuestMenu(go->GetGUID());

        if (player->GetGUID() == keySetter)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Set mythic key level.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1,
                "Enter key level.\nDo not press enter, press accept button.", 0, true);

        SendGossipMenuFor(player, player->GetGossipTextId(go), go->GetGUID());
        return true;
    }

    bool GossipSelectCode(Player* player, uint32 menuId, uint32 gossipId, const char* text) override
    {
        if (instance->IsMythicTimerRunning())
            return false;

        player->PlayerTalkClass->ClearMenus();
        player->PlayerTalkClass->SendCloseGossip();

        std::string sText{ text };
        if (!utf8::is_valid(sText.begin(), sText.end()))
            return false;

        try
        {
            keyLevel = std::stoi(text);
        }
        catch (std::invalid_argument& e)
        {
        }
        catch (std::out_of_range& e)
        {
        }

        if (!keyLevel)
        {
            player->PlayerTalkClass->SendCloseGossip();
            player->Whisper("Provided key level is not valid.", 0, player->GetGUID());
            return false;
        }
        else
        {
            if ( static_cast<uint32>(keyLevel) > player->GetMythicKeyLevel() + 1 )
            {
                player->PlayerTalkClass->SendCloseGossip();
                player->Whisper( "Provided key level is not valid.", 0, player->GetGUID() );
                return false;
            }
        }

        go->SetPhaseMask(2, true);

        task.Schedule(1ms, [this](TaskContext func)
        {
            uint32 _count = 3 - func.GetRepeatCounter();
            std::string _time = std::to_string(_count);
            if (func.GetRepeatCounter() < 3)
            {
                Player* player = ObjectAccessor::GetPlayer(*go, keySetter);
                if (player)
                {
                    if (!func.GetRepeatCounter())
                    {
                        std::string _mythicStart = "Mythic starts in " + _time + "!";
                        player->MonsterTextEmote(_mythicStart.c_str(), nullptr, true);
                    }
                    else
                        player->MonsterTextEmote(_time.c_str(), nullptr, true);

                    func.Repeat(1s);
                }
            }
            else
            {
                Player* player = ObjectAccessor::GetPlayer(*go, keySetter);
                if (player)
                    player->MonsterTextEmote("GO!", nullptr, true);

                StartMythic();
            }
        });

        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        task.Update(diff);
    }

    void StartMythic()
    {
        Player* player = ObjectAccessor::GetPlayer(*go, keySetter);
        if (!player)
            return;

        instance->SetMythicKeyLevel(keyLevel, player);
        instance->ToggleMythicTimers(true);
        if (player->GetGroup())
        {
            for (const Group::MemberSlot& slot : player->GetGroup()->GetMemberSlots())
            {
                Player* gpPlayer = ObjectAccessor::GetPlayer(*go, slot.guid);
                if (!gpPlayer || gpPlayer == player || gpPlayer->IsDuringRemoveFromWorld() ||
                    !gpPlayer->IsInWorld())
                    continue;

                if (gpPlayer->FindMap() != gpPlayer->FindMap())
                    continue;

                gpPlayer->NearTeleportTo(player->GetPosition());
            }
        }
    }

private:
    InstanceScript* instance;
    uint64 keySetter;
    uint32 keyLevel;
    TaskScheduler task;
};

constexpr uint32 SPELL_MYTHIC_CC_REMOVAL_DISPEL_VISUAL{ 24997 };
class spell_mythic_remove_cc_aura : public AuraScript
{
    PrepareAuraScript(spell_mythic_remove_cc_aura);

    void HandlePeriodic(AuraEffect* aurEff)
    {
        Creature* target = GetCaster() ? GetCaster()->ToCreature() : nullptr;
        if (target && target->m_mythicScaling)
        {
            auto _affixCooldown = target->m_mythicScaling->HasAffixCooldown(Mythic::AFFIX_BREAK_CC_IN_COMBAT);
            bool _chance = roll_chance_i(10);
            if (!_affixCooldown && target->IsUnderCrowdControl() && _chance)
            {
                target->RemoveAurasDueToDamage(0x00080002, 1);
                target->RemoveAurasWithMechanic((MECHANIC_POLYMORPH | MECHANIC_ROOT | MECHANIC_FEAR | MECHANIC_ROOT |
                    MECHANIC_BANISH | MECHANIC_SHACKLE | MECHANIC_STUN | MECHANIC_SLEEP));

                target->m_mythicScaling->AddAffixCooldown(1, 25000);
                target->CastSpell(target, SPELL_MYTHIC_CC_REMOVAL_DISPEL_VISUAL, true);
            }
        }
    }

    void Register() override
    {
        OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_mythic_remove_cc_aura::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

class spell_mythic_buff_nearby_on_death : public SpellScript
{
    PrepareSpellScript(spell_mythic_buff_nearby_on_death);

    void FilterTarget(std::list<WorldObject*>& _targets)
    {
        _targets.remove_if([](WorldObject* obj) -> bool
        {
            Unit* unit = obj->ToUnit();
            if (!unit)
                return true;

            Creature* creature = unit->ToCreature();
            if (!creature)
                return true;

            if (IS_PLAYER_GUID(creature->GetGUID()) || creature->IsPet() || creature->IsGuardian()
                || creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC))
                return true;

            if (creature->IsDungeonBoss() || creature->isWorldBoss())
                return true;
            return false;
        });
    }

    void HandleHit(SpellEffIndex /*eff*/)
    {
        Unit* target = GetHitUnit();
        if (!target)
            return;

        target->CastSpell(target, Mythic::SPELL_MYTHIC_BUFF, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_mythic_buff_nearby_on_death::FilterTarget, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
        OnEffectHitTarget += SpellEffectFn(spell_mythic_buff_nearby_on_death::HandleHit, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

struct npc_mythic_crystals_trigger : public ScriptedAI
{
    npc_mythic_crystals_trigger(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _whisped = false;
    }

    virtual void DoAction(int32 param) override
    {
        if (param == 1)
        {
            for (uint32 i = 0; i < Mythic::MYTHIC_CRYSTALS_EVENT_COUNT; ++i)
            {
                uint32 gobId = Mythic::MYTHIC_CRYSTALS_IDS[i];
                Optional<Position> position = sMythicMgr.GetEventCrystalPosition(me->GetInstanceScript()->instance->GetId(), i);
                if (!position)
                    continue;
                if (GameObject* crystal = me->SummonGameObject(gobId, position.get(), 0.f, 0.f, 0.f, 0.f, 0))
                    _crystals.push_back(crystal->GetGUID());
            }
        }
        else if (param == 2)
        {
            for (auto& guid : _crystals)
            {
                if (GameObject* crystal = sObjectAccessor->GetGameObject(*me, guid))
                {
                    crystal->AI()->DoAction(1);
                    Position pos = crystal->GetPosition();
                    if (crystal->GetEntry() == 302100 || crystal->GetEntry() == 302101)
                        pos.RelocateOffset({ 0.f, 0.f, 4.f });

                    if (Creature* trigger = me->SummonCreature(WORLD_TRIGGER, pos, TempSummonType::TEMPSUMMON_TIMED_DESPAWN, 5000))
                        trigger->CastSpell(trigger, 35245, true);

                    _scheduler.Schedule(1500ms, [&](TaskContext func)
                    {
                        if (GameObject* crystal = sObjectAccessor->GetGameObject(*me, guid))
                            crystal->SetLootState(LootState::GO_JUST_DEACTIVATED);
                    });
                }
            }
        }
        else if (param == 3)
        {
            for (auto& ref : me->GetInstanceScript()->instance->GetPlayers())
            {
                Player* player = ref.GetSource();
                if (!player)
                    continue;

                me->MonsterWhisper("Wrong order. Try again", player);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update();
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (_whisped)
            return;
        if (!who->IsPlayer() || who->ToPlayer()->IsGameMaster() || me->GetExactDist2d(who) >= 10.f)
            return;
        for (auto& ref : me->GetInstanceScript()->instance->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player)
                continue;

            me->MonsterWhisper("Pick the correct order to fulfill your destiny!", player);
        }

        _whisped = true;
    }

private:
    std::vector<uint64> _crystals;
    bool _whisped;
    TaskScheduler _scheduler;
};

struct go_mythic_event_crystal : GameObjectAI
{
    go_mythic_event_crystal(GameObject* go) : GameObjectAI(go)
    {
        instance = go->GetInstanceScript();
        ASSERT(instance);
    }

    bool GossipHello(Player* player, bool reportUse) override
    {
        if (!instance->IsMythic() || reportUse)
            return false;

        Mythic::MythicEventInfo::CrystalsEvent& info = instance->GetMythicEventInfo().crystalsEvent;

        if (info.CrystalsClicked >= Mythic::MYTHIC_CRYSTALS_EVENT_COUNT)
            return false;

        if (info.GetCorrectGobjectIdAtPosition(info.CrystalsClicked++) == go->GetEntry())
        {
            if (info.CrystalsClicked == info.CorrectOrder.size())
            {
                if (Creature* trigger = go->FindNearestCreature(Mythic::MYTHIC_CRYSTALS_NPC_ID, 15.f))
                {
                    trigger->AI()->DoAction(2);
                    instance->HandleMythicEventEnd(Mythic::MythicEventType::MYTHIC_EVENT_CRYSTALS);
                }
            }
            player->CastSpell(player, 63295, true); // Visual
        }
        else
        {
            player->CastSpell(player, 24199, true); // Knockback
            info.CrystalsClicked = 0;
            if (Creature* trigger = go->FindNearestCreature(Mythic::MYTHIC_CRYSTALS_NPC_ID, 15.f))
            {
                trigger->AI()->DoAction(3);
            }
        }

        return true;
    }

    void DoAction(int32 param) override
    {
        if (param == 1)
        {
            go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
        }
    }


private:
    InstanceScript* instance;
};

enum LostNpcsEvent
{
    ITEM_LOST_NPCS_EVENT            = 18565,
    SPELL_LOST_NPCS_EVENT           = 70522,
    SPELL_LOST_NPCS_EVENT_VISUAL    = 34349,
};

struct npc_mythic_lost_npcs_main : ScriptedAI
{
    npc_mythic_lost_npcs_main(Creature* creature) : ScriptedAI(creature)
    {
        instance = me->GetInstanceScript();
        ASSERT(instance);
        _npcsSpawned = false;
        _playerGUID = 0;
    }

    void sGossipHello(Player* player) override
    {
        if ( ( _playerGUID && _playerGUID == player->GetGUID() ) || ( !_playerGUID && player->GetGroup() && player->GetGroup()->IsLeader(player->GetGUID()) ) )
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "I will help you.", GOSSIP_SENDER_MAIN, 1);

        uint32 gossipId = 0;
        switch (me->GetMapId())
        {
            case 576: // Nexus
                gossipId = 1210000;
                break;
            case 542: // Blood Furnance
                gossipId = 1210001;
                break;
            case 209: // Zul'Farrak
                gossipId = 1210002;
                break;
        }

        player->SEND_GOSSIP_MENU(gossipId, me->GetGUID());
    }

    void sGossipSelect(Player* player, uint32 /*sender*/, uint32 /*action*/)
    {
        player->CLOSE_GOSSIP_MENU();
        if (!_npcsSpawned)
        {
            _npcsSpawned = true;
            _playerGUID = player->GetGUID();

            SpawnNpcs();
        }
        uint32 itemId = 0;
        switch (me->GetMapId())
        {
            case 576: // Nexus
                itemId = 18565;
                break;
            case 542: // Blood Furnance
                itemId = 5515;
                break;
            case 209: // Zul'Farrak
                itemId = 3686;
                break;
        }

        if (!player->HasItemCount(itemId))
            player->AddItem(itemId, 1);
    }

    void SpawnNpcs()
    {
        Mythic::MythicEventInfo::LostNpcsEvent& info = instance->GetMythicEventInfo().lostNpcsEvent;
        uint32 lostNPCEntry = sMythicMgr.GetEventLostNpcEntry(me->GetMapId(), 1);

        for (uint32 i = 0; i < Mythic::MYTHIC_LOST_NPCS_TO_FOUND; ++i)
        {
            uint32 npcId = info.LostNpcs[i];
            Optional<Position> pos = sMythicMgr.GetEventLostNpcPosition(me->GetMapId(), npcId);
            if (!pos)
                continue;

            if (Creature* npc = me->SummonCreature(lostNPCEntry, pos.get()))
            {
                _lostNpcs.push_back(npcId);
                npc->AI()->SetData(0, npcId);
            }
        }

        ScheduleNextTip();
    }

    bool GiveTip(uint32 npcId)
    {
        if (std::find(_lostNpcs.begin(), _lostNpcs.end(), npcId) == _lostNpcs.end())
            return false;

        for (auto&& ref : instance->instance->GetPlayers())
        {
            Player* player = ref.GetSource();

            if (!player || player->IsGameMaster())
                continue;
            Talk(npcId, player);
        }

        return true;
    }

    void ScheduleNextTip()
    {
        uint32 npcId = _lostNpcs.front();
        instance->GetMythicScheduler().Schedule(10s, [&, npcId](TaskContext func)
        {
            if (GiveTip(npcId))
                func.Repeat(180s);
        });
    }

    void SetData(uint32 id, uint32 value) override
    {
        if (id == 0)
        {
            auto itr = std::find(_lostNpcs.begin(), _lostNpcs.end(), value);
            if (itr == _lostNpcs.end())
                return;
            _lostNpcs.erase(itr);

            if (!_lostNpcs.empty())
                ScheduleNextTip();
        }
    }

private:
    InstanceScript* instance;
    bool _npcsSpawned;
    TaskScheduler _scheduler;
    uint64 _playerGUID;
    std::list<uint32> _lostNpcs;
};

struct npc_mythic_lost_npcs : ScriptedAI
{
    npc_mythic_lost_npcs(Creature* creature) : ScriptedAI(creature)
    {
        instance = me->GetInstanceScript();
        ASSERT(instance);
    }

    void Reset() override
    {
        _found = false;
        switch (me->GetMapId())
        {
            case 209: // Zul'Farrak
            case 576: // Nexus
                me->GetMotionMaster()->MoveRandom(10.f);
                break;
            case 542: // Blood Furnance
                me->SetUInt32Value(UNIT_NPC_EMOTESTATE, ANIM_ATTACK2H);
                break;
        }
        me->SetVisible(false);
        _myId = 0;
    }

    void sGossipHello(Player* player) override
    {
        if (_found)
            return;
        Mythic::MythicEventInfo::LostNpcsEvent& info = instance->GetMythicEventInfo().lostNpcsEvent;

        if (++info.FoundNpcs == Mythic::MYTHIC_LOST_NPCS_TO_FOUND)
            instance->HandleMythicEventEnd(Mythic::MythicEventType::MYTHIC_EVENT_LOST_NPCS);

        switch (me->GetMapId())
        {
            case 576: // Nexus
                me->MonsterTextEmote("Wisp gazes at $n with thankfully sight and disappears.", player);
                break;
            case 542: // Blood Furnance
                me->SetUInt32Value(UNIT_NPC_EMOTESTATE, ANIM_EMOTE_SALUTE);
                break;
            case 209: // Zul'Farrak
                me->CastSpell(me, 47004, true);
                break;
        }
        me->DespawnOrUnsummon(2s);

        if (Unit* summoner = me->GetSummoner())
            summoner->GetAI()->SetData(0, _myId);

        _found = true;
        player->CLOSE_GOSSIP_MENU();
    }

    void SetData(uint32 id, uint32 value) override
    {
        if (id == 0)
            _myId = value;
    }

    void DoAction(int32 param) override
    {
        if (param == 1)
        {
            me->SetVisible(true);
            _scheduler.Schedule(10s, [&](TaskContext func)
            {
                me->SetVisible(false);
            });
        }
    }

    void UpdateAI(uint32 diff) override
    {
        ScriptedAI::UpdateAI(diff);
        _scheduler.Update(diff);
    }

private:
    bool _found;
    InstanceScript* instance;
    TaskScheduler _scheduler;
    uint32 _myId;
};

struct item_mythic_lost_npc : ItemScript
{
public:
    item_mythic_lost_npc() : ItemScript("item_mythic_lost_npc") { }

    bool OnUse(Player* player, ItemRef const& item, SpellCastTargets const& targets)
    {
        if (player->HasSpellCooldown(SPELL_LOST_NPCS_EVENT))
        {
            if (const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(SPELL_LOST_NPCS_EVENT))
                Spell::SendCastResult(player, spellInfo, 0, SPELL_FAILED_NOT_READY);
            return true;
        }

        player->CastSpell(player, SPELL_LOST_NPCS_EVENT_VISUAL, true);

        std::list<Creature*> creatureList;
        player->GetCreatureListWithEntryInGrid(creatureList, sMythicMgr.GetEventLostNpcEntry(player->GetMapId(), 1), 30.f);

        for(auto& lostNpc : creatureList)
            lostNpc->AI()->DoAction(1);

        return false;
    }
};

enum MythicAlchemistEvent
{
    SPELL_ALCHEMIST_EVENT = 16872,
    SPELL_ALCHEMIST_EVENT_FAIL = 58579,
    SPELL_ALCHEMIST_EVENT_SUCCESS = 8212,
    ITEM_ALCHEMIST_EVENT  = 40232,
    GOSSIP_ALCHEMIST_MAIN = 1210010,
    GOSSIP_ALCHEMIST_SUMMON = 1210011,
    NPC_ALCHEMIST_SUMMON = 230021,
};

struct npc_mythic_alchemist : ScriptedAI
{
    npc_mythic_alchemist(Creature* creature) : ScriptedAI(creature)
    {
        _leader = 0;
    }

    void sGossipHello(Player* player) override
    {
        if (!_leader)
        {
            if (Group* group = player->GetGroup())
                _leader = group->GetLeaderGUID();
        }

        if (player->GetGUID() == _leader && !player->HasItemCount(ITEM_ALCHEMIST_EVENT))
            player->AddItem(ITEM_ALCHEMIST_EVENT, 1);

        player->SEND_GOSSIP_MENU(GOSSIP_ALCHEMIST_MAIN, me->GetGUID());
    }

private:
    uint64 _leader;
};

struct go_mythic_event_alchemist_book : GameObjectAI
{
    go_mythic_event_alchemist_book(GameObject* go) : GameObjectAI(go)
    {
        instance = go->GetInstanceScript();
        ASSERT(instance);
        _found = false;
    }

    bool GossipHello(Player* player, bool reportUse) override
    {
        if (!instance->IsMythic() || _found)
            return false;

        if (Creature* creature = go->SummonCreature(WORLD_TRIGGER, go->GetPosition(), TempSummonType::TEMPSUMMON_TIMED_DESPAWN, 22000))
        {
            creature->SetObjectScale(5.f);
            creature->CastSpell(creature, 50247);
        }

        _scheduler.Schedule(20s, [&](TaskContext func)
        {
            go->SetPhaseMask(2, true);
        });

        _found = true;
        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        GameObjectAI::UpdateAI(diff);
        _scheduler.Update(diff);
    }

private:
    InstanceScript* instance;
    TaskScheduler _scheduler;
    bool _found;
};

struct item_mythic_alchemist_whistle : ItemScript
{
public:
    item_mythic_alchemist_whistle() : ItemScript("item_mythic_alchemist_whistle") { }

    bool OnUse(Player* player, ItemRef const& item, SpellCastTargets const& targets)
    {
        if (player->HasSpellCooldown(SPELL_ALCHEMIST_EVENT))
        {
            SendSpellCastResult(player, SPELL_FAILED_NOT_READY);
            return true;
        }

        InstanceScript* instance = player->GetInstanceScript();
        if (!instance)
        {
            SendSpellCastResult(player, SPELL_FAILED_NOT_HERE);
            return true;
        }

        if (!instance->IsMythic())
        {
            SendSpellCastResult(player, SPELL_FAILED_NOT_HERE);
            return true;
        }

        if (!instance->GetMythicEventInfo().alchemistEvent.initialized)
        {
            SendSpellCastResult(player, SPELL_FAILED_NOT_HERE);
            return true;
        }

        Position pos;
        player->GetNearPosition(pos, 5.f, 0.f);
        pos.RelocateOffset({ 0.f, 0.f, 0.f, -M_PI });
        player->SummonCreature(NPC_ALCHEMIST_SUMMON, pos, TEMPSUMMON_TIMED_DESPAWN, 2 * MINUTE * IN_MILLISECONDS);

        return false;
    }

    void SendSpellCastResult(Player* player, SpellCastResult result)
    {
        if (const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(SPELL_ALCHEMIST_EVENT))
            Spell::SendCastResult(player, spellInfo, 0, result);
    }

};

class npc_mythic_alchemist_summon : public CreatureScript
{
public:
    npc_mythic_alchemist_summon() : CreatureScript("npc_mythic_alchemist_summon")
    {
        _correctAnswer = 0;
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (player->GetGroup() && player->GetGroup()->IsLeader(player->GetGUID()))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "<Start preparing Might of the Ancients elixir>", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);

        player->SEND_GOSSIP_MENU(GOSSIP_ALCHEMIST_SUMMON, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 sender, uint32 actionId) override
    {
        Mythic::MythicEventInfo::AlchemistEvent& _eventInfo = creature->GetInstanceScript()->GetMythicEventInfo().alchemistEvent;
        std::vector<uint32> order{ 0, 1, 2 };
        Trinity::Containers::RandomShuffle(order);

        player->PlayerTalkClass->ClearMenus();

        switch (actionId)
        {
            case GOSSIP_ACTION_INFO_DEF:
            {
                for (uint32 i = 0; i < 3; ++i)
                {
                    std::ostringstream os;
                    os << "Let\'s melt " << _eventInfo.PossibleAnswers[0][order[i]] << " of Dream Dust with " << _eventInfo.PossibleAnswers[1][order[i]] << " glasses of Ethereal Oil and add " << _eventInfo.PossibleAnswers[2][order[i]] << " pieces of Sungrass.";
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, os.str(), i, GOSSIP_ACTION_INFO_DEF + 1);

                    if (order[i] == 0)
                        _correctAnswer = i;
                }

                player->SEND_GOSSIP_MENU(GOSSIP_ALCHEMIST_SUMMON + 1, creature->GetGUID());
                break;
            }
            case GOSSIP_ACTION_INFO_DEF + 1:
            {
                if (!CheckCorrectAnswer(sender, creature))
                {
                    player->CLOSE_GOSSIP_MENU();
                    return true;
                }

                for (uint32 i = 0; i < 3; ++i)
                {
                    std::ostringstream os;
                    os << "Let\'s add "<< _eventInfo.PossibleAnswers[3][order[i]] << " powdered Large Fangsand " << _eventInfo.PossibleAnswers[4][order[i]] <<" chopped Small Flame Sacs, also you can add "<< _eventInfo.PossibleAnswers[5][order[i]] <<" small pinches of Dream Dust to make it more clear.";
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, os.str(), i, GOSSIP_ACTION_INFO_DEF + 2);

                    if (order[i] == 0)
                        _correctAnswer = i;
                }

                player->SEND_GOSSIP_MENU(GOSSIP_ALCHEMIST_SUMMON + 2, creature->GetGUID());
                break;
            }
            case GOSSIP_ACTION_INFO_DEF + 2:
            {
                if (!CheckCorrectAnswer(sender, creature))
                {
                    player->CLOSE_GOSSIP_MENU();
                    return true;
                }

                for (uint32 i = 0; i < 3; ++i)
                {
                    std::ostringstream os;
                    os << "Now we have to wait a moment when small sparkles will be appearing at the top of the mixture, then you can add "<< _eventInfo.PossibleAnswers[6][order[i]] <<" pieces of Arthas\' Tears ... and that\'s it Might of the Ancients should be ready to taste!";
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, os.str(), i, GOSSIP_ACTION_INFO_DEF + 3);

                    if (order[i] == 0)
                        _correctAnswer = i;
                }

                player->SEND_GOSSIP_MENU(GOSSIP_ALCHEMIST_SUMMON + 3, creature->GetGUID());
                break;
            }
            case GOSSIP_ACTION_INFO_DEF + 3:
            {
                if (!CheckCorrectAnswer(sender, creature))
                {
                    player->CLOSE_GOSSIP_MENU();
                    return true;
                }

                creature->CastSpell(creature, SPELL_ALCHEMIST_EVENT_SUCCESS, true);
                creature->MonsterYell("Looks like the elixir is ready ... YES! I am feeling like a god ... I have to tell my master about my success. Thank you!", LANG_UNIVERSAL, nullptr);
                creature->SetSelectable(false);
                creature->DespawnOrUnsummon(5s);
                if (InstanceScript* instance = creature->GetInstanceScript())
                    instance->HandleMythicEventEnd(Mythic::MythicEventType::MYTHIC_EVENT_ALCHEMIST);
                player->CLOSE_GOSSIP_MENU();
                break;
            }
        }

        return false;
    }

    bool CheckCorrectAnswer(uint32 answer, Creature* creature)
    {
        if (answer != _correctAnswer)
        {
            creature->CastSpell(creature, SPELL_ALCHEMIST_EVENT_FAIL, true);
            Unit::Kill(creature, creature);
            return false;
        }
        return true;
    }

private:
    uint32 _correctAnswer;
    InstanceScript* _instance;
};

enum DrogonAffix
{
    NPC_DROGON_AFFIX_TARGET = 91701,
    SPELL_PLASMA_BLAST_DROGON = 62997,
    SPELL_PLASMA_BLAST_FIRENOVA = 20203,
    DROGON_SPAWN_SOUND_ID = 8290
};

struct npc_drogon_affix_mythic : public ScriptedAI
{
    npc_drogon_affix_mythic(Creature* creature) : ScriptedAI(creature), summons(me)
    {
        me->SetCanFly(true);
        me->SetDisableGravity(true);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType, SpellSchoolMask) override
    {
        damage = 0;
    }

    void Reset() override
    {
        me->PlayDistanceSound(DROGON_SPAWN_SOUND_ID);
        _triggerGUID = 0;
        scheduler.CancelAll();

        Position triggerPos = me->GetPosition();
        triggerPos.m_positionZ = me->GetMap()->GetWaterOrGroundLevel(me->GetPhaseMask(), triggerPos.m_positionX,
            triggerPos.m_positionY, triggerPos.m_positionZ);

        Creature* trigger = me->SummonCreature(NPC_DROGON_AFFIX_TARGET, triggerPos);
        if (!trigger)
        {
            me->DespawnOrUnsummon();
            return;
        }

        trigger->SetPassive();
        trigger->SetControlled(true, UNIT_STATE_ROOT);
        trigger->DespawnOrUnsummon(15s);
        _triggerGUID = trigger->GetGUID();

        scheduler.Schedule(500ms, [&](TaskContext func)
        {
            Creature* trigger = ObjectAccessor::GetCreature(*me, _triggerGUID);
            if (trigger)
            {
                CustomSpellValues val;
                val.AddSpellMod(SPELLVALUE_AURA_DURATION, 12000);
                val.AddSpellMod(SPELLVALUE_BASE_POINT0, 0);
                me->CastCustomSpell(SPELL_PLASMA_BLAST_DROGON, val, trigger);
            }
        });

        scheduler.Schedule(3500ms, [&](TaskContext func)
        {
            Creature* trigger = ObjectAccessor::GetCreature(*me, _triggerGUID);
            if (trigger)
            {
                CustomSpellValues val;
                val.AddSpellMod(SPELLVALUE_BASE_POINT0, 5000);
                val.AddSpellMod(SPELLVALUE_RADIUS_MOD, 30000);
                trigger->CastCustomSpell(SPELL_PLASMA_BLAST_FIRENOVA, val, (Unit*)nullptr, TRIGGERED_FULL_MASK);
            }

            func.Repeat(1500ms);
        });

        scheduler.Schedule(10s, [&](TaskContext func)
        {
            summons.DespawnAll();
            me->DespawnOrUnsummon();
        });
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
    }

    void AttackStart(Unit* /*who*/) override { }
    void EnterCombat(Unit* /*who*/) override { }
    void UpdateAI(uint32 diff) override { scheduler.Update(diff); }
private:
    SummonList summons;
    uint64 _triggerGUID;
    TaskScheduler scheduler;
};

enum DrudgeGhoulMythic
{
    SPELL_DRUDGE_GHOUL_SPAWN_ANIMATION = 69639
};

struct npc_drudge_ghoul_mythics : public ScriptedAI
{
    npc_drudge_ghoul_mythics(Creature* creature) : ScriptedAI(creature)
    {
        _initialized = false;
        me->SetReactState(REACT_PASSIVE);
        me->SetSelectable(false);
        me->SetImmuneToAll(true);
    }

    void DoZoneInCombatGhouls()
    {
        if (!me->CanHaveThreatList())
            return;

        Map* map = me->GetMap();
        if (!map->IsDungeon())                                  //use IsDungeon instead of Instanceable, in case battlegrounds will be instantiated
        {
            sLog->outDebug(LOG_FILTER_UNITS, "Drudge ghouls mythic call for map that isn't an instance (creature entry = %d)", me->GetTypeId() == TYPEID_UNIT ? me->ToCreature()->GetEntry() : 0);
            return;
        }

        // Skip creatures in evade mode
        if (!me->HasReactState(REACT_PASSIVE) && !me->GetVictim() && !me->IsInEvadeMode())
        {
            if (Unit* nearTarget = me->SelectNearestTarget(50.f))
                me->AI()->AttackStart(nearTarget);
        }

        if (!me->HasReactState(REACT_PASSIVE) && !me->GetVictim())
        {
            sLog->outDebug(LOG_FILTER_UNITS, "Drudge ghouls called for creature that has empty threat list (creature entry = %u)", me->GetEntry());
            return;
        }

        Map::PlayerList const& playerList = map->GetPlayers();

        if (playerList.isEmpty())
            return;

        for (Map::PlayerList::const_iterator itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            if (Player* player = itr->GetSource())
            {
                if (player->IsGameMaster() || me->GetDistance(player) > 50.f)
                    continue;

                if (player->IsAlive())
                {
                    me->SetInCombatWith(player);
                    player->SetInCombatWith(me);
                    me->AddThreat(player, 0.0f);
                }
            }
        }
    }

    bool CanAIAttack(Unit const* who) const override
    {
        return who && who->GetDistance(me) < 70.f;
    }

    void Reset() override
    {
        scheduler.CancelAll();

        if (!_initialized)
        {
            _initialized = true;
            scheduler.Schedule(5s, [&](TaskContext /*func*/)
            {
                me->SetReactState(REACT_AGGRESSIVE);
                me->SetSelectable(true);
                me->SetImmuneToAll(false);
                DoZoneInCombatGhouls();
            });
        }

        if (_initialized && (me->IsPassive() || !me->IsSelectable()))
        {
            me->SetReactState(REACT_AGGRESSIVE);
            me->SetSelectable(true);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

    void EnterCombat(Unit* who) override
    {
        enum GhoulSpells
        {
            SPELL_MYTHIC_GHOUL_CHOMP = 32906,
            SPELL_MYTHIC_GHOUL_BRAIN_BASH = 31046
        };

        scheduler.Schedule(randseconds(2, 5), [&](TaskContext func)
        {
            CustomSpellValues val;
            val.AddSpellMod(SPELLVALUE_BASE_POINT0, 150);
            Unit* target = me->GetVictim();
            if (target)
                me->CastCustomSpell(SPELL_MYTHIC_GHOUL_CHOMP, val, target);

            func.Repeat(randseconds(6, 9));
        });

        scheduler.Schedule(randseconds(6, 12), [&](TaskContext func)
        {
            CustomSpellValues val;
            val.AddSpellMod(SPELLVALUE_AURA_DURATION, 2000);
            Unit* target = me->GetVictim();
            if (target && !target->HasAura(SPELL_MYTHIC_GHOUL_BRAIN_BASH))
                me->CastCustomSpell(SPELL_MYTHIC_GHOUL_BRAIN_BASH, val, target);

            func.Repeat(randseconds(14, 18));
        });
    }

private:
    TaskScheduler scheduler;
    bool _initialized;
};

enum HolyBombSpells
{
    SPELL_HOLY_BOMB_VISUAL = 70785,
    SPELL_HOLY_BOMB_EXPLOSION_MYTHIC = 70786,
    SPELL_HOLY_BOMB_HOLY_WRATH = 69934,
    SPELL_HOLY_BOMB_STUN = 65208
};

struct npc_holy_bomb_mythic : public PassiveAI
{
    npc_holy_bomb_mythic(Creature* creature) : PassiveAI(creature)
    {
        _initialized = false;
        me->SetObjectScale(0.65f);
        me->SetRegeneratingHealth(false);
    }

    void EnterEvadeMode() override
    {
    }

    void Reset() override
    {
        scheduler.CancelAll();
        DoCastSelf(SPELL_HOLY_BOMB_VISUAL);

        scheduler.Schedule(6s, [&](TaskContext func)
        {
            DoCastSelf(SPELL_HOLY_BOMB_EXPLOSION_MYTHIC);

            CustomSpellValues val;
            val.AddSpellMod(SPELLVALUE_BASE_POINT0, Mythic::CalculateMythicValue(4500, 0.1f));
            val.AddSpellMod(SPELLVALUE_RADIUS_MOD, 20000);
            me->CastCustomSpell(SPELL_HOLY_BOMB_HOLY_WRATH, val, (Unit*)nullptr, TRIGGERED_FULL_MASK);

            func.Repeat(6s);
        });

        me->DespawnOrUnsummon(60s);
    }

    void SpellHitTarget(Unit* victim, SpellInfo const* spell) override
    {
        if (victim && spell->Id == SPELL_HOLY_BOMB_HOLY_WRATH)
        {
            CustomSpellValues val;
            val.AddSpellMod(SPELLVALUE_AURA_DURATION, 2500);
            victim->CastCustomSpell(SPELL_HOLY_BOMB_STUN, val, victim, TRIGGERED_FULL_MASK);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        scheduler.Update(diff);
    }

private:
    TaskScheduler scheduler;
    bool _initialized;
};

class spell_drogon_firenova_mythic : public SpellScript
{
    PrepareSpellScript(spell_drogon_firenova_mythic);

    void HandleHit(SpellEffIndex eff)
    {
        if (!GetCaster() || !GetHitUnit())
            return;

        bool const IsMythic = GetHitUnit()->GetInstanceScript() && GetHitUnit()->GetInstanceScript()->IsMythic();
        if (IsMythic)
        {
            float distance = GetHitUnit()->GetDistance2d(GetCaster()->GetPositionX(), GetCaster()->GetPositionY());

            uint32 realDamage = GetSpellValue() ? GetSpellValue()->EffectBasePoints[eff] : 0;
            float divider = 1.f;

            if (distance <= 5.f)
                divider = 0.95f;
            else if (distance > 5.f && distance < 10.f)
                divider = 0.90f;
            else if (distance > 10.f && distance < 15.f)
                divider = 0.80f;
            else if (distance > 15.f && distance < 20.f)
                divider = 0.70f;
            else
                divider = 0.68f;

            int32 damage = realDamage * divider;
            SetEffectValue(damage);
        }
    }

    void Register() override
    {
        OnEffectLaunchTarget += SpellEffectFn(spell_drogon_firenova_mythic::HandleHit, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class spell_broken_bones_mythic_avo : public AuraScript
{
    PrepareAuraScript(spell_broken_bones_mythic_avo);

    void HandleApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        InstanceScript* instance = target->GetInstanceScript();
        if (!instance || !instance->IsMythic())
            return;

        Player* player = target->ToPlayer();
        if (!player)
            return;

        WorldSession* session = player->GetSession();
        if (session)
            ChatHandler(session).SendSysMessage("Your avoidance has been reduced by spell Broken bones, tooltip will display you cannot block, but in reality your block/parry/dodge has been reduced by 20%");
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_broken_bones_mythic_avo::HandleApply, EFFECT_2,
            SPELL_AURA_LINKED, AURA_EFFECT_HANDLE_REAL);
    }
};

struct go_mythic_chest_spawn_in : public GameObjectAI
{
    go_mythic_chest_spawn_in(GameObject* go) : GameObjectAI(go)
    {
        _canUpdate = true;
    }

    void Reset() override
    {
        InstanceScript* instance = go->GetInstanceScript();
        if (!instance || !instance->IsMythic())
        {
            go->RemoveFromWorld();
            return;
        }

        task.Schedule(1s, [&](TaskContext func)
        {
            if (func.GetRepeatCounter() < 5)
            {
                Creature* trigger = go->SummonCreature(WORLD_TRIGGER, go->GetPosition());
                if (trigger)
                {
                    trigger->DespawnOrUnsummon(10s);
                    trigger->CastSpell(trigger, 62077, true);
                    trigger->CastSpell(trigger, 62075, true);
                }

                func.Repeat(2s);
            }
            else
                _canUpdate = false;
        });
    }

    void UpdateAI(uint32 diff) override
    {
        if (_canUpdate)
            task.Update(diff);
    }

private:
    bool _canUpdate;
    TaskScheduler task;
};

struct go_mythic_companion_guild_bank : GameObjectAI
{
    go_mythic_companion_guild_bank(GameObject* go)
        : GameObjectAI(go)
        , _summonerGUID(0)
    {
    }

    void SetGUID( uint64 guid, int32 /*data*/ ) override
    {
        _summonerGUID = guid;
    }

    bool CanBeSeen( Player const* seer ) override
    {
        return seer->GetGUID() == go->GetOwnerGUID();
    }

private:
    uint64 _summonerGUID = 0;
};

class spell_mythic_companion_summon : public SpellScript
{
    PrepareSpellScript(spell_mythic_companion_summon);

    void HandleHit(SpellEffIndex effIndex)
    {
        PreventHitEffect(effIndex);
        if (!GetCaster())
            return;

        Player* player = GetCaster()->ToPlayer();
        if (!player)
            return;

        if (!player->IsMythicCompanionLoaded())
            return;

        Creature* companion = ObjectAccessor::GetCreature(*player, player->GetMythicCompanionGUID());
        if (companion)
        {
            companion->DespawnOrUnsummon();
            return;
        }

        //! 41 equals to mini thors summon property
        SummonPropertiesEntry const* prop = sSummonPropertiesStore.LookupEntry(41U);
        TempSummon* summon                = player->GetMap()->SummonCreature(Mythic::MYTHIC_COMPANION_ENTRY, player->GetPosition(), prop, 0U, player, 0U, 0U);
        if (!summon)
            return;

        summon->setFaction(35U);
        summon->SetLevel(player->m_companionData->m_mythicCompanionLevel);
        player->SetMythicCompanionGUID(summon->GetGUID());
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_mythic_companion_summon::HandleHit, EFFECT_0, SPELL_EFFECT_SUMMON);
    }
};

struct go_mythic_quest_obelisk : GameObjectAI
{
    go_mythic_quest_obelisk(GameObject* gob)
        : GameObjectAI(gob)
    {
    }

    bool GossipHello(Player* player, bool reportUse) override
    {
        if (!reportUse)
            return false;

        if (go->GetGoType() == GAMEOBJECT_TYPE_QUESTGIVER)
            player->PrepareQuestMenu(go->GetGUID());

        if (player->GetQuestStatus(26037) == QUEST_STATUS_REWARDED && !player->HasItemCount(41125))
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Restore Mythic Companion Whistle", GOSSIP_SENDER_MAIN, 1);
        player->SEND_GOSSIP_MENU(DEFAULT_GOSSIP_MESSAGE, go->GetGUID());

        return true;
    }


    bool GossipSelect(Player* player, uint32 /*sender*/, uint32 action)
    {
        if (player->GetQuestStatus(26037) != QUEST_STATUS_REWARDED || player->HasItemCount(41125))
            return false;
        player->AddItem(41125, 1);
        player->PlayerTalkClass->SendCloseGossip();
        return true;
    }
};


void AddSC_mythic_scripts()
{
    new npc_mythic_handler();
    RegisterGameObjectAI(go_mythic_crystal);
    RegisterAuraScript(spell_mythic_remove_cc_aura);
    RegisterSpellScript(spell_mythic_buff_nearby_on_death);
    RegisterGameObjectAI(go_mythic_event_crystal);
    RegisterGameObjectAI( go_mythic_companion_guild_bank );
    RegisterCreatureAI(npc_mythic_crystals_trigger);
    RegisterCreatureAI(npc_mythic_lost_npcs_main);
    RegisterCreatureAI(npc_mythic_lost_npcs);
    new item_mythic_lost_npc();
    RegisterCreatureAI(npc_mythic_alchemist);
    new npc_mythic_alchemist_summon();
    RegisterGameObjectAI(go_mythic_event_alchemist_book);
    new item_mythic_alchemist_whistle();
    RegisterCreatureAI(npc_drogon_affix_mythic);
    RegisterCreatureAI(npc_drudge_ghoul_mythics);
    RegisterCreatureAI(npc_holy_bomb_mythic);
    RegisterSpellScript(spell_drogon_firenova_mythic);
    RegisterAuraScript(spell_broken_bones_mythic_avo);
    RegisterGameObjectAI(go_mythic_chest_spawn_in);
    RegisterSpellScript(spell_mythic_companion_summon);
    RegisterGameObjectAI(go_mythic_quest_obelisk);
}
