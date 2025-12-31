#pragma once

class GameObject;

class Npc : public GameObject
{
public:
	Npc(int id, int x, int y, std::string_view name) 
		: GameObject(id, x, y), _npcId(id), _name(name) 
	{
		_type = ObjectType::NPC;
	}

	virtual void TakeDamage(short damage) override {}

	int GetNpcId() const { return _npcId; }
	void SetNpcId(int npcId) { _npcId = npcId;  }

private:
	int _npcId;
	std::string _name;
};