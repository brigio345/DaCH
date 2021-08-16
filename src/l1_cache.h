#ifndef L1_CACHE_H
#define L1_CACHE_H

#include "address.h"
#include "utils.h"
#include "ap_int.h"
#ifdef __SYNTHESIS__
#include "hls_vector.h"
#else
#include <array>
#endif /* __SYNTHESIS__ */

template <typename T, size_t ADDR_SIZE, size_t N_LINES, size_t N_ENTRIES_PER_LINE>
class l1_cache {
	private:
		static const size_t SET_SIZE = utils::log2_ceil(N_LINES);
		static const size_t OFF_SIZE = utils::log2_ceil(N_ENTRIES_PER_LINE);
		static const size_t TAG_SIZE = (ADDR_SIZE - (SET_SIZE + OFF_SIZE));

#ifdef __SYNTHESIS__
		template <typename TYPE, size_t SIZE>
			using array_type = hls::vector<TYPE, SIZE>;
#else
		template <typename TYPE, size_t SIZE>
			using array_type = std::array<TYPE, SIZE>;
#endif /* __SYNTHESIS__ */

		typedef array_type<T, N_ENTRIES_PER_LINE> line_type;
		typedef address<ADDR_SIZE, TAG_SIZE, SET_SIZE, 0> addr_type;

		ap_uint<TAG_SIZE> m_tag[N_LINES];
		bool m_valid[N_LINES];
		line_type m_cache_mem[N_LINES];

	public:
		l1_cache() {
#pragma HLS array_partition variable=m_tag complete dim=1
#pragma HLS array_partition variable=m_valid complete dim=1
#pragma HLS array_partition variable=m_cache_mem complete dim=1
		}

		void init() {
			for (auto line = 0; line < N_LINES; line++)
				m_valid[line] = false;
		}

		void get_line(const ap_uint<ADDR_SIZE> addr_main, line_type &line) const {
			const addr_type addr(addr_main);

			line = m_cache_mem[addr.m_set];
		}

		void set_line(const ap_uint<ADDR_SIZE> addr_main, const line_type &line) {
			const addr_type addr(addr_main);
			m_cache_mem[addr.m_set] = line;
			m_valid[addr.m_set] = true;
			m_tag[addr.m_set] = addr.m_tag;
		}

		void notify_write(const ap_uint<ADDR_SIZE> addr_main) {
			const addr_type addr(addr_main);

			if (hit(addr))
				m_valid[addr.m_set] = false;
		}

		inline bool hit(const ap_uint<ADDR_SIZE> &addr_main) const {
			const addr_type addr(addr_main);

			return (m_valid[addr.m_set] &&
					(addr.m_tag == m_tag[addr.m_set]));
		}
};

#endif /* L1_CACHE_H */

