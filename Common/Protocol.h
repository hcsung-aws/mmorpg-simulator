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

    // Chat (0x06xx)
    CS_CHAT = 0x0601,
    SC_CHAT = 0x0602,

    // Item (0x07xx)
    SC_ITEM_DROP = 0x0701,
    CS_ITEM_USE = 0x0702,
    SC_ITEM_USE_RESULT = 0x0703,
    CS_ITEM_EQUIP = 0x0704,
    SC_EQUIP_RESULT = 0x0705,
    SC_INVENTORY_UPDATE = 0x0706,
    SC_INVENTORY_LIST = 0x0707,

    // Shop (0x08xx)
    CS_SHOP_OPEN = 0x0801,
    SC_SHOP_LIST = 0x0802,
    CS_SHOP_BUY = 0x0803,
    SC_SHOP_RESULT = 0x0804,
    CS_SHOP_SELL = 0x0805,

    // Quest (0x0Axx)
    CS_QUEST_LIST = 0x0A01,
    SC_QUEST_LIST = 0x0A02,
    CS_QUEST_ACCEPT = 0x0A03,
    SC_QUEST_ACCEPT_RESULT = 0x0A04,
    SC_QUEST_PROGRESS = 0x0A05,
    CS_QUEST_COMPLETE = 0x0A06,
    SC_QUEST_REWARD = 0x0A07,

    // Party (0x0Bxx)
    CS_PARTY_INVITE = 0x0B01,
    SC_PARTY_INVITE = 0x0B02,
    CS_PARTY_ACCEPT = 0x0B03,
    SC_PARTY_UPDATE = 0x0B04,
    CS_PARTY_LEAVE = 0x0B05,
    SC_PARTY_LEAVE = 0x0B06,

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
    uint32_t goldReward;
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

// Chat
struct CS_Chat {
    uint8_t channel;        // 0=World, 1=Whisper
    char targetName[20];    // Whisper target (channel==1)
    char message[128];
};

struct SC_Chat {
    uint8_t channel;
    char senderName[20];
    char message[128];
};

// Item
struct SC_ItemDrop {
    uint8_t slot;
    uint16_t itemId;
    char itemName[32];
};

struct CS_ItemUse {
    uint8_t slot;
};

struct SC_ItemUseResult {
    uint64_t charUid;
    char charName[20];
    char itemName[32];
    uint8_t effectType;     // 0=hp
    uint16_t effectValue;
};

struct CS_ItemEquip {
    uint8_t slot;
};

struct SC_EquipResult {
    uint8_t success;
    uint8_t slot;
    uint16_t atk;
    uint16_t def;
    char weaponName[32];
    char armorName[32];
    char message[32];
};

struct SC_InventoryUpdate {
    uint8_t slot;
    uint16_t itemId;        // 0=empty
    char itemName[32];
};

constexpr int MAX_INVENTORY = 20;

struct SC_InventoryList {
    uint8_t count;
    SC_InventoryUpdate items[MAX_INVENTORY];
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

// Shop
struct SC_ShopEntry {
    uint16_t itemId;
    char itemName[32];
    uint32_t price;
};

struct SC_ShopList {
    uint8_t count;
    SC_ShopEntry items[10];   // max 10 shop items
};

struct CS_ShopBuy {
    uint16_t itemId;
};

struct SC_ShopResult {
    uint8_t success;          // 0=fail, 1=bought, 2=sold
    uint16_t itemId;
    uint64_t remainGold;
    char message[32];
};

struct CS_ShopSell {
    uint8_t slot;
};

// Quest
struct CS_QuestList {
    uint64_t npcUid;
};

struct SC_QuestEntry {
    uint16_t questId;
    char name[32];
    uint8_t status;       // 0=Available, 1=Accepted, 2=Completable, 3=Completed
    uint16_t progress;
    uint16_t target;
};

constexpr int MAX_QUEST_LIST = 10;

struct SC_QuestList {
    uint8_t count;
    SC_QuestEntry quests[MAX_QUEST_LIST];
};

struct CS_QuestAccept {
    uint16_t questId;
};

struct SC_QuestAcceptResult {
    uint8_t success;
    uint16_t questId;
    char message[64];
};

struct SC_QuestProgress {
    uint16_t questId;
    uint16_t progress;
    uint16_t target;
};

struct CS_QuestComplete {
    uint16_t questId;
};

struct SC_QuestReward {
    uint8_t success;
    uint16_t questId;
    uint32_t rewardGold;
    uint32_t rewardExp;
    uint16_t rewardItemId;
    char message[64];
};

// Party
struct CS_PartyInvite {
    char targetName[20];
};

struct SC_PartyInvite {
    char inviterName[20];
};

struct CS_PartyAccept {
    uint8_t accept;  // 1=accept, 0=reject
};

struct SC_PartyMember {
    uint64_t charUid;
    char name[20];
    uint16_t hp;
    uint16_t maxHp;
    int16_t posX;
    int16_t posY;
};

constexpr int MAX_PARTY = 4;

struct SC_PartyUpdate {
    uint8_t memberCount;
    SC_PartyMember members[MAX_PARTY];
};

struct SC_PartyLeave {
    uint64_t charUid;
};

#pragma pack(pop)
