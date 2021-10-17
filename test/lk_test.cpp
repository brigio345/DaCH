#include <iostream>
#ifndef __SYNTHESIS__
#define PROFILE
#include <thread>
#endif	/* __SYNTHESIS__ */
#include "matrix.h"
#include "cache.h"

#define USE_CACHE

#ifndef M_PI
#define M_PI 3.1415926535f
#endif
// Flow vector scaling factor
#define FLOW_SCALING_FACTOR (0.25f)  //(1.0f/4.0f)

#define WINDOW_SIZE 3
#define WIDTH 8
#define HEIGHT 8
#define ARRAY_SIZE (WIDTH * HEIGHT)


typedef int mType;

static const int RD_PORTS = 1;

typedef unsigned char dt_in;
typedef float dt_out;

typedef cache<dt_in, true, false, 1, WIDTH*HEIGHT, 1, 1, 16, false, 0> cache_im1;
typedef cache<dt_in, true, false, 1, WIDTH*HEIGHT, 1, 1, 16, false, 0> cache_im2;
typedef cache<dt_out, false, true, 1, WIDTH*HEIGHT*2, 1, 1, 8, false, 0> cache_out;

int Px(int x) {
#pragma HLS INLINE
	if (x >= WIDTH)
		x = (WIDTH - 1);
	else if (x < 0)
		x = 0;

	return x;
}

int Py(int y) {
#pragma HLS INLINE
	if (y >= HEIGHT)
		y = (HEIGHT - 1);
	else if (y < 0)
		y = 0;

	return y;
}

int get_matrix_inv(mType * const G, float *G_inv) {
#pragma HLS inline
	const auto detG = ((float)(G[0] * G[3]) - (float)(G[1] * G[2]));

	if (detG <= 1.0f)
		return 0;

	const auto detG_inv = (1.0f / detG);
	G_inv[0] = (G[3] * detG_inv);
	G_inv[1] = (-G[1] * detG_inv);
	G_inv[2] = (-G[2] * detG_inv);
	G_inv[3] = (G[0] * detG_inv);

	return 1;
}

template<typename IM1_TYPE, typename IM2_TYPE, typename OUT_TYPE>
void knp(IM1_TYPE im1, IM2_TYPE im2, OUT_TYPE out) {
KNP_J_LOOP:for (auto j = 0; j < HEIGHT; j++) {
KNP_I_LOOP:	for (auto i = 0; i < WIDTH; i++) {
			float G_inv[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
#pragma HLS ARRAY_PARTITION variable=G_inv complete dim=1
			mType G[4] = { 0 };
#pragma HLS ARRAY_PARTITION variable=G complete dim=1
			mType b_k[2] = { 0 };
#pragma HLS ARRAY_PARTITION variable=b_k complete dim=1
			int x;

KNP_WJ_LOOP:		for (auto wj = -WINDOW_SIZE; wj <= WINDOW_SIZE; wj++) {
KNP_WI_LOOP:			for (auto wi = -WINDOW_SIZE; wi <= WINDOW_SIZE; wi++) {
#pragma HLS PIPELINE
//#pragma HLS UNROLL factor=RD_PORTS
					const auto px = Px(i+wi);
					const auto py = Py(j+wj);
					const auto im2_val = im2[px + py * WIDTH];

					const auto deltaIk = im1[px + py * WIDTH] - im2_val;
					const auto cx = Px(i + wi +1) + py * WIDTH;
					const auto dx = Px(i + wi - 1) + py * WIDTH;
					mType cIx = im1[cx];

					cIx -= im1[dx];
					cIx >>= 1;
					const auto cy = px + Py(j + wj + 1) * WIDTH;
					const auto dy = px + Py(j + wj - 1) * WIDTH;

					mType cIy = im1[cy];
					cIy -= im1[dy];
					cIy >>= 1;

					G[0] += cIx * cIx;
					G[1] += cIx * cIy;
					G[2] += cIx * cIy;
					G[3] += cIy * cIy;
					b_k[0] += deltaIk * cIx;
					b_k[1] += deltaIk * cIy;
				}
			}

			get_matrix_inv(G, G_inv);

			const auto fx = (G_inv[0] * b_k[0] + G_inv[1] * b_k[1]);
			const auto fy = (G_inv[2] * b_k[0] + G_inv[3] * b_k[1]);

			out[2 * (j * WIDTH + i)] = fx;
			out[2 * (j * WIDTH + i) + 1] = fy;
		}
	   }
}

void knp_syn(cache_im1 &im1_cache, cache_im2 &im2_cache, cache_out &out_cache) {
#pragma HLS inline off
	im1_cache.init();
	im2_cache.init();
	out_cache.init();

	knp<cache_im1 &, cache_im2 &, cache_out &>(im1_cache, im2_cache, out_cache);

	im1_cache.stop();
	im2_cache.stop();
	out_cache.stop();
}

extern "C" void knp_top(dt_in im1_arr[WIDTH * HEIGHT],
		dt_in im2_arr[WIDTH * HEIGHT], dt_out out_arr[WIDTH * HEIGHT * 2]) {
#pragma HLS INTERFACE m_axi port=im1_arr bundle=gmem0 latency=1
#pragma HLS INTERFACE m_axi port=im2_arr bundle=gmem1 latency=1
#pragma HLS INTERFACE m_axi port=out_arr bundle=gmem2 latency=1

#ifdef USE_CACHE
#pragma HLS dataflow disable_start_propagation
	cache_im1 im1_cache;
	cache_im2 im2_cache;
	cache_out out_cache;
#pragma HLS disaggregate variable=im1_cache
#pragma HLS disaggregate variable=im2_cache
#pragma HLS disaggregate variable=out_cache

#ifdef __SYNTHESIS__
	im1_cache.run(im1_arr);
	im2_cache.run(im2_arr);
	out_cache.run(out_arr);
	knp_syn(im1_cache, im2_cache, out_cache);
#else
	im1_cache.init();
	im2_cache.init();
	out_cache.init();

	std::thread im1_thd([&]{im1_cache.run(im1_arr);});
	std::thread im2_thd([&]{im2_cache.run(im2_arr);});
	std::thread out_thd([&]{out_cache.run(out_arr);});
	std::thread knp_thread([&]{knp<cache_im1 &, cache_im2 &, cache_out &>
			(im1_cache, im2_cache, out_cache);});

	knp_thread.join();

	im1_cache.stop();
	im2_cache.stop();
	out_cache.stop();
	im1_thd.join();
	im2_thd.join();
	out_thd.join();

#ifdef PROFILE
	printf("IM1 hit ratio = %.3f (%d / %d)\n",
			im1_cache.get_hit_ratio(), im1_cache.get_n_hits(),
			im1_cache.get_n_reqs());
	printf("IM2 hit ratio = %.3f (%d / %d)\n",
			im2_cache.get_hit_ratio(), im2_cache.get_n_hits(),
			im2_cache.get_n_reqs());
	printf("OUT hit ratio = %.3f ((%d + %d) / %d)\n",
			out_cache.get_hit_ratio(), out_cache.get_n_l1_hits(),
			out_cache.get_n_hits(), out_cache.get_n_reqs());
#endif /* PROFILE */
#endif	/* __SYNTHESIS__ */
#else
	knp<dt_in *, dt_in *, dt_out *>(im1_arr, im2_arr, out_arr);
#endif
}

int main() {
	dt_in im1_arr[WIDTH * HEIGHT];
	dt_in im2_arr[WIDTH * HEIGHT];
	dt_out out_arr[WIDTH * HEIGHT * 2];
	dt_out out_arr_ref[WIDTH * HEIGHT * 2];

	matrix::generate_random<dt_in>(im1_arr, WIDTH, HEIGHT);
	matrix::generate_random<dt_in>(im2_arr, WIDTH, HEIGHT);

	knp_top(im1_arr, im2_arr, out_arr);
	knp<dt_in *, dt_in *, dt_out *>(im1_arr, im2_arr, out_arr_ref);

	if (!matrix::compare<dt_out *>(out_arr, out_arr_ref, WIDTH, HEIGHT)) {
		std::cerr << "Mismatch detected" << std::endl;
		return -1;
	}

	return 0;
}

