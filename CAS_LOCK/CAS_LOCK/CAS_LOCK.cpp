// CPU: AMD Ryzen 3 3300X 4Core 3.79Ghz

// CAS Lock
//number of threads = 1 exec_time = 106 sum = 20000000
//number of threads = 2 exec_time = 229 sum = 20000000
//number of threads = 4 exec_time = 404 sum = 20000000
//number of threads = 8 exec_time = 995 sum = 20000000


#include <iostream>
#include <chrono>
#include <vector>
#include <atomic>
#include <thread>

using namespace std;
using namespace std::chrono;

#define MAX_THREAD 8

int LOCK;

bool CAS(int* addr, int expected, int new_val) {
	return atomic_compare_exchange_strong(reinterpret_cast<std::atomic_int*>(addr), &expected, new_val);
}

void CAS_LOCK() {
	while (!CAS(&LOCK, 0, 1));
}

void CAS_UNLOCK() {
	while (!CAS(&LOCK, 1, 0));
}

volatile int sum;

void worker(int num_threads, int my_id) {
	const int loop_count = 10'000'000 / num_threads;
	for (auto i = 0; i < loop_count; ++i) {
		CAS_LOCK();
		//m_lock.lock();
		//b_lock->lock2(my_id);
		sum += 2;
		//b_lock->unlock(my_id);
		//m_lock.unlock();
		CAS_UNLOCK();
	}
}


int main() {
	for (int i = 1; i <= MAX_THREAD; i *= 2) {
		sum = 0;
		vector<thread> workers;
		auto start_t = high_resolution_clock::now();
		for (int j = 0; j < i; ++j) {
			workers.emplace_back(worker, i, j);
		}
		for (auto& th : workers) {
			th.join();
		}
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "number of threads = " << i;
		cout << " exec_time = " << duration_cast <milliseconds> (du_t).count();
		cout << " sum = " << sum << endl;
	}
}
