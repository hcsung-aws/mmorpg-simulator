#include <boost/asio.hpp>
#include "TcpServer.h"
#include "SessionManager.h"
#include "PacketHandler.h"
#include "DBConnection.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <vector>
#include <string>

// Simple JSON value parser (minimal, for items.json)
#include <sstream>
#include <map>

constexpr int MAP_SIZE = 20;
constexpr int MAX_NPC = 5;

// Forward declarations
void SendAttendanceInfo(std::shared_ptr<Session> session);

struct NPC {
    uint64_t uid;
    int16_t posX, posY;
    uint16_t hp, maxHp;
    uint16_t atk = 5, def = 2;
    uint32_t expReward = 30;
    uint32_t goldReward = 10;
    bool alive;
};

SessionManager sessionMgr;
DBConnection dbConn;
NPC npcs[MAX_NPC];
uint64_t nextNpcUid = 1;

// Item system
constexpr int MAX_INVENTORY = 20;

struct ItemData {
    uint16_t id;
    std::string name;
    std::string type;  // weapon, armor, consumable
    uint16_t atk = 0, def = 0, hp = 0;
    uint32_t price = 0;
};

struct DropEntry {
    uint16_t itemId;
    int weight;
};

std::vector<ItemData> g_items;
std::vector<DropEntry> g_dropTable;
int g_totalWeight = 0;

const ItemData* FindItem(uint16_t id) {
    for (auto& item : g_items)
        if (item.id == id) return &item;
    return nullptr;
}

// Per-session inventory (loaded from DB)
struct InvSlot {
    uint64_t itemUid = 0;   // DB ItemUid
    uint16_t itemTid = 0;   // item template ID (items.json id)
};
struct Inventory {
    InvSlot slots[MAX_INVENTORY] = {};
    uint64_t weaponUid = 0, armorUid = 0;  // equipped ItemUids
    uint16_t weaponTid = 0, armorTid = 0;  // equipped ItemTids
};
std::map<SessionId, Inventory> g_inventories;

// Minimal JSON item loader (no external dependency)
bool LoadItems(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Parse items array
    auto parseStr = [&](const std::string& s, const std::string& key) -> std::string {
        auto pos = s.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = s.find("\"", pos + key.size() + 2);
        if (pos == std::string::npos) return "";
        auto end = s.find("\"", pos + 1);
        return s.substr(pos + 1, end - pos - 1);
    };
    auto parseInt = [&](const std::string& s, const std::string& key) -> int {
        auto pos = s.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0;
        pos = s.find(":", pos);
        while (pos < s.size() && (s[pos] == ':' || s[pos] == ' ')) pos++;
        return std::atoi(s.c_str() + pos);
    };

    // Find "items" array
    auto itemsStart = json.find("\"items\"");
    auto arrStart = json.find("[", itemsStart);
    auto arrEnd = json.find("]", arrStart);
    std::string itemsArr = json.substr(arrStart, arrEnd - arrStart + 1);

    size_t pos = 0;
    while ((pos = itemsArr.find("{", pos)) != std::string::npos) {
        auto objEnd = itemsArr.find("}", pos);
        std::string obj = itemsArr.substr(pos, objEnd - pos + 1);
        ItemData item;
        item.id = parseInt(obj, "id");
        item.name = parseStr(obj, "name");
        item.type = parseStr(obj, "type");
        item.atk = parseInt(obj, "atk");
        item.def = parseInt(obj, "def");
        item.hp = parseInt(obj, "hp");
        item.price = parseInt(obj, "price");
        g_items.push_back(item);
        pos = objEnd + 1;
    }

    // Find "dropTable" array
    auto dropStart = json.find("\"dropTable\"");
    if (dropStart != std::string::npos) {
        auto dArrStart = json.find("[", dropStart);
        auto dArrEnd = json.find("]", dArrStart);
        std::string dropArr = json.substr(dArrStart, dArrEnd - dArrStart + 1);
        pos = 0;
        while ((pos = dropArr.find("{", pos)) != std::string::npos) {
            auto objEnd = dropArr.find("}", pos);
            std::string obj = dropArr.substr(pos, objEnd - pos + 1);
            DropEntry de;
            de.itemId = parseInt(obj, "itemId");
            de.weight = parseInt(obj, "weight");
            g_dropTable.push_back(de);
            g_totalWeight += de.weight;
            pos = objEnd + 1;
        }
    }

    std::cout << "[Server] Loaded " << g_items.size() << " items, " << g_dropTable.size() << " drop entries" << std::endl;
    return !g_items.empty();
}

void LoadInventory(std::shared_ptr<Session> session) {
    auto& inv = g_inventories[session->GetId()];
    memset(&inv, 0, sizeof(inv));

    auto& player = session->GetPlayer();
    std::wstring procCall = L"CALL spInventoryLoad(" + std::to_wstring(player.charUid) + L")";

    dbConn.ExecuteProc(procCall, nullptr,
        [&](SQLHSTMT hStmt) -> bool {
            // Result set 1: owned items (ItemUid, ItemTid)
            SQLBIGINT itemUid; SQLBIGINT itemTid;
            SQLLEN ind1, ind2;
            SQLBindCol(hStmt, 1, SQL_C_SBIGINT, &itemUid, 0, &ind1);
            SQLBindCol(hStmt, 2, SQL_C_SBIGINT, &itemTid, 0, &ind2);
            int slot = 0;
            while (SQLFetch(hStmt) == SQL_SUCCESS && slot < MAX_INVENTORY) {
                inv.slots[slot].itemUid = itemUid;
                inv.slots[slot].itemTid = (uint16_t)itemTid;
                slot++;
            }
            // Result set 2: equipment (Slot, ItemUid, ItemTid)
            if (SQLMoreResults(hStmt) == SQL_SUCCESS) {
                SQLSMALLINT eqSlot; SQLBIGINT eqUid; SQLBIGINT eqTid;
                SQLLEN ind3, ind4, ind5;
                SQLBindCol(hStmt, 1, SQL_C_SSHORT, &eqSlot, 0, &ind3);
                SQLBindCol(hStmt, 2, SQL_C_SBIGINT, &eqUid, 0, &ind4);
                SQLBindCol(hStmt, 3, SQL_C_SBIGINT, &eqTid, 0, &ind5);
                while (SQLFetch(hStmt) == SQL_SUCCESS) {
                    if (eqSlot == 0) { inv.weaponUid = eqUid; inv.weaponTid = (uint16_t)eqTid; }
                    else if (eqSlot == 1) { inv.armorUid = eqUid; inv.armorTid = (uint16_t)eqTid; }
                }
            }
            return true;
        });

    // Apply equipment stats
    auto* wep = FindItem(inv.weaponTid);
    auto* arm = FindItem(inv.armorTid);
    if (wep) player.atk = 15 + wep->atk;
    if (arm) player.def = 5 + arm->def;

    // Send inventory to client
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (inv.slots[i].itemTid != 0) {
            auto* item = FindItem(inv.slots[i].itemTid);
            if (item) {
                SC_InventoryUpdate pkt{};
                pkt.slot = i;
                pkt.itemId = item->id;
                strncpy_s(pkt.itemName, item->name.c_str(), 31);
                session->Send(SC_INVENTORY_UPDATE, &pkt, sizeof(pkt));
            }
        }
    }

    // Send initial equipment info
    if (inv.weaponTid || inv.armorTid) {
        SC_EquipResult eq{};
        eq.success = 1;
        eq.atk = player.atk;
        eq.def = player.def;
        if (wep) strncpy_s(eq.weaponName, wep->name.c_str(), 31);
        if (arm) strncpy_s(eq.armorName, arm->name.c_str(), 31);
        session->Send(SC_EQUIP_RESULT, &eq, sizeof(eq));
    }
}

uint64_t AddItemToDB(uint64_t charUid, uint16_t itemTid) {
    uint64_t newUid = 0;
    std::wstring procCall = L"CALL spInventoryAdd(" + std::to_wstring(charUid) + L", " + std::to_wstring(itemTid) + L")";
    dbConn.ExecuteProc(procCall, nullptr,
        [&](SQLHSTMT hStmt) -> bool {
            SQLBIGINT uid = 0; SQLLEN ind;
            SQLBindCol(hStmt, 1, SQL_C_SBIGINT, &uid, 0, &ind);
            if (SQLFetch(hStmt) == SQL_SUCCESS) newUid = uid;
            return true;
        });
    return newUid;
}

void RemoveItemFromDB(uint64_t itemUid) {
    std::wstring procCall = L"CALL spInventoryRemove(" + std::to_wstring(itemUid) + L")";
    dbConn.ExecuteProc(procCall, nullptr, nullptr);
}

void EquipItemDB(uint64_t charUid, int slot, uint64_t itemUid) {
    std::wstring procCall = L"CALL spEquipItem(" + std::to_wstring(charUid) + L", " + std::to_wstring(slot) + L", " + std::to_wstring(itemUid) + L")";
    dbConn.ExecuteProc(procCall, nullptr, nullptr);
}

void UnequipItemDB(uint64_t charUid, int slot) {
    std::wstring procCall = L"CALL spUnequipItem(" + std::to_wstring(charUid) + L", " + std::to_wstring(slot) + L")";
    dbConn.ExecuteProc(procCall, nullptr, nullptr);
}

uint16_t RollDrop() {
    if (g_dropTable.empty()) return 0;
    int roll = rand() % g_totalWeight;
    int acc = 0;
    for (auto& de : g_dropTable) {
        acc += de.weight;
        if (roll < acc) return de.itemId;
    }
    return 0;
}

int FindEmptySlot(const Inventory& inv) {
    for (int i = 0; i < MAX_INVENTORY; i++)
        if (inv.slots[i].itemTid == 0) return i;
    return -1;
}

void SpawnNPC(int index) {
    npcs[index].uid = nextNpcUid++;
    npcs[index].posX = rand() % MAP_SIZE;
    npcs[index].posY = rand() % MAP_SIZE;
    npcs[index].hp = npcs[index].maxHp = 30;
    npcs[index].alive = true;
    
    SC_NpcSpawn spawn{};
    spawn.npcUid = npcs[index].uid;
    spawn.posX = npcs[index].posX;
    spawn.posY = npcs[index].posY;
    spawn.hp = npcs[index].hp;
    spawn.maxHp = npcs[index].maxHp;
    sessionMgr.Broadcast(SC_NPC_SPAWN, &spawn, sizeof(spawn));
}

void SendAllNPCs(std::shared_ptr<Session> session) {
    for (int i = 0; i < MAX_NPC; i++) {
        if (npcs[i].alive) {
            SC_NpcSpawn spawn{};
            spawn.npcUid = npcs[i].uid;
            spawn.posX = npcs[i].posX;
            spawn.posY = npcs[i].posY;
            spawn.hp = npcs[i].hp;
            spawn.maxHp = npcs[i].maxHp;
            session->Send(SC_NPC_SPAWN, &spawn, sizeof(spawn));
        }
    }
}

void SendCharInfo(std::shared_ptr<Session> session) {
    auto& player = session->GetPlayer();
    SC_CharInfo info{};
    info.charUid = player.accountUid;
    info.level = player.level;
    info.exp = player.exp;
    info.posX = player.posX;
    info.posY = player.posY;
    info.hp = player.hp;
    info.maxHp = player.maxHp;
    info.gold = player.gold;
    session->Send(SC_CHAR_INFO, &info, sizeof(info));
}

void SendOtherPlayers(std::shared_ptr<Session> newSession) {
    sessionMgr.ForEach([&](std::shared_ptr<Session> other) {
        if (other->GetId() != newSession->GetId() && other->GetPlayer().loggedIn) {
            auto& otherPlayer = other->GetPlayer();
            SC_CharInfo info{};
            info.charUid = otherPlayer.accountUid;
            info.level = otherPlayer.level;
            info.posX = otherPlayer.posX;
            info.posY = otherPlayer.posY;
            info.hp = otherPlayer.hp;
            info.maxHp = otherPlayer.maxHp;
            newSession->Send(SC_CHAR_INFO, &info, sizeof(info));
        }
    });
    
    auto& newPlayer = newSession->GetPlayer();
    SC_CharInfo info{};
    info.charUid = newPlayer.accountUid;
    info.level = newPlayer.level;
    info.posX = newPlayer.posX;
    info.posY = newPlayer.posY;
    info.hp = newPlayer.hp;
    info.maxHp = newPlayer.maxHp;
    sessionMgr.BroadcastExcept(newSession->GetId(), SC_CHAR_INFO, &info, sizeof(info));
}

void CheckLevelUp(std::shared_ptr<Session> session) {
    auto& player = session->GetPlayer();
    while (player.exp >= player.maxExp) {
        player.exp -= player.maxExp;
        player.level++;
        player.maxHp += 20;
        player.hp = player.maxHp;
        player.atk += 3;
        player.def += 2;
        player.maxExp = player.level * 100;
        
        SC_LevelUp lvup{};
        lvup.level = player.level;
        lvup.hp = player.hp;
        lvup.maxHp = player.maxHp;
        lvup.atk = player.atk;
        lvup.def = player.def;
        session->Send(SC_LEVEL_UP, &lvup, sizeof(lvup));
        std::cout << "[Server] Session " << session->GetId() << " Level up! Lv." << player.level << std::endl;
    }
}

void SendCharList(std::shared_ptr<Session> session) {
    auto& player = session->GetPlayer();
    SC_CharList list{};
    list.count = 0;
    
    std::wstring procCall = L"CALL spCharacterList(" + std::to_wstring(player.accountUid) + L")";
    
    dbConn.ExecuteProc(procCall, nullptr,
        [&](SQLHSTMT hStmt) -> bool {
            SQLBIGINT charUid;
            char charName[21] = {0};
            SQLCHAR charType;
            SQLINTEGER level;
            SQLBIGINT exp;
            SQLLEN ind1, ind2, ind3, ind4, ind5;
            
            SQLBindCol(hStmt, 1, SQL_C_SBIGINT, &charUid, 0, &ind1);
            SQLBindCol(hStmt, 2, SQL_C_CHAR, charName, sizeof(charName), &ind2);
            SQLBindCol(hStmt, 3, SQL_C_UTINYINT, &charType, 0, &ind3);
            SQLBindCol(hStmt, 4, SQL_C_SLONG, &level, 0, &ind4);
            SQLBindCol(hStmt, 5, SQL_C_SBIGINT, &exp, 0, &ind5);
            
            while (SQLFetch(hStmt) == SQL_SUCCESS && list.count < 5) {
                auto& entry = list.chars[list.count];
                entry.charUid = charUid;
                strncpy_s(entry.name, charName, 20);
                entry.charType = charType;
                entry.level = (uint16_t)level;
                list.count++;
            }
            return true;
        });
    
    session->Send(SC_CHAR_LIST, &list, sizeof(list));
    std::cout << "[Server] Sent char list: " << (int)list.count << " characters" << std::endl;
}

void OnLogin(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_Login login;
    memcpy(&login, payload, sizeof(login));
    std::cout << "[Server] Login request: " << login.accountId << " (Session " << session->GetId() << ")" << std::endl;

    auto& player = session->GetPlayer();
    
    uint8_t channelType = 0;
    int64_t channelId = std::atoll(login.accountId);
    if (channelId <= 0) {
        channelId = (std::hash<std::string>{}(login.accountId) % 900000000) + 100000000;
    }
    
    int64_t accountUid = 0;
    int64_t charUid = 0;
    
    std::wstring procCall = L"CALL spAccountLogin(" + std::to_wstring(channelType) + L", " 
                          + std::to_wstring(channelId) + L")";
    
    int dbResult = dbConn.ExecuteProc(procCall, nullptr,
        [&](SQLHSTMT hStmt) -> bool {
            SQLBIGINT uid1 = 0, uid2 = 0;
            SQLLEN ind1, ind2;
            SQLBindCol(hStmt, 1, SQL_C_SBIGINT, &uid1, 0, &ind1);
            SQLBindCol(hStmt, 2, SQL_C_SBIGINT, &uid2, 0, &ind2);
            if (SQLFetch(hStmt) == SQL_SUCCESS) {
                accountUid = uid1;
                charUid = uid2;
            }
            return true;
        });
    
    SC_LoginResult result{};
    
    if (dbResult == 0 && accountUid > 0) {
        std::cout << "[Server] DB Login OK - AccountUid: " << accountUid << std::endl;
        player.accountUid = accountUid;
        result.success = 1;
        strcpy_s(result.message, "Login successful");
    } else {
        std::cout << "[Server] DB Login failed for ID: " << channelId << std::endl;
        player.accountUid = channelId;
        result.success = 1;
        strcpy_s(result.message, "Login (no DB)");
    }

    result.accountUid = player.accountUid;
    session->Send(SC_LOGIN_RESULT, &result, sizeof(result));
    
    // Send character list instead of entering game directly
    SendCharList(session);
}

void OnCharList(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    SendCharList(session);
}

void OnCharCreate(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_CharCreate req;
    memcpy(&req, payload, sizeof(req));
    
    auto& player = session->GetPlayer();
    std::cout << "[Server] Create char: " << req.name << " (Type " << (int)req.charType << ")" << std::endl;
    
    SC_CharCreateResult result{};
    
    std::wstring procCall = L"CALL spCharacterCreate(" 
        + std::to_wstring(player.accountUid) + L", "
        + std::to_wstring(req.charUid) + L", '"
        + std::wstring(req.name, req.name + strlen(req.name)) + L"', "
        + std::to_wstring(req.charType) + L")";
    
    int dbResult = dbConn.ExecuteProc(procCall, nullptr,
        [&](SQLHSTMT hStmt) -> bool {
            SQLBIGINT uid = 0;
            SQLLEN ind;
            SQLBindCol(hStmt, 1, SQL_C_SBIGINT, &uid, 0, &ind);
            if (SQLFetch(hStmt) == SQL_SUCCESS) {
                result.charUid = uid;
            }
            return true;
        });
    
    if (dbResult == 0 && result.charUid > 0) {
        result.success = 1;
        strcpy_s(result.message, "Character created");
        std::cout << "[Server] Character created: " << result.charUid << std::endl;
    } else {
        result.success = 0;
        strcpy_s(result.message, "Create failed");
        std::cout << "[Server] Character create failed" << std::endl;
    }
    
    session->Send(SC_CHAR_CREATE_RESULT, &result, sizeof(result));
    
    if (result.success) {
        SendCharList(session);
    }
}

void OnCharSelect(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_CharSelect req;
    memcpy(&req, payload, sizeof(req));
    
    auto& player = session->GetPlayer();
    std::cout << "[Server] Select char: " << req.charUid << std::endl;
    
    // Call spCharacterLogin
    std::wstring procCall = L"CALL spCharacterLogin(" 
        + std::to_wstring(player.accountUid) + L", "
        + std::to_wstring(req.charUid) + L")";
    
    bool success = false;
    int level = 1;
    int64_t exp = 0;
    int64_t gold = 0;
    
    dbConn.ExecuteProc(procCall, nullptr,
        [&](SQLHSTMT hStmt) -> bool {
            SQLINTEGER lv = 1;
            SQLBIGINT ex = 0, gd = 0;
            SQLLEN ind1, ind2, ind3;
            SQLBindCol(hStmt, 1, SQL_C_SLONG, &lv, 0, &ind1);
            SQLBindCol(hStmt, 2, SQL_C_SBIGINT, &ex, 0, &ind2);
            SQLBindCol(hStmt, 3, SQL_C_SBIGINT, &gd, 0, &ind3);
            if (SQLFetch(hStmt) == SQL_SUCCESS) {
                level = lv;
                exp = ex;
                gold = gd;
                success = true;
            }
            return true;
        });
    
    if (success) {
        player.charUid = req.charUid;
        player.loggedIn = true;
        player.level = level;
        player.exp = exp;
        player.gold = gold;
        player.posX = 5 + (rand() % 10);
        player.posY = 5 + (rand() % 10);
        
        // Fetch character name
        std::wstring nameQuery = L"SELECT CharName FROM `character` WHERE CharUid = " + std::to_wstring(req.charUid);
        dbConn.ExecuteProc(nameQuery, nullptr,
            [&](SQLHSTMT hStmt) -> bool {
                char charName[21] = {0};
                SQLLEN ind;
                SQLBindCol(hStmt, 1, SQL_C_CHAR, charName, sizeof(charName), &ind);
                if (SQLFetch(hStmt) == SQL_SUCCESS) {
                    strncpy_s(player.name, charName, 19);
                }
                return true;
            });
        
        SendCharInfo(session);
        SendAllNPCs(session);
        SendOtherPlayers(session);
        SendAttendanceInfo(session);
        LoadInventory(session);
        
        std::cout << "[Server] Character selected (Lv." << level << " Gold:" << gold << ")" << std::endl;
    } else {
        SC_Error err{};
        err.errorCode = 1;
        strcpy_s(err.message, "Character not found");
        session->Send(SC_ERROR, &err, sizeof(err));
    }
}

void OnHeartbeat(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    session->Send(SC_HEARTBEAT, nullptr, 0);
}

void SendAttendanceInfo(std::shared_ptr<Session> session) {
    auto& player = session->GetPlayer();
    SC_AttendanceInfo info{};
    
    std::wstring procCall = L"CALL spAttendanceInfo(" + std::to_wstring(player.charUid) + L")";
    
    dbConn.ExecuteProc(procCall, nullptr,
        [&](SQLHSTMT hStmt) -> bool {
            SQLCHAR attended = 0;
            SQLINTEGER gold = 0;
            SQLLEN ind1, ind2;
            SQLBindCol(hStmt, 1, SQL_C_UTINYINT, &attended, 0, &ind1);
            SQLBindCol(hStmt, 2, SQL_C_SLONG, &gold, 0, &ind2);
            if (SQLFetch(hStmt) == SQL_SUCCESS) {
                info.todayAttended = attended;
                info.rewardGold = gold;
            }
            return true;
        });
    
    session->Send(SC_ATTENDANCE_INFO, &info, sizeof(info));
    std::cout << "[Server] Attendance info sent: attended=" << (int)info.todayAttended << std::endl;
}

void OnAttendanceCheck(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    auto& player = session->GetPlayer();
    SC_AttendanceResult result{};
    
    std::wstring procCall = L"CALL spAttendanceCheck(" + std::to_wstring(player.charUid) + L")";
    
    dbConn.ExecuteProc(procCall, nullptr,
        [&](SQLHSTMT hStmt) -> bool {
            SQLCHAR success = 0;
            SQLINTEGER gold = 0;
            SQLLEN ind1, ind2;
            SQLBindCol(hStmt, 1, SQL_C_UTINYINT, &success, 0, &ind1);
            SQLBindCol(hStmt, 2, SQL_C_SLONG, &gold, 0, &ind2);
            if (SQLFetch(hStmt) == SQL_SUCCESS) {
                result.success = success;
                result.rewardGold = gold;
            }
            return true;
        });
    
    session->Send(SC_ATTENDANCE_RESULT, &result, sizeof(result));
    std::cout << "[Server] Attendance check: success=" << (int)result.success << " gold=" << result.rewardGold << std::endl;
}

void OnMove(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_Move move;
    memcpy(&move, payload, sizeof(move));
    
    auto& player = session->GetPlayer();
    int16_t newX = player.posX + move.dirX;
    int16_t newY = player.posY + move.dirY;
    
    if (newX >= 0 && newX < MAP_SIZE && newY >= 0 && newY < MAP_SIZE) {
        player.posX = newX;
        player.posY = newY;
    }
    
    SC_MoveResult result{};
    result.success = 1;
    result.posX = player.posX;
    result.posY = player.posY;
    session->Send(SC_MOVE_RESULT, &result, sizeof(result));
    
    SC_CharInfo info{};
    info.charUid = player.accountUid;
    info.posX = player.posX;
    info.posY = player.posY;
    sessionMgr.BroadcastExcept(session->GetId(), SC_CHAR_INFO, &info, sizeof(info));
}

void OnItemUse(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_ItemUse req;
    memcpy(&req, payload, sizeof(req));
    auto& player = session->GetPlayer();
    auto& inv = g_inventories[session->GetId()];

    if (req.slot >= MAX_INVENTORY || inv.slots[req.slot].itemTid == 0) return;

    auto* item = FindItem(inv.slots[req.slot].itemTid);
    if (!item || item->type != "consumable") return;

    // Apply effect
    player.hp = std::min((uint16_t)(player.hp + item->hp), player.maxHp);

    // Remove from inventory
    RemoveItemFromDB(inv.slots[req.slot].itemUid);
    inv.slots[req.slot] = {};

    // Broadcast use result
    SC_ItemUseResult res{};
    res.charUid = player.charUid;
    strncpy_s(res.charName, player.name, 19);
    strncpy_s(res.itemName, item->name.c_str(), 31);
    res.effectType = 0; // hp
    res.effectValue = item->hp;
    sessionMgr.Broadcast(SC_ITEM_USE_RESULT, &res, sizeof(res));

    // Send updated slot
    SC_InventoryUpdate upd{};
    upd.slot = req.slot;
    upd.itemId = 0;
    session->Send(SC_INVENTORY_UPDATE, &upd, sizeof(upd));

    SendCharInfo(session);
    std::cout << "[Server] " << player.name << " used " << item->name << std::endl;
}

void OnItemEquip(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_ItemEquip req;
    memcpy(&req, payload, sizeof(req));
    auto& player = session->GetPlayer();
    auto& inv = g_inventories[session->GetId()];

    if (req.slot >= MAX_INVENTORY || inv.slots[req.slot].itemTid == 0) return;

    auto* item = FindItem(inv.slots[req.slot].itemTid);
    if (!item) return;

    SC_EquipResult res{};
    res.slot = req.slot;

    if (item->type == "consumable") {
        strcpy_s(res.message, "Cannot equip consumable");
        session->Send(SC_EQUIP_RESULT, &res, sizeof(res));
        return;
    }

    int eqSlot = (item->type == "weapon") ? 0 : 1; // DB equipment slot

    if (item->type == "weapon") {
        InvSlot old = { inv.weaponUid, inv.weaponTid };
        inv.weaponUid = inv.slots[req.slot].itemUid;
        inv.weaponTid = inv.slots[req.slot].itemTid;
        inv.slots[req.slot] = old;
        player.atk = 15 + item->atk;
    } else {
        InvSlot old = { inv.armorUid, inv.armorTid };
        inv.armorUid = inv.slots[req.slot].itemUid;
        inv.armorTid = inv.slots[req.slot].itemTid;
        inv.slots[req.slot] = old;
        player.def = 5 + item->def;
    }

    EquipItemDB(player.charUid, eqSlot, (eqSlot == 0) ? inv.weaponUid : inv.armorUid);

    res.success = 1;
    res.atk = player.atk;
    res.def = player.def;
    auto* wep = FindItem(inv.weaponTid);
    auto* arm = FindItem(inv.armorTid);
    if (wep) strncpy_s(res.weaponName, wep->name.c_str(), 31);
    if (arm) strncpy_s(res.armorName, arm->name.c_str(), 31);
    session->Send(SC_EQUIP_RESULT, &res, sizeof(res));

    // Send updated slot (now contains old equipped item or empty)
    SC_InventoryUpdate upd{};
    upd.slot = req.slot;
    upd.itemId = inv.slots[req.slot].itemTid;
    if (upd.itemId) {
        auto* old = FindItem(upd.itemId);
        if (old) strncpy_s(upd.itemName, old->name.c_str(), 31);
    }
    session->Send(SC_INVENTORY_UPDATE, &upd, sizeof(upd));

    SendCharInfo(session);
    std::cout << "[Server] " << player.name << " equipped " << item->name << std::endl;
}

void OnChat(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_Chat chat;
    memcpy(&chat, payload, sizeof(chat));
    auto& player = session->GetPlayer();

    SC_Chat reply{};
    reply.channel = chat.channel;
    strncpy_s(reply.senderName, player.name, 19);
    strncpy_s(reply.message, chat.message, 127);

    if (chat.channel == 0) {
        // World: broadcast to all
        sessionMgr.Broadcast(SC_CHAT, &reply, sizeof(reply));
        std::cout << "[Chat] [World] " << player.name << ": " << chat.message << std::endl;
    } else if (chat.channel == 1) {
        // Whisper: find target by name
        bool found = false;
        sessionMgr.ForEach([&](std::shared_ptr<Session> other) {
            if (strncmp(other->GetPlayer().name, chat.targetName, 20) == 0) {
                other->Send(SC_CHAT, &reply, sizeof(reply));
                session->Send(SC_CHAT, &reply, sizeof(reply));
                found = true;
            }
        });
        if (!found) {
            SC_Error err{};
            err.errorCode = 2;
            strcpy_s(err.message, "Player not found");
            session->Send(SC_ERROR, &err, sizeof(err));
        }
        std::cout << "[Chat] [Whisper] " << player.name << " -> " << chat.targetName << ": " << chat.message << std::endl;
    }
}

void OnShopOpen(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    SC_ShopList list{};
    list.count = (uint8_t)std::min((int)g_items.size(), 10);
    for (int i = 0; i < list.count; i++) {
        list.items[i].itemId = g_items[i].id;
        strncpy_s(list.items[i].itemName, g_items[i].name.c_str(), 31);
        list.items[i].price = g_items[i].price;
    }
    session->Send(SC_SHOP_LIST, &list, sizeof(list));
}

void OnShopBuy(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_ShopBuy req;
    memcpy(&req, payload, sizeof(req));
    auto& player = session->GetPlayer();
    auto* item = FindItem(req.itemId);
    SC_ShopResult result{};

    if (!item) {
        strcpy_s(result.message, "Item not found");
    } else if (player.gold < item->price) {
        strcpy_s(result.message, "Not enough gold");
    } else {
        auto& inv = g_inventories[session->GetId()];
        int slot = FindEmptySlot(inv);
        if (slot < 0) {
            strcpy_s(result.message, "Inventory full");
        } else {
            player.gold -= item->price;
            uint64_t newUid = AddItemToDB(player.charUid, req.itemId);
            inv.slots[slot].itemUid = newUid;
            inv.slots[slot].itemTid = req.itemId;
            result.success = 1;
            result.itemId = req.itemId;
            result.remainGold = player.gold;
            strcpy_s(result.message, item->name.c_str());
            // Send inventory update for the new slot
            SC_InventoryUpdate upd{};
            upd.slot = slot;
            upd.itemId = item->id;
            strncpy_s(upd.itemName, item->name.c_str(), 31);
            session->Send(SC_INVENTORY_UPDATE, &upd, sizeof(upd));
            std::cout << "[Server] Shop buy: " << item->name << " (Session " << session->GetId() << ") Gold:" << player.gold << std::endl;
        }
    }
    session->Send(SC_SHOP_RESULT, &result, sizeof(result));
}

void OnShopSell(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_ShopSell req;
    memcpy(&req, payload, sizeof(req));
    auto& player = session->GetPlayer();
    auto& inv = g_inventories[session->GetId()];
    SC_ShopResult result{};

    if (req.slot >= MAX_INVENTORY || inv.slots[req.slot].itemTid == 0) {
        strcpy_s(result.message, "Invalid slot");
    } else {
        auto* item = FindItem(inv.slots[req.slot].itemTid);
        uint32_t sellPrice = item ? item->price / 5 : 0;  // 20%
        if (inv.slots[req.slot].itemUid) RemoveItemFromDB(inv.slots[req.slot].itemUid);
        player.gold += sellPrice;
        result.success = 2;
        result.itemId = inv.slots[req.slot].itemTid;
        result.remainGold = player.gold;
        strcpy_s(result.message, item ? item->name.c_str() : "Unknown");
        inv.slots[req.slot] = {};
        // Send inventory update to clear the slot
        SC_InventoryUpdate upd{};
        upd.slot = req.slot;
        upd.itemId = 0;
        session->Send(SC_INVENTORY_UPDATE, &upd, sizeof(upd));
        std::cout << "[Server] Shop sell: slot " << (int)req.slot << " +" << sellPrice << "G (Session " << session->GetId() << ")" << std::endl;
    }
    session->Send(SC_SHOP_RESULT, &result, sizeof(result));
}

void OnAttack(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    CS_Attack atk;
    memcpy(&atk, payload, sizeof(atk));
    
    auto& player = session->GetPlayer();
    
    // Find target NPC by uid
    NPC* target = nullptr;
    for (int i = 0; i < MAX_NPC; i++) {
        if (npcs[i].alive && npcs[i].uid == atk.targetUid) {
            int dx = abs(npcs[i].posX - player.posX);
            int dy = abs(npcs[i].posY - player.posY);
            if (dx <= 1 && dy <= 1) {
                target = &npcs[i];
            }
            break;
        }
    }
    
    SC_AttackResult result{};
    if (target) {
        uint16_t damage = (player.atk > target->def) ? (player.atk - target->def) : 1;
        target->hp = (target->hp > damage) ? (target->hp - damage) : 0;
        
        result.success = 1;
        result.targetUid = target->uid;
        result.damage = damage;
        result.targetHp = target->hp;
        session->Send(SC_ATTACK_RESULT, &result, sizeof(result));
        
        std::cout << "[Server] Session " << session->GetId() << " Attack! Damage: " << damage << std::endl;
        
        if (target->hp == 0) {
            SC_NpcDeath death{};
            death.npcUid = target->uid;
            death.expReward = target->expReward;
            death.goldReward = target->goldReward;
            sessionMgr.Broadcast(SC_NPC_DEATH, &death, sizeof(death));
            target->alive = false;
            
            player.exp += target->expReward;
            player.gold += target->goldReward;
            SC_ExpUpdate expUp{};
            expUp.exp = player.exp;
            expUp.maxExp = player.maxExp;
            session->Send(SC_EXP_UPDATE, &expUp, sizeof(expUp));
            
            CheckLevelUp(session);

            // Item drop
            uint16_t dropId = RollDrop();
            if (dropId) {
                auto& inv = g_inventories[session->GetId()];
                int slot = FindEmptySlot(inv);
                if (slot >= 0) {
                    auto* item = FindItem(dropId);
                    if (item) {
                        uint64_t newUid = AddItemToDB(player.charUid, dropId);
                        inv.slots[slot].itemUid = newUid;
                        inv.slots[slot].itemTid = dropId;
                        SC_ItemDrop drop{};
                        drop.slot = slot;
                        drop.itemId = item->id;
                        strncpy_s(drop.itemName, item->name.c_str(), 31);
                        session->Send(SC_ITEM_DROP, &drop, sizeof(drop));
                        std::cout << "[Server] Item drop: " << item->name << " (id:" << dropId << ") -> slot " << slot << " (Session " << session->GetId() << ")" << std::endl;
                    }
                }
            }
            
            for (int i = 0; i < MAX_NPC; i++) {
                if (!npcs[i].alive) { SpawnNPC(i); break; }
            }
        }
    } else {
        result.success = 0;
        session->Send(SC_ATTACK_RESULT, &result, sizeof(result));
    }
}

int main() {
    SetConsoleOutputCP(65001);
    setlocale(LC_ALL, ".UTF-8");
    srand((unsigned)time(nullptr));
    
    // DB connection test
    std::cout << "Connecting to MySQL..." << std::endl;
    if (!dbConn.Connect("127.0.0.1", "mockdb", "admin", "flfhelem1!")) {
        std::cerr << "Failed to connect to database!" << std::endl;
        return 1;
    }
    
    // Simple query test
    std::cout << "Testing DB query..." << std::endl;
    if (dbConn.ExecuteQuery(L"SELECT 1")) {
        std::cout << "DB connection test successful!" << std::endl;
    } else {
        std::cerr << "DB query test failed!" << std::endl;
    }

    // Load item data
    if (!LoadItems("data/items.json")) {
        std::cerr << "Failed to load items.json!" << std::endl;
        return 1;
    }
    
    for (int i = 0; i < MAX_NPC; i++) {
        npcs[i].uid = nextNpcUid++;
        npcs[i].posX = rand() % MAP_SIZE;
        npcs[i].posY = rand() % MAP_SIZE;
        npcs[i].hp = npcs[i].maxHp = 30;
        npcs[i].alive = true;
    }

    try {
        boost::asio::io_context io_context;
        
        PacketHandler packetHandler;
        packetHandler.AddHandler(CS_LOGIN, OnLogin);
        packetHandler.AddHandler(CS_CHAR_LIST, OnCharList);
        packetHandler.AddHandler(CS_CHAR_CREATE, OnCharCreate);
        packetHandler.AddHandler(CS_CHAR_SELECT, OnCharSelect);
        packetHandler.AddHandler(CS_ATTENDANCE_CHECK, OnAttendanceCheck);
        packetHandler.AddHandler(CS_HEARTBEAT, OnHeartbeat);
        packetHandler.AddHandler(CS_MOVE, OnMove);
        packetHandler.AddHandler(CS_ATTACK, OnAttack);
        packetHandler.AddHandler(CS_CHAT, OnChat);
        packetHandler.AddHandler(CS_ITEM_USE, OnItemUse);
        packetHandler.AddHandler(CS_ITEM_EQUIP, OnItemEquip);
        packetHandler.AddHandler(CS_SHOP_OPEN, OnShopOpen);
        packetHandler.AddHandler(CS_SHOP_BUY, OnShopBuy);
        packetHandler.AddHandler(CS_SHOP_SELL, OnShopSell);

        TcpServer server(io_context, 9000, sessionMgr, packetHandler);

        std::cout << "NPCs spawned: " << MAX_NPC << std::endl;
        std::cout << "Server running... Press Ctrl+C to stop." << std::endl;
        
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
