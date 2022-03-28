#include <omp.h>
#include <iostream>
using namespace std;

int main() {
	volatile int sum = 0;
#pragma omp parallel
	{
		int num_thread = omp_get_num_threads();
		for(int i = 0; i < 50000000 / num_thread; ++i){
//#pragma omp critical
			//sum += 2;
			++sum;
		}
	}
	cout << "Sum = " << sum << endl;
}
