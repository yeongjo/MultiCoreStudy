//Random book generated.
//Single thread[BCD] = 519
//Exec Time = 4200ms
//TBB concurrent_unordered_map[BCD] = 519
//Exec Time = 1278ms
//TBB concurrent_hash_map[BCD] = 519
//Exec Time = 1453ms

#include <iostream>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <tbb/parallel_for.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>

using namespace std;
using namespace chrono;
using namespace tbb;


constexpr int LIMIT = 50'000'000;
constexpr int THREADS_COUNT = 4;

string words[LIMIT];

int main() {
	for (auto& word : words){
		const int length = rand() % 5;
		for (int j = 0; j < length; j++) {
			char ch = 'A' + rand() % 26;
			word += ch;
		}
	}

	cout << "Random book generated.\n";
	
	unordered_map<string, int> table;
	auto start_t = system_clock::now();
	for(auto &s : words){
		table[s]++;
	}
	auto end_t = system_clock::now();
	auto exec_t = end_t - start_t;
	cout << "Single thread [BCD] = " << table["BCD"] << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";

	
	concurrent_unordered_map<string, atomic_int> tbb_table;
	start_t = system_clock::now();
	parallel_for(0, LIMIT, 1, [&tbb_table](int i) {
		++tbb_table[words[i]]; });
	end_t = system_clock::now();
	exec_t = end_t - start_t;
	cout << "TBB concurrent_unordered_map [BCD] = " << tbb_table["BCD"] << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";

	
	concurrent_hash_map<string, atomic_int> tbb_htable;
	start_t = system_clock::now();
	parallel_for(0, LIMIT, 1, [&tbb_htable](int i) {
		concurrent_hash_map<string, atomic_int>::accessor a;
		tbb_htable.insert(a, words[i]);
		++a->second;
		});
	end_t = system_clock::now();
	exec_t = end_t - start_t;
	concurrent_hash_map<string, atomic_int>::const_accessor a;
	tbb_htable.find(a, "BCD");
	cout << "TBB concurrent_hash_map [BCD] = " << a->second << endl;
	std::cout << "Exec Time = " << duration_cast<milliseconds>(exec_t).count() << "ms\n";
}
