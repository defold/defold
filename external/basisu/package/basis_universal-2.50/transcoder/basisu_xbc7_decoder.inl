// File: basisu_xbc7_decoder.inl
// XBC7 decoder implementation. #included at the END of basisu_transcoder.cpp
// (the transcoder stays a single .cpp). Declarations live in
// basisu_xbc7_decoder.h. Decoding REQUIRES zstd (BASISD_SUPPORT_KTX2_ZSTD); when
// that is disabled the public entry points fail with an error.
namespace basist {
namespace xbc7 {

	bool eval_weight_predictor(
		uint32_t cand_index, uint32_t amp_code,
		uint32_t bx, uint32_t by, uint32_t num_blocks_x,
		const tile_bounds& tile, // every block reference is clamped to this AABB (encoder: the job's tile; decoder: whole image)
		const vector2D<basist::bc7u::log_bc7_block>& log_blks,
		uint32_t p, int pOut_preds[16])
	{
		BASISU_NOTE_UNUSED(num_blocks_x); // bounds now come from the tile
		assert(cand_index != cCandAbsolute);
		assert(cand_index < cTotalCandidates);
		assert(amp_code < 4);
		assert(tile.contains((int)bx, (int)by));

		auto fetch_w = [](const basist::bc7u::log_bc7_block* pBlk, uint32_t plane, uint32_t w) -> int
			{
				const uint32_t sp = pBlk->is_dual_plane() ? plane : 0;
				return basist::bc7u::dequant_weight(pBlk->m_weights[sp][w], pBlk->m_weight_bits[sp]);
			};

		const basist::bc7u::log_bc7_block* pLeft_diag_log_blk = tile.contains((int)bx - 1, (int)by - 1) ? &log_blks(bx - 1, by - 1) : nullptr;
		const basist::bc7u::log_bc7_block* pRight_diag_log_blk = tile.contains((int)bx + 1, (int)by - 1) ? &log_blks(bx + 1, by - 1) : nullptr;
		const basist::bc7u::log_bc7_block* pUp_log_blk = tile.contains((int)bx, (int)by - 1) ? &log_blks(bx, by - 1) : nullptr;
		const basist::bc7u::log_bc7_block* pLeft_log_blk = tile.contains((int)bx - 1, (int)by) ? &log_blks(bx - 1, by) : nullptr;

		const basist::bc7u::log_bc7_block* pCand_log_blk = nullptr;

		if (cand_index == cCandAbsolute)
		{

		}
		else if (cand_index >= cCandFirstXYDelta)
		{
			// generic causal block reference (copy); subsumes the old
			// left/up/left-diag/right-diag copy candidates
			const xbc7_xy_delta& delta = g_xbc7_xy_deltas[cand_index - cCandFirstXYDelta];
			const int nx = (int)bx + delta.m_dx;
			const int ny = (int)by + delta.m_dy;

			if (tile.contains(nx, ny))
				pCand_log_blk = &log_blks(nx, ny);

			if (!pCand_log_blk)
				return false;
		}
		else
		{
			if (cand_index == cCandLeftEdge)
				pCand_log_blk = pLeft_log_blk; // left edge
			else if (cand_index == cCandUpperEdge)
				pCand_log_blk = pUp_log_blk; // upper edge
			else if (cand_index == cCandLUBlend)
				pCand_log_blk = (pLeft_log_blk && pUp_log_blk) ? pLeft_log_blk : nullptr; // left+upper edge blend
			else if (cand_index == cCandReflectLeft)
				pCand_log_blk = pLeft_log_blk; // reflect left
			else if (cand_index == cCandReflectUpper)
				pCand_log_blk = pUp_log_blk; // reflect upper
			else if (cand_index == cCandLUAvg)
				pCand_log_blk = (pLeft_log_blk && pUp_log_blk) ? pLeft_log_blk : nullptr; // left+upper edge avg
			else if (cand_index == cCandLUBlendStrong)
				pCand_log_blk = (pLeft_log_blk && pUp_log_blk) ? pLeft_log_blk : nullptr; // left+upper edge stronger distance blend
			else if (cand_index == cCandGradient)
				pCand_log_blk = (pLeft_log_blk && pUp_log_blk && pLeft_diag_log_blk) ? pLeft_log_blk : nullptr; // gradient
			else if (cand_index == cCandGradientDamped)
				pCand_log_blk = (pLeft_log_blk && pUp_log_blk && pLeft_diag_log_blk) ? pLeft_log_blk : nullptr; // damped gradient
			else if (cand_index == cCandDiagAvg)
				pCand_log_blk = (pLeft_diag_log_blk && pRight_diag_log_blk) ? pLeft_diag_log_blk : nullptr; // left/right diagonal avg
			else if (cand_index == cCandDiagEdgeBlend)
				pCand_log_blk = (pLeft_diag_log_blk && pRight_diag_log_blk) ? pLeft_diag_log_blk : nullptr; // diagonal edge blend
			else if (cand_index == cCandUpperDiagEdgeBlend)
				pCand_log_blk = (pUp_log_blk && pLeft_diag_log_blk && pRight_diag_log_blk) ? pLeft_diag_log_blk : nullptr; // upper + diagonal edge blend
			else if ((cand_index == cCandMED) || (cand_index == cCandGAB))
				pCand_log_blk = (pLeft_log_blk && pUp_log_blk && pLeft_diag_log_blk) ? pLeft_log_blk : nullptr; // MED / gradient-adaptive blend
			else if (cand_index == cCandPlaneFit)
				pCand_log_blk = (pLeft_log_blk && pUp_log_blk) ? pLeft_log_blk : nullptr; // LS plane fit
			else if ((cand_index == cCandDDL) && pUp_log_blk && pRight_diag_log_blk)
				pCand_log_blk = pUp_log_blk;
			else if (cand_index == cCandDDR)
				pCand_log_blk = (pLeft_log_blk && pUp_log_blk && pLeft_diag_log_blk) ? pLeft_log_blk : nullptr;

			if (!pCand_log_blk)
				return false;
		}

		int* pWeight_predictions = nullptr;

		int weight_preds[16];
		if (pCand_log_blk)
		{
			for (uint32_t w = 0; w < 16; w++)
			{
				weight_preds[w] = fetch_w(pCand_log_blk, p, w);
			}

			int orig_weight_preds[16];
			memcpy(orig_weight_preds, weight_preds, sizeof(orig_weight_preds));

			if (cand_index == cCandLeftEdge)
			{
				// left edge
				for (uint32_t y = 0; y < 4; y++)
					for (uint32_t x = 0; x < 4; x++)
						weight_preds[index_from_xy(x, y)] = orig_weight_preds[index_from_xy(3, y)];
			}
			else if (cand_index == cCandUpperEdge)
			{
				// upper edge
				for (uint32_t y = 0; y < 4; y++)
					for (uint32_t x = 0; x < 4; x++)
						weight_preds[index_from_xy(x, y)] = orig_weight_preds[index_from_xy(x, 3)];
			}
			else if ((cand_index == cCandLUBlend) || (cand_index == cCandLUAvg) || (cand_index == cCandLUBlendStrong))
			{
				// left+upper edge blend variants.
				// pCand_log_blk is pLeft_log_blk here, so orig_weight_preds contains the left block.
				// Pull upper edge directly from pUp_log_blk.

				int upper_edge[4];

				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t w = index_from_xy(x, 3); // upper block's bottom edge

					upper_edge[x] = fetch_w(pUp_log_blk, p, w);
				}

				for (uint32_t y = 0; y < 4; y++)
				{
					const int left_val = orig_weight_preds[index_from_xy(3, y)]; // left block's right edge

					for (uint32_t x = 0; x < 4; x++)
					{
						const int upper_val = upper_edge[x];
						int pred;

						if (cand_index == cCandLUBlend)
						{
							// Existing distance-weighted blend.
							const int wl = 4 - static_cast<int>(x); // 4,3,2,1
							const int wu = 4 - static_cast<int>(y); // 4,3,2,1
							const int den = wl + wu;

							pred = (wl * left_val + wu * upper_val + (den >> 1)) / den;
						}
						else if (cand_index == cCandLUAvg)
						{
							// Simple average.
							pred = (left_val + upper_val + 1) >> 1;
						}
						else // cCandLUBlendStrong
						{
							// Stronger distance weighting: trust the nearest edge more.
							const int dx = 4 - static_cast<int>(x); // 4,3,2,1
							const int dy = 4 - static_cast<int>(y); // 4,3,2,1
							const int wl = dx * dx; // 16,9,4,1
							const int wu = dy * dy; // 16,9,4,1
							const int den = wl + wu;

							pred = (wl * left_val + wu * upper_val + (den >> 1)) / den;
						}

						weight_preds[index_from_xy(x, y)] = pred;
					}
				}
			}
			else if (cand_index == cCandReflectLeft)
			{
				// reflect left
				for (uint32_t y = 0; y < 4; y++)
					for (uint32_t x = 0; x < 4; x++)
						weight_preds[index_from_xy(x, y)] = orig_weight_preds[index_from_xy(3 - x, y)];
			}
			else if (cand_index == cCandReflectUpper)
			{
				// reflect upper
				for (uint32_t y = 0; y < 4; y++)
					for (uint32_t x = 0; x < 4; x++)
						weight_preds[index_from_xy(x, y)] = orig_weight_preds[index_from_xy(x, 3 - y)];
			}
			else if ((cand_index == cCandGradient) || (cand_index == cCandGradientDamped) || (cand_index == cCandMED) || (cand_index == cCandGAB))
			{
				int upper_edge[4];

				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t w = index_from_xy(x, 3); // upper block's bottom edge

					upper_edge[x] = fetch_w(pUp_log_blk, p, w);
				}

				const uint32_t corner_w = index_from_xy(3, 3); // upper-left block's bottom-right

				int corner_val;
				corner_val = fetch_w(pLeft_diag_log_blk, p, corner_w);

				for (uint32_t y = 0; y < 4; y++)
				{
					const int left_val = orig_weight_preds[index_from_xy(3, y)]; // left block's right edge

					for (uint32_t x = 0; x < 4; x++)
					{
						const int upper_val = upper_edge[x];

						if (cand_index == cCandGradient)
						{
							int grad = left_val + upper_val - corner_val;
							grad = basisu::clamp<int>(grad, 0, 64);

							weight_preds[index_from_xy(x, y)] = grad;
						}
						else if (cand_index == cCandGradientDamped)
						{
							int grad = left_val + upper_val - corner_val;
							grad = basisu::clamp<int>(grad, 0, 64);

							// Damped gradient: blend gradient with the proven #7 predictor.
							const int wl = 4 - static_cast<int>(x);
							const int wu = 4 - static_cast<int>(y);
							const int den = wl + wu;
							const int blend7 = (wl * left_val + wu * upper_val + (den >> 1)) / den;

							weight_preds[index_from_xy(x, y)] = (grad + blend7 + 1) >> 1;
						}
						else if (cand_index == cCandMED)
						{
							// MED (Median Edge Detector, JPEG-LS / LOCO-I).
							// Plane predictor wrapped in a per-sample edge switch:
							// if the corner is the local extreme, an edge passes
							// between the neighbors -- predict from the neighbor on
							// the current sample's side instead of extrapolating
							// through the edge (which is #12's overshoot failure).
							const int mn = basisu::minimum(left_val, upper_val);
							const int mx = basisu::maximum(left_val, upper_val);

							int pred;
							if (corner_val >= mx)
								pred = mn;
							else if (corner_val <= mn)
								pred = mx;
							else
								pred = left_val + upper_val - corner_val; // in (mn, mx) here, cannot overshoot

							weight_preds[index_from_xy(x, y)] = basisu::clamp<int>(pred, 0, 64);
						}
						else // cCandGAB
						{
							// GAB: gradient-adaptive blend (CALIC-spirit, 3 samples).
							// |left-corner| large => a horizontal edge crossed between
							// the corner row and this row, so the left sample is on the
							// current sample's side of it: trust it more. Symmetrically
							// for |upper-corner| and vertical edges. A self-normalizing
							// soft MED whose weights come from the data, not position.
							const int wl = basisu::iabs(left_val - corner_val) + 1;
							const int wu = basisu::iabs(upper_val - corner_val) + 1;
							const int den = wl + wu;

							const int pred = (wl * left_val + wu * upper_val + (den >> 1)) / den;

							weight_preds[index_from_xy(x, y)] = pred; // convex blend of in-range values: already in [0,64]
						}
					}
				}
			}
			else if (cand_index == cCandDiagAvg)
			{
				// Average upper-left and upper-right diagonal blocks.
				// pCand_log_blk is pLeft_diag_log_blk here, so orig_weight_preds contains upper-left.
				// Pull upper-right directly from pRight_diag_log_blk.

				for (uint32_t w = 0; w < 16; w++)
				{
					int right_diag_val;

					right_diag_val = fetch_w(pRight_diag_log_blk, p, w);

					weight_preds[w] = (orig_weight_preds[w] + right_diag_val + 1) >> 1;
				}
			}
			else if (cand_index == cCandDiagEdgeBlend)
			{
				// Blend upper-left block's right edge with upper-right block's left edge.
				// pCand_log_blk is pLeft_diag_log_blk, so orig_weight_preds contains upper-left.
				// Pull upper-right left edge directly from pRight_diag_log_blk.
				//
				// For each row y:
				//   L = upper-left[3,y]
				//   R = upper-right[0,y]
				// Then interpolate across x.

				int right_diag_left_edge[4];

				for (uint32_t y = 0; y < 4; y++)
				{
					const uint32_t w = index_from_xy(0, y); // upper-right block's left edge

					right_diag_left_edge[y] = fetch_w(pRight_diag_log_blk, p, w);
				}

				for (uint32_t y = 0; y < 4; y++)
				{
					const int left_val = orig_weight_preds[index_from_xy(3, y)]; // upper-left right edge
					const int right_val = right_diag_left_edge[y];                // upper-right left edge

					for (uint32_t x = 0; x < 4; x++)
					{
						// x=0 mostly left_val, x=3 mostly right_val.
						// Use 4-sample interpolation: 3/0, 2/1, 1/2, 0/3.
						const int pred = ((3 - static_cast<int>(x)) * left_val +
							static_cast<int>(x) * right_val + 1) / 3;

						weight_preds[index_from_xy(x, y)] = pred;
					}
				}
			}
			else if (cand_index == cCandUpperDiagEdgeBlend)
			{
				// Blend upper edge predictor with diagonal edge blend.
				//
				// upper_edge[x] = upper block's bottom edge
				// diag_blend[x,y] = horizontal interpolation between:
				//   upper-left block's right edge and upper-right block's left edge
				//
				// This combines direct top continuation with previous-row lateral structure.

				int upper_edge[4];
				int right_diag_left_edge[4];

				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t up_w = index_from_xy(x, 3); // upper block's bottom edge

					upper_edge[x] = fetch_w(pUp_log_blk, p, up_w);
				}

				for (uint32_t y = 0; y < 4; y++)
				{
					const uint32_t rd_w = index_from_xy(0, y); // upper-right block's left edge

					right_diag_left_edge[y] = fetch_w(pRight_diag_log_blk, p, rd_w);
				}

				for (uint32_t y = 0; y < 4; y++)
				{
					const int left_diag_right_val = orig_weight_preds[index_from_xy(3, y)]; // upper-left right edge
					const int right_diag_left_val = right_diag_left_edge[y];

					for (uint32_t x = 0; x < 4; x++)
					{
						// Same as #15: lateral predictor from upper-left/right diagonal edges.
						const int diag_blend =
							((3 - static_cast<int>(x)) * left_diag_right_val +
								static_cast<int>(x) * right_diag_left_val + 1) / 3;

						// Same as #6: direct upper edge replicated downward.
						const int up_val = upper_edge[x];

						// Trust upper edge more near y=0, trust diagonal lateral structure more lower in the block.
						const int wu = 4 - static_cast<int>(y); // 4,3,2,1
						const int wd = 1 + static_cast<int>(y); // 1,2,3,4
						const int den = wu + wd;                // always 5

						weight_preds[index_from_xy(x, y)] =
							(wu * up_val + wd * diag_blend + (den >> 1)) / den;
					}
				}
			}
			else if (cand_index == cCandPlaneFit)
			{
				// LS plane fit through left + upper edges.
				// pCand_log_blk is pLeft_log_blk, so orig_weight_preds holds the left block.
				int upper_edge[4];

				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t w = index_from_xy(x, 3); // upper block's bottom edge

					upper_edge[x] = fetch_w(pUp_log_blk, p, w);
				}

				int left_edge[4];
				for (uint32_t y = 0; y < 4; y++)
					left_edge[y] = orig_weight_preds[index_from_xy(3, y)]; // left block's right edge

				const int sum_u = upper_edge[0] + upper_edge[1] + upper_edge[2] + upper_edge[3];
				const int sum_l = left_edge[0] + left_edge[1] + left_edge[2] + left_edge[3];

				// LS slopes * 10
				const int gx10 = -3 * upper_edge[0] - upper_edge[1] + upper_edge[2] + 3 * upper_edge[3];
				const int gy10 = -3 * left_edge[0] - left_edge[1] + left_edge[2] + 3 * left_edge[3];

				const int base = 5 * (sum_u + sum_l); // mean*40, anchored at edge centroid (.25,.25)

				for (uint32_t y = 0; y < 4; y++)
				{
					for (uint32_t x = 0; x < 4; x++)
					{
						const int num = base + gx10 * (4 * (int)x - 1) + gy10 * (4 * (int)y - 1);

						// round-half-up = floor((num + 20) / 40); numerator can be negative
						// (negative slopes), and C++ '/' truncates toward zero, so force floor.
						const int t = num + 20;
						const int pred_unclamped = (t >= 0) ? (t / 40) : -((-t + 39) / 40);

						weight_preds[index_from_xy(x, y)] = basisu::clamp<int>(pred_unclamped, 0, 64);
					}
				}
			}
			else if (cand_index == cCandDDL)
			{
				// Diagonal-down-left (H.264 4x4 intra mode 3 analog): propagate the
				// extended top row (upper block's bottom edge + upper-RIGHT block's
				// bottom edge) down-left at 45 degrees, with 1-2-1 smoothing along the
				// diagonal. Captures 45-degree edges/stripes entering from the top-right
				// quadrant -- a direction nothing else in the bank can represent.
				// pCand_log_blk is pUp_log_blk; requires pRight_diag_log_blk.
				int T[8];

				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t w = index_from_xy(x, 3);

					T[x] = fetch_w(pUp_log_blk, p, w);

					T[4 + x] = fetch_w(pRight_diag_log_blk, p, w);
				}

				for (uint32_t y = 0; y < 4; y++)
				{
					for (uint32_t x = 0; x < 4; x++)
					{
						const uint32_t d = x + y; // 0..6
						int pred;

						if (d == 6)
							pred = (T[6] + 3 * T[7] + 2) >> 2;
						else
							pred = (T[d] + 2 * T[d + 1] + T[d + 2] + 2) >> 2;

						weight_preds[index_from_xy(x, y)] = pred; // smoothed avg of [0,64] values: in range
					}
				}
			}
			else if (cand_index == cCandDDR)
			{
				// Diagonal-down-right (H.264 4x4 intra mode 4 analog): propagate at 45
				// degrees from the top-left, sourcing the left column, the corner, and
				// the top row, 1-2-1 smoothed. Captures 45-degree structure entering
				// from the top-left quadrant.
				// pCand_log_blk is pLeft_log_blk; requires pUp_log_blk and pLeft_diag_log_blk.
				// A[0..8]: A[0..3] = left column bottom-to-top, A[4] = corner, A[5..8] = top row.
				int A[9];

				for (uint32_t y = 0; y < 4; y++)
					A[3 - y] = orig_weight_preds[index_from_xy(3, y)]; // left block's right edge, reversed

				{
					const uint32_t cw = index_from_xy(3, 3);
					A[4] = fetch_w(pLeft_diag_log_blk, p, cw);
				}

				for (uint32_t x = 0; x < 4; x++)
				{
					const uint32_t w = index_from_xy(x, 3);
					A[5 + x] = fetch_w(pUp_log_blk, p, w);
				}

				for (uint32_t y = 0; y < 4; y++)
				{
					for (uint32_t x = 0; x < 4; x++)
					{
						const int d = 4 + (int)x - (int)y; // 1..7; texels on diagonal k share A-index k
						const int pred = (A[d - 1] + 2 * A[d] + A[d + 1] + 2) >> 2;

						weight_preds[index_from_xy(x, y)] = pred;
					}
				}
			}

			pWeight_predictions = weight_preds;
		}

		if ((amp_code) && (pWeight_predictions))
		{
			// Amplitude code: generalization of the old inversion flag.
			// All transforms are about the prediction's own mean (per
			// plane), so they negate/scale the ACs while leaving the DC
			// for the DC coefficient -- the old 64-w flip also
			// complemented the DC, forcing a DC correction symbol
			// whenever the content mean wasn't complementary.
			int sum = 0;
			for (uint32_t i = 0; i < 16; i++)
				sum += pWeight_predictions[i];
			const int mean = (sum + 8) >> 4;

			for (uint32_t i = 0; i < 16; i++)
			{
				const int w = pWeight_predictions[i];
				int v;

				if (amp_code == 1)
				{
					// flip about mean (s = -1): pure AC negation
					v = basisu::clamp<int>(2 * mean - w, 0, 64);
					//v = 64 - w;
				}
				else if (amp_code == 2)
				{
					// half contrast (s = +1/2): mean + (w - mean)/2, half-up
					v = (w + mean + 1) >> 1; // closed in [0,64], no clamp needed
				}
				else // amp_code == 3
				{
					// half contrast of the flip (s = -1/2): compose 1 then 2
					const int f = basisu::clamp<int>(2 * mean - w, 0, 64);
					v = (f + mean + 1) >> 1;
				}

				pWeight_predictions[i] = v;
			}
		}

		if (!pWeight_predictions)
			return false;

		memcpy(pOut_preds, pWeight_predictions, 16 * sizeof(int));
		return true;
	}

	static inline int unpack_coeff_b(uint32_t v)
	{
		assert(v <= 255);
		return (int)v;
	}

	// LSB-first bit reader matching basisu::bitwise_coder's packing exactly
	// (first bit written lands in bit 0 of byte 0; values may cross byte
	// boundaries). Total: reads past the end return false.
	struct lsb_bit_reader
	{
		const uint8_t* m_p = nullptr;
		uint64_t m_total_bits = 0;
		uint64_t m_bit_ofs = 0;

		void init(const uint8_t* p, size_t size_bytes) { m_p = p; m_total_bits = (uint64_t)size_bytes * 8; m_bit_ofs = 0; }

		// seek to a bit range [start_bit, end_bit) within the blob -- used for
		// per-stripe decoding (the stripe's bits occupy exactly this range)
		void init_range(const uint8_t* p, uint64_t start_bit, uint64_t end_bit) { m_p = p; m_bit_ofs = start_bit; m_total_bits = end_bit; }

		bool get_bits(uint32_t n, uint32_t& v)
		{
			assert(n <= 32);
			v = 0;
			if (n > (m_total_bits - m_bit_ofs))
				return false;
			if (!n)
				return true;

			const uint64_t byte_idx = m_bit_ofs >> 3;
			const uint32_t bit_idx = (uint32_t)(m_bit_ofs & 7);
			const uint32_t num_bytes = (bit_idx + n + 7) >> 3; // 1..5

			uint64_t bits = 0;
			for (uint32_t i = 0; i < num_bytes; i++)
				bits |= ((uint64_t)m_p[byte_idx + i]) << (i * 8);

			v = (uint32_t)((bits >> bit_idx) & ((1ull << n) - 1ull));
			m_bit_ofs += n;
			return true;
		}

		// at most 7 zero pad bits may remain
		bool is_fully_consumed() const { return (m_total_bits - m_bit_ofs) < 8; }
	};

	// forward byte cursor over one blob; total
	struct byte_cursor
	{
		const uint8_t* m_p = nullptr;
		uint32_t m_size = 0, m_ofs = 0;

		void init(const blob_stream_reader& rdr, uint32_t id) { m_p = rdr.get_ptr(id); m_size = rdr.get_size(id); m_ofs = 0; }
		// seek to a byte range [start, end) within the blob -- per-stripe decode
		void init_range(const blob_stream_reader& rdr, uint32_t id, uint32_t start, uint32_t end) { m_p = rdr.get_ptr(id); m_ofs = start; m_size = end; }
		bool get(uint8_t& b) { if (m_ofs >= m_size) return false; b = m_p[m_ofs++]; return true; }
		bool is_fully_consumed() const { return m_ofs == m_size; }
	};

	bool blob_stream_reader::init_internal(const void* pData, size_t data_size, uint64_t max_total_uncomp)
	{
		clear();

		const uint8_t* pBytes = static_cast<const uint8_t*>(pData);

		if ((!pBytes) || (data_size < 3)) // magic + count + magic minimum
			return false;

		uint64_t ofs = 0;
		if (pBytes[ofs++] != BLOB_STREAM_MAGIC_BEGIN)
			return false;
		const uint32_t num_blobs = pBytes[ofs++];

		// pass 1: walk + validate the directory, total the arena size
		struct entry { uint32_t id, uncomp_size, stored_size; uint64_t data_ofs; };
		entry entries[255];
		uint64_t total_arena = 0;

		for (uint32_t i = 0; i < num_blobs; i++)
		{
			if (ofs >= data_size)
				return false;

			entry& e = entries[i];
			const uint8_t id_flag = pBytes[ofs++];
			e.id = id_flag & 0x7Fu;
			const bool compressed = (id_flag & 0x80u) != 0;

			if (!read_varint(pBytes, data_size, ofs, e.uncomp_size))
				return false;
			e.stored_size = 0;
			if (compressed)
			{
				if (!read_varint(pBytes, data_size, ofs, e.stored_size))
					return false;
				if (!e.stored_size)
					return false;
			}
			e.data_ofs = ofs;

			if (!e.uncomp_size)
				return false; // empty blobs are never stored

			if (m_sizes[e.id] || m_ptrs[e.id])
				return false; // duplicate id

			const uint64_t stored_bytes = e.stored_size ? e.stored_size : e.uncomp_size;
			if ((ofs + stored_bytes) > data_size)
				return false;
			ofs += stored_bytes;

			if (e.stored_size)
			{
				if (e.stored_size >= e.uncomp_size)
					return false; // compressed must be strictly smaller (writer guarantees it)
				total_arena += e.uncomp_size;
				if (total_arena > max_total_uncomp)
					return false;
			}

			// mark id as seen (real ptr/size set in pass 2)
			m_sizes[e.id] = e.uncomp_size;
			m_ptrs[e.id] = pBytes; // placeholder, nonzero for dup detection
		}

		// end marker must be the exact final byte: rejects truncation,
		// trailing garbage, and directory/data length disagreements
		if ((ofs != (data_size - 1)) || (pBytes[ofs] != BLOB_STREAM_MAGIC_END))
			return false;

		// pass 2: single arena allocation, decompress, wire up pointers
		if (!m_arena.try_resize((size_t)total_arena))
			return false;

		uint64_t arena_ofs = 0;
		for (uint32_t i = 0; i < num_blobs; i++)
		{
			const entry& e = entries[i];

			if (!e.stored_size)
			{
				m_ptrs[e.id] = pBytes + e.data_ofs; // raw: zero copy into input
			}
			else
			{
				uint8_t* pDst = m_arena.data() + arena_ofs;

#if BASISD_SUPPORT_KTX2_ZSTD
				const size_t res = ZSTD_decompress(pDst, e.uncomp_size, pBytes + e.data_ofs, e.stored_size);
				if (ZSTD_isError(res) || (res != e.uncomp_size))
					return false;
#else
				BASISU_NOTE_UNUSED(pDst);
				return false; // zstd disabled at compile time
#endif

				m_ptrs[e.id] = pDst;
				arena_ofs += e.uncomp_size;
			}
		}

		return true;
	}

	bool image_unpacker::init(
		const byte_span& comp,
		decode_init_callback_ptr pInit_callback, void* pInit_callback_data,
		decode_block_callback_ptr pBlock_callback, void* pBlock_callback_data)
	{
		m_initialized = false; // set true ONLY on full success below; every failure path leaves it false
		m_tiny_mip = false;
		m_block_cb = pBlock_callback;
		m_block_data = pBlock_callback_data;

#if !BASISD_SUPPORT_KTX2_ZSTD
		BASISU_DEVEL_ERROR("xbc7::unpack_image: XBC7 decoding requires zstd (BASISD_SUPPORT_KTX2_ZSTD is 0)");
		return false;
#endif

		// ---- format dispatch on the leading byte ----
		// 0xB7 = blob container (blob_stream_reader validates this marker itself).
		// 0xB8/0xB9 = tiny-mip ([marker][num_blocks_x:u8][num_blocks_y:u8] then 16
		//        bytes per packed BC7 block); 0xB8 = no alpha, 0xB9 = has alpha
		//        (the has_alpha bit rides in the marker). Anything else: not XBC7.
		if (comp.size() < 1)
			return false;

#ifndef NDEBUG
		// Debug builds: report the input buffer size and the stream type.
		{
			const char* pType = "regular (blob)";
			if (comp[0] == 0xB8)
				pType = "tiny-mip (no alpha)";
			else if (comp[0] == 0xB9)
				pType = "tiny-mip (alpha)";
			else if (comp[0] != BLOB_STREAM_MAGIC_BEGIN)
				pType = "unknown / invalid marker";
			fmt_debug_printf("XBC7 decode: input {} bytes, type: {}\n", (uint64_t)comp.size(), pType);
		}
#endif

		if ((comp[0] == 0xB8) || (comp[0] == 0xB9))
			return init_tiny_mip(comp, comp[0] == 0xB9, pInit_callback, pInit_callback_data);
		if (comp[0] != BLOB_STREAM_MAGIC_BEGIN)
			return false; // unknown format marker

		if (!m_rdr.init(comp.data(), comp.size()))
			return false;

		// ---- header ----
		if (m_rdr.get_size(cBlobHeader) != sizeof(xbc7_header))
			return false;

		xbc7_header hdr;
		memcpy((void *)&hdr, m_rdr.get_ptr(cBlobHeader), sizeof(hdr));

		const uint32_t width = hdr.m_width_in_texels;
		const uint32_t height = hdr.m_height_in_texels;
		m_global_q = hdr.m_dct_q;

		if ((!width) || (!height))
			return false;
		const uint32_t XBC7_MAX_SUPPORTED_DIM = 16384;
		if ((width > XBC7_MAX_SUPPORTED_DIM) || (height > XBC7_MAX_SUPPORTED_DIM))
			return false;
		if ((m_global_q < 1) || (m_global_q > 100))
			return false;
		if (hdr.m_flags & ~XBC7_FLAG_HAS_ALPHA)
			return false; // unknown flags

		m_width = width;
		m_height = height;
		m_has_alpha = (hdr.m_flags & XBC7_FLAG_HAS_ALPHA) != 0;
		m_num_blocks_x = (width + 3) / 4;
		m_num_blocks_y = (height + 3) / 4;
		const uint32_t total_blocks = m_num_blocks_x * m_num_blocks_y; // <= 16384^2, no overflow

		// ---- stripe geometry (also governs the implicit solid-block clamp) ----
		m_num_stripes = hdr.m_num_stripes;
		if ((!m_num_stripes) || (m_num_stripes > m_num_blocks_y))
			return false;
		if (m_num_stripes > XBC7_MAX_ENCODER_STRIPES)
			return false;
		compute_stripe_ranges(m_num_blocks_y, m_num_stripes, m_stripes);

		// ---- paranoia gate: structural size check BEFORE any allocation ----
		// One command byte per block makes this a necessary condition and
		// transitively bounds every allocation, killing the huge-dims DoS.
		if (m_rdr.get_size(cBlobCommands) != total_blocks)
			return false;

		// ---- per-stripe seek table ----
		// m_seek(id, s) = start offset of stripe s in blob id (a BYTE offset,
		// or a BIT offset for the three bit blobs); row m_num_stripes holds the
		// end sentinel (blob byte size, or total bits). Stripe 0 starts at 0.
		// Single-stripe files carry no table -- the whole blob is stripe 0.
		const uint32_t SEEK_NUM_STREAMS = (uint32_t)cBlobStripeSeekTable - 1; // ids 1..25
		m_seek.resize((uint32_t)cBlobStripeSeekTable, m_num_stripes + 1); // (id) x (stripe)
		m_seek.set_all(0);
		for (uint32_t id = 1; id < (uint32_t)cBlobStripeSeekTable; id++)
		{
			const bool bit_blob = (id == cBlobCoeffSigns) || (id == cBlobPBits) || (id == cBlobEPRaw);
			m_seek(id, m_num_stripes) = bit_blob ? ((uint64_t)m_rdr.get_size(id) * 8) : (uint64_t)m_rdr.get_size(id);
		}

		if (m_num_stripes > 1)
		{
			if (m_rdr.get_size(cBlobStripeSeekTable) != m_num_stripes * SEEK_NUM_STREAMS * 4)
				return false;

			// Entries are DELTAS from the previous stripe's start, stripe-major,
			// stored byte-plane (SoA) transposed: byte b of entry e lives at
			// pT[b * num_entries + e] (the encoder's inverse of this layout).
			// Reconstruct absolute offsets with a running prefix sum, validating
			// as we go (all BEFORE any block decode): stripe 0's delta must be 0,
			// and each running offset must stay within [0, blob_end] (blob_end =
			// byte size, or total bits for the three bit blobs). Monotonicity is
			// automatic -- deltas are unsigned, so the running sum never
			// decreases. The range test rejects a corrupted table immediately; a
			// wild byte/bit offset can never reach the cursor seek in decode_stripe.
			const uint8_t* pT = m_rdr.get_ptr(cBlobStripeSeekTable);
			const uint32_t num_entries = m_num_stripes * SEEK_NUM_STREAMS;
			for (uint32_t id = 1; id < (uint32_t)cBlobStripeSeekTable; id++)
			{
				const uint64_t blob_end = m_seek(id, m_num_stripes); // sentinel set above
				uint64_t running = 0;
				for (uint32_t st = 0; st < m_num_stripes; st++)
				{
					const uint32_t e = st * SEEK_NUM_STREAMS + (id - 1);
					const uint64_t delta = (uint64_t)pT[e] | ((uint64_t)pT[num_entries + e] << 8) |
						((uint64_t)pT[2u * num_entries + e] << 16) | ((uint64_t)pT[3u * num_entries + e] << 24);

					if ((st == 0) && (delta != 0))
						return false; // stripe 0 must start at offset 0

					running += delta; // unsigned add: offsets are monotonic by construction
					if (running > blob_end)
						return false; // offset past end of blob

					m_seek(id, st) = running;
				}
			}
		}

		// ---- decode state + callbacks ----
		// The decoder always maintains its own full-image logical-block buffer:
		// causal prediction (endpoint DPCM / repeat / weight predictors) reads
		// already-decoded neighbors. Finished blocks are streamed to the caller
		// via the block callback; the caller owns any persistent storage/packing.
		m_log_blks.resize(m_num_blocks_x, m_num_blocks_y);

		// Hand the caller the header geometry/metadata exactly once, before any
		// block, so it can validate and allocate. A false return aborts.
		if (pInit_callback)
		{
			if (!pInit_callback(m_num_blocks_x, m_num_blocks_y, m_width, m_height, m_global_q, m_has_alpha, pInit_callback_data))
				return false;
		}

		m_initialized = true;
		return true;
	}

	bool image_unpacker::init_tiny_mip(
		const byte_span& comp, bool has_alpha,
		decode_init_callback_ptr pInit_callback, void* pInit_callback_data)
	{
		if (comp.size() < 3)
			return false;

		const uint32_t nbx = comp[1];
		const uint32_t nby = comp[2];
		if ((!nbx) || (!nby))
			return false;

		// exact-size tripwire: the 3-byte header plus exactly the BC7 blocks
		const uint64_t expected = 3ull + (uint64_t)nbx * (uint64_t)nby * 16ull;
		if ((uint64_t)comp.size() != expected)
			return false;

		m_tiny_mip = true;
		// checked region pointer to all the BC7 blocks (validated once); aliases
		// the span's underlying buffer, which must outlive the decoder
		m_tiny_blocks = comp.checked_ptr(3, (size_t)nbx * (size_t)nby * 16); // -> first block
		m_num_blocks_x = nbx;
		m_num_blocks_y = nby;
		m_num_stripes = 1;
		m_width = nbx * 4;   // block-aligned; tiny-mip stores no exact texel dims
		m_height = nby * 4;
		m_global_q = 0;      // no DCT in tiny-mip
		m_has_alpha = has_alpha; // from the stream marker (0xB8 = no alpha, 0xB9 = alpha)

		if (pInit_callback)
		{
			if (!pInit_callback(m_num_blocks_x, m_num_blocks_y, m_width, m_height, m_global_q, m_has_alpha, pInit_callback_data))
				return false;
		}

		m_initialized = true;
		return true;
	}

	bool image_unpacker::decode_stripe(uint32_t stripe_index)
	{
		// must follow a successful init() -- guard in ALL builds (assert traps
		// the bug in debug; the return false keeps release total)
		assert(m_initialized && "decode_stripe() called without a successful init()");
		if (!m_initialized)
			return false;

		assert(stripe_index < m_num_stripes);
		if (stripe_index >= m_num_stripes)
			return false;

		// tiny-mip is a single conceptual stripe (m_num_stripes == 1): decode all
		// its blocks straight from the raw BC7 data, no prediction/blobs involved.
		if (m_tiny_mip)
			return decode_tiny_mip();

		const uint32_t first_row = m_stripes[stripe_index].m_first_block_row;
		const uint32_t end_row = first_row + m_stripes[stripe_index].m_num_block_rows;

		// every reference is clamped to this stripe AABB (mirrors the encoder)
		const tile_bounds stripe_tile = { 0, (int)first_row, (int)m_num_blocks_x - 1, (int)end_row - 1 };

		// aliases so the per-block body below resolves to member state
		blob_stream_reader& rdr = m_rdr;
		const uint32_t num_blocks_x = m_num_blocks_x;
		const uint32_t global_q = m_global_q;
		const bool has_alpha = m_has_alpha;
		vector2D<basist::bc7u::log_bc7_block>& log_blks = m_log_blks;

		// cursors seeked to this stripe's range [seek(id,s), seek(id,s+1))
		const uint32_t s = stripe_index;
		byte_cursor commands;		commands.init_range(rdr, cBlobCommands, (uint32_t)m_seek(cBlobCommands, s), (uint32_t)m_seek(cBlobCommands, s + 1));
		byte_cursor configs;		configs.init_range(rdr, cBlobBC7BlockConfig, (uint32_t)m_seek(cBlobBC7BlockConfig, s), (uint32_t)m_seek(cBlobBC7BlockConfig, s + 1));
		byte_cursor partitions2;	partitions2.init_range(rdr, cBlobPartition2, (uint32_t)m_seek(cBlobPartition2, s), (uint32_t)m_seek(cBlobPartition2, s + 1));
		byte_cursor partitions3;	partitions3.init_range(rdr, cBlobPartition3, (uint32_t)m_seek(cBlobPartition3, s), (uint32_t)m_seek(cBlobPartition3, s + 1));
		byte_cursor predictors;		predictors.init_range(rdr, cBlobWeightPredictors, (uint32_t)m_seek(cBlobWeightPredictors, s), (uint32_t)m_seek(cBlobWeightPredictors, s + 1));
		byte_cursor dc_coeffs;		dc_coeffs.init_range(rdr, cBlobDCCoeffsSmall, (uint32_t)m_seek(cBlobDCCoeffsSmall, s), (uint32_t)m_seek(cBlobDCCoeffsSmall, s + 1)); // encoder writes ALL DC here
		byte_cursor ac_coeffs;		ac_coeffs.init_range(rdr, cBlobACCoeffs, (uint32_t)m_seek(cBlobACCoeffs, s), (uint32_t)m_seek(cBlobACCoeffs, s + 1));
		byte_cursor solid_deltas;	solid_deltas.init_range(rdr, cBlobSolidRGBADeltas, (uint32_t)m_seek(cBlobSolidRGBADeltas, s), (uint32_t)m_seek(cBlobSolidRGBADeltas, s + 1));
		byte_cursor ep_block_index;	ep_block_index.init_range(rdr, cBlobEPBlockIndex, (uint32_t)m_seek(cBlobEPBlockIndex, s), (uint32_t)m_seek(cBlobEPBlockIndex, s + 1));
		byte_cursor raw_weights;	raw_weights.init_range(rdr, cBlobRawWeightBits, (uint32_t)m_seek(cBlobRawWeightBits, s), (uint32_t)m_seek(cBlobRawWeightBits, s + 1));

		byte_cursor wt_resid[3];	// DPCM weight residuals by bit width: [0]=2, [1]=3, [2]=4
		wt_resid[0].init_range(rdr, cBlobDPCMWeightResid2, (uint32_t)m_seek(cBlobDPCMWeightResid2, s), (uint32_t)m_seek(cBlobDPCMWeightResid2, s + 1));
		wt_resid[1].init_range(rdr, cBlobDPCMWeightResid3, (uint32_t)m_seek(cBlobDPCMWeightResid3, s), (uint32_t)m_seek(cBlobDPCMWeightResid3, s + 1));
		wt_resid[2].init_range(rdr, cBlobDPCMWeightResid4, (uint32_t)m_seek(cBlobDPCMWeightResid4, s), (uint32_t)m_seek(cBlobDPCMWeightResid4, s + 1));

		byte_cursor ep_deltas[8]; // [0..3] = Fine R,G,B,A; [4..7] = Coarse R,G,B,A
		for (uint32_t i = 0; i < 4; i++)
		{
			ep_deltas[i].init_range(rdr, cBlobEPDeltaFineR + i, (uint32_t)m_seek(cBlobEPDeltaFineR + i, s), (uint32_t)m_seek(cBlobEPDeltaFineR + i, s + 1));
			ep_deltas[4 + i].init_range(rdr, cBlobEPDeltaCoarseR + i, (uint32_t)m_seek(cBlobEPDeltaCoarseR + i, s), (uint32_t)m_seek(cBlobEPDeltaCoarseR + i, s + 1));
		}

		lsb_bit_reader coeff_signs;	coeff_signs.init_range(rdr.get_ptr(cBlobCoeffSigns), m_seek(cBlobCoeffSigns, s), m_seek(cBlobCoeffSigns, s + 1));
		lsb_bit_reader pbits;		pbits.init_range(rdr.get_ptr(cBlobPBits), m_seek(cBlobPBits, s), m_seek(cBlobPBits, s + 1));
		lsb_bit_reader ep_raw;		ep_raw.init_range(rdr.get_ptr(cBlobEPRaw), m_seek(cBlobEPRaw, s), m_seek(cBlobEPRaw, s + 1));

		xbc7::xbc7_weight_grid_dct_fixed weight_grid_dct_fixed;
		weight_grid_dct_fixed.init();
		xbc7::fxvec dct_work_fixed;

		for (uint32_t by = first_row; by < end_row; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				const basist::bc7u::log_bc7_block* pLeft_log_blk = stripe_tile.contains((int)bx - 1, (int)by) ? &log_blks(bx - 1, by) : nullptr;
				const basist::bc7u::log_bc7_block* pUp_log_blk = stripe_tile.contains((int)bx, (int)by - 1) ? &log_blks(bx, by - 1) : nullptr;
				const basist::bc7u::log_bc7_block* pLeft_diag_log_blk = stripe_tile.contains((int)bx - 1, (int)by - 1) ? &log_blks(bx - 1, by - 1) : nullptr;
				const basist::bc7u::log_bc7_block* pRight_diag_log_blk = stripe_tile.contains((int)bx + 1, (int)by - 1) ? &log_blks(bx + 1, by - 1) : nullptr;

				basist::bc7u::log_bc7_block& log_blk = log_blks(bx, by);

				uint8_t cmd_byte;
				if (!commands.get(cmd_byte))
					return false;

				const uint32_t cmd = cmd_byte & 7;
				const uint32_t ep_mode = (cmd_byte >> XBC7_COMMAND_ENDPOINT_MODE_SHIFT) & 7;
				const uint32_t wt_mode = (cmd_byte >> XBC7_COMMAND_WEIGHT_MODE_SHIFT) & 1;

				if (cmd_byte & 0x80)
					return false; // P-frame flag: reserved in v0

				// ---- simple commands: the whole byte is the command id ----
				if (cmd <= (uint32_t)xbc7_command_id::cCmdSolidDPCM)
				{
					if (cmd_byte != cmd)
						return false; // canonical: simple commands carry no EP/WT bits

					if (cmd == (uint32_t)xbc7_command_id::cCmdRepeatLast)
					{
						if (!pLeft_log_blk)
							return false;
						log_blk = *pLeft_log_blk;
					}
					else if (cmd == (uint32_t)xbc7_command_id::cCmdRepeatUpper)
					{
						if (!pUp_log_blk)
							return false;
						log_blk = *pUp_log_blk;
					}
					else // cCmdSolidDPCM
					{
						// neighbor edge-average prediction in decoded-pixel space;
						// mirrors the encoder exactly -- INCLUDING its stripe
						// seam clamp, since this prediction is implicit (the
						// encoder cannot read the stripe above, so neither may we)
						int preds[4] = { 0, 0, 0, 0 };
						int num_preds = 0;

						if (pLeft_log_blk)
						{
							for (uint32_t y = 0; y < 4; y++)
							{
								basist::color_rgba px;
								if (!basist::bc7u::unpack_bc7_texel(*pLeft_log_blk, px, 3, y))
									return false;
								preds[0] += px.r; preds[1] += px.g; preds[2] += px.b; preds[3] += px.a;
							}
							num_preds += 4;
						}

						if (pUp_log_blk) // pUp is already null above the stripe's first row
						{
							for (uint32_t x = 0; x < 4; x++)
							{
								basist::color_rgba px;
								if (!basist::bc7u::unpack_bc7_texel(*pUp_log_blk, px, x, 3))
									return false;
								preds[0] += px.r; preds[1] += px.g; preds[2] += px.b; preds[3] += px.a;
							}
							num_preds += 4;
						}

						if (num_preds)
						{
							for (uint32_t c = 0; c < 4; c++)
								preds[c] = (preds[c] + (num_preds / 2)) / num_preds;
						}

						basist::color_rgba solid_color;
						for (uint32_t c = 0; c < (has_alpha ? 4u : 3u); c++)
						{
							uint8_t delta;
							if (!solid_deltas.get(delta))
								return false;
							solid_color[c] = (uint8_t)(delta + preds[c]);
						}
						if (!has_alpha)
							solid_color.a = 255;

						basist::bc7u::create_solid_blk(log_blk, solid_color);
					}
				}
				else
				{
					// ---- config ----
					if (cmd == (uint32_t)xbc7_command_id::cCmdNewConfig)
					{
						uint8_t config_byte;
						if (!configs.get(config_byte))
							return false;
						if (config_byte & 0xC0)
							return false; // reserved bits

						const uint32_t mode = config_byte & 7;
						const uint32_t rot = (config_byte >> 3) & 3;
						const uint32_t sel = (config_byte >> 5) & 1;

						basist::bc7u::init_log_blk(log_blk, mode);

						if (log_blk.m_num_planes == 2)
							log_blk.m_dp_rotation_index = (uint8_t)rot;
						else if (rot)
							return false; // rotation on a non-dual-plane mode

						if (mode == 4)
							log_blk.m_mode4_index_selector = (uint8_t)sel;
						else if (sel)
							return false; // selector outside mode 4
					}
					else // reuse config from a neighbor
					{
						const basist::bc7u::log_bc7_block* pSrc = nullptr;

						if (cmd == (uint32_t)xbc7_command_id::cCmdReuseConfigLeft)
							pSrc = pLeft_log_blk;
						else if (cmd == (uint32_t)xbc7_command_id::cCmdReuseConfigUpper)
							pSrc = pUp_log_blk;
						else if (cmd == (uint32_t)xbc7_command_id::cCmdReuseConfigLeftDiagonal)
							pSrc = pLeft_diag_log_blk;
						else // cCmdReuseConfigRightDiagonal
							pSrc = pRight_diag_log_blk;

						if (!pSrc)
							return false;

						basist::bc7u::init_log_blk(log_blk, pSrc->m_mode);
						log_blk.m_dp_rotation_index = pSrc->m_dp_rotation_index;
						log_blk.m_mode4_index_selector = pSrc->m_mode4_index_selector;
					}

					// ---- partition index (always sent for partitioned modes) ----
					if (log_blk.m_num_partitions == 2)
					{
						uint8_t pat;
						if (!partitions2.get(pat))
							return false;
						if (pat >= 64)
							return false;
						log_blk.m_pattern_index = pat;
					}
					else if (log_blk.m_num_partitions == 3)
					{
						uint8_t pat;
						if (!partitions3.get(pat))
							return false;
						if (pat >= (1u << log_blk.m_pattern_bits))
							return false; // mode 0: index < 16
						log_blk.m_pattern_index = pat;
					}

					const basist::bc7u::endpoint_format& fmt = basist::bc7u::g_endpoint_formats[log_blk.m_mode];
					const uint32_t num_comps = log_blk.get_num_comps();

					// ---- endpoints ----
					if (ep_mode == (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointRaw)
					{
						for (uint32_t subset = 0; subset < log_blk.m_num_partitions; subset++)
						{
							for (uint32_t c = 0; c < num_comps; c++)
							{
								for (uint32_t e = 0; e < 2; e++)
								{
									uint32_t v;
									if (!ep_raw.get_bits(log_blk.m_endpoint_bits[c == 3], v))
										return false;
									log_blk.m_endpoints[subset][e][c] = (uint8_t)v;
								}
							}
						}

						for (uint32_t pb = 0; pb < log_blk.m_num_pbits; pb++)
						{
							uint32_t v;
							if (!ep_raw.get_bits(1, v))
								return false;
							if (pb < 6) // to shut up gcc bogus warning
								log_blk.m_pbits[pb] = (uint8_t)v;
						}
					}
					else
					{
						// resolve the endpoint predictor block (mirrors the encoder
						// sweep); ep_mode is 3 bits, and 1..7 are all DPCM modes
						const basist::bc7u::log_bc7_block* pEP_pred_blk = nullptr;
						uint32_t ep_pred_subset = 0;

						if (ep_mode == (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMLeft)
							pEP_pred_blk = pLeft_log_blk;
						else if (ep_mode == (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMUp)
							pEP_pred_blk = pUp_log_blk;
						else if (ep_mode == (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMLeftDiagonal)
							pEP_pred_blk = pLeft_diag_log_blk;
						else if (ep_mode == (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMRightDiagonal)
							pEP_pred_blk = pRight_diag_log_blk;
						else if (ep_mode == (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMLeftSubset1)
						{
							pEP_pred_blk = pLeft_log_blk;
							ep_pred_subset = 1;
						}
						else if (ep_mode == (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMUpSubset1)
						{
							pEP_pred_blk = pUp_log_blk;
							ep_pred_subset = 1;
						}
						else // cCmdEndpointDPCMBlockIndex
						{
							uint8_t delta_index;
							if (!ep_block_index.get(delta_index))
								return false;
							if (delta_index >= NUM_XY_DELTAS)
								return false; // top 3 bits reserved-zero

							const xbc7_xy_delta& delta = g_xbc7_xy_deltas[delta_index];
							const int nx = (int)bx + delta.m_dx;
							const int ny = (int)by + delta.m_dy;

							// must reference a block inside this stripe (the encoder
							// never emits a cross-stripe reference); also keeps a
							// worker from reading another stripe's rows
							if (!stripe_tile.contains(nx, ny))
								return false; // reference outside the stripe: malformed stream

							pEP_pred_blk = &log_blks(nx, ny);
						}

						if (!pEP_pred_blk)
							return false;

						// subset-1 references require a partitioned predictor
						if ((ep_pred_subset) && (pEP_pred_blk->m_num_partitions < 2))
							return false;

						const bool fine = (log_blk.m_endpoint_bits[0] >= 6);

						for (uint32_t subset = 0; subset < log_blk.m_num_partitions; subset++)
						{
							uint8_t residuals[8];
							uint32_t num_residuals = num_comps * 2;
							if ((!has_alpha) && (log_blk.m_mode == 6))
							{
								assert(num_residuals == 8);
								num_residuals = 6;
								residuals[6] = 0;
								residuals[7] = 0;
							}

							for (uint32_t i = 0; i < num_residuals; i += 2)
							{
								const uint32_t chan = i >> 1;
								byte_cursor& strm = ep_deltas[(fine ? 0 : 4) + chan];
								if (!strm.get(residuals[i + 0]))
									return false;
								if (!strm.get(residuals[i + 1]))
									return false;
							}

							uint8_t residual_pbits[2] = { 0, 0 };
							for (uint32_t pb = 0; pb < fmt.m_num_pbits; pb++)
							{
								uint32_t v;
								if (!pbits.get_bits(1, v))
									return false;
								residual_pbits[pb] = (uint8_t)v;
							}

							uint32_t num_residuals_out, num_residual_pbits_out;
							basist::bc7u::endpoint_dpcm(true,
								*pEP_pred_blk, ep_pred_subset, // mirrors the encoder (subset 1 for EP modes 6/7)
								log_blk, subset,
								residuals, num_residuals_out, residual_pbits, num_residual_pbits_out);
							if ((!has_alpha) && (log_blk.m_mode == 6))
							{
								log_blk.m_endpoints[0][0][3] = 127;
								log_blk.m_endpoints[0][1][3] = 127;
							}
						}
					}

					// ---- weights: residual DCT (wt bit 1) or lossless residual DPCM (wt bit 0) ----
					uint8_t pred_byte;
					if (!predictors.get(pred_byte))
						return false;

					if (pred_byte >= cTotalCandidates * 4)
						return false;

					const uint32_t cand_index = pred_byte % cTotalCandidates;
					const uint32_t amp_code = pred_byte / cTotalCandidates;

					if ((amp_code) && (cand_index == cCandAbsolute))
						return false;

					int weight_preds[16];
					int* pWeight_predictions = nullptr;

					for (uint32_t p = 0; p < log_blk.m_num_planes; p++)
					{
						if (cand_index != cCandAbsolute)
						{
							// the decoder may reference ANY causal block (whole-image
							// tile): the encoder simply never emits a reference
							// that crosses one of its tile boundaries
							if (!eval_weight_predictor(cand_index, amp_code, bx, by, num_blocks_x, stripe_tile, log_blks, p, weight_preds))
								return false; // candidate unavailable here: malformed stream
							pWeight_predictions = weight_preds;
						}

						if (wt_mode != (uint32_t)xbc7_command_weight_mode::cCmdWeightDCT)
						{
							// ---- lossless residual DPCM weights ----
							// Predictions are quantized to the plane's bit depth and
							// the wrapped n-bit index residuals (or, for the absolute
							// predictor, the raw indices) are read back. Exact.
							const uint32_t num_bits = log_blk.m_weight_bits[p];
							const uint32_t mask = (1u << num_bits) - 1;

							byte_cursor& strm = (cand_index == cCandAbsolute) ? raw_weights : wt_resid[num_bits - 2];

							uint8_t syms[16];
							if (num_bits == 2)
							{
								for (uint32_t i = 0; i < 16; i += 4)
								{
									uint8_t b;
									if (!strm.get(b))
										return false;
									syms[i + 0] = b & 3;
									syms[i + 1] = (b >> 2) & 3;
									syms[i + 2] = (b >> 4) & 3;
									syms[i + 3] = (uint8_t)(b >> 6);
								}
							}
							else
							{
								for (uint32_t i = 0; i < 16; i += 2)
								{
									uint8_t b;
									if (!strm.get(b))
										return false;
									const uint8_t lo = b & 0xF, hi = (uint8_t)(b >> 4);
									if ((num_bits == 3) && ((lo > 7) || (hi > 7)))
										return false; // nibble bit 3 reserved-zero
									syms[i + 0] = lo;
									syms[i + 1] = hi;
								}
							}

							for (uint32_t i = 0; i < 16; i++)
							{
								const uint32_t pred_index = pWeight_predictions ?
									basist::bc7u::quant_weight(pWeight_predictions[i], num_bits) : 0;
								log_blk.m_weights[p][i] = (uint8_t)((syms[i] + pred_index) & mask);
							}

							continue; // next plane
						}

						// ---- DC ----
						uint8_t dc_byte;
						if (!dc_coeffs.get(dc_byte))
							return false;

						int dc = unpack_coeff_b(dc_byte);

						if (pred_byte != cCandAbsolute) // joint index 0 == absolute: DC is unsigned
						{
							uint32_t sign;
							if (!coeff_signs.get_bits(1, sign))
								return false;
							if (sign)
								dc = -dc;
						}

						// ---- ACs ----
						xbc7::dct_syms syms;
						syms.clear();
						syms.m_dc = (int16_t)dc;

						uint32_t zig_idx = 1;

						while (zig_idx < 16)
						{
							uint8_t b;
							if (!ac_coeffs.get(b))
								return false;

							if (b == 0xFF)
							{
								// trailing zeros to the end of the scan
								xbc7::coeff cf;
								cf.m_num_zeros = (int16_t)(16 - zig_idx);
								cf.m_coeff = INT16_MAX;
								syms.m_ac_vals.push_back(cf);
								break;
							}

							const uint32_t run = b;
							if ((zig_idx + run) > 15)
								return false; // a real coefficient must land at position <= 15

							uint8_t mag_byte;
							if (!ac_coeffs.get(mag_byte))
								return false;

							const int mag = unpack_coeff_b(mag_byte);
							if (!mag)
								return false; // zero coefficients are never coded

							uint32_t sign;
							if (!coeff_signs.get_bits(1, sign))
								return false;

							xbc7::coeff cf;
							cf.m_num_zeros = (int16_t)run;
							cf.m_coeff = (int16_t)(sign ? -mag : mag);
							syms.m_ac_vals.push_back(cf);

							zig_idx += run + 1;
						}

						bool status = weight_grid_dct_fixed.inverse(
							basist::fixed16_16::from_int(global_q), p, pWeight_predictions, syms, log_blk, dct_work_fixed);
						if (!status)
							return false;

					} // p
				}
								
				// ---- single emit point: every block path (repeat-last,
				// repeat-upper, solid, config) falls through to here, so the
				// caller's block callback is invoked EXACTLY ONCE per block,
				// after the logical block is stored in m_log_blks. The caller may
				// pack a physical BC7 block, store it, compare it vs a reference,
				// transcode it, etc. A false return aborts the decode. ----
				if (m_block_cb && !m_block_cb(bx, by, log_blk, m_block_data))
					return false;

			} // bx
		} // by

		// ---- per-stripe desync tripwires: each stream consumed exactly its
		// range (cursors were bounded to [seek(s), seek(s+1))) ----
		if (!commands.is_fully_consumed()) return false;
		if (!configs.is_fully_consumed()) return false;
		if (!partitions2.is_fully_consumed()) return false;
		if (!partitions3.is_fully_consumed()) return false;
		if (!predictors.is_fully_consumed()) return false;
		if (!dc_coeffs.is_fully_consumed()) return false;
		if (!ac_coeffs.is_fully_consumed()) return false;
		if (!solid_deltas.is_fully_consumed()) return false;
		if (!ep_block_index.is_fully_consumed()) return false;
		if (!raw_weights.is_fully_consumed()) return false;
		for (uint32_t i = 0; i < 3; i++)
			if (!wt_resid[i].is_fully_consumed()) return false;
		for (uint32_t i = 0; i < 8; i++)
			if (!ep_deltas[i].is_fully_consumed()) return false;
		if (!coeff_signs.is_fully_consumed()) return false;
		if (!pbits.is_fully_consumed()) return false;
		if (!ep_raw.is_fully_consumed()) return false;

		return true;
	} // decode_stripe

	bool image_unpacker::decode_tiny_mip()
	{
		for (uint32_t by = 0; by < m_num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < m_num_blocks_x; bx++)
			{
				const uint8_t* p = m_tiny_blocks + ((size_t)by * m_num_blocks_x + bx) * 16;

				basist::bc7u::log_bc7_block log_blk;
				if (!basist::bc7u::unpack_bc7(p, log_blk))
					return false;

				if (m_block_cb && !m_block_cb(bx, by, log_blk, m_block_data))
					return false;
			}
		}
		return true;
	}

	bool image_unpacker::decode_all()
	{
		// guard here too: with no successful init(), m_num_stripes is 0 and the
		// loop would otherwise return true (false success) without decoding
		assert(m_initialized && "decode_all() called without a successful init()");
		if (!m_initialized)
			return false;

		for (uint32_t s = 0; s < m_num_stripes; s++)
			if (!decode_stripe(s))
				return false;
		return true;
	}

	// Single-threaded one-shot: prep + serial decode.
	bool unpack_image(
		const byte_span& comp,
		decode_init_callback_ptr pInit_callback, void* pInit_callback_data,
		decode_block_callback_ptr pBlock_callback, void* pBlock_callback_data)
	{
#if !BASISD_SUPPORT_KTX2_ZSTD
		BASISU_NOTE_UNUSED(comp); BASISU_NOTE_UNUSED(pInit_callback); BASISU_NOTE_UNUSED(pInit_callback_data);
		BASISU_NOTE_UNUSED(pBlock_callback); BASISU_NOTE_UNUSED(pBlock_callback_data);
		BASISU_DEVEL_ERROR("xbc7::unpack_image: XBC7 decoding requires zstd (BASISD_SUPPORT_KTX2_ZSTD is 0)");
		return false;
#else
		image_unpacker dec;
		if (!dec.init(comp, pInit_callback, pInit_callback_data, pBlock_callback, pBlock_callback_data))
			return false;
		return dec.decode_all();
#endif
	}

	// Threaded: caller owns `dec` (it must outlive the spawned jobs). init + spawn
	// one job per stripe via the spawner; returns false only on init failure and
	// does NOT wait. The caller waits on its own pool, then inspects results.
	bool unpack_image_threaded(
		image_unpacker& dec,
		const byte_span& comp,
		job_spawner& spawner,
		decode_init_callback_ptr pInit_callback, void* pInit_callback_data,
		decode_block_callback_ptr pBlock_callback, void* pBlock_callback_data)
	{
#if !BASISD_SUPPORT_KTX2_ZSTD
		BASISU_NOTE_UNUSED(dec); BASISU_NOTE_UNUSED(comp); BASISU_NOTE_UNUSED(spawner);
		BASISU_NOTE_UNUSED(pInit_callback); BASISU_NOTE_UNUSED(pInit_callback_data);
		BASISU_NOTE_UNUSED(pBlock_callback); BASISU_NOTE_UNUSED(pBlock_callback_data);
		BASISU_DEVEL_ERROR("xbc7::unpack_image_threaded: XBC7 decoding requires zstd (BASISD_SUPPORT_KTX2_ZSTD is 0)");
		return false;
#else
		if (!dec.init(comp, pInit_callback, pInit_callback_data, pBlock_callback, pBlock_callback_data))
			return false;
		const uint32_t num_stripes = dec.get_num_stripes();
		for (uint32_t stripe_index = 0; stripe_index < num_stripes; stripe_index++)
			spawner.spawn_job(dec, stripe_index);
		return true;
#endif
	}

} // namespace xbc7
} // namespace basist
