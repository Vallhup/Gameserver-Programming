#pragma once

template<typename T>
class AtomicQueue
{
	struct Node;

	struct Node {
		Node() = default;
		Node(const T& data) : _data(std::make_shared<T>(data)) {}
		Node(T&& data) : _data(std::make_shared<T>(std::move(data))) {}

		std::shared_ptr<T> _data;
		std::atomic<Node*> _next{ nullptr };
	};

public:
	AtomicQueue()
	{
		Node* dummyNode = new Node();
		_head.store(dummyNode);
		_tail = dummyNode;
	}

	~AtomicQueue()
	{
		while (_tail) {
			Node* next = _tail->_next.load();
			delete _tail;
			_tail = next; 
		}
	}

	void Push(const T& data)
	{
		Node* node = new Node(data);
		Node* prevHead = _head.exchange(node, std::memory_order_acq_rel);
		prevHead->_next.store(node, std::memory_order_release);
	}

	bool tryPop(std::shared_ptr<T>& result)
	{
		Node* currTail = _tail;
		Node* next = _tail->_next.load(std::memory_order_acquire);
		if (next == nullptr) {
			return false;
		}

		result = next->_data;
		_tail = next;
		delete currTail;

		return true;
	}

private:
	std::atomic<Node*> _head;
	Node* _tail;
};

