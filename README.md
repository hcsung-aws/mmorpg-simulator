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
- **DB 연동**: MySQL (AWS ODBC Driver, Aurora failover 지원)
- **패킷 프로토콜**: 17개 패킷 타입 정의

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
-- mockdb 스키마 및 spAccountLogin SP 필요
-- 스키마 파일: (별도 제공)
```

### 2. 서버 실행

```bash
build\GameServer.exe
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
build\GameClient.exe
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
- `Q`: 종료

## 패킷 프로토콜

| Type | Name | Direction | Description |
|------|------|-----------|-------------|
| 0x0101 | CS_LOGIN | C→S | 로그인 요청 |
| 0x0102 | SC_LOGIN_RESULT | S→C | 로그인 결과 |
| 0x0204 | SC_CHAR_INFO | S→C | 캐릭터 정보 |
| 0x0301 | CS_MOVE | C→S | 이동 요청 |
| 0x0302 | SC_MOVE_RESULT | S→C | 이동 결과 |
| 0x0401 | CS_ATTACK | C→S | 공격 요청 |
| 0x0402 | SC_ATTACK_RESULT | S→C | 공격 결과 |
| 0x0403 | SC_NPC_SPAWN | S→C | NPC 스폰 |
| 0x0404 | SC_NPC_DEATH | S→C | NPC 사망 |
| 0x0405 | SC_EXP_UPDATE | S→C | 경험치 업데이트 |
| 0x0406 | SC_LEVEL_UP | S→C | 레벨업 |

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
- **Database**: MySQL (AWS ODBC Driver with Aurora failover support)
- **Protocol**: 17 packet types defined

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
-- Requires mockdb schema and spAccountLogin SP
```

### 2. Start Server

```bash
build\GameServer.exe
```

### 3. Start Client

```bash
build\GameClient.exe
```

### Controls

- `W/A/S/D`: Move
- `Space`: Attack (adjacent NPC)
- `Q`: Quit

## License

MIT License
