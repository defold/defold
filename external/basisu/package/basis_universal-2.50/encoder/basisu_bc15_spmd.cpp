// basisu_bc15_spmd.cpp -- standalone opaque 4-color BC1 encoder (see basisu_bc15_spmd.h).
//
// This TU holds the public API, the solid-color omatch tables, and the scalar reference encoder. The SSE4.1
// cppspmd kernel lives in basisu_bc15_spmd_kernels.inl, compiled by basisu_bc15_spmd_sse.cpp; we only declare +
// call its wrapper here (gated on g_cpu_supports_sse41), with the scalar path as the fallback.

#include "basisu_bc15_spmd.h"
#include "basisu_enc.h"
#include <math.h>

namespace basisu
{
	extern bool g_cpu_supports_sse41; // set by detect_sse41() in basisu_encoder_init()

	namespace bc_spmd
	{
		// Baked-in benchmarked defaults (not exposed as parameters by design).
		static const int kLsRounds = 2; // least-squares alternation rounds
		static const int kAvgVar = 16;  // try the avg-solid 2nd seed when (rng_r+rng_g+rng_b) < kAvgVar

#if BASISU_SUPPORT_SSE
		// Defined in basisu_bc15_spmd_kernels.inl (the SSE4.1 TU). Encodes up to 4 blocks at once, writing num_write
		// of them directly at pOut + j*out_stride (no temp buffer).
		void encode_bc1_blocks_4_sse41(const color_rgba* pBlocks, uint8_t* pOut,
			int* om5lo, int* om5hi, int* om6lo, int* om6hi, int ls_rounds, int avgvar,
			uint32_t out_stride, uint32_t num_write);

		// BC4: encodes up to 4 single-channel blocks at once, writing num_write of them at pOut + j*out_stride.
		// do_ls!=0 adds the 1-D least-squares refit. src_stride = byte stride between texel values in pBlocks (1 or 4).
		void encode_bc4_blocks_4_sse41(const uint8_t* pBlocks, uint8_t* pOut, uint32_t out_stride, uint32_t num_write, uint32_t do_ls, uint32_t src_stride);
#endif

		// ---- solid-color omatch tables (stb_dxt/basisu 3% span penalty), as int[256] for the kernel's gather ----
		static int s_om5_lo[256], s_om5_hi[256], s_om6_lo[256], s_om6_hi[256];

		static void build_omatch_tables()
		{
			for (int bits = 5; bits <= 6; bits++)
			{
				const int levels = 1 << bits;
				int* lo = (bits == 5) ? s_om5_lo : s_om6_lo;
				int* hi = (bits == 5) ? s_om5_hi : s_om6_hi;
				for (int target = 0; target < 256; target++)
				{
					int best = 0x7FFFFFFF, bc0 = 0, bc1 = 0;
					for (int c0 = 0; c0 < levels; c0++)
					{
						const int e0 = (bits == 5) ? ((c0 << 3) | (c0 >> 2)) : ((c0 << 2) | (c0 >> 4));
						for (int c1 = 0; c1 < levels; c1++) // FULL cross product (matches encode_bc1's prepare_bc1_single_color_table)
						{
							const int e1 = (bits == 5) ? ((c1 << 3) | (c1 >> 2)) : ((c1 << 2) | (c1 >> 4));
							const int interp = (2 * e0 + e1) / 3;          // selector-2 (2/3 toward c0), truncated
							int err = (interp > target) ? (interp - target) : (target - interp);
							err += ((e0 > e1 ? e0 - e1 : e1 - e0) * 3) / 100; // 3% linear span penalty (abs: e0 may be < e1 now)
							if (err < best) { best = err; bc0 = c0; bc1 = c1; }
						}
					}
					lo[target] = bc0; hi[target] = bc1;
				}
			}
		}

		// Thread-safe one-time table init: C++11 guarantees a function-local static's initializer runs exactly once,
		// race-free, even under concurrent first calls (no double-checked-locking bug). init() just forces it.
		static inline void ensure_init() { static const bool s_built = (build_omatch_tables(), true); (void)s_built; }
		void init() { ensure_init(); }

		// ---- scalar reference encoder (the oracle) -- identical algorithm/op-order to the SPMD kernel ----
		static inline void unpack565(uint32_t c, int& r, int& g, int& b)
		{
			r = (c >> 11) & 31; g = (c >> 5) & 63; b = c & 31;
			r = (r << 3) | (r >> 2); g = (g << 2) | (g >> 4); b = (b << 3) | (b >> 2);
		}
		static inline uint32_t f_to_565(float r, float g, float b)
		{
			const float cr = (r < 0.0f) ? 0.0f : (r > 255.0f ? 255.0f : r);
			const float cg = (g < 0.0f) ? 0.0f : (g > 255.0f ? 255.0f : g);
			const float cb = (b < 0.0f) ? 0.0f : (b > 255.0f ? 255.0f : b);
			return ((int)rintf(cr * (31.0f / 255.0f)) << 11) | ((int)rintf(cg * (63.0f / 255.0f)) << 5) | (int)rintf(cb * (31.0f / 255.0f));
		}
		static inline int thresh_sel(const int* pr, const int* pg, const int* pb, int r, int g, int b)
		{
			const int ar = pr[1] - pr[0], ag = pg[1] - pg[0], ab = pb[1] - pb[0];
			const int dp0 = pr[0] * ar + pg[0] * ag + pb[0] * ab;
			const int dp2 = pr[2] * ar + pg[2] * ag + pb[2] * ab;
			const int dp3 = pr[3] * ar + pg[3] * ag + pb[3] * ab;
			const int dp1 = pr[1] * ar + pg[1] * ag + pb[1] * ab;
			const int t0 = dp0 + dp2, t1 = dp2 + dp3, t2 = dp3 + dp1;
			const int d = r * (ar + ar) + g * (ag + ag) + b * (ab + ab);
			return (d <= t0) ? 0 : (d < t1) ? 2 : (d < t2) ? 3 : 1;
		}
		static void ls_round(const color_rgba* blk, uint32_t& c0, uint32_t& c1)
		{
			int r0, g0, b0, r1, g1, b1;
			unpack565(c0, r0, g0, b0); unpack565(c1, r1, g1, b1);
			const int pr[4] = { r0, r1, (2 * r0 + r1) / 3, (r0 + 2 * r1) / 3 };
			const int pg[4] = { g0, g1, (2 * g0 + g1) / 3, (g0 + 2 * g1) / 3 };
			const int pb[4] = { b0, b1, (2 * b0 + b1) / 3, (b0 + 2 * b1) / 3 };
			float m00 = 0, m01 = 0, m11 = 0, a0r = 0, a0g = 0, a0b = 0, a1r = 0, a1g = 0, a1b = 0;
			for (uint32_t i = 0; i < 16; i++)
			{
				const int sel = thresh_sel(pr, pg, pb, blk[i].r, blk[i].g, blk[i].b);
				const float tb = (sel == 0) ? 0.0f : (sel == 1) ? 1.0f : (sel == 2) ? (1.0f / 3.0f) : (2.0f / 3.0f);
				const float ta = 1.0f - tb;
				m00 += ta * ta; m01 += ta * tb; m11 += tb * tb;
				const float cr = (float)blk[i].r, cg = (float)blk[i].g, cb = (float)blk[i].b;
				a0r += ta * cr; a0g += ta * cg; a0b += ta * cb;
				a1r += tb * cr; a1g += tb * cg; a1b += tb * cb;
			}
			const float det = m00 * m11 - m01 * m01;
			const float inv = 1.0f / basisu::maximum(det, 1e-6f);
			const float e0r = (a0r * m11 - a1r * m01) * inv, e0g = (a0g * m11 - a1g * m01) * inv, e0b = (a0b * m11 - a1b * m01) * inv;
			const float e1r = (a1r * m00 - a0r * m01) * inv, e1g = (a1g * m00 - a0g * m01) * inv, e1b = (a1b * m00 - a0b * m01) * inv;
			uint32_t nc0 = f_to_565(e0r, e0g, e0b), nc1 = f_to_565(e1r, e1g, e1b);
			{ const uint32_t hi = basisu::maximum(nc0, nc1), lo = basisu::minimum(nc0, nc1); nc0 = hi; nc1 = lo; }
			if (det > 1e-6f) { c0 = nc0; c1 = nc1; }
		}
		static int eval_error(const color_rgba* blk, uint32_t c0, uint32_t c1)
		{
			int r0, g0, b0, r1, g1, b1;
			unpack565(c0, r0, g0, b0); unpack565(c1, r1, g1, b1);
			const int pr[4] = { r0, r1, (2 * r0 + r1) / 3, (r0 + 2 * r1) / 3 };
			const int pg[4] = { g0, g1, (2 * g0 + g1) / 3, (g0 + 2 * g1) / 3 };
			const int pb[4] = { b0, b1, (2 * b0 + b1) / 3, (b0 + 2 * b1) / 3 };
			int err = 0;
			for (uint32_t i = 0; i < 16; i++)
			{
				const int sel = thresh_sel(pr, pg, pb, blk[i].r, blk[i].g, blk[i].b);
				const int dr = blk[i].r - pr[sel], dg = blk[i].g - pg[sel], db = blk[i].b - pb[sel];
				err += dr * dr + dg * dg + db * db;
			}
			return err;
		}
		static void encode_block_scalar(const color_rgba* blk, uint8_t out[8])
		{
			int mnr = 255, mng = 255, mnb = 255, mxr = 0, mxg = 0, mxb = 0, sum_r = 0, sum_g = 0, sum_b = 0;
			int sum_rr = 0, sum_gg = 0, sum_bb = 0, sum_rg = 0, sum_rb = 0, sum_gb = 0;
			for (uint32_t i = 0; i < 16; i++)
			{
				const int r = blk[i].r, g = blk[i].g, b = blk[i].b;
				mnr = basisu::minimum<int>(mnr, r); mng = basisu::minimum<int>(mng, g); mnb = basisu::minimum<int>(mnb, b);
				mxr = basisu::maximum<int>(mxr, r); mxg = basisu::maximum<int>(mxg, g); mxb = basisu::maximum<int>(mxb, b);
				sum_r += r; sum_g += g; sum_b += b;
				sum_rr += r * r; sum_gg += g * g; sum_bb += b * b;
				sum_rg += r * g; sum_rb += r * b; sum_gb += g * b;
			}

			// Solid block: clone of basist::encode_bc1_solid_block -- omatch endpoints from the (full-cross-product)
			// single-color tables, then the same swap + degenerate handling that GUARANTEES a 4-color block
			// (color0 > color1), never 3-color (required so this is safe for BC3/BC5 color blocks).
			if ((mnr == mxr) && (mng == mxg) && (mnb == mxb))
			{
				const int ar = (sum_r + 8) >> 4, ag = (sum_g + 8) >> 4, ab = (sum_b + 8) >> 4;
				uint32_t max16 = (s_om5_lo[ar] << 11) | (s_om6_lo[ag] << 5) | s_om5_lo[ab]; // lo[] = 2x-weighted endpoint (encode_bc1 m_hi)
				uint32_t min16 = (s_om5_hi[ar] << 11) | (s_om6_hi[ag] << 5) | s_om5_hi[ab]; // hi[] = 1x endpoint (encode_bc1 m_lo)
				uint32_t mask = 0xAA;
				if (min16 == max16)
				{
					mask = 0; // selector 0 = color0 directly; force max16 > min16 so the block stays 4-color
					if (min16 > 0) min16--;
					else { max16 = 1; min16 = 0; mask = 0x55; } // l == h == 0
				}
				if (max16 < min16) { const uint32_t t = max16; max16 = min16; min16 = t; mask ^= 0x55; } // selector 2<->3 on swap
				out[0] = (uint8_t)max16; out[1] = (uint8_t)(max16 >> 8); out[2] = (uint8_t)min16; out[3] = (uint8_t)(min16 >> 8);
				out[4] = out[5] = out[6] = out[7] = (uint8_t)mask;
				return;
			}

			// PCA seed (METHOD 2, on-axis, sqrt-free).
			const float cxx = (float)(16 * sum_rr - sum_r * sum_r), cxy = (float)(16 * sum_rg - sum_r * sum_g), cxz = (float)(16 * sum_rb - sum_r * sum_b);
			const float cyy = (float)(16 * sum_gg - sum_g * sum_g), cyz = (float)(16 * sum_gb - sum_g * sum_b), czz = (float)(16 * sum_bb - sum_b * sum_b);
			float ax = (float)(mxr - mnr), ay = (float)(mxg - mng), az = (float)(mxb - mnb);
			for (int it = 0; it < 4; it++)
			{
				const float nr = ax * cxx + ay * cxy + az * cxz;
				const float ng = ax * cxy + ay * cyy + az * cyz;
				const float nb = ax * cxz + ay * cyz + az * czz;
				ax = nr; ay = ng; az = nb;
			}
			const float kk = basisu::maximum(basisu::maximum(fabsf(ax), fabsf(ay)), fabsf(az));
			const float mm = 1024.0f / basisu::maximum(kk, 1e-3f);
			ax *= mm; ay *= mm; az *= mm;
			const float inv_len2 = 1.0f / (ax * ax + ay * ay + az * az + 0.0000125f);
			const float meanx = (float)sum_r * (1.0f / 16.0f), meany = (float)sum_g * (1.0f / 16.0f), meanz = (float)sum_b * (1.0f / 16.0f);
			float minp = 1e30f, maxp = -1e30f;
			for (uint32_t i = 0; i < 16; i++)
			{
				const float prj = ((float)blk[i].r - meanx) * ax + ((float)blk[i].g - meany) * ay + ((float)blk[i].b - meanz) * az;
				minp = basisu::minimum(minp, prj); maxp = basisu::maximum(maxp, prj);
			}
			const float tlo = minp * inv_len2, thi = maxp * inv_len2;
			uint32_t color0 = f_to_565(meanx + tlo * ax, meany + tlo * ay, meanz + tlo * az);
			uint32_t color1 = f_to_565(meanx + thi * ax, meany + thi * ay, meanz + thi * az);
			{ const uint32_t hi = basisu::maximum(color0, color1), lo = basisu::minimum(color0, color1); color0 = hi; color1 = lo; }
			if (color0 == color1) { if (color1 != 0) color1--; else color0++; }

			for (int r = 0; r < kLsRounds; r++) ls_round(blk, color0, color1);
			if (color0 == color1) { if (color1 != 0) color1--; else color0++; }

			// Low-variance avg-solid 2nd seed (keep-better).
			const int var_proxy = (mxr - mnr) + (mxg - mng) + (mxb - mnb);
			if (var_proxy < kAvgVar)
			{
				const int ar = (sum_r + 8) >> 4, ag = (sum_g + 8) >> 4, ab = (sum_b + 8) >> 4;
				uint32_t c0b = (s_om5_lo[ar] << 11) | (s_om6_lo[ag] << 5) | s_om5_lo[ab];
				uint32_t c1b = (s_om5_hi[ar] << 11) | (s_om6_hi[ag] << 5) | s_om5_hi[ab];
				{ const uint32_t hi = basisu::maximum(c0b, c1b), lo = basisu::minimum(c0b, c1b); c0b = hi; c1b = lo; }
				if (c0b == c1b) { if (c1b != 0) c1b--; else c0b++; }
				ls_round(blk, c0b, c1b);
				if (c0b == c1b) { if (c1b != 0) c1b--; else c0b++; }
				if (eval_error(blk, c0b, c1b) < eval_error(blk, color0, color1)) { color0 = c0b; color1 = c1b; }
			}

			int r0, g0, b0, r1, g1, b1;
			unpack565(color0, r0, g0, b0); unpack565(color1, r1, g1, b1);
			const int pr[4] = { r0, r1, (2 * r0 + r1) / 3, (r0 + 2 * r1) / 3 };
			const int pg[4] = { g0, g1, (2 * g0 + g1) / 3, (g0 + 2 * g1) / 3 };
			const int pb[4] = { b0, b1, (2 * b0 + b1) / 3, (b0 + 2 * b1) / 3 };
			uint32_t sw = 0;
			for (uint32_t i = 0; i < 16; i++)
				sw |= ((uint32_t)thresh_sel(pr, pg, pb, blk[i].r, blk[i].g, blk[i].b)) << (i * 2);
			out[0] = (uint8_t)color0; out[1] = (uint8_t)(color0 >> 8);
			out[2] = (uint8_t)color1; out[3] = (uint8_t)(color1 >> 8);
			out[4] = (uint8_t)sw; out[5] = (uint8_t)(sw >> 8); out[6] = (uint8_t)(sw >> 16); out[7] = (uint8_t)(sw >> 24);
		}

		// ---- public API ----
		void encode_bc1_scalar(void* pBlocks, const color_rgba* pSrc_pixels, uint32_t num_blocks, uint32_t block_stride)
		{
			ensure_init();
			uint8_t* pOut = (uint8_t*)pBlocks;
			const uint32_t out_stride = 8 * block_stride;
			for (uint32_t b = 0; b < num_blocks; b++)
				encode_block_scalar(pSrc_pixels + b * 16, pOut + b * out_stride);
		}

		void encode_bc1_spmd(void* pBlocks, const color_rgba* pSrc_pixels, uint32_t num_blocks, uint32_t block_stride)
		{
			ensure_init();
			uint8_t* pOut = (uint8_t*)pBlocks;
			(void)pOut;
			const uint32_t out_stride = 8 * block_stride;
			(void)out_stride;
#if BASISU_SUPPORT_SSE
			if (g_cpu_supports_sse41)
			{
				// Full groups of 4: the kernel writes all 4 blocks directly at the (possibly strided) output.
				uint32_t base = 0;
				for (; base + 4 <= num_blocks; base += 4)
					encode_bc1_blocks_4_sse41(pSrc_pixels + base * 16, pOut + base * out_stride, s_om5_lo, s_om5_hi, s_om6_lo, s_om6_hi, kLsRounds, kAvgVar, out_stride, 4);

				// Final partial group: pad the input to 4, but write only the n valid blocks (still direct, no temp).
				const uint32_t n = num_blocks - base; // 0..3
				if (n)
				{
					color_rgba grp[64];
					for (uint32_t gi = 0; gi < 4; gi++)
						memcpy(grp + gi * 16, pSrc_pixels + (base + ((gi < n) ? gi : (n - 1))) * 16, 16 * sizeof(color_rgba));
					encode_bc1_blocks_4_sse41(grp, pOut + base * out_stride, s_om5_lo, s_om5_hi, s_om6_lo, s_om6_hi, kLsRounds, kAvgVar, out_stride, n);
				}
				return;
			}
#endif
			encode_bc1_scalar(pBlocks, pSrc_pixels, num_blocks, block_stride); // scalar fallback (no SSE4.1)
		}

		// ================================ BC4 (RGTC1, single channel) ================================

		// Scalar reference (oracle) for one BC4 block -- clone of basist::encode_bc4 (raw bbox min/max, exact
		// nearest-of-8 integer-threshold selector), but the solid case is forced to 8-value mode (red0 > red1) so
		// every block is 8-color. v = 16 contiguous channel bytes (texel i = y*4+x). out = 8 bytes.
		// Compute the nearest-of-8 selectors + the resulting decoded SSE for an 8-value endpoint pair (r0 > r1).
		// Optionally accumulates the 1-D least-squares moments (rank = interpolation weight, 0..7).
		static int bc4_eval(const uint8_t* v, int r0, int r1, uint64_t& sel_out, int* pSw, int* pSww, int* pSv, int* pSwv, uint32_t src_stride)
		{
			const int delta = r0 - r1;
			const int t0 = delta * 13, t1 = delta * 11, t2 = delta * 9, t3 = delta * 7, t4 = delta * 5, t5 = delta * 3, t6 = delta;
			const int bias = 4 - r1 * 14;
			uint64_t sel = 0; int sse = 0, Sw = 0, Sww = 0, Sv = 0, Swv = 0;
			for (int i = 0; i < 16; i++)
			{
				const int val = v[i * src_stride];
				const int sv = val * 14 + bias;
				const int rank = (sv >= t0) + (sv >= t1) + (sv >= t2) + (sv >= t3) + (sv >= t4) + (sv >= t5) + (sv >= t6);
				const int idx = (rank == 7) ? 0 : (rank == 0) ? 1 : (8 - rank);
				sel |= (uint64_t)idx << (i * 3);
				const int recon = (rank * r0 + (7 - rank) * r1) / 7; // == decoded value for this selector (matches unpack_bc4)
				const int d = val - recon; sse += d * d;
				Sw += rank; Sww += rank * rank; Sv += val; Swv += rank * val;
			}
			sel_out = sel;
			if (pSw) { *pSw = Sw; *pSww = Sww; *pSv = Sv; *pSwv = Swv; }
			return sse;
		}

		static void encode_bc4_block(const uint8_t* v, uint8_t out[8], bool do_ls, uint32_t src_stride)
		{
			int mn = v[0], mx = v[0];
			for (int i = 1; i < 16; i++) { const int x = v[i * src_stride]; mn = basisu::minimum(mn, x); mx = basisu::maximum(mx, x); }

			// Solid -> force an 8-value block (red0 > red1) that still reconstructs the exact value.
			if (mn == mx)
			{
				int r0, r1, idx;
				if (mx > 0) { r0 = mx; r1 = mx - 1; idx = 0; } // all selectors index 0 = red0 = value
				else { r0 = 1; r1 = 0; idx = 1; }             // value 0: all selectors index 1 = red1 = 0
				out[0] = (uint8_t)r0; out[1] = (uint8_t)r1;
				uint64_t sel = 0; for (int i = 0; i < 16; i++) sel |= (uint64_t)idx << (i * 3);
				for (int i = 0; i < 6; i++) out[2 + i] = (uint8_t)(sel >> (i * 8));
				return;
			}

			// Candidate A: raw bbox endpoints (== basist::encode_bc4) + (for HQ) its LS moments.
			int Sw, Sww, Sv, Swv; uint64_t sel;
			int r0 = mx, r1 = mn;
			int sse = bc4_eval(v, r0, r1, sel, &Sw, &Sww, &Sv, &Swv, src_stride);

			// Candidate B (HQ only): one 1-D least-squares endpoint refit (value ~= a + (rank/7)*b), adopted only if
			// it lowers SSE -- monotonic, can never regress below bbox/encode_bc4. Always 8-value (require nr0 > nr1).
			if (do_ls)
			{
				const int detI = 16 * Sww - Sw * Sw;
				if (detI != 0)
				{
					const float b = 7.0f * (float)(16 * Swv - Sw * Sv) / (float)detI; // slope = red0 - red1
					const float bsw7 = b * (float)Sw * (1.0f / 7.0f);
					const float a = ((float)Sv - bsw7) / 16.0f;                        // intercept = red1
					const int nr1 = basisu::clamp<int>((int)rintf(a), 0, 255);
					const int nr0 = basisu::clamp<int>((int)rintf(a + b), 0, 255);
					if (nr0 > nr1)
					{
						uint64_t selB;
						const int sseB = bc4_eval(v, nr0, nr1, selB, nullptr, nullptr, nullptr, nullptr, src_stride);
						if (sseB < sse) { r0 = nr0; r1 = nr1; sel = selB; sse = sseB; }
					}
				}
			}

			out[0] = (uint8_t)r0; out[1] = (uint8_t)r1;
			for (int i = 0; i < 6; i++) out[2 + i] = (uint8_t)(sel >> (i * 8));
		}

		void encode_bc4_scalar(void* pBlocks, const uint8_t* pSrc_pixels, uint32_t num_blocks, uint32_t block_stride, bool high_quality, uint32_t src_stride)
		{
			uint8_t* pOut = (uint8_t*)pBlocks;
			const uint32_t out_stride = 8 * block_stride;
			const uint32_t in_block = 16 * src_stride; // bytes between consecutive blocks in the source
			for (uint32_t b = 0; b < num_blocks; b++)
				encode_bc4_block(pSrc_pixels + (size_t)b * in_block, pOut + b * out_stride, high_quality, src_stride);
		}

		void encode_bc4_spmd(void* pBlocks, const uint8_t* pSrc_pixels, uint32_t num_blocks, uint32_t block_stride, bool high_quality, uint32_t src_stride)
		{
			uint8_t* pOut = (uint8_t*)pBlocks;
			(void)pOut;
			const uint32_t out_stride = 8 * block_stride;
			(void)out_stride;
			const uint32_t in_block = 16 * src_stride;
			(void)in_block;
#if BASISU_SUPPORT_SSE
			if (g_cpu_supports_sse41)
			{
				const uint32_t do_ls = high_quality ? 1 : 0;
				// Full groups read straight from the (possibly strided) source -- no copy.
				uint32_t base = 0;
				for (; base + 4 <= num_blocks; base += 4)
					encode_bc4_blocks_4_sse41(pSrc_pixels + (size_t)base * in_block, pOut + base * out_stride, out_stride, 4, do_ls, src_stride);

				// Tail (<4): gather the channel into a tightly-packed 64-byte stack buffer (pad with last block), then
				// encode with src_stride=1. Only the tail copies; the bulk above is zero-copy.
				const uint32_t n = num_blocks - base; // 0..3
				if (n)
				{
					uint8_t grp[64];
					for (uint32_t gi = 0; gi < 4; gi++)
					{
						const uint8_t* sp = pSrc_pixels + (size_t)(base + ((gi < n) ? gi : (n - 1))) * in_block;
						for (uint32_t t = 0; t < 16; t++) grp[gi * 16 + t] = sp[t * src_stride];
					}
					encode_bc4_blocks_4_sse41(grp, pOut + base * out_stride, out_stride, n, do_ls, 1);
				}
				return;
			}
#endif
			encode_bc4_scalar(pBlocks, pSrc_pixels, num_blocks, block_stride, high_quality, src_stride); // scalar fallback
		}

		// ================================ High-level RGBA format helpers ================================
		// NO allocations, NO channel-extraction copies: the BC4 path reads the requested channel straight out of the
		// RGBA pixels via src_stride=4 (the channel pointer is &pPixels->r/g/b/a). encode_bc4_* handles the <4 tail
		// without overrunning the (possibly strided) output.

		void encode_bc1(void* pBlocks, const color_rgba* pPixels, uint32_t num_blocks, bool use_spmd)
		{
			if (use_spmd) encode_bc1_spmd(pBlocks, pPixels, num_blocks, 1);
			else encode_bc1_scalar(pBlocks, pPixels, num_blocks, 1);
		}

		void encode_bc4(void* pBlocks, const color_rgba* pPixels, uint32_t num_blocks, bool use_spmd, bool high_quality)
		{
			const uint8_t* pR = &pPixels[0].r; // R channel, stride 4, contiguous output
			if (use_spmd) encode_bc4_spmd(pBlocks, pR, num_blocks, 1, high_quality, 4);
			else encode_bc4_scalar(pBlocks, pR, num_blocks, 1, high_quality, 4);
		}

		void encode_bc5(void* pBlocks, const color_rgba* pPixels, uint32_t num_blocks, bool use_spmd, bool high_quality)
		{
			uint8_t* pOut = (uint8_t*)pBlocks;  // [0..7] = BC4 of R, [8..15] = BC4 of G  (each block_stride 2)
			const uint8_t* pR = &pPixels[0].r, * pG = &pPixels[0].g;
			if (use_spmd) { encode_bc4_spmd(pOut, pR, num_blocks, 2, high_quality, 4); encode_bc4_spmd(pOut + 8, pG, num_blocks, 2, high_quality, 4); }
			else { encode_bc4_scalar(pOut, pR, num_blocks, 2, high_quality, 4); encode_bc4_scalar(pOut + 8, pG, num_blocks, 2, high_quality, 4); }
		}

		void encode_bc3(void* pBlocks, const color_rgba* pPixels, uint32_t num_blocks, bool use_spmd, bool high_quality)
		{
			uint8_t* pOut = (uint8_t*)pBlocks;  // [0..7] = BC4 alpha block, [8..15] = BC1 color block
			const uint8_t* pA = &pPixels[0].a;  // A channel, stride 4
			if (use_spmd) { encode_bc4_spmd(pOut, pA, num_blocks, 2, high_quality, 4); encode_bc1_spmd(pOut + 8, pPixels, num_blocks, 2); }
			else { encode_bc4_scalar(pOut, pA, num_blocks, 2, high_quality, 4); encode_bc1_scalar(pOut + 8, pPixels, num_blocks, 2); }
		}

		void encode_bc2(void* pBlocks, const color_rgba* pPixels, uint32_t num_blocks, bool use_spmd)
		{
			uint8_t* pOut = (uint8_t*)pBlocks; // [0..7] = explicit 4-bit alpha (scalar), [8..15] = BC1 color block
			for (uint32_t b = 0; b < num_blocks; b++)
			{
				uint8_t* o = pOut + b * 16;
				const color_rgba* p = pPixels + b * 16;
				for (uint32_t y = 0; y < 4; y++)
				{
					uint32_t row = 0; // one little-endian 16-bit word per row; texel x in nibble x
					for (uint32_t x = 0; x < 4; x++) { const int a8 = p[y * 4 + x].a; const int a4 = (a8 * 2 + 17) / 34; row |= (uint32_t)a4 << (x * 4); } // round(a8/17)
					o[y * 2] = (uint8_t)row; o[y * 2 + 1] = (uint8_t)(row >> 8);
				}
			}
			if (use_spmd) encode_bc1_spmd(pOut + 8, pPixels, num_blocks, 2);
			else encode_bc1_scalar(pOut + 8, pPixels, num_blocks, 2);
		}

	} // namespace bc_spmd
} // namespace basisu
