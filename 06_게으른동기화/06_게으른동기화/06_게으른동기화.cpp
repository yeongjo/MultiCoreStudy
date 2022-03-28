//CPU: AMD Ryzen 3 3300X 4Core 3.79Ghz
//
//mutex 사용한 성긴동기화
//1 threads exec_time = 1195ms
//2 threads exec_time = 2206ms
//4 threads exec_time = 4871ms
//8 threads exec_time = 11877ms

//락 없는 싱글스레드
//1 threads exec_time = 1178ms
//2 threads crash!!

//stl set+lock 성긴동기화
//리스트 정렬대신 set을 사용하는게 더 빠르다
//1 threads exec_time = 657ms
//2 threads exec_time = 1275ms
//4 threads exec_time = 2695ms
//8 threads exec_time = 6526ms

//세밀한 동기화 리스트 엄청 느리다.
//1 threads exec_time = 13481ms
//2 threads exec_time = 10846ms
//4 threads exec_time = 7835ms
//8 threads exec_time = 6190ms

//낙천적 동기화
//1 threads exec_time = 4221ms
//2 threads exec_time = 2390ms
//4 threads exec_time = 1583ms
//8 threads exec_time = 1071ms

//게으른 동기화
//탐색에서 락을 하지않게 removed 키워드를 만들고 락을 최소한으로 한다
//1 threads exec_time = 1993ms
//2 threads exec_time = 1033ms
//4 threads exec_time = 592ms
//8 threads exec_time = 426ms

// 아직까진 블로킹
// 다른 스레드가 작업을 할 수 있게 락 범위를 좁히는데 중점을 둠

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int NUM_TEST = 4'000'000;
//const int NUM_TEST = 40'000;
const int RANGE = 1'000;

class nullmutex {
public:
	void lock() {}
	void unlock() {}
};


class NODE {
public:
	int value;
	NODE* next;
	mutex nlock;
	volatile bool removed;

	NODE() : next(nullptr) {

	}
	NODE(int x) : value(x), next(nullptr) {

	}
	~NODE() {
	}
};

class FLIST {
public:
	NODE head, tail;
	mutex llock;
	FLIST() {
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}

	~FLIST() {
		Init();
	}

	void Init() {
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}

	bool Add(int x) {
		NODE* pred, * curr;
		head.nlock.lock();
		pred = &head;
		curr = pred->next;
		curr->nlock.lock();
		while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
			pred->nlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->nlock.lock();
		}
		if (curr->value == x) {
			curr->nlock.unlock();
			pred->nlock.unlock();
			return false;
		} else {
			NODE* new_node = new NODE(x);
			pred->next = new_node;
			new_node->next = curr;
			curr->nlock.unlock();
			pred->nlock.unlock();
			return true;
		}
	}

	bool Remove(int x) {
		NODE* pred, * curr;
		head.nlock.lock();
		pred = &head;
		curr = pred->next;
		curr->nlock.lock();
		while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
			pred->nlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->nlock.lock();
		}
		if (curr->value != x) {
			curr->nlock.unlock();
			pred->nlock.unlock();
			return false;
		} else {
			pred->next = curr->next;
			curr->nlock.unlock();
			delete curr;
			pred->nlock.unlock();
			return true;
		}
	}

	bool Contains(int x) {
		NODE* pred, * curr;
		head.nlock.lock();
		pred = &head;
		curr = head.next;
		curr->nlock.lock();
		while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
			pred->nlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->nlock.lock();
		}
		if (curr->value != x) {
			pred->nlock.unlock();
			curr->nlock.unlock();
			return false;
		} else {
			pred->nlock.unlock();
			curr->nlock.unlock();
			return true;
		}
	}

	void Print20() {
		NODE* p = head.next;
		for (int i = 0; i < 20; i++) {
			if (p == &tail) {
				break;
			}
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

class OLIST {
public:
	NODE head, tail;
	mutex llock;
	OLIST() {
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}

	~OLIST() {
		Init();
	}

	void Init() {
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}

	bool validate(NODE* pred, NODE* curr) {
		auto node = &head;
		while (node->value <= pred->value) {
			if (node->value == pred->value) {
				return pred->next == curr;
			}
			node = node->next;
		}
		return false;
	}

	bool Add(int x) {
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (validate(pred, curr)) {
				if (curr->value == x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				} else {
					NODE* new_node = new NODE(x);
					new_node->next = curr;
					pred->next = new_node;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			} else {
				curr->nlock.unlock();
				pred->nlock.unlock();
			}
		}
	}

	bool Remove(int x) {
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (validate(pred, curr)) {
				if (curr->value != x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				} else {
					pred->next = curr->next;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			} else {
				curr->nlock.unlock();
				pred->nlock.unlock();
			}
		}
	}

	bool Contains(int x) {
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = head.next;
			while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (validate(pred, curr)) {
				pred->nlock.unlock();
				auto value = curr->value;
				curr->nlock.unlock();
				return value == x;
			} else {
				curr->nlock.unlock();
				pred->nlock.unlock();
			}
		}
	}

	void Print20() {
		NODE* p = head.next;
		for (int i = 0; i < 20; i++) {
			if (p == &tail) {
				break;
			}
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

class ZLIST {
public:
	NODE head, tail;
	mutex llock;
	ZLIST() {
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}

	~ZLIST() {
		Init();
	}

	void Init() {
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}

	bool validate(NODE* pred, NODE* curr) {
		return (false == pred->removed) &&
			(false == curr->removed) &&
			(pred->next == curr);
	}

	bool Add(int x) {
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (validate(pred, curr)) {
				if (curr->value == x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				} else {
					NODE* new_node = new NODE(x);
					new_node->next = curr;
					pred->next = new_node;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			} else {
				curr->nlock.unlock();
				pred->nlock.unlock();
			}
		}
	}

	bool Remove(int x) {
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (validate(pred, curr)) {
				if (curr->value != x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				} else {
					curr->removed = true;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = curr->next;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			} else {
				curr->nlock.unlock();
				pred->nlock.unlock();
			}
		}
	}

	bool Contains(int x) {
		NODE* pred, * curr;
		while (true) {
			curr = head.next;
			while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
				curr = curr->next;
			}
			return curr->value == x && !curr->removed;
		}
	}

	void Print20() {
		NODE* p = head.next;
		for (int i = 0; i < 20; i++) {
			if (p == &tail) {
				break;
			}
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

ZLIST clist;

void Benchmark(int num_threads) {
	for (int i = 0; i < NUM_TEST / num_threads; i++) {
		int x = rand() % RANGE;
		switch (rand() % 3) {
		case 0:
			clist.Add(x);
			break;
		case 1:
			clist.Remove(x);
			break;
		case 2:
			clist.Contains(x);
			break;
		}
	}
}

int main() {
	cout << "start" << endl;
	for (int i = 1; i <= 8; i *= 2) {
		vector<thread> workers;
		clist.Init();

		auto start_t = system_clock::now();
		for (int j = 0; j < i; j++) {
			workers.emplace_back(Benchmark, i);
		}
		for (auto& th : workers) {
			th.join();
		}
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;
		clist.Print20();

		cout << i << " threads ";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count();
		cout << "ms\n";
	}
}
