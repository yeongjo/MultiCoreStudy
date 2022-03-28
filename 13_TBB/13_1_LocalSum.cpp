//LOOP = 500'000'000;
//
//Single thread Sum = 1000000000
//Exec Time = 232ms
//
//TBB parallel_for Sum = 1000000000
//Exec Time = 100ms
//
//Thread Sum = 1000000000
//Exec Time = 85ms

#include <iostream>
#include <thread>
#include <chrono>
#include <tbb/parallel_for.h>

using namespace std;
using namespace chrono;
using namespace tbb;


constexpr int LOOP = 500'000'000;
constexpr int THREADS_COUNT = 4;

int main() {
	volatile int sum = 0;
	//atomic_int sum;
	auto start_t = system_clock::now();
	for (int i = 0; i < LOOP; i++) {
		sum += 2;
	}
	auto end_t = system_clock::now();
	auto exec_t = end_t - start_t;
	cout << "Single thread Sum = " << sum << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";


	sum = 0;
	start_t = system_clock::now();
	constexpr int CHUNK = LOOP/4;
	parallel_for(0, LOOP / CHUNK, 1, [&sum, CHUNK](int i) {
		volatile int local_sum = 0;
		for (int i = 0; i < CHUNK; i++) {
			local_sum += 2;
		}
		sum += local_sum;
	});
	end_t = system_clock::now();
	exec_t = end_t - start_t;
	cout << "TBB parallel_for Sum = " << sum << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";


	sum = 0;
	start_t = system_clock::now();
	thread threads[THREADS_COUNT];
	for (auto &th : threads) th = thread{ [&sum](int threads_count) {
		volatile int local_sum = 0;
		for (int i = 0; i < LOOP / threads_count; i++) {
			local_sum += 2;
		}
		sum += local_sum;
	}, THREADS_COUNT };
	for (auto &th : threads) th.join();
	end_t = system_clock::now();
	exec_t = end_t - start_t;
	cout << "Thread Sum = " << sum << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";
}
