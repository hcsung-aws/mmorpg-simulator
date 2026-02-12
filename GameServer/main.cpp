#include <boost/asio.hpp>
#include "TcpServer.h"
#include "SessionManager.h"
#include "PacketHandler.h"
#include "DBConnection.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <functional>

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
    bool alive;
};

SessionManager sessionMgr;
DBConnection dbConn;
NPC npcs[MAX_NPC];
uint64_t nextNpcUid = 1;

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
        
        SendCharInfo(session);
        SendAllNPCs(session);
        SendOtherPlayers(session);
        SendAttendanceInfo(session);
        
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
            sessionMgr.Broadcast(SC_NPC_DEATH, &death, sizeof(death));
            target->alive = false;
            
            player.exp += target->expReward;
            SC_ExpUpdate expUp{};
            expUp.exp = player.exp;
            expUp.maxExp = player.maxExp;
            session->Send(SC_EXP_UPDATE, &expUp, sizeof(expUp));
            
            CheckLevelUp(session);
            
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

        TcpServer server(io_context, 9000, sessionMgr, packetHandler);

        std::cout << "NPCs spawned: " << MAX_NPC << std::endl;
        std::cout << "Server running... Press Ctrl+C to stop." << std::endl;
        
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
