#pragma once

constexpr short BUFFER_SIZE = 4096;

class RecvBuffer
{
public:
	RecvBuffer(int bufferSize = BUFFER_SIZE);
	~RecvBuffer() = default;

public:
	char* GetBuffer()   { return _buffer.data(); }
	char* GetWritePos() { return &_buffer[_writePos]; }
	int   GetUsedSize() const; 
	int   GetFreeSize() const;
	int	  GetContiguousFreeSize() const;

public:
	bool Read(char* readBuffer, int readSize);
	bool Write(const char* data, int dataSize);
	
private:
	std::vector<char> _buffer;
	int		_readPos{ 0 };
	int		_writePos{ 0 };
};

