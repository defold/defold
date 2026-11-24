// File: basisu_xbc7_encode.cpp
#include "basisu_xbc7_encode.h"
#include "../transcoder/basisu_transcoder.h"
#include "../zstd/zstd.h"
#include "../transcoder/basisu_xbc7_decoder.h"
#include "basisu_bc7e_scalar.h"

using basist::fixed16_16;

namespace basisu {

namespace xbc7
{
	using namespace basist::xbc7; // shared XBC7 defs (DCT/syms/enums/eval) now live in basist::xbc7

		

	class blob_stream_writer
	{
	public:
		blob_stream_writer() {}

		void clear() { m_blobs.clear(); }

		inline uint8_vec& get_blob_vec(uint32_t id)
		{
			assert(id < BLOB_STREAM_MAX_IDS);
			if (id >= m_blobs.size())
				m_blobs.resize(id + 1);
			return m_blobs[id];
		}

		// Hot path: append one byte to blob `id`. id must be < 128.
		inline void put_byte(uint32_t id, uint8_t b)
		{
			assert(id < BLOB_STREAM_MAX_IDS);
			if (id >= m_blobs.size())
				m_blobs.resize(id + 1);
			m_blobs[id].push_back(b);
		}

		inline void put_bytes(uint32_t id, const void* p, size_t n)
		{
			assert(id < BLOB_STREAM_MAX_IDS);
			if (id >= m_blobs.size())
				m_blobs.resize(id + 1);
			m_blobs[id].append(static_cast<const uint8_t*>(p), n);
		}

		void append_bytes(uint32_t id, const uint8_vec& blob)
		{
			assert(id < BLOB_STREAM_MAX_IDS);
			if (id >= m_blobs.size())
				m_blobs.resize(id + 1);
			m_blobs[id].append(blob);
		}

		// for the per-stream byte accounting / stats dashboard
		inline size_t get_blob_size(uint32_t id) const { return (id < m_blobs.size()) ? m_blobs[id].size() : 0; }
		inline uint32_t get_num_ids() const { return m_blobs.size_u32(); }
		inline const uint8_vec* get_blob_data(uint32_t id) const { return (id < m_blobs.size()) ? &m_blobs[id] : nullptr; }

		// Compress (where it helps) and serialize everything to `out`.
		// Returns false only on internal error (blob > 4GB etc).
		bool serialize(uint8_vec& out, int zstd_level = 19, uint32_t* pStored_sizes = nullptr) const // pStored_sizes: optional [BLOB_STREAM_MAX_IDS] array, receives stored (post-compression) payload bytes per id
		{
			out.resize(0);

			uint32_t num_stored = 0;
			for (uint32_t id = 0; id < m_blobs.size(); id++)
				if (m_blobs[id].size())
					num_stored++;

			if (num_stored > 255)
				return false;

			out.push_back(BLOB_STREAM_MAGIC_BEGIN);
			out.push_back((uint8_t)num_stored);

			uint8_vec comp_buf;

			for (uint32_t id = 0; id < m_blobs.size(); id++)
			{
				const uint8_vec& blob = m_blobs[id];
				if (!blob.size())
					continue;

				if (blob.size() > UINT32_MAX)
					return false;

				const uint32_t uncomp_size = (uint32_t)blob.size();

				// Try Zstd; keep it only if it's worth it. Small blobs keep any
				// win, but a LARGE blob must shrink by at least 1/400 (0.25%):
				// raw blobs are zero-copy at decode (no Zstd frame to walk, no
				// arena bytes), so a marginal win costs more than it saves.
				const uint8_t* pStored = blob.data();
				uint32_t stored_size = 0; // 0 == raw

				const size_t bound = ZSTD_compressBound(blob.size());
				comp_buf.resize(bound);

				const size_t comp_res = ZSTD_compress(comp_buf.data(), bound, blob.data(), blob.size(), zstd_level);
				if ((!ZSTD_isError(comp_res)) && (comp_res < blob.size()))
				{
					const uint32_t BLOB_RATIO_CHECK_MIN_SIZE = 128;	// blobs <= this keep any win
					const uint32_t BLOB_MIN_SAVINGS_DENOM = 400;	// larger blobs must save >= size/400

					const uint64_t saved_bytes = (uint64_t)blob.size() - comp_res;

					if ((blob.size() <= BLOB_RATIO_CHECK_MIN_SIZE) || ((saved_bytes * BLOB_MIN_SAVINGS_DENOM) >= (uint64_t)blob.size()))
					{
						pStored = comp_buf.data();
						stored_size = (uint32_t)comp_res;
					}
				}

				if (pStored_sizes)
					pStored_sizes[id] = stored_size ? stored_size : uncomp_size;

				// directory entry: id+flag byte, then varint size(s)
				out.push_back((uint8_t)(id | (stored_size ? 0x80u : 0u)));
				if (stored_size)
				{
					push_varint(out, uncomp_size);
					push_varint(out, stored_size);
				}
				else
					push_varint(out, uncomp_size);

				out.append(pStored, stored_size ? stored_size : uncomp_size);
			}

			out.push_back(BLOB_STREAM_MAGIC_END);

			return true;
		}

	private:
		basisu::vector<uint8_vec> m_blobs;

		static inline void push_varint(uint8_vec& v, uint32_t x)
		{
			while (x >= 0x80u)
			{
				v.push_back((uint8_t)(x | 0x80u));
				x >>= 7;
			}
			v.push_back((uint8_t)x);
		}
	};



static const char* g_cand_names[cCandFirstXYDelta] =
{
	"absolute", "left edge", "upper edge", "L+U blend", "reflect left", "reflect upper",
	"L+U avg", "L+U strong blend", "gradient", "damped gradient", "diag avg",
	"diag edge blend", "upper+diag edge blend", "MED", "GAB", "plane fit",
	"diag down-left", "diag down-right"
};


// Post-quantization DCT coefficient statistics, collected from the WINNING
// candidate of each non-solid block, per coded plane. Gives the picture the
// run/coeff/sign stream design needs: where surviving ACs sit in zigzag
// order, their amplitudes and signs, run lengths, and DC behavior.
struct xbc7_dct_stats
{
	uint64_t m_total_planes = 0;

	// DC (range is [-256, 256] by construction)
	uint64_t m_dc_zero = 0, m_dc_pos = 0, m_dc_neg = 0;
	uint64_t m_dc_sum_abs = 0;
	int m_dc_min = 257, m_dc_max = -257;
	uint64_t m_dc_mag_hist[10] = {};

	// AC
	uint64_t m_total_acs = 0;
	uint64_t m_ac_pos = 0, m_ac_neg = 0;
	uint64_t m_ac_sum_abs = 0;
	int m_ac_min = 257, m_ac_max = -257;
	uint64_t m_ac_mag_hist[10] = {};
	uint64_t m_ac_count_hist[16] = {};	// nonzero ACs per plane, 0..15
	uint64_t m_zig_pos_hist[16] = {};	// zigzag position (1..15) of each nonzero AC
	uint64_t m_run_hist[16] = {};		// zero-run length coded before each nonzero AC
	uint64_t m_eob_markers = 0;			// trailing-zeros records

	// |v| -> bucket: 1, 2, 3, 4, 5-8, 9-16, 17-32, 33-64, 65-128, 129+
	static uint32_t mag_bucket(uint32_t m)
	{
		if (m <= 4) return m - 1;
		if (m <= 8) return 4;
		if (m <= 16) return 5;
		if (m <= 32) return 6;
		if (m <= 64) return 7;
		if (m <= 128) return 8;
		return 9;
	}

	void record_plane(const dct_syms& syms)
	{
		m_total_planes++;

		const int dc = syms.m_dc;
		if (!dc) m_dc_zero++; else if (dc > 0) m_dc_pos++; else m_dc_neg++;
		m_dc_sum_abs += (uint64_t)basisu::iabs(dc);
		m_dc_min = basisu::minimum(m_dc_min, dc);
		m_dc_max = basisu::maximum(m_dc_max, dc);
		if (dc)
			m_dc_mag_hist[mag_bucket((uint32_t)basisu::iabs(dc))]++;

		uint32_t num_acs = 0;
		uint32_t zig_idx = 1;

		for (uint32_t i = 0; i < syms.m_ac_vals.size_u32(); i++)
		{
			const uint32_t run_len = (uint32_t)syms.m_ac_vals[i].m_num_zeros;
			const int c = syms.m_ac_vals[i].m_coeff;

			if (c == INT16_MAX)
			{
				m_eob_markers++;
				break;
			}

			zig_idx += run_len;
			assert(zig_idx < 16);
			if (zig_idx >= 16)
				break;

			m_run_hist[basisu::minimum<uint32_t>(run_len, 15)]++;
			m_zig_pos_hist[zig_idx]++;

			if (c > 0) m_ac_pos++; else m_ac_neg++;
			m_ac_sum_abs += (uint64_t)basisu::iabs(c);
			m_ac_min = basisu::minimum(m_ac_min, c);
			m_ac_max = basisu::maximum(m_ac_max, c);
			m_ac_mag_hist[mag_bucket((uint32_t)basisu::iabs(c))]++;

			num_acs++;
			zig_idx++;
		}

		m_total_acs += num_acs;
		m_ac_count_hist[basisu::minimum<uint32_t>(num_acs, 15)]++;
	}

	// accumulate another collector (per-job stats -> totals); counters add,
	// extrema take min/max
	void merge(const xbc7_dct_stats& o)
	{
		m_total_planes += o.m_total_planes;

		m_dc_zero += o.m_dc_zero; m_dc_pos += o.m_dc_pos; m_dc_neg += o.m_dc_neg;
		m_dc_sum_abs += o.m_dc_sum_abs;
		m_dc_min = basisu::minimum(m_dc_min, o.m_dc_min);
		m_dc_max = basisu::maximum(m_dc_max, o.m_dc_max);
		for (uint32_t i = 0; i < 10; i++)
			m_dc_mag_hist[i] += o.m_dc_mag_hist[i];

		m_total_acs += o.m_total_acs;
		m_ac_pos += o.m_ac_pos; m_ac_neg += o.m_ac_neg;
		m_ac_sum_abs += o.m_ac_sum_abs;
		m_ac_min = basisu::minimum(m_ac_min, o.m_ac_min);
		m_ac_max = basisu::maximum(m_ac_max, o.m_ac_max);
		for (uint32_t i = 0; i < 10; i++)
			m_ac_mag_hist[i] += o.m_ac_mag_hist[i];
		for (uint32_t i = 0; i < 16; i++)
		{
			m_ac_count_hist[i] += o.m_ac_count_hist[i];
			m_zig_pos_hist[i] += o.m_zig_pos_hist[i];
			m_run_hist[i] += o.m_run_hist[i];
		}

		m_eob_markers += o.m_eob_markers;
	}

	void print() const
	{
		static const char* s_mag_labels[10] = { "1", "2", "3", "4", "5-8", "9-16", "17-32", "33-64", "65-128", "129+" };

		fmt_debug_printf("---- WEIGHTS: DCT coefficient statistics (winning candidates, per coded plane) ----\n");

		const float inv_p = m_total_planes ? (100.0f / (float)m_total_planes) : 0.0f;
		const float inv_a = m_total_acs ? (100.0f / (float)m_total_acs) : 0.0f;

		fmt_debug_printf("Coded planes: {}, total nonzero ACs: {}, avg ACs/plane: {}\n",
			m_total_planes, m_total_acs, m_total_planes ? ((float)m_total_acs / (float)m_total_planes) : 0.0f);

		fmt_debug_printf("DC: zero: {} ({}%), pos: {} ({}%), neg: {} ({}%), range [{}, {}], mean |DC| (all planes): {}\n",
			m_dc_zero, (float)m_dc_zero * inv_p, m_dc_pos, (float)m_dc_pos * inv_p, m_dc_neg, (float)m_dc_neg * inv_p,
			m_dc_min, m_dc_max, m_total_planes ? ((float)m_dc_sum_abs / (float)m_total_planes) : 0.0f);

		fmt_debug_printf("DC magnitude histogram (nonzero DCs): ");
		for (uint32_t i = 0; i < 10; i++)
			if (m_dc_mag_hist[i])
				fmt_debug_printf("{}:{} ", s_mag_labels[i], m_dc_mag_hist[i]);
		fmt_debug_printf("\n");

		fmt_debug_printf("AC: pos: {} ({}%), neg: {} ({}%), range [{}, {}], mean |AC| (per nonzero AC): {}\n",
			m_ac_pos, (float)m_ac_pos * inv_a, m_ac_neg, (float)m_ac_neg * inv_a,
			m_ac_min, m_ac_max, m_total_acs ? ((float)m_ac_sum_abs / (float)m_total_acs) : 0.0f);

		fmt_debug_printf("AC magnitude histogram: ");
		for (uint32_t i = 0; i < 10; i++)
			if (m_ac_mag_hist[i])
				fmt_debug_printf("{}:{} ({}%) ", s_mag_labels[i], m_ac_mag_hist[i], (float)m_ac_mag_hist[i] * inv_a);
		fmt_debug_printf("\n");

		fmt_debug_printf("Nonzero ACs per plane: ");
		for (uint32_t i = 0; i < 16; i++)
			if (m_ac_count_hist[i])
				fmt_debug_printf("{}:{} ({}%) ", i, m_ac_count_hist[i], (float)m_ac_count_hist[i] * inv_p);
		fmt_debug_printf("\n");

		fmt_debug_printf("Zigzag position of nonzero ACs (1..15): ");
		for (uint32_t i = 1; i < 16; i++)
			fmt_debug_printf("{}:{} ", i, m_zig_pos_hist[i]);
		fmt_debug_printf("\n");

		fmt_debug_printf("Zero-run length before each AC: ");
		for (uint32_t i = 0; i < 16; i++)
			if (m_run_hist[i])
				fmt_debug_printf("{}:{} ({}%) ", i, m_run_hist[i], (float)m_run_hist[i] * inv_a);
		fmt_debug_printf("\n");

		fmt_debug_printf("Trailing-zeros (EOB) markers: {} ({}% of planes)\n", m_eob_markers, (float)m_eob_markers * inv_p);
	}
};


	// ---- debug-image color palettes (encode-time visualization only) ----
	// The command byte is split into three field-specific color images rather
	// than one hard-to-read grayscale; each image carries a drawn legend.

	// CORE command id (xbc7_command_id, command bits 0-2).
	static const color_rgba g_xbc7_command_vis_colors[8] =
	{
		color_rgba(  0,   0, 255, 255), // 0 RepeatLast               - blue
		color_rgba(  0, 255, 255, 255), // 1 RepeatUpper              - cyan
		color_rgba(128, 128, 128, 255), // 2 SolidDPCM                - gray
		color_rgba(255,   0,   0, 255), // 3 NewConfig                - red
		color_rgba(  0, 255,   0, 255), // 4 ReuseConfigLeft          - green
		color_rgba(255, 255,   0, 255), // 5 ReuseConfigUpper         - yellow
		color_rgba(255,   0, 255, 255), // 6 ReuseConfigLeftDiagonal  - magenta
		color_rgba(255, 128,   0, 255), // 7 ReuseConfigRightDiagonal - orange
	};

	// ENDPOINT prediction mode (xbc7_command_endpoint_mode, command bits 3-5).
	static const color_rgba g_xbc7_endpoint_mode_vis_colors[8] =
	{
		color_rgba(255,   0,   0, 255), // 0 Raw            - red
		color_rgba(  0, 200,   0, 255), // 1 DPCM-Left      - green
		color_rgba(  0, 128, 255, 255), // 2 DPCM-Up        - blue
		color_rgba(255, 255,   0, 255), // 3 DPCM-LeftDiag  - yellow
		color_rgba(255,   0, 255, 255), // 4 DPCM-RightDiag - magenta
		color_rgba(  0, 255, 255, 255), // 5 DPCM-Index     - cyan
		color_rgba(255, 128,   0, 255), // 6 DPCM-Left-Sub1 - orange
		color_rgba(160,   0, 255, 255), // 7 DPCM-Up-Sub1   - purple
	};

	// WEIGHT mode (xbc7_command_weight_mode, command bit 6).
	static const color_rgba g_xbc7_weight_mode_vis_colors[2] =
	{
		color_rgba(  0, 200,   0, 255), // 0 Raw/DPCM (lossless) - green
		color_rgba(255,   0,   0, 255), // 1 DCT     (lossy)     - red
	};

	// Blocks with no endpoint/weight fields (fully predicted: RepeatLast/
	// RepeatUpper/SolidDPCM) in the endpoint/weight-mode images.
	static const color_rgba g_xbc7_vis_na_color(40, 40, 40, 255);

	// AC-COUNT heatmap: number of nonzero DCT weight ACs in a block (DCT-coded
	// blocks only). Bucketed via xbc7_ac_count_bucket().
	static const color_rgba g_xbc7_ac_count_vis_colors[6] =
	{
		color_rgba(  0,   0, 128, 255), // 0     ACs - dark blue (flat / DC only)
		color_rgba(  0, 128, 255, 255), // 1-2   ACs - blue
		color_rgba(  0, 200,   0, 255), // 3-5   ACs - green
		color_rgba(255, 255,   0, 255), // 6-10  ACs - yellow
		color_rgba(255, 128,   0, 255), // 11-20 ACs - orange
		color_rgba(255,   0,   0, 255), // 21+   ACs - red
	};

	static inline uint32_t xbc7_ac_count_bucket(uint32_t num_acs)
	{
		if (num_acs == 0)  return 0;
		if (num_acs <= 2)  return 1;
		if (num_acs <= 5)  return 2;
		if (num_acs <= 10) return 3;
		if (num_acs <= 20) return 4;
		return 5;
	}

	// PREDICTOR categories: the ~50 weight predictors (absolute, 17 synthetic,
	// 32 block references) collapsed into a handful of buckets; the 32 block
	// refs are split by their causal direction.
	enum xbc7_pred_category
	{
		cPredCatAbsolute = 0, // no prediction
		cPredCatSynthetic,    // edge/gradient synthetic predictors
		cPredCatRefLeft,      // block ref, same row (dy==0)
		cPredCatRefUp,        // block ref, straight up (dx==0)
		cPredCatRefUpLeft,    // block ref, up-and-left
		cPredCatRefUpRight,   // block ref, up-and-right
		cNumPredCategories
	};

	static const color_rgba g_xbc7_predictor_vis_colors[cNumPredCategories] =
	{
		color_rgba(255,   0,   0, 255), // Absolute         - red
		color_rgba(255, 128,   0, 255), // Synthetic        - orange
		color_rgba(  0, 200,   0, 255), // BlockRef Left     - green
		color_rgba(  0, 128, 255, 255), // BlockRef Up       - blue
		color_rgba(  0, 255, 255, 255), // BlockRef Up-Left  - cyan
		color_rgba(255,   0, 255, 255), // BlockRef Up-Right - magenta
	};

	// Map a (cand + amp*cTotalCandidates) predictor index to its category.
	static inline uint32_t xbc7_predictor_category(uint32_t pred_index)
	{
		const uint32_t cand = pred_index % cTotalCandidates;
		if (cand == cCandAbsolute)
			return cPredCatAbsolute;
		if (cand < cCandFirstXYDelta)
			return cPredCatSynthetic;
		const xbc7_xy_delta d = g_xbc7_xy_deltas[cand - cCandFirstXYDelta];
		if (d.m_dy == 0) return cPredCatRefLeft; // dx<0, same row
		if (d.m_dx == 0) return cPredCatRefUp;   // straight up
		return (d.m_dx < 0) ? cPredCatRefUpLeft : cPredCatRefUpRight;
	}

	// Weighted RGBA PSNR of a 16-pixel (4x4) block vs the source, using the
	// caller's per-channel weights (pack_options::m_weights), exactly mirroring
	// basisu_astc_ldr_encode.cpp's WSSE->PSNR: per-pixel WSSE via
	// color_rgba::get_weighted_dist2(), wmse = wsse / (sum_weights * num_pixels),
	// then 20*log10(255/sqrt(wmse)), capped for ~zero error. Used by the BC7
	// alt-pack and endpoint-DPCM RDO passes.
	static inline float xbc7_block_wsse_psnr(const color_rgba* pOrig, const color_rgba* pDec, const uint32_t comp_weights[4])
	{
		uint64_t wsse = 0;
		for (uint32_t i = 0; i < 16; i++)
			wsse += pDec[i].get_weighted_dist2(pOrig[i], comp_weights);

		const float total_comp_weights = (float)(comp_weights[0] + comp_weights[1] + comp_weights[2] + comp_weights[3]);
		const float wmse = (float)wsse / (total_comp_weights * 16.0f);
		return (wmse > 1e-5f) ? (20.0f * log10f(255.0f / sqrtf(wmse))) : 10000.0f;
	}

	// Standalone: recompute the optimal per-texel weights of a BC7 logical block
	// for its FIXED config + endpoints, ignoring its existing (possibly stale)
	// weights. out_blk receives in_blk's config + endpoints with freshly optimized
	// weights. comp_weights[4] are the per-channel RGBA error weights. No
	// dependency on pack_options, so this can be lifted into shared code later.
	//
	// Per plane: each texel's weight independently selects its interpolation
	// between the fixed endpoints, so setting all texels to the same value w and
	// decoding reveals every texel's value at w in one decode; sweeping w over all
	// 1<<m_weight_bits[plane] values and keeping each texel's lowest WSSE yields
	// the per-texel optimum. Error is summed ONLY over the channels this plane
	// drives (get_decoded_channel_weight_plane) -- critical for dual-plane modes,
	// where planes 0/1 control disjoint channels (with mode 4/5 rotation/CCS).
	[[maybe_unused]] static bool optimize_block_weights(const basist::bc7u::log_bc7_block& in_blk, const color_rgba orig_pixels[16], const uint32_t comp_weights[4], basist::bc7u::log_bc7_block& out_blk)
	{
		out_blk = in_blk; // same config + endpoints; weights overwritten below

		for (uint32_t p = 0; p < in_blk.m_num_planes; p++)
		{
			const uint32_t num_weight_vals = in_blk.get_num_weight_vals(p); // 4, 8, or 16

			basist::bc7u::log_bc7_block scratch(in_blk);

			// neutralize the OTHER plane so the decode is well-defined; the
			// channel masking below means it can't affect this plane's WSSE anyway
			if (in_blk.is_dual_plane())
				memset(scratch.m_weights[1u - p], 0, sizeof(scratch.m_weights[0]));

			uint64_t best_err[16];
			uint8_t best_weight[16];
			for (uint32_t i = 0; i < 16; i++)
			{
				best_err[i] = UINT64_MAX;
				best_weight[i] = 0;
			}

			for (uint32_t w = 0; w < num_weight_vals; w++)
			{
				for (uint32_t i = 0; i < 16; i++)
					scratch.m_weights[p][i] = (uint8_t)w;

				basist::color_rgba dec[16];
				if (!basist::bc7u::unpack_bc7(scratch, dec))
				{
					assert(0); // a valid config+endpoints block must always decode
					return false;
				}

				for (uint32_t i = 0; i < 16; i++)
				{
					uint64_t err = 0;
					for (uint32_t c = 0; c < 4; c++)
					{
						if (in_blk.get_decoded_channel_weight_plane(c) != p)
							continue; // only the channels this plane drives

						const int d = (int)dec[i][c] - (int)orig_pixels[i][c];
						err += (uint64_t)(d * d) * comp_weights[c];
					}

					if (err < best_err[i])
					{
						best_err[i] = err;
						best_weight[i] = (uint8_t)w;
					}
				}
			}

			for (uint32_t i = 0; i < 16; i++)
				out_blk.m_weights[p][i] = best_weight[i];
		}

		return true;
	}

	// Encode-time visualization targets (debug only). Each member is null unless
	// m_debug_images is set. One 4x4 pixel block per BC7 block; stripes fill
	// disjoint block rows, so writing these from stripe jobs is thread-safe.
	struct xbc7_debug_image_set
	{
		image* m_pCommand = nullptr;       // core command id (bits 0-2)
		image* m_pEndpoint_mode = nullptr; // endpoint pred mode (bits 3-5); n/a for predicted blocks
		image* m_pWeight_mode = nullptr;   // weight mode (bit 6): DCT vs lossless; n/a for predicted blocks
		image* m_pAC_count = nullptr;      // # nonzero DCT weight ACs; n/a unless DCT-coded
		image* m_pPredictor = nullptr;     // weight predictor category; n/a for predicted blocks
	};

	// Fill one BC7 block's 4x4 region in a (possibly null) debug image.
	static inline void xbc7_vis_fill_block(image* pImg, uint32_t bx, uint32_t by, const color_rgba& c)
	{
		if (pImg)
			pImg->fill_box(bx * 4, by * 4, 4, 4, c);
	}

	// Color-fill a block across all debug images from its final command byte.
	// has_ep_wt is false for fully-predicted blocks (RepeatLast/RepeatUpper/
	// SolidDPCM), which carry no endpoint/weight fields. pred_index is the weight
	// predictor (UINT32_MAX == none); ac_count is the nonzero DCT-weight AC count
	// (UINT32_MAX == not DCT-coded). UINT32_MAX maps to the n/a color.
	static inline void xbc7_vis_fill_command(const xbc7_debug_image_set& dbg, uint32_t bx, uint32_t by,
		uint8_t command_byte, bool has_ep_wt, uint32_t pred_index, uint32_t ac_count)
	{
		xbc7_vis_fill_block(dbg.m_pCommand, bx, by, g_xbc7_command_vis_colors[command_byte & 7]);

		if (has_ep_wt)
		{
			xbc7_vis_fill_block(dbg.m_pEndpoint_mode, bx, by, g_xbc7_endpoint_mode_vis_colors[(command_byte >> XBC7_COMMAND_ENDPOINT_MODE_SHIFT) & 7]);
			xbc7_vis_fill_block(dbg.m_pWeight_mode, bx, by, g_xbc7_weight_mode_vis_colors[(command_byte >> XBC7_COMMAND_WEIGHT_MODE_SHIFT) & 1]);
		}
		else
		{
			xbc7_vis_fill_block(dbg.m_pEndpoint_mode, bx, by, g_xbc7_vis_na_color);
			xbc7_vis_fill_block(dbg.m_pWeight_mode, bx, by, g_xbc7_vis_na_color);
		}

		xbc7_vis_fill_block(dbg.m_pPredictor, bx, by,
			(pred_index == UINT32_MAX) ? g_xbc7_vis_na_color : g_xbc7_predictor_vis_colors[xbc7_predictor_category(pred_index)]);

		xbc7_vis_fill_block(dbg.m_pAC_count, bx, by,
			(ac_count == UINT32_MAX) ? g_xbc7_vis_na_color : g_xbc7_ac_count_vis_colors[xbc7_ac_count_bucket(ac_count)]);
	}

	// ---- legend drawn into a strip at the bottom of each debug image ----
	const uint32_t XBC7_VIS_LEGEND_ROW_H = 10; // px per entry (8px glyph + 2px gap)

	static inline uint32_t xbc7_vis_legend_height(uint32_t num_entries) { return num_entries * XBC7_VIS_LEGEND_ROW_H + 4; }

	struct xbc7_vis_legend_entry { color_rgba m_color; const char* m_pLabel; };

	// Draws a swatch+label list into the BOTTOM strip of img, overwriting the
	// block pixels there (the image stays the source resolution). Everything
	// clips, so it's safe even if the strip is taller than the whole image.
	static void xbc7_vis_draw_legend(image& img, const xbc7_vis_legend_entry* pEntries, uint32_t num_entries)
	{
		const uint32_t legend_h = xbc7_vis_legend_height(num_entries);
		const uint32_t top_y = (img.get_height() >= legend_h) ? (img.get_height() - legend_h) : 0;

		img.fill_box(0, top_y, img.get_width(), img.get_height() - top_y, color_rgba(16, 16, 16, 255));

		for (uint32_t i = 0; i < num_entries; i++)
		{
			const uint32_t y = top_y + 2 + i * XBC7_VIS_LEGEND_ROW_H;
			img.fill_box(2, y, 8, 8, pEntries[i].m_color);
			img.debug_text(14, y, 1, 1, color_rgba(255, 255, 255, 255), nullptr, false, "%s", pEntries[i].m_pLabel);
		}
	}

	// ---------------- encoder striping ----------------
	// The main coding pass splits into 1..XBC7_MAX_ENCODER_STRIPES horizontal
	// stripes of whole block rows, one job per stripe. Each stripe is coded
	// causally from scratch (the encoder never emits references above a
	// stripe's first row), so thin stripes cost compression: small images stay
	// serial, and once striping kicks in every stripe gets at least
	// XBC7_MIN_STRIPE_BLOCK_ROWS rows.

	// (XBC7_MAX_ENCODER_STRIPES is shared with the decoder -- defined in
	// basisu_xbc7_decode.h.)

	// images shorter than this many TEXEL rows always encode as one stripe
	const uint32_t XBC7_MIN_IMAGE_TEXEL_ROWS_TO_STRIPE = 128;

	// minimum BLOCK rows per stripe once striping kicks in (bounds the
	// per-seam compression loss); 16 block rows == 64 texel rows
	const uint32_t XBC7_MIN_STRIPE_BLOCK_ROWS = 16;


	// Computes the encoder's stripe count (1..XBC7_MAX_ENCODER_STRIPES) and
	// each stripe's contiguous block-row range. Deliberately a function of
	// the image dimensions ONLY -- not of thread availability -- so the
	// encoded output is reproducible across machines; with fewer pool threads
	// than stripes the extra jobs simply queue.
	static uint32_t compute_encoder_stripes(
		uint32_t height_in_texels, uint32_t num_blocks_y,
		basisu::vector<stripe_range>& stripes)
	{
		uint32_t num_stripes = 1;

		if (height_in_texels >= XBC7_MIN_IMAGE_TEXEL_ROWS_TO_STRIPE)
		{
			num_stripes = num_blocks_y / XBC7_MIN_STRIPE_BLOCK_ROWS;
			num_stripes = basisu::clamp<uint32_t>(num_stripes, 1, XBC7_MAX_ENCODER_STRIPES);
		}

		num_stripes = basisu::minimum(num_stripes, basisu::maximum(1u, num_blocks_y));

		compute_stripe_ranges(num_blocks_y, num_stripes, stripes);

		return num_stripes;
	}

	// Highest stripe count this image can carry without violating the striping
	// rules: at least XBC7_MIN_STRIPE_BLOCK_ROWS per stripe (so none is tiny or
	// empty), no more than XBC7_MAX_ENCODER_STRIPES, and 1 for images too short
	// to stripe usefully. Mirrors the bound compute_encoder_stripes uses.
	static uint32_t max_stripes_for_image(uint32_t height_in_texels, uint32_t num_blocks_y)
	{
		if (height_in_texels < XBC7_MIN_IMAGE_TEXEL_ROWS_TO_STRIPE)
			return 1;

		const uint32_t by_min_size = num_blocks_y / XBC7_MIN_STRIPE_BLOCK_ROWS; // each stripe >= min block rows
		return basisu::clamp<uint32_t>(by_min_size, 1, XBC7_MAX_ENCODER_STRIPES);
	}

	uint32_t pack_options::set_num_stripes_for_image(const image& img, uint32_t desired_num_stripes)
	{
		const uint32_t height = img.get_height();
		const uint32_t num_blocks_y = (height + 3) / 4;

		const uint32_t max_stripes = max_stripes_for_image(height, num_blocks_y);

		m_num_stripes = basisu::clamp<uint32_t>(desired_num_stripes, 1, max_stripes);
		return m_num_stripes;
	}

	void pack_options::set_rdo_level(uint32_t rdo_level)
	{
		rdo_level = basisu::minimum<uint32_t>(rdo_level, 100);

		if (!rdo_level)
		{
			// RDO fully disabled: flags off, all drops zero.
			m_bc7_alt_pack_enabled = false;
			m_endpoint_rdo_enabled = false;
			m_repeat_rdo_enabled = false;
			m_solid_rdo_enabled = false;

			m_bc7_alt_max_psnr_drop = 0.0f;
			m_endpoint_rdo_max_psnr_drop = 0.0f;
			m_repeat_rdo_max_psnr_drop = 0.0f;
			m_solid_rdo_max_psnr_drop = 0.0f;
			m_ac_trunc_rdo_max_psnr_drop = 0.0f;
			return;
		}

		const float frac = (float)rdo_level / 100.0f; // (0, 1]
		const float general_drop = 10.0f * frac;  // BC7 alt-pack / endpoint-DPCM / AC-trunc -> up to 10 dB
		const float reuse_drop = 4.0f * frac;     // repeat / solid block reuse        -> up to 4 dB

		m_bc7_alt_pack_enabled = true;
		m_bc7_alt_max_psnr_drop = general_drop;

		m_endpoint_rdo_enabled = true;
		m_endpoint_rdo_max_psnr_drop = general_drop;

		m_ac_trunc_rdo_max_psnr_drop = general_drop; // no enable flag; >0 enables

		m_repeat_rdo_enabled = true;
		m_repeat_rdo_max_psnr_drop = reuse_drop;

		m_solid_rdo_enabled = true;
		m_solid_rdo_max_psnr_drop = reuse_drop;
	}

	// ------------------------- XBC7 encoding statistics -------------------------
	// Populated inline during pack_image (negligible cost), printed when
	// opts.m_print_stats is set. Three layers: where the bytes go (per-blob
	// accounting), what the encoder chose (command/mode/predictor histograms),
	// and why (residual and coefficient distributions).
	struct xbc7_pack_stats
	{
		// context
		uint32_t m_width = 0, m_height = 0, m_total_blocks = 0, m_dct_q = 0;
		bool m_has_alpha = false;
		basisu::vector<stripe_range> m_stripes;

		// choices
		uint64_t m_cmd_hist[8] = {};
		uint64_t m_mode_hist[8] = {};					// coded (non-repeat, non-solid) blocks
		uint64_t m_pred_hist[cTotalCandidates * 4] = {};
		uint64_t m_ep_raw_blocks = 0, m_ep_dpcm_blocks = 0, m_ep_dpcm_subsets = 0;

		// AC-truncation RDO
		uint64_t m_ac_trunc_pruned = 0;	// weight-DCT AC coeffs zeroed
		uint64_t m_ac_trunc_blocks = 0;	// blocks where >=1 coeff was pruned
		uint64_t m_ep_mode_hist[8] = {};				// xbc7_command_endpoint_mode of every coded block
		uint64_t m_ep_index_hist[NUM_XY_DELTAS] = {};	// XY-delta table index of every EP block reference
		uint64_t m_pbit_delta_bits = 0, m_pbit_delta_nonzero = 0;

		// distributions: EP residuals by [width class][stream channel slot],
		// wrapped bytes interpreted as int8 (the zero-peaked reading)
		uint64_t m_ep_resid_count[2][4] = {};			// [0]=fine, [1]=coarse
		uint64_t m_ep_resid_sum_abs[2][4] = {};
		uint64_t m_ep_resid_zero[2][4] = {};

		uint64_t m_solid_delta_count[4] = {};
		uint64_t m_solid_delta_sum_abs[4] = {};
		uint64_t m_solid_delta_zero[4] = {};

		// weights: residual DCT vs lossless DPCM decision + tuning data
		uint32_t m_wt_alpha_pct = 0;
		uint64_t m_wt_dct_blocks = 0, m_wt_dpcm_blocks = 0;
		uint64_t m_wt_dct_est_bits_total = 0, m_wt_dpcm_est_bits_total = 0, m_wt_chosen_est_bits = 0;

		uint64_t m_wt_block_acs_hist_dct[33] = {};		// total ACs per block (clamped), split by which mode won
		uint64_t m_wt_block_acs_hist_dpcm[33] = {};
		uint64_t m_wt_dpcm_pred_hist[cTotalCandidates * 4] = {};
		uint64_t m_wt_resid_count[3] = {}, m_wt_resid_zero[3] = {}, m_wt_resid_sum_abs[3] = {};	// [weight_bits - 2]
		uint64_t m_wt_resid_mag_hist[3][9] = {};	// [weight_bits - 2][min(|centered resid|, 8)]

		xbc7_dct_stats m_dct_stats;						// winning-candidate coefficient stats

		// where the bytes go (filled at serialize time)
		uint32_t m_blob_raw_size[BLOB_STREAM_MAX_IDS] = {};
		uint32_t m_blob_stored_size[BLOB_STREAM_MAX_IDS] = {};
		uint32_t m_total_file_size = 0;

		// order-0 byte histograms of the pre-Zstd blob contents
		uint64_t m_blob_hist[cBlobFirstUnused][256] = {};

		void record_blob_bytes(uint32_t id, const uint8_vec* pBlob)
		{
			if ((id >= (uint32_t)cBlobFirstUnused) || (!pBlob))
				return;
			for (size_t i = 0; i < pBlob->size(); i++)
				m_blob_hist[id][(*pBlob)[i]]++;
		}

		void record_ep_residuals(bool fine_class, const uint8_t* pResiduals, uint32_t num_residuals)
		{
			const uint32_t cls = fine_class ? 0 : 1;
			for (uint32_t i = 0; i < num_residuals; i++)
			{
				const uint32_t chan = i >> 1;
				const int v = (int8_t)pResiduals[i];
				m_ep_resid_count[cls][chan]++;
				m_ep_resid_sum_abs[cls][chan] += (uint64_t)basisu::iabs(v);
				if (!v)
					m_ep_resid_zero[cls][chan]++;
			}
		}

		// Accumulate a per-stripe (per-job) collector into this one. Pure
		// counters only: the context fields (dims/Q/stripes/alpha) and the
		// blob size/histogram arrays are owned by the final stats object and
		// filled outside the jobs.
		void merge(const xbc7_pack_stats& o)
		{
			for (uint32_t i = 0; i < 8; i++)
			{
				m_cmd_hist[i] += o.m_cmd_hist[i];
				m_mode_hist[i] += o.m_mode_hist[i];
				m_ep_mode_hist[i] += o.m_ep_mode_hist[i];
			}

			for (uint32_t i = 0; i < cTotalCandidates * 4; i++)
			{
				m_pred_hist[i] += o.m_pred_hist[i];
				m_wt_dpcm_pred_hist[i] += o.m_wt_dpcm_pred_hist[i];
			}

			m_ep_raw_blocks += o.m_ep_raw_blocks;
			m_ep_dpcm_blocks += o.m_ep_dpcm_blocks;
			m_ep_dpcm_subsets += o.m_ep_dpcm_subsets;

			m_ac_trunc_pruned += o.m_ac_trunc_pruned;
			m_ac_trunc_blocks += o.m_ac_trunc_blocks;

			for (uint32_t i = 0; i < NUM_XY_DELTAS; i++)
				m_ep_index_hist[i] += o.m_ep_index_hist[i];

			m_pbit_delta_bits += o.m_pbit_delta_bits;
			m_pbit_delta_nonzero += o.m_pbit_delta_nonzero;

			for (uint32_t cls = 0; cls < 2; cls++)
			{
				for (uint32_t c = 0; c < 4; c++)
				{
					m_ep_resid_count[cls][c] += o.m_ep_resid_count[cls][c];
					m_ep_resid_sum_abs[cls][c] += o.m_ep_resid_sum_abs[cls][c];
					m_ep_resid_zero[cls][c] += o.m_ep_resid_zero[cls][c];
				}
			}

			for (uint32_t c = 0; c < 4; c++)
			{
				m_solid_delta_count[c] += o.m_solid_delta_count[c];
				m_solid_delta_sum_abs[c] += o.m_solid_delta_sum_abs[c];
				m_solid_delta_zero[c] += o.m_solid_delta_zero[c];
			}

			m_wt_dct_blocks += o.m_wt_dct_blocks;
			m_wt_dpcm_blocks += o.m_wt_dpcm_blocks;
			m_wt_dct_est_bits_total += o.m_wt_dct_est_bits_total;
			m_wt_dpcm_est_bits_total += o.m_wt_dpcm_est_bits_total;
			m_wt_chosen_est_bits += o.m_wt_chosen_est_bits;

			for (uint32_t i = 0; i <= 32; i++)
			{
				m_wt_block_acs_hist_dct[i] += o.m_wt_block_acs_hist_dct[i];
				m_wt_block_acs_hist_dpcm[i] += o.m_wt_block_acs_hist_dpcm[i];
			}

			for (uint32_t cls = 0; cls < 3; cls++)
			{
				m_wt_resid_count[cls] += o.m_wt_resid_count[cls];
				m_wt_resid_zero[cls] += o.m_wt_resid_zero[cls];
				m_wt_resid_sum_abs[cls] += o.m_wt_resid_sum_abs[cls];

				for (uint32_t m = 0; m < 9; m++)
					m_wt_resid_mag_hist[cls][m] += o.m_wt_resid_mag_hist[cls][m];
			}

			m_dct_stats.merge(o.m_dct_stats);
		}

		// fmt_variants rejects format specs on strings, so pad manually
		static std::string pad(const char* pStr, size_t n)
		{
			std::string s(pStr);
			if (s.size() < n)
				s.resize(n, ' ');
			return s;
		}

		void print() const
		{
			static const char* s_blob_names[cBlobFirstUnused] =
			{
				"header", "commands", "cfg: block_config", "cfg: partition2", "cfg: partition3",
				"wt: predictors", "wt-dct: dc_small", "wt-dct: dc_large", "wt-dct: ac_coeffs",
				"wt-dct: signs", "ep: pbits",
				"ep: fine_r", "ep: fine_g", "ep: fine_b", "ep: fine_a",
				"ep: coarse_r", "ep: coarse_g", "ep: coarse_b", "ep: coarse_a",
				"ep: raw", "ep: blk_index", "wt-dpcm: absolute", "solid: rgba_deltas",
				"wt-dpcm: resid_2", "wt-dpcm: resid_3", "wt-dpcm: resid_4", "seek_table"
			};
			static const char* s_cmd_names[8] =
			{
				"repeat_last", "repeat_upper", "solid_dpcm", "new_config",
				"reuse_left", "reuse_upper", "reuse_ldiag", "reuse_rdiag"
			};

			const float inv_blocks = m_total_blocks ? (100.0f / (float)m_total_blocks) : 0.0f;

			fmt_debug_printf("\n========== XBC7 pack stats ==========\n");
			fmt_debug_printf("{}x{}, {} blocks, Q={}, has_alpha={}\n",
				m_width, m_height, m_total_blocks, m_dct_q, m_has_alpha);

			{
				std::string s(string_format("encoder stripes: %u", m_stripes.size_u32()));
				for (uint32_t i = 0; i < m_stripes.size_u32(); i++)
					s += string_format("%s block rows %u-%u", i ? "," : " --", m_stripes[i].m_first_block_row, m_stripes[i].m_first_block_row + m_stripes[i].m_num_block_rows - 1);
				fmt_debug_printf("{}\n", s);
			}

			// ---- per-blob byte accounting ----
			fmt_debug_printf("\n---- blob accounting (raw -> stored after Zstd) ----\n");
			fmt_debug_printf("{}{}{}{}{}{}\n",
				pad("blob", 20), pad("raw", 10), pad("stored", 10), pad("ratio", 8), pad("%file", 8), "bits/blk");

			uint64_t total_raw = 0, total_stored = 0;
			for (uint32_t id = 0; id < (uint32_t)cBlobFirstUnused; id++)
			{
				if (!m_blob_raw_size[id])
					continue;

				total_raw += m_blob_raw_size[id];
				total_stored += m_blob_stored_size[id];

				fmt_debug_printf("{}{}{}{}{}{.3}\n",
					pad(s_blob_names[id], 20),
					pad(string_format("%u", m_blob_raw_size[id]).c_str(), 10),
					pad(string_format("%u", m_blob_stored_size[id]).c_str(), 10),
					pad(string_format("%.3f", m_blob_stored_size[id] ? ((float)m_blob_raw_size[id] / (float)m_blob_stored_size[id]) : 0.0f).c_str(), 8),
					pad(string_format("%.2f", m_total_file_size ? ((float)m_blob_stored_size[id] * 100.0f / (float)m_total_file_size) : 0.0f).c_str(), 8),
					m_total_blocks ? ((float)m_blob_stored_size[id] * 8.0f / (float)m_total_blocks) : 0.0f);
			}

			const uint64_t overhead = (uint64_t)m_total_file_size - total_stored;
			fmt_debug_printf("{}{}{}\n", pad("TOTAL", 20),
				pad(string_format("%llu", (unsigned long long)total_raw).c_str(), 10),
				pad(string_format("%llu", (unsigned long long)total_stored).c_str(), 10));
			fmt_debug_printf("container+dir overhead: {} bytes\n", overhead);
			fmt_debug_printf("file: {} bytes, {.3} bits/block, {.4} bpp\n",
				m_total_file_size,
				m_total_blocks ? ((float)m_total_file_size * 8.0f / (float)m_total_blocks) : 0.0f,
				(m_width && m_height) ? ((float)m_total_file_size * 8.0f / (float)(m_width * m_height)) : 0.0f);

			// ---- group rollup: where the bytes go, raw vs stored ----
			{
				// m_super buckets the fine groups into the coarse summary:
				// 0 = commands+config, 1 = endpoints/pbits, 2 = weights, 3 = other
				struct group_def { const char* m_pName; uint8_t m_super; uint8_t m_ids[12]; uint32_t m_num_ids; };
				static const group_def s_groups[] =
				{
					{ "commands",         0, { cBlobCommands }, 1 },
					{ "config/partition", 0, { cBlobBC7BlockConfig, cBlobPartition2, cBlobPartition3 }, 3 },
					{ "endpoints/pbits",  1, { cBlobEPDeltaFineR, cBlobEPDeltaFineG, cBlobEPDeltaFineB, cBlobEPDeltaFineA,
											   cBlobEPDeltaCoarseR, cBlobEPDeltaCoarseG, cBlobEPDeltaCoarseB, cBlobEPDeltaCoarseA,
											   cBlobEPRaw, cBlobEPBlockIndex, cBlobPBits }, 11 },
					{ "wt predictors",    2, { cBlobWeightPredictors }, 1 },
					{ "wt dct",           2, { cBlobDCCoeffsSmall, cBlobDCCoeffsLarge, cBlobACCoeffs, cBlobCoeffSigns }, 4 },
					{ "wt dpcm",          2, { cBlobRawWeightBits, cBlobDPCMWeightResid2, cBlobDPCMWeightResid3, cBlobDPCMWeightResid4 }, 4 },
					{ "solid",            3, { cBlobSolidRGBADeltas }, 1 },
					{ "header",           3, { cBlobHeader }, 1 },
					{ "seek_table",       3, { cBlobStripeSeekTable }, 1 },
				};
				const uint32_t num_groups = (uint32_t)(sizeof(s_groups) / sizeof(s_groups[0]));

				const float inv_file = m_total_file_size ? (100.0f / (float)m_total_file_size) : 0.0f;

				fmt_debug_printf("\n---- group rollup ----\n");
				fmt_debug_printf("{}{}{}{}\n", pad("group", 19), pad("raw", 10), pad("stored", 10), "%file");

				uint64_t group_stored_total = 0;
				uint64_t super_raw[4] = {}, super_stored[4] = {};

				for (uint32_t g = 0; g < num_groups; g++)
				{
					uint64_t raw = 0, stored = 0;
					for (uint32_t i = 0; i < s_groups[g].m_num_ids; i++)
					{
						raw += m_blob_raw_size[s_groups[g].m_ids[i]];
						stored += m_blob_stored_size[s_groups[g].m_ids[i]];
					}
					group_stored_total += stored;

					super_raw[s_groups[g].m_super] += raw;
					super_stored[s_groups[g].m_super] += stored;

					if (!raw)
						continue;

					fmt_debug_printf("{}{}{}{.2}\n",
						pad(s_groups[g].m_pName, 19),
						pad(string_format("%llu", (unsigned long long)raw).c_str(), 10),
						pad(string_format("%llu", (unsigned long long)stored).c_str(), 10),
						(float)stored * inv_file);
				}

				const uint64_t container_ovh = (uint64_t)m_total_file_size - group_stored_total;
				fmt_debug_printf("{}{}{}{.2}\n", pad("container", 19), pad("", 10),
					pad(string_format("%llu", (unsigned long long)container_ovh).c_str(), 10), (float)container_ovh * inv_file);
				fmt_debug_printf("{}{}{}{}\n", pad("TOTAL", 19), pad("", 10),
					pad(string_format("%u", m_total_file_size).c_str(), 10), "100.00");

				// coarse three-bucket summary
				static const char* s_super_names[4] = { "= commands+config", "= endpoints/pbits", "= weights (all)", "= other" };

				fmt_debug_printf("\n");
				for (uint32_t s = 0; s < 4; s++)
				{
					if (!super_raw[s])
						continue;

					fmt_debug_printf("{}{}{}{.2}\n",
						pad(s_super_names[s], 19),
						pad(string_format("%llu", (unsigned long long)super_raw[s]).c_str(), 10),
						pad(string_format("%llu", (unsigned long long)super_stored[s]).c_str(), 10),
						(float)super_stored[s] * inv_file);
				}
			}

			// ---- per-blob order-0 symbol stats ----
			// H = Shannon entropy of the raw byte stream (bits/byte); ideal = the
			// order-0 entropy-coded size. stored/ideal < 1 means Zstd's matches +
			// higher-order context beat a memoryless coder; > 1 means an entropy
			// coder (FSE/rANS over this alphabet) would beat what Zstd achieves.
			fmt_debug_printf("\n---- blob symbol stats (order-0, pre-Zstd) ----\n");
			fmt_debug_printf("{}{}{}{}{}{}{}\n",
				pad("blob", 20), pad("bytes", 10), pad("syms", 6), pad("H b/byte", 10), pad("ideal", 10), pad("stored", 10), "st/id");

			for (uint32_t id = 0; id < (uint32_t)cBlobFirstUnused; id++)
			{
				uint64_t n = 0;
				uint32_t distinct = 0;
				for (uint32_t s = 0; s < 256; s++)
				{
					n += m_blob_hist[id][s];
					if (m_blob_hist[id][s])
						distinct++;
				}
				if (!n)
					continue;

				double entropy = 0.0;
				for (uint32_t s = 0; s < 256; s++)
				{
					if (!m_blob_hist[id][s])
						continue;
					const double p = (double)m_blob_hist[id][s] / (double)n;
					entropy -= p * std::log2(p);
				}

				const uint64_t ideal_bytes = (uint64_t)std::ceil(entropy * (double)n / 8.0);

				fmt_debug_printf("{}{}{}{}{}{}{.3}\n",
					pad(s_blob_names[id], 20),
					pad(string_format("%llu", (unsigned long long)n).c_str(), 10),
					pad(string_format("%u", distinct).c_str(), 6),
					pad(string_format("%.3f", entropy).c_str(), 10),
					pad(string_format("%llu", (unsigned long long)ideal_bytes).c_str(), 10),
					pad(string_format("%u", m_blob_stored_size[id]).c_str(), 10),
					ideal_bytes ? ((float)m_blob_stored_size[id] / (float)ideal_bytes) : 0.0f);

				// condensed top symbols: up to 6, plus a count of the rest
				uint64_t hist[256];
				memcpy(hist, m_blob_hist[id], sizeof(hist));

				std::string tops("  top:");
				uint32_t shown = 0;
				uint64_t shown_count = 0;
				for (; shown < 6; shown++)
				{
					uint32_t best_s = 0;
					uint64_t best_c = 0;
					for (uint32_t s = 0; s < 256; s++)
					{
						if (hist[s] > best_c)
						{
							best_c = hist[s];
							best_s = s;
						}
					}
					if (!best_c)
						break;

					hist[best_s] = 0;
					shown_count += best_c;
					tops += string_format(" %u:%llu(%.1f%%)", best_s, (unsigned long long)best_c, (double)best_c * 100.0 / (double)n);
				}
				if (distinct > shown)
					tops += string_format("  +%u more (%.1f%%)", distinct - shown, (double)(n - shown_count) * 100.0 / (double)n);

				fmt_debug_printf("{}\n", tops);
			}

			// ---- commands / modes ----
			fmt_debug_printf("\n---- COMMANDS: per-block command histogram ----\n");
			for (uint32_t i = 0; i < 8; i++)
				if (m_cmd_hist[i])
					fmt_debug_printf("{}: {} ({.2}%)\n", pad(s_cmd_names[i], 13), m_cmd_hist[i], (float)m_cmd_hist[i] * inv_blocks);

			fmt_debug_printf("\n---- CONFIG: BC7 modes of coded blocks ----\n");
			for (uint32_t i = 0; i < 8; i++)
				if (m_mode_hist[i])
					fmt_debug_printf("mode {}: {} ({.2}%)\n", i, m_mode_hist[i], (float)m_mode_hist[i] * inv_blocks);

			// ---- endpoints ----
			fmt_debug_printf("\n---- ENDPOINTS: predictor modes + residuals ----\n");
			fmt_debug_printf("raw blocks: {}, dpcm blocks: {} ({} subsets)\n", m_ep_raw_blocks, m_ep_dpcm_blocks, m_ep_dpcm_subsets);
			fmt_debug_printf("ep modes: raw {}, left {}, up {}, ldiag {}, rdiag {}, index {}, left_s1 {}, up_s1 {}\n",
				m_ep_mode_hist[0], m_ep_mode_hist[1], m_ep_mode_hist[2], m_ep_mode_hist[3], m_ep_mode_hist[4], m_ep_mode_hist[5],
				m_ep_mode_hist[6], m_ep_mode_hist[7]);

			if (m_ep_mode_hist[(uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMBlockIndex])
			{
				std::string idx_line("ep index deltas:");
				for (uint32_t i = 0; i < NUM_XY_DELTAS; i++)
					if (m_ep_index_hist[i])
						idx_line += string_format(" (%i,%i):%llu", (int)g_xbc7_xy_deltas[i].m_dx, (int)g_xbc7_xy_deltas[i].m_dy, (unsigned long long)m_ep_index_hist[i]);
				fmt_debug_printf("{}\n", idx_line);
			}
			if (m_pbit_delta_bits)
				fmt_debug_printf("pbit deltas: {} bits, nonzero: {} ({.2}%)\n",
					m_pbit_delta_bits, m_pbit_delta_nonzero, (float)m_pbit_delta_nonzero * 100.0f / (float)m_pbit_delta_bits);

			static const char* s_class_names[2] = { "fine", "coarse" };
			static const char* s_chan_names[4] = { "R", "G", "B", "A" };
			for (uint32_t cls = 0; cls < 2; cls++)
			{
				for (uint32_t c = 0; c < 4; c++)
				{
					if (!m_ep_resid_count[cls][c])
						continue;
					fmt_debug_printf("ep resid {} {}: n={}, mean|d|={.3}, zero={.2}%\n",
						pad(s_class_names[cls], 6), s_chan_names[c],
						m_ep_resid_count[cls][c],
						(float)m_ep_resid_sum_abs[cls][c] / (float)m_ep_resid_count[cls][c],
						(float)m_ep_resid_zero[cls][c] * 100.0f / (float)m_ep_resid_count[cls][c]);
				}
			}

			// ---- weight predictors (DCT-mode blocks; DPCM-mode predictors are in the next section) ----
			fmt_debug_printf("\n---- WEIGHTS: DCT-mode predictors ----\n");
			{
				uint64_t class_totals[3] = { 0, 0, 0 }; // absolute / synthetic / dictionary
				uint64_t amp_totals[4] = { 0, 0, 0, 0 };
				uint64_t coded_blocks = 0;

				for (uint32_t c = 0; c < cTotalCandidates; c++)
				{
					for (uint32_t a = 0; a < 4; a++)
					{
						const uint64_t t = m_pred_hist[c + a * cTotalCandidates];
						amp_totals[a] += t;
						coded_blocks += t;

						if (c == cCandAbsolute)
							class_totals[0] += t;
						else if (c >= cCandFirstXYDelta)
							class_totals[2] += t;
						else
							class_totals[1] += t;
					}
				}

				const float inv_coded = coded_blocks ? (100.0f / (float)coded_blocks) : 0.0f;

				fmt_debug_printf("classes: absolute {} ({.2}%), synthetic {} ({.2}%), dictionary {} ({.2}%)\n",
					class_totals[0], (float)class_totals[0] * inv_coded,
					class_totals[1], (float)class_totals[1] * inv_coded,
					class_totals[2], (float)class_totals[2] * inv_coded);
				fmt_debug_printf("amp codes: identity {}, flip {}, half {}, half-flip {}\n",
					amp_totals[0], amp_totals[1], amp_totals[2], amp_totals[3]);

				// top 12 joint (cand, amp) symbols
				fmt_debug_printf("top predictors:\n");

				uint32_t idx[cTotalCandidates * 4];
				for (uint32_t i = 0; i < cTotalCandidates * 4; i++)
					idx[i] = i;
				std::sort(idx, idx + cTotalCandidates * 4,
					[&](uint32_t a, uint32_t b) { return m_pred_hist[a] > m_pred_hist[b]; });

				for (uint32_t r = 0; r < 12; r++)
				{
					const uint32_t i = idx[r];
					if (!m_pred_hist[i])
						break;

					const uint32_t cand = i % cTotalCandidates, amp = i / cTotalCandidates;
					static const char* s_amp_names[4] = { "", " (flip)", " (half)", " (half-flip)" };

					std::string name;
					if (cand < cCandFirstXYDelta)
						name = g_cand_names[cand];
					else
					{
						const xbc7_xy_delta& dlt = g_xbc7_xy_deltas[cand - cCandFirstXYDelta];
						name = string_format("copy (%i,%i)", (int)dlt.m_dx, (int)dlt.m_dy);
					}
					name += s_amp_names[amp];

					fmt_debug_printf("  {}: {} ({.2}%)\n", pad(name.c_str(), 28), m_pred_hist[i], (float)m_pred_hist[i] * inv_coded);
				}
			}

			// ---- weight coding mode ----
			fmt_debug_printf("\n---- WEIGHTS: DCT vs lossless DPCM decision ----\n");
			{
				const uint64_t wt_coded = m_wt_dct_blocks + m_wt_dpcm_blocks;
				const float inv_wt = wt_coded ? (100.0f / (float)wt_coded) : 0.0f;

				fmt_debug_printf("dct: {} ({.2}%), dpcm: {} ({.2}%) [decision: dpcm_bits*100 <= dct_bits*{}]\n",
					m_wt_dct_blocks, (float)m_wt_dct_blocks * inv_wt, m_wt_dpcm_blocks, (float)m_wt_dpcm_blocks * inv_wt, m_wt_alpha_pct);
				fmt_debug_printf("est bits -- all DCT: {}, all DPCM: {}, chosen: {}\n",
					m_wt_dct_est_bits_total / 8, m_wt_dpcm_est_bits_total / 8, m_wt_chosen_est_bits / 8);

				std::string l1("ACs/block when DCT won: ");
				std::string l2("ACs/block when DPCM won: ");
				for (uint32_t i = 0; i <= 32; i++)
				{
					if (m_wt_block_acs_hist_dct[i])
						l1 += string_format("%u:%llu ", i, (unsigned long long)m_wt_block_acs_hist_dct[i]);
					if (m_wt_block_acs_hist_dpcm[i])
						l2 += string_format("%u:%llu ", i, (unsigned long long)m_wt_block_acs_hist_dpcm[i]);
				}
				fmt_debug_printf("{}\n", l1);
				if (m_wt_dpcm_blocks)
					fmt_debug_printf("{}\n", l2);

				for (uint32_t cls = 0; cls < 3; cls++)
				{
					if (!m_wt_resid_count[cls])
						continue;
					fmt_debug_printf("dpcm resid {}b: n={}, mean|d|={.3}, zero={.2}%\n",
						cls + 2, m_wt_resid_count[cls],
						(float)m_wt_resid_sum_abs[cls] / (float)m_wt_resid_count[cls],
						(float)m_wt_resid_zero[cls] * 100.0f / (float)m_wt_resid_count[cls]);

					std::string mh(string_format("dpcm resid %ub |mag| hist:", cls + 2));
					for (uint32_t m = 0; m < 9; m++)
						if (m_wt_resid_mag_hist[cls][m])
							mh += string_format(" %u:%.3f%%", m, (double)m_wt_resid_mag_hist[cls][m] * 100.0 / (double)m_wt_resid_count[cls]);
					fmt_debug_printf("{}\n", mh);
				}

				if (m_wt_dpcm_blocks)
				{
					uint32_t idx[cTotalCandidates * 4];
					for (uint32_t i = 0; i < cTotalCandidates * 4; i++)
						idx[i] = i;
					std::sort(idx, idx + cTotalCandidates * 4,
						[&](uint32_t a, uint32_t b) { return m_wt_dpcm_pred_hist[a] > m_wt_dpcm_pred_hist[b]; });

					std::string tops("top dpcm predictors:");
					for (uint32_t r = 0; r < 8; r++)
					{
						const uint32_t i = idx[r];
						if (!m_wt_dpcm_pred_hist[i])
							break;

						const uint32_t cand = i % cTotalCandidates, amp = i / cTotalCandidates;
						static const char* s_amp_suffix[4] = { "", "/flip", "/half", "/half-flip" };

						std::string name;
						if (cand < cCandFirstXYDelta)
							name = g_cand_names[cand];
						else
						{
							const xbc7_xy_delta& dlt = g_xbc7_xy_deltas[cand - cCandFirstXYDelta];
							name = string_format("copy(%i,%i)", (int)dlt.m_dx, (int)dlt.m_dy);
						}

						tops += string_format(" %s%s:%llu", name.c_str(), s_amp_suffix[amp], (unsigned long long)m_wt_dpcm_pred_hist[i]);
					}
					fmt_debug_printf("{}\n", tops);
				}
			}

			// ---- solid ----
			if (m_cmd_hist[(uint32_t)xbc7_command_id::cCmdSolidDPCM])
			{
				fmt_debug_printf("\n---- SOLID: color deltas vs neighbor edge prediction ----\n");
				for (uint32_t c = 0; c < 4; c++)
				{
					if (!m_solid_delta_count[c])
						continue;
					fmt_debug_printf("{}: n={}, mean|d|={.3}, zero={.2}%\n",
						s_chan_names[c], m_solid_delta_count[c],
						(float)m_solid_delta_sum_abs[c] / (float)m_solid_delta_count[c],
						(float)m_solid_delta_zero[c] * 100.0f / (float)m_solid_delta_count[c]);
				}
			}

			// ---- DCT coefficient stats ----
			fmt_debug_printf("\n");
			m_dct_stats.print();

			fmt_debug_printf("=====================================\n\n");
		}
	};



	// NOTE: prev-emission coherence biasing of the predictor/mode sweeps was
	// tried here (prefer the previously-emitted symbol within a cost margin,
	// to cluster the predictor/command/index streams into Zstd-friendly runs)
	// and MEASURED WORSE at every margin including pure tie-breaking: the
	// existing earliest-candidate tie-break already concentrates ties onto a
	// few globally-frequent symbols, and that global skew compresses better
	// than local runs. Don't re-try without a new mechanism.

	// The mechanism that DOES work, generalized: a later sweep candidate must
	// beat the incumbent by more than this margin to displace it, so ties and
	// near-ties collapse onto the lowest-index candidates GLOBALLY (global
	// symbol skew is what Zstd prices). For the endpoint sweep the low-index
	// candidates are also the dedicated neighbor modes, which don't spend an
	// index byte. The weight-DCT sweep has its own long-standing equivalent
	// (ERR_SWITCH_MARGIN_PERMILLE).
	const uint32_t XBC7_SWEEP_SWITCH_MARGIN_PERMILLE = 30;

	// ---- effort-driven weight-predictor pruning ----------------------------
	// The per-block weight-grid sweeps (pack_weights / pack_weights_dpcm) try
	// every (cand_index, amp_code) predictor; the DCT sweep re-runs a forward+
	// inverse DCT per candidate, so it dominates encode time at Q < 100. The
	// effort knob prunes WHICH candidates are tried (not the iteration ORDER --
	// the sweeps still walk cand_index 0..cTotalCandidates and merely SKIP the
	// disabled ones, so the earliest-candidate tie-break is preserved and effort
	// 10 reproduces the pre-knob output exactly).
	//
	// g_xbc7_pred_priority lists candidates most-valuable first, derived from
	// winning-predictor histograms across many images at Q=1/10/80 (opaque +
	// alpha). The dictionary "copy(dx,dy)" predictors collectively win ~2/3 of
	// blocks, so the high-value core deliberately mixes the strongest synthetic
	// predictors (left/upper edge, diag avg) with the nearest causal copies.
	// cCandAbsolute is first -- it's the no-prediction fallback and must always
	// be available. Any candidate not listed here is appended in natural order
	// by xbc7_build_pred_mask (so the table need only spell out the head).
	static const uint8_t g_xbc7_pred_priority[] =
	{
		cCandAbsolute, cCandLeftEdge, cCandUpperEdge,
		cCandFirstXYDelta + 0, cCandFirstXYDelta + 1, cCandDiagAvg,   // <-- core 6 (effort 0)
		cCandFirstXYDelta + 6, cCandFirstXYDelta + 2, cCandFirstXYDelta + 7,
		cCandFirstXYDelta + 8, cCandReflectLeft, cCandFirstXYDelta + 4,
		cCandFirstXYDelta + 5, cCandReflectUpper, cCandPlaneFit,
		cCandFirstXYDelta + 3, cCandLUBlendStrong, cCandDDR, cCandDDL,
		cCandGradient, cCandGradientDamped, cCandLUBlend, cCandLUAvg,
		cCandUpperDiagEdgeBlend, cCandDiagEdgeBlend, cCandMED, cCandGAB,
	};

	// Build the enabled-candidate bitmask for an effort level [0,10]. effort 0
	// enables the core 6; the count ramps linearly to the full set at effort 10
	// (where the mask is all-ones -> the sweeps are byte-for-byte unchanged).
	// cTotalCandidates < 64, so a single uint64_t holds the whole set.
	static uint64_t xbc7_build_pred_mask(uint32_t effort)
	{
		static_assert(cTotalCandidates <= 64, "predictor mask must fit in uint64_t");

		effort = basisu::minimum<uint32_t>(effort, 10);

		const uint32_t cCoreCands = 6;
		uint32_t count = cCoreCands + ((cTotalCandidates - cCoreCands) * effort + 5) / 10;
		count = basisu::minimum<uint32_t>(count, cTotalCandidates);

		// priority head, then any remaining candidate in natural order -> a valid
		// permutation of [0, cTotalCandidates) regardless of what the head omits
		uint8_t order[cTotalCandidates];
		bool used[cTotalCandidates] = { false };
		uint32_t n = 0;
		for (uint32_t i = 0; i < sizeof(g_xbc7_pred_priority); i++)
		{
			const uint8_t c = g_xbc7_pred_priority[i];
			assert((c < cTotalCandidates) && !used[c]); // head must be unique & in-range
			used[c] = true;
			order[n++] = c;
		}
		for (uint32_t c = 0; c < cTotalCandidates; c++)
			if (!used[c])
				order[n++] = (uint8_t)c;
		assert(n == cTotalCandidates);

		uint64_t mask = 0;
		for (uint32_t i = 0; i < count; i++)
			mask |= (1ull << order[i]);

		assert(mask & (1ull << cCandAbsolute)); // fallback always present
		return mask;
	}

	// Amp codes the per-block sweeps try, by effort [0,10]. The amp loop runs in
	// descending win-share order (0=identity, 1=flip, 2=half-contrast, 3=half-
	// flip -- confirmed by histograms), so trying the first N keeps the most
	// valuable. effort 10 -> 4 (the full set -> output unchanged vs pre-knob).
	static uint32_t xbc7_amp_codes_for_effort(uint32_t effort)
	{
		static const uint8_t tbl[11] = { 1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 4 };
		return tbl[basisu::minimum<uint32_t>(effort, 10)];
	}

	bool pack_weights(
		uint32_t bx, uint32_t by, uint32_t num_blocks_x,
		const tile_bounds& tile,
		const color_rgba orig_block[16],
		const vector2D<basist::bc7u::log_bc7_block> &log_blks,
		dct_syms best_syms[2], basist::bc7u::log_bc7_block &best_cand_log_blk, uint32_t& best_predictor_index,
		xbc7_weight_grid_dct_fixed &weight_grid_dct_fixed, fxvec &dct_work_fixed, const pack_options& opts,
		uint64_t enabled_pred_mask, uint32_t num_amp_codes)
	{
		best_predictor_index = 0;

		const uint32_t global_q = opts.m_dct_q;

		const uint64_t ERR_SWITCH_MARGIN_PERMILLE = 20;
		const uint32_t TOTAL_AMP_CODES = num_amp_codes; // 0=identity, 1=flip, 2=half contrast, 3=half of flip (effort-pruned)

		const basist::bc7u::log_bc7_block& log_blk = log_blks(bx, by);

		best_syms[0].clear();
		best_syms[1].clear();
		best_cand_log_blk = log_blk;
		
		uint64_t best_err = UINT64_MAX;
		uint32_t best_num_ac_syms = UINT32_MAX;

		for (uint32_t amp_code = 0; amp_code < TOTAL_AMP_CODES; amp_code++)
		{
			for (uint32_t cand_index = 0; cand_index < cTotalCandidates; cand_index++)
			{
				if ((amp_code) && (cand_index == cCandAbsolute))
					continue;

				// effort pruning: skip predictors not in this effort's enabled set
				// (mask is all-ones at effort 10, so this never fires there)
				if (!((enabled_pred_mask >> cand_index) & 1u))
					continue;

				basist::bc7u::log_bc7_block cand_log_blk(log_blk);

				uint32_t cand_total_ac_syms = 0;

				dct_syms cand_syms[2];
				cand_syms[0].clear();
				cand_syms[1].clear();

				for (uint32_t p = 0; p < cand_log_blk.m_num_planes; p++)
				{
					int weight_preds[16];
					int* pWeight_predictions = nullptr;

					if (cand_index != cCandAbsolute)
					{
						bool eval_status = eval_weight_predictor(
							cand_index, amp_code,
							bx, by, num_blocks_x,
							tile,
							log_blks,
							p, weight_preds);

						if (!eval_status)
							goto skip_cand;

						pWeight_predictions = weight_preds;
					}

					dct_syms syms;

					weight_grid_dct_fixed.forward(basist::fixed16_16::from_int(global_q), p, pWeight_predictions, cand_log_blk, syms, dct_work_fixed);

					memset(cand_log_blk.m_weights[p], 0, 16);

					bool status = weight_grid_dct_fixed.inverse(basist::fixed16_16::from_int(global_q), p, pWeight_predictions, syms, cand_log_blk, dct_work_fixed);

					if (!status)
					{
						assert(0);
						return false;
					}

					uint32_t total_acs = syms.m_ac_vals.size_u32();
					if (total_acs && (syms.m_ac_vals[total_acs - 1].m_coeff == INT16_MAX))
						total_acs--;
					cand_total_ac_syms += total_acs;

					cand_syms[p] = syms;

				} // p

				// scoped so 'goto skip_cand' leaps over (not into) these inits -- required under /permissive-
				{
				color_rgba cand_block_pixels[16];
				if (!basist::bc7u::unpack_bc7(cand_log_blk, (basist::color_rgba*)cand_block_pixels))
				{
					assert(0);
					return false;
				}

				uint64_t cand_err = 0;
				for (uint32_t i = 0; i < 16; i++)
					cand_err += cand_block_pixels[i].get_weighted_dist2(orig_block[i], opts.m_weights);

				if ((cand_total_ac_syms < best_num_ac_syms) ||
					((cand_total_ac_syms == best_num_ac_syms) && ((cand_err * 1000) < (best_err * (1000 - ERR_SWITCH_MARGIN_PERMILLE)))))
				{
					best_cand_log_blk = cand_log_blk;
					best_predictor_index = cand_index + amp_code * cTotalCandidates;
					best_num_ac_syms = cand_total_ac_syms;
					best_err = cand_err;
					best_syms[0] = cand_syms[0];
					best_syms[1] = cand_syms[1];
				}
				} // end candidate-eval scope

			skip_cand:
				;

			} // cand_index

		} // amp_code

		assert(best_syms[0].m_ac_vals.size());

		return true;
	}

	// Estimated per-symbol coding costs in 1/8-bit units ("octobits"), measured
	// on the kodim corpus via the stats dashboard.
	//
	// DPCM weight residual cost, [weight_bits - 2][min(|centered resid|, 8)]:
	// -log2 of each symbol's probability (the magnitude's mass split across
	// +/-), measured at Q=100 where EVERY block codes DPCM, so the
	// distributions carry no mode-decision selection bias. Order-0 model;
	// Zstd lands within ~13% of it on these streams.
	static const uint32_t g_dpcm_resid_cost_obits[3][9] =
	{
		{ 8, 17, 47, 47, 47, 47, 47, 47, 47 },		// 2-bit (centered range [-2,1])
		{ 12, 19, 27, 38, 63, 63, 63, 63, 63 },	// 3-bit (centered range [-4,3])
		{ 12, 25, 29, 33, 38, 44, 52, 61, 81 },	// 4-bit (centered range [-8,7])
	};

	// DCT stream bytes priced at their measured post-Zstd cost (Q 60..95):
	// DC bytes ~4.7 bits with the 6-bit DC quantization below (was ~6.4 at
	// 8-bit DC), AC run/magnitude stream bytes 3.1..3.8 bits (3.5 used).
	// Sign bits are incompressible noise: exactly 1 bit each.
	const uint32_t DCT_DC_BYTE_COST_OBITS = 38;
	const uint32_t DCT_AC_BYTE_COST_OBITS = 28;

	// Lossless residual DPCM weight coding: predict the dequantized [0,64] grid
	// with the shared predictor bank, quantize the prediction to the plane's bit
	// depth (quant_weight), then wrap-difference the quantized indices mod 2^n.
	// Reconstruction is exact, so the predictor choice affects ONLY compressed
	// size: pick the candidate minimizing the estimated emitted cost (octobit
	// table above) -- a content-adaptive estimate, since zero-heavy residual
	// grids are far cheaper than the stream average. cand 0 (absolute,
	// predictor index 0) is always available, emits the raw indices to a
	// separate blob, and is priced at its raw stored size.
	struct dpcm_weights
	{
		uint32_t m_pred_index = 0;		// joint (cand + amp * cTotalCandidates) byte
		uint8_t m_syms[2][16] = {};		// wrapped n-bit symbols (raw indices if absolute), [plane][texel]
		uint64_t m_est_cost_obits = 0;	// estimated emitted cost of the winning candidate, 1/8-bit units
	};

	static void pack_weights_dpcm(
		uint32_t bx, uint32_t by, uint32_t num_blocks_x,
		const tile_bounds& tile,
		const vector2D<basist::bc7u::log_bc7_block>& log_blks,
		dpcm_weights& result,
		uint64_t enabled_pred_mask, uint32_t num_amp_codes)
	{
		const uint32_t TOTAL_AMP_CODES = num_amp_codes;

		const basist::bc7u::log_bc7_block& log_blk = log_blks(bx, by);

		uint64_t best_cost = UINT64_MAX;

		for (uint32_t amp_code = 0; amp_code < TOTAL_AMP_CODES; amp_code++)
		{
			for (uint32_t cand_index = 0; cand_index < cTotalCandidates; cand_index++)
			{
				if ((amp_code) && (cand_index == cCandAbsolute))
					continue;

				// effort pruning (see pack_weights): all-ones at effort 10
				if (!((enabled_pred_mask >> cand_index) & 1u))
					continue;

				uint64_t cost = 0;
				uint8_t syms[2][16] = {}; // zeroed so single-plane blocks never copy indeterminate plane-1 bytes
				bool valid = true;

				for (uint32_t p = 0; p < log_blk.m_num_planes; p++)
				{
					const uint32_t num_bits = log_blk.m_weight_bits[p];
					const uint32_t mask = (1u << num_bits) - 1;
					const int half = 1 << (num_bits - 1);

					int weight_preds[16];
					int* pWeight_predictions = nullptr;

					if (cand_index != cCandAbsolute)
					{
						if (!eval_weight_predictor(cand_index, amp_code, bx, by, num_blocks_x, tile, log_blks, p, weight_preds))
						{
							valid = false;
							break;
						}
						pWeight_predictions = weight_preds;
					}

					for (uint32_t i = 0; i < 16; i++)
					{
						const uint32_t pred_index = pWeight_predictions ? basist::bc7u::quant_weight(pWeight_predictions[i], num_bits) : 0;
						const uint32_t sym = (log_blk.m_weights[p][i] - pred_index) & mask;
						syms[p][i] = (uint8_t)sym;

						if (pWeight_predictions)
						{
							const int v = (sym >= (uint32_t)half) ? ((int)sym - (int)(mask + 1)) : (int)sym;
							cost += g_dpcm_resid_cost_obits[num_bits - 2][basisu::minimum<int>(basisu::iabs(v), 8)];
						}
						else
						{
							// absolute: raw indices at their stored size (2 bits,
							// or 4 for the nibble-expanded 3-bit class, or 4)
							cost += 8u * ((num_bits == 2) ? 2u : 4u);
						}
					}
				} // p

				if (!valid)
					continue;

				// later candidates must beat the incumbent by the switch margin
				// (UINT64_MAX guard: the multiply would wrap before any winner)
				if ((best_cost == UINT64_MAX) || ((cost * 1000) < (best_cost * (1000 - (uint64_t)XBC7_SWEEP_SWITCH_MARGIN_PERMILLE))))
				{
					best_cost = cost;
					result.m_pred_index = cand_index + amp_code * cTotalCandidates;
					memcpy(result.m_syms, syms, sizeof(syms));
				}

			} // cand_index
		} // amp_code

		result.m_est_cost_obits = best_cost;
	}

	// per-stripe (per-job) output state; merged in stripe order after the jobs join
	struct stripe_output
	{
		blob_stream_writer m_blob_writer;
		basisu::bitwise_coder m_coeff_signs;
		basisu::bitwise_coder m_pbits;
		basisu::bitwise_coder m_raw_endpoint_bits;
		xbc7_pack_stats m_stats;
	};

	// Optional block-reuse RDO pre-pass: runs after the BC7 base pack, BEFORE the
	// endpoint RDO (once a whole block is reused there's no point trying cheaper
	// endpoints). In raster order per stripe (honoring boundaries), for each
	// non-solid block it tries the cheapest whole-block representations and adopts
	// the best acceptable one: a REPEAT (exact copy of the left/upper causal
	// neighbor -> 1-byte command; preferred) or SOLID (the block's mean color ->
	// cheap SolidDPCM). "Acceptable" = weighted RGBA PSNR >= the shared floor AND
	// within the per-mode tolerated drop vs the block's current PSNR. Modifies
	// log_blks in place; serial (causal). Returns false on a decode failure.
	static bool block_reuse_rdo_pass(
		const image& orig_img,
		basisu::vector2D<basist::bc7u::log_bc7_block>& log_blks,
		uint32_t num_blocks_x,
		const basisu::vector<stripe_range>& stripes,
		const pack_options& opts)
	{
		const uint32_t* comp_weights = opts.m_weights;
		const float min_block_psnr = opts.m_rdo_min_block_psnr;
		const bool has_alpha = orig_img.has_alpha();

		uint32_t changed_repeat = 0, changed_solid = 0;

		for (uint32_t st = 0; st < stripes.size(); st++)
		{
			const uint32_t first_row = stripes[st].m_first_block_row;
			const uint32_t end_row = first_row + stripes[st].m_num_block_rows;

			for (uint32_t by = first_row; by < end_row; by++)
			{
				for (uint32_t bx = 0; bx < num_blocks_x; bx++)
				{
					basist::bc7u::log_bc7_block& log_blk = log_blks(bx, by);

					// NOTE: solid blocks are NOT skipped -- a RepeatLast/RepeatUpper
					// command (1 byte) is cheaper than SolidDPCM, and pack_stripe checks
					// Repeat before Solid, so we aggressively reuse a neighbor even for a
					// solid block when it passes the PSNR test (VQ-style).
					color_rgba src_pixels[16];
					orig_img.extract_block_clamped(src_pixels, bx * 4, by * 4, 4, 4);

					color_rgba dec[16];
					if (!basist::bc7u::unpack_bc7(log_blk, (basist::color_rgba*)dec))
					{
						assert(0);
						return false;
					}
					const float orig_psnr = xbc7_block_wsse_psnr(src_pixels, dec, comp_weights);

					// ---- Repeat: copy a causal neighbor wholesale (cheapest command) ----
					if (opts.m_repeat_rdo_enabled)
					{
						const basist::bc7u::log_bc7_block* preds[2] = { nullptr, nullptr };
						if (bx >= 1)        preds[0] = &log_blks(bx - 1, by); // left  -> RepeatLast
						if (by > first_row) preds[1] = &log_blks(bx, by - 1); // upper -> RepeatUpper

						const basist::bc7u::log_bc7_block* best = nullptr;
						float best_psnr = 0.0f;
						for (uint32_t i = 0; i < 2; i++)
						{
							if (!preds[i])
								continue;
							if (!basist::bc7u::unpack_bc7(*preds[i], (basist::color_rgba*)dec))
							{
								assert(0);
								return false;
							}
							const float p = xbc7_block_wsse_psnr(src_pixels, dec, comp_weights);
							if ((p >= min_block_psnr) && (p >= orig_psnr - opts.m_repeat_rdo_max_psnr_drop) && ((!best) || (p > best_psnr)))
							{
								best = preds[i];
								best_psnr = p;
							}
						}

						if (best)
						{
							log_blk = *best; // bit-identical -> pack_stripe emits Repeat
							changed_repeat++;
							continue; // Repeat is the cheapest -- done with this block
						}
					}

					// ---- Solid: replace with the block's mean color (next cheapest) ----
					// (skip if already solid -- it already codes cheaply as SolidDPCM)
					if (opts.m_solid_rdo_enabled && !basist::bc7u::is_solid_blk(log_blk))
					{
						uint32_t sum[4] = { 0, 0, 0, 0 };
						for (uint32_t i = 0; i < 16; i++)
							for (uint32_t c = 0; c < 4; c++)
								sum[c] += src_pixels[i][c];

						color_rgba mean((sum[0] + 8) >> 4, (sum[1] + 8) >> 4, (sum[2] + 8) >> 4, (sum[3] + 8) >> 4);
						if (!has_alpha)
							mean.a = 255;

						basist::bc7u::log_bc7_block cand;
						cand.clear();
						basist::bc7u::create_solid_blk(cand, (const basist::color_rgba&)mean);

						if (!basist::bc7u::unpack_bc7(cand, (basist::color_rgba*)dec))
						{
							assert(0);
							return false;
						}
						const float p = xbc7_block_wsse_psnr(src_pixels, dec, comp_weights);
						if ((p >= min_block_psnr) && (p >= orig_psnr - opts.m_solid_rdo_max_psnr_drop))
						{
							log_blk = cand;
							changed_solid++;
						}
					}
				}
			}
		}

		if (opts.m_debug_output)
		{
			uint32_t total_blocks = 0;
			for (uint32_t st = 0; st < stripes.size(); st++)
				total_blocks += num_blocks_x * stripes[st].m_num_block_rows;
			if (opts.m_repeat_rdo_enabled)
				fmt_debug_printf("repeat RDO: {} blocks changed ({.2}%)\n", changed_repeat, total_blocks ? (changed_repeat * 100.0f / (float)total_blocks) : 0.0f);
			if (opts.m_solid_rdo_enabled)
				fmt_debug_printf("solid RDO: {} blocks changed ({.2}%)\n", changed_solid, total_blocks ? (changed_solid * 100.0f / (float)total_blocks) : 0.0f);
		}

		return true;
	}

	// Optional endpoint-prediction RDO pre-pass: runs after the BC7 base pack but
	// BEFORE stripe coding. In raster order (per stripe, honoring its boundaries)
	// it tries forcing each block's endpoints to a causal neighbor's prediction
	// (left/upper/left-diag/right-diag) via a zero-residual endpoint DPCM, and
	// re-optimizes that candidate's weights for the new endpoints, and adopts the
	// best candidate when its weighted RGBA PSNR drops by no more than
	// max_psnr_drop dB. The matching zero residuals then cost almost nothing in
	// the coding pass (often collapsing to a Repeat). Solid blocks are skipped --
	// they have their own cheaper SolidDPCM path. Modifies log_blks in place;
	// serial, since each block predicts from already-finalized causal neighbors.
	// Returns false on an (unexpected) decode failure.
	static bool endpoint_dpcm_rdo_pass(
		const image& orig_img,
		basisu::vector2D<basist::bc7u::log_bc7_block>& log_blks,
		uint32_t num_blocks_x,
		const basisu::vector<stripe_range>& stripes,
		const pack_options& opts)
	{
		const uint32_t* comp_weights = opts.m_weights;
		const float max_psnr_drop = opts.m_endpoint_rdo_max_psnr_drop;
		const float min_block_psnr = opts.m_rdo_min_block_psnr;

		uint32_t changed_blocks = 0; // blocks whose endpoints+weights were rewritten

#if defined(DEBUG) || defined(_DEBUG)
		// sanity: re-optimizing a candidate's weights can never RAISE its WSSE
		// (the original weights are in the swept set); count any case where it does
		uint64_t dbg_weight_opt_cands = 0, dbg_weight_opt_worse = 0;
#endif

		for (uint32_t st = 0; st < stripes.size(); st++)
		{
			const uint32_t first_row = stripes[st].m_first_block_row;
			const uint32_t end_row = first_row + stripes[st].m_num_block_rows;

			for (uint32_t by = first_row; by < end_row; by++)
			{
				for (uint32_t bx = 0; bx < num_blocks_x; bx++)
				{
					basist::bc7u::log_bc7_block& log_blk = log_blks(bx, by);

					// solid blocks have a dedicated, cheaper command -- leave them alone
					if (basist::bc7u::is_solid_blk(log_blk))
						continue;

					// already identical to a causal neighbor (e.g. from the block-reuse
					// RDO, or naturally) -- it'll code as a cheap Repeat, so don't
					// disturb its endpoints
					if (((bx >= 1) && basist::bc7u::compare_block_full(log_blk, log_blks(bx - 1, by))) ||
						((by > first_row) && basist::bc7u::compare_block_full(log_blk, log_blks(bx, by - 1))))
						continue;

					color_rgba src_pixels[16];
					orig_img.extract_block_clamped(src_pixels, bx * 4, by * 4, 4, 4);

					color_rgba orig_pixels[16];
					if (!basist::bc7u::unpack_bc7(log_blk, (basist::color_rgba*)orig_pixels))
					{
						assert(0);
						return false;
					}
					const float orig_psnr = xbc7_block_wsse_psnr(src_pixels, orig_pixels, comp_weights);

					// causal neighbor predictors valid within this stripe, in order:
					// left, upper, left-diag, right-diag (upper rows must stay in-stripe)
					const basist::bc7u::log_bc7_block* preds[4] = { nullptr, nullptr, nullptr, nullptr };
					if (bx >= 1)                                     preds[0] = &log_blks(bx - 1, by);
					if (by > first_row)                              preds[1] = &log_blks(bx, by - 1);
					if ((bx >= 1) && (by > first_row))               preds[2] = &log_blks(bx - 1, by - 1);
					if ((bx + 1 < num_blocks_x) && (by > first_row)) preds[3] = &log_blks(bx + 1, by - 1);

					basist::bc7u::log_bc7_block best_cand{}; // value-init silences a C4701 false positive (only read when have_cand)
					float best_cand_psnr = 0.0f;
					bool have_cand = false;

					for (uint32_t i = 0; i < 4; i++)
					{
						if (!preds[i])
							continue;

						// copy keeps mode/config/weights; only endpoints+pbits change
						basist::bc7u::log_bc7_block cand(log_blk);

						for (uint32_t s = 0; s < cand.m_num_partitions; s++)
						{
							uint8_t residuals[8] = { 0 }; // zero residuals -> endpoints become the prediction
							uint8_t pbits[2] = { 0 };
							uint32_t num_residuals = 0, num_pbits = 0;
							basist::bc7u::endpoint_dpcm(true, *preds[i], 0, cand, s, residuals, num_residuals, pbits, num_pbits);
						}

#if defined(DEBUG) || defined(_DEBUG)
						// WSSE with the now-stale weights, before re-optimization
						uint64_t dbg_wsse_before = 0;
						{
							color_rgba pre[16];
							if (!basist::bc7u::unpack_bc7(cand, (basist::color_rgba*)pre))
							{
								assert(0);
								return false;
							}
							for (uint32_t k = 0; k < 16; k++)
								dbg_wsse_before += pre[k].get_weighted_dist2(src_pixels[k], comp_weights);
						}
#endif

						// the slammed endpoints leave the copied weights stale -- recompute
						// the optimal weights for the new endpoints (can only help vs. stale)
						basist::bc7u::log_bc7_block cand_opt;
						if (!optimize_block_weights(cand, src_pixels, comp_weights, cand_opt))
							return false; // already asserted inside
						cand = cand_opt;

						color_rgba cand_pixels[16];
						if (!basist::bc7u::unpack_bc7(cand, (basist::color_rgba*)cand_pixels))
						{
							assert(0);
							return false;
						}

#if defined(DEBUG) || defined(_DEBUG)
						{
							uint64_t dbg_wsse_after = 0;
							for (uint32_t k = 0; k < 16; k++)
								dbg_wsse_after += cand_pixels[k].get_weighted_dist2(src_pixels[k], comp_weights);

							dbg_weight_opt_cands++;
							if (dbg_wsse_after > dbg_wsse_before)
								dbg_weight_opt_worse++; // weight re-optimization must never make WSSE worse
						}
#endif

						const float cand_psnr = xbc7_block_wsse_psnr(src_pixels, cand_pixels, comp_weights);
						if ((!have_cand) || (cand_psnr > best_cand_psnr))
						{
							have_cand = true;
							best_cand_psnr = cand_psnr;
							best_cand = cand;
						}
					}

					// adopt the best candidate if quality drops by no more than allowed
					// (a gain is of course also accepted)
					if (have_cand && (best_cand_psnr >= min_block_psnr) && (best_cand_psnr >= orig_psnr - max_psnr_drop))
					{
						log_blk = best_cand;
						changed_blocks++;
					}
				}
			}
		}

		if (opts.m_debug_output)
		{
			uint32_t total_blocks = 0;
			for (uint32_t st = 0; st < stripes.size(); st++)
				total_blocks += num_blocks_x * stripes[st].m_num_block_rows;
			fmt_debug_printf("endpoint RDO: {} blocks changed ({.2}%)\n", changed_blocks, total_blocks ? (changed_blocks * 100.0f / (float)total_blocks) : 0.0f);
		}

#if defined(DEBUG) || defined(_DEBUG)
		fmt_debug_printf("endpoint RDO weight re-opt: {} candidates, {} got WORSE (expected 0)\n", dbg_weight_opt_cands, dbg_weight_opt_worse);
		// assert(!dbg_weight_opt_worse); // intentionally NOT asserted -- probably too strong; observe via the stat above instead
#endif

		return true;
	}

	// Optional weight-DCT AC-truncation RDO. Run ONCE on the winning DCT symbols
	// (after pack_weights, NOT inside its candidate sweep). Zeros the highest-
	// frequency weight-DCT AC coefficients one at a time in reverse zigzag order,
	// protecting the 2x2 low-freq corner (DC,(1,0),(0,1),(1,1) -> zigzag {0,1,2,4}),
	// while the decoded weighted RGBA PSNR stays within m_ac_trunc_rdo_max_psnr_drop
	// dB of the un-truncated block AND >= the shared floor. Updates best_syms
	// (pruned) and best_cand_log_blk (weights rebuilt via the same inverse() the
	// decoder uses, so it round-trips). Returns false on an unexpected decode failure.
	static bool ac_truncate_rdo(
		dct_syms best_syms[2],
		basist::bc7u::log_bc7_block& best_cand_log_blk,
		uint32_t best_predictor_index,
		const color_rgba orig_block[16],
		uint32_t bx, uint32_t by, uint32_t num_blocks_x,
		const tile_bounds& tile,
		const vector2D<basist::bc7u::log_bc7_block>& log_blks,
		xbc7_weight_grid_dct_fixed& weight_grid_dct_fixed,
		fxvec& dct_work,
		const pack_options& opts,
		xbc7_pack_stats& stats)
	{
		const uint32_t num_planes = best_cand_log_blk.m_num_planes;

		// winning predictor -> per-plane predictions (pack_weights left untouched;
		// eval_weight_predictor asserts non-absolute, so absolute -> null preds)
		const uint32_t cand_index = best_predictor_index % cTotalCandidates;
		const uint32_t amp_code = best_predictor_index / cTotalCandidates;

		int preds[2][16];
		const int* pPreds[2] = { nullptr, nullptr };
		if (cand_index != cCandAbsolute)
		{
			for (uint32_t p = 0; p < num_planes; p++)
			{
				if (!eval_weight_predictor(cand_index, amp_code, bx, by, num_blocks_x, tile, log_blks, p, preds[p]))
				{
					assert(0);
					return false;
				}
				pPreds[p] = preds[p];
			}
		}

		// baseline PSNR of the un-truncated DCT-coded block
		color_rgba dec[16];
		if (!basist::bc7u::unpack_bc7(best_cand_log_blk, (basist::color_rgba*)dec))
		{
			assert(0);
			return false;
		}
		const float orig_psnr = xbc7_block_wsse_psnr(orig_block, dec, opts.m_weights);

		int flat[2][16];
		for (uint32_t p = 0; p < num_planes; p++)
			xbc7_syms_to_flat(best_syms[p], flat[p]);

		const float drop = opts.m_ac_trunc_rdo_max_psnr_drop;
		const float floor_psnr = opts.m_rdo_min_block_psnr;

		uint64_t pruned_here = 0;

		// reverse zigzag, skipping protected low-freq slots (zigzag 1,2,4; 0 = DC)
		for (uint32_t zz = 15; zz >= 1; zz--)
		{
			if ((zz == 1) || (zz == 2) || (zz == 4))
				continue;

			const uint32_t nat = g_zigzag4x4_xy[zz][0] + g_zigzag4x4_xy[zz][1] * 4;

			// skip if this coefficient is already zero in every plane
			bool any_nonzero = false;
			for (uint32_t p = 0; p < num_planes; p++)
				if (flat[p][nat] != 0)
					any_nonzero = true;
			if (!any_nonzero)
				continue;

			int saved[2] = { 0, 0 };
			for (uint32_t p = 0; p < num_planes; p++)
			{
				saved[p] = flat[p][nat];
				flat[p][nat] = 0;
			}

			// rebuild the candidate from the tentatively-truncated grids
			basist::bc7u::log_bc7_block trial_blk = best_cand_log_blk; // config + endpoints kept
			dct_syms trial_syms[2];
			for (uint32_t p = 0; p < num_planes; p++)
			{
				xbc7_flat_to_syms(flat[p], trial_syms[p]);
				memset(trial_blk.m_weights[p], 0, 16);
				if (!weight_grid_dct_fixed.inverse(basist::fixed16_16::from_int(opts.m_dct_q), p, pPreds[p], trial_syms[p], trial_blk, dct_work))
				{
					assert(0);
					return false;
				}
			}

			if (!basist::bc7u::unpack_bc7(trial_blk, (basist::color_rgba*)dec))
			{
				assert(0);
				return false;
			}
			const float psnr = xbc7_block_wsse_psnr(orig_block, dec, opts.m_weights);

			if ((psnr >= floor_psnr) && (psnr >= orig_psnr - drop))
			{
				best_cand_log_blk = trial_blk;
				for (uint32_t p = 0; p < num_planes; p++)
				{
					best_syms[p] = trial_syms[p];
					if (saved[p])
						pruned_here++;
				}
			}
			else
			{
				for (uint32_t p = 0; p < num_planes; p++)
					flat[p][nat] = saved[p]; // revert this coefficient
				break; // stop at the first unacceptable truncation
			}
		}

		stats.m_ac_trunc_pruned += pruned_here;
		if (pruned_here)
			stats.m_ac_trunc_blocks++;

		return true;
	}

	// with every causal reference clamped to the stripe (the decoder mirrors
	// the one IMPLICIT cross-row prediction -- solid blocks -- via the header
	// stripe count; explicit references are simply never emitted across a
	// stripe boundary). All scratch state is local or lives in `out`.
	static bool pack_stripe(
		const image& orig_img,
		const pack_options& opts,
		bool has_alpha,
		uint32_t num_blocks_x,
		const stripe_range& stripe,
		vector2D<basist::bc7u::log_bc7_block>& log_blks,
		vector2D<basist::bc7u::log_bc7_block>* pCoded_log_blocks,
		const vector2D<uint8_t>& base_used_lut, // per-block: 1 if the bc7e_scalar base pack used its endpoint LUTs (force DPCM)
		stripe_output& out,
		const xbc7_debug_image_set& dbg_imgs) // debug only; members null unless m_debug_images. Stripes write disjoint rows, so filling is thread-safe.
	{
		const uint32_t first_row = stripe.m_first_block_row;
		const uint32_t end_row = stripe.m_first_block_row + stripe.m_num_block_rows;

		// this stripe as an inclusive block AABB (full width, for now); ALL
		// causal references in this pass are gated through tile.contains()
		const tile_bounds tile = { 0, (int)first_row, (int)num_blocks_x - 1, (int)end_row - 1 };

		blob_stream_writer& blob_writer = out.m_blob_writer;
		basisu::bitwise_coder& coeff_signs = out.m_coeff_signs;
		basisu::bitwise_coder& pbits = out.m_pbits;
		basisu::bitwise_coder& raw_endpoint_bits = out.m_raw_endpoint_bits;
		xbc7_pack_stats& stats = out.m_stats;

		xbc7_weight_grid_dct_fixed weight_grid_dct_fixed;
		weight_grid_dct_fixed.init();

		fxvec dct_work_fixed;

		// hoisted so the per-block visualization calls compile out entirely when disabled
		const bool debug_images = opts.m_debug_images;

		// effort -> enabled weight-predictor set + amp-code count (constant for the
		// whole stripe; at effort 10 both are full so the sweeps are unchanged)
		const uint64_t enabled_pred_mask = xbc7_build_pred_mask(opts.m_effort_level);
		const uint32_t num_amp_codes = xbc7_amp_codes_for_effort(opts.m_effort_level);

		for (uint32_t by = first_row; by < end_row; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				// causal neighbors, clamped to the tile AABB: blocks outside it
				// belong to another coding unit and are off limits
				const basist::bc7u::log_bc7_block* pLeft_log_blk = tile.contains((int)bx - 1, (int)by) ? &log_blks(bx - 1, by) : nullptr;
				const basist::bc7u::log_bc7_block* pUp_log_blk = tile.contains((int)bx, (int)by - 1) ? &log_blks(bx, by - 1) : nullptr;

				const basist::bc7u::log_bc7_block* pLeft_diag_log_blk = tile.contains((int)bx - 1, (int)by - 1) ? &log_blks(bx - 1, by - 1) : nullptr;
				const basist::bc7u::log_bc7_block* pRight_diag_log_blk = tile.contains((int)bx + 1, (int)by - 1) ? &log_blks(bx + 1, by - 1) : nullptr;

				basist::bc7u::log_bc7_block& log_blk = log_blks(bx, by);

				if (pUp_log_blk && basist::bc7u::compare_block_full(log_blk, *pUp_log_blk))
				{
					blob_writer.put_byte(cBlobCommands, (uint8_t)xbc7_command_id::cCmdRepeatUpper);
					stats.m_cmd_hist[(uint32_t)xbc7_command_id::cCmdRepeatUpper]++;

					if (debug_images)
						xbc7_vis_fill_command(dbg_imgs, bx, by, (uint8_t)xbc7_command_id::cCmdRepeatUpper, false, UINT32_MAX, UINT32_MAX);

					if (pCoded_log_blocks)
					{
						(*pCoded_log_blocks)(bx, by) = log_blk;
					}

					continue;
				}

				if (pLeft_log_blk && basist::bc7u::compare_block_full(log_blk, *pLeft_log_blk))
				{
					blob_writer.put_byte(cBlobCommands, (uint8_t)xbc7_command_id::cCmdRepeatLast);
					stats.m_cmd_hist[(uint32_t)xbc7_command_id::cCmdRepeatLast]++;

					if (debug_images)
						xbc7_vis_fill_command(dbg_imgs, bx, by, (uint8_t)xbc7_command_id::cCmdRepeatLast, false, UINT32_MAX, UINT32_MAX);

					if (pCoded_log_blocks)
					{
						(*pCoded_log_blocks)(bx, by) = log_blk;
					}

					continue;
				}
								
				if (basist::bc7u::is_solid_blk(log_blk))
				{
					int preds[4] = {};
					int num_preds = 0;

					if (pLeft_log_blk)
					{
						for (uint32_t y = 0; y < 4; y++)
						{
							basist::color_rgba p;
							if (!basist::bc7u::unpack_bc7_texel(*pLeft_log_blk, p, 3, y))
							{
								assert(0);
								return false;
							}
							preds[0] += p.r;
							preds[1] += p.g;
							preds[2] += p.b;
							preds[3] += p.a;
						}
						num_preds += 4;
					}

					if (pUp_log_blk)
					{
						for (uint32_t x = 0; x < 4; x++)
						{
							basist::color_rgba p;
							if (!basist::bc7u::unpack_bc7_texel(*pUp_log_blk, p, x, 3))
							{
								assert(0);
								return false;
							}
							preds[0] += p.r;
							preds[1] += p.g;
							preds[2] += p.b;
							preds[3] += p.a;
						}
						num_preds += 4;
					}

					if (num_preds)
					{
						for (uint32_t c = 0; c < 4; c++)
							preds[c] = (preds[c] + (num_preds / 2)) / num_preds;
					}

					blob_writer.put_byte(cBlobCommands, (uint8_t)xbc7_command_id::cCmdSolidDPCM);
					stats.m_cmd_hist[(uint32_t)xbc7_command_id::cCmdSolidDPCM]++;

					if (debug_images)
						xbc7_vis_fill_command(dbg_imgs, bx, by, (uint8_t)xbc7_command_id::cCmdSolidDPCM, false, UINT32_MAX, UINT32_MAX);

					basist::color_rgba solid_color;
					if (!basist::bc7u::unpack_bc7_texel(log_blk, solid_color, 0, 0))
					{
						assert(0);
						return false;
					}

					if (!has_alpha)
						solid_color.a = 255; // important in case a p-bit set a=254
					
					for (uint32_t c = 0; c < (has_alpha ? 4u : 3u); c++)
					{
						const uint8_t delta = (uint8_t)(solid_color[c] - preds[c]);
						blob_writer.put_byte(cBlobSolidRGBADeltas, delta);

						stats.m_solid_delta_count[c]++;
						stats.m_solid_delta_sum_abs[c] += (uint64_t)basisu::iabs((int8_t)delta);
						if (!delta)
							stats.m_solid_delta_zero[c]++;
					}

					basist::bc7u::create_solid_blk(log_blk, solid_color);

					if (pCoded_log_blocks)
					{
						(*pCoded_log_blocks)(bx, by) = log_blk;
					}

					continue;
				}

				color_rgba orig_block[16];
				orig_img.extract_block_clamped(orig_block, bx * 4, by * 4, 4, 4);

				uint8_t command_byte = 0;

				// Code config
				if ((pLeft_log_blk) && (basist::bc7u::compare_block_configs(log_blk, *pLeft_log_blk, false)))
				{
					command_byte = (uint8_t)xbc7_command_id::cCmdReuseConfigLeft;
				}
				else if ((pUp_log_blk) && (basist::bc7u::compare_block_configs(log_blk, *pUp_log_blk, false)))
				{
					command_byte = (uint8_t)xbc7_command_id::cCmdReuseConfigUpper;
				}
				else if ((pLeft_diag_log_blk) && (basist::bc7u::compare_block_configs(log_blk, *pLeft_diag_log_blk, false)))
				{
					command_byte = (uint8_t)xbc7_command_id::cCmdReuseConfigLeftDiagonal;
				}
				else if ((pRight_diag_log_blk) && (basist::bc7u::compare_block_configs(log_blk, *pRight_diag_log_blk, false)))
				{
					command_byte = (uint8_t)xbc7_command_id::cCmdReuseConfigRightDiagonal;
				}
				else
				{
					command_byte = (uint8_t)xbc7_command_id::cCmdNewConfig;

					uint32_t config_byte = log_blk.m_mode | (log_blk.m_dp_rotation_index << 3) | (log_blk.m_mode4_index_selector << 5);

					blob_writer.put_byte(cBlobBC7BlockConfig, (uint8_t)config_byte);
				}
								
				stats.m_cmd_hist[command_byte & 7]++;
				stats.m_mode_hist[log_blk.m_mode]++;


				// Code partition pattern index
				if (log_blk.m_num_partitions == 2)
					blob_writer.put_byte(cBlobPartition2, log_blk.m_pattern_index);
				else if (log_blk.m_num_partitions == 3)
					blob_writer.put_byte(cBlobPartition3, log_blk.m_pattern_index);

				// Endpoint coding: sweep every causal DPCM predictor -- the four
				// dedicated neighbor modes, then the 32 XY-delta block references --
				// and keep the one minimizing the SSE of the residual bytes that
				// will actually be emitted (int8 reading, post G-decorrelation,
				// alpha bytes that the opaque mode 6 rule suppresses excluded).
				// Dedicated modes are evaluated first and replacement is strict
				// less-than, so an XY delta aliasing a dedicated neighbor can never
				// win a tie and waste an index byte.
				const basist::bc7u::log_bc7_block* pEP_pred_blk = nullptr;
				uint32_t ep_pred_mode = (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointRaw;
				uint32_t ep_pred_delta_index = 0;
				uint32_t ep_pred_subset = 0;

				{
					uint64_t best_ep_cost = UINT64_MAX;

					auto eval_ep_predictor = [&](const basist::bc7u::log_bc7_block& pred_blk, uint32_t pred_subset) -> uint64_t
						{
							uint64_t cost = 0;

							for (uint32_t s = 0; s < log_blk.m_num_partitions; s++)
							{
								uint8_t residuals[8];
								uint32_t num_residuals;
								uint8_t residual_pbits[2];
								uint32_t num_residual_pbits;

								basist::bc7u::endpoint_dpcm(false,
									pred_blk, pred_subset, // mirrors the emission below
									log_blk, s,
									residuals, num_residuals, residual_pbits, num_residual_pbits);

								uint32_t num_costed = num_residuals;
								if ((log_blk.m_mode == 6) && (!has_alpha))
									num_costed = 6; // alpha residuals are never sent

								for (uint32_t i = 0; i < num_costed; i++)
								{
									const int v = (int8_t)residuals[i];
									cost += (uint64_t)((int64_t)v * v);
								}

								// NOTE: residual_pbits are deliberately NOT costed.
								// Penalizing pbit mismatches was tried and measured
								// a wash at best (penalty 1: ~0%, penalty 2: +0.4%
								// WORSE): the delta stream is raw 1 bit/pbit either
								// way, so the only possible win is making its packed
								// bytes Zstd-compressible -- but at 8 deltas/byte
								// even a 30% nonzero rate leaves ~7 bits/byte of
								// entropy, and the residual bytes traded away to
								// get there cost more.
							}

							return cost;
						};

					const basist::bc7u::log_bc7_block* neighbor_preds[4] = { pLeft_log_blk, pUp_log_blk, pLeft_diag_log_blk, pRight_diag_log_blk };
					const xbc7_command_endpoint_mode neighbor_modes[4] =
					{
						xbc7_command_endpoint_mode::cCmdEndpointDPCMLeft, xbc7_command_endpoint_mode::cCmdEndpointDPCMUp,
						xbc7_command_endpoint_mode::cCmdEndpointDPCMLeftDiagonal, xbc7_command_endpoint_mode::cCmdEndpointDPCMRightDiagonal
					};

					for (uint32_t i = 0; i < 4; i++)
					{
						if (!neighbor_preds[i])
							continue;

						const uint64_t cost = eval_ep_predictor(*neighbor_preds[i], 0);
						if ((!pEP_pred_blk) || ((cost * 1000) < (best_ep_cost * (1000 - (uint64_t)XBC7_SWEEP_SWITCH_MARGIN_PERMILLE))))
						{
							best_ep_cost = cost;
							pEP_pred_blk = neighbor_preds[i];
							ep_pred_mode = (uint32_t)neighbor_modes[i];
							ep_pred_subset = 0;
						}
					}

					// Subset-1 variants of left/up (EP modes 6/7), legal only when
					// the neighbor is partitioned; swept AFTER the subset-0 modes
					// so the switch margin keeps them losing ties. NOTE: a much
					// stricter dedicated margin was tried and measured WORSE --
					// these modes' SSE wins are nearly always decisive, so the
					// filter barely fires; the (small) net cost on rare content
					// with hyper-skewed command streams is an SSE-vs-bytes
					// mismatch that only rate-based EP selection would fix.
					const basist::bc7u::log_bc7_block* s1_preds[2] = { pLeft_log_blk, pUp_log_blk };
					const xbc7_command_endpoint_mode s1_modes[2] =
					{
						xbc7_command_endpoint_mode::cCmdEndpointDPCMLeftSubset1, xbc7_command_endpoint_mode::cCmdEndpointDPCMUpSubset1
					};

					for (uint32_t i = 0; i < 2; i++)
					{
						if ((!s1_preds[i]) || (s1_preds[i]->m_num_partitions < 2))
							continue;

						const uint64_t cost = eval_ep_predictor(*s1_preds[i], 1);
						if ((!pEP_pred_blk) || ((cost * 1000) < (best_ep_cost * (1000 - (uint64_t)XBC7_SWEEP_SWITCH_MARGIN_PERMILLE))))
						{
							best_ep_cost = cost;
							pEP_pred_blk = s1_preds[i];
							ep_pred_mode = (uint32_t)s1_modes[i];
							ep_pred_subset = 1;
						}
					}

					for (uint32_t i = 0; i < NUM_XY_DELTAS; i++)
					{
						const xbc7_xy_delta& delta = g_xbc7_xy_deltas[i];
						const int nx = (int)bx + delta.m_dx;
						const int ny = (int)by + delta.m_dy;

						if (!tile.contains(nx, ny))
							continue;

						const basist::bc7u::log_bc7_block& pred_blk = log_blks(nx, ny);

						const uint64_t cost = eval_ep_predictor(pred_blk, 0);
						if ((!pEP_pred_blk) || ((cost * 1000) < (best_ep_cost * (1000 - (uint64_t)XBC7_SWEEP_SWITCH_MARGIN_PERMILLE))))
						{
							best_ep_cost = cost;
							pEP_pred_blk = &pred_blk;
							ep_pred_mode = (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMBlockIndex;
							ep_pred_delta_index = i;
							ep_pred_subset = 0;
						}
					}
				}

				if (pEP_pred_blk)
				{
					command_byte |= (uint8_t)(ep_pred_mode << XBC7_COMMAND_ENDPOINT_MODE_SHIFT);
					stats.m_ep_dpcm_blocks++;
					stats.m_ep_mode_hist[ep_pred_mode]++;

					if (ep_pred_mode == (uint32_t)xbc7_command_endpoint_mode::cCmdEndpointDPCMBlockIndex)
					{
						blob_writer.put_byte(cBlobEPBlockIndex, (uint8_t)ep_pred_delta_index);
						stats.m_ep_index_hist[ep_pred_delta_index]++;
					}


					for (uint32_t s = 0; s < log_blk.m_num_partitions; s++)
					{
						uint8_t residuals[8];
						uint32_t num_residuals;
						uint8_t residual_pbits[2];
						uint32_t num_residual_pbits;

						basist::bc7u::endpoint_dpcm(false, 
							*pEP_pred_blk, ep_pred_subset, // subset 1 for EP modes 6/7
							log_blk, s,
							residuals, num_residuals, residual_pbits, num_residual_pbits);

						{
							basist::bc7u::log_bc7_block temp_log_blk(log_blk);
							memset(temp_log_blk.m_endpoints, 0, sizeof(temp_log_blk.m_endpoints));

							basist::bc7u::endpoint_dpcm(true,
								*pEP_pred_blk, ep_pred_subset, // subset 1 for EP modes 6/7
								temp_log_blk, s,
								residuals, num_residuals, residual_pbits, num_residual_pbits);

#if defined(DEBUG) || defined(_DEBUG)
							for (uint32_t e = 0; e < 2; e++)
							{
								for (uint32_t c = 0; c < 4; c++)
								{
									if (log_blk.m_endpoints[s][e][c] != temp_log_blk.m_endpoints[s][e][c])
									{
										fmt_error_printf("DPCM endpoint decomp failed");
										assert(0);
										return false;
									}
								}
							}
#endif
						}

						assert((num_residuals % 2) == 0);

						if ((log_blk.m_mode == 6) && (!has_alpha))
						{
							assert(num_residuals == 8);

							if ((log_blk.m_endpoints[0][0][3] != 127) || (log_blk.m_endpoints[0][1][3] != 127))
							{
								assert(0);
								return false;
							}

#if 0
							if ((residuals[6] != 0) || (residuals[7] != 0))
							{
								assert(0);
								return false;
							}
#endif

							num_residuals = 6;
						}

						stats.m_ep_dpcm_subsets++;
						stats.record_ep_residuals(log_blk.m_endpoint_bits[0] >= 6, residuals, num_residuals);

						if (log_blk.m_endpoint_bits[0] >= 6)
						{
							for (uint32_t i = 0; i < num_residuals; i += 2)
							{
								const uint32_t chan = i >> 1;
								blob_writer.put_byte(cBlobEPDeltaFineR + chan, residuals[i + 0]);
								blob_writer.put_byte(cBlobEPDeltaFineR + chan, residuals[i + 1]);
							}
						}
						else
						{
							for (uint32_t i = 0; i < num_residuals; i += 2)
							{
								const uint32_t chan = i >> 1;
								blob_writer.put_byte(cBlobEPDeltaCoarseR + chan, residuals[i + 0]);
								blob_writer.put_byte(cBlobEPDeltaCoarseR + chan, residuals[i + 1]);
							}
						}

						for (uint32_t p = 0; p < num_residual_pbits; p++)
						{
							pbits.put_bits(residual_pbits[p], 1);

							stats.m_pbit_delta_bits++;
							stats.m_pbit_delta_nonzero += residual_pbits[p];
						}
					}
				}
				else
				{
					command_byte |= ((uint8_t)xbc7::xbc7_command_endpoint_mode::cCmdEndpointRaw << XBC7_COMMAND_ENDPOINT_MODE_SHIFT);
					stats.m_ep_raw_blocks++;
					stats.m_ep_mode_hist[(uint32_t)xbc7_command_endpoint_mode::cCmdEndpointRaw]++;

					for (uint32_t s = 0; s < log_blk.m_num_partitions; s++)
					{
						for (uint32_t c = 0; c < log_blk.get_num_comps(); c++)
						{
							for (uint32_t e = 0; e < 2; e++)
							{
								raw_endpoint_bits.put_bits(log_blk.m_endpoints[s][e][c], log_blk.m_endpoint_bits[c == 3]);
							}
						}
					}

					for (uint32_t p = 0; p < log_blk.m_num_pbits; p++)
						raw_endpoint_bits.put_bits(log_blk.m_pbits[p], 1);
				}

				// Weights: residual DCT vs lossless residual DPCM, chosen per
				// block by estimated emitted size (see decision below). Q >= 100
				// is the lossless mode: weights are ALWAYS residual DPCM -- the
				// whole file is then exact relative to the input BC7 logical
				// blocks -- and the expensive DCT candidate search is skipped.
				//
				// We ALSO force lossless DPCM (and skip the DCT search) for any block
				// the bc7e_scalar base pack flagged as using its one-color endpoint
				// LUTs: those extreme endpoints only "land" with exact weights, so the
				// lossy weight DCT would decode them to "trap" artifacts. (LUTs are
				// already disabled at Q < 100; this also covers the always-LUT solid
				// path. At Q == 100 DPCM is forced anyway, so LUTs are harmless there.)
				const bool force_dpcm = (opts.m_dct_q >= 100) || (base_used_lut(bx, by) != 0);

				dct_syms best_syms[2];
				// value-init silences a C4701 false positive: best_cand_log_blk is only
				// read on the DCT path (!force_dpcm), where pack_weights() assigns it
				// first; the DPCM path 'continue's before the read.
				basist::bc7u::log_bc7_block best_cand_log_blk{};
				uint32_t best_predictor_index = 0;

				uint64_t dct_est_obits = 0;
				uint32_t block_total_acs = 0;

				if (!force_dpcm)
				{
					bool pack_status = pack_weights(bx, by, num_blocks_x,
						tile,
						orig_block,
						log_blks,
						best_syms, best_cand_log_blk, best_predictor_index,
						weight_grid_dct_fixed, dct_work_fixed, opts, enabled_pred_mask, num_amp_codes);

					if (!pack_status)
						return false;

					// Estimated DCT emission cost, mirroring the writes below:
					// each stream byte at its measured post-Zstd cost, plus the
					// exact (incompressible) sign bits.
					for (uint32_t p = 0; p < log_blk.m_num_planes; p++)
					{
						uint32_t num_acs = 0, num_eobs = 0;
						for (uint32_t c = 0; c < best_syms[p].m_ac_vals.size(); c++)
						{
							if (best_syms[p].m_ac_vals[c].m_coeff == INT16_MAX)
								num_eobs++;
							else
								num_acs++;
						}
						dct_est_obits += DCT_DC_BYTE_COST_OBITS + (2 * (uint64_t)DCT_AC_BYTE_COST_OBITS) * num_acs + (uint64_t)DCT_AC_BYTE_COST_OBITS * num_eobs +
							8 * ((uint64_t)num_acs + ((best_predictor_index != cCandAbsolute) ? 1 : 0));
						block_total_acs += num_acs;
					}

					stats.m_wt_dct_est_bits_total += dct_est_obits;
				}

				dpcm_weights dpcm;
				pack_weights_dpcm(bx, by, num_blocks_x, tile, log_blks, dpcm, enabled_pred_mask, num_amp_codes);

				stats.m_wt_dpcm_est_bits_total += dpcm.m_est_cost_obits;

				// Both estimates are in measured post-Zstd octobits, so 100 is
				// the neutral operating point. Ties go to DPCM: it's lossless,
				// so a tie is a free quality win.
				bool use_dpcm = force_dpcm || ((dpcm.m_est_cost_obits * 100) <= (dct_est_obits * opts.m_wt_dpcm_alpha_pct));

				if (use_dpcm)
				{
					// the weight-mode command bit is cCmdWeightRaw == 0: nothing to OR in
					stats.m_wt_dpcm_blocks++;
					stats.m_wt_block_acs_hist_dpcm[basisu::minimum<uint32_t>(block_total_acs, 32)]++;
					stats.m_wt_chosen_est_bits += dpcm.m_est_cost_obits;
					stats.m_wt_dpcm_pred_hist[dpcm.m_pred_index]++;

					blob_writer.put_byte(cBlobWeightPredictors, (uint8_t)dpcm.m_pred_index);

					const bool is_absolute = (dpcm.m_pred_index == (uint32_t)cCandAbsolute);

					for (uint32_t p = 0; p < log_blk.m_num_planes; p++)
					{
						const uint32_t num_bits = log_blk.m_weight_bits[p];
						const uint32_t blob_id = is_absolute ? (uint32_t)cBlobRawWeightBits :
							((num_bits == 2) ? (uint32_t)cBlobDPCMWeightResid2 :
							(num_bits == 3) ? (uint32_t)cBlobDPCMWeightResid3 : (uint32_t)cBlobDPCMWeightResid4);

						if (!is_absolute)
						{
							const uint32_t cls = num_bits - 2;
							const int half = 1 << (num_bits - 1);
							for (uint32_t i = 0; i < 16; i++)
							{
								const int sym = dpcm.m_syms[p][i];
								const int v = (sym >= half) ? (sym - (1 << num_bits)) : sym;
								stats.m_wt_resid_count[cls]++;
								stats.m_wt_resid_sum_abs[cls] += (uint64_t)iabs(v);
								if (!v)
									stats.m_wt_resid_zero[cls]++;
								stats.m_wt_resid_mag_hist[cls][basisu::minimum<int>(iabs(v), 8)]++;
							}
						}

						if (num_bits == 2)
						{
							// 4 per byte, LSB first
							for (uint32_t i = 0; i < 16; i += 4)
							{
								blob_writer.put_byte(blob_id, (uint8_t)(dpcm.m_syms[p][i] |
									(dpcm.m_syms[p][i + 1] << 2) | (dpcm.m_syms[p][i + 2] << 4) | (dpcm.m_syms[p][i + 3] << 6)));
							}
						}
						else
						{
							// 3-bit expanded to nibbles / 4-bit: 2 per byte, LSB first
							for (uint32_t i = 0; i < 16; i += 2)
							{
								blob_writer.put_byte(blob_id, (uint8_t)(dpcm.m_syms[p][i] | (dpcm.m_syms[p][i + 1] << 4)));
							}
						}
					} // p

					// lossless: reconstruction == input, no weight write-back needed
					if (pCoded_log_blocks)
					{
						(*pCoded_log_blocks)(bx, by) = log_blk;
					}

					if (debug_images)
						xbc7_vis_fill_command(dbg_imgs, bx, by, command_byte, true, dpcm.m_pred_index, UINT32_MAX); // lossless DPCM weights: no DCT ACs

					blob_writer.put_byte(cBlobCommands, command_byte);

					continue;
				}

				command_byte |= ((uint8_t)xbc7::xbc7_command_weight_mode::cCmdWeightDCT << XBC7_COMMAND_WEIGHT_MODE_SHIFT);

				// optional AC-truncation RDO: prune high-freq weight-DCT ACs from the
				// winning symbols (once per block), within the configured PSNR drop
				if (opts.m_ac_trunc_rdo_max_psnr_drop > 0.0f)
				{
					if (!ac_truncate_rdo(best_syms, best_cand_log_blk, best_predictor_index, orig_block,
						bx, by, num_blocks_x, tile, log_blks, weight_grid_dct_fixed, dct_work_fixed, opts, stats))
						return false;

					// refresh the AC count for the histogram below (pruning changed it)
					block_total_acs = 0;
					for (uint32_t p = 0; p < log_blk.m_num_planes; p++)
					{
						uint32_t n = best_syms[p].m_ac_vals.size_u32();
						if (n && (best_syms[p].m_ac_vals[n - 1].m_coeff == INT16_MAX))
							n--;
						block_total_acs += n;
					}
				}

				stats.m_wt_dct_blocks++;
				stats.m_wt_block_acs_hist_dct[basisu::minimum<uint32_t>(block_total_acs, 32)]++;
				stats.m_wt_chosen_est_bits += dct_est_obits;

				blob_writer.put_byte(cBlobWeightPredictors, (uint8_t)best_predictor_index);
				stats.m_pred_hist[best_predictor_index]++;

				for (uint32_t p = 0; p < log_blk.m_num_planes; p++)
				{
					stats.m_dct_stats.record_plane(best_syms[p]);

					assert(iabs(best_syms[p].m_dc) <= 255);

					// TODO: Always writing full precision DC
					blob_writer.put_byte(cBlobDCCoeffsSmall, (uint8_t)iabs(best_syms[p].m_dc));

					if (best_predictor_index == cCandAbsolute)
					{
						assert(best_syms[p].m_dc >= 0);
					}
					else
					{
						coeff_signs.put_bits((best_syms[p].m_dc < 0) ? 1 : 0, 1);
					}

					assert(best_syms[p].m_ac_vals.size());

					for (uint32_t c = 0; c < best_syms[p].m_ac_vals.size(); c++)
					{
						uint32_t num_zeros = best_syms[p].m_ac_vals[c].m_num_zeros;
						
						int ac_coeff = best_syms[p].m_ac_vals[c].m_coeff;
												
						if (ac_coeff == INT16_MAX)
						{
							blob_writer.put_byte(cBlobACCoeffs, 0xFF);
						}
						else
						{
							assert(iabs(ac_coeff) <= 255);

							blob_writer.put_byte(cBlobACCoeffs, (uint8_t)num_zeros);
							blob_writer.put_byte(cBlobACCoeffs, (uint8_t)iabs(ac_coeff));
							coeff_signs.put_bits((ac_coeff < 0) ? 1 : 0, 1);
						}
					} // c
				} // p

				log_blk = best_cand_log_blk;

				if (pCoded_log_blocks)
				{
					(*pCoded_log_blocks)(bx, by) = best_cand_log_blk;
				}

				if (debug_images)
					xbc7_vis_fill_command(dbg_imgs, bx, by, command_byte, true, best_predictor_index, block_total_acs); // DCT weights

				blob_writer.put_byte(cBlobCommands, command_byte);

			} // bx

		} // by

		return true;
	}

	// Packs a single 4x4 RGBA block into a physical BC7 block using whichever BC7
	// base encoder is selected in opts: cBC7F is the built-in fast real-time
	// packer; cBC7E_Scalar is the higher-quality scalar bc7e encoder (whose
	// per-level params must be prebuilt once into bc7e_params before threading).
	// Either way it returns a standard physical BC7 block, identical in form to
	// what bc7f produces, so the rest of the pipeline is unaffected.
	static inline void xbc7_pack_bc7_base_block(
		basist::bc7u::phys_bc7_block& phys_blk,
		const color_rgba* pPixels,
		const pack_options& opts,
		const bc7e_scalar::bc7e_compress_block_params& bc7e_params,
		bool& used_lut)
	{
		used_lut = false;
		if (opts.m_bc7_encoder == bc7_encoder_type::cBC7E_Scalar)
		{
			// bc7e writes via a uint64_t*; phys_bc7_block::m_bytes has no alignment
			// guarantee, so encode into an aligned local and copy the 16 bytes back.
			// pUsed_lut reports whether the winning encoding used bc7e's one-color
			// endpoint LUTs on any subset (a "this block may be weird" hint).
			uint64_t blk64[2];
			uint8_t lut_flag = 0;
			bc7e_scalar::bc7e_compress_blocks(1, blk64, (const uint32_t*)pPixels, &bc7e_params, &lut_flag);
			memcpy(phys_blk.m_bytes, blk64, sizeof(phys_blk.m_bytes));
			used_lut = (lut_flag != 0);
		}
		else
		{
			// bc7f does not use the one-color endpoint LUTs.
			basist::bc7f::fast_pack_bc7_auto_rgba(phys_blk.m_bytes, (basist::color_rgba*)pPixels, opts.m_bc7_pack_flags);
		}
	}

	bool pack_image(
		const image& orig_img,
		const pack_options& opts,
		uint8_vec &comp_bytes,
		vector2D<basist::bc7u::log_bc7_block> &coded_log_blocks)
	{
#if !BASISD_SUPPORT_KTX2_ZSTD
		// XBC7 is entirely zstd-based: the stream is zstd-compressed (serialize) and
		// decoding/validation requires zstd too. We do not support uncompressed XBC7
		// streams, so without BASISD_SUPPORT_KTX2_ZSTD there is nothing we can do --
		// fail the encode cleanly. (The zstd-using body below isn't compiled here, so
		// the encoder still links: serialize() ends up unreferenced.)
		BASISU_NOTE_UNUSED(orig_img); BASISU_NOTE_UNUSED(opts);
		BASISU_NOTE_UNUSED(comp_bytes); BASISU_NOTE_UNUSED(coded_log_blocks);
		fmt_error_printf("XBC7 pack_image: XBC7 requires zstd (BASISD_SUPPORT_KTX2_ZSTD=0) -- cannot encode\n");
		return false;
#else
		// Internal pointer alias for the caller-provided reference (guaranteed
		// non-null): pack_stripe() and the threaded lambdas below thread the coded
		// logical blocks through as a pointer. The encoder always populates these
		// and then validates the encoded stream decodes back to them exactly.
		vector2D<basist::bc7u::log_bc7_block>* pCoded_log_blocks = &coded_log_blocks;

		const uint32_t width = orig_img.get_width();
		const uint32_t height = orig_img.get_height();

		if ((!width) || (width > UINT16_MAX) || (!height) || (height > UINT16_MAX))
		{
			assert(0);
			return false;
		}

		if ((opts.m_dct_q < 1) || (opts.m_dct_q > 100))
		{
			assert(0);
			return false;
		}

		if (!opts.m_pJob_pool)
		{
			assert(0);
			return false;
		}

		const bool use_threading = (opts.m_pJob_pool->get_total_threads() > 1);

		const uint32_t block_width = 4;
		const uint32_t block_height = 4;

		const uint32_t num_blocks_x = (width + block_width - 1) / block_width;
		const uint32_t num_blocks_y = (height + block_height - 1) / block_height;
		const uint32_t total_blocks = num_blocks_x * num_blocks_y;

		const bool has_alpha = orig_img.has_alpha();

		if (opts.m_debug_output)
			fmt_debug_printf("XBC7 pack: {}x{} texels, alpha: {}, block: {}x{}, blocks: {}x{} = {} total\n",
				width, height, has_alpha, block_width, block_height, num_blocks_x, num_blocks_y, total_blocks);

		// Dump the full set of pack_options that were passed in.
		if (opts.m_debug_output)
		{
			fmt_debug_printf("---- XBC7 pack_options ----\n");
			fmt_debug_printf("  dct_q: {}\n", opts.m_dct_q);
			{
				const uint64_t dbg_mask = xbc7_build_pred_mask(opts.m_effort_level);
				uint32_t dbg_num_preds = 0;
				for (uint32_t c = 0; c < cTotalCandidates; c++)
					dbg_num_preds += (uint32_t)((dbg_mask >> c) & 1u);
				fmt_debug_printf("  effort_level: {} (weight predictors: {}/{}, amp codes: {}/4)\n",
					opts.m_effort_level, dbg_num_preds, (uint32_t)cTotalCandidates, xbc7_amp_codes_for_effort(opts.m_effort_level));
			}
			fmt_debug_printf("  weights[RGBA]: {} {} {} {}\n", opts.m_weights[0], opts.m_weights[1], opts.m_weights[2], opts.m_weights[3]);
			fmt_debug_printf("  optimize_weights_after_bc7f: {}\n", opts.m_optimize_weights_after_bc7f);
			fmt_debug_printf("  bc7_encoder: {} (bc7e_scalar level: {}, perceptual: {})\n",
				(opts.m_bc7_encoder == bc7_encoder_type::cBC7E_Scalar) ? "bc7e_scalar" : "bc7f", opts.m_bc7e_scalar_level, opts.m_perceptual);
			fmt_debug_printf("  self_validate: {}\n", opts.m_self_validate);
			fmt_debug_printf("  bc7_pack_flags: 0x{X}\n", opts.m_bc7_pack_flags);
			fmt_debug_printf("  bc7_alt_pack_enabled: {}, bc7_pack_flags_alt: 0x{X}, bc7_alt_max_psnr_drop: {.3}\n",
				opts.m_bc7_alt_pack_enabled, opts.m_bc7_pack_flags_alt, opts.m_bc7_alt_max_psnr_drop);
			fmt_debug_printf("  repeat_rdo_enabled: {}, repeat_rdo_max_psnr_drop: {.3}\n", opts.m_repeat_rdo_enabled, opts.m_repeat_rdo_max_psnr_drop);
			fmt_debug_printf("  solid_rdo_enabled: {}, solid_rdo_max_psnr_drop: {.3}\n", opts.m_solid_rdo_enabled, opts.m_solid_rdo_max_psnr_drop);
			fmt_debug_printf("  endpoint_rdo_enabled: {}, endpoint_rdo_max_psnr_drop: {.3}\n", opts.m_endpoint_rdo_enabled, opts.m_endpoint_rdo_max_psnr_drop);
			fmt_debug_printf("  ac_trunc_rdo_max_psnr_drop: {.3}\n", opts.m_ac_trunc_rdo_max_psnr_drop);
			fmt_debug_printf("  rdo_min_block_psnr: {.3}\n", opts.m_rdo_min_block_psnr);
			fmt_debug_printf("  wt_dpcm_alpha_pct: {}\n", opts.m_wt_dpcm_alpha_pct);
			fmt_debug_printf("  num_stripes (0=auto): {}\n", opts.m_num_stripes);
			fmt_debug_printf("  job_pool total threads: {}\n", opts.m_pJob_pool ? (uint32_t)opts.m_pJob_pool->get_total_threads() : 0);
			fmt_debug_printf("  print_stats: {}, debug_images: {}, debug_file_prefix: '{}'\n",
				opts.m_print_stats, opts.m_debug_images, opts.m_debug_file_prefix.c_str());
		}

		vector2D<basist::bc7u::log_bc7_block> log_blks(num_blocks_x, num_blocks_y);

		// Per-block flag: 1 if the bc7e_scalar base pack used its one-color endpoint
		// LUTs on the block (pack_stripe forces lossless DPCM for these). Function
		// scope so it survives from the base pack into the stripe coding pass.
		vector2D<uint8_t> base_used_lut(num_blocks_x, num_blocks_y);

		if (opts.m_debug_output)
			fmt_debug_printf("XBC7 progress: BC7 base pack ({} blocks{})...\n", total_blocks, opts.m_bc7_alt_pack_enabled ? ", alt-pack RDO" : "");
				
		// BC7 base pack + canonicalize. Every block is independent and writes
		// only its own pre-sized log_blks slot, so this pass parallelizes with
		// simple atomic row stealing; all per-block state lives on each job's
		// stack. The packer/unpack/canonicalize helpers are pure functions of
		// their inputs (shared state is const tables initialized at startup).
		{
			// Prebuild the bc7e_scalar params once (shared, read-only across the
			// pack jobs); only used when that encoder is selected. The level [0,6]
			// picks one of its speed/quality presets (0=ultrafast .. 6=slowest).
			// A one-time global table init is required before the first encode.
			bc7e_scalar::bc7e_compress_block_params bc7e_params;
			memset(&bc7e_params, 0, sizeof(bc7e_params));
			if (opts.m_bc7_encoder == bc7_encoder_type::cBC7E_Scalar)
			{
				bc7e_scalar::bc7e_compress_block_init();

				typedef void (*bc7e_params_init_func)(bc7e_scalar::bc7e_compress_block_params*, bool);
				static const bc7e_params_init_func s_bc7e_level_init[BC7E_SCALAR_MAX_LEVEL + 1] =
				{
					&bc7e_scalar::bc7e_compress_block_params_init_ultrafast, // 0
					&bc7e_scalar::bc7e_compress_block_params_init_veryfast,  // 1
					&bc7e_scalar::bc7e_compress_block_params_init_fast,      // 2
					&bc7e_scalar::bc7e_compress_block_params_init_basic,     // 3
					&bc7e_scalar::bc7e_compress_block_params_init_slow,      // 4
					&bc7e_scalar::bc7e_compress_block_params_init_veryslow,  // 5
					&bc7e_scalar::bc7e_compress_block_params_init_slowest,   // 6
				};

				const uint32_t lvl = clamp<uint32_t>(opts.m_bc7e_scalar_level, BC7E_SCALAR_MIN_LEVEL, BC7E_SCALAR_MAX_LEVEL);

				// For perceptual (sRGB) sources, run bc7e in its built-in perceptual
				// error mode and leave its channel weights at the preset defaults. For
				// linear sources, run bc7e in linear mode and honor the explicit XBC7
				// channel weights. (bc7f supports neither, so this only affects bc7e.)
				s_bc7e_level_init[lvl](&bc7e_params, opts.m_perceptual);

				if (!opts.m_perceptual)
				{
					for (uint32_t i = 0; i < 4; i++)
						bc7e_params.m_weights[i] = opts.m_weights[i];
				}

				// bc7e's aggressive one-color endpoint LUTs place endpoints at extreme
				// positions that only "land" with exact weights. Only allow them at
				// lossless Q (m_dct_q == 100, weights coded losslessly); at lower Q the
				// lossy weight DCT can't reproduce them and the block decodes to a
				// "trap" artifact, so disable the LUTs there.
				bc7e_params.m_use_luts = (opts.m_dct_q >= 100);
			}

			image raw_bc7_debug_image;
			if (opts.m_debug_images || (opts.m_debug_output && opts.m_print_stats))
			{
				raw_bc7_debug_image.resize(orig_img.get_width(), orig_img.get_height());
			}

			std::atomic<uint32_t> cur_row;
			cur_row.store(0);

			std::atomic<bool> pack_failed_flag;
			pack_failed_flag.store(false);

			std::atomic<uint32_t> alt_changed_blocks; // BC7 alt-pack RDO: blocks where the alternate was kept
			alt_changed_blocks.store(0);

			std::atomic<uint32_t> lut_block_count; // bc7e_scalar: blocks whose winning encoding used the endpoint LUTs
			lut_block_count.store(0);

			std::atomic<uint32_t> lut_block_count_nonsolid; // same, but excluding solid blocks (all 16 source pixels equal)
			lut_block_count_nonsolid.store(0);

			auto pack_rows_func = [num_blocks_x, num_blocks_y, &opts,
				&cur_row, &pack_failed_flag, &alt_changed_blocks, &lut_block_count, &lut_block_count_nonsolid,
				&orig_img, &log_blks, &raw_bc7_debug_image, &bc7e_params, &base_used_lut]()
				{
					for ( ; ; )
					{
						if (pack_failed_flag)
							return;

						const uint32_t by = cur_row.fetch_add(1);
						if (by >= num_blocks_y)
							break;

						for (uint32_t bx = 0; bx < num_blocks_x; bx++)
						{
							color_rgba orig_block[16];
							orig_img.extract_block_clamped(orig_block, bx * 4, by * 4, 4, 4);

							basist::bc7u::phys_bc7_block phys_blk;
							bool used_lut = false;
							xbc7_pack_bc7_base_block(phys_blk, orig_block, opts, bc7e_params, used_lut);
							base_used_lut(bx, by) = used_lut ? (uint8_t)1 : (uint8_t)0;
							if (used_lut)
							{
								lut_block_count.fetch_add(1, std::memory_order_relaxed);

								// A block is "solid" if all 16 source pixels are equal.
								bool is_solid = true;
								for (uint32_t i = 1; i < 16; i++)
								{
									if ((orig_block[i].r != orig_block[0].r) || (orig_block[i].g != orig_block[0].g) ||
										(orig_block[i].b != orig_block[0].b) || (orig_block[i].a != orig_block[0].a))
									{
										is_solid = false;
										break;
									}
								}
								if (!is_solid)
									lut_block_count_nonsolid.fetch_add(1, std::memory_order_relaxed);
							}

							if (raw_bc7_debug_image.get_width())
							{
								basist::color_rgba unpacked_block[16];
								bool as = basist::bc7u::unpack_bc7(&phys_blk, unpacked_block);
								assert(as);
								BASISU_NOTE_UNUSED(as);

								raw_bc7_debug_image.set_block_clipped((color_rgba *)unpacked_block, bx * 4, by * 4, 4, 4);
							}

							// Optional poor-man's RDO: also pack with the alternate
							// (typically cheaper) flags and keep IT unless the primary's
							// RGBA PSNR is at least the threshold dB higher.
							if (opts.m_bc7_alt_pack_enabled)
							{
								basist::bc7u::phys_bc7_block phys_alt;
								basist::bc7f::fast_pack_bc7_auto_rgba(phys_alt.m_bytes, (basist::color_rgba*)orig_block, opts.m_bc7_pack_flags_alt);

								basist::color_rgba dec_primary[16], dec_alt[16];
								if ((!basist::bc7u::unpack_bc7(phys_blk.m_bytes, dec_primary)) ||
									(!basist::bc7u::unpack_bc7(phys_alt.m_bytes, dec_alt)))
								{
									assert(0);
									pack_failed_flag.store(true);
									return;
								}

								const float psnr_primary = xbc7_block_wsse_psnr(orig_block, (const color_rgba*)dec_primary, opts.m_weights);
								const float psnr_alt = xbc7_block_wsse_psnr(orig_block, (const color_rgba*)dec_alt, opts.m_weights);

								if ((psnr_alt >= opts.m_rdo_min_block_psnr) && ((psnr_primary - psnr_alt) < opts.m_bc7_alt_max_psnr_drop))
								{
									phys_blk = phys_alt; // above the quality floor and drop within tolerance -- keep the cheaper block
									alt_changed_blocks.fetch_add(1, std::memory_order_relaxed);
								}
							}

							basist::bc7u::log_bc7_block& log_blk = log_blks(bx, by);

							bool unpack_status = basist::bc7u::unpack_bc7(&phys_blk, log_blk);
							if (!unpack_status)
							{
								assert(0);
								pack_failed_flag.store(true);
								return;
							}

#if defined(DEBUG) || defined(_DEBUG)
							{
								// sanity check canonicalization code
								basist::bc7u::log_bc7_block temp_log_blk(log_blk);
								basist::bc7u::canonicalize_endpoints(temp_log_blk);

								color_rgba ap[16], bp[16];
								bool as = basist::bc7u::unpack_bc7(log_blk, (basist::color_rgba*)ap);
								assert(as);

								bool bs = basist::bc7u::unpack_bc7(temp_log_blk, (basist::color_rgba*)bp);
								assert(bs);

								if (memcmp(ap, bp, sizeof(color_rgba) * 16) != 0)
								{
									assert(0);
									pack_failed_flag.store(true);
									return;
								}
							}
#endif
							basist::bc7u::canonicalize_endpoints(log_blk);

							// optional: re-optimize the weights for bc7f's endpoints.
							// bc7f is a fast real-time packer, so its weights aren't
							// per-texel optimal -- this exhaustively re-derives them
							// (slower, but threaded with the base pack). Endpoints/config
							// unchanged, so it can only hold or improve quality.
							if (opts.m_optimize_weights_after_bc7f)
							{
								basist::bc7u::log_bc7_block opt_blk;
								if (!optimize_block_weights(log_blk, orig_block, opts.m_weights, opt_blk))
								{
									assert(0);
									pack_failed_flag.store(true);
									return;
								}
								log_blk = opt_blk;
							}

						} // bx
					}
				};

			if ((use_threading) && (num_blocks_y > 1))
			{
				const uint32_t num_threads = (uint32_t)opts.m_pJob_pool->get_total_threads();

				// one job per worker; each drains rows via the atomic counter
				for (uint32_t job_index = 0; job_index < num_threads; job_index++)
					opts.m_pJob_pool->add_job(pack_rows_func);

				opts.m_pJob_pool->wait_for_all();
			}
			else
			{
				pack_rows_func();
			}

			if (pack_failed_flag)
				return false;

			if (opts.m_debug_output && opts.m_bc7_alt_pack_enabled)
			{
				const uint32_t n = alt_changed_blocks.load();
				fmt_debug_printf("BC7 alt-pack RDO: {} blocks changed ({.2}%)\n", n, total_blocks ? (n * 100.0f / (float)total_blocks) : 0.0f);
			}

			if (opts.m_debug_output && (opts.m_bc7_encoder == bc7_encoder_type::cBC7E_Scalar))
			{
				const uint32_t n = lut_block_count.load();
				const uint32_t n_nonsolid = lut_block_count_nonsolid.load();
				fmt_debug_printf("bc7e_scalar: {} of {} blocks used endpoint LUTs ({.2}%); {} excluding solid blocks ({.2}%) [m_use_luts={}]\n",
					n, total_blocks, total_blocks ? (n * 100.0f / (float)total_blocks) : 0.0f,
					n_nonsolid, total_blocks ? (n_nonsolid * 100.0f / (float)total_blocks) : 0.0f, bc7e_params.m_use_luts);
			}

			if (raw_bc7_debug_image.get_width())
			{
				if (opts.m_debug_images)
					save_png(opts.m_debug_file_prefix + "vis_raw_bc7.png", raw_bc7_debug_image);

				if ((opts.m_debug_output) && (opts.m_print_stats))
				{
					fmt_debug_printf("\nRaw packed BC7 image stats:\n");
					print_image_metrics(orig_img, raw_bc7_debug_image);
				}
			}
		}

		// Stripe geometry for the main coding pass. The count is a pure function
		// of m_num_stripes (or the image dimensions when it's 0), NOT of the pool
		// size, so the output is reproducible regardless of thread count. Pass
		// m_num_stripes = 1 for the single-stripe (no seam cost, no seek table)
		// format. More stripes than pool threads simply queue.
		basisu::vector<stripe_range> stripes;
		uint32_t num_stripes;

		if (opts.m_num_stripes)
		{
			// caller forced a specific count -- clamp to what the format and the
			// image can hold (can't exceed the block-row count or the max)
			num_stripes = basisu::clamp<uint32_t>(opts.m_num_stripes, 1, basisu::minimum(num_blocks_y, XBC7_MAX_ENCODER_STRIPES));
			compute_stripe_ranges(num_blocks_y, num_stripes, stripes);
		}
		else
		{
			num_stripes = compute_encoder_stripes(height, num_blocks_y, stripes);
		}

		// optional block-reuse RDO: replace whole blocks with a Repeat of a neighbor
		// or a solid color where quality allows -- runs FIRST (cheapest commands)
		if (opts.m_repeat_rdo_enabled || opts.m_solid_rdo_enabled)
		{
			if (opts.m_debug_output)
				fmt_debug_printf("XBC7 progress: block-reuse RDO pass (repeat/solid)...\n");

			if (!block_reuse_rdo_pass(orig_img, log_blks, num_blocks_x, stripes, opts))
				return false;
		}

		// optional endpoint-prediction RDO: rewrite block endpoints toward their
		// causal neighbors (zero-residual DPCM) + re-optimize weights where quality
		// allows, BEFORE coding
		if (opts.m_endpoint_rdo_enabled)
		{
			if (opts.m_debug_output)
				fmt_debug_printf("XBC7 progress: endpoint-DPCM RDO pass...\n");

			if (!endpoint_dpcm_rdo_pass(orig_img, log_blks, num_blocks_x, stripes, opts))
				return false;
		}

		if (pCoded_log_blocks)
		{
			pCoded_log_blocks->resize(num_blocks_x, num_blocks_y);
			for (uint32_t y = 0; y < num_blocks_y; y++)
			{
				for (uint32_t x = 0; x < num_blocks_x; x++)
				{
					(*pCoded_log_blocks)(x, y).clear();
					(*pCoded_log_blocks)(x, y).m_mode = -1;
				}
			}
		}

		blob_stream_writer blob_writer;

		basisu::bitwise_coder coeff_signs;
		coeff_signs.reserve(1024);

		basisu::bitwise_coder pbits;
		pbits.reserve(1024);

		basisu::bitwise_coder raw_endpoint_bits;
		raw_endpoint_bits.reserve(1024);

		xbc7_pack_stats stats;
		stats.m_width = width;
		stats.m_height = height;
		stats.m_total_blocks = total_blocks;
		stats.m_dct_q = opts.m_dct_q;
		stats.m_has_alpha = has_alpha;
		stats.m_wt_alpha_pct = opts.m_wt_dpcm_alpha_pct;
		stats.m_stripes = stripes;

		// ---- main coding pass: one job per stripe ----
		// Each stripe codes into its own stripe_output (disjoint vector
		// elements, pre-sized here, so the jobs never touch shared mutable
		// state outside their own log_blks/pCoded rows).
		basisu::vector<stripe_output> stripe_outputs(num_stripes);

		// optional per-block debug visualizations (one 4x4 pixel block per BC7
		// block), each with a drawn legend strip below the block grid. Shared
		// across stripe jobs, but each stripe fills disjoint rows.
		image vis_command_img, vis_ep_mode_img, vis_wt_mode_img, vis_ac_count_img, vis_predictor_img;
		xbc7_debug_image_set dbg_imgs;
		if (opts.m_debug_images)
		{
			// same resolution as the source image; the legend is drawn into the
			// bottom strip afterward (overwriting a few block rows -- all clipped)
			vis_command_img.resize(width, height);
			vis_ep_mode_img.resize(width, height);
			vis_wt_mode_img.resize(width, height);
			vis_ac_count_img.resize(width, height);
			vis_predictor_img.resize(width, height);
			dbg_imgs.m_pAC_count = &vis_ac_count_img;
			dbg_imgs.m_pPredictor = &vis_predictor_img;
			dbg_imgs.m_pCommand = &vis_command_img;
			dbg_imgs.m_pEndpoint_mode = &vis_ep_mode_img;
			dbg_imgs.m_pWeight_mode = &vis_wt_mode_img;
		}

		std::atomic<bool> stripe_failed_flag;
		stripe_failed_flag.store(false);

		if (opts.m_debug_output)
			fmt_debug_printf("XBC7 progress: main coding pass ({} stripe{}, {})...\n",
				num_stripes, (num_stripes == 1) ? "" : "s", ((use_threading) && (num_stripes > 1)) ? "threaded" : "serial");

		if ((use_threading) && (num_stripes > 1))
		{
			for (uint32_t stripe_index = 0; stripe_index < num_stripes; stripe_index++)
			{
				opts.m_pJob_pool->add_job([stripe_index, has_alpha, num_blocks_x,
					&stripes, &stripe_outputs, &stripe_failed_flag,
					&orig_img, &opts, &log_blks, pCoded_log_blocks, &base_used_lut, dbg_imgs]
					{
						if (!pack_stripe(orig_img, opts, has_alpha, num_blocks_x,
							stripes[stripe_index], log_blks, pCoded_log_blocks, base_used_lut, stripe_outputs[stripe_index], dbg_imgs))
						{
							stripe_failed_flag.store(true);
						}
					});
			}

			opts.m_pJob_pool->wait_for_all();
		}
		else
		{
			for (uint32_t stripe_index = 0; stripe_index < num_stripes; stripe_index++)
			{
				if (!pack_stripe(orig_img, opts, has_alpha, num_blocks_x,
					stripes[stripe_index], log_blks, pCoded_log_blocks, base_used_lut, stripe_outputs[stripe_index], dbg_imgs))
				{
					stripe_failed_flag.store(true);
					break;
				}
			}
		}

		if (stripe_failed_flag)
			return false;

		if (opts.m_debug_images)
		{
			const xbc7_vis_legend_entry cmd_legend[8] =
			{
				{ g_xbc7_command_vis_colors[0], "0 RepeatLast" },
				{ g_xbc7_command_vis_colors[1], "1 RepeatUpper" },
				{ g_xbc7_command_vis_colors[2], "2 SolidDPCM" },
				{ g_xbc7_command_vis_colors[3], "3 NewConfig" },
				{ g_xbc7_command_vis_colors[4], "4 ReuseConfig Left" },
				{ g_xbc7_command_vis_colors[5], "5 ReuseConfig Up" },
				{ g_xbc7_command_vis_colors[6], "6 ReuseConfig LeftDiag" },
				{ g_xbc7_command_vis_colors[7], "7 ReuseConfig RightDiag" },
			};

			const xbc7_vis_legend_entry ep_legend[9] =
			{
				{ g_xbc7_endpoint_mode_vis_colors[0], "0 Raw" },
				{ g_xbc7_endpoint_mode_vis_colors[1], "1 DPCM Left" },
				{ g_xbc7_endpoint_mode_vis_colors[2], "2 DPCM Up" },
				{ g_xbc7_endpoint_mode_vis_colors[3], "3 DPCM LeftDiag" },
				{ g_xbc7_endpoint_mode_vis_colors[4], "4 DPCM RightDiag" },
				{ g_xbc7_endpoint_mode_vis_colors[5], "5 DPCM BlockIndex" },
				{ g_xbc7_endpoint_mode_vis_colors[6], "6 DPCM Left Subset1" },
				{ g_xbc7_endpoint_mode_vis_colors[7], "7 DPCM Up Subset1" },
				{ g_xbc7_vis_na_color,                "n/a (fully predicted block)" },
			};

			const xbc7_vis_legend_entry wt_legend[3] =
			{
				{ g_xbc7_weight_mode_vis_colors[0], "0 Raw/DPCM weights (lossless)" },
				{ g_xbc7_weight_mode_vis_colors[1], "1 DCT weights (lossy)" },
				{ g_xbc7_vis_na_color,              "n/a (fully predicted block)" },
			};

			const xbc7_vis_legend_entry ac_legend[7] =
			{
				{ g_xbc7_ac_count_vis_colors[0], "0 ACs (flat / DC only)" },
				{ g_xbc7_ac_count_vis_colors[1], "1-2 ACs" },
				{ g_xbc7_ac_count_vis_colors[2], "3-5 ACs" },
				{ g_xbc7_ac_count_vis_colors[3], "6-10 ACs" },
				{ g_xbc7_ac_count_vis_colors[4], "11-20 ACs" },
				{ g_xbc7_ac_count_vis_colors[5], "21+ ACs" },
				{ g_xbc7_vis_na_color,           "n/a (not DCT-coded)" },
			};

			const xbc7_vis_legend_entry pred_legend[7] =
			{
				{ g_xbc7_predictor_vis_colors[cPredCatAbsolute],   "Absolute (no prediction)" },
				{ g_xbc7_predictor_vis_colors[cPredCatSynthetic],  "Synthetic edge/gradient" },
				{ g_xbc7_predictor_vis_colors[cPredCatRefLeft],    "BlockRef Left" },
				{ g_xbc7_predictor_vis_colors[cPredCatRefUp],      "BlockRef Up" },
				{ g_xbc7_predictor_vis_colors[cPredCatRefUpLeft],  "BlockRef Up-Left" },
				{ g_xbc7_predictor_vis_colors[cPredCatRefUpRight], "BlockRef Up-Right" },
				{ g_xbc7_vis_na_color,                             "n/a (fully predicted block)" },
			};

			xbc7_vis_draw_legend(vis_command_img, cmd_legend, 8);
			xbc7_vis_draw_legend(vis_ep_mode_img, ep_legend, 9);
			xbc7_vis_draw_legend(vis_wt_mode_img, wt_legend, 3);
			xbc7_vis_draw_legend(vis_ac_count_img, ac_legend, 7);
			xbc7_vis_draw_legend(vis_predictor_img, pred_legend, 7);

			save_png(opts.m_debug_file_prefix + "vis_xbc7_command.png", vis_command_img);
			save_png(opts.m_debug_file_prefix + "vis_xbc7_endpoint_mode.png", vis_ep_mode_img);
			save_png(opts.m_debug_file_prefix + "vis_xbc7_weight_mode.png", vis_wt_mode_img);
			save_png(opts.m_debug_file_prefix + "vis_xbc7_weight_ac_count.png", vis_ac_count_img);
			save_png(opts.m_debug_file_prefix + "vis_xbc7_weight_predictor.png", vis_predictor_img);
		}

		if (opts.m_debug_output)
			fmt_debug_printf("XBC7 progress: merging stripe streams{}...\n", (num_stripes > 1) ? " + building seek table" : "");

		// Per-stripe seek table: for each stripe, the start offset of its data
		// in every per-stripe stream id 1..25 -- a BYTE offset for byte blobs,
		// a BIT offset for the three bit blobs (signs/pbits/ep_raw, which stay
		// bit-merged with no padding). Recorded BEFORE each stripe is appended.
		// Built only when there's more than one stripe.
		const uint32_t SEEK_NUM_STREAMS = (uint32_t)cBlobStripeSeekTable - 1; // ids 1..25
		basisu::vector<basisu::packed_uint<4>> seek_table; // [stripe * SEEK_NUM_STREAMS + (id-1)], little-endian DELTAS from prev stripe
		if (num_stripes > 1)
			seek_table.resize(num_stripes * SEEK_NUM_STREAMS);

		// running start offset of the PREVIOUS stripe in each stream (indexed by
		// blob id), so each table entry can be stored as a small delta
		uint64_t prev_seek_ofs[(uint32_t)cBlobStripeSeekTable] = {};

		// ---- merge the per-stripe streams, strictly in stripe (raster) order ----
		for (uint32_t stripe_index = 0; stripe_index < num_stripes; stripe_index++)
		{
			const stripe_output& so = stripe_outputs[stripe_index];

			if (num_stripes > 1)
			{
				for (uint32_t id = 1; id < (uint32_t)cBlobStripeSeekTable; id++)
				{
					uint64_t ofs;
					if (id == cBlobCoeffSigns)
						ofs = coeff_signs.get_total_bits();		// bit offset
					else if (id == cBlobPBits)
						ofs = pbits.get_total_bits();			// bit offset
					else if (id == cBlobEPRaw)
						ofs = raw_endpoint_bits.get_total_bits();	// bit offset
					else
						ofs = blob_writer.get_blob_size(id);	// byte offset

					// store the delta from the previous stripe's start (stripe 0 -> 0).
					// offsets are monotonic, so the delta is non-negative and small;
					// packed_uint<4> keeps the low 32 bits, little-endian.
					seek_table[stripe_index * SEEK_NUM_STREAMS + (id - 1)] = ofs - prev_seek_ofs[id];
					prev_seek_ofs[id] = ofs;
				}
			}

			for (uint32_t id = 0; id < BLOB_STREAM_MAX_IDS; id++)
			{
				const uint8_vec* pBlob = so.m_blob_writer.get_blob_data(id);
				if ((pBlob) && (pBlob->size()))
					blob_writer.append_bytes(id, *pBlob);
			}

			// bit streams: append() carries each stripe's unflushed partial bit
			// buffer, so the merged streams contain NO padding at the seams --
			// the decoder still reads single continuous LSB-first streams, and
			// the seek table addresses stripe starts at BIT granularity
			coeff_signs.append(so.m_coeff_signs);
			pbits.append(so.m_pbits);
			raw_endpoint_bits.append(so.m_raw_endpoint_bits);

			stats.merge(so.m_stats);
		}

		if (raw_endpoint_bits.get_total_bits())
		{
			raw_endpoint_bits.flush();
			blob_writer.get_blob_vec(cBlobEPRaw) = raw_endpoint_bits.get_bytes();
		}

		if (pbits.get_total_bits())
		{
			pbits.flush();
			blob_writer.get_blob_vec(cBlobPBits) = pbits.get_bytes();
		}

		if (coeff_signs.get_total_bits())
		{
			coeff_signs.flush();
			blob_writer.get_blob_vec(cBlobCoeffSigns) = coeff_signs.get_bytes();
		}

		// emit the seek table, byte-plane (SoA) transposed: all entries' byte 0
		// first, then every byte 1, then byte 2, then byte 3. The deltas are
		// small, so the upper planes are near-all-zero and Zstd crushes them --
		// much better than the interleaved little-endian layout. Lossless
		// reordering; the decoder reverses it before reading the deltas.
		if (num_stripes > 1)
		{
			const uint32_t num_entries = (uint32_t)seek_table.size(); // num_stripes * SEEK_NUM_STREAMS
			const uint8_t* pSrc = (const uint8_t*)seek_table.data();  // little-endian, 4 bytes/entry
			uint8_vec planed(num_entries * 4);
			for (uint32_t plane = 0; plane < 4; plane++)
				for (uint32_t e = 0; e < num_entries; e++)
					planed[plane * num_entries + e] = pSrc[e * 4 + plane];
			blob_writer.put_bytes(cBlobStripeSeekTable, planed.data(), planed.size());
		}

		xbc7_header hdr;
		clear_obj(hdr);
		hdr.m_dct_q = (uint8_t)opts.m_dct_q;
		
		hdr.m_flags = 0;
		if (has_alpha)
			hdr.m_flags = hdr.m_flags | (uint8_t)XBC7_FLAG_HAS_ALPHA;

		hdr.m_width_in_texels = orig_img.get_width();
		hdr.m_height_in_texels = orig_img.get_height();
		hdr.m_num_stripes = (uint8_t)num_stripes;

		blob_writer.put_bytes(cBlobHeader, &hdr, sizeof(hdr));

		for (uint32_t id = 0; id < BLOB_STREAM_MAX_IDS; id++)
		{
			stats.m_blob_raw_size[id] = (uint32_t)blob_writer.get_blob_size(id);
			stats.record_blob_bytes(id, blob_writer.get_blob_data(id));
		}

		if (opts.m_debug_output)
			fmt_debug_printf("XBC7 progress: serializing + Zstd compressing blobs...\n");

		if (!blob_writer.serialize(comp_bytes, 19, stats.m_blob_stored_size))
			return false;

		stats.m_total_file_size = comp_bytes.size_u32();

		if (opts.m_debug_output && (opts.m_ac_trunc_rdo_max_psnr_drop > 0.0f))
			fmt_debug_printf("AC-trunc RDO: {} coeffs pruned across {} blocks ({.2}% of blocks)\n",
				stats.m_ac_trunc_pruned, stats.m_ac_trunc_blocks, total_blocks ? (stats.m_ac_trunc_blocks * 100.0f / (float)total_blocks) : 0.0f);

		// m_debug_output is the master gate; m_print_stats selects the dashboard
		if (opts.m_debug_output && opts.m_print_stats)
			stats.print();

		if (pCoded_log_blocks)
		{
			for (uint32_t by = 0; by < num_blocks_y; by++)
			{
				for (uint32_t bx = 0; bx < num_blocks_x; bx++)
				{
					if (!basist::bc7u::compare_block_full((*pCoded_log_blocks)(bx, by), log_blks(bx, by)))
					{
						assert(0);
						return false;
					}
				}
			}
		}

		if (opts.m_debug_output)
			fmt_debug_printf("XBC7 progress: tiny-mip size check...\n");

		// Tiny-mip fallback: for the smallest levels the blob container's fixed
		// overhead can exceed a raw packed-BC7 encoding. If a tiny-mip stream --
		// [marker][num_blocks_x:u8][num_blocks_y:u8] followed by 16 bytes per BC7
		// block -- is smaller (and the block dims fit a byte), emit that instead.
		// The decoder distinguishes streams by the leading byte: 0xB7 = blob,
		// 0xB8 = tiny-mip without alpha, 0xB9 = tiny-mip with alpha (so the
		// has_alpha bit rides in the marker). The packed blocks are the FINAL
		// logical blocks (identical to pCoded_log_blocks), so the decode round-
		// trips exactly (pack<->unpack of a logical block is lossless).
		if ((num_blocks_x <= 255) && (num_blocks_y <= 255))
		{
			const size_t tiny_size = 3 + (size_t)total_blocks * 16;
			const uint32_t blob_size = comp_bytes.size_u32();

			if (tiny_size < (size_t)blob_size)
			{
				comp_bytes.resize(0);
				comp_bytes.reserve((uint32_t)tiny_size);
				comp_bytes.push_back(has_alpha ? (uint8_t)0xB9 : (uint8_t)0xB8); // marker carries the alpha bit
				comp_bytes.push_back((uint8_t)num_blocks_x);
				comp_bytes.push_back((uint8_t)num_blocks_y);

				for (uint32_t by = 0; by < num_blocks_y; by++)
				{
					for (uint32_t bx = 0; bx < num_blocks_x; bx++)
					{
						basist::bc7u::phys_bc7_block phys;
						if (!basist::bc7u::pack_bc7(log_blks(bx, by), &phys))
						{
							assert(0);
							return false;
						}
						comp_bytes.append(phys.m_bytes, sizeof(phys.m_bytes));

						// The tiny-mip decoder reconstructs each block by UNPACKING
						// the stored physical block. BC7 pack<->unpack is pixel-
						// lossless but can re-canonicalize the LOGICAL fields
						// (endpoint swap + weight complement -> identical pixels,
						// different struct). So the coded reference must be the
						// unpacked form -- exactly what the decoder will yield --
						// not the original log_blks, or a per-block logical compare
						// would spuriously fail.
						if (pCoded_log_blocks)
						{
							if (!basist::bc7u::unpack_bc7(&phys, (*pCoded_log_blocks)(bx, by)))
							{
								assert(0);
								return false;
							}
						}
					}
				}

				assert(comp_bytes.size() == tiny_size);

				if (opts.m_debug_output)
					fmt_debug_printf("XBC7 tiny-mip selected: {} blocks, {} bytes (blob path was {} bytes)\n",
						total_blocks, comp_bytes.size_u32(), blob_size);
			}
		}

		if (opts.m_debug_output)
			fmt_debug_printf("XBC7 output: {} bytes, {} bits/pixel\n",
				comp_bytes.size_u32(), ((float)comp_bytes.size() * 8.0f) / (float)orig_img.get_total_pixels());

		// Self-validation (opts.m_self_validate, ON by default): decode the stream we
		// just produced through the transcoder and verify EVERY logical BC7 block
		// round-trips exactly to the coded reference (coded_log_blocks). This
		// guarantees that every emitted XBC7 stream is valid and unpacks correctly
		// during encoding. It exercises whichever stream format was written above --
		// the blob format or the tiny-mip format (coded_log_blocks already holds the
		// form the decoder yields in each case, including the tiny-mip's
		// re-canonicalized blocks).
		if (opts.m_self_validate)
		{
			if (opts.m_debug_output)
				fmt_debug_printf("XBC7 progress: decode self-validation...\n");

			struct verify_ctx
			{
				const vector2D<basist::bc7u::log_bc7_block>* m_pRef;
				uint32_t m_expected_nbx, m_expected_nby;
				bool m_dims_ok;
				bool m_blocks_ok;
			} vctx;
			vctx.m_pRef = &coded_log_blocks;
			vctx.m_expected_nbx = num_blocks_x;
			vctx.m_expected_nby = num_blocks_y;
			vctx.m_dims_ok = true;
			vctx.m_blocks_ok = true;

			auto verify_init_cb = [](uint32_t nbx, uint32_t nby, uint32_t, uint32_t, uint32_t, bool, void* pData) -> bool
			{
				verify_ctx& c = *(verify_ctx*)pData;
				if ((nbx != c.m_expected_nbx) || (nby != c.m_expected_nby))
				{
					c.m_dims_ok = false;
					return false; // abort decode: dimensions disagree with what we encoded
				}
				return true;
			};

			auto verify_block_cb = [](uint32_t bx, uint32_t by, const basist::bc7u::log_bc7_block& decoded_blk, void* pData) -> bool
			{
				verify_ctx& c = *(verify_ctx*)pData;
				if (!basist::bc7u::compare_block_full((*c.m_pRef)(bx, by), decoded_blk))
				{
					c.m_blocks_ok = false;
					return false; // abort decode: a block did not round-trip
				}
				return true;
			};

			const bool decode_ok = basist::xbc7::unpack_image(
				basist::xbc7::byte_span(comp_bytes),
				verify_init_cb, &vctx,
				verify_block_cb, &vctx);

			if ((!decode_ok) || (!vctx.m_dims_ok) || (!vctx.m_blocks_ok))
			{
				assert(0 && "XBC7 pack_image: encoded stream failed decode self-validation");
				fmt_error_printf("XBC7 pack_image: SELF-VALIDATION FAILED -- the encoded stream did not decode back to the expected BC7 logical blocks (decode_ok={}, dims_ok={}, blocks_ok={})\n",
					decode_ok, vctx.m_dims_ok, vctx.m_blocks_ok);
				return false;
			}

			if (opts.m_debug_output)
				fmt_debug_printf("XBC7 self-validation: decode round-trip OK ({} blocks)\n", total_blocks);
		}

		return true;
#endif // BASISD_SUPPORT_KTX2_ZSTD
	}



} // namespace xbc7

} // namespace basisu
