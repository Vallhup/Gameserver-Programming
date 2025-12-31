#pragma once

#define NOMINMAX

#include <winsock2.h>
#include <mswsock.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <iostream>
#include <vector>
#include <array>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <concurrent_unordered_map.h>
#include <concurrent_queue.h>
#include <concurrent_priority_queue.h>
#include <concurrent_unordered_set.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <type_traits>
#include <chrono>
#include <random>
#include <functional>
#include <codecvt>
#include <future>

#include "RecvBuffer.h"
#include "AtomicQueue.h"

#include "ExpOver.h"
#include "IocpCore.h"

#include "Service.h"
#include "Session.h"
#include "Listener.h"
#include "Sector.h"
#include "ViewManager.h"
#include "GameObject.h"
#include "Monster.h"
#include "Npc.h"
#include "MonsterBehavior.h"
#include "ObjectManager.h"
#include "CombatManager.h"

#include "AStar.h"
#include "ChatManager.h"
#include "PacketFactory.h"

#include "Party.h"
#include "PartyManager.h"
#include "Inventory.h"
#include "ItemManager.h"
#include "Quest.h"
#include "QuestManager.h"	
#include "QuestType.h"

#include "Logger.h"
#include "DBManager.h"
#include "Macro.h"
#include "protocol.h"

#include "include/lua.hpp"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "MSWSock.LIB")
#pragma comment(lib, "lua54.lib")

constexpr short SERVER_PORT = 4000;

using ServicePtr = std::shared_ptr<class Service>;
using IocpCorePtr = std::shared_ptr<class IocpCore>;
using GameObjectPtr = std::shared_ptr<class GameObject>;
using SessionPtr = std::shared_ptr<class Session>;
using GameSessionPtr = std::shared_ptr<class GameSession>;
using ListenerPtr = std::shared_ptr<class Listener>;