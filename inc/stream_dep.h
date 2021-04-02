#ifndef STREAM_DEP_H
#define STREAM_DEP_H

#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"

template <typename T, size_t DEPTH = 1>
class stream_dep {
	private:
		hls::stream<T> _stream;

	public:
		stream_dep() {
#pragma HLS stream depth=DEPTH variable=_stream
		}

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

