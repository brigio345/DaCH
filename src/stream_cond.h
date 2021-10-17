#ifndef STREAM_COND_H
#define STREAM_COND_H

#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"

template <typename DATA_TYPE, int DEPTH, bool BUILD>
class stream_cond {};

template <typename DATA_TYPE, int DEPTH>
class stream_cond<DATA_TYPE, DEPTH, true> {
	private:
		hls::stream<DATA_TYPE, DEPTH> m_stream;

	public:
		void read(DATA_TYPE &data) {
#pragma HLS inline
			m_stream.read(data);
		}

		bool read_nb(DATA_TYPE &data) {
#pragma HLS inline
			return m_stream.read_nb(data);
		}

		void write(const DATA_TYPE &data) {
#pragma HLS inline
			m_stream.write(data);
		}
};

template <typename DATA_TYPE, int DEPTH>
class stream_cond<DATA_TYPE, DEPTH, false> {
	public:
		void read(DATA_TYPE &data) {
#pragma HLS inline
		}

		bool read_nb(DATA_TYPE &data) {
#pragma HLS inline
			return false;
		}

		void write(const DATA_TYPE &data) {
#pragma HLS inline
		}
};

#endif /* STREAM_COND_H */

