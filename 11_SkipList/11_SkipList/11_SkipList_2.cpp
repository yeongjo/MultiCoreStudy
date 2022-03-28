// 성긴 락프리
//Threads[ 1 ] , sum= 0, Exec_time =624 msecs
//Threads[2], sum = 0, Exec_time = 651 msecs
//Threads[4], sum = 0, Exec_time = 773 msecs
//Threads[8], sum = 0, Exec_time = 1026 msecs
//Threads[16], sum = 0, Exec_time = 1033 msecs

#include <chrono>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>

using namespace std;
using namespace std::chrono;


const auto NUM_TEST = 4000000;
const auto KEY_RANGE = 1000;

volatile int sum;

constexpr int MAXLEVEL = 10;


class LFSKNODE;

class SKMPTR {
	atomic_int V;
public:
	SKMPTR() {

	}

	// ㅠㅠ 누군가한테 얻어야할듯

	bool CAS() {

	}
};

//skip list node
class SLNODE {
public:
	int key;
	SLNODE *next[MAXLEVEL]; //key range가 최대 1000개니까 평균이 500정도고, 사실 500이 넘는다. add는 실패확률이 높으니 500보다 크다. 2의 10승정도니까 10층정도로 하자 
	int height; //몇층이냐. 모든 노드가 층이 같은게 아니라 높은게 있고 낮은게 있으니까

	SLNODE() {
		key = 0;
		height = MAXLEVEL;
		for (auto &p : next)
			p = nullptr;
	}

	SLNODE(int _key, int _height) {
		key = _key;
		height = _height;
		for (auto &p : next)
			p = nullptr;
	}

	SLNODE(int _key) {
		key = _key;
		height = MAXLEVEL;
		for (auto &p : next)
			p = nullptr;
	}

};
//skip list node
class LFSKNODE {
public:
	int value;
	LFSKNODE *next[MAXLEVEL]; //key range가 최대 1000개니까 평균이 500정도고, 사실 500이 넘는다. add는 실패확률이 높으니 500보다 크다. 2의 10승정도니까 10층정도로 하자 
	int toplevel; //몇층이냐. 모든 노드가 층이 같은게 아니라 높은게 있고 낮은게 있으니까

	LFSKNODE() : toplevel(0) {
		value = 0;
		for (auto &p : next)
			p = nullptr;
	}

	LFSKNODE(int x, int level) {
		value = x;
		toplevel = level;
		for (auto &p : next)
			p = nullptr;
	}

	LFSKNODE(int x) {
		value = x;
		toplevel = MAXLEVEL;
		for (auto &p : next)
			p = nullptr;
	}

	LFSKNODE *get_ptr();
	void set_ptr(__resharper_unknown_type p);
	bool get_removed(LFSKNODE* succ);
	bool CAS(LFSKNODE * curr, LFSKNODE* succ, bool cond);
};

//skip list
class SKLIST {
	SLNODE head, tail;
	mutex glock;

public:

	SKLIST() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.height = tail.height = MAXLEVEL; //모든 노드를 감싸고 있어야한다. 
		for (auto &p : head.next) p = &tail; //tail의 주소를 줘야하한다, head.next[0] = tail.next[0] head.next[1] = tail.next[1] 이런식으로 프로그래밍하면 안된다. tail의 주소가 들어가야하지 tail.next의 주소가 들어가면 안된다. head next가 다 널이된다. 왜? tail next는 널이니까. head.next[0] = &tail; head.next[1] = &tail 이렇게 해야한다. 
	}
	~SKLIST() {
		Init();
	}

	void Init() {
		SLNODE *ptr;
		while (head.next[0] != &tail) {
			ptr = head.next[0];
			head.next[0] = head.next[0]->next[0];
			delete ptr;
		}

		for (auto &p : head.next) p = &tail;
	}

	void Find(int key, SLNODE *preds[MAXLEVEL], SLNODE *currs[MAXLEVEL]) {
		//curr_level
		int cl = MAXLEVEL - 1; //맨 윗레벨 
		while (true) {
			if (MAXLEVEL - 1 == cl) //맨 꼭대기층이면 head부터, 아니라면 else
				preds[cl] = &head; //head부터 검사
			else
				preds[cl] = preds[cl + 1]; //위에서 검색 시작한 위치에서 검색 시작

			currs[cl] = preds[cl]->next[cl]; //head가 가리키는 것의 next가 curr

			while (currs[cl]->key < key) {
				preds[cl] = currs[cl];
				currs[cl] = currs[cl]->next[cl];
			}

			if (0 == cl) return;
			cl--;

			//이렇게 하면 맨 윗층부터 맨 아래층까지 차곡차곡 연결이 되어있게 된다 

		}
	}
	bool Add(int key) {
		SLNODE *preds[MAXLEVEL], *currs[MAXLEVEL];

		glock.lock();
		Find(key, preds, currs);

		if (key == currs[0]->key) {
			glock.unlock();
			return false;
		} else {
			int height = 1; //높이가 0층은 없고 무조건 1층은 있어야 함. 
			while (rand() % 2 == 0) ////위로 올라갈수록 확률이 줄어들어야한다. if문 10층 쌓기 싫으니까 while로 
			{
				height++;
				if (MAXLEVEL == height) break;
			}

			SLNODE *node = new SLNODE(key, height);

			//어떻게 끼워넣을거냐
			for (int i = 0; i < height; ++i) {
				node->next[i] = currs[i];
				preds[i]->next[i] = node;
			}

			glock.unlock();
			return true;
		}

	}
	bool Remove(int key) {
		SLNODE *preds[MAXLEVEL], *currs[MAXLEVEL];

		glock.lock();
		Find(key, preds, currs);
		if (key == currs[0]->key) {
			//삭제
			for (int i = 0; i < currs[0]->height; ++i) {
				preds[i]->next[i] = currs[i]->next[i];
			}

			delete currs[0];
			glock.unlock();
			return true;
		} else {
			glock.unlock();
			return false;
		}

	}
	bool Contains(int key) {
		SLNODE *preds[MAXLEVEL], *currs[MAXLEVEL];

		glock.lock();

		Find(key, preds, currs);

		if (key == currs[0]->key) {
			glock.unlock();
			return true;
		} else {
			glock.unlock();
			return false;
		}

	}

	void display20() {

		int c = 20;
		SLNODE *p = head.next[0];

		while (p != &tail) {
			cout << p->key << ", ";
			p = p->next[0]; //밑바닥만 보여주자 지름길은 신경쓰지않고 
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};

//skip list
class LFSKLIST {
	LFSLNODE head, tail;

public:

	LFSKLIST() {
		head.value = 0x80000000;
		tail.value = 0x7FFFFFFF;
		head.toplevel = tail.toplevel = MAXLEVEL; //모든 노드를 감싸고 있어야한다. 
		for (auto &p : head.next)
			p = &tail; //tail의 주소를 줘야하한다, head.next[0] = tail.next[0] head.next[1] = tail.next[1] 이런식으로 프로그래밍하면 안된다. tail의 주소가 들어가야하지 tail.next의 주소가 들어가면 안된다. head next가 다 널이된다. 왜? tail next는 널이니까. head.next[0] = &tail; head.next[1] = &tail 이렇게 해야한다. 
	}
	~LFSKLIST() {
		Init();
	}

	void Init() {
		LFSKNODE *ptr;
		while (head.next[0].get_ptr() != &tail) {
			LFSKNODE *p = head.next[0].get_ptr();
			head.next[0].set_ptr(p->next[0]);
			delete ptr;
		}

		for (auto &p : head.next) p = &tail;
	}

	bool Find(int x, LFSKNODE *preds[], LFSKNODE *currs[]) {
	retry:
		//curr_level
		int curr_level = MAXLEVEL; //맨 윗레벨
		preds[curr_level] = &head;
		while (true) {
			while (true) {
				currs[curr_level] = preds[curr_level]->next[curr_level].get_ptr();
				LFSKNODE *succ;
				bool is_removed = currs[curr_level]->next[curr_level].get_removed(succ);
				while (true == is_removed) {
					if (false == preds[curr_level]->next[curr_level].CAS(currs[curr_level], succ, false)) {
						goto retry;
					}
					currs[curr_level] = succ;
					is_removed = currs[curr_level]->next[curr_level].get_removed(succ);
				}
				if (currs[curr_level]->value >= x) {
					break;
				}
				preds[curr_level] = currs[curr_level];
			}
			if (0 == curr_level) return currs[0]->value == x;
			curr_level--;
			preds[curr_level] = preds[curr_level-1];
		}
	}
	bool Add(int key) {
		LFSLNODE *preds[MAXLEVEL], *currs[MAXLEVEL];

		glock.lock();
		Find(key, preds, currs);

		if (key == currs[0]->value) {
			glock.unlock();
			return false;
		} else {
			int height = 1; //높이가 0층은 없고 무조건 1층은 있어야 함. 
			while (rand() % 2 == 0) ////위로 올라갈수록 확률이 줄어들어야한다. if문 10층 쌓기 싫으니까 while로 
			{
				height++;
				if (MAXLEVEL == height) break;
			}

			LFSLNODE *node = new LFSLNODE(key, height);

			//어떻게 끼워넣을거냐
			for (int i = 0; i < height; ++i) {
				node->next[i] = currs[i];
				preds[i]->next[i] = node;
			}

			glock.unlock();
			return true;
		}

	}
	
	bool Remove(int x) {
		LFSKNODE *preds[MAXLEVEL+1], *currs[MAXLEVEL+1];
		if (false == Find(x, preds, currs)) return false;
		LFSKNODE *target = currs[0];

		for(int i = target->toplevel; i > 0; ++i){
			LFSKNODE* succ = target->next[i].get_ptr();
			while (true) {
				if (true == target->next[i].CAS(succ, succ, false, true)) {
					break;
				}
				if (true == target->next[i].get_removed(succ)) {
					break;
				}
				Find(x, preds, currs);
			}
		}

		while (true) {
			LFSKNODE *succ;
			bool other_removed = target->next[0].get_removed(succ);
			if (true == other_removed){
				return false;
			}
			if (true == target->next[0].CAS(succ, succ, false, true)) {
				Find(x, preds, currs);
				return true;
			}
			Find(x, preds, currs);
		}
	}
	bool Contains(int key) {
		LFSLNODE *preds[MAXLEVEL], *currs[MAXLEVEL];

		glock.lock();

		Find(key, preds, currs);

		if (key == currs[0]->value) {
			glock.unlock();
			return true;
		} else {
			glock.unlock();
			return false;
		}

	}

	void display20() {

		int c = 20;
		LFSLNODE *p = head.next[0];

		while (p != &tail) {
			cout << p->value << ", ";
			p = p->next[0]; //밑바닥만 보여주자 지름길은 신경쓰지않고 
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};


SKLIST sklist;
void ThreadFunc(/*void *lpVoid,*/ int num_thread) {
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0: key = rand() % KEY_RANGE;
			sklist.Add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			sklist.Remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			sklist.Contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main() {

	for (auto n = 1; n <= 16; n *= 2) {
		sum = 0;
		sklist.Init();
		vector <thread> threads;
		auto start_time = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			threads.emplace_back(ThreadFunc, n);
		}
		for (auto &th : threads) th.join();

		auto end_time = high_resolution_clock::now();
		auto exec_time = end_time - start_time;
		int exec_ms = duration_cast<milliseconds>(exec_time).count();
		sklist.display20();
		cout << "Threads[ " << n << " ] , sum= " << sum;
		cout << ", Exec_time =" << exec_ms << " msecs\n";
		cout << endl;
	}

	system("pause");
}