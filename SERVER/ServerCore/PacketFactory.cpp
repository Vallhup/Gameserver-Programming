#include "pch.h"
#include "PacketFactory.h"

std::vector<char> PacketFactory::BuildLoginOkPacket(const GameObject& target)
{
	SC_LOGIN_INFO_PACKET login;
	login.id = target.GetId();
	login.size = sizeof(login);
	login.type = SC_LOGIN_INFO;
	login.x = target.GetX();
	login.y = target.GetY();

	if (target.GetType() == ObjectType::PLAYER) {
		auto player = static_cast<const GameSession*>(&target);
		login.id = player->GetUserID();
	}

	return Serialize(login);
}

std::vector<char> PacketFactory::BuildLoginFailPacket(const GameObject& target)
{
	SC_LOGIN_FAIL_PACKET fail;
	fail.size = sizeof(fail);
	fail.type = SC_LOGIN_FAIL;

	return Serialize(fail);
}

std::vector<char> PacketFactory::BuildAddPacket(const GameObject& target, char symbol)
{
	SC_ADD_OBJECT_PACKET add;
	add.id = target.GetId();
	add.size = sizeof(add);
	add.type = SC_ADD_OBJECT;
	add.x = target.GetX();
	add.y = target.GetY();
	add.objType = target.GetType();
	add.questSymbol = symbol;

	if (add.objType == ObjectType::MONSTER) {
		auto monster = static_cast<const Monster*>(&target);
		if (nullptr != monster) {
			add.monsterType = monster->GetTypeId();
		}

		else {
			add.monsterType = -1;
		}
	}

	else if (add.objType == ObjectType::PLAYER) {
		auto player = static_cast<const GameSession*>(&target);
		add.id = player->GetUserID();
		add.monsterType = -1;
	}

	else {
		add.monsterType = -1;
	}

	strcpy_s(add.name, NAME_SIZE, target.GetName().c_str());
	add.name[NAME_SIZE - 1] = '\0';

	return Serialize(add);
}

std::vector<char> PacketFactory::BuildMovePacket(const GameObject& target)
{
	SC_MOVE_OBJECT_PACKET move;
	move.id = target.GetId();
	move.size = sizeof(move);
	move.type = SC_MOVE_OBJECT;
	move.x = target.GetX();
	move.y = target.GetY();
	move.move_time = target._lastMoveTime;

	if (target.GetType() == ObjectType::PLAYER) {
		auto player = static_cast<const GameSession*>(&target);
		move.id = player->GetUserID();
	}

	return Serialize(move);
}

std::vector<char> PacketFactory::BuildRemovePacket(const GameObject& target)
{
	SC_REMOVE_OBJECT_PACKET remove;
	remove.id = target.GetId();
	remove.size = sizeof(remove);
	remove.type = SC_REMOVE_OBJECT;

	if (target.GetType() == ObjectType::PLAYER) {
		auto player = static_cast<const GameSession*>(&target);
		remove.id = player->GetUserID();
	}

	return Serialize(remove);
}

std::vector<char> PacketFactory::BuildStatChangePacket(const GameObject& target)
{
	SC_STAT_CHANGE_PACKET stat;
	stat.exp = target.GetExp();
	stat.hp = target.GetHp();
	stat.level = target.GetLevel();
	stat.max_hp = target.GetMaxHp();
	stat.size = sizeof(stat);
	stat.type = SC_STAT_CHANGE;

	return Serialize(stat);
}

std::vector<char> PacketFactory::BuildPartyRequestPacket(int fromId)
{
	SC_PARTY_REQUEST_PACKET request;
	request.fromId = fromId;
	request.size = sizeof(request);
	request.type = SC_PARTY_REQUEST;

	return Serialize(request);
}

std::vector<char> PacketFactory::BuildPartyResultPacket(int targetId, bool accept)
{
	SC_PARTY_RESULT_PACKET result;
	result.acceptFlag = accept;
	result.size = sizeof(result);
	result.targetId = targetId;
	result.type = SC_PARTY_RESULT;

	return Serialize(result);
}

std::vector<char> PacketFactory::BuildPartyUpdatePacket(const Party& party)
{
	auto infos = party.GetMemberInfos();

	SC_PARTY_UPDATE_PACKET update;
	update.memberCount = infos.size();
	update.partyId = party.GetId();
	update.size = static_cast<unsigned char>(sizeof(update) + sizeof(MemberInfo) * infos.size());
	update.type = SC_PARTY_UPDATE;

	std::vector<char> buf(update.size);
	std::memcpy(buf.data(), &update, sizeof(update));
	std::memcpy(buf.data() + sizeof(update), infos.data(), sizeof(MemberInfo) * infos.size());

	return buf;
}

std::vector<char> PacketFactory::BuildPartyDisbandPacket(const Party& party)
{
	SC_PARTY_DISBAND_PACKET disband;
	disband.size = sizeof(disband);
	disband.type = SC_PARTY_DISBAND;
	disband.partyId = party.GetId();

	return Serialize(disband);
}

std::vector<char> PacketFactory::BuildAddItemPacket(char itemId, int count)
{
	SC_ADD_ITEM_PACKET item;
	item.count = count;
	item.itemId = itemId;
	item.size = sizeof(item);
	item.type = SC_ADD_ITEM;

	return Serialize(item);
}

std::vector<char> PacketFactory::BuildUseItemOkPacket(char itemId)
{
	SC_USE_ITEM_OK_PACKET itemOk;
	itemOk.itemId = itemId;
	itemOk.size = sizeof(itemOk);
	itemOk.type = SC_USE_ITEM_OK;

	return Serialize(itemOk);
}

std::vector<char> PacketFactory::BuildQuestAcceptPacket(int questId, const std::string& title, const std::string& desc, const std::string& progress)
{
	SC_QUEST_ACCEPT_PACKET accept;
	accept.size = sizeof(accept);
	accept.type = SC_QUEST_ACCEPT;
	accept.questId = questId;

	strcpy_s(accept.title, sizeof(accept.title), title.c_str());
	accept.title[TITLE_SIZE - 1] = '\0';

	strcpy_s(accept.description, sizeof(accept.description), desc.c_str());
	accept.description[DESCRIPTION_SIZE - 1] = '\0';

	strcpy_s(accept.progressText, sizeof(accept.progressText), progress.c_str());
	accept.progressText[PROGRESS_SIZE - 1] = '\0';

	return Serialize(accept);
}

std::vector<char> PacketFactory::BuildQuestUpdatePacket(int questId, const std::string& progress)
{
	SC_QUEST_UPDATE_PACKET update;
	update.size = sizeof(update);
	update.type = SC_QUEST_UPDATE;
	update.questId = questId;

	strcpy_s(update.progressText, sizeof(update.progressText), progress.c_str());
	update.progressText[PROGRESS_SIZE - 1] = '\0';

	return Serialize(update);
}

std::vector<char> PacketFactory::BuildQuestCompletePacket(int questId)
{
	SC_QUEST_COMPLETE_PACKET complete;
	complete.size = sizeof(complete);
	complete.type = SC_QUEST_COMPLETE;
	complete.questId = questId;

	return Serialize(complete);
}

std::vector<char> PacketFactory::BuildQuestSymbolUpdatePacket(int npcId, char symbol)
{
	SC_QUEST_SYMBOL_UPDATE_PACKET symbolUpdate;
	symbolUpdate.size = sizeof(symbolUpdate);
	symbolUpdate.type = SC_QUEST_SYMBOL_UPDATE;
	symbolUpdate.npcId = npcId;
	symbolUpdate.questSymbol = symbol;

	return Serialize(symbolUpdate);
}

std::vector<char> PacketFactory::BuildChatPacket(const std::shared_ptr<GameObject>& object, const char* msg)
{
	SC_CHAT_PACKET chat;
	chat.size = sizeof(chat);
	chat.type = SC_CHAT;
	chat.id = object->GetId();
	chat.targetId = -1;
	strcpy_s(chat.message, CHAT_SIZE - 1, msg);
	chat.message[CHAT_SIZE - 1] = '\0';

	if (object->GetType() == ObjectType::PLAYER) {
		auto player = static_pointer_cast<GameSession>(object);
		chat.id = player->GetUserID();
	}

	return Serialize(chat);
}
