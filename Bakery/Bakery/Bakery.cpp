// CPU: AMD Ryzen 3 3300X 4Core 3.79Ghz

// No Lock
//number of threads = 1 exec_time = 36 sum = 20000000
//number of threads = 2 exec_time = 26 sum = 10645852
//number of threads = 4 exec_time = 27 sum = 8189622
//number of threads = 8 exec_time = 31 sum = 5576834

// mutex
//number of threads = 1 exec_time = 171 sum = 20000000
//number of threads = 2 exec_time = 178 sum = 20000000
//number of threads = 4 exec_time = 308 sum = 20000000
//number of threads = 8 exec_time = 493 sum = 20000000

// 교재 빵집 알고리즘  lock()
//number of threads = 1 exec_time = 52 sum = 20000000
//number of threads = 2 exec_time = 785 sum = 19999992
//number of threads = 4 exec_time = 1526 sum = 19999978
//number of threads = 8 exec_time = 3820 sum = 20000000

// 인터넷 빵집 알고리즘  lock2()
//number of threads = 1 exec_time = 97 sum = 20000000
//number of threads = 2 exec_time = 380 sum = 19457634
//number of threads = 4 exec_time = 1525 sum = 19955074
//number of threads = 8 exec_time = 6084 sum = 19981820

#include <iostream>
#include <chrono>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

using namespace std;
using namespace std::chrono;

#define MAX_THREAD 8

int max(int* num_list, int count) {
	int max_number = 0;
	for (int idx = 0; idx < count; ++idx) {
		max_number = num_list[idx] > max_number ? num_list[idx] : max_number;
	}
	return max_number;
}

class Bakery {
	bool * volatile flag;
	int * volatile label;
	int size;
public:
	Bakery(int thread_count) {
		size = thread_count;
		flag = new bool[thread_count];
		label = new int[thread_count];
		memset(label, 0, sizeof(int) * thread_count);
		memset(flag, 0, sizeof(bool) * thread_count);
	}

	~Bakery() {
		delete[] flag;
		delete[] label;
	}

	// 교재 알고리즘
	void lock(int thread_id) {
		int i = thread_id;
		flag[i] = true;
		label[i] = max(label, size) + 1;
		for(int k = 0; k < size; ++k){
			while (flag[k] && (label[k] < label[i] || (label[k] == label[i] && k < i)));
		}
	}

	// 인터넷 알고리즘
	void lock2(int thread_id) {
		int i = thread_id;
		flag[i] = true;
		label[i] = max(label, size) + 1;
		flag[i] = false;
		atomic_thread_fence(std::memory_order_seq_cst);
		for(int k = 0; k < size; ++k){
			while (flag[k]);
			while (label[k] != 0 && (label[k] < label[i] || (label[k] == label[i] && k < i)));
		}
		label[i] = 0;
	}

	void unlock(int thread_id) {
		flag[thread_id] = false;
	}
};

Bakery* b_lock;
mutex m_lock;
volatile int sum;

void worker(int num_threads, int my_id) {
	const int loop_count = 10'000'000 / num_threads;
	for (auto i = 0; i < loop_count; ++i){
		//m_lock.lock();
		//b_lock->lock2(my_id);
		sum += 2;
		//b_lock->unlock(my_id);
		//m_lock.unlock();
	}
}


int main() {
	for (int i = 1; i <= MAX_THREAD; i *= 2){
		sum = 0;
		b_lock = new Bakery(i);
		vector<thread> workers;
		auto start_t = high_resolution_clock::now();
		for (int j = 0; j < i; ++j){
			workers.emplace_back(worker, i, j);
		}
		for (auto& th : workers){
			th.join();
		}
		auto end_t = high_resolution_clock::now();
		auto du_t = end_t - start_t;

		cout << "number of threads = " << i;
		cout << " exec_time = " << duration_cast <milliseconds> (du_t).count();
		cout << " sum = " << sum << endl;
		delete b_lock;
	}
}
