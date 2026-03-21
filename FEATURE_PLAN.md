# mmorpg_simulator 기능 추가 계획

## 구현 순서

### 1. 채팅 (Chat)
- 채널: World, Whisper, Party (파티 구현 후)
- 패킷:
  - `CS_CHAT` (C2S): channel(uint8), targetName(string32, 귓속말용), message(string128)
  - `SC_CHAT` (S2C): channel(uint8), senderName(string20), message(string128)
- 서버: World → 전체 Broadcast, Whisper → 대상 세션 검색 후 Send
- DB: 불필요 (메모리만)
- 난이도: 낮음

### 2. 인벤토리/아이템 (Inventory & Items)
- 아이템 정의: itemId, name, type(weapon/armor/consumable), stats(atk/def/hp)
- 인벤토리: 슬롯 기반 (최대 20칸)
- NPC 드롭: 사망 시 확률적 아이템 드롭
- 패킷:
  - `SC_ITEM_DROP` (S2C): itemId, itemName, slot
  - `CS_ITEM_USE` (C2S): slot
  - `SC_ITEM_USE_RESULT` (S2C): success, slot, effect
  - `CS_ITEM_EQUIP` (C2S): slot
  - `SC_EQUIP_RESULT` (S2C): success, slot, statChanges
  - `SC_INVENTORY_UPDATE` (S2C): 전체 또는 단일 슬롯 갱신
- DB: 아이템 테이블 + 인벤토리 테이블 (SP 추가)
- 난이도: 중

### 3. 상점 (Shop)
- NPC 상점: 고정 아이템 목록, 골드로 구매/판매
- 상점 NPC: 맵에 고정 위치, 인접 시 상호작용
- 패킷:
  - `CS_SHOP_OPEN` (C2S): npcUid
  - `SC_SHOP_LIST` (S2C): items[] (itemId, name, price)
  - `CS_SHOP_BUY` (C2S): itemId, count
  - `SC_SHOP_RESULT` (S2C): success, itemId, remainGold
  - `CS_SHOP_SELL` (C2S): slot, count
- 의존: 인벤토리 시스템
- 난이도: 중

### 4. 파티 (Party)
- 최대 4인 파티
- 파티원 HP/위치 동기화
- 파티 내 EXP/드롭 분배
- 패킷:
  - `CS_PARTY_INVITE` (C2S): targetName(string20)
  - `SC_PARTY_INVITE` (S2C): inviterName(string20)
  - `CS_PARTY_ACCEPT` (C2S): accept(uint8)
  - `SC_PARTY_UPDATE` (S2C): memberCount, members[] (charUid, name, hp, maxHp, posX, posY)
  - `CS_PARTY_LEAVE` (C2S)
  - `SC_PARTY_LEAVE` (S2C): charUid
- 의존: 채팅 (파티 채널)
- 난이도: 중~높

## 공통 고려사항
- Protocol.h에 패킷 타입 추가 시 기존 번호 체계 유지 (0x06xx=Chat, 0x07xx=Item, 0x08xx=Shop, 0x09xx=Party)
- mmorpg_simulator.json (packet-capture-log-agent)도 동기화 필요
- 각 기능 구현 후 E2E 캡처 테스트로 검증

## 관련 프로젝트
- mmorpg_simulator: 서버/클라이언트 구현
- packet-capture-log-agent: 프로토콜 JSON 동기화 + 캡처 검증
