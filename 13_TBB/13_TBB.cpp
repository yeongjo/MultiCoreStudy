//LOOP = 50'000'000;
//
//Single thread Sum = 100000000
//Exec Time = 18ms
//
//TBB parallel_for Sum = 21705356
//Exec Time = 274ms
//
//Thread Sum = 33113384
//Exec Time = 142ms

#include <iostream>
#include <thread>
#include <chrono>
#include <tbb/parallel_for.h>

using namespace std;
using namespace chrono;
using namespace tbb;


constexpr int LOOP = 50'000'000;
constexpr int THREADS_COUNT = 4;

int main()
{
	volatile int sum = 0;
	//atomic_int sum;
	auto start_t = system_clock::now();
	for (int i = 0; i < LOOP; i++) {
		sum += 2;
	}
	auto end_t = system_clock::now();
	auto exec_t = end_t - start_t;
	cout << "Single thread Sum = " << sum << endl;
    std::cout << "Exec Time = "<< duration_cast<milliseconds>(exec_t).count() << "ms\n";

	
	sum = 0;
	start_t = system_clock::now();
	parallel_for(0, LOOP, 1, [&sum](int i) {sum += 2; });
	end_t = system_clock::now();
	exec_t = end_t - start_t;
	cout << "TBB parallel_for Sum = " << sum << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";


	sum = 0;
	start_t = system_clock::now();
	thread threads[THREADS_COUNT];
	for (auto& th : threads) th = thread{ [&sum](int threads_count) {
		for (int i = 0; i < LOOP/threads_count; i++) {
			sum += 2;
		}
	}, THREADS_COUNT };
	for (auto& th : threads) th.join();
	end_t = system_clock::now();
	exec_t = end_t - start_t;
	cout << "Thread Sum = " << sum << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";
}
