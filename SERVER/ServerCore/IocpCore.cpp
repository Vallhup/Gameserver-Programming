#include "pch.h"
#include "IocpCore.h"

int IocpCore::objectId{ 0 };

IocpCore::IocpCore(const std::shared_ptr<Service>& service)
{
	_iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	_service = service;
}

IocpCore::~IocpCore()
{
	CloseHandle(_iocpHandle);
}

bool IocpCore::Register(std::shared_ptr<IocpObject> iocpObject)
{
	iocpObject->SetSessionId(objectId);
	HANDLE result = CreateIoCompletionPort(iocpObject->GetHandle(), _iocpHandle, objectId++, 0);
	if (result == NULL) {
		if (INVALID_HANDLE_VALUE != iocpObject->GetHandle()) {
			LOG_ERR("IOCP registration failed for session[%d]", objectId - 1);
			return false;
		}
	}

	return true;
}

bool IocpCore::Dispatch(unsigned int timeoutMs)
{
	/*DWORD ioSize{ 0 };
	ULONG_PTR key{ 0 };
	ExpOver* expOver{ nullptr };

	BOOL result = GetQueuedCompletionStatus(_iocpHandle, &ioSize, &key, reinterpret_cast<LPOVERLAPPED*>(&expOver), timeoutMs);
	if ((not result) and (WAIT_TIMEOUT == WSAGetLastError())) {
		LOG_DBG("Dispatch timeout");
		return true;
	}

	if (nullptr == expOver) {
		LOG_WRN("Dispatch received shutdown signal");
		SetLastError(ERROR_OPERATION_ABORTED);
		return false;
	}

	std::shared_ptr<IocpObject> iocpObject = expOver->_owner;
	if (nullptr == iocpObject) {
		LOG_ERR("ExpOver has no owner");
		delete expOver;
		return false;
	}

	LOG_DBG("Dispatch success: key=%11u", (unsigned long long)key);
	iocpObject->Dispatch(expOver, ioSize);

	return true;*/

	const ULONG maxEntry{ 32 };
	std::array<OVERLAPPED_ENTRY, maxEntry> entries;
	ULONG numEntries{ 0 };

	BOOL result = GetQueuedCompletionStatusEx(_iocpHandle, entries.data(), maxEntry, &numEntries, timeoutMs, FALSE);
	if (not result) {
		DWORD error = GetLastError();
		if(error == WAIT_TIMEOUT) {
			LOG_DBG("Dispatch timeout");
			return true;
		}

		else {
			LOG_ERR("GQCSEx Failed : %u", error);
			return false;
		}
	}

	for (ULONG i = 0; i < numEntries; ++i) {
		ExpOver* expOver = reinterpret_cast<ExpOver*>(entries[i].lpOverlapped);
		ULONG_PTR key = entries[i].lpCompletionKey;
		DWORD ioSize = entries[i].dwNumberOfBytesTransferred;

		if (nullptr == expOver) {
			LOG_WRN("Dispatch received shutdown signal");
			SetLastError(ERROR_OPERATION_ABORTED);
			return false;
		}

		std::shared_ptr<IocpObject> iocpObject = expOver->_owner;
		if (nullptr == iocpObject) {
			LOG_ERR("ExpOver has no owner");
			delete expOver;
			continue;
		}

		LOG_DBG("Dispatch success: key=%11u", (unsigned long long)key);
		iocpObject->Dispatch(expOver, ioSize);
	}

	return true;
}