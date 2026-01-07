#include "TcpClient.h"
#include <iostream>
#include <cstring>
#include <conio.h>
#include <map>

TcpClient client;
constexpr int MAP_SIZE = 20;

struct Player {
    uint64_t uid = 0;
    int16_t posX = 10, posY = 10;
    uint16_t level = 1;
    uint16_t hp = 100, maxHp = 100;
    uint16_t atk = 15, def = 5;
    uint32_t exp = 0, maxExp = 100;
} myPlayer;

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
    std::cout << " | NPCs:" << npcs.size() << " Players:" << (otherPlayers.size() + 1) << "\n";
    if (!lastMsg.empty()) std::cout << lastMsg << "\n";
    std::cout << "@ = You, P = Other Player, N = NPC\n";
    std::cout << "WASD=Move SPACE=Attack Q=Quit\n";
}

void OnLoginResult(const PacketHeader& header, const char* payload) {
    SC_LoginResult result;
    memcpy(&result, payload, sizeof(result));
    myPlayer.uid = result.accountUid;
}

void OnCharInfo(const PacketHeader& header, const char* payload) {
    SC_CharInfo info;
    memcpy(&info, payload, sizeof(info));
    
    if (info.charUid == myPlayer.uid) {
        myPlayer.level = info.level;
        myPlayer.exp = info.exp;
        myPlayer.posX = info.posX;
        myPlayer.posY = info.posY;
        myPlayer.hp = info.hp;
        myPlayer.maxHp = info.maxHp;
    } else {
        otherPlayers[info.charUid] = {info.charUid, info.posX, info.posY};
    }
    DrawMap();
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
    DrawMap();
}

void OnCharLeave(const PacketHeader& header, const char* payload) {
    SC_CharLeave leave;
    memcpy(&leave, payload, sizeof(leave));
    otherPlayers.erase(leave.charUid);
    lastMsg = "A player left the game";
    DrawMap();
}

void OnNpcDeath(const PacketHeader& header, const char* payload) {
    SC_NpcDeath death;
    memcpy(&death, payload, sizeof(death));
    npcs.erase(death.npcUid);
    lastMsg = "NPC defeated! +" + std::to_string(death.expReward) + " EXP";
    DrawMap();
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

int main() {
    client.SetHandler(SC_LOGIN_RESULT, OnLoginResult);
    client.SetHandler(SC_CHAR_INFO, OnCharInfo);
    client.SetHandler(SC_CHAR_LEAVE, OnCharLeave);
    client.SetHandler(SC_MOVE_RESULT, OnMoveResult);
    client.SetHandler(SC_HEARTBEAT, OnHeartbeat);
    client.SetHandler(SC_NPC_SPAWN, OnNpcSpawn);
    client.SetHandler(SC_NPC_DEATH, OnNpcDeath);
    client.SetHandler(SC_ATTACK_RESULT, OnAttackResult);
    client.SetHandler(SC_EXP_UPDATE, OnExpUpdate);
    client.SetHandler(SC_LEVEL_UP, OnLevelUp);

    // 로그인 ID 입력
    std::cout << "=== MMORPG Simulator ===" << std::endl;
    std::cout << "Enter Account ID (number): ";
    std::string inputId;
    std::getline(std::cin, inputId);
    if (inputId.empty()) inputId = "1";

    // Server IP input
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
    std::cout << "Logging in as ID: " << inputId << "..." << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    DrawMap();

    while (client.IsConnected()) {
        if (_kbhit()) {
            char key = _getch();
            CS_Move move{};
            switch (tolower(key)) {
                case 'w': move.dirY = -1; client.Send(CS_MOVE, &move, sizeof(move)); break;
                case 's': move.dirY = 1; client.Send(CS_MOVE, &move, sizeof(move)); break;
                case 'a': move.dirX = -1; client.Send(CS_MOVE, &move, sizeof(move)); break;
                case 'd': move.dirX = 1; client.Send(CS_MOVE, &move, sizeof(move)); break;
                case ' ': client.Send(CS_ATTACK, nullptr, 0); break;
                case 'q': goto exit;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

exit:
    client.Disconnect();
    return 0;
}
