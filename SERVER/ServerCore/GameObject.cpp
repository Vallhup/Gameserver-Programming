#include "pch.h"
#include "GameObject.h"

GameObject::GameObject()
	: _id(-1), _x(400), _y(400), _type(ObjectType::PLAYER), _damage(10),
	_lastMoveTime(0), _lastAttackTime(0), _hp(20), _maxHp(1000), _exp(10), _level(1),
	_defaultX(1000), _defaultY(1000)
{
}

GameObject::GameObject(int id, short x, short y)
	: _id(id), _x(x), _y(y), _type(ObjectType::PLAYER), _damage(10),
	_lastMoveTime(0), _lastAttackTime(0), _hp(20), _maxHp(1000), _exp(10), _level(1),
	_defaultX(_x), _defaultY(_y)
{
}

void GameObject::AddExp(int exp)
{
	if (exp <= 0) {
		return;
	}

	_exp += exp;
	UpdateLevel();
}

void GameObject::UpdateLevel()
{
	static const std::vector<int> maxExpTable{ 100, 200, 400, 800, 1600, 3200, 6400, 12800, 25600, 51200 };

	while ((_level < maxExpTable.size()) and (_exp >= maxExpTable[_level - 1])) {
		_exp -= maxExpTable[_level - 1];
		++_level;
		_damage = _level * 10;
		_maxHp = _level * 100;
	}
}

void GameObject::Die()
{
	_hp = 0;
	_isAlive.store(false);
}

void GameObject::Revive()
{
	_hp = _maxHp;
	_isAlive.store(true);
	_x = _defaultX;
	_y = _defaultY;
}

bool GameObject::CheckDie()
{
	return _hp == 0;
}