// CPU: AMD Ryzen 3 3300X 4Core 3.79Ghz
// 
// mutex 사용한 성긴동기화
// 1 threads exec_time = 1195ms
// 2 threads exec_time = 2206ms
// 4 threads exec_time = 4871ms
// 8 threads exec_time = 11877ms

// 싱글스레드동작 nullmutex 사용한 성긴동기화
// 1 threads exec_time = 1178ms
// 2 threads crash!!

// stl set+lock 성긴동기화
// 리스트 정렬대신 set을 사용하는게 더 빠르다
// 1 threads exec_time = 657ms
// 2 threads exec_time = 1275ms
// 4 threads exec_time = 2695ms
// 8 threads exec_time = 6526ms

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
//1 threads exec_time = 1993ms
//2 threads exec_time = 1033ms
//4 threads exec_time = 592ms
//8 threads exec_time = 426ms

//게으른 동기화 shared_ptr
//사용할수가 없다, 너무느리다
//1 threads exec_time = 14581ms
// Crash!!!

//논블럭킹 동기화
//1 threads exec_time = 1526ms
//2 threads exec_time = 769ms
//4 threads exec_time = 481ms
//8 threads exec_time = 299ms

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
const long INVERT_ONE = ~0x01;

class nullmutex {
public:
	void lock() {}
	void unlock() {}
};


class NODE {
public:
	int value;
	NODE *next;
	mutex nlock;
	volatile bool removed;

	NODE() : next(nullptr) {

	}
	NODE(int x) : value(x), next(nullptr) {

	}
	~NODE() {
	}
};

class SPNODE {
public:
	int value;
	shared_ptr<SPNODE> next;
	mutex nlock;
	volatile bool removed;

	SPNODE() : next(nullptr), removed(false) {

	}
	SPNODE(int x) : value(x), next(nullptr), removed(false) {

	}
	~SPNODE() {
	}
};

class LFNODE;

//마킹 포인터 자료구조
class MPTR {
	int64_t ptr;
public:
	bool CAS(int64_t old_v, int64_t new_v) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_llong*>(&ptr), &old_v, new_v);
	}

	bool CAS(LFNODE *old_node, LFNODE *new_node,
		bool old_removed, bool new_removed) {
		auto old_value = reinterpret_cast<int64_t>(old_node);
		if (old_removed) old_value = old_value | 0x01;
		else old_value = old_value & INVERT_ONE;

		auto new_value = reinterpret_cast<int64_t>(new_node);
		if (new_removed) new_value = new_value | 0x01;
		else new_value = new_value & INVERT_ONE;

		return CAS(old_value, new_value);
	}

	bool AttemptMark(LFNODE *old_node, bool newMark) {
		auto old_value = reinterpret_cast<int64_t>(old_node);
		old_value = old_value & INVERT_ONE;
		auto new_value = old_value;

		if (newMark) new_value = new_value | 0x01;

		return CAS(old_value, new_value);
	}

	LFNODE *getptr() {
		return reinterpret_cast<LFNODE *>(ptr & INVERT_ONE);
	}

	LFNODE *getptr(bool *removed) {
		auto temp = ptr;
		if (0 == (temp & 0x1)) {
			*removed = false;
		} else {
			*removed = true;
		}
		return getptr();
	}

	void set(LFNODE *node, bool removed) {
		ptr = reinterpret_cast<int64_t>(node);
		if (removed) {
			ptr = ptr | 0x01;
		} else {
			ptr = ptr & INVERT_ONE;
		}
	}
};

class LFNODE {
public:
	int value;
	MPTR next;

	LFNODE() {
		next.set(nullptr, false);
	}
	LFNODE(int x) : value(x) {
		next.set(nullptr, false);
	}
	~LFNODE() {
	}
};

class LFLIST {
public:
	LFNODE head, tail;
	LFNODE *freelist;
	LFNODE freetail;
	mutex llock;
	LFLIST() {
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next.set(&tail, false);
		freetail.value = 0x7fffffff;
		freelist = &freetail;
	}

	~LFLIST() {
		Init();
	}

	void Init() {
		LFNODE *p;
		while (head.next.getptr() != &tail) {
			p = head.next.getptr();
			head.next = p->next.getptr()->next;
			delete p;
		}
	}

	void find(int value, LFNODE *(&pred), LFNODE *(&curr)) {
		LFNODE *succ;
		bool removed;
		bool snip;
	retry:
		pred = &head;
		curr = pred->next.getptr();
		while (true) {
			succ = curr->next.getptr(&removed);
			while (removed) {
				snip = pred->next.CAS(curr, succ, false, false);
				if (!snip) goto retry;
				curr = succ;
				succ = curr->next.getptr(&removed);
			}
			if (curr->value >= value) {
				return;
			}
			pred = curr;
			curr = succ;
		}
	}

	bool Add(int x) {
		LFNODE *pred, *curr;
		while(true){
			find(x, pred, curr);
			if(x == curr->value){
				return false;
			}else{
				LFNODE *node = new LFNODE(x);
				node->next.set(curr, false);

				if(pred->next.CAS(curr, node, false, false)){
					return true;
				}
			}
		}
	}

	bool Remove(int x) {
		LFNODE *pred, *curr;
		bool snip;
		while(true){
			find(x, pred, curr);
			if(x != curr->value){
				return false;
			}else{
				LFNODE *succ = curr->next.getptr();
				snip = curr->next.AttemptMark(succ, true);
				if(!snip){
					continue;
				}
				pred->next.CAS(curr, succ, false, false);
				return true;
			}
		}
	}

	bool Contains(int x) {
		LFNODE *pred, *curr;
		bool removed = false;
		curr = &head;
		while(curr->value < x){
			curr = curr->next.getptr();
			LFNODE *succ = curr->next.getptr(&removed);
		}
		return curr->value == x && !removed;
	}

	void Print20() {
		LFNODE *p = head.next.getptr();
		for (int i = 0; i < 20; i++) {
			if (p == &tail) {
				break;
			}
			cout << p->value << ", ";
			p = p->next.getptr();
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
			NODE *p = head.next;
			head.next = p->next;
			delete p;
		}
	}

	bool validate(NODE *pred, NODE *curr) {
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
		NODE *pred, *curr;
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
					NODE *new_node = new NODE(x);
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
		NODE *pred, *curr;
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
		NODE *pred, *curr;
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
		NODE *p = head.next;
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
			NODE *p = head.next;
			head.next = p->next;
			delete p;
		}
	}

	bool validate(NODE *pred, NODE *curr) {
		return (false == pred->removed) &&
			(false == curr->removed) &&
			(pred->next == curr);
	}

	bool Add(int x) {
		NODE *pred, *curr;
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
					NODE *new_node = new NODE(x);
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
		NODE *pred, *curr;
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
		NODE *pred, *curr;
		while (true) {
			curr = head.next;
			while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
				curr = curr->next;
			}
			return curr->value == x && !curr->removed;
		}
	}

	void Print20() {
		NODE *p = head.next;
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

class SPZLIST {
public:
	shared_ptr<SPNODE> head, tail;
	mutex llock;
	SPZLIST() {
		head = make_shared<SPNODE>();
		tail = make_shared <SPNODE>();
		head->value = 0x80000000;
		tail->value = 0x7FFFFFFF;
		head->next = tail;
	}

	~SPZLIST() {
		Init();
	}

	void Init() {
		head->next = tail;
	}

	bool validate(const shared_ptr<SPNODE> &pred, const shared_ptr<SPNODE> &curr) {
		return (false == pred->removed) &&
			(false == curr->removed) &&
			(pred->next == curr);
	}

	bool Add(int x) {
		shared_ptr<SPNODE> pred, curr;
		while (true) {
			pred = head;
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
					auto new_node = make_shared<SPNODE>(x);
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
		shared_ptr<SPNODE> pred, curr;
		while (true) {
			pred = head;
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
		shared_ptr<SPNODE> pred, curr;
		while (true) {
			curr = head->next;
			while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
				curr = curr->next;
			}
			return curr->value == x && !curr->removed;
		}
	}

	void Print20() {
		auto p = head->next;
		for (int i = 0; i < 20; i++) {
			if (p == tail) {
				break;
			}
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

LFLIST clist;

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
		for (auto &th : workers) {
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
