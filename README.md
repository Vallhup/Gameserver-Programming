# GameServer Programming Term Project

## 1. Overview

This project is a multiplayer game serve rterm project that implements core MMO-style systems such as login, movement, combat, parties, quests, and large-scale NPC management.

The server is designed with server-authoritative logic, database persistence, and concurrency-aware data structures.

## 2. Gameplay

### Basic Controls

- A: Attack
- Q: Accept quest from NPC
- 1: Use HP Potion
- Tab: Toggle Quest UI
- I: Toggle Inventory UI

- Left Click:
  - NPC: Complete quest / receive next quest
  - Player: Send party request

- P: Leave party
- Chat supported (including Korean input)

### Core Systems

**Party System**

A player can send party requests to multiple players simultaneously

Party requests are rejected if:
 - The target player already has a party
 - The target player already has a pending request
 - Maximum party size: 6 players

Party UI displays member status:
 - Online & Alive: Green
 - Online & Dead: Red
 - Offline: Gray

Experience is shared among party members

Party state is persisted in the database

Party remains intact after logout/login unless disbanded

**Item System**

HP Potion implemented

30% drop chance on monster kill

Items are added directly to player inventory

Stackable up to 9999

Using a potion restores 5 HP

**Quest System**

Quest progress tracking

NPC interaction without separate dialogue scripts

NPC displays ! or ? based on quest state

Five-step chained quest line:
 - Kill 3 Peacefixed monsters
 - Kill 5 PeaceRoaming monsters
 - Kill 7 AgroFixed monstes
 - Kill 10 AgroRoaming monsters
 - Use 1 HP Potion

 ## 3. World Design

 **Map**

 Size: 2000 x 2000

 View Windows: 20 x 20 (effective vision: 15 x 15)

 Obstacles loaded from text files

 Walkable tiles: white / Obstacles: black

 Map data is immutable after server startup

 **Characters**

 All Character data persisted in DB

 Passive regeneration: 10% max HP every 5 seconds

 On death: 
  - Respawn at (1000, 1000)
  - HP restored
  - Experience reduced by 50% (level unchanged)

**Monsters**

Defined via Lua scripts (type, level, spawn position)

Loaded during server initialization

Total monster count: 200,000

Monster Types:
 - PeaceFixed: Passive, stationary
 - PeaceRoaming: Passiva, random movement within 20 x 20
 - Agrofixed: Aggressive within 11 x 11 range, stationary
 - AgroRoaming: Aggressive within 11 x 11 range, roaming

Use A* pathfinding to avoid obstacles

## 4. Combat System

Player Attack:
 - Triggered by A
 - Attacks in four directions (up/down/left/right)
 - Cooldown: 1 attack per second

Monster Attack:
 - Chases player
 - Deals damage on position overlap

Combat logs displayed in message window:
 - Player Attack
 - Healing
 - Death / Revive
 - Monster Attack

## 5. Login System

Duplicate logins with the same ID are not allowed

New Ids are created in the database automatically

Existing IDs load persisted position and stats on login

## 6. Network Protocol Overview

**Login / Logout / Chat**

CS_LOGIN_PACKET -> login request

SC_LOGIN_INFO / SC_LOGIN_FAIL -> login response

CS_LOGOUT_PACKET -> logout request


Chat messages are broadcast via SC_CHAT_PACKET

targetId == -1 indicates system / combat messages

**Movement & Combat**

Clients sends CS_MOVE_PACKET

Server performs collision validation

Visibility-based updates:
 - SC_ADD_OBJECT_PACKET
 - SC_REMOVE_OBJECT_PACKET
 - SC_MOVE_OBJECT_PACKET

Attacks validated server-side before notification

**Party / Item / Quest**

Party request / response handled explicitly via packets

Item acquisition and usage validated server-side

Quest state updated and synchronized with client

NPC quest symbols updated dynamically

## 7. Data Structures & Algorithms

**Data Structures**

Object Container: concurrent_unordered_map + atomic_shared_ptr + GameObject
 - Unified base class for Player, Monster, NPC

Sector System: unordered_set with shared_mutex
 - Optimized for frequent reads

ViewList: atomic_shared_ptr + unordered_set

Copy-on-write strategy to reduce lock contention

Timer Queue: concurrent_priority_queue

Map: Static 2D array loaded at startup (no synchronization required)

**Algorithms**

Monster Movement:
 - Random movement with bounded retry
 - A* pathfinding with Manhattan distance heuristic

