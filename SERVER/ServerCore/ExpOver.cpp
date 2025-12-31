#include "pch.h"
#include "ExpOver.h"

ExpOver::ExpOver(OperationType operationType) : _operationType(operationType)
{
	Init();
}

void ExpOver::Init(const std::shared_ptr<IocpObject>& owner)
{
	OVERLAPPED::hEvent = 0;
	OVERLAPPED::Internal = 0;
	OVERLAPPED::InternalHigh = 0;
	OVERLAPPED::Offset = 0;
	OVERLAPPED::OffsetHigh = 0;

	_owner = owner;
}

AcceptOver::AcceptOver() : ExpOver(Accept)
{
	memset(_buffer, 0, sizeof(_buffer));
}

RecvOver::RecvOver() : ExpOver(Recv)
{
	_wsaBuf[0] = { 0, nullptr };
	_wsaBuf[1] = { 0, nullptr };
}

int RecvOver::SetBuffers()
{
	int totalFree = _buffer.GetFreeSize();
	int contiguousFree = _buffer.GetContiguousFreeSize();
	int firstRecvSize = std::min<int>(contiguousFree, totalFree);

	_wsaBuf[0].buf = _buffer.GetWritePos();
	_wsaBuf[0].len = static_cast<ULONG>(firstRecvSize);

	int secondRecvSize = totalFree - firstRecvSize;
	if (secondRecvSize > 0) {
		_wsaBuf[1].buf = _buffer.GetBuffer();
		_wsaBuf[1].len = static_cast<ULONG>(secondRecvSize);

		return 2;
	}

	return 1;
}

SendOver::SendOver() : ExpOver(Send)
{
}

void SendOver::SetBuffers(const std::vector<std::shared_ptr<std::vector<char>>>&& buffers)
{
	_wsaBufs.clear();
	_sendDataList.clear();

	_sendDataList = buffers;
	_wsaBufs.resize(buffers.size());

	for (size_t i = 0; i < buffers.size(); ++i) {
		_wsaBufs[i].buf = const_cast<char*>(_sendDataList[i]->data());
		_wsaBufs[i].len = static_cast<ULONG>(_sendDataList[i]->size());
	}
}

EventOver::EventOver(OperationType op, int id) : ExpOver(op), _id(id)
{
}
