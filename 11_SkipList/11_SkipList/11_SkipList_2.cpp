// ���� ������
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

	// �Ф� ���������� �����ҵ�

	bool CAS() {

	}
};

//skip list node
class SLNODE {
public:
	int key;
	SLNODE *next[MAXLEVEL]; //key range�� �ִ� 1000���ϱ� ����� 500������, ��� 500�� �Ѵ´�. add�� ����Ȯ���� ������ 500���� ũ��. 2�� 10�������ϱ� 10�������� ���� 
	int height; //�����̳�. ��� ��尡 ���� ������ �ƴ϶� ������ �ְ� ������ �����ϱ�

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
	LFSKNODE *next[MAXLEVEL]; //key range�� �ִ� 1000���ϱ� ����� 500������, ��� 500�� �Ѵ´�. add�� ����Ȯ���� ������ 500���� ũ��. 2�� 10�������ϱ� 10�������� ���� 
	int toplevel; //�����̳�. ��� ��尡 ���� ������ �ƴ϶� ������ �ְ� ������ �����ϱ�

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
		head.height = tail.height = MAXLEVEL; //��� ��带 ���ΰ� �־���Ѵ�. 
		for (auto &p : head.next) p = &tail; //tail�� �ּҸ� ������Ѵ�, head.next[0] = tail.next[0] head.next[1] = tail.next[1] �̷������� ���α׷����ϸ� �ȵȴ�. tail�� �ּҰ� �������� tail.next�� �ּҰ� ���� �ȵȴ�. head next�� �� ���̵ȴ�. ��? tail next�� ���̴ϱ�. head.next[0] = &tail; head.next[1] = &tail �̷��� �ؾ��Ѵ�. 
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
		int cl = MAXLEVEL - 1; //�� ������ 
		while (true) {
			if (MAXLEVEL - 1 == cl) //�� ��������̸� head����, �ƴ϶�� else
				preds[cl] = &head; //head���� �˻�
			else
				preds[cl] = preds[cl + 1]; //������ �˻� ������ ��ġ���� �˻� ����

			currs[cl] = preds[cl]->next[cl]; //head�� ����Ű�� ���� next�� curr

			while (currs[cl]->key < key) {
				preds[cl] = currs[cl];
				currs[cl] = currs[cl]->next[cl];
			}

			if (0 == cl) return;
			cl--;

			//�̷��� �ϸ� �� �������� �� �Ʒ������� �������� ������ �Ǿ��ְ� �ȴ� 

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
			int height = 1; //���̰� 0���� ���� ������ 1���� �־�� ��. 
			while (rand() % 2 == 0) ////���� �ö󰥼��� Ȯ���� �پ�����Ѵ�. if�� 10�� �ױ� �����ϱ� while�� 
			{
				height++;
				if (MAXLEVEL == height) break;
			}

			SLNODE *node = new SLNODE(key, height);

			//��� ���������ų�
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
			//����
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
			p = p->next[0]; //�عٴڸ� �������� �������� �Ű澲���ʰ� 
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
		head.toplevel = tail.toplevel = MAXLEVEL; //��� ��带 ���ΰ� �־���Ѵ�. 
		for (auto &p : head.next)
			p = &tail; //tail�� �ּҸ� ������Ѵ�, head.next[0] = tail.next[0] head.next[1] = tail.next[1] �̷������� ���α׷����ϸ� �ȵȴ�. tail�� �ּҰ� �������� tail.next�� �ּҰ� ���� �ȵȴ�. head next�� �� ���̵ȴ�. ��? tail next�� ���̴ϱ�. head.next[0] = &tail; head.next[1] = &tail �̷��� �ؾ��Ѵ�. 
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
		int curr_level = MAXLEVEL; //�� ������
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
			int height = 1; //���̰� 0���� ���� ������ 1���� �־�� ��. 
			while (rand() % 2 == 0) ////���� �ö󰥼��� Ȯ���� �پ�����Ѵ�. if�� 10�� �ױ� �����ϱ� while�� 
			{
				height++;
				if (MAXLEVEL == height) break;
			}

			LFSLNODE *node = new LFSLNODE(key, height);

			//��� ���������ų�
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
			p = p->next[0]; //�عٴڸ� �������� �������� �Ű澲���ʰ� 
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