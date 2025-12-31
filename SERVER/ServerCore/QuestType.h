#pragma once

class GameSession;

enum QuestEventType : char {
	KillMonster,
	TalkToNpc,
	UseItem
};

class QuestType {
public:
	virtual ~QuestType() = default;

public:
	virtual std::unique_ptr<QuestType> Clone() const abstract;

public:
	virtual void OnEvent(const std::shared_ptr<GameSession>& session) abstract;
	virtual void OnEvent(const std::shared_ptr<GameSession>& session, int param) abstract;
	virtual bool IsComplete() const abstract;

	virtual std::string GetProgressText() const abstract;
	virtual int GetProgress() const abstract;
	virtual QuestEventType GetEventType() const abstract;
	
	virtual int GetNpcId() const { return _npcId; }

protected:
	int _npcId;
};

class KillMonsterType : public QuestType {
public:
	KillMonsterType(int npcId, int monsterId, int count);
	KillMonsterType(const KillMonsterType&) = default;

	virtual ~KillMonsterType() = default;

public:
	virtual std::unique_ptr<QuestType> Clone() const override;

public:
	virtual void OnEvent(const std::shared_ptr<GameSession>& session) override;
	virtual void OnEvent(const std::shared_ptr<GameSession>& session, int monsterId) override;
	virtual bool IsComplete() const override;

	virtual std::string GetProgressText() const override;
	virtual int GetProgress() const override;
	virtual QuestEventType GetEventType() const override;

private:
	int _monsterId;
	int _targetCount;
	int _currentCount;
};

class TalkToNpcType : public QuestType {
public:
	TalkToNpcType(int npcId);
	TalkToNpcType(const TalkToNpcType&) = default;

	virtual ~TalkToNpcType() = default;

public:
	virtual std::unique_ptr<QuestType> Clone() const override;

public:
	virtual void OnEvent(const std::shared_ptr<GameSession>& session) override;
	virtual void OnEvent(const std::shared_ptr<GameSession>& session, int npcId) override;
	virtual bool IsComplete() const override;

	virtual std::string GetProgressText() const override;
	virtual int GetProgress() const override;
	virtual QuestEventType GetEventType() const override;

private:
	bool _talked{ false };
};

class UseItemType : public QuestType {
public:
	UseItemType(int npcId, int itemId);

	virtual ~UseItemType() = default;

public:
	virtual std::unique_ptr<QuestType> Clone() const override;

public:
	virtual void OnEvent(const std::shared_ptr<GameSession>& session) override;
	virtual void OnEvent(const std::shared_ptr<GameSession>& session, int itemId) override;
	virtual bool IsComplete() const override;

	virtual std::string GetProgressText() const override;
	virtual int GetProgress() const override;
	virtual QuestEventType GetEventType() const override;

private:
	int _targetItemId;
	bool _used{ false };
};


