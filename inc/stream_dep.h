#ifndef STREAM_DEP_H
#define STREAM_DEP_H

#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"

template <typename T>
class stream_dep {
	private:
		hls::stream<T> _stream;

	public:
		void read(T &data) {
			data = _stream.read();
		}

		void write(T data) {
			_stream.write(data);
		}

		bool read_dep(T &data, volatile bool dep) {
#pragma HLS inline off
			data = _stream.read();
			return dep;
		}

		bool write_dep(T data, volatile bool dep) {
#pragma HLS inline off
			_stream.write(data);
			return dep;
		}
};

#endif /* STREAM_DEP_H */

