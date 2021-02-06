#include "Containers.h"
#include "Group.h"
#include "InstanceScript.h"
#include "MythicMgr.hpp"
#include "Player.h"
#include "Util.h"

#include "fmt/format.h"

#include <algorithm>
#include <string>
#include <numeric>

MythicMgr& MythicMgr::GetInstance()
{
    static MythicMgr instance;

    return instance;
}

MythicMgr::MythicMgr()
{
    m_previousShuffleTime = 0;
    m_currentAffixPresetId = 1;
    m_currentEventId = Mythic::MythicEventType::MYTHIC_EVENT_CRYSTALS;
    m_currentHandledFuture.reset();
    m_mythicMapStore.clear();
}

void MythicMgr::Initialize()
{
    sLog->outString(">> Initializing mythic dungeon system...");

    //! loads every mythic state data and initializes it
    LoadMythicMapData();
    LoadMythicSpells();
    LoadCreatureMythicTemplates();
    LoadAffixData();
    LoadMythicStates();

    uint32 exp = 0;
    for ( uint32 i = 0; i < Mythic::MYTHIC_COMPANION_MAX_LEVEL; ++i )
    {
        uint32 expRequiredForLevel = i * 500;
        exp += expRequiredForLevel;
        // 1 = 0
        // 2 = 500
        // 3 = 1000 (exp for level required 1500 at this point)
        // 4 = 1500 (exp for level required 3000 at this point) and so on till 80
        CompanionExpTableStore[ i + 1 ] = exp;
    }

    sLog->outString("Done initializing mythic dungeon system!");
}

void MythicMgr::LoadMythicStates()
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MYTHIC_STATE_VALUE);
    //! przerobic na zwykla petle..
    stmt->setUInt32(0, Mythic::MYTHIC_STATE_NEXT_SHUFFLE);
    PreparedQueryResult shuffleResult = CharacterDatabase.Query(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MYTHIC_STATE_VALUE);
    stmt->setUInt32(0, Mythic::MYTHIC_STATE_CURRENT_MYTHIC_MAPS);
    PreparedQueryResult mapsResult = CharacterDatabase.Query(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MYTHIC_STATE_VALUE);
    stmt->setUInt32(0, Mythic::MYTHIC_STATE_CURRENT_AFFIX_PRESET);
    PreparedQueryResult presetResult = CharacterDatabase.Query(stmt);

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MYTHIC_STATE_VALUE);
    stmt->setUInt32(0, Mythic::MYTHIC_STATE_INSTANCE_EVENT_SEQUENCE);
    PreparedQueryResult instanceEventSequenceResult = CharacterDatabase.Query(stmt);

    //! Always load shuffle as last, it will call DoMythicShuffle if needed
    //! Mythic states wont populate itself, we need to feed it data manually at least once
    //! those function will assert if there is no result
    LoadAvailableMaps(mapsResult);
    LoadInstanceEventSequence(instanceEventSequenceResult);
    LoadCurrentAffixSet(presetResult);
    LoadShuffleTime(shuffleResult);
}

void MythicMgr::LoadInstanceEventSequence(PreparedQueryResult& result)
{
    ASSERT(result);

    Field* field = result->Fetch();
    std::string eventSequence = field[0].GetString();
    ASSERT(!eventSequence.empty());
    Tokenizer toks(eventSequence, ';');
    for (auto&& tok : toks)
    {
        uint32 eventId = std::atoi(tok);
        m_currentEventSequence.push_back(eventId);
    }

    m_currentEventId = Mythic::MythicEventType(m_currentEventSequence[0]);
}

void MythicMgr::DoMythicShuffle()
{
    /** Set next shuffle time **/
    time_t NextShuffleTime = time(nullptr) + WEEK;
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_MYTHIC_STATE_VALUE);
    stmt->setUInt32(0, Mythic::MYTHIC_STATE_NEXT_SHUFFLE);
    stmt->setString(1, std::to_string(NextShuffleTime));
    CharacterDatabase.Execute(stmt);

    /** Mythic events and obstacles **/
    //! rotate current sequence one element to the right
    std::rotate(m_currentEventSequence.begin(), m_currentEventSequence.begin() + 1, m_currentEventSequence.end());
    m_currentEventId = Mythic::MythicEventType(m_currentEventSequence[0]);

    std::string _eventSequenceString = "";
    for (size_t i = 0; i < m_currentEventSequence.size(); ++i)
        _eventSequenceString += std::to_string(m_currentEventSequence[i]) + ";";
    //! remove last semicolon, not needed
    _eventSequenceString.pop_back();

    stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_MYTHIC_STATE_VALUE);
    stmt->setUInt32(0, Mythic::MYTHIC_STATE_INSTANCE_EVENT_SEQUENCE);
    stmt->setString(1, _eventSequenceString);
    CharacterDatabase.Execute(stmt);

    /** Affix related **/
    std::vector<size_t> _allPresets(m_mythicAffixPresets.size());
    std::iota(_allPresets.begin(), _allPresets.end(), static_cast<size_t>(0));
    _allPresets.erase(
        std::remove(_allPresets.begin(), _allPresets.end(), static_cast<size_t>(m_currentAffixPresetId)),
        _allPresets.end());

    if (!_allPresets.empty())
        m_currentAffixPresetId = Trinity::Containers::SelectRandomContainerElement(_allPresets);
    // else

    if (!m_currentAffixPresetId)
        m_currentAffixPresetId = 1;
    // we do not have any other presets, leave it as is

    //! save new affix preset id to database
    stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_MYTHIC_STATE_VALUE);
    stmt->setUInt32(0, Mythic::MYTHIC_STATE_CURRENT_AFFIX_PRESET);
    stmt->setString(1, std::to_string(m_currentAffixPresetId));
    CharacterDatabase.Execute(stmt);
}

void MythicMgr::LoadCreatureMythicTemplates()
{
    sLog->outString(">>>> [Mythic] Loading mythic templates...");

    PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_CREATURE_MYTHIC_TEMPLATES);
    PreparedQueryResult result = WorldDatabase.Query(stmt);

    if (result)
    {
        do
        {
            Field* field = result->Fetch();
            Mythic::MythicCreatureData data;
            uint32 Id = field[0].GetUInt32();
            data.CreatureType = static_cast<Mythic::MythicCreatureType>(field[1].GetUInt32());
            data.CreatureLevel = field[2].GetUInt32();
            data.EncounterAbilityMask = field[3].GetUInt32();
            data.BaseMythicHealth = field[4].GetUInt32();
            data.BaseMythicPower = field[5].GetUInt32();
            data.MinRawMeleeDamage = field[6].GetUInt32();
            data.MaxRawMeleeDamage = field[7].GetUInt32();
            data.KeyVarianceMelee = field[8].GetFloat();
            data.KeyVarianceStats = field[9].GetFloat();
            data.MythicLootId = field[10].GetUInt32();

            m_mythicCreatureTemplateStore[Id] = std::move(data);
        } while (result->NextRow());
    }

    sLog->outString(">>>> [Mythic] Done loading mythic templates [%zu]", m_mythicCreatureTemplateStore.size());
}

void MythicMgr::LoadMythicSpells()
{
    sLog->outString(">>>> [Mythic] Loading mythic spells...");

    PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MYTHIC_SPELL_DATA);
    PreparedQueryResult result = WorldDatabase.Query(stmt);

    for (uint32 i = Mythic::CREATURE_TYPE_MELEE; i < Mythic::CREATURE_TYPE_MAX; ++i)
        m_mythicSpellStore[static_cast<Mythic::MythicCreatureType>(i)].clear();

    if (result)
    {
        do
        {
            Field* field = result->Fetch();
            Mythic::MythicSpells data;
            Mythic::MythicCreatureType creatureType = static_cast<Mythic::MythicCreatureType>(field[0].GetUInt32());
            data.SpellId                            = field[1].GetUInt32();
            data.SpellEffectValue0                  = field[2].GetInt32();
            data.SpellEffectValue1                  = field[3].GetInt32();
            data.SpellEffectValue2                  = field[4].GetInt32();
            data.AuraDuration                       = field[5].GetInt32();
            data.MaxEffectedTargets                 = field[6].GetUInt32();
            data.SpellValueVariance                 = field[7].GetFloat();
            std::string _timers                     = field[8].GetString();
            ASSERT(!_timers.empty());
            Tokenizer token(_timers, ';');
            ASSERT(token.size() == 2);
            uint32 _scheduleTime                    = std::atoi(token[0]);
            uint32 _repeatTime                      = std::atoi(token[1]);
            data.ScheduleTime                       = Milliseconds(_scheduleTime);
            data.RepeatTime                         = Milliseconds(_repeatTime);
            data.TargetType                         = field[9].GetUInt32();

            m_mythicSpellStore[creatureType].push_back(std::move(data));
        } while (result->NextRow());
    }

    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MYTHIC_SPELL_VALUES);
    result = WorldDatabase.Query(stmt);

    if (result)
    {
        do
        {
            Field* field = result->Fetch();
            Mythic::MythicSpells data;
            uint32 _spellId                     = field[0].GetUInt32();
            data.SpellId                        = _spellId;
            data.SpellEffectValue0              = field[1].GetInt32();
            data.SpellEffectValue1              = field[2].GetInt32();
            data.SpellEffectValue2              = field[3].GetInt32();
            data.AuraDuration                   = field[4].GetInt32();
            data.MaxEffectedTargets             = field[5].GetUInt32();
            data.SpellValueVariance             = field[6].GetFloat();

            m_mythicSpellValueStore[_spellId]   = std::move(data);
        } while (result->NextRow());
    }

    sLog->outString(">>>> [Mythic] Done loading mythic spells.");
}

void MythicMgr::LoadShuffleTime(PreparedQueryResult& res)
{
    sLog->outString(">>>> [Mythic] Loading last shuffle time");

    if (res)
    {
        Field* field = res->Fetch();
        m_previousShuffleTime = std::stoi(field[0].GetString());
    }

    //! If there is no timer set or previous shuffle has happened over a week ago then calculate next shuffle
    auto const _timeconst = 10 * YEAR; // week by default, for now 10 years to disable shuffles
    bool const shouldShuffle = !m_previousShuffleTime || (m_previousShuffleTime - (time(nullptr) - _timeconst) < _timeconst);

    if (shouldShuffle)
        DoMythicShuffle();
}

void MythicMgr::LoadAvailableMaps(PreparedQueryResult& result)
{
    ASSERT(result);

    Field* field = result->Fetch();
    std::string _maps = field[0].GetString();
    Tokenizer tokens(_maps, ';');
    for (auto&& token : tokens)
    {
        uint32 id = std::atoi(token);
        //ASSER T(m_mythicMapStore.find(id) != m_mythicMapStore.end());
        m_currentMythicMaps.push_back(id);
    }
}


Optional<Mythic::MythicCreatureData> MythicMgr::GetMythicTemplateFor(uint32 templateId)
{
    auto it = m_mythicCreatureTemplateStore.find(templateId);
    if (it != m_mythicCreatureTemplateStore.end())
        return it->second;

    return { };
}

//Mythic::MythicTimerType MythicMgr::EvaluateElapsedTime(uint32 timeElapsed, uint32 mapId)
//{
//    auto const& mythicData = GetMapData(mapId);
//    ASSERT(mythicData);
//    std::vector < std::pair < uint32, Mythic::MythicTimerType > > _times;
//    _times.push_back({ mythicData->BaseGoldTime, Mythic::MYTHIC_GOLD_TIME });
//    _times.push_back({ mythicData->BaseSilverTime, Mythic::MYTHIC_SILVER_TIME });
//    _times.push_back({ mythicData->BaseBronzeTime, Mythic::MYTHIC_BRONZE_TIME });
//
//    for (auto const& time : _times)
//    {
//        if (timeElapsed <= time.first)
//            return time.second;
//    }
//
//    return Mythic::MYTHIC_TIMER_MAX;
//}
//
///** Server wide best times **/
//void MythicMgr::EvaluateBestTimes(uint32 timeElapsed, uint32 mapId, std::vector<uint64> participants)
//{
//    auto const& mythicData = GetMapData(mapId);
//    ASSERT(mythicData);
//    //std::vector < std::pair < uint32, Mythic::MythicTimerType > > _times;
//    std::vector< std::pair <uint32, Mythic::MythicTimerType > > _times
//    {
//        { mythicData->CurrentGoldRecord, Mythic::MYTHIC_GOLD_TIME      },
//        { mythicData->CurrentSilverRecord, Mythic::MYTHIC_SILVER_TIME  },
//        { mythicData->CurrentBronzeRecord, Mythic::MYTHIC_BRONZE_TIME  }
//    };
//
//    Mythic::MythicTimerType _type = Mythic::MYTHIC_TIMER_MAX;
//    for (auto const& time : _times)
//    {
//        if (timeElapsed <= time.first)
//        {
//            _type = time.second;
//            break;
//        }
//    }
//
//    if (_type != Mythic::MYTHIC_TIMER_MAX)
//    {
//        UpdateCurrentRecordFor(timeElapsed, mapId, participants, _type);
//        std::string newRecordStr = "new record for mapId: " + mapId;
//        newRecordStr += ". New record is: " + timeElapsed;
//        sWorld->SendGlobalText(newRecordStr.c_str(), nullptr);
//    }
//}
//
//void MythicMgr::UpdateCurrentRecordFor(uint32 newRecord, uint32 mapId, std::vector<uint64> participants, Mythic::MythicTimerType type)
//{
//    if (participants.empty())
//        return;
//
//    std::ostringstream stream;
//    std::vector<std::string> _names;
//
//    for (uint64 guid : participants)
//    {
//        GlobalPlayerData const* data = sWorld->GetGlobalPlayerData(guid);
//
//        if (data)
//        {
//            stream << data->name << ";";
//            _names.push_back(data->name);
//        }
//    }
//
//    std::lock_guard lock(m_lock);
//    PreparedStatement* stmt = nullptr;
//    switch (type)
//    {
//        case Mythic::MYTHIC_GOLD_TIME:
//            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_MYTHIC_GOLD_RECORD);
//            m_mythicMapStore[mapId].CurrentGoldRecord = newRecord;
//            break;
//        case Mythic::MYTHIC_SILVER_TIME:
//            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_MYTHIC_SILVER_RECORD);
//            m_mythicMapStore[mapId].CurrentSilverRecord = newRecord;
//            break;
//        case Mythic::MYTHIC_BRONZE_TIME:
//            stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_MYTHIC_BRONZE_RECORD);
//            m_mythicMapStore[mapId].CurrentBronzeRecord = newRecord;
//            break;
//    }
//
//    m_mythicMapStore[mapId].RecordHolders[type] = std::move(_names);
//
//    stmt->setUInt32(0, newRecord);
//    stmt->setString(1, stream.str());
//    stmt->setUInt32(2, mapId);
//    CharacterDatabase.Execute(stmt);
//}

bool MythicMgr::IsMythicAvailableForMap(uint32 mapId) const
{
    //! current mapids
    auto it = std::find(m_currentMythicMaps.begin(), m_currentMythicMaps.end(), mapId);
    return it != m_currentMythicMaps.end();
}

void MythicMgr::HandleMythicApiCall(Mythic::MythicApiCalls /*call*/)
{
    sLog->outString("bla bla bla dupa, nie zaimplementowales mythic api");
}

void MythicMgr::MythicPowerLog(uint32 guid, Mythic::MythicPowerLogType type, uint32 data1, uint32 data2, int32 value)
{
    int32 _value = type == Mythic::MythicPowerLogType::MYTHIC_POWER_LOG_INSTANCE_FINISHED ? value : -value;
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_MYTHICPOWER_LOG);
    stmt->setUInt32(0, guid);
    stmt->setUInt32(1, static_cast<uint32>(type));
    stmt->setUInt32(2, data1);
    stmt->setInt32(3, data2);
    stmt->setInt32(4, _value);
    CharacterDatabase.Execute(stmt);
}

Optional<Position> MythicMgr::GetEventCrystalPosition(uint32 mapId, uint32 index)
{
    // Nexus
    switch(mapId)
    {
        case 576: // Nexus
        {
            switch (index)
            {
                case 0:
                    return { {284.29f, -83.34f, -16.63f, 4.75f} };
                case 1:
                    return { {288.28f, -83.18f, -12.63f, 4.75f} };
                case 2:
                    return { {292.28f, -83.01f, -12.63f, 4.75f} };
                case 3:
                    return { {295.17f, -81.95f, -16.63f, 5.01f} };
                case 4: // npc
                    return { {289.99f, -86.10f, -14.30f, 1.71f} };
            }
            break;
        }
        case 209: // Zul'Farrak
            switch (index)
            {
                case 0:
                    return { { 1865.50f, 1144.16f, 14.30f, 0.78f } };
                case 1:
                    return { { 1862.68f, 1147.37f, 18.30f, 0.67f } };
                case 2:
                    return { { 1859.89f, 1150.89f, 18.30f, 0.67f } };
                case 3:
                    return { { 1856.84f, 1154.74f, 14.30f, 0.67f } };
                case 4: // npc
                    return { {1866.21f, 1153.98f, 12.23f, 3.95f} };
            }
        case 542: // Blood Furnance
            switch (index)
            {
            case 0:
                return { { 347.66f, -162.60f, -25.51f, 4.98f } };
            case 1:
                return { { 345.74f, -163.14f, -21.51f, 4.98f } };
            case 2:
                return { { 343.81f, -163.69f, -21.51f, 4.98f } };
            case 3:
                return { { 341.89f, -164.23f, -25.51f, 4.98f } };
            case 4: // npc
                return { {347.16f, -173.09f, -25.51f, 1.77f} };
            }
    }

    return {};
}

Optional<Position> MythicMgr::GetEventLostNpcPosition(uint32 mapId, uint32 index)
{
    // Nexus
    switch (mapId)
    {
        case 576: // Nexus
        {
            switch (index)
            {
                case 0:
                    return { {394.39f, 142.69f, -35.02f, 2.19f} };
                case 1:
                    return { {540.69f, 52.82f, -16.63f, 1.19f} };
                case 2:
                    return { {541.20f, 12.33f, -15.02f, 4.83f} };
                case 3:
                    return { {725.85f, -124.04f, -28.97f, 4.08f} };
                case 4:
                    return { {354.70f, -209.04f, -14.45f, 0.95f} };
                case 5:
                    return { {290.75f, -148.29f, -16.33f, 5.61f} };
                case 6: // main npc
                    return { {174.72f, -10.74f, -16.63f, 0.39f} };
            }
            break;
        }
        case 542: // Blood Furnance
        {
            switch (index)
            {
            case 0:
                return { { 6.24f, -106.80f, -41.32f, 3.03f } };
            case 1:
                return { { 253.27f, -84.58f, 9.64f, 3.06f } };
            case 2:
                return { { 356.40f, 83.03f, 9.61f, 3.21f } };
            case 3:
                return { { 425.40f, 134.90f, 9.59f, 5.44f } };
            case 4:
                return { { 499.86f, -17.66f, 9.55f, 2.64f } };
            case 5:
                return { { 313.48f, -162.66f, -25.51f, 5.21f } };
            case 6: // main npc
                return { { -8.64f, 8.83f, -44.64f, 4.82f } };
            }
            break;
        }
        case 209: // Zul'Farrak
        {
            switch (index)
            {
            case 0:
                return { { 1663.36f, 1185.58f, -2.88f, 0.f } };
            case 1:
                return { { 1671.00f, 901.56f, 9.67f, 0.f } };
            case 2:
                return { { 1796.27f, 701.16f, 28.09f, 1.25f } };
            case 3:
                return { { 1907.96f, 1008.04f, 11.51f, 0.f  } };
            case 4:
                return { { 1884.46f, 1338.46f, 28.53f, 0.f } };
            case 5:
                return { { 1913.62f, 1132.36f, 8.94f, 3.09f } };
            case 6: // main npc
                return { { 1217.93f, 846.13f, 9.49f, 0.10f } };
            }
            break;
        }
    }

    return {};
}

uint32 MythicMgr::GetEventLostNpcEntry(uint32 mapId, uint32 type)
{
    // Nexus
    switch (mapId)
    {
        case 576: // Nexus
        {
            switch (type)
            {
                case 0: // Main NPC
                    return 230001;
                case 1: // Lost NPC
                    return 230010;
            }
            break;
        }
        case 542: // Blood Furnance
        {
            switch (type)
            {
            case 0: // Main NPC
                return 230002;
            case 1: // Lost NPC
                return 230011;
            }
            break;
        }
        case 209: // Zul'Farrak
        {
            switch (type)
            {
            case 0: // Main NPC
                return 230003;
            case 1: // Lost NPC
                return 230012;
            }
            break;
        }
    }

    return 0;
}

Optional<Position> MythicMgr::GetEventAlchemistPosition(uint32 mapId, uint32 index)
{
    switch (mapId)
    {
        case 576: // Nexus
        {
            switch (index)
            {
                case 0:
                    return { {393.86f, 144.09f, -35.01f, 2.20f} };
                case 1:
                    return { {634.60f, -326.69f, -9.60f, 1.24f} };
                case 2:
                    return { {292.41f, -163.47f, -14.26f, 0.64f} };
                case 3: // main npc
                    return { {174.72f, -10.74f, -16.63f, 0.39f} };
            }
            break;
        }
        case 542: // Blood Furnance
        {
            switch (index)
            {
                case 0:
                    return { {349.04f, 122.16f, 10.90f, 6.05f} };
                case 1:
                    return { {482.33f, 132.40f, 9.60f, 4.72f} };
                case 2:
                    return { {424.22f, -18.68f, 10.85f, 0.87f} };
                case 3: // main npc
                    return { { -8.64f, 8.83f, -44.64f, 4.82f } };
            }
            break;
        }
        case 209: // Zul'Farrak
        {
            switch (index)
            {
                case 0:
                    return { {1638.62f, 818.70f, 8.98f, 5.31f} };
                case 1:
                    return { {1803.30f, 869.91f, 9.89f, 2.44f} };
                case 2:
                    return { {1540.12f, 996.14f, 8.87f, 4.69f} };
                case 3: // main npc
                    return { { 1217.93f, 846.13f, 9.49f, 0.10f } };
            }
            break;
        }
    }
    return {};
}

Mythic::MythicGroupConvertErrors MythicMgr::ConvertGroupToMythic(Group* group)
{
    if (!group)
        return Mythic::MYTHIC_GROUP_ERROR_NO_GROUP;

    if (group->IsMythicGroup())
        return Mythic::MYTHIC_GROUP_ERROR_ALREADY_MYTHIC;

    if (group->isBFGroup() || group->isBGGroup() || group->isLFGGroup())
        return Mythic::MYTHIC_GROUP_ERROR_LFG_BG_BF_GROUP;

    if (group->IsLfgRandomInstance())
        return Mythic::MYTHIC_GROUP_ERROR_LFG_BG_BF_GROUP;

    group->SetMythicGroup(true);
    return Mythic::MYTHIC_GROUP_ERROR_OK;
}

void MythicMgr::LoadMythicMapData()
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MYTHIC_MAP_DATA);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);

    //! If mythic is enabled, we have to have some data about maps, otherwise mythic wont work
    //! Disable mythic in config if you're hitting this assert locally
    if (!result)
        return;

    do
    {
        Field* field = result->Fetch();
        uint32 _mapId = field[0].GetUInt32();
        uint32 _keyLevel = field[1].GetUInt32();
        uint32 _timeType = field[2].GetUInt32();
        uint32 _timeMs = field[3].GetUInt32();
        Mythic::MythicRecordsStore _store;
        std::string _players = field[4].GetString();
        if (!_players.empty())
        {
            Tokenizer tokens(_players, ';');
            for (auto&& token : tokens)
                _store.emplace_back(token);
        }

        switch (_timeType)
        {
            case 1:
            {
                m_mythicMapStore[_mapId][_keyLevel].goldTime = _timeMs;
                m_mythicMapStore[_mapId][_keyLevel].goldPlayers = std::move( _store );
                break;
            }
            case 2:
            {
                m_mythicMapStore[_mapId][_keyLevel].silverTime = _timeMs;
                m_mythicMapStore[_mapId][_keyLevel].silverPlayers = std::move( _store );
                break;
            }
            case 3:
            {
                m_mythicMapStore[_mapId][_keyLevel].bronzeTime = _timeMs;
                m_mythicMapStore[_mapId][_keyLevel].bronzePlayers = std::move( _store );
                break;
            }
        }
    } while (result->NextRow());
}

void MythicMgr::TeleportToMythicDestination(uint32 instanceMapId, Player* player)
{
    Position dest;
    uint32 destMapId;

    switch (instanceMapId)
    {
        case 209:
            dest = { -6799.261719f, -2890.528809f, 8.882697f };
            destMapId = 1;
            break;
        case 576:
            dest = { 3783.67f, 6958.20f, 104.95f, 0.374755f };
            destMapId = 571;
            break;
        case 542:
            dest = { -295.044f, 3149.7f, 31.6503f, 2.13083f };
            destMapId = 530;
            break;
        default:
            return;
    }

    player->TeleportTo(destMapId, dest);
}

Position const& MythicMgr::GetMythicStoneSpawnpositionFor(uint32 instanceMapId)
{
    auto it = std::find_if(_mythicStoneMapPositions.begin(), _mythicStoneMapPositions.end(), [instanceMapId](std::pair<uint32, Position>& dest)
    {
        return dest.first == instanceMapId;
    });

    ASSERT(it != _mythicStoneMapPositions.end());
    return it->second;
}

bool MythicMgr::GetSpellValuesFor(uint32 spellId, Mythic::MythicSpells& data) const
{
    auto it = m_mythicSpellValueStore.find(spellId);
    if (it == m_mythicSpellValueStore.end())
        return false;

    data = it->second;
    return true;
}

void MythicMgr::LoadCurrentAffixSet(PreparedQueryResult& res)
{
    //! set default to 1
    m_currentAffixPresetId = 1;
    if (!res)
        return;

    Field* field = res->Fetch();
    m_currentAffixPresetId = std::stoul(field[0].GetString());
}

void MythicMgr::LoadAffixData()
{
    PreparedStatement* stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MYTHIC_AFFIX_DATA);
    ASSERT(stmt);
    PreparedQueryResult result = WorldDatabase.Query(stmt);
    ASSERT(result);

    //! Loads specific affix data
    do
    {
        Field* field = result->Fetch();
        Mythic::MythicAffixData data;
        uint32 AffixId = field[0].GetUInt32();
        data.AffixId = AffixId;
        data.AffixName = field[1].GetString();
        data.AffixFlags = field[2].GetUInt32();
        data.AffixChance = field[3].GetUInt32();
        data.AffixCooldown = field[4].GetUInt32();
        data.AffixKeyRequirement = field[5].GetUInt32();
        data.IsInstanceAffix = static_cast<bool>(field[6].GetUInt32());

        m_mythicAffixStore[AffixId] = std::move(data);
    } while (result->NextRow());


    //! Loads affix preset
    //! preset contains a seuqnece of affixes that is supposed to be used at that specific time
    stmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_MYTHIC_PRESETS);
    ASSERT(stmt);
    PreparedQueryResult presetResult = WorldDatabase.Query(stmt);
    ASSERT(presetResult);

    do
    {
        Field* field = presetResult->Fetch();
        uint32 presetId = field[0].GetUInt32();
        Mythic::MythicPresetData data;
        data.FirstAffix = field[1].GetUInt32();
        data.SecondAffix = field[2].GetUInt32();
        data.ThirdAffix = field[3].GetUInt32();
        data.FourthAffix = field[4].GetUInt32();
        m_mythicAffixPresets[presetId] = std::move(data);
    } while (presetResult->NextRow());
}

/** Riztazz note:
    We've got a list of mythic affixes and a set presets that uses those affixes
    preset contains 4 affixes, one of each key bracket which you can configure via config.
    Later, in MythicScaling class we filter out which affixes do not qualify for current mythic
    due to key restrictions or any other you might have in the future.
**/

Optional<Mythic::MythicAffixData const> MythicMgr::GetSingleAffix(uint32 affixId)
{
    auto it = m_mythicAffixStore.find(affixId);
    if (it == m_mythicAffixStore.end())
        return { };

    return it->second;
}

void MythicMgr::GetMythicAffixesForCreature(std::vector<Mythic::MythicAffixData>& store)
{
    auto presetIt = m_mythicAffixPresets.find(m_currentAffixPresetId);
    ASSERT(presetIt != m_mythicAffixPresets.end());

    uint32 firstAffix = presetIt->second.FirstAffix;
    uint32 secondAffix = presetIt->second.SecondAffix;
    uint32 thirdAffix = presetIt->second.ThirdAffix;
    uint32 fourthAffix = presetIt->second.FourthAffix;

    for (auto const& affixId : { firstAffix, secondAffix, thirdAffix, fourthAffix })
    {
        auto it = m_mythicAffixStore.find(affixId);
        //! Affix might not exist if we set it to zero in database
        //! we might want a preset that only has ie. one affix
        if (it != m_mythicAffixStore.end())
        {
            if (!it->second.IsInstanceAffix)
                store.push_back(it->second);
        }
    }
}

void MythicMgr::GetMythicAffixesForInstance(std::vector<Mythic::MythicAffixData>& store)
{
    auto presetIt = m_mythicAffixPresets.find(m_currentAffixPresetId);
    ASSERT(presetIt != m_mythicAffixPresets.end());

    uint32 firstAffix = presetIt->second.FirstAffix;
    uint32 secondAffix = presetIt->second.SecondAffix;
    uint32 thirdAffix = presetIt->second.ThirdAffix;
    uint32 fourthAffix = presetIt->second.FourthAffix;

    for (auto const& affixId : { firstAffix, secondAffix, thirdAffix, fourthAffix })
    {
        auto it = m_mythicAffixStore.find(affixId);
        //! Affix might not exist if we set it to zero in database
        //! we might want a preset that only has ie. one affix
        if (it != m_mythicAffixStore.end())
        {
            if (it->second.IsInstanceAffix)
                store.push_back(it->second);
        }
    }
}

// TODO: move entries to database and allow reloading
enum ProhibitedType
{
    PROHIBITED_TYPE_SPAWNID = 0,
    PROHIBITED_TYPE_ENTRY,
};

struct ProhibitedEntry
{
    ProhibitedType type = PROHIBITED_TYPE_SPAWNID;
    uint32 value         = 0;
};

std::vector<ProhibitedEntry> const ProhibitedEntries = {
    /// The Nexus
    { PROHIBITED_TYPE_SPAWNID, 126667 }, // Hall of Stasis
    { PROHIBITED_TYPE_SPAWNID, 126665 },
    { PROHIBITED_TYPE_SPAWNID, 126472 }, // After Hall of Stasis and before Telestra
    { PROHIBITED_TYPE_SPAWNID, 126459 },
    { PROHIBITED_TYPE_SPAWNID, 126467 },
    { PROHIBITED_TYPE_SPAWNID, 126465 },
    { PROHIBITED_TYPE_SPAWNID, 126460 }, // After Telestra and before Anomalus
    { PROHIBITED_TYPE_SPAWNID, 126468 },
    { PROHIBITED_TYPE_SPAWNID, 126473 },
    { PROHIBITED_TYPE_SPAWNID, 126495 },
    { PROHIBITED_TYPE_SPAWNID, 126491 },
    { PROHIBITED_TYPE_SPAWNID, 126518 },
    { PROHIBITED_TYPE_SPAWNID, 126486 },
    { PROHIBITED_TYPE_SPAWNID, 126487 },
    { PROHIBITED_TYPE_SPAWNID, 126483 },
    { PROHIBITED_TYPE_SPAWNID, 126510 },
    { PROHIBITED_TYPE_SPAWNID, 126509 },
    { PROHIBITED_TYPE_SPAWNID, 126489 }, // After Anomalus and before Gromrok
    { PROHIBITED_TYPE_SPAWNID, 126498 },
    { PROHIBITED_TYPE_SPAWNID, 126646 },
    { PROHIBITED_TYPE_SPAWNID, 126634 },
    { PROHIBITED_TYPE_SPAWNID, 126648 },
    { PROHIBITED_TYPE_SPAWNID, 126636 },
    { PROHIBITED_TYPE_SPAWNID, 126624 },
    { PROHIBITED_TYPE_SPAWNID, 126656 },
    { PROHIBITED_TYPE_SPAWNID, 126653 },
    { PROHIBITED_TYPE_SPAWNID, 126644 },
    { PROHIBITED_TYPE_SPAWNID, 126658 },
    { PROHIBITED_TYPE_SPAWNID, 126651 },
    { PROHIBITED_TYPE_SPAWNID, 126650 },
    { PROHIBITED_TYPE_SPAWNID, 126630 },
    { PROHIBITED_TYPE_SPAWNID, 126626 },
    { PROHIBITED_TYPE_SPAWNID, 126659 },
    { PROHIBITED_TYPE_SPAWNID, 126642 },
    { PROHIBITED_TYPE_SPAWNID, 126660 },
    { PROHIBITED_TYPE_SPAWNID, 126645 },
    { PROHIBITED_TYPE_SPAWNID, 126662 },
    { PROHIBITED_TYPE_SPAWNID, 126657 },
    { PROHIBITED_TYPE_SPAWNID, 126627 },
    { PROHIBITED_TYPE_SPAWNID, 126631 },
    { PROHIBITED_TYPE_SPAWNID, 126639 },
    { PROHIBITED_TYPE_SPAWNID, 126632 },
    { PROHIBITED_TYPE_SPAWNID, 126635 },
    // Zul'farrak
    { PROHIBITED_TYPE_SPAWNID, 38012 },
    { PROHIBITED_TYPE_SPAWNID, 38013 },
    { PROHIBITED_TYPE_SPAWNID, 42597 },
    { PROHIBITED_TYPE_SPAWNID, 42596 },
    { PROHIBITED_TYPE_SPAWNID, 42605 },
    { PROHIBITED_TYPE_SPAWNID, 42607 },
    { PROHIBITED_TYPE_SPAWNID, 42599 },
    { PROHIBITED_TYPE_SPAWNID, 42611 },
    { PROHIBITED_TYPE_SPAWNID, 42612 },
    { PROHIBITED_TYPE_SPAWNID, 42614 },
    { PROHIBITED_TYPE_SPAWNID, 42617 },
    { PROHIBITED_TYPE_SPAWNID, 42623 },
    { PROHIBITED_TYPE_SPAWNID, 42627 },
    { PROHIBITED_TYPE_SPAWNID, 43704 },
    { PROHIBITED_TYPE_SPAWNID, 44163 },
    { PROHIBITED_TYPE_SPAWNID, 81444 },
    { PROHIBITED_TYPE_SPAWNID, 81440 },
    { PROHIBITED_TYPE_SPAWNID, 81443 },
    { PROHIBITED_TYPE_SPAWNID, 81609 },
    { PROHIBITED_TYPE_SPAWNID, 81608 },
    { PROHIBITED_TYPE_SPAWNID, 81575 },
    { PROHIBITED_TYPE_SPAWNID, 81569 },
    { PROHIBITED_TYPE_SPAWNID, 81566 },
    { PROHIBITED_TYPE_SPAWNID, 81580 },
    { PROHIBITED_TYPE_SPAWNID, 81589 },
    { PROHIBITED_TYPE_SPAWNID, 81579 },
    { PROHIBITED_TYPE_SPAWNID, 81598 },
    { PROHIBITED_TYPE_SPAWNID, 81578 },
    { PROHIBITED_TYPE_SPAWNID, 81568 },
    { PROHIBITED_TYPE_SPAWNID, 81539 },
    { PROHIBITED_TYPE_SPAWNID, 81543 },
    { PROHIBITED_TYPE_SPAWNID, 81551 },
    { PROHIBITED_TYPE_SPAWNID, 81549 },
    { PROHIBITED_TYPE_SPAWNID, 81548 },
    { PROHIBITED_TYPE_SPAWNID, 81542 },
    { PROHIBITED_TYPE_SPAWNID, 81536 },
    { PROHIBITED_TYPE_SPAWNID, 81538 },
    { PROHIBITED_TYPE_SPAWNID, 81462 },
    { PROHIBITED_TYPE_SPAWNID, 81468 },
    { PROHIBITED_TYPE_SPAWNID, 81452 },
    { PROHIBITED_TYPE_SPAWNID, 81563 }
};

bool MythicMgr::ShouldBeSpawned(Creature* creature)
{
    for (ProhibitedEntry entry : ProhibitedEntries)
    {
        if (entry.type == PROHIBITED_TYPE_SPAWNID && creature->GetDBTableGUIDLow() == entry.value)
            return false;
        if (entry.type == PROHIBITED_TYPE_ENTRY && creature->GetEntry() == entry.value)
            return false;
    }

    return true;
}

uint32 MythicMgr::GetBestMythicTimeFor(uint32 mapId, uint32 keyLevel)
{
    std::lock_guard lock(m_lock);

    auto mapIt = m_mythicMapStore.find( mapId );
    if (mapIt == m_mythicMapStore.end())
        return 0;

    auto keyIt = mapIt->second.find( keyLevel );
    if (keyIt == mapIt->second.end())
        return 0;

    return keyIt->second.goldTime;
}

uint32 MythicMgr::GetSilverBracketTimeFor( uint32 mapId, uint32 keyLevel )
{
    std::lock_guard lock( m_lock );

    auto mapIt = m_mythicMapStore.find( mapId );
    if (mapIt == m_mythicMapStore.end())
        return 0;

    auto keyIt = m_mythicMapStore[mapId].find( keyLevel );
    if (keyIt == m_mythicMapStore[mapId].end())
        return 0;

    return m_mythicMapStore[mapId][keyLevel].silverTime;
}

uint32 MythicMgr::GetBronzeBracketTimeFor( uint32 mapId, uint32 keyLevel )
{
    std::lock_guard lock( m_lock );

    auto mapIt = m_mythicMapStore.find( mapId );
    if (mapIt == m_mythicMapStore.end())
        return 0;

    auto keyIt = m_mythicMapStore[mapId].find( keyLevel );
    if (keyIt == m_mythicMapStore[mapId].end())
        return 0;

    return m_mythicMapStore[mapId][keyLevel].bronzeTime;
}

void MythicMgr::SaveRecords( uint32 mapId, uint32 keyLevel, uint32 timeType, uint32 time, Mythic::MythicRecordsStore players, bool populateTable /* false */)
{
    bool _dbSave = timeType >= 0 && timeType < 4;
    std::string _playerDbString = "";
    if (_dbSave)
    {
        for (auto const& el : players)
            _playerDbString += el + ";";
    }

    if (_playerDbString.empty())
        return;

    _playerDbString.pop_back();

    uint32 _realTimeType = timeType;
    if ( !_realTimeType ) // equals zero so a best time
        _realTimeType = 1;

    std::lock_guard lock(m_lock);

    switch (_realTimeType)
    {
        case 1:
        {
            m_mythicMapStore[mapId][keyLevel].goldTime = time;
            m_mythicMapStore[mapId][keyLevel].goldPlayers = std::move(players);
            break;
        }
        case 2:
        {
            m_mythicMapStore[mapId][keyLevel].silverTime = time;
            m_mythicMapStore[mapId][keyLevel].silverPlayers = std::move( players );
            break;
        }
        case 3:
        {
            m_mythicMapStore[mapId][keyLevel].bronzeTime = time;
            m_mythicMapStore[mapId][keyLevel].bronzePlayers = std::move( players );
            break;
        }
        default:
        {
            break;
        }
    }

    if (_dbSave)
    {
        RecordData realRecord{ mapId, keyLevel, _realTimeType, time, _playerDbString };
        m_recordStore.add( std::move( realRecord ) );

        //! Will only happen once per key during realm lifetime
        if ( populateTable )
        {
            uint32 newSilverTime = time * 1.5f;
            uint32 newBronzeTime = time * 2.f;

            RecordData silverRecord{ mapId, keyLevel, 2, newSilverTime, _playerDbString };
            RecordData bronzeRecord{ mapId, keyLevel, 3, newBronzeTime, _playerDbString };

            m_recordStore.add( std::move( silverRecord ) );
            m_recordStore.add( std::move( bronzeRecord ) );
        }
    }
}

void MythicMgr::HandleAsyncQueries()
{
    if ( m_currentHandledFuture )
    {
        if (isFutureReady(m_currentHandledFuture.get()))
        {
            m_currentHandledFuture.reset();
            if ( !m_recordStore.empty() )
            {
                //m_recordStore.lock();

                RecordData data;
                if ( m_recordStore.next( data ) )
                {
                    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement( CHAR_REP_MYTHIC_RECORD );
                    stmt->setUInt32( 0, data.MapId );
                    stmt->setUInt32( 1, data.KeyLevel );
                    stmt->setUInt32( 2, data.Type );
                    stmt->setUInt32( 3, data.Time );
                    stmt->setString( 4, data.Players );
                    m_currentHandledFuture = CharacterDatabase.AsyncQuery( stmt );
                }

                //m_recordStore.unlock();
            }
        }
    }
    else if ( !m_recordStore.empty() )
    {
        //! alert, code duplication \o/
        //m_recordStore.lock();

        RecordData data;
        if ( m_recordStore.next( data ) )
        {

            PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement( CHAR_REP_MYTHIC_RECORD );
            stmt->setUInt32( 0, data.MapId );
            stmt->setUInt32( 1, data.KeyLevel );
            stmt->setUInt32( 2, data.Type );
            stmt->setUInt32( 3, data.Time );
            stmt->setString( 4, data.Players );
            m_currentHandledFuture = CharacterDatabase.AsyncQuery( stmt );
        }

        //m_recordStore.unlock();
    }

    /*if (m_currentHandledFuture)
    {
        if (m_currentHandledFuture->ready())
        {
            m_currentHandledFuture->cancel();
            m_currentHandledFuture.reset();
        }

        return;
    }

    if (m_mythicQueryStore.empty())
        return;
    std::cout << "wchodze w save" << std::endl;
    Mythic::NewRecordData data = m_mythicQueryStore.front();
    m_mythicQueryStore.pop_front();

    if (data.type == Mythic::MythicTimeType::TIME_TYPE_BEST)
        data.type = Mythic::MythicTimeType::TIME_TYPE_GOLD;

    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement( CHAR_REP_MYTHIC_RECORD );

    stmt->setUInt32( 0, data.mapId );
    stmt->setUInt32( 1, data.keyLevel );
    stmt->setUInt32( 2, uint32( data.type ) );
    stmt->setUInt32( 3, data.time );
    std::string _recordString = "";
    for (auto const& el : data.store)
        _recordString += el + ";";

    _recordString.pop_back();
    stmt->setString( 4, _recordString );

    m_currentHandledFuture = CharacterDatabase.AsyncQuery( stmt );*/
}

