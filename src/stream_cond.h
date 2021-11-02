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

		bool read_dep(DATA_TYPE &data, volatile bool dep) {
#pragma HLS inline
			return m_stream.read_dep(data, dep);
		}

		void write(const DATA_TYPE &data) {
#pragma HLS inline
			m_stream.write(data);
		}

		bool write_dep(const DATA_TYPE &data, volatile bool dep) {
#pragma HLS inline
			return m_stream.write_dep(data, dep);
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

		bool read_dep(DATA_TYPE &data, volatile bool dep) {
#pragma HLS inline
			return false;
		}

		void write(const DATA_TYPE &data) {
#pragma HLS inline
		}

		bool write_dep(const DATA_TYPE &data, volatile bool dep) {
#pragma HLS inline
			return false;
		}
};

#endif /* STREAM_COND_H */

