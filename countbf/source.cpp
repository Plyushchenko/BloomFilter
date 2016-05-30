#include <stdio.h>
#include <iostream>
#include <city.h>
#include <string.h>
#include <vector>
#include <atomic>
#include <algorithm>
#define pb push_back
using namespace std;

const uint64_t size_of_cell = 64;



//just debugging, printing binary representation
void bin(uint64_t x)
{
	typedef unsigned int ui;
    vector <bool> ans;
	for (ui i = 0; i < size_of_cell; i++)
	{
		ans.push_back(x % 2);
		x /= 2;	
	}
	reverse(ans.begin(), ans.end());
	for (ui i = 0; i < ans.size(); i++)
	{
	    if (!(i % 4))
			cerr << "|";
		cerr << ans[i];
	}
	cerr << "\n";
} 

class BFAdaptor
{
	public:
        template <class T> pair <const void *, const size_t> operator()(const vector<T> &v) const
        {
//	        cerr << "HERE\n";
            return {v.data(), v.size()};
        }
        template <class T> pair <const void *, const size_t> operator()(const T	&v) const
		{
			//just to make it compile
			return {0, 0};
		}
};

template <class T>
uint64_t double_hashing(const T& value, uint32_t i)
{
	//hash1 is std::hash and hash2 is just polynom with magic prime numbers
	return (hash<T>()(value) + 31013 * i * i + 1177711 * i);
}

template <class T>    
class BloomFilter
{
	private:
		uint32_t
			bits_per_counter, //should be divisor of size_of_cell (which is 64)
			counters_per_cell,
			counters_per_element,
			bits_per_element,
			number_of_hashes;
		uint64_t
			capacity,
			mod,
			length;
		uint64_t max_counter;
		atomic <uint64_t> *data;
		uint64_t (*get_hash)(const T&, uint32_t);
		bool is_adaptor = false;
		BFAdaptor adaptor;

		uint64_t get_index(uint64_t x) const
		{
			return x / counters_per_cell;
		}
	
		uint32_t get_offset(uint64_t x) const
		{
			return ((x % counters_per_cell) + 1) * bits_per_counter;
		}

		uint64_t get_counter(uint64_t value, uint32_t offset) const
		{
			return (value >> (size_of_cell - offset)) & max_counter;
		}

		uint64_t increment(uint64_t x)
		{
			return x += (x != max_counter);
		}   	

		uint64_t decrement(uint64_t x)
		{
			return x -= (x != 0);
		}

		uint64_t get_new_value(uint64_t value, uint32_t offset, uint64_t new_counter)
		{
			uint32_t shift = size_of_cell - offset + bits_per_counter;
			uint64_t old = (shift == size_of_cell) ? 0 : ((value >> shift) << shift);
			return old | (new_counter << (shift - bits_per_counter)) | (value % (1ull << (shift - bits_per_counter)));
		}
		void init_fields(uint64_t my_capacity, uint32_t my_bpc, uint32_t my_cpe) 
		{	
			bits_per_counter = my_bpc;
			counters_per_cell = size_of_cell / bits_per_counter;	
			counters_per_element = my_cpe;
			bits_per_element = bits_per_counter * counters_per_element;
			number_of_hashes = counters_per_element;
			mod = my_capacity;
			length = (my_capacity + counters_per_cell - 1) / counters_per_cell;
			max_counter = (1ull << bits_per_counter) - 1;
			data = new atomic <uint64_t>[length];
			fill(data, data + length, 0);
		}
		void init_for_methods(uint32_t i, uint64_t &h, uint64_t &index, uint32_t &offset, 
							  uint64_t &value, pair <const void *, size_t> &tmp, const T &x)
		{
			if (!is_adaptor)
				h = get_hash(x, i) % mod;
			else
				h = CityHash64WithSeed((char *)tmp.first, tmp.second, i) % mod;
			index = get_index(h);				
			offset = get_offset(h);
			value = data[index];
		}

		void cas_loop(uint64_t &index, uint32_t &offset, uint64_t &value, uint64_t &new_counter)
		{
			do
			{
				if (get_counter(value, offset) == new_counter)
					break;
				if (!(data[index].compare_exchange_weak(value, get_new_value(value, offset, new_counter))))
					value = data[index];
			}
			while (1);
		}

	public:
		BloomFilter(uint64_t my_capacity, uint32_t my_bpc, uint32_t my_cpe, uint64_t (*my_hash_family)(const T&, uint32_t) = &double_hashing<T>)
		{
			init_fields(my_capacity, my_bpc, my_cpe);
			get_hash = my_hash_family;
			if (get_hash == &double_hashing<T>)
				cerr << "\nWARNING: hash family hasn't been provided!\nUsing double-hashing based on std::hash\n\n";
		}

		BloomFilter(uint64_t my_capacity, uint32_t my_bpc, uint32_t my_cpe, BFAdaptor &my_adaptor)
		{
			init_fields(my_capacity, my_bpc, my_cpe);
			adaptor = my_adaptor;
			is_adaptor = true;
		}
		
		void insert(const T &x)
		{
			uint64_t h, index, value, new_counter;
			uint32_t offset;
			pair <const void *, size_t> tmp;
			
			if (is_adaptor)		
				tmp = adaptor(x);
			for (uint32_t i = 0; i < number_of_hashes; i++)
			{
				init_for_methods(i, h, index, offset, value, tmp, x);
				new_counter = increment(get_counter(value, offset));
				cas_loop(index, offset, value, new_counter);					
			}
		}

		void erase(const T &x)
		{
			uint64_t h, index, value, new_counter;
			uint32_t offset;
			pair <const void *, size_t> tmp;
			
			if (is_adaptor)
				tmp = adaptor(x);
			for (uint32_t i = 0; i < number_of_hashes; i++)
			{
				init_for_methods(i, h, index, offset, value, tmp, x);
				new_counter = decrement(get_counter(value, offset));
				cas_loop(index, offset, value, new_counter);					
			}
		}

		uint64_t count(const T &x)
		{
			uint64_t result = max_counter, h, index, value, counter;
			uint32_t offset;
			pair <const void *, size_t> tmp;

			if (is_adaptor)
				tmp = adaptor(x);
			for (uint32_t i = 0; i < number_of_hashes; i++)
			{
				init_for_methods(i, h, index, offset, value, tmp, x);
				counter = get_counter(value, offset);
				result = min(result, counter);
			}
			return result;
		} 
};

uint64_t test(char * const &value, uint32_t i)
{
	return CityHash64WithSeed(value, strlen(value), i);
}

int main()
{
    freopen("cerr", "w", stderr);

	BloomFilter <string> bf(1e3, 4, 3);
	for (int i = 0; i < 10; i++)
		bf.insert("aaaaa");
	bf.erase("aaaaa");
	for (int i = 0; i < 5; i++)
	{
		string tmp; tmp += char('A' + 3 * i);		
		bf.insert(tmp);
	}
	for (int i = 0; i < 26; i++)
	{
		string tmp; tmp += char('A' + i);
		cout << bf.count(tmp) << " ";
	}
	cout << bf.count("aaaaa") << "\n";
	
	
	BloomFilter <char *> bf2(1e3, 4, 3, test);
	return 0;
}
