#ifndef ADDRESS_H
#define ADDRESS_H

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t SET_SIZE, size_t WAY_SIZE>
class address {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - (TAG_SIZE + SET_SIZE));
		static const unsigned int OFF_MASK = (~(-1U << OFF_SIZE));
		static const unsigned int SET_MASK = (~(-1U << SET_SIZE));
		static const unsigned int TAG_MASK = (~(-1U << TAG_SIZE));

	public:
		unsigned int _addr_main;
		unsigned int _addr_cache;
		unsigned int _addr_line;
		unsigned int _tag;
		unsigned int _set;
		unsigned int _off;
		unsigned int _way;

		address(unsigned int addr_main): _addr_main(addr_main) {
			_off = (addr_main & OFF_MASK);
			_set = ((addr_main >> OFF_SIZE) & SET_MASK);
			_tag = ((addr_main >> (OFF_SIZE + SET_SIZE)) & TAG_MASK);
		}

		address(unsigned int tag, unsigned int set,
				unsigned int off, unsigned int way):
				_tag(tag), _set(set), _off(off) {
			tag = (_tag << (SET_SIZE + OFF_SIZE));
			set = (_set << OFF_SIZE);

			_addr_main = (tag | set | _off);

			set_way(way);
		}

		void set_way(unsigned int way) {
			_way = way;

			auto set = (_set << WAY_SIZE);

			_addr_line = (set | _way);

			set = (_set << (WAY_SIZE + OFF_SIZE));
			way = (_way << OFF_SIZE);

			_addr_cache = (set | way | _off);
		}
};

#endif /* ADDRESS_H */

