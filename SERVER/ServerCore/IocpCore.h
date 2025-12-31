#pragma once

class IocpObject
{
public:
	virtual HANDLE GetHandle() abstract;
	virtual void Dispatch(class ExpOver* expOver, int nuOfBytes = 0) abstract;

public:
	int GetSessionId() const { return _sessionId; }
	void SetSessionId(int sessionId) { _sessionId = sessionId; }

protected:
	int _sessionId{ 0 };
};

class Service;

class IocpCore
{
public:
	IocpCore() = default;
	IocpCore(const std::shared_ptr<Service>& service);
	~IocpCore();

public:
	HANDLE GetHandle() { return _iocpHandle; };

public:
	bool Register(std::shared_ptr<IocpObject> iocpObject);
	bool Dispatch(unsigned int timeoutMs = INFINITE);

private:
	HANDLE _iocpHandle;
	std::weak_ptr<Service> _service;

	static int objectId;
};

