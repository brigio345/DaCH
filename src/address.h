#ifndef ADDRESS_H
#define ADDRESS_H

template <size_t ADDR_SIZE, size_t TAG_SIZE, size_t SET_SIZE, size_t WAY_SIZE>
class address {
	private:
		static const size_t OFF_SIZE = (ADDR_SIZE - (TAG_SIZE + SET_SIZE));

	public:
		unsigned int m_addr_main;
		unsigned int m_addr_cache;
		unsigned int m_addr_line;
		unsigned int m_tag;
		unsigned int m_set;
		unsigned int m_off;
		unsigned int m_way;

		address(unsigned int addr_main): m_addr_main(addr_main) {
			unsigned int off_mask = (~(-1U << OFF_SIZE));
			unsigned int set_mask = (~(-1U << SET_SIZE));
			unsigned int tag_mask = (~(-1U << TAG_SIZE));

			m_off = (addr_main & off_mask);
			m_set = ((addr_main >> OFF_SIZE) & set_mask);
			m_tag = ((addr_main >> (OFF_SIZE + SET_SIZE)) & tag_mask);
		}

		address(unsigned int tag, unsigned int set,
				unsigned int off, unsigned int way):
				m_tag(tag), m_set(set), m_off(off) {
			tag = (m_tag << (SET_SIZE + OFF_SIZE));
			set = (m_set << OFF_SIZE);

			m_addr_main = (tag | set | m_off);

			set_way(way);
		}

		void set_way(unsigned int way) {
			m_way = way;

			auto set = (m_set << WAY_SIZE);

			m_addr_line = (set | m_way);

			set = (m_set << (WAY_SIZE + OFF_SIZE));
			way = (m_way << OFF_SIZE);

			m_addr_cache = (set | way | m_off);
		}
};

#endif /* ADDRESS_H */

