#include <stdio.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <string.h>
#include <cassert>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <set>
#include <atomic>

using namespace std;
class bloom_filter
{
	private:
		int bits_per_element,
			capacity,
			mod,
			number_of_hashes,
			length;
		atomic <char> *data;	
		inline int get_hash(int x, int i) 
		{
			return ((1234ll + i) * x) % mod;
		}

	public:
		bloom_filter(int my_maxn, int my_bits_per_element): 
			bits_per_element(my_bits_per_element), 
			capacity(1 << int(ceil(log(my_maxn) / log(2)))),
			mod(capacity * bits_per_element), 
			number_of_hashes(ceil(0.693 * bits_per_element)),
			length((capacity * bits_per_element + 7) / 8),
			data(new atomic <char>[length]){}
		   
		~bloom_filter() 
		{
			delete[] data;
		}
		bool find(int x)
		{
			for (int i = 0; i < number_of_hashes; i++)
			{
			    int cell, index;
			    char offset, value;
				cell = get_hash(x, i);
				index = cell / 8;
				offset = 1 << (cell % 8);
				value = data[index];
				if (!(value & offset))
					return 0;	
			}
			return 1;
		}
		void insert(int x)
		{
			for (int i = 0; i < number_of_hashes; i++)
			{
			    int cell, index;
			    char offset, value;
				cell = get_hash(x, i);
				index = cell / 8;
				offset = 1 << (cell % 8);
				value = data[index];
				do
				{
				    if (value & offset)
				    	break;
					if (!(data[index].compare_exchange_weak(value, value | offset)))
						value = data[index];
				}
				while (1);
				
			}
	   	}
	   	void erase(int x)
	   	{
			for (int i = 0; i < number_of_hashes; i++)
			{
			    int cell, index;
			    char offset, value;
				cell = get_hash(x, i);
				index = cell / 8;
				offset = 1 << (cell % 8);
				value = data[index];
				do
				{
				    if (!(value & offset))
				    	break;
					if (!(data[index].compare_exchange_weak(value, value ^ offset)))
						value = data[index];
				}
				while (1);
				
			}

	   	}
};

const int maxn = 10000;
const int bpe = 4;
set <int> s;
int fails;
int main()
{
    srand(time(0));            
    freopen("test.out", "w", stdout);
	cout.precision(10);
	bloom_filter bf(maxn, bpe);
	for (int i = 0; i < 2 * maxn; i++)
	{
	    int value = rand() % maxn;
		if (rand() % 2)
		{
			s.insert(value);
			bf.insert(value);
		}
		else
		{
 			s.erase(value);
			bf.erase(value);
		}
	}
	for (int i = 0; i < maxn; i++)
	{
	    int a = bf.find(i);
	    int b = s.count(i);
//		cout << "i = "  << i << ", bf result is = " << a << ", set result is = " << b;
		if (a != b)
		{
//		    cout << "\tDIFF\n";
			fails++;
		}
		else
		{
//			cout << "\tSAME\n";
		}
	}
	cout << fixed << (1. * fails / maxn) / 100. << "%";
	return 0;
}