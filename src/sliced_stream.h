#ifndef SLICED_STREAM_H
#define SLICED_STREAM_H

#include <hls_stream.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wpedantic"
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-label"

template <typename DATA_TYPE, size_t LINE_SIZE, size_t STREAM_DEPTH,
	 size_t MAX_SLICE_BW = 4096>
class sliced_stream {
	private:
		static const size_t MAX_SLICE_SIZE =
			(MAX_SLICE_BW / (sizeof(DATA_TYPE) * 8));
		static const size_t N_SLICES =
			((LINE_SIZE / MAX_SLICE_SIZE) > 0) ?
			(LINE_SIZE / MAX_SLICE_SIZE) : 1;
		static const size_t SLICE_SIZE = (LINE_SIZE / N_SLICES);
		static const size_t SLICE_BYTES =
			(SLICE_SIZE * sizeof(DATA_TYPE));

		typedef DATA_TYPE line_type[LINE_SIZE];
		typedef DATA_TYPE slice_type[SLICE_SIZE];
		typedef struct {
			slice_type payload;
		} slice_pack_type;

		hls::stream<slice_pack_type, STREAM_DEPTH> m_stream[N_SLICES];

	public:
		sliced_stream() {
#pragma HLS array_partition variable=m_stream type=complete dim=0
		}

		bool read_dep(line_type line, volatile bool dep) {
#pragma HLS inline off
			read(line);
			return dep;
		}

		void read(line_type line) {
#pragma HLS inline
			for (size_t slice = 0; slice < N_SLICES; slice++) {
				slice_pack_type slice_buff = m_stream[slice].read();
				for (size_t off = 0; off < SLICE_SIZE; off++) {
#pragma HLS unroll
					line[(slice * SLICE_SIZE) + off] =
						slice_buff.payload[off];
				}
			}
		}

		bool write_dep(const line_type line, volatile bool dep) {
#pragma HLS inline off
			write(line);
			return dep;
		}

		void write(const line_type line) {
#pragma HLS inline
			for (size_t slice = 0; slice < N_SLICES; slice++) {
				slice_pack_type slice_buff;
				for (size_t off = 0; off < SLICE_SIZE; off++) {
#pragma HLS unroll
					slice_buff.payload[off] =
						line[(slice * SLICE_SIZE) + off];
				}
				m_stream[slice].write(slice_buff);
			}
		}
};

#pragma GCC diagnostic pop

#endif /* SLICED_STREAM_H */

