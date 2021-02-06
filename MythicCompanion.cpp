#include "CreatureOutfits.h"
#include "GameObjectAI.h"
#include "Guild.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "MythicAI.h"
#include "Player.h"
#include "AuctionHouseMgr.h"
#include "Transmogrification.h"

enum CompanionMainActions
{
    ACTION_APPEARANCE = 0,
    ACTION_TOOLS,
    ACTION_EXPERIENCE,
    ACTION_REWARDS_CURRENCY,
    ACTION_REWARD_MYTHIC,
    ACTION_SHOW_TRANSMOGS,
    ACTION_TRANSMOG,
    ACTION_BUY_ITEM_STORE,
    ACTION_MYTHIC_TELEPORTER
};

enum CompanionActions
{
    ACTION_RACE = 100,
    ACTION_GENDER = 101,
    ACTION_HEAD = 102,
    ACTION_SHOULDERS = 103,
    ACTION_BODY = 104,
    ACTION_CHEST = 105,
    ACTION_WAIST = 106,
    ACTION_LEGS = 107,
    ACTION_FEET = 108,
    ACTION_WRISTS = 109,
    ACTION_HANDS = 110,
    ACTION_BACK = 111,
    ACTION_MAINHAND = 112,
    ACTION_OFFHAND = 113,
    ACTION_RANGED = 114
};

enum CompanionToolActions
{
    ACTION_BANK = 200,
    ACTION_GUIDLBANK,
    ACTION_AUCTION,
    ACTION_MASS_RESS,
    ACTION_GO_TO_STORE_DEFAULT,
    ACTION_SHOW_COMPANION_TOOLS_STORE,
    ACTION_SHOW_STORE,
    ACTION_BUY_BANK,
    ACTION_BUY_GUILD_BANK,
    ACTION_BUY_AUCTION_HOUSE,
    ACTION_BUY_MASS_RESSURECT,
    ACTION_BUY_GOBLIN_RACE,
    ACTION_BUY_TAUNKA_RACE,
    ACTION_RETURN,

    ACTION_MAX,
};

enum CompanionToolFakeSpells
{
    SPELL_COMPANION_BANK_MARKER = 120000,
    SPELL_COMPANION_GUILD_BANK_MARKER,
    SPELL_COMPANION_AUCTION_HOUSE_MARKER
};

enum CompanionSenderTypes
{
    SENDER_CLOSE_AFTER_USE = 1000
};

struct CompanionEquipmentData
{
    uint32 levelRequirement = 0;
    EquipmentSlots slot;
    std::string slotName = "";
    uint32 defaultItemEntry = 0;
    CompanionActions action = ACTION_RACE;
};

std::array<CompanionEquipmentData, 15> _companionEquipmentData
{ {
    { 1, static_cast<EquipmentSlots>(0),    "Race",         0, ACTION_RACE },
    { 1, static_cast<EquipmentSlots>(0),    "Gender",       0, ACTION_GENDER },
    { 1, EQUIPMENT_SLOT_HEAD,               "Head",         0, ACTION_HEAD },
    { 1, EQUIPMENT_SLOT_SHOULDERS,          "Shoulders",    0, ACTION_SHOULDERS },
    { 1, EQUIPMENT_SLOT_BODY,               "Shirt",        0, ACTION_BODY },
    { 1, EQUIPMENT_SLOT_CHEST,              "Chest",        0, ACTION_CHEST },
    { 1, EQUIPMENT_SLOT_WAIST,              "Waist",        0, ACTION_WAIST },
    { 1, EQUIPMENT_SLOT_LEGS,               "Pants",        0, ACTION_LEGS},
    { 1, EQUIPMENT_SLOT_FEET,               "Boots",        0, ACTION_FEET },
    { 1, EQUIPMENT_SLOT_WRISTS,             "Wrists",       0, ACTION_WRISTS },
    { 1, EQUIPMENT_SLOT_HANDS,              "Hands",        0, ACTION_HANDS },
    { 1, EQUIPMENT_SLOT_BACK,               "Cloak",        0, ACTION_BACK },
    { 1, EQUIPMENT_SLOT_MAINHAND,           "Main Hand",    0, ACTION_MAINHAND },
    { 1, EQUIPMENT_SLOT_OFFHAND,            "Off hand",     0, ACTION_OFFHAND },
    { 1, EQUIPMENT_SLOT_RANGED,             "Ranged",       0, ACTION_RANGED }
} };

enum MythicCompanionTexts
{
    NPC_TEXT_COMPANION_GREET = 91501,
    NPC_TEXT_APPEARNCE = 91502,
    NPC_TEXT_OTHER = 91503, // unusued, dont delete
    NPC_TEXT_NO_TOOLS_OR_REWARDS = 91504,
    NPC_TEXT_REWARD = 91505,
    NPC_TRANSMOGS_LIST = 91506,

    COMPANION_TEXT_HEAD = 91507,
    COMPANION_TEXT_SHOULDERS,
    COMPANION_TEXT_SHIRT,
    COMPANION_TEXT_CHEST,
    COMPANION_TEXT_BELT,
    COMPANION_TEXT_LEGS,
    COMPANION_TEXT_FEET,
    COMPANION_TEXT_WRIST,
    COMPANION_TEXT_GLOVES,
    COMPANION_TEXT_CAPE,
    COMPANION_TEXT_MAIN_WEAPON,
    COMPANION_TEXT_OFF_HAND,
    COMPANION_TEXT_RANGED,
    COMPANION_TEXT_TABARD,

    COMPANION_TEXT_TOOLS,
    COMPANION_TEXT_CURRENT_LVL,
    COMPANION_TEXT_REWARDS,
    COMPANION_TEXT_STORE,
    COMPANION_TEXT_NO_TOOLS,

    COMPANION_TEXT_MYTHIC_TELEPORTS = 91530
};

enum MythicCompanionGObjects
{
    GOB_MYTHIC_GUILD_BANK = 303000
};

struct CompanionGreetData
{
    Races CompanionRace;
    Gender CompanionGender;
    uint32 SoundId;
};

std::vector<CompanionGreetData> _companionDefaultVoiceData{
    { RACE_HUMAN, GENDER_MALE, 5971u },
    { RACE_HUMAN, GENDER_FEMALE, 5983u },

    { RACE_NIGHTELF, GENDER_MALE, 5993u },
    { RACE_NIGHTELF, GENDER_FEMALE, 6005u },

    { RACE_DRAENEI, GENDER_MALE, 9761u },
    { RACE_DRAENEI, GENDER_FEMALE, 9749u },

    { RACE_DWARF, GENDER_MALE, 5910u },
    { RACE_DWARF, GENDER_FEMALE, 5916u },

    { RACE_GNOME, GENDER_MALE, 5922u },
    { RACE_GNOME, GENDER_FEMALE, 5937u },

    { RACE_ORC, GENDER_MALE, 6018u },
    { RACE_ORC, GENDER_FEMALE, 6033u },

    { RACE_TAUREN, GENDER_MALE, 6008u },
    { RACE_TAUREN, GENDER_FEMALE, 6055u },

    { RACE_TROLL, GENDER_MALE, 5944u },
    { RACE_TROLL, GENDER_FEMALE, 5955u },

    { RACE_UNDEAD_PLAYER, GENDER_MALE, 6039u },
    { RACE_UNDEAD_PLAYER, GENDER_FEMALE, 6052u },

    { RACE_BLOODELF, GENDER_MALE, 9737u },
    { RACE_BLOODELF, GENDER_FEMALE, 9733u },

    { RACE_TAUNKA, GENDER_MALE, 6011u },

    { RACE_GOBLIN, GENDER_MALE, 5958u },
    { RACE_GOBLIN, GENDER_FEMALE, 5967u }
};

constexpr uint32 SPELL_LEVEL_UP_COMPANION_VISUAL{ 47292 };
constexpr uint32 SPELL_COMPANION_TRANSFORM_VISUAL{ 24085 };
constexpr uint32 TRANSMOGS_PER_PAGE{ 10 };
struct npc_mythic_custom_companion : public MythicCompanionAI
{
    npc_mythic_custom_companion(Creature* creature)
        : MythicCompanionAI(creature)
        , _summonerGUID(0)
    {
        me->SetFlag(UNIT_NPC_FLAGS, (UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_BANKER | UNIT_NPC_FLAG_AUCTIONEER));
        me->SetFlag( UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC | UNIT_FLAG_IMMUNE_TO_PC );
        me->SetObjectScale(DEFAULT_WORLD_OBJECT_SIZE * 1.6f);
    }

    void IsSummonedBy(Unit* summoner) override
    {
        Player* player = summoner->ToPlayer();
        if (!player)
        {
            me->DespawnOrUnsummon();
            return;
        }

        _summonerGUID = player->GetGUID();
        SetMyOutfit();

        CritterAI::IsSummonedBy(summoner);
        player->GetObjectSize();
        Reset();
    }

    void SetMyOutfit()
    {
        Player* player = ObjectAccessor::GetPlayer(*me, _summonerGUID);
        auto const& _data = player->m_companionData;
        if (!_data)
            return;

        auto _race = _data->race;
        Gender _gender = static_cast<Gender>(_data->gender);
        if (_race == RACE_TAUNKA)
            _gender = GENDER_MALE;

        auto it = std::find_if( _companionDefaultVoiceData.begin(), _companionDefaultVoiceData.end(), [&_race, &_gender]( CompanionGreetData const& data )
        {
            return data.CompanionRace == _race && data.CompanionGender == _gender;
        } );

        std::shared_ptr<CreatureOutfit> outfit(new CreatureOutfit(_race, _gender));
        if ( it != _companionDefaultVoiceData.end() )
            _greetSoundId = it->SoundId;

        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_BACK, _data->cloak);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_BODY, _data->shirt);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_CHEST, _data->chest);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_FEET, _data->boots);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_HANDS, _data->hands);
        if (_data->race != 19/*RACE_TAUNKA*/) // that race does not support helmet models
            outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_HEAD, _data->head);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_LEGS, _data->pants);
        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, _data->mainHand);
        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1, _data->offHand);
        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 2, _data->ranged);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_SHOULDERS, _data->shoulders);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_WAIST, _data->waist);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_WRISTS, _data->wrists);
        outfit->SetItemEntry(EquipmentSlots::EQUIPMENT_SLOT_TABARD, _data->tabard);

        me->SetOutfit(outfit);
        me->CastSpell(me, SPELL_COMPANION_TRANSFORM_VISUAL, true);
    }

    void Reset() override
    {
        if (!me->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

        MythicCompanionAI::Reset();
        me->SetImmuneToAll(true);
        if (!me->HasUnitState(UNIT_STATE_FOLLOW))
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _summonerGUID);
            if (player)
                me->GetMotionMaster()->MoveFollow(player, PET_FOLLOW_DIST, COMPANION_FOLLOW_ANGLE, MOTION_SLOT_ACTIVE);
        }
    }

    void UpdateLevel(uint32 newlevel) override
    {
        //! Do not use the other level-up animation, it has additional effects we do not want
        DoCastSelf(SPELL_LEVEL_UP_COMPANION_VISUAL, true);
        me->SetLevel(newlevel, true);
    }

    void sGossipHello(Player* player) override
    {
        if (_greetSoundId)
            me->PlayDistanceSound(_greetSoundId, player);

        if (_summonerGUID != player->GetGUID())
            return;

        Player* owner = ObjectAccessor::GetPlayer(*me, _summonerGUID);
        if (!owner)
            return;

        //! Wait for transaction to end before allowing another one
        if (owner->IsMythicTransactionInProcess())
        {
            std::string _msg = owner->GetName() + ", your previous mythic transaction is currently in progress. Please wait.";
            player->MonsterSay(_msg.c_str(), 0, nullptr);
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }

        player->PlayerTalkClass->ClearMenus();
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Change appearance.", 0, ACTION_APPEARANCE);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Companion tools.", 0, ACTION_TOOLS);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Rewards & Mythic Currency.", 0, ACTION_REWARDS_CURRENCY);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Go to mythic store.", 0, ACTION_GO_TO_STORE_DEFAULT);
        if (player->GetMythicTransmogs().size() > 0)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Show me my transmogrification collection.", 0, ACTION_SHOW_TRANSMOGS);

        InstanceScript* instance = player->GetInstanceScript();
        if (instance && instance->IsMythic())
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Mythic teleports.", ACTION_MYTHIC_TELEPORTER, ACTION_MYTHIC_TELEPORTER);

        SendGossipMenuFor(player, NPC_TEXT_COMPANION_GREET, me);
    }

    bool ValidateItemAction(uint32 actionId, uint32 itemId, Player* player) const
    {
        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
            return false;

        switch (actionId)
        {
        case ACTION_HEAD:
            return proto->InventoryType == INVTYPE_HEAD;
        case ACTION_SHOULDERS:
            return proto->InventoryType == INVTYPE_SHOULDERS;
        case ACTION_BODY:
            return proto->InventoryType == INVTYPE_BODY;
        case ACTION_CHEST:
            return proto->InventoryType == INVTYPE_CHEST;
        case ACTION_WAIST:
            return proto->InventoryType == INVTYPE_WAIST;
        case ACTION_LEGS:
            return proto->InventoryType == INVTYPE_LEGS;
        case ACTION_FEET:
            return proto->InventoryType == INVTYPE_FEET;
        case ACTION_WRISTS:
            return proto->InventoryType == INVTYPE_WRISTS;
        case ACTION_HANDS:
            return proto->InventoryType == INVTYPE_HANDS;
        case ACTION_BACK:
            return proto->InventoryType == INVTYPE_CLOAK;
        case ACTION_MAINHAND:
            return proto->InventoryType == INVTYPE_WEAPONMAINHAND || proto->InventoryType == INVTYPE_2HWEAPON
                || proto->InventoryType == INVTYPE_WEAPON;
        case ACTION_OFFHAND:
        {
            uint32 currentMainHand = player->m_companionData->mainHand;
            ItemTemplate const* weaponTemp = sObjectMgr->GetItemTemplate(currentMainHand);
            if (!weaponTemp)
                return false;

            if (weaponTemp->InventoryType == INVTYPE_2HWEAPON)
                return false;

            return proto->InventoryType == INVTYPE_WEAPONOFFHAND || proto->InventoryType == INVTYPE_WEAPON;
        }
        case ACTION_RANGED:
            return proto->InventoryType == INVTYPE_RANGED || proto->InventoryType == INVTYPE_RANGEDRIGHT ||
                proto->InventoryType == INVTYPE_THROWN;
        }

        return false;
    }

    void sGossipSelect(Player* player, uint32 menuId, uint32 actionId) override
    {
        GossipMenuItem const* item = player->PlayerTalkClass->GetGossipMenu().GetItem(actionId);
        if (!item)
            return;

        uint32 action = item->OptionType;
        uint32 sender = item->Sender;

        bool _success = false;
        player->PlayerTalkClass->ClearMenus();

        uint32 slot = 0;
        bool transmog = false;
        bool mythicTransmog = false;

        if (sender == Mythic::MYTHIC_GOSSIP_SENDER_TELEPORT_BOSS)
        {
            InstanceScript* instance = me->GetInstanceScript();
            if (instance && instance->IsMythic() && !instance->IsEncounterInProgress() && !player->IsInCombat())
                instance->OnPlayerUseMythicShortcut(action, player);
            return;
        }
        else if (sender == ACTION_MYTHIC_TELEPORTER && action == ACTION_MYTHIC_TELEPORTER)
        {
            InstanceScript* instance = me->GetInstanceScript();
            if (instance && instance->IsMythic())
                instance->BuildMythicResurrectGossip(player);
            SendGossipMenuFor(player, COMPANION_TEXT_MYTHIC_TELEPORTS, me);
            return;
        }

        auto ReturnCooldownString = [](std::string const& message, uint32 cdTime)
        {
            return message + secsToTimeString(cdTime / 1000) + ".";
        };

        if (action > ACTION_MAX && sender < EQUIPMENT_SLOT_END)
        {
            slot = sender;
            transmog = true;
        }
        else if (sender >= 100 && sender < 100 + EQUIPMENT_SLOT_END)
        {
            slot = sender - 100;
            transmog = true;
            mythicTransmog = true;
        }

        if (transmog)
        {
            if (!mythicTransmog && !player->HasEnoughMythicPower(Mythic::MYTHIC_COST_TRANSMOG))
                ChatHandler(player->GetSession()).SendSysMessage("You don't have enough mythic power!");
            else
            {
                if (!mythicTransmog)
                    player->TakeMythicPower(Mythic::MYTHIC_COST_TRANSMOG);
                SetItem(player, slot, action, mythicTransmog);
            }
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }
        switch (sender)
        {
        case EQUIPMENT_SLOT_END:
        {
            sTransmogrification->AddTransmogsGossips(player, me, action, true);
            return;
        }
        case EQUIPMENT_SLOT_END + 3:
        {
            SetItem(player, action, 0, true);
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }
        case 200:
        {
            sTransmogrification->AddMythicTransmogsGossips(player, me, action, true);
            return;
        }
        }

        switch (action)
        {
        case ACTION_APPEARANCE:
        {
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Inv_alchemy_potion_02:30:30:-18:0|tChange race", 0, ACTION_RACE, player->FormatCompanionRaces().c_str(), 0, true);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Inv_alchemy_potion_05:30:30:-18:0|tChange gender", 0, ACTION_GENDER, player->FormatCompanionGenders().c_str(), 0, true);
            sTransmogrification->AddSlotsGossips(player, me, true);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Ability_vehicle_loadselfcatapult:30:30:-18:0|tTake me back.", ACTION_RETURN, ACTION_RETURN);
            SendGossipMenuFor(player, NPC_TEXT_APPEARNCE, me->GetGUID());
            break;
        }
        case ACTION_TOOLS:
        {
            SendToolsWindow(player);
            break;
        }
        case ACTION_EXPERIENCE:
        {
            std::ostringstream stream;
            uint32 curTotalExp = player->m_companionData->m_mythicCompanionXP;
            //uint32 nextLevelExpReq = Mythic::MythicCompanionExperienceTable[player->m_companionData->m_mythicCompanionLevel];
            //stream << std::to_string(curTotalExp) << " / " << std::to_string(nextLevelExpReq);
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, stream.str().c_str(), ACTION_RETURN, ACTION_RETURN);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Take me back.", ACTION_RETURN, ACTION_RETURN);
            SendGossipMenuFor(player, COMPANION_TEXT_CURRENT_LVL, me->GetGUID());
            break;
        }
        case ACTION_RETURN:
        {
            sGossipHello(player);
            return;
        }
        case ACTION_BANK:
        {
            if (!player->HasSpellCooldown(SPELL_COMPANION_BANK_MARKER))
            {
                player->GetSession()->SendShowBank(me->GetGUID());
                player->AddSpellCooldown(SPELL_COMPANION_BANK_MARKER, 0, 30 * MINUTE * IN_MILLISECONDS);
            }
            else
                me->MonsterWhisper(ReturnCooldownString(std::string("Bank tool is currently on cooldown "),
                    player->GetSpellCooldownDelay(SPELL_COMPANION_BANK_MARKER)).c_str(), player, false);

            break;
        }
        case ACTION_GUIDLBANK:
        {
            if (!player->HasSpellCooldown(SPELL_COMPANION_GUILD_BANK_MARKER))
            {
                Position pos;
                me->GetNearPosition(pos, 3.f, 0.f);
                GameObject* go = player->SummonGameObject(GOB_MYTHIC_GUILD_BANK, pos, 0.f, 0.f, 0.f, 0.f, 120);

                player->AddSpellCooldown(SPELL_COMPANION_GUILD_BANK_MARKER, 0, 3 * HOUR * IN_MILLISECONDS);
            }
            else
                me->MonsterWhisper(ReturnCooldownString(std::string("Guild Bank tool is currently on cooldown "),
                    player->GetSpellCooldownDelay(SPELL_COMPANION_GUILD_BANK_MARKER)).c_str(), player, false);
            break;
        }
        case ACTION_AUCTION:
        {
            if (!player->HasSpellCooldown(SPELL_COMPANION_AUCTION_HOUSE_MARKER))
            {
                AuctionHouseEntry const* ahEntry = AuctionHouseMgr::GetAuctionHouseEntry(player->getFaction());
                if (!ahEntry)
                    return;

                WorldPacket data(MSG_AUCTION_HELLO, 12);
                data << uint64(me->GetGUID());
                data << uint32(ahEntry->houseId);
                data << uint8(1);                                       // 3.3.3: 1 - AH enabled, 0 - AH disabled
                player->GetSession()->SendPacket(&data);
                player->AddSpellCooldown(SPELL_COMPANION_AUCTION_HOUSE_MARKER, 0, 4 * HOUR * IN_MILLISECONDS);
            }
            else
                me->MonsterWhisper(ReturnCooldownString(std::string("Auctionhouse tool is currently on cooldown "),
                    player->GetSpellCooldownDelay(SPELL_COMPANION_AUCTION_HOUSE_MARKER)).c_str(), player, false);
            break;
        }
        case ACTION_MASS_RESS:
        {
            InstanceScript* instance = me->GetInstanceScript();
            Milliseconds _cooldown{};
            if (instance && !instance->IsEncounterInProgress() && !player->IsInCombat() && instance->CanUseMassResurrect(_cooldown))
            {
                uint32 const _mapId = instance->instance->GetId();
                if (_mapId != 631 && _mapId != 724)
                {
                    instance->ApplyMassResurrectCooldown();
                    me->CastSpell(me, 72429, true);
                }
            }
            else
            {
                std::string _massRessCd = "Mass resurrect is still on cooldown. " +
                    secsToTimeString(_cooldown.count() / 1000);
                me->MonsterWhisper(_massRessCd.c_str(), player, false);
            }
            break;
        }
        case ACTION_REWARDS_CURRENCY:
        {
            auto& rewards = player->GetMythicRewards();
            std::string power = "Mythic power: " + std::to_string(player->m_mythicPower);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, power.c_str(), ACTION_RETURN, ACTION_RETURN);

            if (!rewards.RewardsBracketOne && !rewards.RewardsBracketTwo && !rewards.RewardsBracketThree)
            {
                SendGossipMenuFor(player, NPC_TEXT_NO_TOOLS_OR_REWARDS, me);
                break;
            }

            if (rewards.RewardsBracketOne)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Redeem your rewards.", 0, ACTION_REWARD_MYTHIC);
            }

            SendGossipMenuFor(player, COMPANION_TEXT_REWARDS, me);
            break;
        }
        case ACTION_REWARD_MYTHIC:
        {
            auto& rewards = player->GetMythicRewards();
            if (!rewards.RewardsBracketOne)
            {
                player->PlayerTalkClass->SendCloseGossip();
                return;
            }

            //! 0 - item reward id
            //! 1 - item reward quantity
            //! 2 - transmog item id
            //! 3 - transmog item id quantity
            std::tuple<uint32, uint32, uint32, uint32> _rewards{ 0, 0, 0, 0 };
            auto GenerateRewardIDsAndQuantity =
                [&_rewards]
            (std::vector<Mythic::MythicReward> const& rData,
                std::vector<Mythic::MythicReward> const& rtData)
            {
                Mythic::MythicReward const& _rData = Trinity::Containers::SelectRandomContainerElement(rData);
                Mythic::MythicReward const& _rtData = Trinity::Containers::SelectRandomContainerElement(rtData);
                _rewards = std::make_tuple(_rData.entry, _rData.quantity, _rtData.entry, _rtData.quantity);
            };

            if (rewards.RewardsBracketThree)
            {
                --rewards.RewardsBracketThree;
                --rewards.RewardsBracketTwo;
                --rewards.RewardsBracketOne;

                GenerateRewardIDsAndQuantity(Mythic::MythicRewardsOneTwoThreeConsolidated,
                    Mythic::MythicTransmogConsolidatedOneTwoThree);
            }
            else if (rewards.RewardsBracketTwo)
            {
                --rewards.RewardsBracketTwo;
                --rewards.RewardsBracketOne;

                GenerateRewardIDsAndQuantity(Mythic::MythicRewardsOneTwoConsolidated,
                    Mythic::MythicTransmogConsolidatedOneTwo);
            }
            else
            {
                --rewards.RewardsBracketOne;
                GenerateRewardIDsAndQuantity(Mythic::MythicRewardSetOne,
                    Mythic::MythicRewardsTransmogSetOne);
            }

            AddTransmogItem(player, std::get<2>(_rewards));
            std::string const _itemFormat = player->FormatMythicRewardLink(std::get<0>(_rewards));
            ChatHandler(player->GetSession()).
                PSendSysMessage("and %s has been mailed to you.", _itemFormat.c_str());

            auto _item = std::get<0>(_rewards);
            auto _itemQuant = std::get<1>(_rewards);
            {
                SQLTransaction trans = CharacterDatabase.BeginTransaction();
                MailSender sender(MAIL_CREATURE, 34337 /* The Postmaster */);
                MailDraft draft("Mythic Rewards", "This is your mythic reward, congratulations!");

                Item* item = Item::CreateItem(_item, _itemQuant, player);
                if (item)
                {
                    item->SetUInt64Value(ITEM_FIELD_OWNER, player->GetGUID());
                    item->SetUInt64Value(ITEM_FIELD_CONTAINED, player->GetGUID());
                    item->SetBinding(true);
                }

                item->SaveToDB(trans);
                draft.AddItem(item);
                draft.AddMoney(urand(40, 50)* GOLD);

                draft.SendMailTo(trans, MailReceiver(player, player->GetGUIDLow()), sender);
                CharacterDatabase.CommitTransaction(trans);
            }

            player->SaveMythicRewards();
            player->PlayerTalkClass->SendCloseGossip();

            break;
        }
        case ACTION_SHOW_TRANSMOGS:
        {
            ListTransmogCollection(player, sender);
            break;
        }
        case ACTION_TRANSMOG:
        {
            WorldSession* session = player->GetSession();
            ChatHandler(session).SendSysMessage(sTransmogrification->GetItemLink(sender, session, ItemQualityColors[ITEM_QUALITY_EPIC]).c_str());
            player->PlayerTalkClass->SendCloseGossip();
            break;
        }
        case ACTION_SHOW_COMPANION_TOOLS_STORE:
        {
            SendToolsStore(player);
            SendGossipMenuFor(player, COMPANION_TEXT_STORE, me->GetGUID());
            break;
        }
        case ACTION_GO_TO_STORE_DEFAULT:
        {
            SendStoreWindowDefault(player);
            SendGossipMenuFor(player, COMPANION_TEXT_STORE, me->GetGUID());
            break;
        }
        case ACTION_SHOW_STORE:
        {
            SendItemStoreWindow(player, sender);
            SendGossipMenuFor(player, COMPANION_TEXT_STORE, me->GetGUID());
            break;
        }
        case ACTION_BUY_BANK:
        {
            if (player->HasEnoughMythicPower(Mythic::MYTHIC_COST_BANK))
            {
                player->TakeMythicPower(Mythic::MYTHIC_COST_BANK);
                player->m_companionData->toolBank = true;
                _success = true;

                sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_TOOL_BOUGHT, 0, 0, Mythic::MYTHIC_COST_BANK);
            }
            break;
        }
        case ACTION_BUY_GUILD_BANK:
        {
            if (player->HasEnoughMythicPower(Mythic::MYTHIC_COST_GUILDBANK))
            {
                player->TakeMythicPower(Mythic::MYTHIC_COST_GUILDBANK);
                player->m_companionData->toolGuildBank = true;
                _success = true;

                sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_TOOL_BOUGHT, 1, 0, Mythic::MYTHIC_COST_GUILDBANK);
            }
            break;
        }
        case ACTION_BUY_AUCTION_HOUSE:
        {
            if (player->HasEnoughMythicPower(Mythic::MYTHIC_COST_AUCTIONHOUSE))
            {
                player->TakeMythicPower(Mythic::MYTHIC_COST_AUCTIONHOUSE);
                player->m_companionData->toolAuctionHouse = true;
                _success = true;

                sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_TOOL_BOUGHT, 2, 0, Mythic::MYTHIC_COST_AUCTIONHOUSE);
            }
            break;
        }
        case ACTION_BUY_MASS_RESSURECT:
        {
            if (player->HasEnoughMythicPower(Mythic::MYTHIC_COST_MASSRESS))
            {
                player->TakeMythicPower(Mythic::MYTHIC_COST_MASSRESS);
                player->m_companionData->toolMassRess = true;
                _success = true;

                sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_TOOL_BOUGHT, 3, 0, Mythic::MYTHIC_COST_MASSRESS);
            }
            break;
        }
        case ACTION_BUY_GOBLIN_RACE:
        {
            if (player->HasEnoughMythicPower(Mythic::MYTHIC_COST_GOBLIN))
            {
                player->TakeMythicPower(Mythic::MYTHIC_COST_GOBLIN);
                player->m_companionData->goblinRaceBought = true;
                _success = true;
                sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_TOOL_BOUGHT, 4, 0, Mythic::MYTHIC_COST_GOBLIN);
            }
            break;
        }
        case ACTION_BUY_TAUNKA_RACE:
        {
            if (player->HasEnoughMythicPower(Mythic::MYTHIC_COST_TAUNKA))
            {
                player->TakeMythicPower(Mythic::MYTHIC_COST_TAUNKA);
                player->m_companionData->taunkaRaceBought = true;
                _success = true;
                sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_TOOL_BOUGHT, 5, 0, Mythic::MYTHIC_COST_TAUNKA);
            }
            break;
        }
        case ACTION_BUY_ITEM_STORE:
        {
            uint32 price = 0;
            if (sender == Mythic::HONOR_PACK_ID)
                price = Mythic::HONOR_PACK_PRICE;
            else
            {
                auto itr = std::find_if(Mythic::MythicRewardsOneTwoThreeConsolidated.begin(), Mythic::MythicRewardsOneTwoThreeConsolidated.end(), [&sender](const auto& item) {return item.entry == sender; });
                if (itr == Mythic::MythicRewardsOneTwoThreeConsolidated.end())
                {
                    player->PlayerTalkClass->SendCloseGossip();
                    break;
                }
                price = itr->price;
            }
            if (!price || !player->HasEnoughMythicPower(price))
            {
                ChatHandler(player->GetSession()).SendSysMessage("You don't have enough Mythic Power.");
                player->PlayerTalkClass->SendCloseGossip();
                break;
            }

            player->TakeMythicPower(price);
            _success = true;
            me->MonsterWhisper("The item has been sent to your mailbox.", player, false);
            player->PlayerTalkClass->SendCloseGossip();
            SQLTransaction trans = CharacterDatabase.BeginTransaction();
            MailSender mailsender(MAIL_CREATURE, 34337 /* The Postmaster */);
            MailDraft draft("Mythic Store", "Here is your purchase.");

            Item* item = Item::CreateItem(sender, 1, player);
            if (item)
            {
                item->SetUInt64Value(ITEM_FIELD_OWNER, player->GetGUID());
                item->SetUInt64Value(ITEM_FIELD_CONTAINED, player->GetGUID());
                item->SetBinding(true);
            }

            item->SaveToDB(trans);
            draft.AddItem(item);

            draft.SendMailTo(trans, MailReceiver(player, player->GetGUIDLow()), mailsender);
            CharacterDatabase.CommitTransaction(trans);
            player->SaveMythicRewards(); // saves mythic power

            sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_ITEM_BOUGHT, sender, 0, price);
            break;
        }
        default:
            break;
        }

        if (_success)
        {
            player->SaveMythicCompanion();
            player->SaveMythicRewards();
        }

        if (_success || sender == SENDER_CLOSE_AFTER_USE)
        {
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }

        if (action >= ACTION_RETURN)
            player->PlayerTalkClass->SendCloseGossip();
    }

    void SendItemStoreWindow(Player* player, uint32 page)
    {
        uint32 ITEMS_PER_PAGE = 16;
        uint32 begin = page * ITEMS_PER_PAGE;
        uint32 end = (page + 1) * ITEMS_PER_PAGE - 1;

        uint32 size = static_cast<uint32>(Mythic::MythicRewardsOneTwoThreeConsolidated.size());
        begin = std::min(begin, size);
        end = std::min(end, size);

        WorldSession* session = player->GetSession();

        if (page == 0)
        {
            std::ostringstream os;
            os << sTransmogrification->GetItemIcon(Mythic::HONOR_PACK_ID, 30, 30, -18, 0) <<
                sTransmogrification->GetItemLink(Mythic::HONOR_PACK_ID, session, ItemQualityColors[ITEM_QUALITY_EPIC]);

            if (const ItemTemplate* temp = sObjectMgr->GetItemTemplate(Mythic::HONOR_PACK_ID))
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, os.str().c_str(), Mythic::HONOR_PACK_ID, ACTION_BUY_ITEM_STORE,
                    player->FormatMythicCostString("Grants you 2000 honor.", Mythic::HONOR_PACK_PRICE).c_str(), 0, false);
        }

        for (uint32 i = begin; i < end; ++i)
        {
            std::ostringstream os;
            os << sTransmogrification->GetItemIcon(Mythic::MythicRewardsOneTwoThreeConsolidated[i].entry, 30, 30, -18, 0) <<
                sTransmogrification->GetItemLink(Mythic::MythicRewardsOneTwoThreeConsolidated[i].entry, session, ItemQualityColors[ITEM_QUALITY_EPIC]);

            const ItemTemplate* temp = sObjectMgr->GetItemTemplate(Mythic::MythicRewardsOneTwoThreeConsolidated[i].entry);
            if (!temp)
                continue;

            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, os.str().c_str(), Mythic::MythicRewardsOneTwoThreeConsolidated[i].entry, ACTION_BUY_ITEM_STORE,
                player->FormatMythicCostString(temp->Description, Mythic::MythicRewardsOneTwoThreeConsolidated[i].price).c_str(), 0, false);
        }

        if (page > 0)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Show previous page", page - 1, ACTION_SHOW_STORE);
        if (Mythic::MythicRewardsOneTwoThreeConsolidated.size() > end)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Show next page", page + 1, ACTION_SHOW_STORE);

        SendGossipMenuFor(player, NPC_TRANSMOGS_LIST, me);
    }

    void SendToolsWindow(Player* player)
    {
        uint8 count = 0;
        if (player->m_companionData->toolBank)
        {
            ++count;
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Show me my bank.", SENDER_CLOSE_AFTER_USE, ACTION_BANK);
        }

        if (player->m_companionData->toolGuildBank)
        {
            ++count;
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Show me my guild bank.", SENDER_CLOSE_AFTER_USE, ACTION_GUIDLBANK);
        }

        if (player->m_companionData->toolAuctionHouse)
        {
            ++count;
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Show me auctionhouse.", SENDER_CLOSE_AFTER_USE, ACTION_AUCTION);
        }

        if (player->m_companionData->toolMassRess)
        {
            ++count;
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Use instance-wide mass ressurection.", SENDER_CLOSE_AFTER_USE, ACTION_MASS_RESS);
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Take me back.", ACTION_RETURN, ACTION_RETURN);


        if (count)
            SendGossipMenuFor(player, COMPANION_TEXT_TOOLS, me->GetGUID());
        else
            SendGossipMenuFor(player, NPC_TEXT_NO_TOOLS_OR_REWARDS, me->GetGUID());
    }

    void SendToolsStore(Player* player)
    {
        if (!player->m_companionData->toolBank)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Access to your personal bank.", ACTION_BUY_BANK, ACTION_BUY_BANK,
                player->FormatMythicCostString("Unlocks bank feature.", Mythic::MYTHIC_COST_BANK).c_str(), 0, false);

        if (!player->m_companionData->toolGuildBank)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Access to your guild bank.", ACTION_BUY_GUILD_BANK, ACTION_BUY_GUILD_BANK,
                player->FormatMythicCostString("Unlocks guildbank feature.", Mythic::MYTHIC_COST_GUILDBANK).c_str(), 0, false);

        if (!player->m_companionData->toolAuctionHouse)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Access to the auction house.", ACTION_BUY_AUCTION_HOUSE, ACTION_BUY_AUCTION_HOUSE,
                player->FormatMythicCostString("Unlocks auctionhouse feature.", Mythic::MYTHIC_COST_AUCTIONHOUSE).c_str(), 0, false);

        if (!player->m_companionData->toolMassRess)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Access to the Mass Ressurrection tool.", ACTION_BUY_MASS_RESSURECT, ACTION_BUY_MASS_RESSURECT,
                player->FormatMythicCostString("Unlocks mass ressurect feature.", Mythic::MYTHIC_COST_MASSRESS).c_str(), 0, false);

        //! Goblin race
        if (!player->m_companionData->goblinRaceBought)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Unlock goblin race for your companion.", ACTION_BUY_GOBLIN_RACE, ACTION_BUY_GOBLIN_RACE,
                player->FormatMythicCostString("Unlocks goblin race for your companion", Mythic::MYTHIC_COST_GOBLIN).c_str(), 0, false);

        //! Taunka race
        if (!player->m_companionData->taunkaRaceBought)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Unlock taunka race for your companion.", ACTION_BUY_TAUNKA_RACE, ACTION_BUY_TAUNKA_RACE,
                player->FormatMythicCostString("Unlocks taunka race for your companion", Mythic::MYTHIC_COST_TAUNKA).c_str(), 0, false);


        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Take me back.", ACTION_RETURN, ACTION_RETURN);
    }

    void SendStoreWindowDefault(Player* player)
    {
        player->PlayerTalkClass->ClearMenus();
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Companion tool store", ACTION_SHOW_COMPANION_TOOLS_STORE, ACTION_SHOW_COMPANION_TOOLS_STORE);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Item store", 0, ACTION_SHOW_STORE);
        SendGossipMenuFor(player, COMPANION_TEXT_STORE, me->GetGUID());
    }

    void sGossipSelectCode(Player* player, uint32 menuId, uint32 actionId, const char* text) override
    {
        GossipMenuItem const* item = player->PlayerTalkClass->GetGossipMenu().GetItem(actionId);
        uint32 action = item->OptionType;

        bool _success = false;
        std::string textPassed(text);
        if (textPassed.empty())
        {
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }

        if (action == ACTION_RACE)
        {
            if (!player->HasEnoughMythicPower(Mythic::MYTHIC_COST_GENDER_RACE))
            {
                ChatHandler(player->GetSession()).SendSysMessage("You don't have enough mythic power!");
                player->PlayerTalkClass->SendCloseGossip();
                return;
            }
            uint8 CurRace = player->m_companionData->race;
            uint8 NewRace = CurRace;
            if (textPassed == "orc")
                NewRace = RACE_ORC;
            else if (textPassed == "tauren")
                NewRace = RACE_TAUREN;
            else if (textPassed == "night elf")
                NewRace = RACE_NIGHTELF;
            else if (textPassed == "human")
                NewRace = RACE_HUMAN;
            else if (textPassed == "dwarf")
                NewRace = RACE_DWARF;
            else if (textPassed == "undead")
                NewRace = RACE_UNDEAD_PLAYER;
            else if (textPassed == "gnome")
                NewRace = RACE_GNOME;
            else if (textPassed == "troll")
                NewRace = RACE_TROLL;
            else if (textPassed == "blood elf")
                NewRace = RACE_BLOODELF;
            else if (textPassed == "draenei")
                NewRace = RACE_DRAENEI;
            else if (textPassed == "goblin" && player->m_companionData->goblinRaceBought)
                NewRace = 9/*RACE_GOBLIN*/;
            else if (textPassed == "taunka" && player->m_companionData->taunkaRaceBought)
                NewRace = 19/*RACE_TAUNKA*/;

            if (CurRace != NewRace)
            {
                player->m_companionData->race = NewRace;
                player->TakeMythicPower(Mythic::MYTHIC_COST_GENDER_RACE);
                _success = true;
                sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_RACE_CHANGED, static_cast<uint32>(NewRace), 0, Mythic::MYTHIC_COST_GENDER_RACE);
            }
        }
        else if (action == ACTION_GENDER)
        {
            if (!player->HasEnoughMythicPower(Mythic::MYTHIC_COST_GENDER_RACE))
            {
                ChatHandler(player->GetSession()).SendSysMessage("You don't have enough mythic power!");
                player->PlayerTalkClass->SendCloseGossip();
                return;
            }
            if (textPassed == "male" && player->m_companionData->gender != GENDER_MALE)
            {
                player->m_companionData->gender = GENDER_MALE;
                player->TakeMythicPower(Mythic::MYTHIC_COST_GENDER_RACE);
                _success = true;
            }
            else if (textPassed == "female" && player->m_companionData->gender != GENDER_FEMALE)
            {
                player->m_companionData->gender = GENDER_FEMALE;
                player->TakeMythicPower(Mythic::MYTHIC_COST_GENDER_RACE);
                _success = true;
            }

            if (_success)
                sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_GENDER_CHANGED, static_cast<uint32>(player->m_companionData->gender), 0, Mythic::MYTHIC_COST_GENDER_RACE);
        }
        else
        {
            uint32 newItem = 0;
            try
            {
                newItem = std::stoi(textPassed);
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << std::endl;
            }

            if (!ValidateItemAction(action, newItem, player))
            {
                player->PlayerTalkClass->SendCloseGossip();
                return;
            }

            //! validate slot
            switch (action)
            {
            case ACTION_HEAD:
            {
                player->m_companionData->head = newItem;
                _success = true;
                break;
            }
            case ACTION_SHOULDERS:
            {
                player->m_companionData->shoulders = newItem;
                _success = true;
                break;
            }
            case ACTION_BODY:
            {
                player->m_companionData->shirt = newItem;
                _success = true;
                break;
            }
            case ACTION_CHEST:
            {
                player->m_companionData->chest = newItem;
                _success = true;
                break;
            }
            case ACTION_WAIST:
            {
                player->m_companionData->waist = newItem;
                _success = true;
                break;
            }
            case ACTION_LEGS:
            {
                player->m_companionData->pants = newItem;
                _success = true;
                break;
            }
            case ACTION_FEET:
            {
                player->m_companionData->boots = newItem;
                _success = true;
                break;
            }
            case ACTION_WRISTS:
            {
                player->m_companionData->wrists = newItem;
                _success = true;
                break;
            }
            case ACTION_HANDS:
            {
                player->m_companionData->hands = newItem;
                _success = true;
                break;
            }
            case ACTION_BACK:
            {
                player->m_companionData->cloak = newItem;
                _success = true;
                break;
            }
            case ACTION_MAINHAND:
            {
                player->m_companionData->mainHand = newItem;
                me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, newItem);
                _success = true;
                break;
            }
            case ACTION_OFFHAND:
            {
                player->m_companionData->offHand = newItem;
                me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1, newItem);
                _success = true;
                break;
            }
            case ACTION_RANGED:
            {
                player->m_companionData->ranged = newItem;
                me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, newItem);
                _success = true;
                break;
            }
            default:
                break;
            }
        }

        if (_success)
        {
            player->SaveMythicCompanion();
            SetMyOutfit();

        }

        player->PlayerTalkClass->SendCloseGossip();
    }

    void AddTransmogItem(Player* player, uint32 itemEntry)
    {
        player->AddMythicTransmog(itemEntry);
    }

    void ListTransmogCollection(Player* player, uint32 page)
    {
        uint32 begin = page * TRANSMOGS_PER_PAGE;
        uint32 end = (page + 1) * TRANSMOGS_PER_PAGE - 1;

        const auto& transmogs = player->GetMythicTransmogs();
        uint32 size = static_cast<uint32>(transmogs.size());
        begin = std::min(begin, size);
        end = std::min(end, size);

        WorldSession* session = player->GetSession();

        for (uint32 i = begin; i < end; ++i)
        {
            std::ostringstream os;
            os << sTransmogrification->GetItemIcon(transmogs[i], 30, 30, -18, 0) << sTransmogrification->GetItemLink(transmogs[i], session, ItemQualityColors[ITEM_QUALITY_EPIC]);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, os.str().c_str(), transmogs[i], ACTION_TRANSMOG);
        }

        if (page > 0)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Show previous page", page - 1, ACTION_SHOW_TRANSMOGS);
        if (transmogs.size() > end)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Show next page", page + 1, ACTION_SHOW_TRANSMOGS);

        SendGossipMenuFor(player, NPC_TRANSMOGS_LIST, me);
    }

    void SetItem(Player* player, uint8 slot, uint32 newItem, bool mythicTransmog)
    {
        bool _success = false;
        uint32 itemEntry = newItem;

        if (!mythicTransmog)
        {
            ItemRef itemTransmogrified = player->GetItemByLowGuid(newItem);
            if (!itemTransmogrified)
                return;

            itemTransmogrified->UpdatePlayedTime(player);

            itemTransmogrified->SetOwnerGUID(player->GetGUID());
            itemTransmogrified->SetNotRefundable(player);
            itemTransmogrified->ClearSoulboundTradeable(player);

            itemEntry = itemTransmogrified->GetEntry();
        }

        switch (slot)
        {
        case EQUIPMENT_SLOT_HEAD:
        {
            player->m_companionData->head = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_SHOULDERS:
        {
            player->m_companionData->shoulders = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_BODY:
        {
            player->m_companionData->shirt = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_CHEST:
        {
            player->m_companionData->chest = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_WAIST:
        {
            player->m_companionData->waist = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_LEGS:
        {
            player->m_companionData->pants = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_FEET:
        {
            player->m_companionData->boots = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_WRISTS:
        {
            player->m_companionData->wrists = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_HANDS:
        {
            player->m_companionData->hands = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_BACK:
        {
            player->m_companionData->cloak = itemEntry;
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_TABARD:
        {
            player->m_companionData->tabard = itemEntry;
            _success                       = true;
            break;
        }
        case EQUIPMENT_SLOT_MAINHAND:
        {
            player->m_companionData->mainHand = itemEntry;
            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID, itemEntry);
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_OFFHAND:
        {
            player->m_companionData->offHand = itemEntry;
            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1, itemEntry);
            _success = true;
            break;
        }
        case EQUIPMENT_SLOT_RANGED:
        {
            player->m_companionData->ranged = itemEntry;
            me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 2, itemEntry);
            _success = true;
            break;
        }
        default:
            break;
        }

        if (_success)
        {
            player->SaveMythicCompanion();
            player->SaveMythicRewards();
            SetMyOutfit();
            int32 _itemEntry = itemEntry;
            if (mythicTransmog)
            {
                player->DeleteMythicTransmog(itemEntry);
                _itemEntry = -_itemEntry;
            }

            uint32 transmogCost = itemEntry ? Mythic::MYTHIC_COST_TRANSMOG : 0;
            sMythicMgr.MythicPowerLog(player->GetGUIDLow(), Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_TRANSMOG, slot, _itemEntry, transmogCost);
        }
    }
private:
    uint32 _greetSoundId;
    uint64 _summonerGUID;
};


void AddSC_mythic_companion()
{
    RegisterCreatureAI(npc_mythic_custom_companion);
}