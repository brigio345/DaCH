#ifndef L1_CACHE_H
#define L1_CACHE_H

#include "address.h"

template <typename line_t, size_t ADDR_SIZE, size_t TAG_SIZE, size_t N_ENTRIES_PER_LINE>
class l1_cache {
	private:
		typedef address<ADDR_SIZE, TAG_SIZE, 0, 0> l1_addr_t;

		bool m_valid;
		line_t m_line;
		unsigned int m_tag;

	public:
		void init() {
			m_valid = false;
		}

		bool get_line(unsigned int addr_main, line_t &line) {
			l1_addr_t addr(addr_main);

			for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
				line[off] = m_line[off];

			return hit(addr);
		}

		void set_line(unsigned int addr_main, line_t &line) {
			for (auto off = 0; off < N_ENTRIES_PER_LINE; off++)
				m_line[off] = line[off];
			l1_addr_t addr(addr_main);
			m_valid = true;
			m_tag = addr.m_tag;
		}

		void invalidate_line(unsigned int addr_main) {
			l1_addr_t addr(addr_main);

			if (hit(addr))
				m_valid = false;
		}

	private:
		inline bool hit(l1_addr_t addr) {
			return (m_valid && (addr.m_tag == m_tag));
		}
};

#endif /* L1_CACHE_H */

