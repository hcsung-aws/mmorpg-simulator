#include "TcpClient.h"
#include <iostream>
#include <string>
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
std::vector<std::string> chatLog;
constexpr int MAX_CHAT_LOG = 5;
bool chatMode = false;

// Inventory
struct InvSlot { uint16_t itemId = 0; char name[32] = {}; };
InvSlot inventory[MAX_INVENTORY];
bool showInventory = false;
char equippedWeapon[32] = {};
char equippedArmor[32] = {};

int ReadSlot(const char* prompt) {
    std::cout << prompt;
    std::string input;
    while (true) {
        char c = _getch();
        if (c == '\r') break;
        if (c == 27) { input.clear(); break; }
        if (c >= '0' && c <= '9') { input += c; std::cout << c; }
    }
    std::cout << std::endl;
    if (input.empty()) return -1;
    return std::stoi(input);
}

// Shop
struct ShopItem { uint16_t itemId; char name[32]; uint32_t price; };
ShopItem shopItems[10];
int shopCount = 0;
bool showShop = false;

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
    for (auto& line : chatLog) std::cout << line << "\n";
    if (showInventory) {
        std::cout << "--- Inventory (U=Use E=Equip X=Sell ESC=Close) ---\n";
        std::cout << " [Weapon] " << (equippedWeapon[0] ? equippedWeapon : "None") << "\n";
        std::cout << " [Armor]  " << (equippedArmor[0] ? equippedArmor : "None") << "\n";
        for (int i = 0; i < MAX_INVENTORY; i++) {
            if (inventory[i].itemId)
                std::cout << " " << i << ": " << inventory[i].name << "\n";
        }
        std::cout << "-------------------------------------------\n";
    }
    if (showShop) {
        std::cout << "--- Shop (1-9=Buy S+slot=Sell ESC=Close) ---\n";
        for (int i = 0; i < shopCount; i++)
            std::cout << " " << (i+1) << ": " << shopItems[i].name << " (" << shopItems[i].price << "G)\n";
        std::cout << "-------------------------------------------\n";
    }
    std::cout << "@ = You, P = Other Player, N = NPC\n";
    std::cout << "WASD=Move SPACE=Attack I=Inventory B=Shop T=Attendance ENTER=Chat Q=Quit\n";
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
    lastMsg = "NPC defeated! +" + std::to_string(death.expReward) + " EXP +" + std::to_string(death.goldReward) + " Gold";
    myPlayer.gold += death.goldReward;
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

void OnItemDrop(const PacketHeader& header, const char* payload) {
    SC_ItemDrop drop;
    memcpy(&drop, payload, sizeof(drop));
    if (drop.slot < MAX_INVENTORY) {
        inventory[drop.slot].itemId = drop.itemId;
        strncpy_s(inventory[drop.slot].name, drop.itemName, 31);
    }
    lastMsg = "Dropped: " + std::string(drop.itemName) + " [slot " + std::to_string(drop.slot) + "]";
    if (gameState == GameState::InGame) DrawMap();
}

void OnItemUseResult(const PacketHeader& header, const char* payload) {
    SC_ItemUseResult res;
    memcpy(&res, payload, sizeof(res));
    lastMsg = std::string(res.charName) + " used " + res.itemName + " (+" + std::to_string(res.effectValue) + " HP)";
    if (gameState == GameState::InGame) DrawMap();
}

void OnEquipResult(const PacketHeader& header, const char* payload) {
    SC_EquipResult res;
    memcpy(&res, payload, sizeof(res));
    if (res.success) {
        myPlayer.atk = res.atk;
        myPlayer.def = res.def;
        strncpy_s(equippedWeapon, res.weaponName, 31);
        strncpy_s(equippedArmor, res.armorName, 31);
        lastMsg = "Equipped! ATK:" + std::to_string(res.atk) + " DEF:" + std::to_string(res.def);
    } else {
        lastMsg = std::string(res.message);
    }
    if (gameState == GameState::InGame) DrawMap();
}

void OnInventoryUpdate(const PacketHeader& header, const char* payload) {
    SC_InventoryUpdate upd;
    memcpy(&upd, payload, sizeof(upd));
    if (upd.slot < MAX_INVENTORY) {
        inventory[upd.slot].itemId = upd.itemId;
        if (upd.itemId) strncpy_s(inventory[upd.slot].name, upd.itemName, 31);
        else memset(inventory[upd.slot].name, 0, 32);
    }
    if (gameState == GameState::InGame) DrawMap();
}

void OnInventoryList(const PacketHeader& header, const char* payload) {
    SC_InventoryList list;
    memcpy(&list, payload, sizeof(list));
    for (int i = 0; i < list.count && i < MAX_INVENTORY; i++) {
        auto& item = list.items[i];
        if (item.slot < MAX_INVENTORY) {
            inventory[item.slot].itemId = item.itemId;
            if (item.itemId) strncpy_s(inventory[item.slot].name, item.itemName, 31);
            else memset(inventory[item.slot].name, 0, 32);
        }
    }
    if (gameState == GameState::InGame) DrawMap();
}

void OnShopList(const PacketHeader& header, const char* payload) {
    SC_ShopList list;
    memcpy(&list, payload, sizeof(list));
    shopCount = list.count;
    for (int i = 0; i < shopCount; i++) {
        shopItems[i].itemId = list.items[i].itemId;
        strncpy_s(shopItems[i].name, list.items[i].itemName, 31);
        shopItems[i].price = list.items[i].price;
    }
    showShop = true;
    if (gameState == GameState::InGame) DrawMap();
}

void OnShopResult(const PacketHeader& header, const char* payload) {
    SC_ShopResult res;
    memcpy(&res, payload, sizeof(res));
    myPlayer.gold = res.remainGold;
    if (res.success == 1)
        lastMsg = "Bought: " + std::string(res.message) + " (Gold:" + std::to_string(res.remainGold) + ")";
    else if (res.success == 2)
        lastMsg = "Sold: " + std::string(res.message) + " (Gold:" + std::to_string(res.remainGold) + ")";
    else
        lastMsg = std::string("Shop: ") + res.message;
    if (gameState == GameState::InGame) DrawMap();
}

void OnChatMsg(const PacketHeader& header, const char* payload) {
    SC_Chat chat;
    memcpy(&chat, payload, sizeof(chat));
    std::string prefix = (chat.channel == 0) ? "[World]" : "[Whisper]";
    std::string line = prefix + " " + chat.senderName + ": " + chat.message;
    chatLog.push_back(line);
    if (chatLog.size() > MAX_CHAT_LOG) chatLog.erase(chatLog.begin());
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
    client.SetHandler(SC_CHAT, OnChatMsg);
    client.SetHandler(SC_ITEM_DROP, OnItemDrop);
    client.SetHandler(SC_ITEM_USE_RESULT, OnItemUseResult);
    client.SetHandler(SC_EQUIP_RESULT, OnEquipResult);
    client.SetHandler(SC_INVENTORY_UPDATE, OnInventoryUpdate);
    client.SetHandler(SC_INVENTORY_LIST, OnInventoryList);
    client.SetHandler(SC_SHOP_LIST, OnShopList);
    client.SetHandler(SC_SHOP_RESULT, OnShopResult);

    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);

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

    // Flush leftover characters from std::getline to prevent _getch() picking them up
    FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));

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
                    case 'i': {
                        showInventory = !showInventory;
                        showShop = false;
                        DrawMap();
                        break;
                    }
                    case 'b': {
                        showShop = !showShop;
                        showInventory = false;
                        if (showShop) client.Send(CS_SHOP_OPEN, nullptr, 0);
                        else DrawMap();
                        break;
                    }
                    case '1': case '2': case '3': case '4': case '5':
                    case '6': case '7': case '8': case '9': {
                        if (showShop) {
                            int idx = key - '1';
                            if (idx < shopCount) {
                                CS_ShopBuy req{};
                                req.itemId = shopItems[idx].itemId;
                                client.Send(CS_SHOP_BUY, &req, sizeof(req));
                            }
                        }
                        break;
                    }
                    case 'u': {
                        if (showInventory) {
                            int slot = ReadSlot("Use slot: ");
                            if (slot >= 0 && slot < MAX_INVENTORY) {
                                CS_ItemUse req{};
                                req.slot = slot;
                                client.Send(CS_ITEM_USE, &req, sizeof(req));
                            }
                        }
                        break;
                    }
                    case 'e': {
                        if (showInventory) {
                            int slot = ReadSlot("Equip slot: ");
                            if (slot >= 0 && slot < MAX_INVENTORY) {
                                CS_ItemEquip req{};
                                req.slot = slot;
                                client.Send(CS_ITEM_EQUIP, &req, sizeof(req));
                            }
                        }
                        break;
                    }
                    case 'x': {
                        if (showInventory) {
                            int slot = ReadSlot("Sell slot: ");
                            if (slot >= 0 && slot < MAX_INVENTORY) {
                                CS_ShopSell req{};
                                req.slot = slot;
                                client.Send(CS_SHOP_SELL, &req, sizeof(req));
                            }
                        }
                        break;
                    }
                    case 27: { // ESC
                        if (showInventory) { showInventory = false; DrawMap(); }
                        if (showShop) { showShop = false; DrawMap(); }
                        break;
                    }
                    case '\r': {
                        // Chat mode - use _getwch() for UTF-8 support
                        std::cout << "Chat (/w name msg for whisper): ";
                        std::string input;
                        while (true) {
                            wchar_t wc = _getwch();
                            if (wc == L'\r' || wc == L'\n') break;
                            if (wc == L'\b' || wc == 127) {
                                if (!input.empty()) {
                                    // Find start of last UTF-8 char
                                    size_t i = input.size() - 1;
                                    while (i > 0 && (input[i] & 0xC0) == 0x80) i--;
                                    int width = (input.size() - i >= 3) ? 2 : 1;
                                    input.erase(i);
                                    for (int j = 0; j < width; j++) std::cout << "\b \b";
                                }
                            } else {
                                char buf[4] = {};
                                int len = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, buf, sizeof(buf), nullptr, nullptr);
                                input.append(buf, len);
                                std::cout.write(buf, len);
                            }
                        }
                        std::cout << std::endl;
                        if (!input.empty()) {
                            CS_Chat chat{};
                            if (input.rfind("/w ", 0) == 0) {
                                chat.channel = 1;
                                size_t sp = input.find(' ', 3);
                                if (sp != std::string::npos) {
                                    strncpy_s(chat.targetName, input.substr(3, sp - 3).c_str(), 19);
                                    strncpy_s(chat.message, input.substr(sp + 1).c_str(), 127);
                                    client.Send(CS_CHAT, &chat, sizeof(chat));
                                }
                            } else {
                                chat.channel = 0;
                                strncpy_s(chat.message, input.c_str(), 127);
                                client.Send(CS_CHAT, &chat, sizeof(chat));
                            }
                        }
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
