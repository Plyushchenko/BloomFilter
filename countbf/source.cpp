#include <stdio.h>
#include <iostream>
#include <city.h>
#include <string.h>
#include <vector>
#include <atomic>
#include <algorithm>
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
class BloomFilter
{
	private:
		uint32_t
					bits_per_counter, //should be divisor of size_of_cell (which is 64)
					counters_per_cell,
					counters_per_element,
					bits_per_element,
					number_of_hashes,
					capacity,
					mod,
					length;
				uint64_t max_counter;
                atomic <uint64_t> *data;

				int32_t get_hash(const char *s, uint32_t i) const
				{
					return CityHash64WithSeed(s, strlen(s), i) % mod;
//					return ((1234 + i) * s[0]) % mod;
				}
				uint32_t get_index(uint32_t x) const
				{
					return x / counters_per_cell;
				}
				uint32_t get_offset(uint32_t x) const
				{
					return ((x % counters_per_cell) + 1) * bits_per_counter;
				}
				uint64_t get_counter(uint64_t value, uint32_t offset) const
				{
    				return (value >> (size_of_cell - offset)) & max_counter;
				}
				uint64_t increment(uint64_t x)
				{
					return x += (x != (1ull << bits_per_counter));
				}   	
				uint64_t decrement(uint64_t x)
				{
					return x -= (x != 0);
				}
				uint64_t get_new_value(uint64_t value, uint32_t offset, uint64_t new_counter)
				{
					uint32_t shift = size_of_cell - offset + bits_per_counter;
					uint64_t old = (shift == size_of_cell) ? 0 : ((value >> shift) << shift);
					return old | (new_counter << (shift - bits_per_counter)) | value % (1ull << (shift - bits_per_counter));
				}
		public:
			BloomFilter(uint32_t my_capacity, uint32_t my_bpc, uint32_t my_cpe):
				bits_per_counter(my_bpc),
				counters_per_cell(size_of_cell / bits_per_counter),
				counters_per_element(my_cpe),
				bits_per_element(bits_per_counter * counters_per_element),
				number_of_hashes(counters_per_element),
				mod(my_capacity),
				length((my_capacity + counters_per_cell - 1) / counters_per_cell),
				max_counter((1ull << bits_per_counter) - 1),
				data(new atomic <uint64_t>[length]){fill(data, data + length, 0);}//{cerr << "length = " << length << "\n";}

			void insert(const char *s)
			{
				for (uint32_t i = 0; i < number_of_hashes; i++)
				{
					uint32_t h = get_hash(s, i);
					uint32_t index = get_index(h);				
					uint32_t offset = get_offset(h);
					uint64_t new_counter = increment(get_counter(data[index], offset));
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
			void erase(const char *s)
			{
				for (uint32_t i = 0; i < number_of_hashes; i++)
				{
					uint32_t h = get_hash(s, i);
					uint32_t index = get_index(h);				
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
			uint64_t count(const char *s) const
			{
				uint64_t result = 1 << bits_per_counter;
				for (uint32_t i = 0; i < number_of_hashes; i++)
				{
					uint32_t h = get_hash(s, i);
   					uint32_t index = get_index(h);				
					uint32_t offset = get_offset(h);
					uint64_t counter = get_counter(data[index], offset);

					result = min(result, counter);
				}
				return result;
			} 
			
};
int main()
{
    freopen("cerr", "w", stderr);
	BloomFilter bf(1e3, 4, 3);
	for (int i = 0; i < 26; i++)
	{
	    char s[1];
	    s[0] = i + 'a';
	    cerr << i << " " << s[0] << "\n";
	    cout << i << "\n";
	    bf.insert(s);
	}
	for (int i = 0; i < 26; i++)
	{
	    char s[1];
	    s[0] = i + 'a';
	    cerr << i << " " << s[0] << "\n";
	    cout << i << "\n";
	    bf.erase(s);
	}
		
	for (int i = 0; i < 26; i++)
	{
	    char s[1];
	    s[0] = i + 'a';
	    cerr << i << " " << s[0] << "\n";
	    cout << bf.count(s) << "\n";
	}	

//	cerr << bf.count("a");
}
