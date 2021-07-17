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
		const unsigned int m_addr_main;
		const unsigned int m_tag;
		const unsigned int m_set;
		const unsigned int m_off;
		unsigned int m_addr_cache;
		unsigned int m_addr_line;
		unsigned int m_way;

		address(const unsigned int addr_main):
			m_addr_main(addr_main),
			m_tag((addr_main >> (OFF_SIZE + SET_SIZE)) & TAG_MASK),
			m_set((addr_main >> OFF_SIZE) & SET_MASK),
			m_off(addr_main & OFF_MASK) {}

		address(const unsigned int tag, const unsigned int set,
				const unsigned int off, const unsigned int way):
				m_addr_main((tag << (SET_SIZE + OFF_SIZE)) |
						(set << OFF_SIZE) | off),
				m_tag(tag), m_set(set), m_off(off) {
			set_way(way);
		}

		void set_way(const unsigned int way) {
			m_way = way;
			m_addr_line = ((m_set << WAY_SIZE) | way);
			m_addr_cache = ((m_set << (WAY_SIZE + OFF_SIZE)) |
					(m_way << OFF_SIZE) | m_off);
		}
};

#endif /* ADDRESS_H */

