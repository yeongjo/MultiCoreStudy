// CPU: AMD Ryzen 3 3300X 4Core 3.79Ghz
//
// 큐
//
// 락을 사용하지 않았을때
// 1 threads exec_time = 621ms
// 2 threads exec_time = 416ms - 잘못된 결과
// 
// mutex 사용한 성긴동기화
//1 threads exec_time = 758ms
//2 threads exec_time = 814ms
//4 threads exec_time = 996ms
//8 threads exec_time = 1186ms
//
// LockFree
// 드물게 충돌이 발생한다.
// 스레드를 늘리거나 큐에 데이터가 없을때 enq deq 비슷한 시기에 불리면 문제가 생긴다
// remove할때 delete해서 생기는 문제
//1 threads exec_time = 665ms
//2 threads exec_time = 655ms
//4 threads exec_time = 623ms
//8 threads exec_time = 808ms

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>

using namespace std;
using namespace chrono;

const int NUM_TEST = 10'000'000;
//const int NUM_TEST = 40'000;

class nullmutex {
public:
	void lock() {}
	void unlock() {}
};


class NODE {
public:
	int value;
	NODE *volatile next;

	NODE() : next(nullptr) {

	}
	NODE(int x) : value(x), next(nullptr) {

	}
	~NODE() {
	}
};

class CQUEUE {
	NODE *head, *tail;
	nullmutex enq_lock, deq_lock;
public:
	CQUEUE() {
		head = tail = new NODE(0);
	}
	~CQUEUE() {

	}

	void Init() {
		while (head != tail) {
			NODE *p = head;
			head = head->next;
			delete p;
		}
	}

	void Enq(int x) {
		enq_lock.lock();
		NODE *e = new NODE(x);
		tail->next = e;
		tail = e;
		enq_lock.unlock();
	}

	int Deq() {
		int result;
		deq_lock.lock();
		if(head->next == nullptr){
			deq_lock.unlock();
			return -1;
		}
		result = head->next->value;
		head = head->next;
		deq_lock.unlock();
		return result;
	}

	void Print20() {
		NODE *p = head->next;
		for (int i = 0; i < 20; i++) {
			if (p == nullptr) {
				break;
			}
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

class LFQUEUE {
	NODE * volatile head, *volatile tail;
public:
	LFQUEUE() {
		head = tail = new NODE(0);
	}
	~LFQUEUE() {

	}

	void Init() {
		while (head != tail) {
			NODE *p = head;
			head = head->next;
			delete p;
		}
	}

	bool CAS(NODE * volatile *addr, NODE* old_node, NODE *new_node) {
		int a_addr = reinterpret_cast<int>(addr);
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(a_addr), reinterpret_cast<int*>(&old_node), reinterpret_cast<int>(new_node));
	}

	void Enq(int x) {
		NODE *e = new NODE(x);
		while(true){
			NODE *last = tail;
			NODE *next = last->next;
			if(last != tail){
				continue;
			}
			if(nullptr == next){
				if(CAS(&(last->next), nullptr, e)){
					CAS(&tail, last, e);
					return;
				}
			} else{
				CAS(&tail, last, next);
			}
		}
	}

	int Deq() {
		while(true){
			NODE *first = head;
			NODE *last = tail;
			NODE *next = first->next;
			if (first != head) continue;
			if (nullptr == next) return -1;
			if(first == last){
				CAS(&tail, last, next);
				continue;
			}
			int value = next->value;
			if(false == CAS(&head, first, next)){
				continue;
			}
			delete first;
			return value;
		}
	}

	void Print20() {
		NODE *p = head->next;
		for (int i = 0; i < 20; i++) {
			if (p == nullptr) {
				break;
			}
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

struct STAMP_POINTER {
	NODE *volatile ptr;
	int volatile stamp;
};

class SPLFQUEUE {
	STAMP_POINTER head, tail;
public:
	SPLFQUEUE() {
		head.ptr = tail.ptr = new NODE(0);
		head.stamp = tail.stamp = 0;
	}
	~SPLFQUEUE() {
		Init();
		delete head.ptr;
	}

	void Init() {
		while (head.ptr != tail.ptr) {
			NODE *p = head.ptr;
			head.ptr = head.ptr->next;
			delete p;
		}
	}

	bool CAS(NODE * volatile *addr, NODE* old_node, NODE *new_node) {
		int a_addr = reinterpret_cast<int>(addr);
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(a_addr), reinterpret_cast<int*>(&old_node), reinterpret_cast<int>(new_node));
	}

	bool STAMP_CAS(STAMP_POINTER * volatile addr, NODE* old_node, NODE *new_node, int old_stamp, int new_stamp) {
		STAMP_POINTER old_p{ old_node, old_stamp };
		STAMP_POINTER new_p{ new_node, new_stamp };
		long long new_value = *(reinterpret_cast<long long *>(&new_p));
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong*>(addr), reinterpret_cast<long long*>(&old_p), new_value);
	}

	void Enq(int x) {
		NODE *e = new NODE(x);
		while(true){
			STAMP_POINTER last = tail;
			NODE *next = last.ptr->next;
			if(last.ptr != tail.ptr || last.stamp != tail.stamp){
				continue;
			}
			if(nullptr == next){
				if(CAS(&(last.ptr->next), nullptr, e)){
					STAMP_CAS(&tail, last.ptr, e, last.stamp, last.stamp+1);
					return;
				}
			} else{
				STAMP_CAS(&tail, last.ptr, next, last.stamp, last.stamp + 1);
			}
		} 
	}

	int Deq() {
		while(true){
			STAMP_POINTER first = head;
			STAMP_POINTER last = tail;
			NODE *next = first.ptr->next;
			if (first.ptr != head.ptr || first.stamp != head.stamp) continue;
			if (nullptr == next) return -1;
			if(first.ptr == last.ptr){
				STAMP_CAS(&tail, last.ptr, next, last.stamp, last.stamp+1);
				continue;
			}
			int value = next->value;
			if(false == STAMP_CAS(&head, first.ptr, next, first.stamp, first.stamp+1)){
				continue;
			}
			delete first.ptr;
			return value;
		}
	}

	void Print20() {
		NODE *p = head.ptr->next;
		for (int i = 0; i < 20; i++) {
			if (p == nullptr) {
				break;
			}
			cout << p->value << ", ";
			p = p->next;
		}
		cout << endl;
	}
};

SPLFQUEUE myqueue;

void Benchmark(int num_threads) {
	for (int i = 0; i < NUM_TEST / num_threads; i++) {
		if ((0 == rand() % 2) || (i < 32 / num_threads)) {
			myqueue.Enq(i);
		} else {
			myqueue.Deq();
		}
	}
}

int main() {
	cout << "start" << endl;
	for (int i = 1; i <= 16; i *= 2) {
		vector<thread> workers;
		myqueue.Init();

		auto start_t = system_clock::now();
		for (int j = 0; j < i; j++) {
			workers.emplace_back(Benchmark, i);
		}
		for (auto &th : workers) {
			th.join();
		}
		auto end_t = system_clock::now();
		auto exec_t = end_t - start_t;
		myqueue.Print20();

		cout << i << " threads ";
		cout << "exec_time = " << duration_cast<milliseconds>(exec_t).count();
		cout << "ms\n";
	}
}
