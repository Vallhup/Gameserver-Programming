#pragma once

class Monster;
class Service;

class IMonsterBehavior
{
public:
	virtual ~IMonsterBehavior() = default;

	virtual void OnMove(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) abstract;
	virtual void OnHeal(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) abstract;
};

class PeaceFixedBehavior : public IMonsterBehavior
{
public:
	virtual ~PeaceFixedBehavior() = default;

	virtual void OnMove(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) override;
	virtual void OnHeal(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) override;
};

class PeaceRoamingBehavior : public IMonsterBehavior
{
public:
	virtual ~PeaceRoamingBehavior() = default;

	virtual void OnMove(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) override;
	virtual void OnHeal(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) override;
};

class AgroFixedBehavior : public IMonsterBehavior
{
public:
	virtual ~AgroFixedBehavior() = default;

	virtual void OnMove(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) override;
	virtual void OnHeal(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) override;
};

class AgroRoamingBehavior : public IMonsterBehavior
{
public:
	virtual ~AgroRoamingBehavior() = default;

	virtual void OnMove(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) override;
	virtual void OnHeal(const std::shared_ptr <Monster>& owner, const std::shared_ptr<Service>& service) override;
};

