#include "pch.h"
#include "RecvBuffer.h"

RecvBuffer::RecvBuffer(int bufferSize) : _readPos(0), _writePos(0)
{
	_buffer.resize(bufferSize);
}

int RecvBuffer::GetUsedSize() const
{
	if (_writePos >= _readPos) {
		return _writePos - _readPos;
	}

	else {
		return static_cast<int>(_buffer.size()) - _readPos + _writePos;
	}
}

int RecvBuffer::GetFreeSize() const
{
	return static_cast<int>(_buffer.size()) - GetUsedSize() - 1;
}

int RecvBuffer::GetContiguousFreeSize() const
{
	if (_writePos >= _readPos) {
		return static_cast<int>(_buffer.size()) - _writePos;
	}

	else {
		return _readPos - _writePos - 1;
	}
}

bool RecvBuffer::Write(const char* data, int dataSize)
{
	// 1. Buffer에 빈 공간이 있는지 확인
	//  - 있다면 Write 진행
	//  - 없다면 false를 return하고 나중에 Write진행
	if (dataSize <= 0) return false;
	if (dataSize > GetFreeSize()) {
		LOG_WRN("RecvBuffer::Write overflow (size: %d, free: %d)", dataSize, GetFreeSize());
		return false;
	}

	// 2. Write 진행
	//  - data를 _buffer에 복사하고
	//  - writePos를 dataSize만큼 이동

	// 유의할 점
	// dataSize가 _capacity - _writePos 보다 클 때
	// _capacity - _writePos만큼만 복사하고
	// 남은 Data를 _buffer의 첫 부분에 복사해야 함
	
	if (data != nullptr) {
		int	sizeToEnd = static_cast<int>(_buffer.size()) - _writePos;
		int firstCopySize = std::min<int>(dataSize, sizeToEnd);
		int secondCopySize = dataSize - firstCopySize;

		std::memcpy(&_buffer[_writePos], data, firstCopySize);
		std::memcpy(&_buffer[0], data + firstCopySize, secondCopySize);
	}

	_writePos = (_writePos + dataSize) % static_cast<int>(_buffer.size());

	return true;
}

bool RecvBuffer::Read(char* readBuffer, int readSize)
{
	// 1. 읽을 Size만큼 Data가 들어가 있는지 확인
	//  - 있다면 Read 진행
	//  - 없다면 false를 return하고 나중에 Read 진행
	if (readSize <= 0) return false;
	if (readSize > GetUsedSize()) {
		LOG_WRN("RecvBuffer::Read underflow (size: %d, used: %d)", readSize, GetUsedSize());
		return false;
	}

	// 2. Read 진행
	//  - data를 readBuffer에 복사하고
	//  - readPos를 readSize만큼 이동

	// 유의할 점
	// dataSize가 _capacity - _readPos 보다 클 때
	// _capacity - _readPos만큼만 복사하고
	// 남은 Size만큼 _buffer의 첫 부분에서 복사해야 됨
	
	int sizeToEnd = static_cast<int>(_buffer.size()) - _readPos;
	int firstCopySize = std::min<int>(readSize, sizeToEnd);
	int secondCopySize = readSize - firstCopySize;

	std::memcpy(readBuffer, &_buffer[_readPos], firstCopySize);
	std::memcpy(readBuffer + firstCopySize, &_buffer[0], secondCopySize);

	_readPos = (_readPos + readSize) % static_cast<int>(_buffer.size());

	return true;
}

