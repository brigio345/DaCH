#include <iostream>
#include <cstring>
#include <stdlib.h>
#define PROFILE
#include "cache.h"

#define CACHE
//#define BASELINE
//#define MANUAL

static const size_t WIDTH = 64;
static const size_t HEIGHT = 64;
static const size_t FILTER_V_SIZE = 3;
static const size_t FILTER_H_SIZE = 3;

static const size_t WIDTH_PADDED = (utils::ceil(WIDTH / 16.0) * 16);
static const size_t SIZE = (WIDTH * HEIGHT);
static const size_t SIZE_PADDED_CACHE = (1 << utils::log2_ceil(SIZE));
static const size_t SIZE_PADDED_MANUAL = (WIDTH_PADDED * HEIGHT);
static const size_t FILTER_SIZE = (FILTER_V_SIZE * FILTER_H_SIZE);
static const size_t FILTER_V_SIZE_PADDED = (1 << utils::log2_ceil(FILTER_V_SIZE));
static const size_t FILTER_H_SIZE_PADDED = (1 << utils::log2_ceil(FILTER_H_SIZE));
static const size_t FILTER_SIZE_PADDED = (1 << utils::log2_ceil(FILTER_H_SIZE_PADDED * FILTER_V_SIZE_PADDED));

#ifndef UNROLL
#define UNROLL 2
#endif

static const size_t RD_PORTS = UNROLL;

#ifndef COEFF_WORDS
#define COEFF_WORDS FILTER_H_SIZE_PADDED
#endif /* COEFF_WORDS */
#ifndef COEFF_L2_SETS
#define COEFF_L2_SETS 1
#endif /* COEFF_L2_SETS */
#ifndef COEFF_L2_WAYS
#define COEFF_L2_WAYS 1
#endif /* COEFF_L2_WAYS */
#ifndef COEFF_L2_LATENCY
#define COEFF_L2_LATENCY 3
#endif /* COEFF_L2_LATENCY */
#ifndef COEFF_L1_SETS
#define COEFF_L1_SETS FILTER_V_SIZE_PADDED
#endif /* COEFF_L1_SETS */
#ifndef COEFF_L1_WAYS
#define COEFF_L1_WAYS 1
#endif /* COEFF_L1_WAYS */

#ifndef SRC_WORDS
#define SRC_WORDS FILTER_SIZE_PADDED
#endif /* SRC_WORDS */
#ifndef SRC_L2_SETS
#define SRC_L2_SETS 1
#endif /* SRC_L2_SETS */
#ifndef SRC_L2_WAYS
#define SRC_L2_WAYS 1
#endif /* SRC_L2_WAYS */
#ifndef SRC_L2_LATENCY
#define SRC_L2_LATENCY 3
#endif /* SRC_L2_LATENCY */
#ifndef SRC_L1_SETS
#define SRC_L1_SETS 1
#endif /* SRC_L1_SETS */
#ifndef SRC_L1_WAYS
#define SRC_L1_WAYS 1
#endif /* SRC_L1_WAYS */

#ifndef DST_WORDS
#define DST_WORDS FILTER_SIZE_PADDED
#endif /* DST_WORDS */
#ifndef DST_L2_SETS
#define DST_L2_SETS 1
#endif /* DST_L2_SETS */
#ifndef DST_L2_WAYS
#define DST_L2_WAYS 1
#endif /* DST_L2_WAYS */
#ifndef DST_L2_LATENCY
#define DST_L2_LATENCY 3
#endif /* DST_L2_LATENCY */

typedef cache<char, true, false, RD_PORTS, FILTER_SIZE_PADDED, COEFF_L2_SETS,
	COEFF_L2_WAYS, COEFF_WORDS, false, COEFF_L1_SETS, COEFF_L1_WAYS, false,
	COEFF_L2_LATENCY> cache_coeff;
typedef cache<unsigned char, true, false, RD_PORTS, SIZE_PADDED_CACHE,
	SRC_L2_SETS, SRC_L2_WAYS, SRC_WORDS, false, SRC_L1_SETS, SRC_L1_WAYS,
	false, SRC_L2_LATENCY> cache_src;
typedef cache<unsigned char, false, true, 1, SIZE_PADDED_CACHE, DST_L2_SETS,
	DST_L2_WAYS, DST_WORDS, false, 0, 0, false, DST_L2_LATENCY> cache_dst;

template <typename FILTER_TYPE, typename SRC_TYPE, typename DST_TYPE>
void convolution(FILTER_TYPE &coeffs, SRC_TYPE &src, DST_TYPE &dst) {
	for(int y=0; y<HEIGHT; ++y) {
		for(int x=0; x<WIDTH; ++x) {
			// Apply 2D filter to the pixel window
			int sum = 0;
			for(int row=0; row<FILTER_V_SIZE; row++) {
				for(int col=0; col<FILTER_H_SIZE; col++) {
					unsigned char pixel;
					int xoffset = (x+col-(FILTER_H_SIZE/2));
					int yoffset = (y+row-(FILTER_V_SIZE/2));
					// Deal with boundary conditions : clamp pixels to 0 when outside of image 
					if ( (xoffset<0) || (xoffset>=WIDTH) || (yoffset<0) || (yoffset>=HEIGHT) ) {
						pixel = 0;
					} else {
						pixel = src[yoffset*WIDTH+xoffset];
					}
					sum += pixel*coeffs[row * FILTER_H_SIZE + col];
				}
			}

			// Write output
			dst[y*WIDTH+x] = (unsigned char)sum;
		}
	}
}

template<>
void convolution(cache_coeff &coeffs, cache_src &src, cache_dst &dst) {
Y:	for(int y=0; y<HEIGHT; ++y) {
X:		for(int x=0; x<WIDTH; ++x) {
			// Apply 2D filter to the pixel window
			int sum = 0;
ROW:			for(int row=0; row<FILTER_V_SIZE; row += RD_PORTS) {
COL:				for(int col=0; col<FILTER_H_SIZE; col++) {
#pragma HLS pipeline II=1
PORT:					for (auto port = 0; port < RD_PORTS; port++) {
						int tmp = 0;
						if ((row + port) < FILTER_V_SIZE) {
							unsigned char pixel;
							int xoffset = (x+col-(FILTER_H_SIZE/2));
							int yoffset = (y+row+port-(FILTER_V_SIZE/2));
							// Deal with boundary conditions : clamp pixels to 0 when outside of image 
							if ( (xoffset<0) || (xoffset>=WIDTH) || (yoffset<0) || (yoffset>=HEIGHT) ) {
								pixel = 0;
							} else {
								pixel = src.get(yoffset*WIDTH+xoffset, port);
							}
							tmp = pixel*coeffs.get((row+port) * FILTER_H_SIZE + col, port);
						}
						sum += tmp;
					}
				}
			}

			// Write output
			dst[y*WIDTH+x] = (unsigned char)sum;
		}
	}
}

void ReadFromMem(
        const char          *coeffs,
        hls::stream<char>   &coeff_stream,
        const unsigned char *src,
        hls::stream<unsigned char>     &pixel_stream )
{
    unsigned num_coefs = FILTER_V_SIZE*FILTER_H_SIZE;
    unsigned num_coefs_padded = (((num_coefs-1)/16)+1)*16; // Make sure number of reads of multiple of 16, enables auto-widening
    read_coefs: for (int i=0; i<num_coefs_padded; i++) {
        unsigned char coef = coeffs[i];
        if (i<num_coefs) coeff_stream.write( coef );        
    }

    unsigned offset = 0;
    unsigned x = 0;
    read_image: for (int n = 0; n < HEIGHT*WIDTH_PADDED; n++) {
        unsigned char pix = src[n];
        if (x<WIDTH) pixel_stream.write( pix );
        if (x==(WIDTH_PADDED-1)) x=0; else x++;
     }
}


void WriteToMem(
        hls::stream<unsigned char>     &pixel_stream,
        unsigned char       *dst)
{
    const auto stride = (WIDTH_PADDED/16)*16; // Makes compiler see that stride is a multiple of 16, enables auto-widening
    unsigned offset = 0;
    unsigned x = 0;
    write_image: for (int n = 0; n < HEIGHT*WIDTH_PADDED; n++) {
        unsigned char pix = (x<WIDTH) ? pixel_stream.read() : 0;
        dst[n] = pix;
        if (x==(WIDTH_PADDED-1)) x=0; else x++;
    }    
}


struct window {
    unsigned char pix[FILTER_V_SIZE][FILTER_H_SIZE];
};


void Window2D(
        hls::stream<unsigned char>      &pixel_stream,
        hls::stream<window>  &window_stream)
{
    // Line buffers - used to store [FILTER_V_SIZE-1] entire lines of pixels
    unsigned char LineBuffer[FILTER_V_SIZE-1][WIDTH_PADDED];  
#pragma HLS ARRAY_PARTITION variable=LineBuffer dim=1 complete
#pragma HLS DEPENDENCE variable=LineBuffer inter false
#pragma HLS DEPENDENCE variable=LineBuffer intra false

    // Sliding window of [FILTER_V_SIZE][FILTER_H_SIZE] pixels
    window Window;

    unsigned col_ptr = 0;
    unsigned ramp_up = WIDTH*((FILTER_V_SIZE-1)/2)+(FILTER_H_SIZE-1)/2;
    unsigned num_pixels = WIDTH*HEIGHT;
    unsigned num_iterations = num_pixels + ramp_up;

    const unsigned max_iterations = WIDTH * HEIGHT + WIDTH*((FILTER_V_SIZE-1)/2)+(FILTER_H_SIZE-1)/2;

    // Iterate until all pixels have been processed
    update_window: for (int n=0; n<num_iterations; n++)
    {
#pragma HLS LOOP_TRIPCOUNT max=max_iterations
#pragma HLS PIPELINE II=1

        // Read a new pixel from the input stream
        unsigned char new_pixel = (n<num_pixels) ? pixel_stream.read() : 0;

        // Shift the window and add a column of new pixels from the line buffer
        for(int i = 0; i < FILTER_V_SIZE; i++) {
            for(int j = 0; j < FILTER_H_SIZE-1; j++) {
                Window.pix[i][j] = Window.pix[i][j+1];
            }
            Window.pix[i][FILTER_H_SIZE-1] = (i<FILTER_V_SIZE-1) ? LineBuffer[i][col_ptr] : new_pixel;
        }

        // Shift pixels in the column of pixels in the line buffer, add the newest pixel
        for(int i = 0; i < FILTER_V_SIZE-2; i++) {
            LineBuffer[i][col_ptr] = LineBuffer[i+1][col_ptr];
        }
        LineBuffer[FILTER_V_SIZE-2][col_ptr] = new_pixel;

        // Update the line buffer column pointer
        if (col_ptr==(WIDTH-1)) {
            col_ptr = 0;
        } else {
            col_ptr++;
        }

        // Write output only when enough pixels have been read the buffers and ramped-up
        if (n>=ramp_up) {
            window_stream.write(Window);
        }

    }
}

void Filter2D(
        hls::stream<char>   &coeff_stream,
        hls::stream<window> &window_stream,
		hls::stream<unsigned char>     &pixel_stream )
{
	// Filtering coefficients
	char coeffs[FILTER_V_SIZE * FILTER_H_SIZE];
#pragma HLS ARRAY_PARTITION variable=coeffs complete dim=0

	// Load the coefficients into local storage
load_coefs: for (int i=0; i<FILTER_V_SIZE*FILTER_H_SIZE; i++) {
#pragma HLS PIPELINE II=1
			    coeffs[i] = coeff_stream.read();
	    }

	    // Process the incoming stream of pixel windows
apply_filter: for (int y = 0; y < HEIGHT; y++) 
	      {
		      for (int x = 0; x < WIDTH; x++) 
		      {
#pragma HLS PIPELINE II=1
			      // Read a 2D window of pixels
			      window w = window_stream.read();

			      // Apply filter to the 2D window
			      int sum = 0;
			      for(int row=0; row<FILTER_V_SIZE; row++) 
			      {
				      for(int col=0; col<FILTER_H_SIZE; col++) 
				      {
					      unsigned char pixel;
					      int xoffset = (x+col-(FILTER_H_SIZE/2));
					      int yoffset = (y+row-(FILTER_V_SIZE/2));
					      // Deal with boundary conditions : clamp pixels to 0 when outside of image 
					      if ( (xoffset<0) || (xoffset>=WIDTH) || (yoffset<0) || (yoffset>=HEIGHT) ) {
						      pixel = 0;
					      } else {
						      pixel = w.pix[row][col];
					      }
					      sum += pixel*(char)coeffs[row * FILTER_H_SIZE + col];
				      }
			      }

			      // Write the output pixel
			      pixel_stream.write((unsigned char)sum);
		      }
	      }
}


extern "C" {

	void Filter2DKernel(
			const char           coeffs[256],
			const unsigned char  *src,
			unsigned char        *dst)
	{

#pragma HLS DATAFLOW

		// Stream of pixels from kernel input to filter, and from filter to output
		hls::stream<char,2>    coefs_stream;
		hls::stream<unsigned char,2>      pixel_stream;
		hls::stream<window,3>  window_stream; // Set FIFO depth to 0 to minimize resources
		hls::stream<unsigned char,16>     output_stream;

		// Read image data from global memory over AXI4 MM, and stream pixels out
		ReadFromMem(coeffs, coefs_stream, src, pixel_stream);

		// Read incoming pixels and form valid HxV windows
		Window2D(pixel_stream, window_stream);

		// Process incoming stream of pixels, and stream pixels out
		Filter2D(coefs_stream, window_stream, output_stream);

		// Write incoming stream of pixels and write them to global memory over AXI4 MM
		WriteToMem(output_stream, dst);

	}

}

extern "C" void conv2d_top(char *coeffs, unsigned char *src, unsigned char *dst) {
#pragma HLS INTERFACE m_axi port=coeffs offset=slave bundle=gmem0 depth=FILTER_SIZE_PADDED
#pragma HLS INTERFACE m_axi port=src offset=slave bundle=gmem1 depth=SIZE_PADDED_CACHE
#pragma HLS interface m_axi port=dst offset=slave bundle=gmem2 depth=SIZE_PADDED_CACHE
#pragma HLS INTERFACE ap_ctrl_hs port=return

#if defined(CACHE)
#pragma HLS dataflow disable_start_propagation
	cache_coeff coeffs_cache(coeffs);
	cache_src src_cache(src);
	cache_dst dst_cache(dst);

	cache_wrapper(convolution<cache_coeff, cache_src, cache_dst>,
			coeffs_cache, src_cache, dst_cache);

#ifndef __SYNTHESIS__
	printf("coeffs hit ratio = \n");
	for (auto port = 0; port < RD_PORTS; port++) {
		printf("\tP=%d: L1=%d/%d; L2=%d/%d\n", port,
				coeffs_cache.get_n_l1_hits(port), coeffs_cache.get_n_l1_reqs(port),
				coeffs_cache.get_n_hits(port), coeffs_cache.get_n_reqs(port));
	}
	printf("src hit ratio = \n");
	for (auto port = 0; port < RD_PORTS; port++) {
		printf("\tP=%d: L1=%d/%d; L2=%d/%d\n", port,
				src_cache.get_n_l1_hits(port), src_cache.get_n_l1_reqs(port),
				src_cache.get_n_hits(port), src_cache.get_n_reqs(port));
	}
	printf("dst hit ratio = L1=%d/%d; L2=%d/%d\n",
			dst_cache.get_n_l1_hits(0), dst_cache.get_n_l1_reqs(0),
			dst_cache.get_n_hits(0), dst_cache.get_n_reqs(0));
#endif	/* __SYNTHESIS__ */
#elif defined(BASELINE)
	convolution(coeffs, src, dst);
#elif defined(MANUAL)
	Filter2DKernel(coeffs, src, dst);
#endif /* CACHE */
}

int main() {
	char *coeffs;
	unsigned char *src;
	unsigned char *dst;
	unsigned char *src_ref;
	unsigned char *dst_ref;

	coeffs = (char *)malloc(FILTER_SIZE_PADDED * sizeof(char));
#ifndef MANUAL
	src = (unsigned char *)malloc(SIZE_PADDED_CACHE * sizeof(unsigned char));
	dst = (unsigned char *)malloc(SIZE_PADDED_CACHE * sizeof(unsigned char));
#else
	src = (unsigned char *)malloc(SIZE_PADDED_MANUAL * sizeof(unsigned char));
	dst = (unsigned char *)malloc(SIZE_PADDED_MANUAL * sizeof(unsigned char));
#endif
	src_ref = (unsigned char *)malloc(SIZE * sizeof(unsigned char));
	dst_ref = (unsigned char *)malloc(SIZE * sizeof(unsigned char));

	for (auto i = 0; i < HEIGHT; i++) {
		for (auto j = 0; j < WIDTH; j++) {
			src_ref[i * WIDTH + j] = (std::rand() % 256);
#ifndef MANUAL
			src[i * WIDTH + j] = src_ref[i * WIDTH + j];
#else
			src[i * WIDTH_PADDED + j] = src_ref[i * WIDTH + j];
#endif
		}
	}

	for (auto i = 0; i < (FILTER_V_SIZE * FILTER_H_SIZE); i++)
		coeffs[i] = 1;

	conv2d_top(coeffs, src, dst);
	convolution(coeffs, src_ref, dst_ref);

	int ret = 0;
	for (auto i = 0; i < HEIGHT; i++) {
		for (auto j = 0; j < WIDTH; j++) {
#ifndef MANUAL
			if (dst[i * WIDTH + j] != dst_ref[i * WIDTH + j])
#else
			if (dst[i * WIDTH_PADDED + j] != dst_ref[i * WIDTH + j])
#endif
				ret++;
		}
	}

	free(coeffs);
	free(src);
	free(dst);
	free(src_ref);
	free(dst_ref);

	return ret;
}

