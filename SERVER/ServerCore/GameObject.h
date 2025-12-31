#pragma once

class Service;
class IocpObject;

struct APos;

enum ObjectType : char {
	PLAYER,
	MONSTER,
	NPC
};

class GameObject
{
public:
	GameObject();
	GameObject(int id, short x, short y);

	virtual ~GameObject() {}

public:
	virtual bool IsVisible() const { return true; }
	virtual void TakeDamage(short damage) abstract;

public:
	const std::string& GetName() const { return _name; }
	const ObjectType GetType() const { return _type; }
	int GetId()      const { return _id; }
	int GetExp()     const { return _exp; }
	short GetX()     const { return _x; }
	short GetY()     const { return _y; }
	short GetHp()    const { return _hp; }
	short GetMaxHp() const { return _maxHp; }
	short GetLevel() const { return _level; }
	short GetDamage() const { return _damage; }
	short GetDefaultX() const { return _defaultX; }
	short GetDefaultY() const { return _defaultY; }
	bool IsAlive()   const { return _isAlive.load(); }

	// Setter
	void SetName(const std::string& name) { _name = name; }
	void SetId(int id) { _id = id; }
	void SetPos(short x, short y) { _x = x; _y = y; }
	void SetHp(short hp) { _hp = std::clamp<short>(hp, 0, _maxHp); }
	void SetAlive(bool alive) { _isAlive.store(alive); }

	virtual void AddExp(int exp);
	virtual void UpdateLevel();

	void IncreaseHp(short value) { _hp = std::min<short>(_hp + value, _maxHp); }
	void DecreaseHp(short value) { _hp = std::max<short>(_hp - value, 0); }

	virtual void Die();
	virtual void Revive();

	bool CheckDie();

protected:
	ObjectType _type;

	int _id;
	short _x, _y;
	std::string _name;

	// 레벨, HP 등 Game Contents에 추가적으로 필요한 변수들
	short _hp;
	short _level;
	int _exp;
	short _damage;

	short _maxHp;
	short _defaultX, _defaultY;

	std::atomic<bool> _isAlive{ true };

public:
	int _lastMoveTime;
	int _lastAttackTime;
};