// basisu_bc15_spmd_kernels.inl -- Do NOT directly include.
//
// SSE4.1 cppspmd kernel for the standalone BC1 encoder. Included by basisu_bc15_spmd_sse.cpp from file scope after
// cppspmd_sse.h + "using namespace CPPSPMD;". LANES = BLOCKS: 4 independent BC1 blocks per vector, one per lane;
// each lane runs the same scalar-looking program on its block's 16 texels. Mirrors encoder/basisu_kernels_imp.h.

namespace bc_spmd_kern
{
	// Opaque 4-color BC1 encode of 4 blocks at once (one block per lane). pBlocks = 4 blocks x 16 texels,
	// block-contiguous; pOut = 4*8 bytes. om5*/om6* = solid-color omatch tables (int[256]); ls_rounds, avgvar tune.
	struct encode_bc1_4color_blocks : spmd_kernel
	{
		inline vint div3(const vint& x) { return VUINT_SHIFT_RIGHT(x * vint(43691), 17); } // exact floor(x/3), x small
		inline vint expand5(const vint& c) { return VINT_SHIFT_LEFT(c, 3) | VUINT_SHIFT_RIGHT(c, 2); }
		inline vint expand6(const vint& c) { return VINT_SHIFT_LEFT(c, 2) | VUINT_SHIFT_RIGHT(c, 4); }

		// Round per-lane float RGB (0..255) STRAIGHT into 5/6/5-bit space (single round, like encode_bc1's
		// *31/255+0.5) -- avoids the double-rounding (float->8bit->565) that biases endpoints inward.
		inline vint f_to_565(const vfloat& r, const vfloat& g, const vfloat& b)
		{
			const vint r5 = vint(round_nearest(min(max(r, vfloat(0.0f)), vfloat(255.0f)) * vfloat(31.0f / 255.0f)));
			const vint g6 = vint(round_nearest(min(max(g, vfloat(0.0f)), vfloat(255.0f)) * vfloat(63.0f / 255.0f)));
			const vint b5 = vint(round_nearest(min(max(b, vfloat(0.0f)), vfloat(255.0f)) * vfloat(31.0f / 255.0f)));
			return VINT_SHIFT_LEFT(r5, 11) | VINT_SHIFT_LEFT(g6, 5) | b5;
		}

		// Load texel i of all 4 blocks into per-channel vints (lane j = block j's texel i).
		inline void load_texel(const int32_t* p, int i, vint& r, vint& g, vint& b)
		{
			vint rgba; rgba.m_value = _mm_setr_epi32(p[i], p[16 + i], p[32 + i], p[48 + i]);
			r = rgba & vint(0xFF);
			g = VINT_SHIFT_RIGHT(rgba, 8) & vint(0xFF);
			b = VINT_SHIFT_RIGHT(rgba, 16) & vint(0xFF);
			// Opaque 4-color encoder: alpha is ignored entirely.
		}

		// Integer-threshold selector (encode_bc1's bc1_find_sels scheme): 1 dot + 3 compares/texel. The asymmetric
		// (d<=t0) biases extreme texels onto the OUTER selectors so the LS step reaches wider endpoints.
		// Palette order is [pal0,pal2,pal3,pal1] along the c0->c1 axis; lut4={1,3,2,0} via the ternary cascade.
		struct thresholds { vint t0, t1, t2, ar2, ag2, ab2; };
		inline thresholds make_thresholds(
			const vint& pr0, const vint& pg0, const vint& pb0, const vint& pr1, const vint& pg1, const vint& pb1,
			const vint& pr2, const vint& pg2, const vint& pb2, const vint& pr3, const vint& pg3, const vint& pb3)
		{
			const vint ar = pr1 - pr0, ag = pg1 - pg0, ab = pb1 - pb0;
			const vint dp0 = pr0 * ar + pg0 * ag + pb0 * ab;
			const vint dp2 = pr2 * ar + pg2 * ag + pb2 * ab;
			const vint dp3 = pr3 * ar + pg3 * ag + pb3 * ab;
			const vint dp1 = pr1 * ar + pg1 * ag + pb1 * ab;
			thresholds T;
			T.t0 = dp0 + dp2; T.t1 = dp2 + dp3; T.t2 = dp3 + dp1;
			T.ar2 = ar + ar; T.ag2 = ag + ag; T.ab2 = ab + ab;
			return T;
		}
		inline vint thresh_sel(const thresholds& T, const vint& r, const vint& g, const vint& b)
		{
			const vint d = r * T.ar2 + g * T.ag2 + b * T.ab2;
			return spmd_ternaryi(d <= T.t0, 0, spmd_ternaryi(d < T.t1, 2, spmd_ternaryi(d < T.t2, 3, 1)));
		}

		// One least-squares round (per lane): assign integer-threshold selectors for the current endpoints, refit
		// the endpoint line via the 2x2 normal equations (Cramer), adopt only where well-conditioned (det).
		inline void ls_round(const vint* R, const vint* G, const vint* B, vint& c0, vint& c1)
		{
			const vint pr0 = expand5(VINT_SHIFT_RIGHT(c0, 11) & vint(31)), pg0 = expand6(VINT_SHIFT_RIGHT(c0, 5) & vint(63)), pb0 = expand5(c0 & vint(31));
			const vint pr1 = expand5(VINT_SHIFT_RIGHT(c1, 11) & vint(31)), pg1 = expand6(VINT_SHIFT_RIGHT(c1, 5) & vint(63)), pb1 = expand5(c1 & vint(31));
			const vint pr2 = div3(vint(2) * pr0 + pr1), pg2 = div3(vint(2) * pg0 + pg1), pb2 = div3(vint(2) * pb0 + pb1);
			const vint pr3 = div3(pr0 + vint(2) * pr1), pg3 = div3(pg0 + vint(2) * pg1), pb3 = div3(pb0 + vint(2) * pb1);

			const thresholds T = make_thresholds(pr0, pg0, pb0, pr1, pg1, pb1, pr2, pg2, pb2, pr3, pg3, pb3);
			vfloat m00(0.0f), m01(0.0f), m11(0.0f);
			vfloat a0r(0.0f), a0g(0.0f), a0b(0.0f), a1r(0.0f), a1g(0.0f), a1b(0.0f);
			for (int i = 0; i < 16; i++)
			{
				const vint& r = R[i]; const vint& g = G[i]; const vint& b = B[i];
				const vint sel = thresh_sel(T, r, g, b);
				// fraction toward c1 (the LS weight b): {0, 1, 1/3, 2/3}[sel]; a = 1 - b.
				const vfloat tb = spmd_ternaryf(sel == vint(0), vfloat(0.0f), spmd_ternaryf(sel == vint(1), vfloat(1.0f), spmd_ternaryf(sel == vint(2), vfloat(1.0f / 3.0f), vfloat(2.0f / 3.0f))));
				const vfloat ta = vfloat(1.0f) - tb;
				m00 = m00 + ta * ta; m01 = m01 + ta * tb; m11 = m11 + tb * tb;
				const vfloat cr = (vfloat)r, cg = (vfloat)g, cb = (vfloat)b;
				a0r = a0r + ta * cr; a0g = a0g + ta * cg; a0b = a0b + ta * cb;
				a1r = a1r + tb * cr; a1g = a1g + tb * cg; a1b = a1b + tb * cb;
			}
			const vfloat det = m00 * m11 - m01 * m01;
			const vfloat inv = vfloat(1.0f) / max(det, vfloat(1e-6f)); // clamp not bias: exact 1/det for real blocks, never /0
			const vfloat e0r = (a0r * m11 - a1r * m01) * inv, e0g = (a0g * m11 - a1g * m01) * inv, e0b = (a0b * m11 - a1b * m01) * inv;
			const vfloat e1r = (a1r * m00 - a0r * m01) * inv, e1g = (a1g * m00 - a0g * m01) * inv, e1b = (a1b * m00 - a0b * m01) * inv;
			vint nc0 = f_to_565(e0r, e0g, e0b), nc1 = f_to_565(e1r, e1g, e1b);
			{ const vint hi = max(nc0, nc1), lo = min(nc0, nc1); nc0 = hi; nc1 = lo; }
			const vbool ok = det > vfloat(1e-6f); // only adopt a well-conditioned refit; degenerate lane keeps current
			c0 = spmd_ternaryi(ok, nc0, c0);
			c1 = spmd_ternaryi(ok, nc1, c1);
		}

		// Block RGB SSE for an endpoint pair (integer-threshold selectors + squared distance). For keep-better.
		inline vint eval_error(const vint* R, const vint* G, const vint* B, const vint& c0, const vint& c1)
		{
			const vint pr0 = expand5(VINT_SHIFT_RIGHT(c0, 11) & vint(31)), pg0 = expand6(VINT_SHIFT_RIGHT(c0, 5) & vint(63)), pb0 = expand5(c0 & vint(31));
			const vint pr1 = expand5(VINT_SHIFT_RIGHT(c1, 11) & vint(31)), pg1 = expand6(VINT_SHIFT_RIGHT(c1, 5) & vint(63)), pb1 = expand5(c1 & vint(31));
			const vint pr2 = div3(vint(2) * pr0 + pr1), pg2 = div3(vint(2) * pg0 + pg1), pb2 = div3(vint(2) * pb0 + pb1);
			const vint pr3 = div3(pr0 + vint(2) * pr1), pg3 = div3(pg0 + vint(2) * pg1), pb3 = div3(pb0 + vint(2) * pb1);
			const thresholds T = make_thresholds(pr0, pg0, pb0, pr1, pg1, pb1, pr2, pg2, pb2, pr3, pg3, pb3);
			vint err(0);
			for (int i = 0; i < 16; i++)
			{
				const vint& r = R[i]; const vint& g = G[i]; const vint& b = B[i];
				const vint sel = thresh_sel(T, r, g, b);
				const vint psr = spmd_ternaryi(sel == vint(0), pr0, spmd_ternaryi(sel == vint(1), pr1, spmd_ternaryi(sel == vint(2), pr2, pr3)));
				const vint psg = spmd_ternaryi(sel == vint(0), pg0, spmd_ternaryi(sel == vint(1), pg1, spmd_ternaryi(sel == vint(2), pg2, pg3)));
				const vint psb = spmd_ternaryi(sel == vint(0), pb0, spmd_ternaryi(sel == vint(1), pb1, spmd_ternaryi(sel == vint(2), pb2, pb3)));
				const vint dr = r - psr, dg = g - psg, db = b - psb;
				err = err + dr * dr + dg * dg + db * db;
			}
			return err;
		}

		void _call(const basisu::color_rgba* pBlocks, uint8_t* pOut,
			int* om5lo, int* om5hi, int* om6lo, int* om6hi, int ls_rounds, int avgvar,
			uint32_t out_stride, uint32_t num_write)
		{
			const int32_t* p = (const int32_t*)pBlocks;

			// Stage source pixels to SoA ONCE (lane = block): R[i]/G[i]/B[i] hold texel i of all 4 blocks.
			vint R[16], G[16], B[16];
			for (int i = 0; i < 16; i++) load_texel(p, i, R[i], G[i], B[i]);

			// Pass 1: per-lane bounding box + channel sums + integer moments for covariance.
			vint mnr(255), mng(255), mnb(255), mxr(0), mxg(0), mxb(0), sum_r(0), sum_g(0), sum_b(0);
			vint sum_rr(0), sum_gg(0), sum_bb(0), sum_rg(0), sum_rb(0), sum_gb(0);
			for (int i = 0; i < 16; i++)
			{
				const vint& r = R[i]; const vint& g = G[i]; const vint& b = B[i];
				mnr = min(mnr, r); mng = min(mng, g); mnb = min(mnb, b);
				mxr = max(mxr, r); mxg = max(mxg, g); mxb = max(mxb, b);
				sum_r = sum_r + r; sum_g = sum_g + g; sum_b = sum_b + b;
				sum_rr = sum_rr + r * r; sum_gg = sum_gg + g * g; sum_bb = sum_bb + b * b;
				sum_rg = sum_rg + r * g; sum_rb = sum_rb + r * b; sum_gb = sum_gb + g * b;
			}

			vint color0(0), color1(0), sel_word(0);

			// SOLID lanes -> omatch endpoints; all-solid groups skip the non-solid path via SPMD_SELSE's any() check.
			const vbool solid = (mnr == mxr) && (mng == mxg) && (mnb == mxb);
			SPMD_SIF(solid)
			{
				// Clone of basist::encode_bc1_solid_block: omatch endpoints from the (full-cross-product) single-color
				// tables, then the swap + degenerate handling that GUARANTEES 4-color (color0 > color1), never 3-color.
				const vint ar = VUINT_SHIFT_RIGHT(sum_r + vint(8), 4), ag = VUINT_SHIFT_RIGHT(sum_g + vint(8), 4), ab = VUINT_SHIFT_RIGHT(sum_b + vint(8), 4);
				vint max16 = VINT_SHIFT_LEFT(load_all(ar[om5lo]), 11) | VINT_SHIFT_LEFT(load_all(ag[om6lo]), 5) | load_all(ab[om5lo]); // 2x-weighted (m_hi)
				vint min16 = VINT_SHIFT_LEFT(load_all(ar[om5hi]), 11) | VINT_SHIFT_LEFT(load_all(ag[om6hi]), 5) | load_all(ab[om5hi]); // 1x (m_lo)
				vint mask = vint(0xAA);
				SPMD_SIF(min16 == max16) // force max16 > min16 (stay 4-color, never 3-color)
				{
					store(mask, vint(0));
					SPMD_SIF(min16 > vint(0)) { store(min16, min16 - vint(1)); }
					SPMD_SELSE(min16 > vint(0)) { store(max16, vint(1)); store(min16, vint(0)); store(mask, vint(0x55)); }
					SPMD_SENDIF
				}
				SPMD_SENDIF
				SPMD_SIF(max16 < min16) // ensure color0 > color1; selector 2<->3 flips with the swap
				{
					const vint a = max16, b = min16;
					store(max16, b); store(min16, a);
					store(mask, mask ^ vint(0x55));
				}
				SPMD_SENDIF
				store(color0, max16); store(color1, min16);
				store(sel_word, mask | VINT_SHIFT_LEFT(mask, 8) | VINT_SHIFT_LEFT(mask, 16) | VINT_SHIFT_LEFT(mask, 24));
			}
			SPMD_SELSE(solid)
			{
				// PCA seed (METHOD 2, on-axis, sqrt-free): integer-moment covariance -> no-renorm power iteration
				// (bbox-diagonal seed, can't overflow at 4 iters) -> scale 1024/max -> endpoints = mean +- extent.
				const vfloat cxx = (vfloat)(vint(16) * sum_rr - sum_r * sum_r), cxy = (vfloat)(vint(16) * sum_rg - sum_r * sum_g), cxz = (vfloat)(vint(16) * sum_rb - sum_r * sum_b);
				const vfloat cyy = (vfloat)(vint(16) * sum_gg - sum_g * sum_g), cyz = (vfloat)(vint(16) * sum_gb - sum_g * sum_b), czz = (vfloat)(vint(16) * sum_bb - sum_b * sum_b);
				vfloat ax = (vfloat)(mxr - mnr), ay = (vfloat)(mxg - mng), az = (vfloat)(mxb - mnb);
				// Overflow-safe: covariance entries <= 4.16e6, 4 iters -> |v| <= ~6.2e30 << float max. Do not exceed 5 iters.
				for (int it = 0; it < 4; it++)
				{
					const vfloat nr = ax * cxx + ay * cxy + az * cxz;
					const vfloat ng = ax * cxy + ay * cyy + az * cyz;
					const vfloat nb = ax * cxz + ay * cyz + az * czz;
					ax = nr; ay = ng; az = nb;
				}
				const vfloat kk = max(max(abs(ax), abs(ay)), abs(az));
				const vfloat mm = vfloat(1024.0f) / max(kk, vfloat(1e-3f)); // clamp guards the near-degenerate lane
				ax = ax * mm; ay = ay * mm; az = az * mm;
				const vfloat inv_len2 = vfloat(1.0f) / (ax * ax + ay * ay + az * az + vfloat(0.0000125f)); // tiny bias: never /0
				const vfloat meanx = (vfloat)sum_r * vfloat(1.0f / 16.0f), meany = (vfloat)sum_g * vfloat(1.0f / 16.0f), meanz = (vfloat)sum_b * vfloat(1.0f / 16.0f);
				vfloat minp(1e30f), maxp(-1e30f);
				for (int i = 0; i < 16; i++)
				{
					const vfloat pr = ((vfloat)R[i] - meanx) * ax + ((vfloat)G[i] - meany) * ay + ((vfloat)B[i] - meanz) * az;
					minp = min(minp, pr); maxp = max(maxp, pr);
				}
				const vfloat tlo = minp * inv_len2, thi = maxp * inv_len2;
				vint c0 = f_to_565(meanx + tlo * ax, meany + tlo * ay, meanz + tlo * az);
				vint c1 = f_to_565(meanx + thi * ax, meany + thi * ay, meanz + thi * az);
				{ const vint hi = max(c0, c1), lo = min(c0, c1); c0 = hi; c1 = lo; }
				SPMD_SIF(c0 == c1) // 4-color needs color0 > color1; nudge the degenerate lanes
				{
					SPMD_SIF(c1 != vint(0)) { store(c1, c1 - vint(1)); }
					SPMD_SELSE(c1 != vint(0)) { store(c0, c0 + vint(1)); }
					SPMD_SENDIF
				}
				SPMD_SENDIF

				// Least-squares refinement (the big quality lever).
				for (int r = 0; r < ls_rounds; r++) ls_round(R, G, B, c0, c1);
				SPMD_SIF(c0 == c1)
				{
					SPMD_SIF(c1 != vint(0)) { store(c1, c1 - vint(1)); }
					SPMD_SELSE(c1 != vint(0)) { store(c0, c0 + vint(1)); }
					SPMD_SENDIF
				}
				SPMD_SENDIF

				// Low-variance blocks (ramps) collapse the PCA seed: try an omatch-solid-of-average 2nd seed (which
				// survives the collapse), 1 LS round, and keep whichever has lower error. Gated to where it's needed.
				const vint var_proxy = (mxr - mnr) + (mxg - mng) + (mxb - mnb);
				SPMD_SIF(var_proxy < vint(avgvar))
				{
					const vint ar = VUINT_SHIFT_RIGHT(sum_r + vint(8), 4), ag = VUINT_SHIFT_RIGHT(sum_g + vint(8), 4), ab = VUINT_SHIFT_RIGHT(sum_b + vint(8), 4);
					vint c0b = VINT_SHIFT_LEFT(load_all(ar[om5lo]), 11) | VINT_SHIFT_LEFT(load_all(ag[om6lo]), 5) | load_all(ab[om5lo]);
					vint c1b = VINT_SHIFT_LEFT(load_all(ar[om5hi]), 11) | VINT_SHIFT_LEFT(load_all(ag[om6hi]), 5) | load_all(ab[om5hi]);
					{ const vint hi = max(c0b, c1b), lo = min(c0b, c1b); c0b = hi; c1b = lo; }
					SPMD_SIF(c0b == c1b)
					{
						SPMD_SIF(c1b != vint(0)) { store(c1b, c1b - vint(1)); }
						SPMD_SELSE(c1b != vint(0)) { store(c0b, c0b + vint(1)); }
						SPMD_SENDIF
					}
					SPMD_SENDIF
					ls_round(R, G, B, c0b, c1b);
					SPMD_SIF(c0b == c1b)
					{
						SPMD_SIF(c1b != vint(0)) { store(c1b, c1b - vint(1)); }
						SPMD_SELSE(c1b != vint(0)) { store(c0b, c0b + vint(1)); }
						SPMD_SENDIF
					}
					SPMD_SENDIF
					const vbool use_avg = eval_error(R, G, B, c0b, c1b) < eval_error(R, G, B, c0, c1);
					store(c0, spmd_ternaryi(use_avg, c0b, c0));
					store(c1, spmd_ternaryi(use_avg, c1b, c1));
				}
				SPMD_SENDIF

				// Final palette + integer-threshold selector pass.
				const vint pr0 = expand5(VINT_SHIFT_RIGHT(c0, 11) & vint(31)), pg0 = expand6(VINT_SHIFT_RIGHT(c0, 5) & vint(63)), pb0 = expand5(c0 & vint(31));
				const vint pr1 = expand5(VINT_SHIFT_RIGHT(c1, 11) & vint(31)), pg1 = expand6(VINT_SHIFT_RIGHT(c1, 5) & vint(63)), pb1 = expand5(c1 & vint(31));
				const vint pr2 = div3(vint(2) * pr0 + pr1), pg2 = div3(vint(2) * pg0 + pg1), pb2 = div3(vint(2) * pb0 + pb1);
				const vint pr3 = div3(pr0 + vint(2) * pr1), pg3 = div3(pg0 + vint(2) * pg1), pb3 = div3(pb0 + vint(2) * pb1);
				const thresholds T = make_thresholds(pr0, pg0, pb0, pr1, pg1, pb1, pr2, pg2, pb2, pr3, pg3, pb3);
				vint sw(0);
				for (int i = 0; i < 16; i++)
				{
					const vint bestsel = thresh_sel(T, R[i], G[i], B[i]);
					sw = sw | (bestsel << (i * 2));
				}
				store(color0, c0); store(color1, c1); store(sel_word, sw);
			}
			SPMD_SENDIF

			// Emit one BC1 block per lane directly to the (possibly strided) output -- no temp buffer. num_write < 4
			// for the final partial group so the padded lanes aren't written; out_stride lets BC3/BC5 interleave.
			CPPSPMD_DECL(int, c0a[4]); CPPSPMD_DECL(int, c1a[4]); CPPSPMD_DECL(int, swa[4]);
			storeu_linear_all(c0a, color0);
			storeu_linear_all(c1a, color1);
			storeu_linear_all(swa, sel_word);
			for (uint32_t j = 0; j < num_write; j++)
			{
				uint8_t* o = pOut + j * out_stride;
				const uint32_t c0 = (uint32_t)c0a[j], c1 = (uint32_t)c1a[j], sw = (uint32_t)swa[j];
				o[0] = (uint8_t)c0; o[1] = (uint8_t)(c0 >> 8);
				o[2] = (uint8_t)c1; o[3] = (uint8_t)(c1 >> 8);
				o[4] = (uint8_t)sw; o[5] = (uint8_t)(sw >> 8); o[6] = (uint8_t)(sw >> 16); o[7] = (uint8_t)(sw >> 24);
			}
		}
	};
} // namespace bc_spmd_kern

namespace bc_spmd_kern
{
	// BC4 (RGTC1) -- one single-channel block per lane, 4 blocks/call. Clone of basist::encode_bc4: raw bbox
	// min/max, exact nearest-of-8 integer-threshold selector, ALWAYS 8-value mode (solid forced to red0>red1).
	// All-integer, GATHER-FREE: texels via manual load; rank->index by arithmetic (no s_tran gather); the 48-bit
	// selector packed via uniform shifts into two vints (texels 0-9 in lo, 10-15 in hi), combined with <<30 in scalar.
	struct encode_bc4_blocks : spmd_kernel
	{
		// Nearest-of-8 selectors + decoded SSE for an 8-value endpoint pair (r0 > r1). Packs texels 0-9 into slo,
		// 10-15 into shi. Optionally accumulates the 1-D least-squares moments (rank = interpolation weight 0..7).
		inline vint bc4_eval(const vint* A, const vint& r0, const vint& r1, vint& slo, vint& shi, vint* pSw, vint* pSww, vint* pSv, vint* pSwv)
		{
			const vint delta = r0 - r1;
			const vint bias = vint(4) - r1 * vint(14);
			const vint t0 = delta * vint(13), t1 = delta * vint(11), t2 = delta * vint(9), t3 = delta * vint(7), t4 = delta * vint(5), t5 = delta * vint(3), t6 = delta;
			vint sse(0), Sw(0), Sww(0), Sv(0), Swv(0);
			slo = vint(0); shi = vint(0);
			for (int i = 0; i < 16; i++)
			{
				const vint val = A[i];
				const vint sv = val * vint(14) + bias;
				const vint rank = spmd_ternaryi(sv >= t0, 1, 0) + spmd_ternaryi(sv >= t1, 1, 0) + spmd_ternaryi(sv >= t2, 1, 0)
					+ spmd_ternaryi(sv >= t3, 1, 0) + spmd_ternaryi(sv >= t4, 1, 0) + spmd_ternaryi(sv >= t5, 1, 0) + spmd_ternaryi(sv >= t6, 1, 0);
				const vint idx = spmd_ternaryi(rank == vint(7), vint(0), spmd_ternaryi(rank == vint(0), vint(1), vint(8) - rank));
				if (i < 10) slo = slo | (idx << (i * 3));
				else        shi = shi | (idx << ((i - 10) * 3));
				const vint num = rank * r0 + (vint(7) - rank) * r1;
				const vint recon = VUINT_SHIFT_RIGHT(num * vint(9363), 16); // == num/7 floor for num<=1785 (matches scalar /7)
				const vint d = val - recon; sse = sse + d * d;
				Sw = Sw + rank; Sww = Sww + rank * rank; Sv = Sv + val; Swv = Swv + rank * val;
			}
			if (pSw) { *pSw = Sw; *pSww = Sww; *pSv = Sv; *pSwv = Swv; }
			return sse;
		}

		void _call(const uint8_t* pBlocks, uint8_t* pOut, uint32_t out_stride, uint32_t num_write, uint32_t do_ls, uint32_t src_stride)
		{
			// Stage 16 channel values to SoA: A[i] = texel i of the 4 lane-blocks. Source values are src_stride bytes
			// apart (1 = packed channel, 4 = one channel of RGBA); blocks are 16*src_stride bytes apart.
			const uint32_t S = src_stride;
			vint A[16];
			for (int i = 0; i < 16; i++)
				A[i].m_value = _mm_setr_epi32(pBlocks[i * S], pBlocks[(16 + i) * S], pBlocks[(32 + i) * S], pBlocks[(48 + i) * S]);

			vint mn = A[0], mx = A[0];
			for (int i = 1; i < 16; i++) { mn = min(mn, A[i]); mx = max(mx, A[i]); }

			vint color0(0), color1(0), sel_lo(0), sel_hi(0);

			const vbool solid = (mx == mn);
			SPMD_SIF(solid)
			{
				// Force 8-value solid: value>0 -> (max, max-1, all idx 0); value==0 -> (1, 0, all idx 1).
				const vbool pos = (mx > vint(0));
				store(color0, spmd_ternaryi(pos, mx, vint(1)));
				store(color1, spmd_ternaryi(pos, mx - vint(1), vint(0)));
				const vint idx = spmd_ternaryi(pos, vint(0), vint(1)); // replicated into every 3-bit field
				store(sel_lo, idx * vint(0x9249249)); // 10 fields (texels 0-9) all = idx
				store(sel_hi, idx * vint(0x9249));     // 6 fields  (texels 10-15) all = idx
			}
			SPMD_SELSE(solid)
			{
				if (do_ls)
				{
					// HQ: candidate A = raw bbox (== encode_bc4) + LS moments; candidate B = 1-D LS endpoint refit
					// (value ~= a + (rank/7)*b). detf guards /0 for degenerate lanes (refit then rejected). Keep-best.
					vint sloA, shiA, Sw, Sww, Sv, Swv;
					const vint sseA = bc4_eval(A, mx, mn, sloA, shiA, &Sw, &Sww, &Sv, &Swv);
					const vint detI = vint(16) * Sww - Sw * Sw;
					const vfloat detf = (vfloat)spmd_ternaryi(detI == vint(0), vint(1), detI);
					const vfloat b = vfloat(7.0f) * (vfloat)(vint(16) * Swv - Sw * Sv) / detf; // slope = red0 - red1
					const vfloat bsw7 = b * (vfloat)Sw * vfloat(1.0f / 7.0f);
					const vfloat a = ((vfloat)Sv - bsw7) / vfloat(16.0f);                       // intercept = red1
					vint nr1 = vint(round_nearest(a));     nr1 = min(max(nr1, vint(0)), vint(255));
					vint nr0 = vint(round_nearest(a + b)); nr0 = min(max(nr0, vint(0)), vint(255));
					vint sloB, shiB;
					const vint sseB = bc4_eval(A, nr0, nr1, sloB, shiB, nullptr, nullptr, nullptr, nullptr);
					const vbool adopt = ((detI != vint(0)) && (nr0 > nr1)) && (sseB < sseA);
					store(color0, spmd_ternaryi(adopt, nr0, mx));
					store(color1, spmd_ternaryi(adopt, nr1, mn));
					store(sel_lo, spmd_ternaryi(adopt, sloB, sloA));
					store(sel_hi, spmd_ternaryi(adopt, shiB, shiA));
				}
				else
				{
					// FAST: raw bbox endpoints + nearest-of-8 selector only (== basist::encode_bc4 quality, ~1.4x).
					store(color0, mx); store(color1, mn);
					const vint delta = mx - mn;
					const vint bias = vint(4) - mn * vint(14);
					const vint t0 = delta * vint(13), t1 = delta * vint(11), t2 = delta * vint(9), t3 = delta * vint(7), t4 = delta * vint(5), t5 = delta * vint(3), t6 = delta;
					vint slo(0), shi(0);
					for (int i = 0; i < 16; i++)
					{
						const vint sv = A[i] * vint(14) + bias;
						const vint rank = spmd_ternaryi(sv >= t0, 1, 0) + spmd_ternaryi(sv >= t1, 1, 0) + spmd_ternaryi(sv >= t2, 1, 0)
							+ spmd_ternaryi(sv >= t3, 1, 0) + spmd_ternaryi(sv >= t4, 1, 0) + spmd_ternaryi(sv >= t5, 1, 0) + spmd_ternaryi(sv >= t6, 1, 0);
						const vint idx = spmd_ternaryi(rank == vint(7), vint(0), spmd_ternaryi(rank == vint(0), vint(1), vint(8) - rank));
						if (i < 10) slo = slo | (idx << (i * 3));
						else        shi = shi | (idx << ((i - 10) * 3));
					}
					store(sel_lo, slo); store(sel_hi, shi);
				}
			}
			SPMD_SENDIF

			CPPSPMD_DECL(int, c0a[4]); CPPSPMD_DECL(int, c1a[4]); CPPSPMD_DECL(int, sloa[4]); CPPSPMD_DECL(int, shia[4]);
			storeu_linear_all(c0a, color0);
			storeu_linear_all(c1a, color1);
			storeu_linear_all(sloa, sel_lo);
			storeu_linear_all(shia, sel_hi);
			for (uint32_t j = 0; j < num_write; j++)
			{
				uint8_t* o = pOut + j * out_stride;
				o[0] = (uint8_t)c0a[j]; o[1] = (uint8_t)c1a[j];
				const uint64_t W = (uint64_t)(uint32_t)sloa[j] | ((uint64_t)(uint32_t)shia[j] << 30); // 48-bit selector word
				o[2] = (uint8_t)W; o[3] = (uint8_t)(W >> 8); o[4] = (uint8_t)(W >> 16);
				o[5] = (uint8_t)(W >> 24); o[6] = (uint8_t)(W >> 32); o[7] = (uint8_t)(W >> 40);
			}
		}
	};
} // namespace bc_spmd_kern

namespace basisu
{
	namespace bc_spmd
	{
		void encode_bc4_blocks_4_sse41(const uint8_t* pBlocks, uint8_t* pOut, uint32_t out_stride, uint32_t num_write, uint32_t do_ls, uint32_t src_stride)
		{
			spmd_call<bc_spmd_kern::encode_bc4_blocks>(pBlocks, pOut, out_stride, num_write, do_ls, src_stride);
		}
	}
}

// ISA wrapper -- encode 4 blocks at once. Defined here (inside the SSE4.1 TU), declared in basisu_bc15_spmd.cpp.
namespace basisu
{
	namespace bc_spmd
	{
		void encode_bc1_blocks_4_sse41(const color_rgba* pBlocks, uint8_t* pOut,
			int* om5lo, int* om5hi, int* om6lo, int* om6hi, int ls_rounds, int avgvar,
			uint32_t out_stride, uint32_t num_write)
		{
			spmd_call<bc_spmd_kern::encode_bc1_4color_blocks>(pBlocks, pOut, om5lo, om5hi, om6lo, om6hi, ls_rounds, avgvar, out_stride, num_write);
		}
	}
}
