#pragma once

struct lua_State;

enum MonsterType : char {
	Peace,
	Agro
};

enum MovementType : char {
	Fixed,
	Roaming
};

enum NpcState : char {
	ST_Random,
	ST_Agro,
};

class Monster :
	public IocpObject,
	public GameObject,
	public std::enable_shared_from_this<Monster>
{
public:
	Monster(int id, short x, short y, const std::string& name, MonsterType mType = MonsterType::Agro, MovementType mvType = MovementType::Roaming);

public:
	void WakeUp(bool force = false, int playerId = -1);

	void OnMove();
	void OnHeal();

	void RegisterTimer();

public:
	// Service 관련
	void SetService(std::shared_ptr<Service> service) { _service = service; }
	const std::shared_ptr<Service>& GetService() const { return _service.lock(); }

public:
	// 인터페이스 구현
	virtual HANDLE GetHandle() override;
	virtual void Dispatch(class ExpOver* expOver, int nuOfBytes = 0) override;

public:
	int GetTypeId() const { return _typeId; }
	char GetState() const { return _state.load(); }

	void SetState(NpcState state) { _state.store(state); }
	void SetMovePending(bool move) { _movePending.store(move); }
	void SetHealPending(bool heal) { _healPending.store(heal); }
	void SetActive(bool active) { _isActive.store(active); }
	void SetLevel(int level)
	{
		_level = level;
		_hp = _level * 20;
		_exp = _level * _level * 2;
		_damage = _level * 10;
	}

	virtual void Die() override;

public:
	std::unordered_set<int> RandomMove(std::shared_ptr<Service> service);
	std::unordered_set<int> AStarMove(std::shared_ptr<Service> service, APos npcPos, APos targetPos);

public:
	virtual void TakeDamage(short damage) override;

private:
	std::weak_ptr<Service> _service;

private:
	int _typeId{ 1 };

private:
	std::atomic<bool> _isActive{ false };
	std::atomic<bool> _movePending{ false };
	std::atomic<bool> _healPending{ false };

	std::atomic<NpcState> _state{ NpcState::ST_Random };

	MonsterType _basemonsterType{ MonsterType::Peace };
	MovementType _movementType{ MovementType::Fixed };

	MonsterType _currentMonsterType{ MonsterType::Peace };

	std::unique_ptr<class IMonsterBehavior> _behavior;
};

