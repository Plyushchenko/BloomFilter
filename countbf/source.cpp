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

typedef unsigned int ui;

//just debugging, printing binary representation
void bin(uint64_t x)
{
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

template <class T> uint64_t double_hashing(const T&, uint32_t);
template <class T> class BFAdaptor;

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
				BFAdaptor <T> adaptor;

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

		public:
			BloomFilter(uint64_t my_capacity, uint32_t my_bpc, uint32_t my_cpe, 
						uint64_t (*my_hash_family)(const T&, uint32_t) = &double_hashing<T>):
				bits_per_counter(my_bpc),
				counters_per_cell(size_of_cell / bits_per_counter),
				counters_per_element(my_cpe),
				bits_per_element(bits_per_counter * counters_per_element),
				number_of_hashes(counters_per_element),
				mod(my_capacity),
				length((my_capacity + counters_per_cell - 1) / counters_per_cell),
				max_counter((1ull << bits_per_counter) - 1),
				data(new atomic <uint64_t>[length]),
				get_hash(my_hash_family)
				{
					fill(data, data + length, 0);
					if (get_hash == &double_hashing<T>)
						cout << "\nWARNING: hash family hasn't been provided!\nUsing double-hashing based on std::hash\n\n";
				}
			BloomFilter(uint64_t my_capacity, uint32_t my_bpc, uint32_t my_cpe, BFAdaptor<T> &my_adaptor):
                bits_per_counter(my_bpc),
                counters_per_cell(size_of_cell / bits_per_counter),
                counters_per_element(my_cpe),
                bits_per_element(bits_per_counter * counters_per_element),
                number_of_hashes(counters_per_element),
                mod(my_capacity),
                length((my_capacity + counters_per_cell - 1) / counters_per_cell),
                max_counter((1ull << bits_per_counter) - 1),
                data(new atomic <uint64_t>[length]),
                is_adaptor(true),
				adaptor(my_adaptor)
				{
                	fill(data, data + length, 0);
                }

			void insert(const T &value)
			{
				cerr << is_adaptor << "\n";
				for (uint32_t i = 0; i < number_of_hashes; i++)
				{
					uint64_t h;
					if (!is_adaptor)
						h = get_hash(value, i) % mod;
					else
					{
						pair <const void *, size_t> tmp = adaptor(value);
						h = CityHash64WithSeed((char *)tmp.first, tmp.second, i) % mod;
					}
					uint64_t index = get_index(h);				
					uint32_t offset = get_offset(h);
					uint64_t new_counter = increment(get_counter(data[index], offset));
					uint64_t value = data[index];
					cerr << "h = " << h << "\n";
					do
					{
						if (get_counter(value, offset) == new_counter)
							break;
						if (!(data[index].compare_exchange_weak(value, get_new_value(value, offset, new_counter))))
							value = data[index];
					}
					while (1);
				}
			}

			void erase(const T& value)
			{
				for (uint32_t i = 0; i < number_of_hashes; i++)
				{
					
                    uint64_t h;
                    if (!is_adaptor)
                        h = get_hash(value, i) % mod;
                    else
                    {
                        pair <const void *, size_t> tmp = adaptor(value);
                        h = CityHash64WithSeed((char *)tmp.first, tmp.second, i) % mod;
                    }
					uint64_t index = get_index(h);				
					uint32_t offset = get_offset(h);
					uint64_t new_counter = decrement(get_counter(data[index], offset));
					uint64_t value = data[index];
					do
					{
						if (get_counter(value, offset) == new_counter)
							break;
						if (!(data[index].compare_exchange_weak(value, get_new_value(value, offset, new_counter))))
							value = data[index];
					}
					while (1);
				}
			}

			uint64_t count(const T& value) const
			{
				uint64_t result = max_counter;
				for (uint32_t i = 0; i < number_of_hashes; i++)
				{

                    uint64_t h;
                    if (!is_adaptor)
                        h = get_hash(value, i) % mod;
                    else
                    {
                    	pair <const void *, size_t> tmp = adaptor(value);
                        h = CityHash64WithSeed((char *)tmp.first, tmp.second, i) % mod;
					}

   					uint64_t index = get_index(h);				
					uint32_t offset = get_offset(h);
					uint64_t counter = get_counter(data[index], offset);
					result = min(result, counter);
				}
				return result;
			} 
};

template <class T>
uint64_t double_hashing(const T& value, uint32_t i)
{
	//hash1 is std::hash and hash2 is just polynom with magic prime numbers
	return (hash<T>()(value) + 31013 * i * i + 1177711 * i);
}

uint64_t const_test(const char * const &value, uint32_t i)
{
	return CityHash64WithSeed(value, strlen(value), i);
}

uint64_t test(char * const &value, uint32_t i)
{
	return CityHash64WithSeed(value, strlen(value), i);
}

template <class T>
class BFAdaptor 
{
	public:
		pair <const void *, const size_t> operator()(const T &v) const 
		{
			return {v.data(), v.size()};
		}
};

int main()
{
//    freopen("cerr", "w", stderr);
	vector <int> v;	
	for (int i = 0; i < 10; i++)
		v.pb(5 * i * i + 6 * i - 36);
	for (int x : v) cout << x << " ";
	BFAdaptor <vector<int> > a;
//	pair <const void *, const size_t> tmp = a(v);
	BloomFilter <vector <int> > bf(1e3, 4, 3, a); 
	bf.insert(v);	
	cout << bf.count(v) << "\n";
	bf.erase(v);
	cout << bf.count(v) << "\n";
	return 0;
}
