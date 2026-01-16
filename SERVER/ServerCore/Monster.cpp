#include "pch.h"
#include "Monster.h"

Monster::Monster(int id, short x, short y, const std::string& name, MonsterType mType, MovementType mvType)
	:GameObject(id, x, y), _basemonsterType(mType), _movementType(mvType), _currentMonsterType(mType)
{
	_type = ObjectType::MONSTER;
	_name = name;

	if (_basemonsterType == MonsterType::Peace) {
		if (_movementType == MovementType::Fixed) {
			_behavior = std::make_unique<PeaceFixedBehavior>();
			_typeId = 1;
		}

		else {
			_behavior = std::make_unique<PeaceRoamingBehavior>();
			_typeId = 2;
		}
	}

	else {
		if (_movementType == MovementType::Fixed) {
			_behavior = std::make_unique<AgroFixedBehavior>();
			_typeId = 3;
		}

		else {
			_behavior = std::make_unique<AgroRoamingBehavior>();
			_typeId = 4;
		}
	}
}

void Monster::WakeUp(bool force, int playerId)
{
	if (force) {
		if (_isActive.exchange(true)) {
			return;
		}

		if (_isAlive.load()) {
			_movePending.store(false);
			RegisterTimer();
		}
	}

	else {
		if (not _isActive.exchange(true)) {
			if (_isAlive.load()) {
				_movePending.store(false);
				RegisterTimer();
			}
		}
	}
}

void Monster::OnMove()
{
	if (not _isAlive.load()) {
		_movePending.store(false);
		return;
	}

	if (not _isActive.load()) {
		_movePending.store(false);
		return;
	}

	if (not _movePending.exchange(false)) {
		return;
	}

	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	_behavior->OnMove(shared_from_this(), service);
}

void Monster::OnHeal()
{
	_movePending.store(false);
	_healPending.store(false);

	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	_currentMonsterType = _basemonsterType;
	if (_currentMonsterType == MonsterType::Peace) {
		if (_movementType == MovementType::Fixed) {
			_behavior = std::make_unique<PeaceFixedBehavior>();
		}

		else {
			_behavior = std::make_unique<PeaceRoamingBehavior>();
		}
	}

	else {
		if (_movementType == MovementType::Fixed) {
			_behavior = std::make_unique<AgroFixedBehavior>();
		}

		else {
			_behavior = std::make_unique<AgroRoamingBehavior>();
		}
	};

	_behavior->OnHeal(shared_from_this(), service);
}

void Monster::RegisterTimer()
{
	if (_movePending.exchange(true)) {
		LOG_INF("Monster %d: RegisterTimer skipped, movePending already true", _id);
		return;
	}

	if (not _isAlive.load()) {
		LOG_DBG("Monster %d: RegisterTimer blocked, isAlive == false", _id);
		return;
	}

	if (auto service = _service.lock()) {
		int interval = service->GetRandomInterval(1000, 2000);
		service->_timerQueue.push(
			Event{ _id,
			std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(interval),
			EV_MOVE, 0 });
	}
}

std::unordered_set<int> Monster::RandomMove(std::shared_ptr<Service> service)
{
	// 0. Random State에서만 실행
	if (_state.load() != NpcState::ST_Random) {
		return {};
	}

	int oldX = GetX();
	int oldY = GetY();

	// 2. 이동 범위 제한 (Default Pos를 중심으로 20 x 20)
	int minX = std::max<int>(0, _defaultX - 10);
	int maxX = std::min<int>(W_WIDTH - 1, _defaultX + 10);

	int minY = std::max<int>(0, _defaultY - 10);
	int maxY = std::min<int>(W_HEIGHT - 1, _defaultY + 10);

	thread_local static std::default_random_engine dre{ std::random_device{}() };

	std::array<int, 4> dirs{ 0, 1, 2, 3 };
	std::shuffle(dirs.begin(), dirs.end(), dre);

	const int dx[4]{ 0, 0, -1, 1 };
	const int dy[4]{ -1, 1, 0, 0 };

	// 3. 범위 내에서 Random Move
	for (int dir : dirs) {
		int nx = _x + dx[dir];
		int ny = _y + dy[dir];

		if ((nx < minX) or (nx > maxX) or (ny < minY) or (ny > maxY)) continue;
		if (not service->_navigationMap[ny][nx]) continue;

		_x = nx;
		_y = ny;

		break;
	}

	service->OnNpcMove(shared_from_this(), oldX, oldY);

	return service->CollectViewList(shared_from_this());
}

std::unordered_set<int> Monster::AStarMove(std::shared_ptr<Service> service, APos npcPos, APos targetPos)
{
	// Agro State에서만 실행
	if (_state.load() != NpcState::ST_Agro) {
		return {};
	}

	// Temp : service에서 Map Data 파싱 필요
	auto path = AStar(service->_navigationMap, npcPos, targetPos);
	if (path.size() > 1) {
		int oldX = GetX();
		int oldY = GetY();

		// 이동
		APos next = path[1];
		_x = next.x;
		_y = next.y;

		service->OnNpcMove(shared_from_this(), oldX, oldY);
	}

	return service->CollectViewList(shared_from_this());
}

void Monster::TakeDamage(short damage)
{
	if (not _isAlive.load()) {
		return;
	}

	DecreaseHp(damage);

	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	std::string str = "Monster[" + std::to_string(_id - MAX_USER) + "] suffered " + std::to_string(damage) + " damage.";
	service->Broadcast(PacketFactory::BuildChatPacket(shared_from_this(), str.c_str()));

	if (_currentMonsterType == MonsterType::Peace) {
		_state.store(NpcState::ST_Agro);
		_currentMonsterType = MonsterType::Agro;

		if (_movementType == MovementType::Fixed) {
			_behavior = std::make_unique<AgroFixedBehavior>();
		}

		else {
			_behavior = std::make_unique<AgroRoamingBehavior>();
		}
	}

	if (CheckDie()) {
		Die();
		return;
	}
}

HANDLE Monster::GetHandle()
{
	return INVALID_HANDLE_VALUE;
}

void Monster::Dispatch(ExpOver* expOver, int numBytes)
{
	switch (expOver->_operationType) {
	case NpcMove:
		OnMove();
		delete static_cast<EventOver*>(expOver);
		break;

	case NpcHeal:
		OnHeal();
		delete static_cast<EventOver*>(expOver);
		break;

	default:
		LOG_ERR("Unknown Monster OperationType: %d", expOver->_operationType);
		break;
	}
}
	
void Monster::Die()
{
	auto service = _service.lock();
	if (nullptr == service) {
		return;
	}

	if (not _isAlive.exchange(false)) {
		return;
	}

	_movePending.store(false);
	
	// Respawn Event Push
	if (not _healPending.exchange(true)) {
		service->_timerQueue.push(Event{ GetId(),
			std::chrono::high_resolution_clock::now() + std::chrono::seconds(30),
			EV_HEAL, 0 });
	}
}