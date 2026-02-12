#include "TcpClient.h"
#include <iostream>
#include <cstring>
#include <conio.h>
#include <map>
#include <vector>

TcpClient client;
constexpr int MAP_SIZE = 20;

// Game state
enum class GameState { Login, CharSelect, InGame };
GameState gameState = GameState::Login;

uint64_t accountUid = 0;

struct CharEntry {
    uint64_t charUid;
    char name[20];
    uint8_t charType;
    uint16_t level;
};
std::vector<CharEntry> charList;

struct Player {
    uint64_t uid = 0;
    int16_t posX = 10, posY = 10;
    uint16_t level = 1;
    uint16_t hp = 100, maxHp = 100;
    uint16_t atk = 15, def = 5;
    uint32_t exp = 0, maxExp = 100;
    uint64_t gold = 0;
} myPlayer;

// Attendance state
struct AttendanceState {
    bool todayAttended = false;
    uint32_t rewardGold = 0;
} attendance;

struct OtherPlayer {
    uint64_t uid;
    int16_t posX, posY;
};
std::map<uint64_t, OtherPlayer> otherPlayers;

struct NPC {
    uint64_t uid;
    int16_t posX, posY;
    uint16_t hp, maxHp;
};
std::map<uint64_t, NPC> npcs;

std::string lastMsg;

void DrawCharSelect() {
    system("cls");
    std::cout << "=== Character Select ===" << std::endl;
    std::cout << "Account: " << accountUid << std::endl << std::endl;
    
    if (charList.empty()) {
        std::cout << "No characters found." << std::endl;
    } else {
        for (size_t i = 0; i < charList.size(); i++) {
            std::cout << (i + 1) << ". " << charList[i].name 
                      << " (Lv." << charList[i].level 
                      << ", Type " << (int)charList[i].charType << ")" << std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << "C = Create New Character" << std::endl;
    std::cout << "1-5 = Select Character" << std::endl;
    std::cout << "Q = Quit" << std::endl;
    if (!lastMsg.empty()) std::cout << std::endl << lastMsg << std::endl;
}

void DrawMap() {
    system("cls");
    std::cout << "+" << std::string(MAP_SIZE, '-') << "+\n";
    for (int y = 0; y < MAP_SIZE; y++) {
        std::cout << "|";
        for (int x = 0; x < MAP_SIZE; x++) {
            char c = '.';
            if (x == myPlayer.posX && y == myPlayer.posY) {
                c = '@';
            } else {
                for (auto& [uid, p] : otherPlayers) {
                    if (p.posX == x && p.posY == y) { c = 'P'; break; }
                }
                if (c == '.') {
                    for (auto& [uid, npc] : npcs) {
                        if (npc.posX == x && npc.posY == y) { c = 'N'; break; }
                    }
                }
            }
            std::cout << c;
        }
        std::cout << "|\n";
    }
    std::cout << "+" << std::string(MAP_SIZE, '-') << "+\n";
    std::cout << "Lv." << myPlayer.level << " | HP:" << myPlayer.hp << "/" << myPlayer.maxHp;
    std::cout << " | ATK:" << myPlayer.atk << " DEF:" << myPlayer.def << "\n";
    std::cout << "EXP: " << myPlayer.exp << "/" << myPlayer.maxExp;
    std::cout << " | Gold: " << myPlayer.gold;
    std::cout << " | NPCs:" << npcs.size() << " Players:" << (otherPlayers.size() + 1) << "\n";
    if (!attendance.todayAttended) {
        std::cout << "[T] Attendance Check (+" << attendance.rewardGold << " Gold)\n";
    }
    if (!lastMsg.empty()) std::cout << lastMsg << "\n";
    std::cout << "@ = You, P = Other Player, N = NPC\n";
    std::cout << "WASD=Move SPACE=Attack T=Attendance Q=Quit\n";
}

void OnLoginResult(const PacketHeader& header, const char* payload) {
    SC_LoginResult result;
    memcpy(&result, payload, sizeof(result));
    accountUid = result.accountUid;
    std::cout << "Login: " << result.message << std::endl;
}

void OnCharList(const PacketHeader& header, const char* payload) {
    SC_CharList list;
    memcpy(&list, payload, sizeof(list));
    
    charList.clear();
    for (int i = 0; i < list.count && i < 5; i++) {
        CharEntry entry;
        entry.charUid = list.chars[i].charUid;
        strncpy_s(entry.name, list.chars[i].name, 20);
        entry.charType = list.chars[i].charType;
        entry.level = list.chars[i].level;
        charList.push_back(entry);
    }
    
    gameState = GameState::CharSelect;
    DrawCharSelect();
}

void OnCharCreateResult(const PacketHeader& header, const char* payload) {
    SC_CharCreateResult result;
    memcpy(&result, payload, sizeof(result));
    
    if (result.success) {
        lastMsg = "Character created!";
    } else {
        lastMsg = std::string("Create failed: ") + result.message;
    }
    if (gameState == GameState::CharSelect) DrawCharSelect();
}

void OnCharInfo(const PacketHeader& header, const char* payload) {
    SC_CharInfo info;
    memcpy(&info, payload, sizeof(info));
    
    if (gameState == GameState::CharSelect) {
        // Entering game
        gameState = GameState::InGame;
        myPlayer.uid = info.charUid;
        myPlayer.level = info.level;
        myPlayer.exp = info.exp;
        myPlayer.posX = info.posX;
        myPlayer.posY = info.posY;
        myPlayer.hp = info.hp;
        myPlayer.maxHp = info.maxHp;
        myPlayer.gold = info.gold;
        DrawMap();
    } else if (info.charUid == myPlayer.uid) {
        myPlayer.level = info.level;
        myPlayer.exp = info.exp;
        myPlayer.posX = info.posX;
        myPlayer.posY = info.posY;
        myPlayer.hp = info.hp;
        myPlayer.maxHp = info.maxHp;
        myPlayer.gold = info.gold;
        DrawMap();
    } else {
        otherPlayers[info.charUid] = {info.charUid, info.posX, info.posY};
        if (gameState == GameState::InGame) DrawMap();
    }
}

void OnMoveResult(const PacketHeader& header, const char* payload) {
    SC_MoveResult result;
    memcpy(&result, payload, sizeof(result));
    if (result.success) {
        myPlayer.posX = result.posX;
        myPlayer.posY = result.posY;
        lastMsg = "";
        DrawMap();
    }
}

void OnNpcSpawn(const PacketHeader& header, const char* payload) {
    SC_NpcSpawn spawn;
    memcpy(&spawn, payload, sizeof(spawn));
    npcs[spawn.npcUid] = {spawn.npcUid, spawn.posX, spawn.posY, spawn.hp, spawn.maxHp};
    if (gameState == GameState::InGame) DrawMap();
}

void OnCharLeave(const PacketHeader& header, const char* payload) {
    SC_CharLeave leave;
    memcpy(&leave, payload, sizeof(leave));
    otherPlayers.erase(leave.charUid);
    lastMsg = "A player left the game";
    if (gameState == GameState::InGame) DrawMap();
}

void OnNpcDeath(const PacketHeader& header, const char* payload) {
    SC_NpcDeath death;
    memcpy(&death, payload, sizeof(death));
    npcs.erase(death.npcUid);
    lastMsg = "NPC defeated! +" + std::to_string(death.expReward) + " EXP";
    if (gameState == GameState::InGame) DrawMap();
}

void OnAttackResult(const PacketHeader& header, const char* payload) {
    SC_AttackResult result;
    memcpy(&result, payload, sizeof(result));
    if (result.success) {
        lastMsg = "Hit! Damage:" + std::to_string(result.damage) + " NPC HP:" + std::to_string(result.targetHp);
        if (npcs.count(result.targetUid)) {
            npcs[result.targetUid].hp = result.targetHp;
        }
    } else {
        lastMsg = "No target nearby!";
    }
    DrawMap();
}

void OnExpUpdate(const PacketHeader& header, const char* payload) {
    SC_ExpUpdate exp;
    memcpy(&exp, payload, sizeof(exp));
    myPlayer.exp = exp.exp;
    myPlayer.maxExp = exp.maxExp;
}

void OnLevelUp(const PacketHeader& header, const char* payload) {
    SC_LevelUp lvup;
    memcpy(&lvup, payload, sizeof(lvup));
    myPlayer.level = lvup.level;
    myPlayer.hp = lvup.hp;
    myPlayer.maxHp = lvup.maxHp;
    myPlayer.atk = lvup.atk;
    myPlayer.def = lvup.def;
    lastMsg = "*** LEVEL UP! Lv." + std::to_string(myPlayer.level) + " ***";
    DrawMap();
}

void OnHeartbeat(const PacketHeader& header, const char* payload) {}

void OnAttendanceInfo(const PacketHeader& header, const char* payload) {
    SC_AttendanceInfo info;
    memcpy(&info, payload, sizeof(info));
    attendance.todayAttended = (info.todayAttended != 0);
    attendance.rewardGold = info.rewardGold;
    if (gameState == GameState::InGame) DrawMap();
}

void OnAttendanceResult(const PacketHeader& header, const char* payload) {
    SC_AttendanceResult result;
    memcpy(&result, payload, sizeof(result));
    if (result.success) {
        attendance.todayAttended = true;
        myPlayer.gold += result.rewardGold;
        lastMsg = "Attendance checked! +" + std::to_string(result.rewardGold) + " Gold";
    } else {
        lastMsg = "Already attended today!";
    }
    if (gameState == GameState::InGame) DrawMap();
}

void OnError(const PacketHeader& header, const char* payload) {
    SC_Error err;
    memcpy(&err, payload, sizeof(err));
    lastMsg = std::string("Error: ") + err.message;
    if (gameState == GameState::CharSelect) DrawCharSelect();
    else if (gameState == GameState::InGame) DrawMap();
}

uint64_t FindNearestNpc() {
    uint64_t nearest = 0;
    int minDist = INT_MAX;
    for (auto& [uid, npc] : npcs) {
        int dist = abs(npc.posX - myPlayer.posX) + abs(npc.posY - myPlayer.posY);
        if (dist < minDist) {
            minDist = dist;
            nearest = uid;
        }
    }
    return nearest;
}

void CreateCharacter() {
    std::cout << std::endl << "Enter character name: ";
    std::string name;
    std::cin >> name;
    std::cin.ignore();
    
    std::cout << "Enter character type (0-3): ";
    int type = 0;
    std::cin >> type;
    std::cin.ignore();
    
    CS_CharCreate req{};
    req.charUid = accountUid * 1000 + charList.size();
    strncpy_s(req.name, name.c_str(), 19);
    req.charType = (uint8_t)type;
    
    client.Send(CS_CHAR_CREATE, &req, sizeof(req));
}

int main() {
    client.SetHandler(SC_LOGIN_RESULT, OnLoginResult);
    client.SetHandler(SC_CHAR_LIST, OnCharList);
    client.SetHandler(SC_CHAR_CREATE_RESULT, OnCharCreateResult);
    client.SetHandler(SC_CHAR_INFO, OnCharInfo);
    client.SetHandler(SC_CHAR_LEAVE, OnCharLeave);
    client.SetHandler(SC_MOVE_RESULT, OnMoveResult);
    client.SetHandler(SC_HEARTBEAT, OnHeartbeat);
    client.SetHandler(SC_NPC_SPAWN, OnNpcSpawn);
    client.SetHandler(SC_NPC_DEATH, OnNpcDeath);
    client.SetHandler(SC_ATTACK_RESULT, OnAttackResult);
    client.SetHandler(SC_EXP_UPDATE, OnExpUpdate);
    client.SetHandler(SC_LEVEL_UP, OnLevelUp);
    client.SetHandler(SC_ATTENDANCE_INFO, OnAttendanceInfo);
    client.SetHandler(SC_ATTENDANCE_RESULT, OnAttendanceResult);
    client.SetHandler(SC_ERROR, OnError);

    std::cout << "=== MMORPG Simulator ===" << std::endl;
    std::cout << "Enter Account ID: ";
    std::string inputId;
    std::getline(std::cin, inputId);
    if (inputId.empty()) inputId = "1";

    std::cout << "Enter Server IP (default: 127.0.0.1): ";
    std::string serverIp;
    std::getline(std::cin, serverIp);
    if (serverIp.empty()) serverIp = "127.0.0.1";

    if (!client.Connect(serverIp.c_str(), 9000)) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }

    CS_Login login{};
    strcpy_s(login.accountId, inputId.c_str());
    strcpy_s(login.password, "");
    client.Send(CS_LOGIN, &login, sizeof(login));
    std::cout << "Logging in..." << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (client.IsConnected()) {
        if (_kbhit()) {
            char key = _getch();
            
            if (gameState == GameState::CharSelect) {
                if (key == 'c' || key == 'C') {
                    CreateCharacter();
                } else if (key >= '1' && key <= '5') {
                    int idx = key - '1';
                    if (idx < (int)charList.size()) {
                        CS_CharSelect sel{};
                        sel.charUid = charList[idx].charUid;
                        client.Send(CS_CHAR_SELECT, &sel, sizeof(sel));
                    }
                } else if (key == 'q' || key == 'Q') {
                    break;
                }
            } else if (gameState == GameState::InGame) {
                CS_Move move{};
                switch (tolower(key)) {
                    case 'w': move.dirY = -1; client.Send(CS_MOVE, &move, sizeof(move)); break;
                    case 's': move.dirY = 1; client.Send(CS_MOVE, &move, sizeof(move)); break;
                    case 'a': move.dirX = -1; client.Send(CS_MOVE, &move, sizeof(move)); break;
                    case 'd': move.dirX = 1; client.Send(CS_MOVE, &move, sizeof(move)); break;
                    case ' ': {
                        CS_Attack atk{};
                        atk.targetUid = FindNearestNpc();
                        client.Send(CS_ATTACK, &atk, sizeof(atk));
                        break;
                    }
                    case 't': {
                        client.Send(CS_ATTENDANCE_CHECK, nullptr, 0);
                        break;
                    }
                    case 'q': goto exit;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

exit:
    client.Disconnect();
    return 0;
}
