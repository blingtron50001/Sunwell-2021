#ifndef SC_ACMGR_H
#define SC_ACMGR_H

#include "Common.h"
#include "SharedDefines.h"
#include "ScriptMgr.h"
#include "AnticheatData.h"
#include "Chat.h"

class Player;
class AnticheatData;

enum ReportTypes
{
    SPEED_HACK_REPORT          = 0x00,
    FLY_HACK_REPORT            = 0x01,
    WALK_WATER_HACK_REPORT     = 0x02,
    JUMP_HACK_REPORT           = 0x03,
    TELEPORT_PLANE_HACK_REPORT = 0x04,
    CLIMB_HACK_REPORT          = 0x05
    // MAX_REPORT_TYPES
};

enum DetectionTypes
{
    SPEED_HACK_DETECTION          = 0x01,
    FLY_HACK_DETECTION            = 0x02,
    WALK_WATER_HACK_DETECTION     = 0x04,
    JUMP_HACK_DETECTION           = 0x08,
    TELEPORT_PLANE_HACK_DETECTION = 0x10,
    CLIMB_HACK_DETECTION          = 0x20
};

// GUIDLow is the key.
typedef std::map<uint32, AnticheatData> AnticheatPlayersDataMap;

class AnticheatMgr
{
    AnticheatMgr();
    ~AnticheatMgr();

public:
    static AnticheatMgr* instance()
    {
        static AnticheatMgr* instance = new AnticheatMgr();
        return instance;
    }

    void StartHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode);
    void DeletePlayerReport(Player* player, bool login);
    void DeletePlayerData(Player* player);
    void CreatePlayerData(Player* player);
    void SavePlayerData(Player* player);

    void StartScripts();

    void HandlePlayerLogin(Player* player);
    void HandlePlayerLogout(Player* player);

    uint32 GetTotalReports(uint32 lowGUID);
    float GetAverage(uint32 lowGUID);
    uint32 GetTypeReports(uint32 lowGUID, uint8 type);

    void AnticheatGlobalCommand(ChatHandler* handler);
    void AnticheatDeleteCommand(uint32 guid);

    void ResetDailyReportStates();

private:
    void SpeedHackDetection(Player* player, MovementInfo movementInfo);
    //void FlyHackDetection(Player* player, MovementInfo movementInfo);
    //void WalkOnWaterHackDetection(Player* player, MovementInfo movementInfo);
    void JumpHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode);
    void TeleportPlaneHackDetection(Player* player, MovementInfo);
    void ClimbHackDetection(Player* player, MovementInfo movementInfo, uint32 opcode);

    void BuildReport(Player* player, uint8 reportType);

    bool MustCheckTempReports(uint8 type);

    AnticheatPlayersDataMap m_Players; ///< Player data
};

#define sAnticheatMgr AnticheatMgr::instance()

#endif
