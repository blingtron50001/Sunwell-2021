#pragma once
#include "Optional.h"
#include "MythicLoot.hpp"

#include "Threading/LockedQueue.h"

class Group;
struct Position;

namespace Mythic
{
    using MythicRecordsStore = std::vector<std::string>;

    template <class T, class U>
    inline T CalculateMythicValue(T base, U variance)
    {
        return base + T(base * variance);
    }

    constexpr uint32 MYTHIC_SECRET_PHASE_KEY_LEVEL{ 6 };
    constexpr uint32 MYTHIC_COMPANION_ENTRY{ 91501 };
    constexpr uint32 MYTHIC_COMPANION_MAX_LEVEL{ 80 };

    constexpr uint32 MYTHIC_POWER_REWARD_FIRST{ 500 }; // * MythicKeyLevel
    constexpr uint32 MYTHIC_POWER_REWARD_NEXT{ 100 }; // * MythicKeyLevel
    constexpr uint32 MYTHIC_POWER_REWARD_TIME{ 200 }; // each bracket adds additional value, bronze * 1, silver * 2 etc
    constexpr uint32 GO_MYTHIC_CONSUMABLE_CHEST_HORDE{ 302326 };
    constexpr uint32 GO_MYTHIC_CONSUMABLE_CHEST_ALLIANCE{ 302325 };
    constexpr uint32 ITEM_MYTHIC_KEY_ENTRY{ 47395 };

    enum AffixIds
    {
        AFFIX_BREAK_CC_IN_COMBAT = 1,
        AFFIX_BUFF_NEARBY_ON_DEATH,
        AFFIX_BELOW_20_PCT_HEALTH_ENRAGE,
        AFFIX_MOJO_POOL_ON_DEATH,
        AFFIX_FIRE_DRAGON_WHILE_IN_COMBAT,
        AFFIX_MORTAL_WOUND_ON_MELEE,
        AFFIX_PERIODIC_SPAWN_GHOULS,
        AFFIX_HOLY_BOMBS,

        /** Persistent affixes **/
        AFFIX_SPELL_BOUNCE = 9, // data bound to database, if you change this value, change db value
    };

    enum MythicApiCalls
    {
        MYTHIC_API_NONE = 0,
        MYTHIC_API_MAX
    };

    enum MythicStates
    {
        //! Returns a timestamp of when previous shuffle has been made
        MYTHIC_STATE_NEXT_SHUFFLE = 0,
        MYTHIC_STATE_CURRENT_MYTHIC_MAPS = 1,

        /** Affixes etc **/
        MYTHIC_STATE_CURRENT_AFFIX_PRESET = 2,
        // unused = 3,
        MYTHIC_STATE_INSTANCE_EVENT_SEQUENCE = 4,
    };

    enum MythicAffixProcFlags : uint32
    {
        MYTHIC_PROC_FLAG_ON_CREATURE_ADDON_LOAD         = 0x01,
        MYTHIC_PROC_FLAG_DEATH                          = 0x02,
        MYTHIC_PROC_FLAG_HEALTHLESS_20_PCT              = 0x04,
        MYTHIC_PROC_FLAG_INITIALIZE_MYTHIC              = 0x08,
        MYTHIC_PROC_FLAG_MELEE_ATTACK                   = 0x10,
        MYTHIC_PROC_FLAG_SUCCESFUL_MAGIC_SPELL_HIT      = 0x20
    };

    struct MythicAffixData
    {
        uint32 AffixId                      = 0;
        std::string AffixName               = "";
        uint32 AffixFlags                   = 0;
        uint32 AffixChance                  = 100;
        uint32 AffixCooldown                = 0; // in milliseconds
        uint32 AffixKeyRequirement          = 0;
        bool   IsInstanceAffix              = 0;
    };

    struct MythicPresetData
    {
        uint32 FirstAffix                   = 0;
        uint32 SecondAffix                  = 0;
        uint32 ThirdAffix                   = 0;
        uint32 FourthAffix                  = 0;
    };

    enum MythicCreatureType
    {
        CREATURE_TYPE_MELEE = 1,
        CREATURE_TYPE_CASTER,
        CREATURE_TYPE_TANK,
        CREATURE_TYPE_HEALER,
        CREATURE_TYPE_RANGED,
        CREATURE_TYPE_PASSIVE,

        CREATURE_TYPE_MAX
    };

    struct MythicSpells
    {
        uint32 SpellId                          = 0;
        int32 SpellEffectValue0                 = 0;
        int32 SpellEffectValue1                 = 0;
        int32 SpellEffectValue2                 = 0;
        int32 AuraDuration                      = 0;
        uint32 MaxEffectedTargets               = 0;
        float SpellValueVariance                = 0.f;
        Milliseconds ScheduleTime               = 0ms;
        Milliseconds RepeatTime                 = 0ms;
        uint32 TargetType                       = 0;
    };

    struct MythicCreatureData
    {
        MythicCreatureType CreatureType         = CREATURE_TYPE_MAX;
        uint32 CreatureLevel                    = 80;
        uint32 EncounterAbilityMask             = 0;
        uint32 BaseMythicHealth                 = 1000;
        uint32 BaseMythicPower                  = 100;
        uint32 MinRawMeleeDamage                = 100;
        uint32 MaxRawMeleeDamage                = 100;
        float  KeyVarianceMelee                 = 0.f;
        float  KeyVarianceStats                 = 0.f;
        uint32 MythicLootId                     = 0;
    };

    struct MythicPlayerRewards
    {
        uint32 RewardsBracketOne            = 0;
        uint32 RewardsBracketTwo            = 0;
        uint32 RewardsBracketThree          = 0;
    };

    constexpr uint32 MYTHIC_CRYSTALS_EVENT_COUNT = 4;
    constexpr uint32 MYTHIC_CRYSTALS_IDS[MYTHIC_CRYSTALS_EVENT_COUNT] = { 302100, 302102, 302103, 302101 };
    constexpr uint32 MYTHIC_CRYSTALS_NPC_ID = 230000;
    constexpr uint32 MYTHIC_LOST_NPCS_TO_FOUND = 3;
    constexpr uint32 MYTHIC_ALCHEMIST_ENTRY = 230020;
    constexpr uint32 MYTHIC_ALCHEMIST_BOOKS_COUNT = 3;
    constexpr uint32 MYTHIC_ALCHEMIST_BOOK_ENTRY = 302200;
    constexpr uint32 MYTHIC_ALCHEMIST_INGREDIENTS_COUNT = 7;

    struct MythicEventInfo
    {
        struct CrystalsEvent
        {
            std::vector<uint32> CorrectOrder{ 0, 1, 2, 3 };
            uint32 CrystalsSpawned = 0;
            uint32 CrystalsClicked = 0;

            uint32 GetCorrectGobjectIdAtPosition(uint32 pos) { return MYTHIC_CRYSTALS_IDS[CorrectOrder[pos]]; };
        } crystalsEvent;

        struct LostNpcsEvent
        {
            std::vector<uint32> LostNpcs{ 0, 1, 2, 3, 4, 5 };
            uint32 FoundNpcs = 0;
        } lostNpcsEvent;

        struct AlchemistEvent
        {
            uint32 IngredientsCount[3][3];
            uint32 PossibleAnswers[MYTHIC_ALCHEMIST_INGREDIENTS_COUNT][3];
            bool initialized = false;
        } alchemistEvent;
    };

    enum MythicEventType
    {
        MYTHIC_EVENT_CRYSTALS   = 0,
        MYTHIC_EVENT_LOST_NPCS  = 1,
        MYTHIC_EVENT_ALCHEMIST  = 2,
    };

    enum MythicEventsWorldStates
    {
        MYTHIC_ALCHEMIST_WS_FIRST   = 5000,
        MYTHIC_ALCHEMIST_WS_LAST    = 5006,
    };

    constexpr uint32 RECORD_HOLDER_MAX_SIZE{ 5 };
    constexpr uint32 GO_MYTHIC_STONE_ENTRY{ 302000 };
    constexpr uint32 MYTHIC_PLAYER_DEATH_TIME_PENALTY{ 5000 };
    constexpr uint32 SPELL_BUFF_NEARBY_CREATURES_MYTHIC{ 90002 };
    constexpr uint32 SPELL_MYTHIC_BROKEN_BONES_BASE{ 62354 };
    constexpr uint32 SPELL_HOT_FEET_FLAME_PATCH_MYTHIC{ 52208 };
    constexpr uint32 SPELL_MYTHIC_BUFF{ 64036 };
    constexpr uint32 NPC_HOT_FEET_TRIGGER{ 91500 };

    enum MythicGroupConvertErrors
    {
        MYTHIC_GROUP_ERROR_BELOW_LEVEL_80   = 0,
        MYTHIC_GROUP_ERROR_NO_GROUP,
        MYTHIC_GROUP_ERROR_ALREADY_MYTHIC,
        MYTHIC_GROUP_ERROR_NOT_GROUP_LEADER,
        MYTHIC_GROUP_ERROR_LFG_BG_BF_GROUP,
        MYTHIC_GROUP_ERROR_RAID_GROUP,
        MYTHIC_GROUP_ERROR_IN_BG_OR_LFG_QUEUE,
        MYTHIC_GROUP_ERROR_OK,

        MYTHIC_GROUP_ERROR_MAX
    };

    constexpr uint32 MYTHIC_COST_TRANSMOG       = 25;
    constexpr uint32 MYTHIC_COST_GENDER_RACE    = 200;
    constexpr uint32 MYTHIC_COST_GUILDBANK      = 10000;
    constexpr uint32 MYTHIC_COST_MASSRESS       = 5000;
    constexpr uint32 MYTHIC_COST_BANK           = 2500;
    constexpr uint32 MYTHIC_COST_AUCTIONHOUSE   = 20000;
    constexpr uint32 MYTHIC_COST_GOBLIN         = 400;
    constexpr uint32 MYTHIC_COST_TAUNKA         = 600;

    constexpr uint32 MYTHIC_GOSSIP_SENDER_TELEPORT_BOSS{ 10000000 };

    enum MythicPowerLogType
    {
        MYTHIC_POWER_LOG_INSTANCE_FINISHED  = 0,
        MYTHIC_POWER_LOG_ITEM_BOUGHT        = 1,
        MYTHIC_POWER_LOG_TOOL_BOUGHT        = 2,
        MYTHIC_POWER_LOG_TRANSMOG           = 3,
        MYTHIC_POWER_LOG_GENDER_CHANGED     = 4,
        MYTHIC_POWER_LOG_RACE_CHANGED       = 5,
    };
}

using MythicCreatureTemplate = std::unordered_map<uint32 /*MythicTemplateId*/, Mythic::MythicCreatureData /*data*/>;
using MythicSpellData = std::unordered_map<Mythic::MythicCreatureType /*type*/, std::vector<Mythic::MythicSpells>>;
using MythicSpellValues = std::unordered_map<uint32 /*SpellId*/, Mythic::MythicSpells>;

using MythicAffixStore = std::unordered_map<uint32 /*affixId*/, Mythic::MythicAffixData>;
using MythicAffixPresets = std::unordered_map<uint32, Mythic::MythicPresetData>;

struct MythicTimeInfo
{
    uint32 goldTime{ 0 };
    uint32 silverTime{ 0 };
    uint32 bronzeTime{ 0 };

    Mythic::MythicRecordsStore goldPlayers;
    Mythic::MythicRecordsStore silverPlayers;
    Mythic::MythicRecordsStore bronzePlayers;
};

//! key | time data
using MythicPerKeyStore = std::unordered_map<uint32, MythicTimeInfo>;
//! MapId | per key store
using MythicBestTimeStore = std::unordered_map<uint32, MythicPerKeyStore>;

static std::vector<std::pair<uint32, Position>> _mythicStoneMapPositions =
{
    { 209, { 1221.205f, 831.880f, 8.877f, 6.203f } },
    { 542, { -8.25118f, -0.287088f, -44.0936f, 0.216717f  } },
    { 576, { 175.300f, 0.0815047f, -16.63600f, 0.144913f } },
    { 189, { 870.810181f, 1312.047363f, 18.006071f, 4.735905f } }
};

static std::vector<uint32> BossEntryStore =
{
    /** Zulfarrak **/
    { 7272 }, // Theka the martyr
    { 8127 }, // Antusul
    { 7271 }, // zumrah
    { 7267 }, // Ukorz
    { 7797 }, // Ruuzlu
    { 7795 }, // velratha

    // blod furnace
    { 17381 }, // The Maker
    { 17380 }, // Brogokk
    { 17377 }, // Kelidan

    // nexus
    { 26731 }, // Telestra
    { 26763 }, // Anomalus
    { 26794 }, // Ormormok
    { 26723 }  // Keristrasza
};

static std::string MYTHIC_WHISPER_PLAYER_DIED = "{} has died, {} second penalty.";
static std::string MYTHIC_FINISHED_INSTANCE_TEXT_NO_BRACKET = "You've finished mythic instance with key level {} and it took: {}. You've been awarded {} mythic power.";
static std::string MYTHIC_FINISHED_INSTANCE_TEXT_BRACKET = "You've finished mythic instance with key level {} and it took: {} ({} bracket). You've been awarded {} mythic power.";
static std::string MYTHIC_FINISHED_INSTANCE_NEW_RECORD_TEXT =
"Congratulations!! This is a new record for {} bracket! Your time is {}. You've been awarded {} mythic power and additional {} mythic power for "
"achieving new record!";

class MythicMgr
{
private:
    MythicMgr();

    MythicCreatureTemplate      m_mythicCreatureTemplateStore;
    //! Contains data about additional mythic spells and its values
    MythicSpellData             m_mythicSpellStore;
    //! Contains data about spells that are used on non-mythic mode as well
    MythicSpellValues           m_mythicSpellValueStore;
    //! Contains data about map times, entrances and everything needed for mythic from map level
    MythicBestTimeStore         m_mythicMapStore;
    //! Contains data about affixes, its cooldown and proc flags
    MythicAffixStore            m_mythicAffixStore;
    //! Presets, each preset contains 4 affix IDs
    MythicAffixPresets          m_mythicAffixPresets;

    void LoadMythicStates();
    void LoadMythicMapData();
    void LoadCreatureMythicTemplates();
    void LoadMythicSpells();
    void LoadShuffleTime(PreparedQueryResult& /*res*/);
    void LoadAvailableMaps(PreparedQueryResult& /*res*/);
    void LoadInstanceEventSequence(PreparedQueryResult& /*res*/);
    void LoadCurrentAffixSet(PreparedQueryResult& /*res*/);
    void LoadAffixData();
    void DoMythicShuffle();

    uint32 m_previousShuffleTime;
    uint32 m_currentAffixPresetId;
    std::vector<uint32> m_currentMythicMaps;
    std::vector<uint32> m_currentEventSequence;
    Mythic::MythicEventType m_currentEventId;
    std::mutex m_lock;
    Optional<PreparedQueryResultFuture> m_currentHandledFuture;

public:
    static MythicMgr& GetInstance();

    void Initialize();
    bool IsMythicAvailableForMap(uint32 /*mapId*/) const;
    bool AmIMythicBoss(uint32 entry) const
    {
        auto it = std::find(BossEntryStore.begin(), BossEntryStore.end(), entry);
        return it != BossEntryStore.end();
    }
    bool ShouldBeSpawned(Creature* creature);

    //uint32 GetBestTimeFor( uint32 mapId, uint32 keyLevel );
    uint32 GetBestMythicTimeFor( uint32 mapId, uint32 keyLevel );
    uint32 GetSilverBracketTimeFor( uint32 mapId, uint32 keyLevel );
    uint32 GetBronzeBracketTimeFor( uint32 mapId, uint32 keyLevel );
    void SaveRecords( uint32 mapId, uint32 keyLevel, uint32 timeType, uint32 time, Mythic::MythicRecordsStore players, bool populateTable = false );
    void HandleAsyncQueries();

    //! Exp for companion level, will be always valid for keys 1-80 EXCLUDING 0
    std::unordered_map<uint32, uint32> CompanionExpTableStore;

    Optional<Mythic::MythicCreatureData> GetMythicTemplateFor(uint32 /*templateId*/);
    bool GetSpellValuesFor(uint32 spellId, Mythic::MythicSpells& data) const;
    Optional<Mythic::MythicAffixData const> GetSingleAffix(uint32 /*affixId*/);
    void GetMythicAffixesForCreature(std::vector<Mythic::MythicAffixData>& /*container*/);
    void GetMythicAffixesForInstance(std::vector<Mythic::MythicAffixData>& /*container*/);
    std::vector<uint32> const& GetCurrentMythicMaps() { return m_currentMythicMaps; }

    /* Mythic Events functions */
    Mythic::MythicEventType GetCurrentMythicEventId() const { return m_currentEventId; }
    void SetCurrentMythicEventId(Mythic::MythicEventType eventType) { m_currentEventId = eventType; }
    /* Crystal Event*/
    Optional<Position> GetEventCrystalPosition(uint32 mapId, uint32 index);
    /* Lost NPCs Event*/
    Optional<Position> GetEventLostNpcPosition(uint32 mapId, uint32 index);
    uint32 GetEventLostNpcEntry(uint32 mapId, uint32 type);
    /* Alchemist Event */
    Optional<Position> GetEventAlchemistPosition(uint32 mapId, uint32 index);

    Mythic::MythicGroupConvertErrors ConvertGroupToMythic(Group* group);
    void TeleportToMythicDestination(uint32 /*mapId*/, Player* /*player*/);
    Position const& GetMythicStoneSpawnpositionFor(uint32 /*mapId*/);
    void HandleMythicApiCall(Mythic::MythicApiCalls /*call*/);

    void MythicPowerLog(uint32 guid, Mythic::MythicPowerLogType, uint32 data1, uint32 data2, int32 value);
protected:
    struct RecordData
    {
        uint32 MapId = 0;
        uint32 KeyLevel = 0;
        uint32 Type = 0;
        uint32 Time = 0;
        std::string Players;
    };

    Threading::LockedQueue<RecordData> m_recordStore;

    MythicMgr(MythicMgr const&) = delete;
    MythicMgr(MythicMgr&&) = delete;
    MythicMgr& operator=(MythicMgr const&) = delete;
    MythicMgr& operator=(MythicMgr&&) = delete;
};

#define sMythicMgr MythicMgr::GetInstance()
