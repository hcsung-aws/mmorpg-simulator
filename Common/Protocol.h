#pragma once
#include <cstdint>

// Packet Header
#pragma pack(push, 1)
struct PacketHeader {
    uint16_t length;  // Total packet length (header + payload)
    uint16_t type;    // Packet type
};
#pragma pack(pop)

constexpr size_t HEADER_SIZE = sizeof(PacketHeader);
constexpr size_t MAX_PACKET_SIZE = 4096;
constexpr size_t MAX_RECV_BUFFER_SIZE = 8192;

// Packet Types
enum PacketType : uint16_t {
    // Auth (0x01xx)
    CS_LOGIN = 0x0101,
    SC_LOGIN_RESULT = 0x0102,
    CS_LOGOUT = 0x0103,
    SC_LOGOUT_RESULT = 0x0104,

    // Character (0x02xx)
    CS_CHAR_LIST = 0x0201,
    SC_CHAR_LIST = 0x0202,
    CS_CHAR_CREATE = 0x0203,
    SC_CHAR_CREATE_RESULT = 0x0204,
    CS_CHAR_SELECT = 0x0205,
    SC_CHAR_INFO = 0x0206,
    SC_CHAR_LEAVE = 0x0207,

    // Attendance (0x03xx)
    CS_ATTENDANCE_CHECK = 0x0301,
    SC_ATTENDANCE_INFO = 0x0302,
    SC_ATTENDANCE_RESULT = 0x0303,

    // Move (0x04xx)
    CS_MOVE = 0x0401,
    SC_MOVE_RESULT = 0x0402,

    // Combat (0x05xx)
    CS_ATTACK = 0x0501,
    SC_ATTACK_RESULT = 0x0502,
    SC_NPC_SPAWN = 0x0503,
    SC_NPC_DEATH = 0x0504,
    SC_EXP_UPDATE = 0x0505,
    SC_LEVEL_UP = 0x0506,

    // System (0xFFxx)
    CS_HEARTBEAT = 0xFF01,
    SC_HEARTBEAT = 0xFF02,
    SC_ERROR = 0xFF03,
};

// Packet Payloads
#pragma pack(push, 1)

// Auth
struct CS_Login {
    char accountId[32];
    char password[32];
};

struct SC_LoginResult {
    uint8_t success;
    uint64_t accountUid;
    char message[64];
};

// Character List
struct SC_CharListEntry {
    uint64_t charUid;
    char name[20];
    uint8_t charType;
    uint16_t level;
};

struct SC_CharList {
    uint8_t count;
    SC_CharListEntry chars[5];  // max 5 characters
};

struct CS_CharCreate {
    uint64_t charUid;  // client generates: accountUid * 1000 + index
    char name[20];
    uint8_t charType;
};

struct SC_CharCreateResult {
    uint8_t success;
    uint64_t charUid;
    char message[32];
};

struct CS_CharSelect {
    uint64_t charUid;
};

// Character Info
struct SC_CharInfo {
    uint64_t charUid;
    char name[32];
    uint16_t level;
    uint32_t exp;
    int16_t posX;
    int16_t posY;
    uint16_t hp;
    uint16_t maxHp;
    uint64_t gold;
};

struct SC_CharLeave {
    uint64_t charUid;
};

// Move
struct CS_Move {
    int8_t dirX;
    int8_t dirY;
};

struct SC_MoveResult {
    uint8_t success;
    int16_t posX;
    int16_t posY;
};

// Combat
struct CS_Attack {
    uint64_t targetUid;
};

struct SC_AttackResult {
    uint8_t success;
    uint64_t targetUid;
    uint16_t damage;
    uint16_t targetHp;
};

// System
struct SC_Error {
    uint16_t errorCode;
    char message[64];
};

// NPC
struct SC_NpcSpawn {
    uint64_t npcUid;
    int16_t posX;
    int16_t posY;
    uint16_t hp;
    uint16_t maxHp;
};

struct SC_NpcDeath {
    uint64_t npcUid;
    uint32_t expReward;
};

struct SC_ExpUpdate {
    uint32_t exp;
    uint32_t maxExp;
};

struct SC_LevelUp {
    uint16_t level;
    uint16_t hp;
    uint16_t maxHp;
    uint16_t atk;
    uint16_t def;
};

// Attendance
struct SC_AttendanceInfo {
    uint8_t todayAttended;    // 0=not yet, 1=already attended
    uint32_t rewardGold;      // reward amount
};

struct SC_AttendanceResult {
    uint8_t success;          // 0=fail, 1=success
    uint32_t rewardGold;      // gold received
};

#pragma pack(pop)
