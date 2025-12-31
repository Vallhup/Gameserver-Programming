#include "pch.h"
#include "Listener.h"

Listener::Listener(const std::shared_ptr<Service>& service)
{
	_service = service;
	_socket = INVALID_SOCKET;
	_accepting.store(false);
}

Listener::~Listener()
{
	StopAccept();

	for (AcceptOver* acceptOver : _acceptOvers) {
		delete acceptOver;
	}
	_acceptOvers.clear();

	CloseSocket();
}

bool Listener::StartAccept()
{
	LOG_DBG("StartAccept Listener");

	auto service = _service.lock();
	if (nullptr == service) {
		LOG_ERR("StartAccept failed: service is nullptr");
		return false;
	}

	_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == _socket) {
		LOG_ERR("WSASocket failed: %d", WSAGetLastError());
		return false;
	}

	if (false == service->GetIocpCore()->Register(shared_from_this())) {
		LOG_ERR("Filed to register Listen Socket to IOCP");
		return false;
	}

	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (SOCKET_ERROR == bind(_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(SOCKADDR_IN))) {
		LOG_ERR("bind failed: %d", WSAGetLastError());
		return false;
	}

	if (listen(_socket, SOMAXCONN)) {
		LOG_ERR("listen failed: %d", WSAGetLastError());
		return false;
	}

	_accepting.store(true);
	
	const unsigned int acceptCount = std::thread::hardware_concurrency() * 2;
	_acceptOvers.resize(acceptCount);
	for (unsigned int i = 0; i < acceptCount; ++i) {
		AcceptOver* acceptOver = new AcceptOver;
		acceptOver->Init(shared_from_this());
		_acceptOvers.push_back(acceptOver);

		doAccept(acceptOver);
	}

	LOG_INF("Posted initial %d AcceptEx calls", acceptCount);
	return true;
}

void Listener::StopAccept()
{
	_accepting = false;
	CancelIoEx(GetHandle(), nullptr);
}

void Listener::CloseSocket()
{
	closesocket(_socket);
}

HANDLE Listener::GetHandle()
{
	return reinterpret_cast<HANDLE>(_socket);
}

void Listener::Dispatch(ExpOver* expOver, int numOfBytes)
{
	if (expOver->_operationType == OperationType::Accept) {
		AcceptOver* acceptOver = static_cast<AcceptOver*>(expOver);
		AcceptCallback(acceptOver);
	}
}

void Listener::doAccept(AcceptOver* acceptOver)
{
	if (not _accepting.load()) {
		return;
	}

	GameSessionPtr session = std::make_shared<GameSession>();

	acceptOver->Init(shared_from_this());
	acceptOver->_session = session;

	DWORD bytesReceived{ 0 };
	BOOL result = AcceptEx(_socket, session->GetSocket(), acceptOver->_buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		&bytesReceived, static_cast<LPOVERLAPPED>(acceptOver));

	if (result == FALSE) {
		int error = WSAGetLastError();
		if (error != ERROR_IO_PENDING) {
			LOG_ERR("AcceptEx Error : %d", error);
		}
	}

	else {
		LOG_INF("AcceptEx Success");
	}
}

void Listener::AcceptCallback(AcceptOver* acceptOver)
{
	if (not _accepting.load()) {
		return;
	}

	ServicePtr service = _service.lock();
	if (not service) {
		LOG_WRN("AcceptCallback failed: service is nullptr");
		return;
	}

	// 어떤 Session이 Accept했는지 확인
	std::shared_ptr<GameSession> session = acceptOver->_session;

	setsockopt(session->GetSocket(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&_socket, sizeof(_socket));

	// session의 socket을 IocpCore에 등록
	service->GetIocpCore()->Register(session);

	// session의 Service, Inventory 등록
	session->SetService(service);
	session->SetInventory(std::make_shared<Inventory>(session));

	// session Recv 시작
	session->doRecv();

	LOG_INF("New Session[%d] accepted and initialized", session->GetId());
	doAccept(acceptOver);
}