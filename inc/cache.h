#ifndef CACHE_H
#define CACHE_H

#include "ap_int.h"
#include "streamedArray.h"

// direct mapping, write back
// TODO: support different policies through virtual functions
// TODO: use more friendly template parameters:
// 	LINE_SIZE -> N_LINES; TAG_SIZE -> CACHE_LINE_SIZE
template <typename T, size_t ADDR_SIZE = 32, size_t TAG_SIZE = 28, size_t LINE_SIZE = 2>
class cache {
	private:
		static const size_t OFF_SIZE = ADDR_SIZE - (TAG_SIZE + LINE_SIZE);
		static const size_t N_LINES = 1 << LINE_SIZE;
		static const size_t N_ENTRIES_PER_LINE = 1 << OFF_SIZE;

		std::string _name;
		streamedArray<T> &_arr; // TODO: replace with memory pointer + directly override [] operator of cache??
		bool _valid[N_LINES] = {false};
		bool _dirty[N_LINES];
		ap_int<TAG_SIZE> _tag[N_LINES];
		T _cache_mem[N_LINES * N_ENTRIES_PER_LINE];

	public:
		cache(std::string name, streamedArray<T> &arr): _name(name), _arr(arr) {}

		~cache() {
			flush();
		}

		void read() {
			ap_int<ADDR_SIZE> addr_main;
			T data;

			while (1) {
				// get address to be read to
				addr_main = _arr.rdAddr.read();
				// stop if address is "end-of-request"
				if (addr_main == -1)
					break;

				// extract information from address
				address addr = address(addr_main);

				// prepare the cache for the access
				prepare(addr);

				// read data from cache
				data = _cache_mem[addr._addr_cache];

				// send read data
				_arr.rdData.write(data);
			}
		}

		void write() {
			ap_int<ADDR_SIZE> addr_main;

			while (1) {
				// get address to be written to
				addr_main = _arr.wrAddr.read();
				
				// stop if address is "end-of-request"
				if (addr_main == -1)
					break;

				// extract information from address
				address addr = address(addr_main);

				// prepare the cache for the access
				prepare(addr);
				
				// store received data to cache
				T data = _arr.wrData.read();
				_cache_mem[addr._addr_cache] = data;

				_dirty[addr._line] = true;
			}
		}

		void stopRead() {
			_arr.rdAddr.write(-1);
		}

		void stopWrite() {
			_arr.wrAddr.write(-1);
		}

		// store all valid dirty lines from cache to main memory
		void flush() {
			for (int line = 0; line < N_LINES; line++) {
				if (_valid[line] && _dirty[line])
					spill(address::build(_tag[line], line, 0));
			}
		}

	private:
		class address {
			private:
				static const unsigned int TAG_HIGH = ADDR_SIZE - 1;
				static const unsigned int TAG_LOW = TAG_HIGH - TAG_SIZE + 1;
				static const unsigned int LINE_HIGH = TAG_LOW - 1;
				static const unsigned int LINE_LOW = LINE_HIGH - LINE_SIZE + 1;
				static const unsigned int OFF_HIGH = LINE_LOW - 1;
				static const unsigned int OFF_LOW = 0;

			public:
				ap_uint<ADDR_SIZE> _addr_main;
				ap_uint<ADDR_SIZE> _addr_main_first_of_line;
				ap_uint<ADDR_SIZE> _addr_cache;
				ap_uint<ADDR_SIZE> _addr_cache_first_of_line;
				ap_uint<TAG_SIZE> _tag;
				ap_uint<LINE_SIZE> _line;
				ap_uint<OFF_SIZE> _off;

				address(ap_uint<ADDR_SIZE> addr_main): _addr_main(addr_main) {
					_tag = addr_main.range(TAG_HIGH, TAG_LOW);
					_line = addr_main.range(LINE_HIGH, LINE_LOW);
					_off = addr_main.range(OFF_HIGH, OFF_LOW);
					_addr_cache = _line * N_ENTRIES_PER_LINE + _off;
					_addr_cache_first_of_line = _addr_cache - _off;
					_addr_main_first_of_line = addr_main - _off;
				}

				static address build(ap_uint<TAG_SIZE> tag,
						ap_uint<LINE_SIZE> line,
						ap_uint<OFF_SIZE> off = 0) {
					return address((tag, line, off));
				}
		};

		inline bool hit(address addr) {
			return (_valid[addr._line] && (addr._tag == _tag[addr._line]));
		}

		// load line from main to cache memory
		// (taking care of writing back dirty lines)
		void fill(address addr) {
			if (_valid[addr._line] && _dirty[addr._line])
				spill(address::build(_tag[addr._line], addr._line));

			for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
				_cache_mem[addr._addr_cache_first_of_line + off] =
					_arr.arr[addr._addr_main_first_of_line + off];
			}

			_tag[addr._line] = addr._tag;
			_valid[addr._line] = true;
			_dirty[addr._line] = false;
		}

		// store line from cache to main memory
		void spill(address addr) {
			for (int off = 0; off < N_ENTRIES_PER_LINE; off++) {
				_arr.arr[addr._addr_main_first_of_line + off] =
					_cache_mem[addr._addr_cache_first_of_line + off];
			}

			_dirty[addr._line] = false;
		}

		// prepare the cache for accessing addr
		// (load the line if not present)
		void prepare(address addr) {
			if (!hit(addr))
				fill(addr);
		}
};

#endif /* CACHE_H */

