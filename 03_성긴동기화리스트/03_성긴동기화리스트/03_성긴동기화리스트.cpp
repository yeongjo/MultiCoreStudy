// CPU: AMD Ryzen 3 3300X 4Core 3.79Ghz
// 
// mutex 사용한 성긴동기화
// 1 threads exec_time = 1195ms
// 2 threads exec_time = 2206ms
// 4 threads exec_time = 4871ms
// 8 threads exec_time = 11877ms

// nullmutex 사용한 성긴동기화
// 1 threads exec_time = 1178ms
// 2 threads crash!!

// stl set+lock 성긴동기화
// 리스트 정렬대신 set을 사용하는게 더 빠르다
// 1 threads exec_time = 657ms
// 2 threads exec_time = 1275ms
// 4 threads exec_time = 2695ms
// 8 threads exec_time = 6526ms

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <set>
#include <chrono>

using namespace std;
using namespace chrono;

const int NUM_TEST = 4'000'000;
//const int NUM_TEST = 40'000;
const int RANGE = 1'000;

class nullmutex {
public:
	void lock(){}
	void unlock(){}
};


class NODE {
public:
	int value;
	NODE* next;
	NODE() : next(nullptr) {
		
	}
	NODE(int x) : value(x), next(nullptr) {
		
	}
	~NODE(){}
};

class CLIST {
public:
	NODE head, tail;
	nullmutex llock;
	CLIST() {
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.next = &tail;
	}

	~CLIST() {
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
		pred = &head;
		llock.lock();
		curr = pred->next;
		while(curr->value < x){ // x가 노드값보다 커질때까지 뒤 노드로 전진
			pred = curr;
			curr = curr->next;
		}
		if (curr->value == x){
			llock.unlock();
			return false;
		}else{
			NODE* new_node = new NODE(x);
			pred->next = new_node;
			new_node->next = curr;
			llock.unlock();
			return true;
		}		
	}

	bool Remove(int x) {
		NODE* pred, * curr;
		pred = &head;
		llock.lock();
		curr = pred->next;
		while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
			pred = curr;
			curr = curr->next;
		}
		if (curr->value != x) {
			llock.unlock();
			return false;
		} else {
			pred->next = curr->next;
			delete curr;
			llock.unlock();
			return true;
		}
	}

	bool Contains(int x) {
		NODE* curr;
		llock.lock();
		curr = head.next;
		while (curr->value < x) { // x가 노드값보다 커질때까지 뒤 노드로 전진
			curr = curr->next;
		}
		if (curr->value != x) {
			llock.unlock();
			return false;
		} else {
			llock.unlock();
			return true;
		}
	}

	void Print20() {
		NODE* p = head.next;
		for (int i = 0; i < 20; i++) {
			if(p == &tail){
				break;
			}
			cout << p->value << ", ";
			p = p->next;
		}
	}
};

CLIST clist;

void Benchmark(int num_threads) {
	for (int i = 0; i < NUM_TEST; i++) {
		int x = rand() % RANGE;
		switch(rand() % 3){
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

set<int> set_list;
mutex stll;

void Benchmark2(int num_threads) {
	for (int i = 0; i < NUM_TEST; i++) {
		int x = rand() % RANGE;
		stll.lock();
		switch(rand() % 3){
		case 0:
			set_list.insert(x);
			break;
		case 1:
			set_list.erase(x);
			break;
		case 2:
			set_list.count(x);
			break;
		}
		stll.unlock();
	}
}

int main()
{
	for (int i = 1; i <= 8; i*=2) {
		vector<thread> workers;
		clist.Init();
		set_list.clear();

		auto start_t = system_clock::now();
		for (int j = 0; j < i; j++) {
			workers.emplace_back(Benchmark2, i);
		}
		for( auto& th : workers){
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
