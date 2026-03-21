# MMORPG Simulator

패킷 캡처 및 재현 테스트를 위한 간단한 MMORPG 시뮬레이터입니다.

**관련 프로젝트:** [Packet Capture Log Agent](https://github.com/hcsung-aws/packet-capture-log-agent) - 이 시뮬레이터의 패킷을 캡처/재현하는 도구

[English](#english)

## 개요

이 프로젝트는 온라인 게임의 패킷 캡처/재현 도구 테스트를 위해 개발된 TCP 기반 게임 서버/클라이언트입니다.

## 기능

- **네트워크**: Boost.Asio 기반 비동기 TCP 서버
- **멀티플레이어**: 다중 클라이언트 지원, 플레이어 위치 동기화
- **게임 요소**: 이동, NPC 전투, 경험치/레벨업
- **채팅**: 월드 채팅, 귓속말
- **인벤토리/장비**: 슬롯 기반 인벤토리, 무기/방어구 장착, NPC 드롭
- **상점**: 아이템 구매/판매 (골드 시스템)
- **DB 연동**: MySQL (AWS ODBC Driver, Aurora failover 지원)
- **패킷 프로토콜**: 38개 패킷 타입 정의

## 요구사항

- Windows 10/11
- Visual Studio 2022
- Boost 1.78.0+
- MySQL 8.0+ (로컬 또는 Aurora)
- AWS ODBC Driver for MySQL (선택, failover 지원 시)

## 빌드

```bash
# Visual Studio에서 MMORPGSimulator.sln 열기
# 또는 명령줄에서:
build.bat
```

## 실행

### 1. MySQL 설정

```sql
-- mockdb 스키마 생성 후 SP 적용
mysql -h 127.0.0.1 -u admin -p mockdb < scripts/inventory_system.sql
```

### 2. 서버 실행

```bash
run_server.bat
```

출력:
```
Connecting to MySQL...
DB Connected: 127.0.0.1/mockdb
DB connection test successful!
[Server] Started on port 9000
NPCs spawned: 5
Server running... Press Ctrl+C to stop.
```

### 3. 클라이언트 실행

```bash
run_client.bat
```

```
=== MMORPG Simulator ===
Enter Account ID (number): test001
Enter Server IP (default: 127.0.0.1): 
Logging in as ID: test001...
```

### 조작법

- `W/A/S/D`: 이동
- `Space`: 공격 (인접 NPC)
- `T`: 채팅 (월드)
- `Y`: 귓속말
- `I`: 인벤토리
- `E`: 장비 장착
- `B`: 상점
- `Q`: 종료

## 패킷 프로토콜

| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x0101 | CS_LOGIN | C→S | 로그인 요청 |
| 0x0102 | SC_LOGIN_RESULT | S→C | 로그인 결과 |
| 0x0206 | SC_CHAR_INFO | S→C | 캐릭터 정보 |
| 0x0401 | CS_MOVE | C→S | 이동 요청 |
| 0x0402 | SC_MOVE_RESULT | S→C | 이동 결과 |
| 0x0501 | CS_ATTACK | C→S | 공격 요청 |
| 0x0502 | SC_ATTACK_RESULT | S→C | 공격 결과 |
| 0x0503 | SC_NPC_SPAWN | S→C | NPC 스폰 |
| 0x0504 | SC_NPC_DEATH | S→C | NPC 사망 |
| 0x0601 | CS_CHAT | C→S | 채팅 전송 |
| 0x0602 | SC_CHAT | S→C | 채팅 수신 |
| 0x0706 | SC_INVENTORY_UPDATE | S→C | 인벤토리 갱신 |
| 0x0801 | CS_SHOP_OPEN | C→S | 상점 열기 |
| ... | | | 총 38개 패킷 |

전체 프로토콜: [Common/Protocol.h](Common/Protocol.h)

## 프로젝트 구조

```
mmorpg_simulator/
├── Common/
│   ├── Protocol.h      # 패킷 타입/구조체 정의
│   └── Types.h         # 공통 타입
├── GameServer/
│   ├── main.cpp        # 게임 로직
│   ├── TcpServer.*     # Boost.Asio TCP 서버
│   ├── Session.*       # 클라이언트 세션
│   ├── SessionManager.*# 세션 관리
│   ├── PacketHandler.* # 패킷 라우팅
│   └── DBConnection.*  # MySQL ODBC 연결
├── GameClient/
│   ├── main.cpp        # ASCII UI 클라이언트
│   └── TcpClient.*     # Winsock 클라이언트
├── data/
│   └── items.json      # 아이템 정의 데이터
├── scripts/
│   └── inventory_system.sql # DB 스키마 (인벤토리/골드)
└── MMORPGSimulator.sln
```

## 라이선스

MIT License

---

# English

**Related Project:** [Packet Capture Log Agent](https://github.com/hcsung-aws/packet-capture-log-agent) - Tool for capturing/replaying packets from this simulator

## Overview

A simple MMORPG simulator for testing packet capture and replay tools. TCP-based game server/client.

## Features

- **Network**: Boost.Asio async TCP server
- **Multiplayer**: Multiple clients, player position sync
- **Gameplay**: Movement, NPC combat, EXP/Level up
- **Chat**: World chat, whisper
- **Inventory/Equipment**: Slot-based inventory, weapon/armor equip, NPC drops
- **Shop**: Buy/sell items (gold system)
- **Database**: MySQL (AWS ODBC Driver with Aurora failover support)
- **Protocol**: 38 packet types defined

## Requirements

- Windows 10/11
- Visual Studio 2022
- Boost 1.78.0+
- MySQL 8.0+ (local or Aurora)
- AWS ODBC Driver for MySQL (optional, for failover)

## Build

```bash
# Open MMORPGSimulator.sln in Visual Studio
# Or from command line:
build.bat
```

## Run

### 1. MySQL Setup

```sql
-- Create mockdb schema, then apply SPs
mysql -h 127.0.0.1 -u admin -p mockdb < scripts/inventory_system.sql
```

### 2. Start Server

```bash
run_server.bat
```

### 3. Start Client

```bash
run_client.bat
```

### Controls

- `W/A/S/D`: Move
- `Space`: Attack (adjacent NPC)
- `T`: Chat (world)
- `Y`: Whisper
- `I`: Inventory
- `E`: Equip
- `B`: Shop
- `Q`: Quit

## License

MIT License
