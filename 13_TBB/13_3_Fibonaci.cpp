//Single thread Fibo[45] = 1836311903
//Exec Time = 3123ms

#include <iostream>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <tbb/parallel_for.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>
#include <tbb/task.h>

using namespace std;
using namespace chrono;
using namespace tbb;
using namespace tbb::detail::d1;


constexpr int LIMIT = 50'000'000;
constexpr int THREADS_COUNT = 4;


int serial_fibo(int n) {
	return n <= 2 ? n : serial_fibo(n - 2) + serial_fibo(n - 1);
	//if (n <= 2) {
	//	return n;
	//} else{
	//	return serial_fibo(n - 2) + serial_fibo(n - 1);
	//}
}

class FibTask : public task {
public:
	const long n;
	long *const sum;
	FibTask(long n_, long *sum_) : n(n_), sum(sum_) {
	}
	task *execute() { // Overrides virtual function task::execute
		if (n <= 2) { // �׽�ũ�� �ʹ� �߰Գ������� ����ϴ°ź��� �׽�ũ ���� ����� �� ���̵��. �ƿ����� 30���� �̱۽������ �����ϰ��ϸ� ������ ����
			*sum = serial_fibo(n);
		} else {
			long x, y;
			// vcpkg�� �������� ��ƾ� �ߵ�
			FibTask &a = *new(allocate_child()) FibTask(n - 1, &x);
			FibTask &b = *new(allocate_child()) FibTask(n - 2, &y);
			// Set ref_count to "two children plus one for the wait".
			set_ref_count(3);
			// Start b running.
			spawn(b);
			// Start a running and wait for all children (a and b).
			spawn_and_wait_for_all(a);
			// Do the sum
			*sum = x + y;
		}
		return NULL;
	}
};

int main() {
	constexpr int F_INDEX = 45;
	//int fibo = 0;
	volatile int fibo = 0;
	auto start_t = system_clock::now();
	cout << "start" << endl;
	fibo = serial_fibo(F_INDEX);
	auto end_t = system_clock::now();
	cout << "end" << endl;
	auto exec_t = end_t - start_t;
	cout << "Single thread Fibo["<< F_INDEX <<"] = " << fibo << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";
}
