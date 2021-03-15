#ifndef STREAM_DEP_H
#define STREAM_DEP_H

#define HLS_STREAM_THREAD_SAFE
#include "hls_stream.h"

template <typename T>
class stream_dep {
	private:
		hls::stream<T> _stream;

	public:
		bool read(T &data, volatile bool dep) {
			data = _stream.read();
			return dep;
		}

		bool write(T data, volatile bool dep) {
			_stream.write(data);
			return dep;
		}
};

#endif /* STREAM_DEP_H */

