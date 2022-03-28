//CPU: AMD Ryzen 3 3300X 4Core 3.79Ghz
//
//const int NUM_TEST = 4'000'000;
// 싱글코어 프로그램
// 1 threads    exec_time = 745ms
//
// 성긴 동기화
// 1 threads    exec_time = 878ms
// 2 threads    exec_time = 891ms
// 4 threads    exec_time = 987ms
// 8 threads    exec_time = 1126ms
//
//게으른 동기화
//1 threads    exec_time = 908ms
//2 threads    exec_time = 504ms
//4 threads    exec_time = 276ms
//8 threads    exec_time = 183ms
//
//락프리
//1 threads    exec_time = 1472ms
//2 threads    exec_time = 706ms
//4 threads    exec_time = 429ms
//8 threads    exec_time = 299ms

//리스트 성긴동기화 60% Contain
//1 threads    exec_time = 1914ms
//2 threads    exec_time = 1626ms
//4 threads    exec_time = 1775ms
//8 threads    exec_time = 2151ms
//RW_LOCK
//1 threads    exec_time = 2016ms
//2 threads    exec_time = 1570ms
//4 threads    exec_time = 1714ms
//8 threads    exec_time = 1945ms
//RW_LOCK 60% Contain
//1 threads    exec_time = 2077ms
//2 threads    exec_time = 1511ms
//4 threads    exec_time = 1774ms
//8 threads    exec_time = 1977ms
//RW_LOCK 80% Contain
//1 threads    exec_time = 1958ms
//2 threads    exec_time = 1517ms
//4 threads    exec_time = 1739ms
//8 threads    exec_time = 1975ms
//RW_LOCK 90% Contain
//1 threads    exec_time = 584ms
//2 threads    exec_time = 420ms
//4 threads    exec_time = 490ms
//8 threads    exec_time = 463ms

//const int NUM_TEST = 10'000'000;
//1 threads    exec_time = 2081ms
//2 threads    exec_time = 1602ms
//4 threads    exec_time = 1768ms
//8 threads    exec_time = 1962ms
//
//const int NUM_TEST = 20'000'000;
//1 threads    exec_time = 3419ms
//2 threads    exec_time = 2907ms
//4 threads    exec_time = 3331ms
//8 threads    exec_time = 3988ms
//lock_shared 98%까지 써야 성능향상이 보였다.
//고속 rw락을 만들어서 쓰는 경우가 많다.
//윈도우 rw_lock도 존재하지만 성능이 안좋다

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <set>
#include <shared_mutex>

using namespace std;
using namespace chrono;

const int NUM_TEST = 20'000'000;
const int RANGE = 1000;

class NODE {
public:
	int value;
	NODE* next;
	mutex nlock;
	volatile bool removed;
	NODE() : next(nullptr), removed(false) {}
	NODE(int x) : value(x), next(nullptr), removed(false) {}
	~NODE() {}
};

class SPNODE {
public:
	int value;
	shared_ptr <SPNODE> next;
	mutex nlock;
	volatile bool removed;
	SPNODE() : next(nullptr), removed(false) {}
	SPNODE(int x) : value(x), next(nullptr), removed(false) {}
	~SPNODE() {}
};

class LFNODE;

class MPTR {
	atomic_int V;
public:
	MPTR() : V(0) {}
	~MPTR() {};
	void set_ptr(LFNODE* p)
	{
		V = reinterpret_cast<int>(p);
	}
	LFNODE* get_ptr()
	{
		return reinterpret_cast<LFNODE *>(V & 0xFFFFFFFE);
	}
	bool get_removed(LFNODE*& p)
	{
		int curr_v = V;
		p = reinterpret_cast<LFNODE *>(curr_v & 0xFFFFFFFE);
		return (curr_v & 0x1) == 1;
	}
	bool CAS(LFNODE* old_p, LFNODE* new_p, bool old_removed, bool new_removed)
	{
		int old_v = reinterpret_cast<int>(old_p);
		if (true == old_removed) old_v++;
		int new_v = reinterpret_cast<int>(new_p);
		if (true == new_removed) new_v++;
		return atomic_compare_exchange_strong(&V, &old_v, new_v);
	}
};

class LFNODE {
public:
	int value;
	MPTR next;
	LFNODE() {}
	LFNODE(int x) : value(x) {}
	~LFNODE() {}
};

class nullmutex
{
public:
	void lock() {}
	void unlock() {}
};

class CLIST
{
	NODE head, tail;
	mutex llock;
public:
	CLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~CLIST() {
		Init();
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}
	bool Add(int x)
	{
		NODE* pred, * curr;
		pred = &head;
		llock.lock();
		curr = pred->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}
		if (curr->value == x) {
			llock.unlock();
			return false;
		}
		else {
			NODE* new_node = new NODE(x);
			pred->next = new_node;
			new_node->next = curr;
			llock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		pred = &head;
		llock.lock();
		curr = pred->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}
		if (curr->value != x) {
			llock.unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			delete curr;
			llock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE* curr;
		llock.lock();
		curr = head.next;
		while (curr->value < x) {
			curr = curr->next;
		}
		if (curr->value != x) {
			llock.unlock();
			return false;
		}
		else {
			llock.unlock();
			return true;
		}
	}
	void Print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

class CRWLIST
{
	NODE head, tail;
	shared_mutex llock;
public:
	CRWLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~CRWLIST() {
		Init();
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}
	bool Add(int x)
	{
		NODE* pred, * curr;
		pred = &head;
		llock.lock();
		curr = pred->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}
		if (curr->value == x) {
			llock.unlock();
			return false;
		}
		else {
			NODE* new_node = new NODE(x);
			pred->next = new_node;
			new_node->next = curr;
			llock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		pred = &head;
		llock.lock();
		curr = pred->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}
		if (curr->value != x) {
			llock.unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			delete curr;
			llock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE* curr;
		llock.lock_shared();
		curr = head.next;
		while (curr->value < x) {
			curr = curr->next;
		}
		if (curr->value != x) {
			llock.unlock_shared();
			return false;
		}
		else {
			llock.unlock_shared();
			return true;
		}
	}
	void Print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

class FLIST
{
	NODE head, tail;
public:
	FLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~FLIST() {
		Init();
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}
	bool Add(int x)
	{
		NODE* pred, * curr;
		head.nlock.lock();
		pred = &head;
		curr = pred->next;
		curr->nlock.lock();
		while (curr->value < x) {
			pred->nlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->nlock.lock();
		}
		if (curr->value == x) {
			curr->nlock.unlock();
			pred->nlock.unlock();
			return false;
		}
		else {
			NODE* new_node = new NODE(x);
			new_node->next = curr;
			pred->next = new_node;
			curr->nlock.unlock();
			pred->nlock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		head.nlock.lock();
		pred = &head;
		curr = pred->next;
		curr->nlock.lock();
		while (curr->value < x) {
			pred->nlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->nlock.lock();
		}
		if (curr->value != x) {
			curr->nlock.unlock();
			pred->nlock.unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			curr->nlock.unlock();
			pred->nlock.unlock();
			delete curr;
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE* pred, * curr;
		head.nlock.lock();
		pred = &head;
		curr = pred->next;
		curr->nlock.lock();
		while (curr->value < x) {
			pred->nlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->nlock.lock();
		}
		if (curr->value != x) {
			curr->nlock.unlock();
			pred->nlock.unlock();
			return false;
		}
		else {
			curr->nlock.unlock();
			pred->nlock.unlock();
			return true;
		}
	}
	void Print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

class OLIST
{
	NODE head, tail;
public:
	OLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~OLIST() {
		Init();
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}
	bool validate(NODE* pred, NODE* curr)
	{
		NODE* node = &head;
		while (node->value <= pred->value) {
			if (node == pred)
				return pred->next == curr;
			node = node->next;
		}
		return false;
	}
	bool Add(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (true == validate(pred, curr)) {
				if (curr->value == x) {
					curr->nlock.unlock();
						pred->nlock.unlock();
						return false;
				}
				else {
					NODE* new_node = new NODE(x);
						new_node->next = curr;
						pred->next = new_node;
						curr->nlock.unlock();
						pred->nlock.unlock();
					return true;
				}
			}
			else {
				pred->nlock.unlock();
				curr->nlock.unlock();
			}
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (true == validate(pred, curr)) {
				if (curr->value != x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				}
				else {
					pred->next = curr->next;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			}
			else {
				pred->nlock.unlock();
				curr->nlock.unlock();
			}
		}
	}
	bool Contains(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (true == validate(pred, curr)) {
				if (curr->value == x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
				else {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				}
			}
			else {
				pred->nlock.unlock();
				curr->nlock.unlock();
			}
		}
	}
	void Print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

class ZLIST
{
	NODE head, tail;
public:
	ZLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}
	~ZLIST() {
		Init();
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE* p = head.next;
			head.next = p->next;
			delete p;
		}
	}
	bool validate(NODE* pred, NODE* curr)
	{
		return (false == pred->removed)
			&& (false == curr->removed)
			&& (pred->next == curr);
	}
	bool Add(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (true == validate(pred, curr)) {
				if (curr->value == x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				}
				else {
					NODE* new_node = new NODE(x);
					new_node->next = curr;
					pred->next = new_node;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			}
			else {
				pred->nlock.unlock();
				curr->nlock.unlock();
			}
		}
	}
	bool Remove(int x)
	{
		NODE* pred, * curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (true == validate(pred, curr)) {
				if (curr->value != x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				}
				else {
					curr->removed = true;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = curr->next;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			}
			else {
				pred->nlock.unlock();
				curr->nlock.unlock();
			}
		}
	}
	bool Contains(int x)
	{
		NODE* curr = head.next;
		while (curr->value < x) {
				curr = curr->next;
		}
		return (curr->value == x) && (curr->removed == false);
	}
	void Print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

class SPZLIST
{
	shared_ptr <SPNODE> head, tail;
public:
	SPZLIST()
	{
		head = make_shared<SPNODE>(0x80000000);
		tail = make_shared<SPNODE>(0x7FFFFFFF);
		head->next = tail;
	}
	~SPZLIST() {
		Init();
	}
	void Init()
	{
		head->next = tail;
	}
	bool validate(const shared_ptr<SPNODE> &pred, const shared_ptr<SPNODE> &curr)
	{
		return (false == pred->removed)
			&& (false == curr->removed)
			&& (pred->next == curr);
	}
	bool Add(int x)
	{
		shared_ptr <SPNODE> pred, curr;
		while (true) {
			pred = head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (true == validate(pred, curr)) {
				if (curr->value == x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				}
				else {
					shared_ptr<SPNODE> new_node = make_shared<SPNODE>(x);
					new_node->next = curr;
					pred->next = new_node;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			}
			else {
				pred->nlock.unlock();
				curr->nlock.unlock();
			}
		}
	}
	bool Remove(int x)
	{
		shared_ptr <SPNODE> pred, curr;
		while (true) {
			pred = head;
			curr = pred->next;
			while (curr->value < x) {
				pred = curr;
				curr = curr->next;
			}
			pred->nlock.lock();
			curr->nlock.lock();
			if (true == validate(pred, curr)) {
				if (curr->value != x) {
					curr->nlock.unlock();
					pred->nlock.unlock();
					return false;
				}
				else {
					curr->removed = true;
					atomic_thread_fence(memory_order_seq_cst);
					pred->next = curr->next;
					curr->nlock.unlock();
					pred->nlock.unlock();
					return true;
				}
			}
			else {
				pred->nlock.unlock();
				curr->nlock.unlock();
			}
		}
	}
	bool Contains(int x)
	{
		shared_ptr<SPNODE> curr = head->next;
		while (curr->value < x) {
			curr = curr->next;
		}
		return (curr->value == x) && (curr->removed == false);
	}
	void Print20()
	{
		shared_ptr <SPNODE> p = head->next;
		for (int i = 0; i < 20; ++i) {
			if (p == tail)
				break;
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

class LFLIST
{
	LFNODE head, tail;
public:
	LFLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next.set_ptr(&tail);
	}
	~LFLIST() {
		Init();
	}
	void Init()
	{
		while (head.next.get_ptr() != &tail) {
			LFNODE* p = head.next.get_ptr();
			head.next.set_ptr(p->next.get_ptr());
			delete p;
		}
	}

	void Find(int x, LFNODE*& pred, LFNODE*& curr)
	{
		while (true) {
			retry:
			pred = &head;
			curr = pred->next.get_ptr();
			while (true) {
				LFNODE* succ;
				bool removed = curr->next.get_removed(succ);
				while (true == removed) {
					if (false == pred->next.CAS(curr, succ, false, false))
						goto retry;
					removed = curr->next.get_removed(succ);
				}
				if (curr->value >= x)
					return;
				pred = curr;
				curr = curr->next.get_ptr();
			}
		}
	}

	bool Add(int x)
	{
		LFNODE* pred, * curr;
		while (true) {
			Find(x, pred, curr);
			if (curr->value == x) {
				return false;
			}
			else {
				LFNODE* new_node = new LFNODE(x);
				new_node->next.set_ptr(curr);
				if (true == pred->next.CAS(curr, new_node, false, false))
					return true;
				else delete new_node;
			}
		}
	}
	bool Remove(int x)
	{
		LFNODE* pred, * curr;
		while (true) {
			Find(x, pred, curr);
			if (curr->value != x) {
				return false;
			}
			else {
				LFNODE* succ = curr->next.get_ptr();
				if (false == curr->next.CAS(succ, succ, false, true))
					continue;
				pred->next.CAS(curr, succ, false, false);
				return true;
			}
		}
	}

	bool Contains(int x)
	{
		LFNODE* curr = head.next.get_ptr();
		while (curr->value < x) {
			curr = curr->next.get_ptr();
		}
		LFNODE* temp;
		return (curr->value == x) && (curr->next.get_removed(temp) == false);
	}
	void Print20()
	{
		LFNODE* p = head.next.get_ptr();
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next.get_ptr();
		}
	}
};

constexpr int MAX_LEVEL = 10;
class SKNODE {
public:
	int value;
	SKNODE* next[MAX_LEVEL + 1];
	int toplevel;
	atomic_bool fully_linked;
	atomic_bool is_removed;
	recursive_mutex node_lock;
	SKNODE() : value(0), toplevel(0), fully_linked(false), is_removed(false)
	{
		for (auto& n : next) n = nullptr;
	}
	SKNODE(int v, int top) : value(v), toplevel(top), fully_linked(false), is_removed(false)
	{
		for (auto& n : next) n = nullptr;
	}
};

class CSKLIST
{
	SKNODE head, tail;
	mutex llock;
public:
	CSKLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		//head.next[1] = &tail.next[1];    X
		//head.next[1] = &tail;            O
		for (auto& n : head.next) n = &tail;
	}
	~CSKLIST() {
		Init();
	}
	void Init()
	{
		while (head.next[0] != &tail) {
			SKNODE* p = head.next[0];
			head.next[0] = p->next[0];
			delete p;
		}
		for (auto& n : head.next) n = &tail;
	}
	void Find(int x, SKNODE* preds[], SKNODE* currs[])
	{
		int curr_level = MAX_LEVEL;
		preds[curr_level] = &head;
		while (true) {
			currs[curr_level] = preds[curr_level]->next[curr_level];
			while (currs[curr_level]->value < x) {
				preds[curr_level] = currs[curr_level];
				currs[curr_level] = currs[curr_level]->next[curr_level];
			}
			if (0 == curr_level)
				return;
			curr_level--;
			preds[curr_level] = preds[curr_level + 1];
		}
	}
	bool Add(int x)
	{
		SKNODE* preds[MAX_LEVEL + 1], * currs[MAX_LEVEL + 1];
		llock.lock();
		Find(x, preds, currs);

		if (currs[0]->value == x) {
			llock.unlock();
			return false;
		}
		else {
			// int new_level = rand() % (MAX_LEVEL + 1);
			int new_level = 0;
			while ((rand() % 2) == 1) {
				new_level++;
				if (MAX_LEVEL == new_level)
					break;
			}
			SKNODE* new_node = new SKNODE(x, new_level);
			for (int i = 0 ; i<= new_level; ++i)
				new_node->next[i] = currs[i];
			for (int i = 0; i <= new_level; ++i)
				preds[i]->next[i] = new_node;
			llock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		SKNODE* preds[MAX_LEVEL + 1], * currs[MAX_LEVEL + 1];
		llock.lock();
		Find(x, preds, currs);

		if (currs[0]->value != x) {
			llock.unlock();
			return false;
		}
		else {
			for (int i = 0; i <= currs[0]->toplevel; ++i)
				preds[i]->next[i] = currs[0]->next[i];
			delete currs[0];
			llock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		SKNODE* preds[MAX_LEVEL + 1], * currs[MAX_LEVEL + 1];
		llock.lock();
		Find(x, preds, currs);

		if (currs[0]->value != x) {
			llock.unlock();
			return false;
		}
		else {
			llock.unlock();
			return true;
		}
	}

	void Print20()
	{
		SKNODE* p = head.next[0];
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next[0];
		}
	}
};

class ZSKLIST
{
	SKNODE head, tail;
public:
	ZSKLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		for (auto& n : head.next) n = &tail;
	}
	~ZSKLIST() {
		Init();
	}
	void Init()
	{
		while (head.next[0] != &tail) {
			SKNODE* p = head.next[0];
			head.next[0] = p->next[0];
			delete p;
		}
		for (auto& n : head.next) n = &tail;
	}
	int Find(int x, SKNODE* preds[], SKNODE* currs[])
	{
		int found_level = -1;
		int curr_level = MAX_LEVEL;
		preds[curr_level] = &head;
		while (true) {
			currs[curr_level] = preds[curr_level]->next[curr_level];
			while (currs[curr_level]->value < x) {
				preds[curr_level] = currs[curr_level];
				currs[curr_level] = currs[curr_level]->next[curr_level];
			}
			if ((-1 == found_level) && (currs[curr_level]->value == x))
				found_level = curr_level;
			if (0 == curr_level)
				return found_level;
			curr_level--;
			preds[curr_level] = preds[curr_level + 1];
		}
	}
	bool Add(int x)
	{
		SKNODE* preds[MAX_LEVEL + 1], * succs[MAX_LEVEL + 1];

		while (true) {
			int f_level = Find(x, preds, succs);

			if (-1 != f_level) {
				if (true == succs[f_level]->is_removed)
					continue;
				while (false == succs[f_level]->fully_linked){}
				return false;
			}

			// 노드가 존재하지 않는다 추가하자.
			// Locking을 하자.
			int new_level = 0;
			while ((rand() % 2) == 1) {
				new_level++;
				if (MAX_LEVEL == new_level)
					break;
			}

			int is_valid = true;
			int max_lock_level = -1;
			for (int i = 0; i <= new_level; ++i) {
				preds[i]->node_lock.lock();
				max_lock_level = i;
				if ((false == preds[i]->is_removed)
					&& (false == succs[i]->is_removed)
					&& (preds[i]->next[i] == succs[i])) {
				}
				else {
					is_valid = false;
					break;
				}
			}
			if (false == is_valid) {
				for (int i = 0; i <= max_lock_level; ++i)
					preds[i]->node_lock.unlock();
				continue;
			}
			SKNODE* new_node = new SKNODE(x, new_level);
			for (int i = 0; i <= new_level; ++i)
				new_node->next[i] = succs[i];
			for (int i = 0; i <= new_level; ++i)
				preds[i]->next[i] = new_node;
			new_node->fully_linked = true;
			for (int i = 0; i <= max_lock_level; ++i)
				preds[i]->node_lock.unlock();
			return true;
		}
	}

	bool Remove(int x)
	{
		SKNODE* preds[MAX_LEVEL + 1], *succs[MAX_LEVEL + 1];
		SKNODE *victim = nullptr;
		bool is_removed = false;
		int toplevel = -1;
		while (true) {
			int f_level = Find(x, preds, succs);
			if (-1 != f_level) victim = succs[f_level];
			if (is_removed || 
				(-1 != f_level &&
				(victim->fully_linked &&
				victim->toplevel == f_level &&
				false == victim->is_removed))){
				if(false == is_removed){
					toplevel = victim->toplevel;
					victim->node_lock.lock();
					if(victim->is_removed){
						victim->node_lock.unlock();
						return false;
					}
					victim->is_removed = true;
					is_removed = true;
				}
				int max_lock_level = -1;
				bool valid = true;
				for (int i = 0; valid && i <= toplevel; i++) {
					SKNODE* pred = preds[i];
					pred->node_lock.lock();
					max_lock_level = i;
					valid = false == pred->is_removed && pred->next[i] == victim;
				}
				if (false == valid){
					for (int i = 0; i <= max_lock_level; ++i)
						preds[i]->node_lock.unlock();
					continue;
				}
				for (int i = toplevel; i >= 0; i--) {
					preds[i]->next[i] = victim->next[i];
				}
				victim->node_lock.unlock();
				for (int i = 0; i <= max_lock_level; ++i)
					preds[i]->node_lock.unlock();
				return true;
			}else{
				return false;
			}
		}
	}
	
	bool Contains(int x)
	{
		SKNODE* preds[MAX_LEVEL + 1], *succs[MAX_LEVEL + 1];
		int f_level = Find(x, preds, succs);
		return (-1 != f_level &&
			succs[f_level]->fully_linked &&
			false == succs[f_level]->is_removed);
	}

	void Print20()
	{
		SKNODE* p = head.next[0];
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next[0];
		}
	}
};

class LFSKNODE;

class SKMPTR {
	atomic_int V;
public:
	SKMPTR() : V(0) {}
	~SKMPTR() {};
	void set_ptr(LFSKNODE* p)
	{
		V = reinterpret_cast<int>(p);
	}
	void set_ptr(LFSKNODE* p, bool removed)
	{
		V = (reinterpret_cast<int>(p) & 0xFFFFFFFE) | (removed & 0x1);
	}
	LFSKNODE* get_ptr()
	{
		return reinterpret_cast<LFSKNODE*>(V & 0xFFFFFFFE);
	}
	bool get_removed(LFSKNODE*& p)
	{
		int curr_v = V;
		p = reinterpret_cast<LFSKNODE*>(curr_v & 0xFFFFFFFE);
		return (curr_v & 0x1) == 1;
	}
	bool CAS(LFSKNODE* old_p, LFSKNODE* new_p, bool old_removed, bool new_removed)
	{
		int old_v = reinterpret_cast<int>(old_p);
		if (true == old_removed) old_v++;
		int new_v = reinterpret_cast<int>(new_p);
		if (true == new_removed) new_v++;
		return atomic_compare_exchange_strong(&V, &old_v, new_v);
	}
};

class LFSKNODE {
public:
	int value;
	SKMPTR next[MAX_LEVEL + 1];
	int toplevel;
	LFSKNODE() : toplevel(0){}
	LFSKNODE(int x, int level) : value(x), toplevel(level) {}
	~LFSKNODE() {}
};

class LFSKLIST
{
	LFSKNODE head, tail;
public:
	LFSKLIST()
	{
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		for (auto& n : head.next) n.set_ptr(&tail);
	}
	~LFSKLIST() {
		Init();
	}
	void Init()
	{
		while (head.next[0].get_ptr() != &tail) {
			LFSKNODE* p = head.next[0].get_ptr();
			head.next[0].set_ptr(p->next[0].get_ptr());
			delete p;
		}
		for (auto& n : head.next) n.set_ptr(&tail);
	}
	bool Find(int x, LFSKNODE* preds[], LFSKNODE* currs[])
	{
	retry:
		int curr_level = MAX_LEVEL;
		preds[curr_level] = &head;
		while (true) {
			while (true) {
				currs[curr_level] = preds[curr_level]->next[curr_level].get_ptr();
				LFSKNODE* succ;
				bool is_removed = currs[curr_level]->next[curr_level].get_removed(succ);
				while (true == is_removed) {
					if (false == preds[curr_level]->next[curr_level].CAS(currs[curr_level],
						succ, false, false)) goto retry;
					currs[curr_level] = succ;
					is_removed = currs[curr_level]->next[curr_level].get_removed(succ);
				}
				if (currs[curr_level]->value >= x) break;
				preds[curr_level] = currs[curr_level];
			}
			if (0 == curr_level) return currs[0]->value == x;
			curr_level--;
			preds[curr_level] = preds[curr_level + 1];
		}
	}

	bool Add(int x)
	{
		LFSKNODE* preds[MAX_LEVEL + 1], * succs[MAX_LEVEL + 1];
		int bottom_level = 0;
		int top_level = rand() & MAX_LEVEL;
		LFSKNODE *new_node = new LFSKNODE(x, top_level);

		while (true) {
			bool found = Find(x, preds, succs);

			if(found){
				delete new_node;
				return false;
			}else{
				for (int i = bottom_level; i <= top_level; ++i) {
					LFSKNODE *succ = succs[i];
					new_node->next[i].set_ptr(succ); // ?????????????????
				}
				LFSKNODE *pred = preds[bottom_level];
				LFSKNODE *succ = succs[bottom_level];
				new_node->next[bottom_level].set_ptr(succ);
				if(false == pred->next[bottom_level].CAS(succ, new_node, false, false)){
					continue;
				}
				for (int i = bottom_level+1; i <= top_level; i++) {
					while(true){
						pred = preds[i];
						succ = succs[i];
						if (pred->next[i].CAS(succ, new_node, false, false)){
							break;
						}
						Find(x, preds, succs);
					}
				}
				return true;
			}
		}
	}

	bool Remove(int x)
	{
		LFSKNODE* preds[MAX_LEVEL + 1], * currs[MAX_LEVEL + 1];
		if (false == Find(x, preds, currs)) return false;
		LFSKNODE* target = currs[0];
		for (int i = target->toplevel; i > 0; --i) {
			LFSKNODE* succ = target->next[i].get_ptr();
			while (true) {
				if (true == target->next[i].CAS(succ, succ, false, true)) break;
				if (true == target->next[i].get_removed(succ)) break;
			}
		}
		while (true) {
			LFSKNODE* succ;	
			bool other_removed = target->next[0].get_removed(succ);
			if (true == other_removed) return false;
			if (true == target->next[0].CAS(succ, succ, false, true)) {
				Find(x, preds, currs);
				return true;
			}
			//Find(x, preds, currs); // 필요없대요
		}
	}

	bool Contains(int x)
	{
		int bottom_level = 0;
		bool marked = false;
		LFSKNODE *pred = &head;
		LFSKNODE *curr = nullptr;
		LFSKNODE *succ = nullptr;
		for(int level = MAX_LEVEL; level >= bottom_level; level--){
			curr = pred->next[level].get_ptr();
			while(true){
				marked = curr->next[level].get_removed(succ);
				while(marked){
					curr = curr->next[level].get_ptr();
					marked = curr->next[level].get_removed(succ);
				}
				if(curr->value < x){
					pred = curr;
					curr = succ;
				}else{
					break;
				}
			}
		}
		return curr->value == x;
	}

	void Print20()
	{
		LFSKNODE* p = head.next[0].get_ptr();
		for (int i = 0; i < 20; ++i) {
			if (p == &tail)
				break;
			cout << p->value << ", ";
			p = p->next[0].get_ptr();
		}
	}
};


CRWLIST clist;
set <int> set_list;

void Benchmark(int num_threads)
{
	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 10) {
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

mutex stll;

void Benchmark2(int num_threads)
{
	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int x = rand() % RANGE;
		switch (rand() % 20) {
		case 0:
			stll.lock();
			set_list.insert(x);
			stll.unlock();
			break;
		case 2:
			stll.lock();
			set_list.erase(x);
			stll.unlock();
			break;
		default:
			stll.lock();
			set_list.count(x);
			stll.unlock();
			break;
		}
	}
}

int main()
{
	for (int i = 1; i <= 8; i = i * 2) {
		vector <thread> workers;
		clist.Init();
		auto start_t = system_clock::now();
		for (int j = 0; j < i; ++j)
			workers.emplace_back(Benchmark, i);
		for (auto& th : workers)
			th.join();
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;

		clist.Print20();
		cout << endl;
		cout << i << " threads";
		cout << "    exec_time = " << duration_cast<milliseconds>(exec_t).count();
		cout << "ms\n";
	}
}