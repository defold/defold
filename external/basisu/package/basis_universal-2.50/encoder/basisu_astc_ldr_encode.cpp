// File: basisu_astc_ldr_encode.cpp
// Copyright (C) 2019-2026 Binomial LLC. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "basisu_enc.h"
#include "basisu_astc_ldr_encode.h"
#include "basisu_astc_hdr_common.h"
#include "basisu_astc_ldr_common.h"

// pick up BASISD_SUPPORT_KTX2_ZSTD macro (this defines it automatically and sets to 1 if not defined)
#include "../transcoder/basisu_transcoder.h" 

#include "basisu_astc_ldr_fencode.h"

#include <queue>

#ifndef BASISD_SUPPORT_KTX2_ZSTD
#error BASISD_SUPPORT_KTX2_ZSTD must be defined here
#endif

#if BASISD_SUPPORT_KTX2_ZSTD
#include "../zstd/zstd.h"
#endif

#ifndef BASISU_SUPPORT_ASTCENC
#define BASISU_SUPPORT_ASTCENC (0)
#endif

#if BASISU_SUPPORT_ASTCENC
#include "3rdparty/astc-encoder-main/Source/astcenc.h"
#endif

// Compensate for endpoint adjustment (otherwise we're too pessimestic/underranks 2-3 levels)
#define BASISU_MODIFIED_WEIGHT_QUANT_MSE_ESTIMATE (1)

namespace basisu {
namespace astc_ldr {

const bool g_devel_messages = true;
const bool ASTC_LDR_CONSISTENCY_CHECKING = true;

bool g_initialized;

const uint32_t BLUR_ID_GAUSSIAN_ALTERNATE = 1;
const uint32_t BLUR_ID_GRID_DIM_BASE = 32; // experimental
const uint32_t BLUR_ID_DC_LATENT_BASE = 128;
const uint32_t BLUR_ID_AC_LATENT_BASE = 138;
const uint32_t BLUR_ID_BEST_CANDIDATE = 256; // experimental

[[maybe_unused]] const uint32_t BLUR_ID_ASTCENC = 512;
const uint32_t BLUR_ID_ASTCF = 513;

const uint32_t BLUR_ID_EXP = 1024;

// Number of blurred source images used during ASTC LDR encoding. Defined at module scope (not as a function local)
// so the per-block job lambda can reference it directly without capturing it (avoids MSVC C3493 and clang/gcc -Wunused-lambda-capture).
const uint32_t TOTAL_BLURRED_IMAGES = 1 * 3;

const uint32_t EXPECTED_SUPERBUCKET_HASH_SIZE = 8192;
const uint32_t EXPECTED_SHORTLIST_HASH_SIZE = 4096;

const uint32_t MAX_BASE_PARTS2 = 128;
const uint32_t MAX_BASE_PARTS3 = 128;

const uint32_t PART_ESTIMATE_STAGE1_MULTIPLIER = 4;

const uint32_t MAX_WIDTH = 65535, MAX_HEIGHT = 65535;

static inline uint32_t apply_kernel(uint32_t a, uint32_t b, uint32_t c)
{
	return (a + b + c + 1u) / 3u;
}

static inline color_rgba filter_horiz(const image& src, int x, int y)
{
	color_rgba l(src.get_clamped(x - 1, y));
	color_rgba c(src.get_clamped(x, y));
	color_rgba r(src.get_clamped(x + 1, y));

	color_rgba out;
	for (uint32_t ch = 0; ch < 4; ch++)
		out[ch] = (uint8_t)apply_kernel(l[ch], c[ch], r[ch]);
	return out;
}

static inline color_rgba filter_vert(const image& src, int x, int y)
{
	color_rgba u(src.get_clamped(x, y - 1));
	color_rgba c(src.get_clamped(x, y));
	color_rgba d(src.get_clamped(x, y + 1));

	color_rgba out;
	for (uint32_t ch = 0; ch < 4; ch++)
		out[ch] = (uint8_t)apply_kernel(u[ch], c[ch], d[ch]);
	return out;
}

#define BASISU_CORNER(SX, SY, TX, TY) do {                                                \
	const color_rgba h_l = src_img.get_clamped((SX) - 1, (SY));                           \
	const color_rgba h_c = src_img.get_clamped((SX),     (SY));                           \
	const color_rgba h_r = src_img.get_clamped((SX) + 1, (SY));                           \
	const color_rgba v_u = src_img.get_clamped((SX), (SY) - 1);                           \
	const color_rgba v_d = src_img.get_clamped((SX), (SY) + 1);                           \
                                                                                          \
	color_rgba out;                                                                       \
	out.r = (uint8_t)minimum<int>(255, basisu::fast_roundf_pos_int(                       \
		(float)(h_l.r + 2 * h_c.r + h_r.r + v_u.r + v_d.r) * (1.0f / 6.0f)));             \
	out.g = (uint8_t)minimum<int>(255, basisu::fast_roundf_pos_int(                       \
		(float)(h_l.g + 2 * h_c.g + h_r.g + v_u.g + v_d.g) * (1.0f / 6.0f)));             \
	out.b = (uint8_t)minimum<int>(255, basisu::fast_roundf_pos_int(                       \
		(float)(h_l.b + 2 * h_c.b + h_r.b + v_u.b + v_d.b) * (1.0f / 6.0f)));             \
	out.a = (uint8_t)minimum<int>(255, basisu::fast_roundf_pos_int(                       \
		(float)(h_l.a + 2 * h_c.a + h_r.a + v_u.a + v_d.a) * (1.0f / 6.0f)));             \
	dst_tile.set_clipped((TX), (TY), out);                                                \
} while (0)

// bx,by=texel offsets
static void deblock_block_region(int fbw, int fbh, const image& src_img, int bx, int by, image& dst_tile)
{
	assert(&src_img != &dst_tile);
	assert((int)dst_tile.get_width() == (fbw + 2));
	assert((int)dst_tile.get_height() == (fbh + 2));

	for (int ty = 0; ty < (fbh + 2); ty++)
	{
		const bool on_horiz_edge = (ty <= 1) || (ty >= fbh);
		const int sy = by - 1 + ty;
		
		for (int tx = 0; tx < (fbw + 2); tx++)
		{
			const bool on_vert_edge = (tx <= 1) || (tx >= fbw);
			const int sx = bx - 1 + tx;
						
			if (on_vert_edge && on_horiz_edge)
			{
				BASISU_CORNER(sx, sy, tx, ty);
				continue;
			}
			
			color_rgba out;
			if (on_vert_edge)
				out = filter_horiz(src_img, sx, sy);
			else if (on_horiz_edge)
				out = filter_vert(src_img, sx, sy);
			else
				out = src_img.get_clamped(sx, sy);

			dst_tile.set_clipped(tx, ty, out);
		}
	}
}

static void deblock_block_region_interior(int fbw, int fbh, const image& src_img, int bx, int by, image& dst_tile, int dst_x, int dst_y)
{
	assert((bx >= 0) && (bx < (int)src_img.get_width()));
	assert((by >= 0) && (by < (int)src_img.get_height()));
	assert(fbw >= 3);
	assert(fbh >= 3);
	assert(&src_img != &dst_tile);
	assert(src_img.get_width() == dst_tile.get_width());
	assert(src_img.get_height() == dst_tile.get_height());

	const int x_left = bx;
	const int x_right = bx + fbw - 1;
	const int y_top = by;
	const int y_bottom = by + fbh - 1;

	// --- Four corners -------------------------------------------------------

	BASISU_CORNER(x_left, y_top, dst_x, dst_y);
	BASISU_CORNER(x_right, y_top, dst_x + fbw - 1, dst_y);
	BASISU_CORNER(x_left, y_bottom, dst_x, dst_y + fbh - 1);
	BASISU_CORNER(x_right, y_bottom, dst_x + fbw - 1, dst_y + fbh - 1);

	// --- Top and Bottom edges: rows y_top and y_bottom, columns (x_left, x_right) exclusive.
	for (int sy = y_top; sy <= y_bottom; sy += (fbh - 1))
	{
		const int ty = dst_y + (sy - by);
		for (int tx_offset = 1; tx_offset < fbw - 1; tx_offset++)
		{
			const int sx = bx + tx_offset;
			const color_rgba u = src_img.get_clamped(sx, sy - 1);
			const color_rgba c = src_img.get_clamped(sx, sy);
			const color_rgba d = src_img.get_clamped(sx, sy + 1);

			color_rgba out;
			out.r = (uint8_t)(((uint32_t)u.r + (uint32_t)c.r + (uint32_t)d.r + 1u) / 3u);
			out.g = (uint8_t)(((uint32_t)u.g + (uint32_t)c.g + (uint32_t)d.g + 1u) / 3u);
			out.b = (uint8_t)(((uint32_t)u.b + (uint32_t)c.b + (uint32_t)d.b + 1u) / 3u);
			out.a = (uint8_t)(((uint32_t)u.a + (uint32_t)c.a + (uint32_t)d.a + 1u) / 3u);

			dst_tile.set_clipped(dst_x + tx_offset, ty, out);
		}
	}

	// --- Left and Right edges: columns x_left and x_right, rows (y_top, y_bottom) exclusive.
	for (int sx = x_left; sx <= x_right; sx += (fbw - 1))
	{
		const int tx = dst_x + (sx - bx);
		for (int ty_offset = 1; ty_offset < fbh - 1; ty_offset++)
		{
			const int sy = by + ty_offset;
			const color_rgba l = src_img.get_clamped(sx - 1, sy);
			const color_rgba c = src_img.get_clamped(sx, sy);
			const color_rgba r = src_img.get_clamped(sx + 1, sy);

			color_rgba out;
			out.r = (uint8_t)(((uint32_t)l.r + (uint32_t)c.r + (uint32_t)r.r + 1u) / 3u);
			out.g = (uint8_t)(((uint32_t)l.g + (uint32_t)c.g + (uint32_t)r.g + 1u) / 3u);
			out.b = (uint8_t)(((uint32_t)l.b + (uint32_t)c.b + (uint32_t)r.b + 1u) / 3u);
			out.a = (uint8_t)(((uint32_t)l.a + (uint32_t)c.a + (uint32_t)r.a + 1u) / 3u);

			dst_tile.set_clipped(tx, dst_y + ty_offset, out);
		}
	}

	// --- Interior: pass-through copy from source.
	// Fast path: interior fully in-bounds -> memcpy each row.
	const int interior_last_sx = bx + fbw - 2;
	const int interior_last_sy = by + fbh - 2;
	const bool interior_in_bounds =
		(interior_last_sx < (int)src_img.get_width()) &&
		(interior_last_sy < (int)src_img.get_height());

	if (interior_in_bounds)
	{
		const uint32_t bytes_per_row = (fbw - 2) * (uint32_t)sizeof(color_rgba);
		const uint32_t src_pitch = src_img.get_pitch();
		const uint32_t dst_pitch = dst_tile.get_pitch();

		for (int ty_offset = 1; ty_offset < (fbh - 1); ty_offset++)
		{
			const color_rgba* pSrc = src_img.get_ptr() + (bx + 1) + (by + ty_offset) * src_pitch;
			color_rgba* pDst = dst_tile.get_ptr() + (dst_x + 1) + (dst_y + ty_offset) * dst_pitch;
			memcpy(pDst, pSrc, bytes_per_row);
		}
	}
	else
	{
		for (int ty_offset = 1; ty_offset < (fbh - 1); ty_offset++)
		{
			const int sy = by + ty_offset;
			for (int tx_offset = 1; tx_offset < (fbw - 1); tx_offset++)
			{
				const int sx = bx + tx_offset;
				dst_tile.set_clipped(dst_x + tx_offset, dst_y + ty_offset, src_img.get_clamped(sx, sy));
			}
		}
	}
}

static void deblock_image(int fbw, int fbh, const image& src_img, image& dst_img)
{
	assert(&src_img != &dst_img);
	dst_img.match_dimensions(src_img);

	const uint32_t num_blocks_x = src_img.get_block_width(fbw);
	const uint32_t num_blocks_y = src_img.get_block_height(fbh);

	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			deblock_block_region_interior(fbw, fbh,
				src_img, bx * (uint32_t)fbw, by * (uint32_t)fbh,
				dst_img, bx * (uint32_t)fbw, by * (uint32_t)fbh);
		} // bx
	} // by
}

#undef BASISU_CORNER

static void code_block_weights(
	basist::astc_ldr_t::grid_weight_dct &gw_dct,
	float q, uint32_t plane_index,
	const astc_helpers::log_astc_block& log_blk,
	basist::astc_ldr_t::dct_syms& syms,
	basist::astc_ldr_t::fvec &dct_work)
{
	assert(q > 0.0f);
		
	syms.clear();

	const uint32_t grid_width = log_blk.m_grid_width, grid_height = log_blk.m_grid_height;
	const uint32_t total_grid_samples = grid_width * grid_height;
	const uint32_t num_planes = log_blk.m_dual_plane ? 2 : 1;

	//const auto& dequant_tab = astc_helpers::g_dequant_tables.get_weight_tab(log_blk.m_weight_ise_range).m_ISE_to_val;
	//const auto& quant_tab = astc_helpers::g_dequant_tables.get_weight_tab(log_blk.m_weight_ise_range).m_val_to_ise;

	uint8_t dequantized_raw_weights0[astc_helpers::MAX_BLOCK_PIXELS];

	for (uint32_t i = 0; i < grid_width * grid_height; i++)
		dequantized_raw_weights0[i] = astc_helpers::g_dequant_tables.get_weight_tab(log_blk.m_weight_ise_range).m_ISE_to_val[log_blk.m_weights[i * num_planes + plane_index]];

	auto grid_dim_vals_iter = gw_dct.m_grid_dim_key_vals.find(basist::astc_ldr_t::grid_dim_key(grid_width, grid_height));
	assert(grid_dim_vals_iter != gw_dct.m_grid_dim_key_vals.end());

	auto& grid_dim_vals = grid_dim_vals_iter->second;

	float orig_weights[astc_helpers::MAX_BLOCK_PIXELS];
	float weight_sum = 0;
	for (uint32_t y = 0; y < grid_height; y++)
	{
		for (uint32_t x = 0; x < grid_width; x++)
		{
			orig_weights[x + y * grid_width] = dequantized_raw_weights0[x + y * grid_width];
			weight_sum += orig_weights[x + y * grid_width];
		}
	}

	float scaled_weight_coding_scale = basist::astc_ldr_t::SCALED_WEIGHT_BASE_CODING_SCALE;
	if (log_blk.m_weight_ise_range <= astc_helpers::BISE_8_LEVELS)
		scaled_weight_coding_scale = 1.0f / 8.0f;

	float scaled_mean_weight = std::round((float)scaled_weight_coding_scale * (weight_sum / total_grid_samples));
	scaled_mean_weight = basisu::clamp<float>(scaled_mean_weight, 0.0f, 64.0f * (float)scaled_weight_coding_scale);

	float mean_weight = scaled_mean_weight / (float)scaled_weight_coding_scale;

	for (uint32_t y = 0; y < grid_height; y++)
		for (uint32_t x = 0; x < grid_width; x++)
			orig_weights[x + y * grid_width] -= mean_weight;

	const float span_len = gw_dct.get_max_span_len(log_blk, plane_index);

	float dct_weights[astc_helpers::MAX_BLOCK_PIXELS];
	
	grid_dim_vals.m_dct.forward(orig_weights, dct_weights, dct_work);

	const float level_scale = gw_dct.compute_level_scale(q, span_len, grid_width, grid_height, log_blk.m_weight_ise_range);

	int dct_quant_tab[astc_helpers::MAX_BLOCK_PIXELS];
	gw_dct.compute_quant_table(q, grid_width, grid_height, level_scale, dct_quant_tab);

#if defined(DEBUG) || defined(_DEBUG)
	// sanity checking
	basist::astc_ldr_t::sample_quant_table_state quant_state;
	quant_state.init(q, gw_dct.m_block_width, gw_dct.m_block_height, level_scale);
#endif

	syms.m_dc_sym = (int)scaled_mean_weight;
	syms.m_num_dc_levels = (uint32_t)(64.0f * scaled_weight_coding_scale) + 1;
	assert(syms.m_num_dc_levels == gw_dct.get_num_weight_dc_levels(log_blk.m_weight_ise_range));

	int dct_coeffs[astc_helpers::MAX_BLOCK_PIXELS]; // TODO: max grid size is 8x8

	for (uint32_t y = 0; y < grid_height; y++)
	{
		for (uint32_t x = 0; x < grid_width; x++)
		{
			if (!x && !y)
			{
				dct_coeffs[0] = 0;
				continue;
			}

			const int levels = dct_quant_tab[x + y * grid_width];

#if defined(DEBUG) || defined(_DEBUG)			
			// sanity checking
			assert(levels == gw_dct.sample_quant_table(quant_state, x, y));
#endif

			float d = dct_weights[x + y * grid_width];

			int id = gw_dct.quantize_deadzone(d, levels, basist::astc_ldr_t::DEADZONE_ALPHA, x, y);

			dct_coeffs[x + y * grid_width] = id;

		} // x

	}  // y

	const basisu::int_vec& zigzag = grid_dim_vals.m_zigzag;
	assert(zigzag.size() == total_grid_samples);
		
	syms.m_coeffs.reserve(65);

	int total_zeros = 0;
	for (uint32_t i = 0; i < total_grid_samples; i++)
	{
		uint32_t dct_idx = zigzag[i];
		if (!dct_idx)
			continue;

		int coeff = dct_coeffs[dct_idx];
		if (!coeff)
		{
			total_zeros++;
			continue;
		}

		basist::astc_ldr_t::dct_syms::coeff cf;
		cf.m_num_zeros = basisu::safe_cast_uint16(total_zeros);
		cf.m_coeff = basisu::safe_cast_int16(coeff);

		syms.m_coeffs.push_back(cf);
		
		syms.m_max_coeff_mag = basisu::maximum(syms.m_max_coeff_mag, basisu::iabs(coeff));
		syms.m_max_zigzag_index = basisu::maximum(syms.m_max_zigzag_index, i);

		total_zeros = 0;
	}
	
	{
		// reduce allocation
		basisu::vector<basist::astc_ldr_t::dct_syms::coeff> temp_coeffs(syms.m_coeffs);
		
		syms.m_coeffs.swap(temp_coeffs);
	}

	if (total_zeros)
	{
		basist::astc_ldr_t::dct_syms::coeff cf;
		cf.m_num_zeros = basisu::safe_cast_uint16(total_zeros);
		cf.m_coeff = INT16_MAX;
		syms.m_coeffs.push_back(cf);
	}
}

static void astc_ldr_requantize_astc_weights(uint32_t n, const uint8_t* pSrc_ise_vals, uint32_t from_ise_range, uint8_t* pDst_ise_vals, uint32_t to_ise_range)
{
	if (from_ise_range == to_ise_range)
	{
		if (pDst_ise_vals != pSrc_ise_vals)
			memcpy(pDst_ise_vals, pSrc_ise_vals, n);
		return;
	}

	// from/to BISE ranges not equal
	if (from_ise_range == astc_helpers::BISE_64_LEVELS)
	{
		// from [0,64]
		const auto& quant_tab = astc_helpers::g_dequant_tables.get_weight_tab(to_ise_range).m_val_to_ise;

		for (uint32_t i = 0; i < n; i++)
			pDst_ise_vals[i] = quant_tab[pSrc_ise_vals[i]];
	}
	else if (to_ise_range == astc_helpers::BISE_64_LEVELS)
	{
		// to [0,64]
		const auto& dequant_tab = astc_helpers::g_dequant_tables.get_weight_tab(from_ise_range).m_ISE_to_val;

		for (uint32_t i = 0; i < n; i++)
			pDst_ise_vals[i] = dequant_tab[pSrc_ise_vals[i]];
	}
	else
	{
		// from/to any other
		const auto& dequant_tab = astc_helpers::g_dequant_tables.get_weight_tab(from_ise_range).m_ISE_to_val;
		const auto& quant_tab = astc_helpers::g_dequant_tables.get_weight_tab(to_ise_range).m_val_to_ise;

		for (uint32_t i = 0; i < n; i++)
			pDst_ise_vals[i] = quant_tab[dequant_tab[pSrc_ise_vals[i]]];
	}
}

static void astc_ldr_downsample_ise_weights(
	uint32_t dequant_weight_ise_range, uint32_t quant_weight_ise_range,
	uint32_t block_w, uint32_t block_h,
	uint32_t grid_w, uint32_t grid_h,
	const uint8_t* pSrc_weights, uint8_t* pDst_weights,
	const float* pDownsample_matrix)
{
	assert((block_w <= astc_ldr::ASTC_LDR_MAX_BLOCK_WIDTH) && (block_h <= astc_ldr::ASTC_LDR_MAX_BLOCK_HEIGHT));
	assert((grid_w >= 2) && (grid_w <= block_w));
	assert((grid_h >= 2) && (grid_h <= block_h));

	assert(((dequant_weight_ise_range >= astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE) && (dequant_weight_ise_range <= astc_helpers::LAST_VALID_WEIGHT_ISE_RANGE)) ||
		(dequant_weight_ise_range == astc_helpers::BISE_64_LEVELS));

	assert(((quant_weight_ise_range >= astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE) && (quant_weight_ise_range <= astc_helpers::LAST_VALID_WEIGHT_ISE_RANGE)) ||
		(quant_weight_ise_range == astc_helpers::BISE_64_LEVELS));

	assert(pDownsample_matrix);

	if ((block_w == grid_w) && (block_h == grid_h))
	{
		if (dequant_weight_ise_range != quant_weight_ise_range)
		{
			astc_ldr_requantize_astc_weights(block_w * block_h, pSrc_weights, dequant_weight_ise_range, pDst_weights, quant_weight_ise_range);
		}
		else
		{
			if (pDst_weights != pSrc_weights)
				memcpy(pDst_weights, pSrc_weights, block_w * block_h);
		}

		return;
	}

	uint8_t desired_weights[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	if (dequant_weight_ise_range == astc_helpers::BISE_64_LEVELS)
	{
		memcpy(desired_weights, pSrc_weights, block_w * block_h);
	}
	else
	{
		const auto& dequant_tab = astc_helpers::g_dequant_tables.get_weight_tab(dequant_weight_ise_range).m_ISE_to_val;

		for (uint32_t by = 0; by < block_h; by++)
			for (uint32_t bx = 0; bx < block_w; bx++)
				desired_weights[bx + by * block_w] = dequant_tab[pSrc_weights[bx + by * block_w]];
	}

	if (quant_weight_ise_range == astc_helpers::BISE_64_LEVELS)
	{
		downsample_weight_grid(
			pDownsample_matrix,
			block_w, block_h,		// source/from dimension (block size)
			grid_w, grid_h,			// dest/to dimension (grid size)
			desired_weights,		// these are dequantized weights, NOT ISE symbols, [by][bx]
			pDst_weights);	// [wy][wx]
	}
	else
	{
		uint8_t raw_downsampled_weights[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		downsample_weight_grid(
			pDownsample_matrix,
			block_w, block_h,		// source/from dimension (block size)
			grid_w, grid_h,			// dest/to dimension (grid size)
			desired_weights,		// these are dequantized weights, NOT ISE symbols, [by][bx]
			raw_downsampled_weights);	// [wy][wx]

		const auto& weight_quant_tab = astc_helpers::g_dequant_tables.get_weight_tab(quant_weight_ise_range).m_val_to_ise;

		for (uint32_t gy = 0; gy < grid_h; gy++)
			for (uint32_t gx = 0; gx < grid_w; gx++)
				pDst_weights[gx + gy * grid_w] = weight_quant_tab[raw_downsampled_weights[gx + gy * grid_w]];
	}
}

void downsample_weight_residual_grid(
	const float* pMatrix_weights,
	uint32_t bx, uint32_t by,		// source/from dimension (block size)
	uint32_t wx, uint32_t wy,		// dest/to dimension (grid size)
	const int* pSrc_weights,	// these are dequantized weights, NOT ISE symbols, [by][bx]
	float* pDst_weights)			// [wy][wx]
{
	const uint32_t total_block_samples = bx * by;

	for (uint32_t y = 0; y < wy; y++)
	{
		for (uint32_t x = 0; x < wx; x++)
		{
			float total = 0.0f;

			for (uint32_t i = 0; i < total_block_samples; i++)
				if (pMatrix_weights[i])
					total += pMatrix_weights[i] * (float)pSrc_weights[i];

			pDst_weights[x + y * wx] = total;

			pMatrix_weights += total_block_samples;
		}
	}
}

#if 0
static void downsample_weightsf(
	const float* pMatrix_weights,
	uint32_t bx, uint32_t by,		// source/from dimension (block size)
	uint32_t wx, uint32_t wy,		// dest/to dimension (grid size)
	const float* pSrc_weights,	// these are dequantized weights, NOT ISE symbols, [by][bx]
	float* pDst_weights)			// [wy][wx]
{
	const uint32_t total_block_samples = bx * by;

	for (uint32_t y = 0; y < wy; y++)
	{
		for (uint32_t x = 0; x < wx; x++)
		{
			float total = 0.0f;

			for (uint32_t i = 0; i < total_block_samples; i++)
				if (pMatrix_weights[i])
					total += pMatrix_weights[i] * pSrc_weights[i];

			pDst_weights[x + y * wx] = total;

			pMatrix_weights += total_block_samples;
		}
	}
}
#endif

static inline uint32_t weighted_color_error(const color_rgba& a, const color_rgba& b, const astc_ldr::cem_encode_params& p)
{
#if 0
	uint32_t total_e = 0;
	for (uint32_t c = 0; c < 4; c++)
	{
		int av = a[c];
		int bv = b[c];
		int ev = av - bv;
		total_e += (uint32_t)(ev * ev) * p.m_comp_weights[c];
	}
	
	return total_e;
#else
	const uint32_t total_e2 = squarei(a[0] - b[0]) * p.m_comp_weights[0] +
		squarei(a[1] - b[1]) * p.m_comp_weights[1] +
		squarei(a[2] - b[2]) * p.m_comp_weights[2] +
		squarei(a[3] - b[3]) * p.m_comp_weights[3];

	return total_e2;
#endif
}

static uint64_t eval_error(
	uint32_t block_width, uint32_t block_height,
	const astc_helpers::log_astc_block& enc_log_block,
	const astc_ldr::pixel_stats_t& pixel_stats,
	const astc_ldr::cem_encode_params& params)
{
	color_rgba dec_block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	bool status = astc_helpers::decode_block_xuastc_ldr(enc_log_block, dec_block_pixels, block_width, block_height, params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
	if (!status)
	{
		// Shouldn't ever happen
		assert(0);
		return UINT64_MAX;
	}

#if defined(_DEBUG) || defined(DEBUG)
	// Sanity check vs. unoptimized decoder
	color_rgba dec_block_pixels_alt[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	bool alt_status = astc_helpers::decode_block(enc_log_block, dec_block_pixels_alt, block_width, block_height, params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
	if (!alt_status)
	{
		// Shouldn't ever happen
		assert(0);
		return UINT64_MAX;
	}

	if (memcmp(dec_block_pixels, dec_block_pixels_alt, sizeof(color_rgba) * block_width * block_height) != 0)
	{
		// Very bad
		assert(0);
		return UINT64_MAX;
	}
#endif

	uint64_t total_err = 0;

	const uint32_t total_block_pixels = block_width * block_height;
	for (uint32_t i = 0; i < total_block_pixels; i++)
		total_err += weighted_color_error(dec_block_pixels[i], pixel_stats.m_pixels[i], params);

	return total_err;
}

static uint64_t eval_error(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	uint32_t cem_index,
	bool dual_plane_flag, int ccs_index,
	uint32_t endpoint_ise_range, uint32_t weight_ise_range,
	uint32_t grid_width, uint32_t grid_height,
	const uint8_t* pEndpoint_vals, const uint8_t* pWeight_grid_vals0, const uint8_t* pWeight_grid_vals1,
	const astc_ldr::cem_encode_params& params)
{
	//const uint32_t total_block_pixels = block_width * block_height;
	const uint32_t total_grid_pixels = grid_width * grid_height;

	astc_helpers::log_astc_block enc_log_block;

	enc_log_block.clear();
	enc_log_block.m_grid_width = (uint8_t)grid_width;
	enc_log_block.m_grid_height = (uint8_t)grid_height;
	enc_log_block.m_weight_ise_range = (uint8_t)weight_ise_range;
	enc_log_block.m_endpoint_ise_range = (uint8_t)endpoint_ise_range;
	enc_log_block.m_color_endpoint_modes[0] = (uint8_t)cem_index;
	enc_log_block.m_num_partitions = 1;

	memcpy(enc_log_block.m_endpoints, pEndpoint_vals, astc_helpers::get_num_cem_values(cem_index));

	if (dual_plane_flag)
	{
		assert((ccs_index >= 0) && (ccs_index <= 3));

		enc_log_block.m_dual_plane = true;
		enc_log_block.m_color_component_selector = (uint8_t)ccs_index;

		for (uint32_t i = 0; i < total_grid_pixels; i++)
		{
			enc_log_block.m_weights[i * 2 + 0] = pWeight_grid_vals0[i];
			enc_log_block.m_weights[i * 2 + 1] = pWeight_grid_vals1[i];
		}
	}
	else
	{
		assert(ccs_index < 0);

		memcpy(enc_log_block.m_weights, pWeight_grid_vals0, total_grid_pixels);
	}

	return eval_error(block_width, block_height, enc_log_block, pixel_stats, params);
}

static float compute_psnr_from_wsse(uint32_t block_width, uint32_t block_height, uint64_t sse, float total_comp_weights)
{
	const uint32_t total_block_pixels = block_width * block_height;
	const float wmse = (float)sse / (total_comp_weights * (float)total_block_pixels);
	const float wpsnr = (wmse > 1e-5f) ? (20.0f * log10f(255.0f / sqrtf(wmse))) : 10000.0f;
	return wpsnr;
}

// quantized coordinate descent (QCD), quadratic objective
namespace qcd
{
	struct qcd_min_solver
	{
		// geometry / sizes
		int m_N = 0; // texels
		int m_K = 0; // controls
		int m_Q = 0; // label count

		// inputs (not owned), (N x K) row-major
		const float* m_pU = nullptr; // grid to texel upsample matrix

		// cached
		float_vec m_ucols; // N*K, column k at &m_ucols[k*m_N]
		float_vec m_alpha; // K, ||u_k||^2 (>= eps)
		float_vec m_labels; // Q, sorted unique u-labels (ints in [0..64]), ASTC raw [0,64] weights

		bool m_ready_flag = false;

		// init: cache columns, norms, and label set
		bool init(const float* pU_rowmajor, int N, int K, const int* pLabels_u, int Q)
		{
			if ((!pU_rowmajor) || (!pLabels_u) || (N <= 0) || (K <= 0) || (Q <= 0))
				return false;

			m_pU = pU_rowmajor;
			m_N = N;
			m_K = K;
			m_Q = Q;

			// cache columns
			m_ucols.assign(size_t(N) * K, 0.0f);

			for (int k = 0; k < K; ++k)
			{
				float* pDst = &m_ucols[size_t(k) * size_t(N)];
				const float* pSrc = m_pU + k; // first element of column k
				for (int t = 0; t < N; ++t)
					pDst[t] = pSrc[size_t(t) * size_t(K)];
			}

			// column norms
			m_alpha.resize(K);

			for (int k = 0; k < K; ++k)
			{
				const float* pUK = &m_ucols[size_t(k) * size_t(N)];

				float a = 0.0f;
				for (int t = 0; t < N; ++t)
					a += pUK[t] * pUK[t];

				if (!(a > 0.0f))
					a = 1e-8f;

				m_alpha[k] = a;
			}

			m_labels.assign(pLabels_u, pLabels_u + Q);

#if defined(_DEBUG) || defined(DEBUG)
			for (size_t i = 1; i < m_labels.size(); ++i)
			{
				assert(m_labels[i] > m_labels[i - 1]); // strictly increasing
				assert((m_labels[i] >= 0) && (m_labels[i] <= 64));
			}
#endif

			m_Q = (int)m_labels.size();
			if (m_Q <= 0)
				return false;

			m_ready_flag = true;
			return true;
		}

		// compute residual r = U*g - w* (uses label IDs -> u-values)
		void build_residual(const int* pG_idx, const float* pW_star, float* pR_out) const
		{
			assert(m_ready_flag && pG_idx && pW_star && pR_out);

			// r = sum_k (u_label[pG_idx[k]] * ucol_k) - pW_star
			std::fill(pR_out, pR_out + m_N, 0.0f);

			for (int k = 0; k < m_K; ++k)
			{
				const float* pUK = &m_ucols[size_t(k) * size_t(m_N)];
				const float s = m_labels[pG_idx[k]];

				for (int t = 0; t < m_N; ++t)
					pR_out[t] += s * pUK[t];
			}

			for (int t = 0; t < m_N; ++t)
				pR_out[t] -= pW_star[t];
		}

		// one QCD sweep: returns num moves accepted (strict dE < -eps)
		int sweep(int* pG_idx, float* pR_io, float accept_eps = 1e-6f) const
		{
			assert(m_ready_flag && pG_idx && pR_io);
			int num_moved = 0;

			for (int k = 0; k < m_K; ++k)
			{
				const float* pUK = &m_ucols[size_t(k) * size_t(m_N)];

				// beta = <r, u_k>
				float beta = 0.0f;
				for (int t = 0; t < m_N; ++t)
					beta += pR_io[t] * pUK[t];

				const float a = m_alpha[k]; // >= 1e-8

				const float cur_u = m_labels[pG_idx[k]];
				const float s_star = cur_u - beta / a; // continuous minimizer (u-domain)

				// nearest label index to s_star (binary search)
				const int j0 = nearest_label_idx(s_star);

				const int cand[3] =
				{
					j0,
					(j0 + 1 < m_Q) ? (j0 + 1) : j0,
					(j0 - 1 >= 0) ? (j0 - 1) : j0
				};

				int best_j = pG_idx[k];
				float best_dE = 0.0f;

				for (int c = 0; c < 3; ++c)
				{
					const int j = cand[c];
					if (j == pG_idx[k])
						continue;

					const float s = m_labels[j];
					const float d = s - cur_u; // u-change at coord k
					const float dE = 2.0f * d * beta + d * d * a; // exact delta E

					if ((best_j == pG_idx[k]) || (dE < best_dE))
					{
						best_dE = dE;
						best_j = j;
					}
				}

				if ((best_j != pG_idx[k]) && (best_dE < -accept_eps))
				{
					// commit: update residual and label ID
					const float d = m_labels[best_j] - cur_u;

					for (int t = 0; t < m_N; ++t)
						pR_io[t] += d * pUK[t];

					pG_idx[k] = best_j;
					++num_moved;
				}
			} // k

			return num_moved;
		}

		// utility: energy from residual (sum r^2)
		float residual_energy(const float* pR) const
		{
			assert(pR);

			float E = 0.0f;
			for (int t = 0; t < m_N; ++t)
				E += pR[t] * pR[t];

			return E;
		}

	private:
		// nearest label index by u-value (handles non-uniform spacing)
		int nearest_label_idx(float x) const
		{
			const int Q = m_Q;

			if (Q <= 1)
				return 0;
			if (x <= m_labels.front())
				return 0;
			if (x >= m_labels.back())
				return Q - 1;

			int lo = 0, hi = Q - 1;
			while (hi - lo > 1)
			{
				const int mid = (lo + hi) >> 1;
				(x >= m_labels[mid]) ? lo = mid : hi = mid;
			}

			const float dlo = std::fabs(x - m_labels[lo]);
			const float dhi = std::fabs(x - m_labels[hi]);
			return (dlo <= dhi) ? lo : hi;
		}
	};

} // namespace qcd

const uint32_t NUM_WEIGHT_POLISH_PASSES = 1;

#if 0
// true if improved
static bool polish_block_weights_final_slow(
	astc_helpers::log_astc_block& enc_log_block, // assumes there is already a good encoding to improve here
	uint8_t* pWeights0, uint8_t* pWeights1, // the latest weights, will be updated if improved
	uint32_t block_width, uint32_t block_height, uint32_t grid_width, uint32_t grid_height, 
	const astc_ldr::pixel_stats_t& pixel_stats,
	const astc_ldr::cem_encode_params& params,
	uint64_t &cur_err)
{
	const bool dual_plane_flag = enc_log_block.m_dual_plane;

	//const uint32_t endpoint_ise_range = enc_log_block.m_endpoint_ise_range;
	const uint32_t weight_ise_range = enc_log_block.m_weight_ise_range;
	
	//const uint32_t total_block_pixels = block_width * block_height;
	const uint32_t total_grid_pixels = grid_width * grid_height;

	bool improved_flag = false;

	for (uint32_t polish_pass = 0; polish_pass < NUM_WEIGHT_POLISH_PASSES; polish_pass++)
	{
		for (uint32_t y = 0; y < grid_height; y++)
		{
			for (uint32_t x = 0; x < grid_width; x++)
			{
				for (uint32_t plane_iter = 0; plane_iter < (dual_plane_flag ? 2u : 1u); plane_iter++)
				{
					uint8_t base_grid_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], base_grid_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

					memcpy(base_grid_weights0, pWeights0, total_grid_pixels);
					if (dual_plane_flag)
						memcpy(base_grid_weights1, pWeights1, total_grid_pixels);

					for (int delta = -1; delta <= 1; delta += 2)
					{
						uint8_t trial_grid_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], trial_grid_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

						memcpy(trial_grid_weights0, base_grid_weights0, total_grid_pixels);

						if (dual_plane_flag)
							memcpy(trial_grid_weights1, base_grid_weights1, total_grid_pixels);

						if (plane_iter == 0)
							trial_grid_weights0[x + y * grid_width] = (uint8_t)astc_ldr::apply_delta_to_bise_weight_val(weight_ise_range, base_grid_weights0[x + y * grid_width], delta);
						else
							trial_grid_weights1[x + y * grid_width] = (uint8_t)astc_ldr::apply_delta_to_bise_weight_val(weight_ise_range, base_grid_weights1[x + y * grid_width], delta);

						astc_helpers::log_astc_block trial_log_block(enc_log_block);

						astc_helpers::set_weights(trial_log_block, trial_grid_weights0, 0);

						if (dual_plane_flag)
							astc_helpers::set_weights(trial_log_block, trial_grid_weights1, 1);

						uint64_t trial_err = eval_error(block_width, block_height, trial_log_block, pixel_stats, params);

						if (trial_err < cur_err)
						{
							cur_err = trial_err;

							memcpy(pWeights0, trial_grid_weights0, total_grid_pixels);

							if (dual_plane_flag)
								memcpy(pWeights1, trial_grid_weights1, total_grid_pixels);

							improved_flag = true;
						}

					} // delta

				} // plane_iter

			} // x
		} // y

	} // polish_pass

	return improved_flag;
}
#endif

#define BASISU_POLISH_DEBUG (0)

// true if improved
static bool polish_block_weights_final_fast(
	astc_helpers::log_astc_block& enc_log_block, // assumes there is already a good encoding to improve here
	uint8_t* pWeights0, uint8_t* pWeights1, // the latest weights, will be updated if improved
	uint32_t block_width, uint32_t block_height, uint32_t grid_width, uint32_t grid_height, 
	const astc_ldr::pixel_stats_t& pixel_stats,
	const astc_ldr::cem_encode_params& params,
	const basist::astc_ldr_t::astc_block_grid_data* pGrid_data,
	uint64_t& cur_err)
{
	if (!cur_err)
		return false;

	const bool dual_plane_flag = enc_log_block.m_dual_plane;

	//const uint32_t endpoint_ise_range = enc_log_block.m_endpoint_ise_range;
	const uint32_t weight_ise_range = enc_log_block.m_weight_ise_range;

	const uint32_t total_block_pixels = block_width * block_height;
	BASISU_NOTE_UNUSED(total_block_pixels);
	//const uint32_t total_grid_pixels = grid_width * grid_height;

	bool improved_flag = false;

	astc_helpers::log_astc_block cur_log_block(enc_log_block);
	
	astc_helpers::set_weights(cur_log_block, pWeights0, 0);
	if (dual_plane_flag)
		astc_helpers::set_weights(cur_log_block, pWeights1, 1);
		
	astc_helpers::xuastc_ldr_block_decoder block_decoder(
		cur_log_block, block_width, block_height, 
		params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8, 
		pGrid_data->m_upsample_weights.get_ptr());

	for (uint32_t polish_pass = 0; polish_pass < NUM_WEIGHT_POLISH_PASSES; polish_pass++)
	{
		for (uint32_t y = 0; y < grid_height; y++)
		{
			for (uint32_t x = 0; x < grid_width; x++)
			{
				const basisu::uint16_vec& influenced_texels = pGrid_data->m_grid_to_texel_influence_list[x + y * grid_width];

				for (uint32_t plane_iter = 0; plane_iter < (dual_plane_flag ? 2u : 1u); plane_iter++)
				{

#if BASISU_POLISH_DEBUG
#if defined(_DEBUG) || defined(DEBUG)
					assert(cur_err == eval_error(block_width, block_height, cur_log_block, pixel_stats, params));

					{
						color_rgba alt_block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
						bool status = astc_helpers::decode_block_xuastc_ldr(cur_log_block, alt_block_pixels, block_width, block_height, params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
						assert(status);

						uint64_t alt_err = 0;
						for (uint32_t k = 0; k < total_block_pixels; k++)
						{
							const uint32_t texel_x = k % block_width;
							const uint32_t texel_y = k / block_width;

							color_rgba dec_c;
							block_decoder.decode_texel(texel_x, texel_y, (astc_helpers::color_rgba&)dec_c);

							if ((dec_c.r != alt_block_pixels[k].r) ||
								(dec_c.g != alt_block_pixels[k].g) ||
								(dec_c.b != alt_block_pixels[k].b) ||
								(dec_c.a != alt_block_pixels[k].a))
							{
								assert(0);
							}

							uint64_t w = weighted_color_error(dec_c, pixel_stats.m_pixels[k], params);
							alt_err += w;
						}
						assert(alt_err == cur_err);
					}
#endif
#endif

					const uint8_t base_weight = astc_helpers::get_weight(cur_log_block, plane_iter, x + y * grid_width);
										
					for (int delta = -1; delta <= 1; delta += 2)
					{
						const uint8_t new_weight = (uint8_t)astc_ldr::apply_delta_to_bise_weight_val(weight_ise_range, base_weight, delta);
						if (new_weight == base_weight)
							continue;
												
						// remove out influence from the current weight
						uint64_t trial_err = cur_err;
						for (uint32_t j = 0; j < influenced_texels.size(); j++)
						{
							const uint32_t packed_texel_index = influenced_texels[j];
							const uint32_t texel_x = packed_texel_index & 0xFF;
							const uint32_t texel_y = packed_texel_index >> 8;
							const uint32_t texel_index = texel_x + texel_y * block_width;

							color_rgba dec_c;
							block_decoder.decode_texel(texel_x, texel_y, (astc_helpers::color_rgba &)dec_c);

							uint64_t w = weighted_color_error(dec_c, pixel_stats.m_pixels[texel_index], params);
							assert(trial_err >= w);
							trial_err -= w;
						}

						// save weight in case it's worse
						const uint8_t saved_weight = astc_helpers::get_weight(cur_log_block, plane_iter, x + y * grid_width);

						// change current weight
						astc_helpers::get_weight(cur_log_block, plane_iter, x + y * grid_width) = new_weight;

						// now add in influence from the new weight
						for (uint32_t j = 0; j < influenced_texels.size(); j++)
						{
							const uint32_t packed_texel_index = influenced_texels[j];
							const uint32_t texel_x = packed_texel_index & 0xFF;
							const uint32_t texel_y = packed_texel_index >> 8;
							const uint32_t texel_index = texel_x + texel_y * block_width;

							color_rgba dec_c;
							block_decoder.decode_texel(texel_x, texel_y, (astc_helpers::color_rgba&)dec_c);

							trial_err += weighted_color_error(dec_c, pixel_stats.m_pixels[texel_index], params);
						}

#if BASISU_POLISH_DEBUG
						assert(trial_err == eval_error(block_width, block_height, cur_log_block, pixel_stats, params));
#endif

						// see if we've improved the solution by this one weight change
						if (trial_err < cur_err)
						{
							cur_err = trial_err;
							
							improved_flag = true;
						}
						else
						{
							// candidate was worse, so restore the weight 
							astc_helpers::get_weight(cur_log_block, plane_iter, x + y * grid_width) = saved_weight;
						}

					} // delta

#if BASISU_POLISH_DEBUG
					assert(cur_err == eval_error(block_width, block_height, cur_log_block, pixel_stats, params));
#endif

				} // plane_iter

			} // x
		} // y

	} // polish_pass

	if (improved_flag)
	{
		astc_helpers::extract_weights(cur_log_block, pWeights0, 0);
		
		if (dual_plane_flag)
			astc_helpers::extract_weights(cur_log_block, pWeights1, 1);
	}

	return improved_flag;
}

// 1-3 subsets, requires initial weights
static bool polish_block_weights(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	astc_helpers::log_astc_block& enc_log_block, // assumes there is already a good encoding to improve here
	const astc_ldr::cem_encode_params& params,
	const astc_ldr::partition_pattern_vec* pPat,
	bool& improved_flag,
	bool gradient_descent_flag, bool polish_weights_flag, bool qcd_enabled_flag)
{
	improved_flag = false;

	if (!gradient_descent_flag && !polish_weights_flag && !qcd_enabled_flag)
		return true;

	const uint32_t grid_width = enc_log_block.m_grid_width, grid_height = enc_log_block.m_grid_height;
	const uint32_t cem_index = enc_log_block.m_color_endpoint_modes[0];
	const uint32_t num_subsets = enc_log_block.m_num_partitions;
	const bool dual_plane_flag = enc_log_block.m_dual_plane;
	//const uint32_t num_planes = dual_plane_flag ? 2 : 1;
	const int ccs_index = dual_plane_flag ? enc_log_block.m_color_component_selector : -1;

	const uint32_t endpoint_ise_range = enc_log_block.m_endpoint_ise_range;
	const uint32_t weight_ise_range = enc_log_block.m_weight_ise_range;

	const auto& dequant_tab = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_ISE_to_val;
	const auto& quant_tab = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_val_to_ise;

	const basist::astc_ldr_t::astc_block_grid_data *pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, grid_width, grid_height);

	//const bool is_downsampling = (grid_width < block_width) || (grid_height < block_height);

#if defined(_DEBUG) || defined(DEBUG)
	if (num_subsets > 1)
	{
		for (uint32_t i = 1; i < num_subsets; i++)
		{
			assert(enc_log_block.m_color_endpoint_modes[i] == cem_index);
		}
	}
#endif
		
	const uint32_t total_block_pixels = block_width * block_height;
	const uint32_t total_grid_pixels = grid_width * grid_height;

	uint64_t cur_err = eval_error(block_width, block_height, enc_log_block, pixel_stats, params);

	uint8_t weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint8_t weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	astc_helpers::extract_weights(enc_log_block, weights0, 0);

	if (dual_plane_flag)
		astc_helpers::extract_weights(enc_log_block, weights1, 1);

	const bool global_gradient_desc_enabled = true;
	const bool global_qcd_enabled = true;
	const bool global_polish_weights_enabled = true;
		
	// Gradient descent
	if ((gradient_descent_flag) && (global_gradient_desc_enabled))
	{
		// Downsample the residuals to grid res
				
		// First compute the block's ideal raw weights given the current endpoints at full block/texel res
		// TODO: Move to helper
		uint8_t ideal_block_raw_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], ideal_block_raw_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		if (num_subsets == 1)
		{
			if (dual_plane_flag)
				astc_ldr::eval_solution_dp(pixel_stats, cem_index, ccs_index, enc_log_block.m_endpoints, endpoint_ise_range, ideal_block_raw_weights0, ideal_block_raw_weights1, astc_helpers::BISE_64_LEVELS, params);
			else
				astc_ldr::eval_solution(pixel_stats, cem_index, enc_log_block.m_endpoints, endpoint_ise_range, ideal_block_raw_weights0, astc_helpers::BISE_64_LEVELS, params);
		}
		else
		{
			// Extract each subset's texels, compute the raw weights, place back into full res texel/block weight grid.
			color_rgba part_pixels[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
			uint32_t num_part_pixels[astc_helpers::MAX_PARTITIONS] = { 0 };

			for (uint32_t y = 0; y < block_height; y++)
			{
				for (uint32_t x = 0; x < block_width; x++)
				{
					const color_rgba& px = pixel_stats.m_pixels[x + y * block_width];

					const uint32_t part_index = (*pPat)(x, y);
					assert(part_index < num_subsets);

					// Sanity check
					assert(part_index == (uint32_t)astc_helpers::compute_texel_partition(enc_log_block.m_partition_id, x, y, 0, num_subsets, astc_helpers::is_small_block(block_width, block_height)));

					part_pixels[part_index][num_part_pixels[part_index]] = px;
					num_part_pixels[part_index]++;
				} // x
			} // y

			astc_ldr::pixel_stats_t part_pixel_stats[astc_helpers::MAX_PARTITIONS];

			for (uint32_t i = 0; i < num_subsets; i++)
				part_pixel_stats[i].clear();

			uint8_t part_raw_weights[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

			for (uint32_t part_index = 0; part_index < num_subsets; part_index++)
			{
				part_pixel_stats[part_index].init(num_part_pixels[part_index], &part_pixels[part_index][0]);

				const uint8_t* pPart_endpoints = astc_helpers::get_endpoints(enc_log_block, part_index);

				astc_ldr::eval_solution(part_pixel_stats[part_index], cem_index, pPart_endpoints, endpoint_ise_range, &part_raw_weights[part_index][0], astc_helpers::BISE_64_LEVELS, params);

			} // part_index

			clear_obj(num_part_pixels);

			for (uint32_t y = 0; y < block_height; y++)
			{
				for (uint32_t x = 0; x < block_width; x++)
				{
					const uint32_t part_index = (*pPat)(x, y);
					assert(part_index < num_subsets);

					ideal_block_raw_weights0[x + y * block_width] = part_raw_weights[part_index][num_part_pixels[part_index]];
					num_part_pixels[part_index]++;
				} // x
			} // y
		}

#if 1
		// Now compute the current block/texel res (upsampled) raw [0,64] weights given the current quantized grid weights. Dequant then upsample.
		// This is what an ASTC decoder would use during unpacking.
		uint8_t dequantized_grid_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], dequantized_grid_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
		uint8_t dequantized_block_weights_upsampled0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], dequantized_block_weights_upsampled1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		astc_ldr_requantize_astc_weights(total_grid_pixels, weights0, weight_ise_range, dequantized_grid_weights0, astc_helpers::BISE_64_LEVELS);

		if (dual_plane_flag)
			astc_ldr_requantize_astc_weights(total_grid_pixels, weights1, weight_ise_range, dequantized_grid_weights1, astc_helpers::BISE_64_LEVELS);

		astc_helpers::upsample_weight_grid(
			block_width, block_height,		 // destination/to dimension
			grid_width, grid_height,		 // source/from dimension
			dequantized_grid_weights0,			 // these are dequantized [0,64] weights, NOT ISE symbols, [wy][wx]
			dequantized_block_weights_upsampled0); // [by][bx]

		if (dual_plane_flag)
		{
			astc_helpers::upsample_weight_grid(
				block_width, block_height,		 // destination/to dimension
				grid_width, grid_height,		 // source/from dimension
				dequantized_grid_weights1,			 // these are dequantized [0,64] weights, NOT ISE symbols, [wy][wx]
				dequantized_block_weights_upsampled1); // [by][bx]
		}

		// Now compute residuals at the block res
		int weight_block_raw_residuals0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], weight_block_raw_residuals1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		for (uint32_t i = 0; i < total_block_pixels; i++)
			weight_block_raw_residuals0[i] = ideal_block_raw_weights0[i] - dequantized_block_weights_upsampled0[i];

		if (dual_plane_flag)
		{
			for (uint32_t i = 0; i < total_block_pixels; i++)
				weight_block_raw_residuals1[i] = ideal_block_raw_weights1[i] - dequantized_block_weights_upsampled1[i];
		}

		float weight_grid_residuals_downsampled0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], weight_grid_residuals_downsampled1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		const basisu::vector<float>& unweighted_downsample_matrix = pGrid_data->m_unweighted_downsample_matrix;
		const basisu::vector<float>& one_over_diag_AtA = pGrid_data->m_one_over_diag_AtA;

		downsample_weight_residual_grid(
			unweighted_downsample_matrix.get_ptr(),
			block_width, block_height,		// source/from dimension (block size)
			grid_width, grid_height,		// dest/to dimension (grid size)
			weight_block_raw_residuals0,	// these are dequantized weights, NOT ISE symbols, [by][bx]
			weight_grid_residuals_downsampled0);			// [wy][wx]

		for (uint32_t i = 0; i < total_grid_pixels; i++)
			weight_grid_residuals_downsampled0[i] *= one_over_diag_AtA[i];

		if (dual_plane_flag)
		{
			downsample_weight_residual_grid(
				unweighted_downsample_matrix.get_ptr(),
				block_width, block_height,		// source/from dimension (block size)
				grid_width, grid_height,		// dest/to dimension (grid size)
				weight_block_raw_residuals1,	// these are dequantized weights, NOT ISE symbols, [by][bx]
				weight_grid_residuals_downsampled1);			// [wy][wx]

			for (uint32_t i = 0; i < total_grid_pixels; i++)
				weight_grid_residuals_downsampled1[i] *= one_over_diag_AtA[i];
		}

		// Apply the residuals at grid res and quantize
		const float Q = 1.0f;

		uint8_t refined_grid_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], refined_grid_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		for (uint32_t i = 0; i < total_grid_pixels; i++)
		{
			float v = (float)dequant_tab[weights0[i]] + weight_grid_residuals_downsampled0[i] * Q;
			int iv = clamp((int)std::roundf(v), 0, 64);
			refined_grid_weights0[i] = quant_tab[iv];
		}

		if (dual_plane_flag)
		{
			for (uint32_t i = 0; i < total_grid_pixels; i++)
			{
				float v = (float)dequant_tab[weights1[i]] + weight_grid_residuals_downsampled1[i] * Q;
				int iv = clamp((int)std::roundf(v), 0, 64);
				refined_grid_weights1[i] = quant_tab[iv];
			}
		}
#else
		uint8_t refined_grid_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], refined_grid_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		for (uint32_t i = 0; i < total_grid_pixels; i++)
			refined_grid_weights0[i] = weights0[i];

		if (dual_plane_flag)
		{
			for (uint32_t i = 0; i < total_grid_pixels; i++)
				refined_grid_weights1[i] = weights1[i];
		}
#endif

		astc_helpers::log_astc_block refined_log_block(enc_log_block);

		// TODO: This refines both weight planes simultaneously, probably not optimal, could do individually.
		astc_helpers::set_weights(refined_log_block, refined_grid_weights0, 0);

		if (dual_plane_flag)
			astc_helpers::set_weights(refined_log_block, refined_grid_weights1, 1);

		uint64_t refined_err = eval_error(block_width, block_height, refined_log_block, pixel_stats, params);

		if (refined_err < cur_err)
		{
			cur_err = refined_err;

			memcpy(weights0, refined_grid_weights0, total_grid_pixels);

			if (dual_plane_flag)
				memcpy(weights1, refined_grid_weights1, total_grid_pixels);

			improved_flag = true;
		}

		// QCD - not a huge boost (.05-.75 dB), but on the toughest blocks it does help.
		if ((qcd_enabled_flag) && (global_qcd_enabled))
		{
			float ideal_block_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], ideal_block_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
			for (uint32_t i = 0; i < total_block_pixels; i++)
			{
				ideal_block_weights0[i] = (float)ideal_block_raw_weights0[i];

				if (dual_plane_flag)
					ideal_block_weights1[i] = (float)ideal_block_raw_weights1[i];
			}

			//const float* pUpsample_matrix = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, grid_width, grid_height)->m_upsample_matrix.get_ptr();
			const float* pUpsample_matrix = pGrid_data->m_upsample_matrix.get_ptr();

			qcd::qcd_min_solver solver;

			const uint32_t num_weight_levels = astc_helpers::get_ise_levels(weight_ise_range);

			assert(num_weight_levels <= 32);
			int labels[32 + 1];

			for (uint32_t i = 0; i < num_weight_levels; i++)
				labels[i] = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).get_rank_to_val(i);

			solver.init(pUpsample_matrix, total_block_pixels, total_grid_pixels, labels, num_weight_levels);

			int grid_idx0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], grid_idx1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

			const auto& ise_to_rank = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_ISE_to_rank;

			for (uint32_t i = 0; i < total_grid_pixels; i++)
			{
				grid_idx0[i] = ise_to_rank[refined_grid_weights0[i]];

				if (dual_plane_flag)
					grid_idx1[i] = ise_to_rank[refined_grid_weights1[i]];
			}

			float resid0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], resid1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

			solver.build_residual(grid_idx0, ideal_block_weights0, resid0);

			const uint32_t MAX_QCD_SWEEPS = 5;
			for (uint32_t t = 0; t < MAX_QCD_SWEEPS; t++)
			{
				int moved0 = solver.sweep(grid_idx0, resid0);
				if (!moved0)
					break;
			}

			if (dual_plane_flag)
			{
				solver.build_residual(grid_idx1, ideal_block_weights1, resid1);

				for (uint32_t t = 0; t < MAX_QCD_SWEEPS; t++)
				{
					int moved1 = solver.sweep(grid_idx1, resid1);
					if (!moved1)
						break;
				}
			}

			const auto& rank_to_ise = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_rank_to_ISE;

			for (uint32_t i = 0; i < total_grid_pixels; i++)
			{
				refined_grid_weights0[i] = rank_to_ise[grid_idx0[i]];

				if (dual_plane_flag)
					refined_grid_weights1[i] = rank_to_ise[grid_idx1[i]];
			}

			refined_log_block = enc_log_block;

			astc_helpers::set_weights(refined_log_block, refined_grid_weights0, 0);

			if (dual_plane_flag)
				astc_helpers::set_weights(refined_log_block, refined_grid_weights1, 1);

			refined_err = eval_error(block_width, block_height, refined_log_block, pixel_stats, params);

			if (refined_err < cur_err)
			{
				cur_err = refined_err;

				memcpy(weights0, refined_grid_weights0, total_grid_pixels);

				if (dual_plane_flag)
					memcpy(weights1, refined_grid_weights1, total_grid_pixels);

				improved_flag = true;
			}
		}
	} // if (qcd_enabled)

	if ((polish_weights_flag) && (global_polish_weights_enabled))
	{
#if BASISU_POLISH_DEBUG
		uint64_t cur_err_fast = cur_err;

		uint8_t weights0_fast[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
		uint8_t weights1_fast[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
		
		memcpy(weights0_fast, weights0, total_grid_pixels);
		if (dual_plane_flag)
			memcpy(weights1_fast, weights1, total_grid_pixels);

		bool improved_flag_fast = polish_block_weights_final_fast(
			enc_log_block,
			weights0_fast, weights1_fast,
			block_width, block_height, grid_width, grid_height,
			pixel_stats,
			params,
			pGrid_data,
			cur_err_fast);

		if (polish_block_weights_final_slow(
			enc_log_block,
			weights0, weights1, // the latest weights, will be updated if improved
			block_width, block_height, grid_width, grid_height, 
			pixel_stats,
			params,
			cur_err))
		{
			assert(improved_flag_fast);
			
			assert(cur_err == cur_err_fast);
			
			assert(memcmp(weights0, weights0_fast, total_grid_pixels) == 0);
			if (dual_plane_flag)
			{
				assert(memcmp(weights1, weights1_fast, total_grid_pixels) == 0);
			}

			improved_flag = true;
		}
		else
		{
			assert(!improved_flag_fast);
		}
#else
		if (polish_block_weights_final_fast(
			enc_log_block,
			weights0, weights1, // the latest weights, will be updated if improved
			block_width, block_height, grid_width, grid_height,
			pixel_stats,
			params,
			pGrid_data,
			cur_err))
		{
			improved_flag = true;
		}
#endif

	} // polish_flag

#if defined(_DEBUG) || defined(DEBUG)
	// sanity checking
	if (improved_flag)
	{
		uint64_t orig_err = eval_error(block_width, block_height, enc_log_block, pixel_stats, params);
		assert(cur_err < orig_err);
	}
#endif

	if (improved_flag)
	{
		astc_helpers::set_weights(enc_log_block, weights0, 0);

		if (dual_plane_flag)
			astc_helpers::set_weights(enc_log_block, weights1, 1);
	}

#if defined(_DEBUG) || defined(DEBUG)
	// sanity checking
	uint64_t new_err = eval_error(block_width, block_height, enc_log_block, pixel_stats, params);
	assert(cur_err == new_err);
#endif

	return true;
}

static bool encode_trial_subsets(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	uint32_t cem_index, uint32_t num_parts,
	uint32_t pat_seed_index, const astc_ldr::partition_pattern_vec* pPat, // seed index is a ASTC partition pattern index
	uint32_t endpoint_ise_range, uint32_t weight_ise_range,
	uint32_t grid_width, uint32_t grid_height,
	float early_out_thresh,
	astc_helpers::log_astc_block& enc_log_block,
	const astc_ldr::cem_encode_params& params,
	bool refine_only_flag = false,
	bool gradient_descent_flag = true, bool polish_weights_flag = true, bool qcd_enabled_flag = true,
	bool use_blue_contraction = true,
	bool* pTry_direct_encoding_flag = nullptr)
{
	assert((num_parts >= 2) && (num_parts <= astc_helpers::MAX_PARTITIONS));
	assert(pPat);
	assert(pat_seed_index < astc_helpers::NUM_PARTITION_PATTERNS);

	if (pTry_direct_encoding_flag)
		*pTry_direct_encoding_flag = false;

	const bool is_downsampling = (grid_width < block_width) || (grid_height < block_height);
	//const uint32_t total_block_pixels = block_width * block_height;
	const uint32_t total_grid_pixels = grid_width * grid_height;

	color_rgba part_pixels[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint32_t num_part_pixels[astc_helpers::MAX_PARTITIONS] = { 0 };

	for (uint32_t y = 0; y < block_height; y++)
	{
		for (uint32_t x = 0; x < block_width; x++)
		{
			const color_rgba& px = pixel_stats.m_pixels[x + y * block_width];

			const uint32_t part_index = (*pPat)(x, y);
			assert(part_index < num_parts);

			part_pixels[part_index][num_part_pixels[part_index]] = px;
			num_part_pixels[part_index]++;
		} // x
	} // y

#if defined(_DEBUG) || defined(DEBUG)
	for (uint32_t i = 0; i < num_parts; i++)
		assert(num_part_pixels[i]);
#endif

	astc_ldr::pixel_stats_t part_pixel_stats[astc_helpers::MAX_PARTITIONS];

	for (uint32_t i = 0; i < num_parts; i++)
		part_pixel_stats[i].clear();

	uint8_t part_endpoints[astc_helpers::MAX_PARTITIONS][astc_helpers::MAX_CEM_ENDPOINT_VALS];
	uint8_t part_weights[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	for (uint32_t part_index = 0; part_index < num_parts; part_index++)
	{
		part_pixel_stats[part_index].init(num_part_pixels[part_index], &part_pixels[part_index][0]);

		if (!refine_only_flag)
		{
			bool try_direct_encoding_flag = false;

			// Encode at block res, but with quantized weights
			uint64_t block_err = astc_ldr::cem_encode_pixels(cem_index, -1, part_pixel_stats[part_index], params,
				endpoint_ise_range, weight_ise_range,
				&part_endpoints[part_index][0], &part_weights[part_index][0], nullptr, UINT64_MAX, use_blue_contraction, &try_direct_encoding_flag);

			if (block_err == UINT64_MAX)
				return false;

			if ((pTry_direct_encoding_flag) && (try_direct_encoding_flag))
				*pTry_direct_encoding_flag = true;
		}

	} // part_index

	const uint32_t num_endpoint_vals = astc_helpers::get_num_cem_values(cem_index);

	if (!refine_only_flag)
	{
		uint8_t block_weights[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		clear_obj(num_part_pixels);

		for (uint32_t y = 0; y < block_height; y++)
		{
			for (uint32_t x = 0; x < block_width; x++)
			{
				const uint32_t part_index = (*pPat)(x, y);
				assert(part_index < num_parts);

				block_weights[x + y * block_width] = part_weights[part_index][num_part_pixels[part_index]];
				num_part_pixels[part_index]++;
			} // x
		} // y

		enc_log_block.clear();

		enc_log_block.m_grid_width = (uint8_t)grid_width;
		enc_log_block.m_grid_height = (uint8_t)grid_height;
		enc_log_block.m_weight_ise_range = (uint8_t)weight_ise_range;
		enc_log_block.m_endpoint_ise_range = (uint8_t)endpoint_ise_range;

		enc_log_block.m_num_partitions = (uint8_t)num_parts;
		for (uint32_t i = 0; i < num_parts; i++)
			enc_log_block.m_color_endpoint_modes[i] = (uint8_t)cem_index;
		enc_log_block.m_partition_id = (uint16_t)pat_seed_index;

		if (is_downsampling)
		{
			// TODO: Make the downsample step faster
			const float* pDownsample_matrix = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, grid_width, grid_height)->m_downsample_matrix.get_ptr();

			// Now downsample the weight grid (quantized to quantized)
			astc_ldr_downsample_ise_weights(
				weight_ise_range, weight_ise_range,
				block_width, block_height,
				grid_width, grid_height,
				block_weights, enc_log_block.m_weights,
				pDownsample_matrix);
		}
		else
		{
			memcpy(enc_log_block.m_weights, block_weights, total_grid_pixels);
		}

		for (uint32_t p = 0; p < num_parts; p++)
			memcpy(enc_log_block.m_endpoints + num_endpoint_vals * p, &part_endpoints[p][0], num_endpoint_vals);
	}

	uint64_t prev_cur_err = UINT64_MAX;

	// attempt endpoint refinement given the current weights
	// TODO: Expose to caller
	const uint32_t NUM_REFINEMENT_PASSES = 3;
	for (uint32_t refine_pass = 0; refine_pass < NUM_REFINEMENT_PASSES; refine_pass++)
	{
		uint64_t cur_err = eval_error(block_width, block_height, enc_log_block, pixel_stats, params);
		
		if (!cur_err)
			break;

		if ((early_out_thresh != 0.0f) && (refine_pass) && (prev_cur_err))
		{
			double percentage_improvement = (double)(prev_cur_err - cur_err) / (double)prev_cur_err;

			if (percentage_improvement < early_out_thresh)
				break;
		}

		prev_cur_err = cur_err;

		uint8_t dequantized_raw_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
		uint8_t upsampled_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS]; // raw weights, NOT ISE

		for (uint32_t i = 0; i < total_grid_pixels; i++)
			dequantized_raw_weights0[i] = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_ISE_to_val[enc_log_block.m_weights[i]];

		astc_helpers::upsample_weight_grid(block_width, block_height, grid_width, grid_height, dequantized_raw_weights0, upsampled_weights0);

		astc_helpers::log_astc_block alt_enc_log_block(enc_log_block);

		uint8_t raw_part_weights[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		clear_obj(num_part_pixels);

		for (uint32_t y = 0; y < block_height; y++)
		{
			for (uint32_t x = 0; x < block_width; x++)
			{
				const uint32_t part_index = (*pPat)(x, y);
				assert(part_index < num_parts);

				raw_part_weights[part_index][num_part_pixels[part_index]] = upsampled_weights0[x + y * block_width];
				num_part_pixels[part_index]++;
			} // x
		} // y

		for (uint32_t part_index = 0; part_index < num_parts; part_index++)
		{
			assert(num_part_pixels[part_index] == part_pixel_stats[part_index].m_num_pixels);

			astc_ldr::cem_encode_params temp_params(params);
			temp_params.m_pForced_weight_vals0 = &raw_part_weights[part_index][0];

			uint8_t temp_weights[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

			bool try_direct_encoding_flag = false;

			// Encode at block res, but with quantized weights
			uint64_t block_err = astc_ldr::cem_encode_pixels(cem_index, -1, part_pixel_stats[part_index], temp_params,
				endpoint_ise_range, astc_helpers::BISE_64_LEVELS,
				&alt_enc_log_block.m_endpoints[num_endpoint_vals * part_index], temp_weights, nullptr, UINT64_MAX, use_blue_contraction, &try_direct_encoding_flag);

			if (block_err == UINT64_MAX)
				return false;

			if ((pTry_direct_encoding_flag) && (try_direct_encoding_flag))
				*pTry_direct_encoding_flag = true;

#if defined(_DEBUG) || defined(DEBUG)
			for (uint32_t i = 0; i < part_pixel_stats[part_index].m_num_pixels; i++)
			{
				assert(temp_weights[i] == temp_params.m_pForced_weight_vals0[i]);
			}
#endif

		} // part_index
				
		uint64_t ref_err = eval_error(block_width, block_height, alt_enc_log_block, pixel_stats, params);

		if (ref_err < cur_err)
		{
			memcpy(&enc_log_block, &alt_enc_log_block, sizeof(astc_helpers::log_astc_block));
		}

		if (refine_pass == (NUM_REFINEMENT_PASSES - 1))
			break;

		if ((is_downsampling) && (gradient_descent_flag || polish_weights_flag))
		{
			bool improved_flag = false;
			bool status = polish_block_weights(block_width, block_height, pixel_stats, enc_log_block, params, pPat, improved_flag, gradient_descent_flag, polish_weights_flag, qcd_enabled_flag);
			if (!status)
			{
				assert(0);
			}

			if (!improved_flag)
				break;
		}
		else
		{
			break;
		}
	} // refine_pass

	return true;
}

static bool encode_trial(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	uint32_t cem_index,
	bool dual_plane_flag, int ccs_index,
	uint32_t endpoint_ise_range, uint32_t weight_ise_range,
	uint32_t grid_width, uint32_t grid_height,
	float early_out_thresh,
	astc_helpers::log_astc_block& enc_log_block,
	const astc_ldr::cem_encode_params& params,
	bool gradient_descent_flag = true, bool polish_weights_flag = true, bool qcd_enabled_flag = true,
	bool use_blue_contraction = true,
	bool* pTry_direct_encoding_flag = nullptr)
{
	assert(dual_plane_flag || (ccs_index == -1));

	if (pTry_direct_encoding_flag)
		*pTry_direct_encoding_flag = false;

	const bool is_downsampling = (grid_width < block_width) || (grid_height < block_height);
		
	const float* pDownsample_matrix = nullptr;
	if (is_downsampling)
	{
		const basist::astc_ldr_t::astc_block_grid_data* pBlock_grid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, grid_width, grid_height);
		pDownsample_matrix = pBlock_grid_data->m_downsample_matrix.get_ptr();
	}

	//const uint32_t total_block_pixels = block_width * block_height;
	const uint32_t total_grid_pixels = grid_width * grid_height;

	const auto& dequant_tab = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_ISE_to_val;
	//const auto& quant_tab = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_val_to_ise;

	enc_log_block.clear();

	enc_log_block.m_grid_width = (uint8_t)grid_width;
	enc_log_block.m_grid_height = (uint8_t)grid_height;
	enc_log_block.m_weight_ise_range = (uint8_t)weight_ise_range;
	enc_log_block.m_endpoint_ise_range = (uint8_t)endpoint_ise_range;

	enc_log_block.m_dual_plane = dual_plane_flag;
	if (dual_plane_flag)
	{
		assert((ccs_index >= 0) && (ccs_index <= 3));
		enc_log_block.m_color_component_selector = (uint8_t)ccs_index;
	}
	else
	{
		assert(ccs_index == -1);
	}

	enc_log_block.m_num_partitions = 1;
	enc_log_block.m_color_endpoint_modes[0] = (uint8_t)cem_index;

	uint8_t fullres_endpoints[astc_helpers::MAX_CEM_ENDPOINT_VALS];
	uint8_t weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint8_t weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	if ((grid_width == block_width) && (grid_height == block_height))
	{
		// No downsampling: a lot easier.
		bool try_direct_encoding_flag = false;

		uint64_t block_err = astc_ldr::cem_encode_pixels(cem_index, ccs_index, pixel_stats, params,
			endpoint_ise_range, weight_ise_range,
			fullres_endpoints, weights0, weights1, UINT64_MAX, use_blue_contraction, &try_direct_encoding_flag);

		if (block_err == UINT64_MAX)
			return false;

		if ((pTry_direct_encoding_flag) && (try_direct_encoding_flag))
			*pTry_direct_encoding_flag = try_direct_encoding_flag;

		if (dual_plane_flag)
		{
			for (uint32_t i = 0; i < total_grid_pixels; i++)
			{
				enc_log_block.m_weights[i * 2 + 0] = weights0[i];
				enc_log_block.m_weights[i * 2 + 1] = weights1[i];
			}
		}
		else
		{
			memcpy(enc_log_block.m_weights, weights0, total_grid_pixels);
		}

		memcpy(enc_log_block.m_endpoints, fullres_endpoints, astc_helpers::get_num_cem_values(cem_index));

		return true;
	}

	// Handle downsampled weight grids case

	uint8_t fullres_raw_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint8_t fullres_raw_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	bool try_direct_encoding_flag = false;

	// Encode at block res, but with quantized weights
	uint64_t block_err = astc_ldr::cem_encode_pixels(cem_index, ccs_index, pixel_stats, params,
		endpoint_ise_range, weight_ise_range,
		fullres_endpoints, fullres_raw_weights0, fullres_raw_weights1, UINT64_MAX, use_blue_contraction, &try_direct_encoding_flag);

	if (block_err == UINT64_MAX)
		return false;

	if ((pTry_direct_encoding_flag) && (try_direct_encoding_flag))
		*pTry_direct_encoding_flag = try_direct_encoding_flag;

	// Now downsample the weight grid (quantized to quantized)
	astc_ldr_downsample_ise_weights(
		weight_ise_range, weight_ise_range,
		block_width, block_height,
		grid_width, grid_height,
		fullres_raw_weights0, weights0,
		pDownsample_matrix);

	astc_helpers::set_weights(enc_log_block, weights0, 0);

	if (dual_plane_flag)
	{
		astc_ldr_downsample_ise_weights(
			weight_ise_range, weight_ise_range,
			block_width, block_height,
			grid_width, grid_height,
			fullres_raw_weights1, weights1,
			pDownsample_matrix);
	}

	if (dual_plane_flag)
		astc_helpers::set_weights(enc_log_block, weights1, 1);

	memcpy(enc_log_block.m_endpoints, fullres_endpoints, astc_helpers::get_num_cem_values(cem_index));

	uint64_t prev_cur_err = UINT64_MAX;
		
	const uint32_t NUM_OUTER_PASSES = 3;
	for (uint32_t outer_pass = 0; outer_pass < NUM_OUTER_PASSES; outer_pass++)
	{
		uint64_t cur_err = eval_error(
			block_width, block_height,
			pixel_stats,
			cem_index,
			dual_plane_flag, ccs_index,
			endpoint_ise_range, weight_ise_range,
			grid_width, grid_height,
			enc_log_block.m_endpoints, weights0, weights1,
			params);

		if (!cur_err)
			break;

		if ((early_out_thresh != 0.0f) && (outer_pass) && (prev_cur_err))
		{
			double percentage_improvement = (double)(prev_cur_err - cur_err) / (double)prev_cur_err;

			if (percentage_improvement < early_out_thresh)
				break;
		}

		prev_cur_err = cur_err;

		// endpoint refinement, given current upsampled weights
		{
			astc_helpers::extract_weights(enc_log_block, weights0, 0);

			if (dual_plane_flag)
				astc_helpers::extract_weights(enc_log_block, weights1, 1);

			// Plane 0
			uint8_t dequantized_raw_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
			uint8_t upsampled_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS]; // raw weights, NOT ISE

			for (uint32_t i = 0; i < total_grid_pixels; i++)
				dequantized_raw_weights0[i] = dequant_tab[weights0[i]];

			astc_helpers::upsample_weight_grid(block_width, block_height, grid_width, grid_height, dequantized_raw_weights0, upsampled_weights0);

			// Plane 1
			uint8_t dequantized_raw_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
			uint8_t upsampled_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS]; // raw weights, NOT ISE

			if (dual_plane_flag)
			{
				for (uint32_t i = 0; i < total_grid_pixels; i++)
					dequantized_raw_weights1[i] = dequant_tab[weights1[i]];
				astc_helpers::upsample_weight_grid(block_width, block_height, grid_width, grid_height, dequantized_raw_weights1, upsampled_weights1);
			}

			// Jam in the actual raw [0,64] weights the decoder is going to use after upsampling the grid.
			astc_ldr::cem_encode_params refine_params(params);
			refine_params.m_pForced_weight_vals0 = upsampled_weights0;
			if (dual_plane_flag)
				refine_params.m_pForced_weight_vals1 = upsampled_weights1;

			uint8_t refined_endpoints[astc_helpers::MAX_CEM_ENDPOINT_VALS];
			uint8_t refined_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
			uint8_t refined_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

			uint64_t refined_block_err = astc_ldr::cem_encode_pixels(cem_index, ccs_index, pixel_stats, refine_params,
				endpoint_ise_range, astc_helpers::BISE_64_LEVELS,
				refined_endpoints, refined_weights0, refined_weights1, UINT64_MAX, use_blue_contraction, &try_direct_encoding_flag);
									
			if (refined_block_err != UINT64_MAX)
			{
				if (refined_block_err < cur_err)
				{
					memcpy(enc_log_block.m_endpoints, refined_endpoints, astc_helpers::get_num_cem_values(cem_index));

					if ((pTry_direct_encoding_flag) && (try_direct_encoding_flag))
						*pTry_direct_encoding_flag = try_direct_encoding_flag;
				}
			}
		}

		if ( (outer_pass == (NUM_OUTER_PASSES - 1)) || 
			((!gradient_descent_flag) && (!polish_weights_flag)) )
			break;

		bool improved_flag = false;

		bool status = polish_block_weights(
			block_width, block_height,
			pixel_stats,
			enc_log_block, // assumes there is already a good encoding to improve here
			params,
			nullptr,
			improved_flag,
			gradient_descent_flag,
			polish_weights_flag,
			qcd_enabled_flag);

		if (!status)
		{
			assert(0);
			return false;
		}

		if (!improved_flag)
			break;

	} // outer_pass

	return true;
}

// 1 subset only, refines endpoints given current weights
static bool encode_trial_refine_only(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	astc_helpers::log_astc_block& enc_log_block,
	const astc_ldr::cem_encode_params& params,
	bool use_blue_contraction = true,
	bool* pTry_direct_encoding_flag = nullptr)
{
	assert(enc_log_block.m_num_partitions == 1);

	if (pTry_direct_encoding_flag)
		*pTry_direct_encoding_flag = false;

	const uint32_t cem_index = enc_log_block.m_color_endpoint_modes[0];
	const bool dual_plane_flag = enc_log_block.m_dual_plane;
	const int ccs_index = dual_plane_flag ? enc_log_block.m_color_component_selector : -1;
	const uint32_t endpoint_ise_range = enc_log_block.m_endpoint_ise_range;
	const uint32_t weight_ise_range = enc_log_block.m_weight_ise_range;
	const uint32_t grid_width = enc_log_block.m_grid_width;
	const uint32_t grid_height = enc_log_block.m_grid_height;

	//const bool is_downsampling = (grid_width < block_width) || (grid_height < block_height);

	//const uint32_t total_block_pixels = block_width * block_height;
	const uint32_t total_grid_pixels = grid_width * grid_height;

	uint8_t dequantized_raw_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint8_t upsampled_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS]; // raw weights, NOT ISE

	for (uint32_t i = 0; i < total_grid_pixels; i++)
		dequantized_raw_weights0[i] = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_ISE_to_val[astc_helpers::get_weight(enc_log_block, 0, i)];

	// suppress bogus gcc warning on dequantized_raw_weights0
#ifndef __clang__
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif

	astc_helpers::upsample_weight_grid(block_width, block_height, grid_width, grid_height, dequantized_raw_weights0, upsampled_weights0);

#ifndef __clang__
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#endif

	uint8_t dequantized_raw_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint8_t upsampled_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS]; // raw weights, NOT ISE

	if (dual_plane_flag)
	{
		for (uint32_t i = 0; i < total_grid_pixels; i++)
			dequantized_raw_weights1[i] = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_ISE_to_val[astc_helpers::get_weight(enc_log_block, 1, i)];
		astc_helpers::upsample_weight_grid(block_width, block_height, grid_width, grid_height, dequantized_raw_weights1, upsampled_weights1);
	}

	astc_ldr::cem_encode_params refine_params(params);
	refine_params.m_pForced_weight_vals0 = upsampled_weights0;
	if (dual_plane_flag)
		refine_params.m_pForced_weight_vals1 = upsampled_weights1;

	uint8_t refined_endpoints[astc_helpers::MAX_CEM_ENDPOINT_VALS];
	uint8_t refined_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint8_t refined_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	//bool use_blue_contraction = true;

	bool try_direct_encoding_flag = false;

	uint64_t refined_block_err = astc_ldr::cem_encode_pixels(cem_index, ccs_index, pixel_stats, refine_params,
		endpoint_ise_range, astc_helpers::BISE_64_LEVELS,
		refined_endpoints, refined_weights0, refined_weights1, UINT64_MAX, use_blue_contraction, &try_direct_encoding_flag);
	assert(refined_block_err != UINT64_MAX);

	if ((pTry_direct_encoding_flag) && (try_direct_encoding_flag))
		*pTry_direct_encoding_flag = try_direct_encoding_flag;

#if defined(_DEBUG) || defined(DEBUG)
	for (uint32_t i = 0; i < total_grid_pixels; i++)
	{
		assert(refined_weights0[i] == upsampled_weights0[i]);

		if (dual_plane_flag)
		{
			assert(refined_weights1[i] == upsampled_weights1[i]);
		}
	}
#endif

	if (refined_block_err != UINT64_MAX)
	{
		astc_helpers::log_astc_block alt_enc_log_block(enc_log_block);
		memcpy(alt_enc_log_block.m_endpoints, refined_endpoints, astc_helpers::get_num_cem_values(cem_index));

#if defined(_DEBUG) || defined(DEBUG)
		// refined_block_err was computed on the actual ASTC [0,64] upsampled weights the decoder would use. But double check this for sanity.
		{
			uint64_t ref_err = eval_error(block_width, block_height, alt_enc_log_block, pixel_stats, params);
			assert(ref_err == refined_block_err);
		}
#endif

		uint64_t cur_err = eval_error(block_width, block_height, enc_log_block, pixel_stats, params);

		if (refined_block_err < cur_err)
		{
			memcpy(enc_log_block.m_endpoints, refined_endpoints, astc_helpers::get_num_cem_values(cem_index));
		}
	}

	return true;
}

struct log_surrogate_astc_blk
{
	// Important: If not downsampling, grid width/height may match the block width/height and may not be valid ASTC (which has a limit of 64 weight samples).
	int m_grid_width, m_grid_height;

	uint32_t m_cem_index; // base+scale or direct variants only
	int m_ccs_index; // -1 for single plane

	uint32_t m_num_endpoint_levels;
	uint32_t m_num_weight_levels;

	uint32_t m_num_parts; // 1-3
	uint32_t m_seed_index; // ASTC seed index, 10-bits if m_num_parts > 1

	vec4F m_endpoints[astc_helpers::MAX_PARTITIONS][2]; // [subset_index][l/h endpoint]
	float m_scales[astc_helpers::MAX_PARTITIONS]; // scale factor used for each subset

	float m_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	float m_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	void clear()
	{
		memset((void *)this, 0, sizeof(*this));
	}

	void decode(uint32_t block_width, uint32_t block_height, vec4F* pPixels, const astc_ldr::partition_pattern_vec* pPat) const;
	void decode(uint32_t block_width, uint32_t block_height, vec4F* pPixels, const astc_ldr::partitions_data* pPat_data) const;
};

static void upsample_surrogate_weights(
	const astc_helpers::weighted_sample* pWeighted_samples,
	const float* pSrc_weights,
	float* pDst_weights,
	uint32_t by, uint32_t bx,
	uint32_t wx, uint32_t wy,
	uint32_t num_weight_levels)
{
	const uint32_t total_src_weights = wx * wy;
	const float weight_levels_minus_1 = (float)(num_weight_levels - 1) * (1.0f / 16.0f);
	const float inv_weight_levels = 1.0f / (float)(num_weight_levels - 1);

	const astc_helpers::weighted_sample* pS = pWeighted_samples;

	for (uint32_t y = 0; y < by; y++)
	{
		for (uint32_t x = 0; x < bx; x++, ++pS)
		{
			const uint32_t w00 = pS->m_weights[0][0];
			const uint32_t w01 = pS->m_weights[0][1];
			const uint32_t w10 = pS->m_weights[1][0];
			const uint32_t w11 = pS->m_weights[1][1];

			assert(w00 || w01 || w10 || w11);

			const uint32_t sx = pS->m_src_x, sy = pS->m_src_y;

			float total = 0.0f;
						
			if (w00) total += pSrc_weights[bounds_check(sx + sy * wx, 0U, total_src_weights)] * (float)w00;
			if (w01) total += pSrc_weights[bounds_check(sx + 1 + sy * wx, 0U, total_src_weights)] * (float)w01;
			if (w10) total += pSrc_weights[bounds_check(sx + (sy + 1) * wx, 0U, total_src_weights)] * (float)w10;
			if (w11) total += pSrc_weights[bounds_check(sx + 1 + (sy + 1) * wx, 0U, total_src_weights)] * (float)w11;

			float w = (float)fast_roundf_pos_int(total * weight_levels_minus_1) * inv_weight_levels;

			pDst_weights[x + y * bx] = w;
		} // x
	} // y
}

void log_surrogate_astc_blk::decode(uint32_t block_width, uint32_t block_height, vec4F* pPixels, const astc_ldr::partition_pattern_vec* pPat) const
{
	const bool dual_plane = (m_ccs_index >= 0);

	const uint32_t total_block_pixels = block_width * block_height;
	const uint32_t total_grid_pixels = m_grid_width * m_grid_height;

	const bool needs_upsampling = total_grid_pixels < total_block_pixels;

	const bool is_small_block = total_block_pixels < 31; // astc_helpers::is_small_block(block_width, block_height);
	BASISU_NOTE_UNUSED(is_small_block);

	float upsampled_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], upsampled_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	const float* pWeights0 = m_weights0;
	const float* pWeights1 = m_weights1;

	if (needs_upsampling)
	{
		const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, m_grid_width, m_grid_height);
		const astc_helpers::weighted_sample* pUp_weights = pGrid_data->m_upsample_weights.get_ptr();

		upsample_surrogate_weights(pUp_weights, m_weights0, upsampled_weights0, block_width, block_height, m_grid_width, m_grid_height, m_num_weight_levels);
		pWeights0 = upsampled_weights0;

		if (dual_plane)
		{
			upsample_surrogate_weights(pUp_weights, m_weights1, upsampled_weights1, block_width, block_height, m_grid_width, m_grid_height, m_num_weight_levels);
			pWeights1 = upsampled_weights1;
		}
	}

	for (uint32_t y = 0; y < block_height; y++)
	{
		for (uint32_t x = 0; x < block_width; x++)
		{
			uint32_t part_index = 0;
			if (m_num_parts > 1)
			{
				part_index = (*pPat)(x, y);
				assert(part_index < m_num_parts);

				assert(part_index == (uint32_t)astc_helpers::compute_texel_partition(m_seed_index, x, y, 0, m_num_parts, is_small_block));
			}

			const vec4F& l = m_endpoints[part_index][0];
			const vec4F& h = m_endpoints[part_index][1];

			vec4F& dst = pPixels[x + y * block_width];

			for (uint32_t c = 0; c < 4; c++)
			{
				float w = ((int)c == m_ccs_index) ? pWeights1[x + y * block_width] : pWeights0[x + y * block_width];

				//dst[c] = lerp(l[c], h[c], w);

				const float one_minus_w = 1.0f - w;
				dst[c] = l[c] * one_minus_w + h[c] * w;
			} // c

		} // x
	} // y
}

void log_surrogate_astc_blk::decode(uint32_t block_width, uint32_t block_height, vec4F* pPixels, const astc_ldr::partitions_data* pPat_data) const
{
	if (m_num_parts == 1)
		return decode(block_width, block_height, pPixels, (const astc_ldr::partition_pattern_vec*)nullptr);

	uint32_t unique_pat_index = pPat_data->m_part_seed_to_unique_index[m_seed_index];
	assert(unique_pat_index < pPat_data->m_total_unique_patterns);

	return decode(block_width, block_height, pPixels, &pPat_data->m_partition_pats[unique_pat_index]);
}

static void downsample_float_weight_grid(
	const float* pMatrix_weights,
	uint32_t bx, uint32_t by,		// source/from dimension (block size)
	uint32_t wx, uint32_t wy,		// dest/to dimension (grid size)
	const float* pSrc_weights,		// these are dequantized weights, NOT ISE symbols, [by][bx]
	float* pDst_weights,			// [wy][wx]
	uint32_t num_weight_levels)
{
	const uint32_t total_block_samples = bx * by;
	const float weight_levels_minus_1 = (float)(num_weight_levels - 1);
	const float inv_weight_levels = 1.0f / (float)(num_weight_levels - 1);

	for (uint32_t y = 0; y < wy; y++)
	{
		for (uint32_t x = 0; x < wx; x++)
		{
			float total = 0.0f;

			// TODO - optimize!
			for (uint32_t i = 0; i < total_block_samples; i++)
				if (pMatrix_weights[i])
					total += pMatrix_weights[i] * (float)pSrc_weights[i];

			pDst_weights[x + y * wx] = (float)fast_roundf_pos_int(total * weight_levels_minus_1) * inv_weight_levels;

			pMatrix_weights += total_block_samples;
		}
	}
}

static float decode_surrogate_and_compute_error(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	log_surrogate_astc_blk& log_block,
	const astc_ldr::partition_pattern_vec* pPat,
	const astc_ldr::cem_encode_params& params)
{
	vec4F dec_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	log_block.decode(block_width, block_height, dec_pixels, pPat);

	const float wr = (float)params.m_comp_weights[0];
	const float wg = (float)params.m_comp_weights[1];
	const float wb = (float)params.m_comp_weights[2];
	const float wa = (float)params.m_comp_weights[3];

#if 0
	float total_err = 0.0f;
	for (uint32_t by = 0; by < block_height; by++)
	{
		for (uint32_t bx = 0; bx < block_width; bx++)
		{
			const vec4F& s = pixel_stats.m_pixels_f[bx + by * block_width];
			const vec4F& d = dec_pixels[bx + by * block_width];

			float dr = s[0] - d[0];
			float dg = s[1] - d[1];
			float db = s[2] - d[2];
			float da = s[3] - d[3];

			total_err += (wr * dr * dr) + (wg * dg * dg) + (wb * db * db) + (wa * da * da);
		} // bx

	} // by
#else
	float total_err_alt = 0.0f;
	
	const uint32_t total_texels = block_width * block_height;

	if ((wr == 1.0f) && (wg == 1.0f) && (wb == 1.0f) && (wa == 1.0f))
	{
		for (uint32_t i = 0; i < total_texels; i++)
		{
			const vec4F& s = pixel_stats.m_pixels_f[i];
			const vec4F& d = dec_pixels[i];

			float dr = s[0] - d[0];
			float dg = s[1] - d[1];
			float db = s[2] - d[2];
			float da = s[3] - d[3];

			total_err_alt += (dr * dr) + (dg * dg) + (db * db) + (da * da);
		} // i
	}
	else
	{
		for (uint32_t i = 0; i < total_texels; i++)
		{
			const vec4F& s = pixel_stats.m_pixels_f[i];
			const vec4F& d = dec_pixels[i];

			float dr = s[0] - d[0];
			float dg = s[1] - d[1];
			float db = s[2] - d[2];
			float da = s[3] - d[3];

			total_err_alt += (wr * dr * dr) + (wg * dg * dg) + (wb * db * db) + (wa * da * da);
		} // i
	}

	//assert(equal_tol(total_err, total_err_alt, .000125f));
	float total_err = total_err_alt;
#endif

	return total_err;
}

// Returns WSSE error
static float encode_surrogate_trial(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	uint32_t cem_index,
	int ccs_index,
	uint32_t endpoint_ise_range, uint32_t weight_ise_range,
	uint32_t grid_width, uint32_t grid_height,
	log_surrogate_astc_blk& log_block,
	const astc_ldr::cem_encode_params& params,
	uint32_t flags)
{
	const bool is_downsampling = (grid_width < block_width) || (grid_height < block_height);
	const bool dual_plane_flag = (ccs_index >= 0);
		
	const float* pDownsample_matrix = nullptr;
	if (is_downsampling)
	{
		const basist::astc_ldr_t::astc_block_grid_data* pBlock_grid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, grid_width, grid_height);
		pDownsample_matrix = pBlock_grid_data->m_downsample_matrix.get_ptr();
	}

	//const uint32_t total_block_pixels = block_width * block_height;
	//const uint32_t total_grid_pixels = grid_width * grid_height;

	log_block.m_cem_index = cem_index;
	log_block.m_ccs_index = ccs_index;
	log_block.m_grid_width = grid_width;
	log_block.m_grid_height = grid_height;
	log_block.m_num_parts = 1;
	log_block.m_seed_index = 0;
	clear_obj(log_block.m_scales);
	log_block.m_num_endpoint_levels = astc_helpers::get_ise_levels(endpoint_ise_range);
	log_block.m_num_weight_levels = astc_helpers::get_ise_levels(weight_ise_range);

	float wsse_err = 0.0f;

	if (is_downsampling)
	{
		float temp_weights0[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], temp_weights1[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

		astc_ldr::cem_surrogate_encode_pixels(
			cem_index, ccs_index,
			pixel_stats, params,
			endpoint_ise_range, weight_ise_range,
			log_block.m_endpoints[0][0], log_block.m_endpoints[0][1], log_block.m_scales[0], temp_weights0, temp_weights1,
			flags);

		downsample_float_weight_grid(
			pDownsample_matrix,
			block_width, block_height,
			grid_width, grid_height,
			temp_weights0,
			log_block.m_weights0,
			log_block.m_num_weight_levels);

		if (dual_plane_flag)
		{
			downsample_float_weight_grid(
				pDownsample_matrix,
				block_width, block_height,
				grid_width, grid_height,
				temp_weights1,
				log_block.m_weights1,
				log_block.m_num_weight_levels);
		}

		wsse_err = decode_surrogate_and_compute_error(block_width, block_height, pixel_stats, log_block, nullptr, params);
	}
	else
	{
		wsse_err = astc_ldr::cem_surrogate_encode_pixels(
			cem_index, ccs_index,
			pixel_stats, params,
			endpoint_ise_range, weight_ise_range,
			log_block.m_endpoints[0][0], log_block.m_endpoints[0][1], log_block.m_scales[0], log_block.m_weights0, log_block.m_weights1,
			flags);

#if 0
#if defined(_DEBUG) || defined(DEBUG)
		{
			float alt_wsse_err = decode_surrogate_and_compute_error(block_width, block_height, pixel_stats, log_block, nullptr, params);
			//assert(fabs(wsse_err - alt_wsse_err) < .125f);
			assert(basisu::equal_rel_tol(wsse_err, alt_wsse_err, .0125f));
		}
#endif
#endif
	}

	return wsse_err;
}

static float encode_surrogate_trial_subsets(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	uint32_t cem_index,
	uint32_t num_subsets, uint32_t pat_seed_index, const astc_ldr::partition_pattern_vec* pPat,
	uint32_t endpoint_ise_range, uint32_t weight_ise_range,
	uint32_t grid_width, uint32_t grid_height,
	log_surrogate_astc_blk& log_block,
	const astc_ldr::cem_encode_params& params,
	uint32_t flags)
{
	assert((num_subsets >= 2) && (num_subsets <= astc_helpers::MAX_PARTITIONS));

	const bool is_downsampling = (grid_width < block_width) || (grid_height < block_height);
	//const uint32_t total_block_pixels = block_width * block_height;
	//const uint32_t total_grid_pixels = grid_width * grid_height;

	const uint32_t num_weight_levels = astc_helpers::get_ise_levels(weight_ise_range);
	const uint32_t num_endpoint_levels = astc_helpers::get_ise_levels(endpoint_ise_range);
		
	const float* pDownsample_matrix = nullptr;
	if (is_downsampling)
	{
		const basist::astc_ldr_t::astc_block_grid_data* pBlock_grid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, grid_width, grid_height);
		pDownsample_matrix = pBlock_grid_data->m_downsample_matrix.get_ptr();
	}

	color_rgba part_pixels[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint32_t num_part_pixels[astc_helpers::MAX_PARTITIONS] = { 0 };

	for (uint32_t y = 0; y < block_height; y++)
	{
		for (uint32_t x = 0; x < block_width; x++)
		{
			const color_rgba& px = pixel_stats.m_pixels[x + y * block_width];

			const uint32_t part_index = (*pPat)(x, y);
			assert(part_index < num_subsets);

			part_pixels[part_index][num_part_pixels[part_index]] = px;
			num_part_pixels[part_index]++;
		} // x
	} // y

#if defined(_DEBUG) || defined(DEBUG)
	for (uint32_t i = 0; i < num_subsets; i++)
		assert(num_part_pixels[i] > 0);
#endif

	astc_ldr::pixel_stats_t part_pixel_stats[astc_helpers::MAX_PARTITIONS];

	for (uint32_t i = 0; i < num_subsets; i++)
		part_pixel_stats[i].clear();

	float part_weights[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	float temp_block_weights[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

	double total_subset_err = 0.0f;
	for (uint32_t part_index = 0; part_index < num_subsets; part_index++)
	{
		part_pixel_stats[part_index].init(num_part_pixels[part_index], &part_pixels[part_index][0]);

		float subset_err = astc_ldr::cem_surrogate_encode_pixels(
			cem_index, -1,
			part_pixel_stats[part_index], params,
			endpoint_ise_range, weight_ise_range,
			log_block.m_endpoints[part_index][0], log_block.m_endpoints[part_index][1],
			log_block.m_scales[part_index], part_weights[part_index], temp_block_weights,
			flags);

		total_subset_err += subset_err;

	} // part_index

	float* pDst_weights = is_downsampling ? temp_block_weights : log_block.m_weights0;

	clear_obj(num_part_pixels);

	for (uint32_t y = 0; y < block_height; y++)
	{
		for (uint32_t x = 0; x < block_width; x++)
		{
			const uint32_t part_index = (*pPat)(x, y);
			assert(part_index < num_subsets);

			pDst_weights[x + y * block_width] = part_weights[part_index][num_part_pixels[part_index]];
			num_part_pixels[part_index]++;
		} // x
	} // y

	log_block.m_cem_index = cem_index;
	log_block.m_ccs_index = -1;
	log_block.m_num_endpoint_levels = num_endpoint_levels;
	log_block.m_num_weight_levels = num_weight_levels;
	log_block.m_grid_width = grid_width;
	log_block.m_grid_height = grid_height;
	log_block.m_num_parts = num_subsets;
	log_block.m_seed_index = pat_seed_index;

	if (is_downsampling)
	{
		downsample_float_weight_grid(
			pDownsample_matrix,
			block_width, block_height,
			grid_width, grid_height,
			temp_block_weights,
			log_block.m_weights0,
			astc_helpers::get_ise_levels(weight_ise_range));

		total_subset_err = decode_surrogate_and_compute_error(block_width, block_height, pixel_stats, log_block, pPat, params);
	}

#if 0
#if defined(_DEBUG) || defined(DEBUG)
	if (!is_downsampling)
	{
		float alt_subset_err = decode_surrogate_and_compute_error(block_width, block_height, pixel_stats, log_block, pPat, params);

		//assert(fabs(total_subset_err - alt_subset_err) < .00125f);
		assert(basisu::equal_rel_tol(total_subset_err, (double)alt_subset_err, (double).0125f));
	}
#endif
#endif

	return (float)total_subset_err;
}

#if 0
static inline vec4F vec4F_norm_approx(vec4F axis)
{
	float l = axis.norm();
	axis = (fabs(l) >= SMALL_FLOAT_VAL) ? (axis * bu_math::inv_sqrt(l)) : vec4F(.5f);
	return axis;
}
#endif

// if cov[] wasn't divided by the # of pixels, this is SSE
static inline float estimate_slam_to_line_sse_3D(const float cov[6], float xr, float yr, float zr)
{
	// total var
	const float total_var = cov[0] + cov[3] + cov[5];

	float l = sqrtf(xr * xr + yr * yr + zr * zr);
	if (l < basisu::SMALL_FLOAT_VAL)
	{
		xr = yr = zr = 0.577350269f;
	}
	else
	{
		l = 1.0f / l;
		xr *= l; yr *= l; zr *= l;
	}

	float xr2 = cov[0] * xr + cov[1] * yr + cov[2] * zr;
	float xg2 = cov[1] * xr + cov[3] * yr + cov[4] * zr;
	float xb2 = cov[2] * xr + cov[4] * yr + cov[5] * zr;

	// Rayleigh quotient/est var of principal axis
	const float principal_axis_var = xr2 * xr + xg2 * yr + xb2 * zr;

	// Compute leftover var, this is the var unexplaind by the principal axis
	const float ortho_var = basisu::maximum(0.0f, total_var - principal_axis_var);

	return ortho_var;
}

static inline float estimate_slam_to_line_sse_4D(const float cov[10], float xr, float yr, float zr, float wr)
{
	// total var
	const float total_var = cov[0] + cov[4] + cov[7] + cov[9];

	float l = sqrtf(xr * xr + yr * yr + zr * zr + wr * wr);
	if (l < basisu::SMALL_FLOAT_VAL)
	{
		xr = yr = zr = wr = .5f;
	}
	else
	{
		l = 1.0f / l;
		xr *= l; yr *= l; zr *= l; wr *= l;
	}

	float xr2 = cov[0] * xr + cov[1] * yr + cov[2] * zr + cov[3] * wr;
	float xg2 = cov[1] * xr + cov[4] * yr + cov[5] * zr + cov[6] * wr;
	float xb2 = cov[2] * xr + cov[5] * yr + cov[7] * zr + cov[8] * wr;
	float xa2 = cov[3] * xr + cov[6] * yr + cov[8] * zr + cov[9] * wr;

	// Rayleigh quotient/est var of principal axis
	const float principal_axis_var = xr2 * xr + xg2 * yr + xb2 * zr + xa2 * wr;

	// Compute leftover var, this is the var unexplaind by the principal axis
	const float ortho_var = basisu::maximum(0.0f, total_var - principal_axis_var);

	return ortho_var;
}

static float estimate_partition_rgb_sse(
	const astc_ldr::pixel_stats_t& pixel_stats,
	bool base_scale_flag,
	uint32_t block_width, uint32_t block_height,
	uint32_t num_subsets,
	const astc_ldr::partition_pattern_vec* pPat)
{
	assert((num_subsets >= 2) && (num_subsets <= astc_helpers::MAX_PARTITIONS));

	color_rgba part_pixels[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint32_t num_part_pixels[astc_helpers::MAX_PARTITIONS] = { 0 };

	int part_means[astc_helpers::MAX_PARTITIONS][3];
	clear_obj(part_means);

	const uint32_t total_block_pixels = block_width * block_height;
	
	// TODO: A moment-based approach could do both RGB direct and base+scale simultaneously.

	if (base_scale_flag)
	{
		for (uint32_t i = 0; i < total_block_pixels; i++)
		{
			const color_rgba& px = pixel_stats.m_pixels[i];

			const uint32_t part_index = (*pPat)[i];
			assert(part_index < num_subsets);

			part_pixels[part_index][num_part_pixels[part_index]] = px;

			num_part_pixels[part_index]++;

		} // i
	}
	else
	{
		for (uint32_t i = 0; i < total_block_pixels; i++)
		{
			const color_rgba& px = pixel_stats.m_pixels[i];

			const uint32_t part_index = (*pPat)[i];
			assert(part_index < num_subsets);

			part_pixels[part_index][num_part_pixels[part_index]] = px;

			part_means[part_index][0] += px.r;
			part_means[part_index][1] += px.g;
			part_means[part_index][2] += px.b;

			num_part_pixels[part_index]++;

		} // i

		for (uint32_t i = 0; i < num_subsets; i++)
		{
			assert(num_part_pixels[i] > 0);

			const int n = num_part_pixels[i];
			const int r = n >> 1;

			part_means[i][0] = (part_means[i][0] + r) / n;
			part_means[i][1] = (part_means[i][1] + r) / n;
			part_means[i][2] = (part_means[i][2] + r) / n;
		} // i
	}

	// rr, rg, rb, gg, gb, bb
	int part_icov[astc_helpers::MAX_PARTITIONS][6];
	clear_obj(part_icov);

	for (uint32_t p = 0; p < num_subsets; p++)
	{
		const uint32_t np = num_part_pixels[p];

		const int mean_r = part_means[p][0];
		const int mean_g = part_means[p][1];
		const int mean_b = part_means[p][2];

		int* pCov = &part_icov[p][0];

		for (uint32_t i = 0; i < np; i++)
		{
			const color_rgba& px = part_pixels[p][i];

			const int r = (int)px.r - mean_r;
			const int g = (int)px.g - mean_g;
			const int b = (int)px.b - mean_b;

			pCov[0] += r * r; pCov[1] += r * g; pCov[2] += r * b;
			pCov[3] += g * g; pCov[4] += g * b;
			pCov[5] += b * b;

		} // i

	} // p

	float slam_to_line_sse_est = 0.0f;

	for (uint32_t p = 0; p < num_subsets; p++)
	{
		const int block_max_var = basisu::maximum(part_icov[p][0], part_icov[p][3], part_icov[p][5]);

		float cov[6];
		for (uint32_t i = 0; i < 6; i++)
			cov[i] = (float)part_icov[p][i];

		const float sc = block_max_var ? (1.0f / (float)block_max_var) : 0;
		const float wx = sc * cov[0], wy = sc * cov[3], wz = sc * cov[5];

		// estimate principle axis using one iteration of the power method
		const float alt_xr = cov[0] * wx + cov[1] * wy + cov[2] * wz;
		const float alt_xg = cov[1] * wx + cov[3] * wy + cov[4] * wz;
		const float alt_xb = cov[2] * wx + cov[4] * wy + cov[5] * wz;

		slam_to_line_sse_est += estimate_slam_to_line_sse_3D(cov, alt_xr, alt_xg, alt_xb);

	} // p

	return slam_to_line_sse_est;
}

static float estimate_partition_rgba_sse(
	const astc_ldr::pixel_stats_t& pixel_stats,
	bool base_scale_flag,
	uint32_t block_width, uint32_t block_height,
	uint32_t num_subsets,
	const astc_ldr::partition_pattern_vec* pPat)
{
	assert((num_subsets >= 2) && (num_subsets <= astc_helpers::MAX_PARTITIONS));

	color_rgba part_pixels[astc_helpers::MAX_PARTITIONS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint32_t num_part_pixels[astc_helpers::MAX_PARTITIONS] = { 0 };

	int part_means[astc_helpers::MAX_PARTITIONS][4];
	clear_obj(part_means);

	const uint32_t total_block_pixels = block_width * block_height;

	if (base_scale_flag)
	{
		for (uint32_t i = 0; i < total_block_pixels; i++)
		{
			const color_rgba& px = pixel_stats.m_pixels[i];

			const uint32_t part_index = (*pPat)[i];
			assert(part_index < num_subsets);

			part_pixels[part_index][num_part_pixels[part_index]] = px;

			num_part_pixels[part_index]++;

		} // i
	}
	else
	{
		for (uint32_t i = 0; i < total_block_pixels; i++)
		{
			const color_rgba& px = pixel_stats.m_pixels[i];

			const uint32_t part_index = (*pPat)[i];
			assert(part_index < num_subsets);

			part_pixels[part_index][num_part_pixels[part_index]] = px;

			part_means[part_index][0] += px.r;
			part_means[part_index][1] += px.g;
			part_means[part_index][2] += px.b;
			part_means[part_index][3] += px.a;

			num_part_pixels[part_index]++;

		} // i

		for (uint32_t i = 0; i < num_subsets; i++)
		{
			assert(num_part_pixels[i] > 0);

			const int n = num_part_pixels[i];
			const int r = n >> 1;

			part_means[i][0] = (part_means[i][0] + r) / n;
			part_means[i][1] = (part_means[i][1] + r) / n;
			part_means[i][2] = (part_means[i][2] + r) / n;
			part_means[i][3] = (part_means[i][3] + r) / n;
		} // i
	}

	int part_icov4[astc_helpers::MAX_PARTITIONS][10];
	clear_obj(part_icov4);

	for (uint32_t p = 0; p < num_subsets; p++)
	{
		const uint32_t np = num_part_pixels[p];

		const int mean_r = part_means[p][0];
		const int mean_g = part_means[p][1];
		const int mean_b = part_means[p][2];
		const int mean_a = part_means[p][3];

		int* pCov = &part_icov4[p][0];

		for (uint32_t i = 0; i < np; i++)
		{
			const color_rgba& px = part_pixels[p][i];

			const int r = (int)px.r - mean_r;
			const int g = (int)px.g - mean_g;
			const int b = (int)px.b - mean_b;
			const int a = (int)px.a - mean_a;

			pCov[0] += r * r; pCov[1] += r * g; pCov[2] += r * b; pCov[3] += r * a;
			pCov[4] += g * g; pCov[5] += g * b; pCov[6] += g * a;
			pCov[7] += b * b; pCov[8] += b * a;
			pCov[9] += a * a;

		} // i

	} // p

	float slam_to_line_sse_est = 0.0f;

	for (uint32_t s = 0; s < num_subsets; s++)
	{
		const int block_max_var4 = basisu::maximum(part_icov4[s][0], part_icov4[s][4], part_icov4[s][7], part_icov4[s][9]);

		float cov4[10];
		for (uint32_t i = 0; i < 10; i++)
			cov4[i] = (float)part_icov4[s][i];

		const float sc4 = block_max_var4 ? (1.0f / (float)block_max_var4) : 0;
		const float wx = sc4 * cov4[0], wy = sc4 * cov4[4], wz = sc4 * cov4[7], wa = sc4 * cov4[9];

		const float x0 = cov4[0] * wx + cov4[1] * wy + cov4[2] * wz + cov4[3] * wa;
		const float y0 = cov4[1] * wx + cov4[4] * wy + cov4[5] * wz + cov4[6] * wa;
		const float z0 = cov4[2] * wx + cov4[5] * wy + cov4[7] * wz + cov4[8] * wa;
		const float w0 = cov4[3] * wx + cov4[6] * wy + cov4[8] * wz + cov4[9] * wa;

		const float x1 = cov4[0] * x0 + cov4[1] * y0 + cov4[2] * z0 + cov4[3] * w0;
		const float y1 = cov4[1] * x0 + cov4[4] * y0 + cov4[5] * z0 + cov4[6] * w0;
		const float z1 = cov4[2] * x0 + cov4[5] * y0 + cov4[7] * z0 + cov4[8] * w0;
		const float w1 = cov4[3] * x0 + cov4[6] * y0 + cov4[8] * z0 + cov4[9] * w0;

		slam_to_line_sse_est += estimate_slam_to_line_sse_4D(cov4, x1, y1, z1, w1);

	} // s

	return slam_to_line_sse_est;
}

static inline float estimate_partition_sse(uint32_t cem_index,
	const astc_ldr::pixel_stats_t& pixel_stats,
	uint32_t block_width, uint32_t block_height,
	uint32_t num_subsets,
	const astc_ldr::partition_pattern_vec* pPat)
{
	switch (cem_index)
	{
	// RGB
	case astc_helpers::CEM_LDR_RGB_DIRECT:
		return estimate_partition_rgb_sse(pixel_stats, false, block_width, block_height, num_subsets, pPat);
	case astc_helpers::CEM_LDR_RGB_BASE_SCALE:
		return estimate_partition_rgb_sse(pixel_stats, true, block_width, block_height, num_subsets, pPat);

	// RGBA
	case astc_helpers::CEM_LDR_RGBA_DIRECT:
		return estimate_partition_rgba_sse(pixel_stats, false, block_width, block_height, num_subsets, pPat);
	case astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A:
		return estimate_partition_rgba_sse(pixel_stats, true, block_width, block_height, num_subsets, pPat);

	default:
		assert(0);
		break;
	}

	return 0;
}

#define BASISU_USE_LSH2 (1)
#define BASISU_USE_LSH3 (1)
#define BASISU_LSH_FILTERING true

static bool estimate_partition2(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixels,
	int* pBest_parts, // unique indices, not ASTC seeds
	int &num_best_parts, // will be modified with the actual number of results
	const astc_ldr::partitions_data* pPart_data, 
	bool brute_force_flag)
{
	assert(num_best_parts && (num_best_parts <= (int)pPart_data->m_total_unique_patterns));

	const uint32_t num_block_pixels = block_width * block_height;

	if (brute_force_flag)
	{
		int desired_parts[astc_ldr::ASTC_LDR_MAX_BLOCK_HEIGHT][astc_ldr::ASTC_LDR_MAX_BLOCK_WIDTH]; // [y][x]

		for (uint32_t i = 0; i < num_block_pixels; i++)
		{
			float proj = (pixels.m_pixels_f[i] - pixels.m_mean_f).dot(pixels.m_mean_rel_axis4);

			desired_parts[i / block_width][i % block_width] = proj < 0.0f;
		}

		uint32_t part_similarity[astc_helpers::NUM_PARTITION_PATTERNS];

		for (uint32_t part_index = 0; part_index < pPart_data->m_total_unique_patterns; part_index++)
		{
			const astc_ldr::partition_pattern_vec& pat_vec = pPart_data->m_partition_pats[part_index];

			int total_sim_non_inv = 0;
			int total_sim_inv = 0;

			for (uint32_t y = 0; y < block_height; y++)
			{
				for (uint32_t x = 0; x < block_width; x++)
				{
					int part = pat_vec[x + y * block_width];

					if (part == desired_parts[y][x])
						total_sim_non_inv++;

					if ((part ^ 1) == desired_parts[y][x])
						total_sim_inv++;
				}
			}

			int total_sim = maximum(total_sim_non_inv, total_sim_inv);

			part_similarity[part_index] = (total_sim << 16) | part_index;

		} // part_index;

		std::sort(part_similarity, part_similarity + pPart_data->m_total_unique_patterns);

		for (int i = 0; i < num_best_parts; i++)
			pBest_parts[i] = part_similarity[(pPart_data->m_total_unique_patterns - 1) - i] & 0xFFFF;
	}
	else if (BASISU_USE_LSH2)
	{
		astc_ldr::partition_pattern_vec desired_part(block_width, block_height);
		astc_ldr::partition_pattern_vec desired_part_alt(block_width, block_height);

		for (uint32_t i = 0; i < num_block_pixels; i++)
		{
			float proj = (pixels.m_pixels_f[i] - pixels.m_mean_f).dot(pixels.m_mean_rel_axis4);

			desired_part.m_parts[i] = proj < 0.0f;
			desired_part_alt.m_parts[i] = 1 - desired_part.m_parts[i];
		}

		uint32_t results[astc_helpers::NUM_PARTITION_PATTERNS];
		uint32_t total_results = pPart_data->m_part_lhs_map.find(desired_part, results, astc_helpers::NUM_PARTITION_PATTERNS, BASISU_LSH_FILTERING);

		if (!total_results)
		{
			num_best_parts = 0;
			return false;
		}

		uint32_t part_similarity[astc_helpers::NUM_PARTITION_PATTERNS];

		for (uint32_t res_index = 0; res_index < total_results; res_index++)
		{
			const uint32_t part_index = results[res_index];

			const astc_ldr::partition_pattern_vec& pat_vec = pPart_data->m_partition_pats[part_index];

			const int dist2_a = desired_part.get_squared_distance_2subsets(pat_vec);
			const int dist2_b = desired_part_alt.get_squared_distance_2subsets(pat_vec);
				
			const int dist2 = minimum(dist2_a, dist2_b);
								
			part_similarity[res_index] = (dist2 << 16) | part_index;

		} // part_index;

		std::sort(part_similarity, part_similarity + total_results);

		num_best_parts = minimum<int>(num_best_parts, total_results);

		for (int i = 0; i < num_best_parts; i++)
			pBest_parts[i] = part_similarity[i] & 0xFFFF;
	}
	else
	{
		astc_ldr::partition_pattern_vec desired_part(block_width, block_height);

		for (uint32_t i = 0; i < num_block_pixels; i++)
		{
			float proj = (pixels.m_pixels_f[i] - pixels.m_mean_f).dot(pixels.m_mean_rel_axis4);

			desired_part.m_parts[i] = proj < 0.0f;
		}

		astc_ldr::vp_tree::result_queue results;
		results.reserve(num_best_parts);

		pPart_data->m_part_vp_tree.find_nearest(2, desired_part, results, num_best_parts);

		assert((int)results.get_size() == num_best_parts);

		const auto& elements = results.get_elements();

		for (uint32_t i = 0; i < results.get_size(); i++)
			pBest_parts[i] = elements[1 + i].m_pat_index;
	}

	return true;
}

static bool estimate_partition3(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixels,
	int* pBest_parts, 
	int &num_best_parts, // will be modified with the actual number of results
	const astc_ldr::partitions_data* pPart_data, bool brute_force_flag)
{
	assert(num_best_parts && (num_best_parts <= (int)pPart_data->m_total_unique_patterns));

	vec4F training_vecs[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS], mean(0.0f);

	const uint32_t num_block_pixels = block_width * block_height, NUM_SUBSETS = 3;

	float brightest_inten = -BIG_FLOAT_VAL, darkest_inten = BIG_FLOAT_VAL;

	vec4F cluster_centroids[NUM_SUBSETS];
	clear_obj(cluster_centroids);

	for (uint32_t i = 0; i < num_block_pixels; i++)
	{
		vec4F& v = training_vecs[i];

		v = pixels.m_pixels_f[i];
				
		float inten = (v - pixels.m_mean_f).dot(pixels.m_mean_rel_axis4);
		if (inten < darkest_inten)
		{
			darkest_inten = inten;
			cluster_centroids[0] = v;
		}

		if (inten > brightest_inten)
		{
			brightest_inten = inten;
			cluster_centroids[1] = v;
		}
	}

	if (cluster_centroids[0] == cluster_centroids[1])
		return false;

	float furthest_dist2 = 0.0f;
	for (uint32_t i = 0; i < num_block_pixels; i++)
	{
		vec4F& v = training_vecs[i];

		float dist_a = v.squared_distance(cluster_centroids[0]);
		if (dist_a == 0.0f)
			continue;

		float dist_b = v.squared_distance(cluster_centroids[1]);
		if (dist_b == 0.0f)
			continue;

		float dist2 = dist_a + dist_b;
		if (dist2 > furthest_dist2)
		{
			furthest_dist2 = dist2;
			cluster_centroids[2] = v;
		}
	}

	if ((cluster_centroids[0] == cluster_centroids[2]) || (cluster_centroids[1] == cluster_centroids[2]))
		return false;

	uint32_t cluster_pixels[NUM_SUBSETS][astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
	uint32_t num_cluster_pixels[NUM_SUBSETS];
	vec4F new_cluster_means[NUM_SUBSETS];

	const uint32_t NUM_ITERS = 4;

	for (uint32_t s = 0; s < NUM_ITERS; s++)
	{
		memset(num_cluster_pixels, 0, sizeof(num_cluster_pixels));
		memset((void *)new_cluster_means, 0, sizeof(new_cluster_means));

		for (uint32_t i = 0; i < num_block_pixels; i++)
		{
			float d[NUM_SUBSETS] = {
				training_vecs[i].squared_distance(cluster_centroids[0]),
				training_vecs[i].squared_distance(cluster_centroids[1]),
				training_vecs[i].squared_distance(cluster_centroids[2]) };

			float min_d = d[0];
			uint32_t min_idx = 0;
			for (uint32_t j = 1; j < NUM_SUBSETS; j++)
			{
				if (d[j] < min_d)
				{
					min_d = d[j];
					min_idx = j;
				}
			}

			cluster_pixels[min_idx][num_cluster_pixels[min_idx]] = i;
			new_cluster_means[min_idx] += training_vecs[i];
			num_cluster_pixels[min_idx]++;
		} // i

		// Can skip updating the centroids on the last iteration - all we care about is the final partitioning.
		if (s == (NUM_ITERS - 1))
		{
			for (uint32_t j = 0; j < NUM_SUBSETS; j++)
			{
				if (!num_cluster_pixels[j])
					return false;
			}
		}
		else
		{
			for (uint32_t j = 0; j < NUM_SUBSETS; j++)
			{
				if (!num_cluster_pixels[j])
					return false;

				cluster_centroids[j] = new_cluster_means[j] / (float)num_cluster_pixels[j];
			} // j
		}

	} // s

	astc_ldr::partition_pattern_vec desired_part(block_width, block_height);

	for (uint32_t p = 0; p < NUM_SUBSETS; p++)
	{
		for (uint32_t i = 0; i < num_cluster_pixels[p]; i++)
		{
			const uint32_t pix_index = cluster_pixels[p][i];
			desired_part[pix_index] = (uint8_t)p;
		} // i
	} // p

	if (brute_force_flag)
	{
		astc_ldr::partition_pattern_vec desired_parts[astc_ldr::NUM_PART3_MAPPINGS];
		for (uint32_t j = 0; j < astc_ldr::NUM_PART3_MAPPINGS; j++)
			desired_parts[j] = desired_part.get_permuted3(j);

		uint32_t part_similarity[astc_helpers::NUM_PARTITION_PATTERNS];

		for (uint32_t part_index = 0; part_index < pPart_data->m_total_unique_patterns; part_index++)
		{
			const astc_ldr::partition_pattern_vec& pat = pPart_data->m_partition_pats[part_index];

			uint32_t lowest_pat_dist = UINT32_MAX;
			for (uint32_t p = 0; p < astc_ldr::NUM_PART3_MAPPINGS; p++)
			{
				uint32_t dist = pat.get_squared_distance(desired_parts[p]);
				if (dist < lowest_pat_dist)
					lowest_pat_dist = dist;
			}

			part_similarity[part_index] = (lowest_pat_dist << 16) | part_index;

		} // part_index;

		std::sort(part_similarity, part_similarity + pPart_data->m_total_unique_patterns);

		for (int i = 0; i < num_best_parts; i++)
			pBest_parts[i] = part_similarity[i] & 0xFFFF;
	}
	else if (BASISU_USE_LSH3)
	{
		astc_ldr::partition_pattern_vec desired_parts[astc_ldr::NUM_PART3_MAPPINGS];
		for (uint32_t j = 0; j < astc_ldr::NUM_PART3_MAPPINGS; j++)
			desired_parts[j] = desired_part.get_permuted3(j);

		uint32_t results[astc_helpers::NUM_PARTITION_PATTERNS];
		uint32_t total_results = pPart_data->m_part_lhs_map.find(desired_part, results, astc_helpers::NUM_PARTITION_PATTERNS, BASISU_LSH_FILTERING);

		if (!total_results)
		{
			num_best_parts = 0;
			return false;
		}

		uint32_t part_similarity[astc_helpers::NUM_PARTITION_PATTERNS];

		for (uint32_t res_index = 0; res_index < total_results; res_index++)
		{
			const uint32_t part_index = results[res_index];

			const astc_ldr::partition_pattern_vec& pat = pPart_data->m_partition_pats[part_index];

			uint32_t lowest_pat_dist = UINT32_MAX;
			for (uint32_t p = 0; p < astc_ldr::NUM_PART3_MAPPINGS; p++)
			{
				uint32_t dist = pat.get_squared_distance(desired_parts[p]);
				if (dist < lowest_pat_dist)
					lowest_pat_dist = dist;
			}

			part_similarity[res_index] = (lowest_pat_dist << 16) | part_index;

		} // part_index;

		std::sort(part_similarity, part_similarity + total_results);

		num_best_parts = minimum<int>(num_best_parts, total_results);

		for (int i = 0; i < num_best_parts; i++)
			pBest_parts[i] = part_similarity[i] & 0xFFFF;
	}
	else
	{
		astc_ldr::vp_tree::result_queue results;
		results.reserve(num_best_parts);

		pPart_data->m_part_vp_tree.find_nearest(3, desired_part, results, num_best_parts);

		assert((int)results.get_size() == num_best_parts);

		const auto& elements = results.get_elements();

		for (uint32_t i = 0; i < results.get_size(); i++)
			pBest_parts[i] = elements[1 + i].m_pat_index;
	}

	return true;
}

//---------------------------------------------------------------------

static const float g_sobel_x[3][3] = // [y][x]
{
	{ -1.0f, 0.0f, 1.0f },
	{ -2.0f, 0.0f, 2.0f },
	{ -1.0f, 0.0f, 1.0f }
};

static const float g_sobel_y[3][3] = // [y][x]
{
	{ -1.0f, -2.0f, -1.0f },
	{  0.0f,  0.0f,  0.0f },
	{  1.0f,  2.0f,  1.0f }
};

static void compute_sobel(const image& orig, image& dest, const float* pMatrix_3x3)
{
	const uint32_t width = orig.get_width();
	const uint32_t height = orig.get_height();

	dest.resize(width, height);

	for (int y = 0; y < (int)height; y++)
	{
		for (int x = 0; x < (int)width; x++)
		{
			vec4F d(128.0f);

			for (int my = -1; my <= 1; my++)
			{
				for (int mx = -1; mx <= 1; mx++)
				{
					float w = pMatrix_3x3[(my + 1) * 3 + (mx + 1)];
					if (w == 0.0f)
						continue;

					const color_rgba& s = orig.get_clamped(x + mx, y + my);

					for (uint32_t c = 0; c < 4; c++)
						d[c] += w * (float)s[c];

				} // mx

			} // my

			dest(x, y).set(fast_roundf_int(d[0]), fast_roundf_int(d[1]), fast_roundf_int(d[2]), fast_roundf_int(d[3]));

		} // x
	} // y
}

// Returns total energy excluding DC
static float compute_ac_energy_from_dct(uint32_t block_width, uint32_t block_height, float* pDCT)
{
	const uint32_t num_texels = block_width * block_height;

	float total_energy = 0.0f;
	for (uint32_t i = 1; i < num_texels; i++)
	{
		const float v = square(pDCT[i]);
		pDCT[i] = v;
		total_energy += v;
	}

	pDCT[0] = 0.0f;
	return total_energy;
}

// Results scaled by # block texels (block-SSE in weight space)
static float compute_preserved_dct_energy(uint32_t block_width, uint32_t block_height, const float* pEnergy, uint32_t grid_w, uint32_t grid_h)
{
	float tot = 0.0f;

	for (uint32_t y = 0; y < block_height; y++)
	{
		for (uint32_t x = 0; x < block_width; x++)
		{
			if ((x < grid_w) && (y < grid_h))
				tot += pEnergy[x + y * block_width];
		}
	}

	return tot;
}

// Results scaled by # block texels (block-SSE in weight space)
[[maybe_unused]] static inline float compute_lost_dct_energy_orig(uint32_t block_width, uint32_t block_height, const float* pEnergy, uint32_t grid_w, uint32_t grid_h)
{
	float tot = 0.0f;

	for (uint32_t y = 0; y < block_height; y++)
	{
		for (uint32_t x = 0; x < block_width; x++)
		{
			if ((x < grid_w) && (y < grid_h))
				continue;

			tot += pEnergy[x + y * block_width];
		}
	}

	return tot;
}

static inline float compute_lost_dct_energy(uint32_t block_width, uint32_t block_height, const float* pEnergy, uint32_t grid_w, uint32_t grid_h)
{
	float tot0 = 0.0f, tot1 = 0.0f, tot2 = 0.0f, tot3 = 0.0f;

	const float* pSrc_row = pEnergy;
	for (uint32_t y = 0; y < block_height; y++, pSrc_row += block_width)
	{
		uint32_t x = (y < grid_h) ?  grid_w : 0;

		while ((x + 4) <= block_width)
		{
			tot0 += pSrc_row[x + 0];
			tot1 += pSrc_row[x + 1];
			tot2 += pSrc_row[x + 2];
			tot3 += pSrc_row[x + 3];
			x += 4;
		}

		while (x < block_width)
		{
			tot0 += pSrc_row[x];
			++x;
		}
	} // y

	return tot0 + tot1 + tot2 + tot3;
}

// Build 2D prefix sums over a row-major block of energies.
// prefix[x + y * block_width] contains the sum over [0..x] x [0..y], inclusive.
static inline void prepare_dct_energy_prefix_table(uint32_t block_width, uint32_t block_height, const float* pEnergy, float* pPrefix)
{
	for (uint32_t y = 0; y < block_height; y++)
	{
		float row_sum = 0.0f;

		for (uint32_t x = 0; x < block_width; x++)
		{
			row_sum += pEnergy[x + y * block_width];

			float above = 0.0f;
			if (y > 0)
				above = pPrefix[x + (y - 1) * block_width];

			pPrefix[x + y * block_width] = row_sum + above;
		}
	}
}

// Sum of the origin-anchored rectangle [0, grid_w) x [0, grid_h).
static inline float query_dct_energy_prefix_sum(uint32_t block_width, uint32_t block_height, const float* pPrefix, uint32_t grid_w, uint32_t grid_h)
{
	(void)block_height;

	assert((grid_w >= 1) && (grid_w <= block_width));
	assert((grid_h >= 1) && (grid_h <= block_height));

	return pPrefix[(grid_w - 1) + (grid_h - 1) * block_width];
}

static inline float compute_lost_dct_energy_prefix_sum(uint32_t block_width, uint32_t block_height, const float* pEnergy_prefix_sum, uint32_t grid_w, uint32_t grid_h, float total_ac_energy)
{
	const float kept_energy = query_dct_energy_prefix_sum(block_width, block_height, pEnergy_prefix_sum, grid_w, grid_h);

	return maximum<float>(total_ac_energy - kept_energy, 0.0f);
}

struct ldr_astc_lowlevel_block_encoder_params
{
	ldr_astc_lowlevel_block_encoder_params()
	{
		clear();
	}

	void clear()
	{
		clear_obj(*this);

		for (uint32_t i = 0; i < 4; i++)
			m_dp_active_chans[i] = true;

		m_subsets_edge_filtering = true;

		m_use_superbuckets = true;
		m_bucket_pruning_passes = true;
		m_use_dual_planes = true;

		m_superbucket_max_to_retain[0] = 4;
		m_superbucket_max_to_retain[1] = 8;
		m_superbucket_max_to_retain[2] = 16;

		m_shortlist_buckets_to_examine_fract = 1.0f; // after high-level bucket surrogate encoding and pruning stages, 1.0=effectively disabled
		m_shortlist_buckets_to_examine_min = 1;
		m_shortlist_buckets_to_examine_max = 1024;

		// TODO: Expose these at a higher level. Add alpha specific?
		m_num_similar_modes_in_bucket_to_shortlist_fract = .33f;
		m_num_similar_modes_in_bucket_to_shortlist_fract_min = 2;
		m_num_similar_modes_in_bucket_to_shortlist_fract_max = 4096;

		m_final_shortlist_fraction[0] = .2f;
		m_final_shortlist_fraction[1] = .3f;
		m_final_shortlist_fraction[2] = .5f;
		m_final_shortlist_min_size[0] = 1;
		m_final_shortlist_min_size[1] = 1;
		m_final_shortlist_min_size[2] = 1;
		m_final_shortlist_max_size[0] = 4096;
		m_final_shortlist_max_size[1] = 4096;
		m_final_shortlist_max_size[2] = 4096;

		m_gradient_descent_flag = true;
		m_polish_weights_flag = true;
		m_qcd_enabled_flag = true;
		m_encode_trial_early_out_thresh = .1f;
		m_encode_trial_subsets_early_out_thresh = .1f;

		m_final_encode_try_base_ofs = true;
		m_final_encode_always_try_rgb_direct = false; // if true, even if base_ofs succeeds, we try RGB/RGBA direct too

#if 0
		m_use_parts_std_dev_thresh = (8.0f / 255.0f);
		m_use_parts_std_dev_thresh2 = (40.0f / 255.0f);
		m_sobel_energy_thresh1 = 3200.0f;
		m_sobel_energy_thresh2 = 30000.0f;
		m_sobel_energy_thresh3 = 50000.0f;
#else
		const float s = .2f; // exp
		m_use_parts_std_dev_thresh = (8.0f / 255.0f) * s;
		m_use_parts_std_dev_thresh2 = (40.0f / 255.0f) * s;
		m_sobel_energy_thresh1 = 3200.0f * s;
		m_sobel_energy_thresh2 = 30000.0f * s;
		m_sobel_energy_thresh3 = 50000.0f * s;
#endif

		m_part2_fraction_to_keep = 2;
		m_part3_fraction_to_keep = 2;
		m_base_parts2 = 32;
		m_base_parts3 = 32;
		m_use_fast_part_est_stage2 = true;

		// TODO: Prehaps expose this at a higher level.
		m_use_blue_contraction = true;
	}

	uint32_t m_bx, m_by, m_block_width, m_block_height, m_total_block_pixels;

	const image* m_pOrig_img_sobel_xy_t;

	const astc_ldr::partitions_data* m_pPart_data_p2;
	const astc_ldr::partitions_data* m_pPart_data_p3;

	const astc_ldr::cem_encode_params* m_pEnc_params;

	// RGB or alpha trial lists (shouldn't have both in same lists)
	uint32_t m_num_trial_modes;
	const basist::astc_ldr_t::trial_mode* m_pTrial_modes;

	const basist::astc_ldr_t::grouped_trial_modes* m_pGrouped_trial_modes;

	uint32_t m_superbucket_max_to_retain[3]; // [block_complexity_index]

	float m_shortlist_buckets_to_examine_fract;
	uint32_t m_shortlist_buckets_to_examine_min;
	uint32_t m_shortlist_buckets_to_examine_max;

	float m_num_similar_modes_in_bucket_to_shortlist_fract;
	uint32_t m_num_similar_modes_in_bucket_to_shortlist_fract_min;
	uint32_t m_num_similar_modes_in_bucket_to_shortlist_fract_max;

	float m_final_shortlist_fraction[3];
	uint32_t m_final_shortlist_min_size[3];
	uint32_t m_final_shortlist_max_size[3];

	bool m_use_superbuckets;
	bool m_bucket_pruning_passes;

	// true if this is a trial mode list containing alpha
	bool m_alpha_cems;

	bool m_use_alpha_or_opaque_modes; // true for only alpha cems, false of only opaque cems;
	bool m_use_lum_direct_modes;
	bool m_use_base_scale_modes;
	bool m_use_direct_modes;
	bool m_use_dual_planes;

	bool m_grid_hv_filtering;
	bool m_filter_horizontally_flag; // = h_energy_lost < v_energy_lost, if true it's visually better to resample the block on the X axis vs. Y
	bool m_use_small_grids_only;

	bool m_dp_active_chans[4];

	bool m_subsets_enabled;
	bool m_subsets_edge_filtering;

	// TODO: Make polishing controllable per superpass.
	bool m_gradient_descent_flag;
	bool m_polish_weights_flag;
	bool m_qcd_enabled_flag;
	
	float m_encode_trial_early_out_thresh;
	float m_encode_trial_subsets_early_out_thresh;

	bool m_final_encode_try_base_ofs;
	bool m_final_encode_always_try_rgb_direct;

	bool m_brute_force_est_parts;
	bool m_disable_part_est_stage2; // only use single stage partition estimation
	bool m_use_fast_part_est_stage2; 

	bool m_use_blue_contraction; // currently global enable/disable

	float m_use_parts_std_dev_thresh;
	float m_use_parts_std_dev_thresh2;
	float m_sobel_energy_thresh1;
	float m_sobel_energy_thresh2;
	float m_sobel_energy_thresh3;

	uint32_t m_part2_fraction_to_keep;
	uint32_t m_part3_fraction_to_keep;
	uint32_t m_base_parts2;
	uint32_t m_base_parts3;

	float m_early_stop_wpsnr;
	float m_early_stop2_wpsnr;

	const basist::astc_ldr_t::dct2f* m_pDCT2F; // at block size
};

struct trial_surrogate
{
	uint32_t m_trial_mode_index;
	float m_err;

	log_surrogate_astc_blk m_log_blk;

	void clear()
	{
		m_trial_mode_index = 0;
		m_err = 0;
		m_log_blk.clear();
	}

	bool operator < (const trial_surrogate& rhs) const
	{
		return m_err < rhs.m_err;
	}
};

struct encode_block_output
{
	int16_t m_trial_mode_index; // -1 = solid, no trial mode
	uint16_t m_blur_id; // blur index, or codec identifier

	astc_helpers::log_astc_block m_log_blk;

	// Packed per-plane DCT data
	basist::astc_ldr_t::dct_syms m_packed_dct_plane_data[2];

	uint64_t m_sse;

	void clear()
	{
		m_trial_mode_index = -1;
		m_blur_id = 0;
		m_log_blk.clear();
		m_sse = 0;
	}
};

struct encode_block_stats
{
	uint32_t m_total_superbuckets_created;
	uint32_t m_total_buckets_created;
	uint32_t m_total_surrogate_encodes;
	uint32_t m_total_shortlist_candidates;
	uint32_t m_total_full_encodes;

	encode_block_stats() { clear(); }

	void clear()
	{
		clear_obj(*this);
	}
};

struct chan_mse_est
{
	float m_ep;
	float m_wp;

	chan_mse_est() {}
	chan_mse_est(float ep, float wp) : m_ep(ep), m_wp(wp) {}
};

struct weight_terms
{
	float m_mean;
	float m_var;
	float m_endpoint_factor;
	float m_weight_spread_scale;

	void calc(uint32_t n, const float* pWeights)
	{
		assert(n);

		float weight_total = 0.0f;
		for (uint32_t i = 0; i < n; i++)
		{
			assert(is_in_range(pWeights[i], 0.0f, 1.0f));
			weight_total += pWeights[i];
		}
		m_mean = weight_total / (float)n;

		float weight_var = 0.0f;
		for (uint32_t i = 0; i < n; i++)
			weight_var += squaref(pWeights[i] - m_mean);
		m_var = weight_var / (float)n;

		// drops below 2/3 on smooth blocks and tends to 2/3 when weights are well spread (so normalized by (2.0f / 3.0f))
		//m_endpoint_factor = (1.0f + 2.0f * m_var + 2.0f * m_mean * m_mean - 2.0f * m_mean) / (2.0f / 3.0f);
		m_endpoint_factor = (1.0f + 2.0f * m_var + 2.0f * m_mean * m_mean - 2.0f * m_mean) * (3.0f / 2.0f);
		m_endpoint_factor = clamp<float>(m_endpoint_factor, .25f, 1.50f);

		const float UNIFORM_VAR = 1.0f / 12.0f;
		float s = m_var / UNIFORM_VAR;

		// shrinks the weight term on smooth blocks and is ~1 when weights are spread.
		m_weight_spread_scale = saturate(s);
	}
};

// weight_gamma is block size/grid size specific factor (0,1] (the amount of MSE quant error remaining taking into account bilinear smoothing)
static inline chan_mse_est compute_quantized_channel_mse_estimates(uint32_t num_endpoint_levels, uint32_t num_weight_levels, float span_size, float weight_gamma, const weight_terms* pWeight_terms = nullptr)
{
	assert(num_endpoint_levels >= 2);
	assert(num_weight_levels >= 2);

	const float Dep = 1.0f / (float)(num_endpoint_levels - 1); // endpoint quant step
	
#if BASISU_MODIFIED_WEIGHT_QUANT_MSE_ESTIMATE
	const float Dw = 1.0f / (float)(num_weight_levels); // weight quant step
#else
	const float Dw = 1.0f / (float)(num_weight_levels - 1); // weight quant step
#endif

	// Endpoint quant MSE estimate is not span dependent
	float ep_lower = (Dep * Dep) / 12.0f * (2.0f / 3.0f);

	// Weight quant MSE estimate is span dependent
	float wq_lower = (Dw * Dw) / 12.0f * weight_gamma * (span_size * span_size);

	if (pWeight_terms)
	{
		ep_lower *= pWeight_terms->m_endpoint_factor;
		wq_lower *= pWeight_terms->m_weight_spread_scale;
	}

	return chan_mse_est(ep_lower, wq_lower);
}

static inline float compute_quantized_channel_endpoint_mse_estimate(uint32_t num_endpoint_levels, const weight_terms* pWeight_terms = nullptr)
{
	assert(num_endpoint_levels >= 2);

	const float Dep = 1.0f / (float)(num_endpoint_levels - 1); // endpoint quant step

	// Endpoint quant MSE estimate is not span dependent
	float ep_lower = (Dep * Dep) * (1.0f / 12.0f) * (2.0f / 3.0f);

	if (pWeight_terms)
		ep_lower *= pWeight_terms->m_endpoint_factor;

	return ep_lower;
}

static inline float compute_quantized_channel_weight_mse_estimate(uint32_t num_weight_levels, float span_size, float weight_gamma, const weight_terms* pWeight_terms = nullptr)
{
	assert(num_weight_levels >= 2);
		
#if BASISU_MODIFIED_WEIGHT_QUANT_MSE_ESTIMATE
	const float Dw = 1.0f / (float)(num_weight_levels); // weight quant step
#else
	const float Dw = 1.0f / (float)(num_weight_levels - 1); // weight quant step
#endif

	// Weight quant MSE estimate is span dependent
	float wq_lower = (Dw * Dw) * (1.0f / 12.0f) * weight_gamma * (span_size * span_size);

	if (pWeight_terms)
		wq_lower *= pWeight_terms->m_weight_spread_scale;

	return wq_lower;
}

const float BLUE_CONTRACTION_BASE_OFS_DISCOUNT = .9f;
const float SKIP_IF_BUCKET_WORSE_MULTIPLIER = 5.0f;

struct shortlist_bucket
{
	bool m_examined_flag;
	int8_t m_grid_width, m_grid_height;
	int8_t m_ccs_index;

	uint8_t m_cem_index;
	uint8_t m_num_parts;
	uint16_t m_unique_seed_index;

	log_surrogate_astc_blk m_surrogate_log_blk;
	float m_sse;

	shortlist_bucket()
	{
	}

	shortlist_bucket(int grid_width, int grid_height, uint32_t cem_index, int ccs_index, uint32_t num_parts, uint32_t unique_seed_index) :
		m_grid_width((int8_t)grid_width), m_grid_height((int8_t)grid_height),
		m_ccs_index((int8_t)ccs_index),
		m_cem_index((uint8_t)cem_index),
		m_num_parts((uint8_t)num_parts),
		m_unique_seed_index((uint16_t)unique_seed_index)
	{
		m_surrogate_log_blk.clear();
		m_sse = 0.0f;
		m_examined_flag = false;
	}

	operator size_t() const
	{
#define ADD_HASH(H) h ^= basist::hash_hsieh((uint8_t*)&(H), sizeof(H));
		size_t h = 0;
		ADD_HASH(m_grid_width);
		ADD_HASH(m_grid_height);
		ADD_HASH(m_ccs_index);
		ADD_HASH(m_cem_index);
		ADD_HASH(m_num_parts);
		ADD_HASH(m_unique_seed_index);
#undef ADD_HASH
		return h;
	}

	// equality for hashing
	bool operator== (const shortlist_bucket& rhs) const
	{
		return (m_grid_width == rhs.m_grid_width) && (m_grid_height == rhs.m_grid_height) && (m_cem_index == rhs.m_cem_index) && (m_ccs_index == rhs.m_ccs_index) &&
			(m_num_parts == rhs.m_num_parts) && (m_unique_seed_index == rhs.m_unique_seed_index);
	}
};

typedef static_vector<uint16_t, 16> trial_mode_index_vec;
typedef basisu::hash_map<shortlist_bucket, trial_mode_index_vec > shortlist_bucket_hash_t;

#pragma pack(push, 1)
struct trial_mode_estimate_superbucket_key
{
	// All member vars from beginning to m_last will be hashed. Be careful of alignment.
	// Total size must be sizeof(uint64_t). Unused members MUST be set to 0.
	uint8_t m_cem_index;
	int8_t m_ccs_index;
	uint16_t m_subset_unique_index;

	uint8_t m_num_subsets;
	uint8_t m_last;
	uint8_t m_unused[2];

	inline trial_mode_estimate_superbucket_key()
	{
		static_assert((sizeof(*this) % 4) == 0, "struct size must be divisible by 4");
	}

	inline void clear()
	{
		clear_obj(*this);
	}

	inline operator size_t() const
	{
#if 0
		return basist::hash_hsieh((const uint8_t*)this, BASISU_OFFSETOF(trial_mode_estimate_superbucket_key, m_last));
#elif 1
		static_assert(sizeof(*this) == sizeof(uint64_t), "struct size must be sizeof(uint64_t)");

		uint64_t x = *reinterpret_cast<const uint64_t*>(this);

		x ^= (x >> 33);
		x *= 0xff51afd7ed558ccdULL; // Murmur finalizer constant
		x ^= (x >> 33);

		return (size_t)x;
#else
		uint64_t x =
			(uint64_t)m_cem_index |
			(((uint64_t)(uint8_t)m_ccs_index) << 8) |
			(((uint64_t)m_subset_unique_index) << 16) |
			(((uint64_t)m_num_subsets) << 32);

		x ^= (x >> 33);
		x *= 0xff51afd7ed558ccdULL; // Murmur finalizer constant
		x ^= (x >> 33);

		return (size_t)x;
#endif
	}

	inline bool operator== (const trial_mode_estimate_superbucket_key& rhs) const
	{
#if 0
#define COMP(e) if (e != rhs.e) return false;
		COMP(m_cem_index);
		COMP(m_ccs_index);
		COMP(m_subset_unique_index);
		COMP(m_num_subsets);
#undef COMP
		return true;
#endif

		static_assert(sizeof(*this) == sizeof(uint64_t), "struct size must be sizeof(uint64_t)");

		return *reinterpret_cast<const uint64_t*>(this) == *reinterpret_cast<const uint64_t*>(&rhs);
	}

	inline bool operator!= (const trial_mode_estimate_superbucket_key& rhs) const
	{
		return !(*this == rhs);
	}
};
#pragma pack(pop)

struct trial_mode_estimate_superbucket_value
{
	basisu::vector<uint32_t> m_trial_mode_list;
};

typedef hash_map<trial_mode_estimate_superbucket_key, trial_mode_estimate_superbucket_value> trial_mode_estimate_superbucket_hash;

struct trial_mode_estimate
{
	trial_mode_estimate_superbucket_key m_superbucket_key;

	uint32_t m_trial_mode_index;
	float m_wsse;

	bool operator< (const trial_mode_estimate& rhs) const
	{
		return m_wsse < rhs.m_wsse;
	}
};

struct ranked_shortlist_bucket
{
	shortlist_bucket m_bucket;
	trial_mode_index_vec m_trial_mode_indices;

	bool operator < (const ranked_shortlist_bucket& rhs) const { return m_bucket.m_sse < rhs.m_bucket.m_sse; }
};

struct ldr_astc_lowlevel_block_encoder
{
	ldr_astc_lowlevel_block_encoder() :
		m_used_flag(false)
	{
		clear();
	}

	// Warning: These objects can migrate between threads (be cautious of determinism issues with containers/hash tables!)
	bool m_used_flag;

	// Thread-local data follows
	uint_vec m_trial_modes_to_estimate;

	trial_mode_estimate_superbucket_hash m_superbucket_hash;

	std::priority_queue<trial_mode_estimate> m_trial_mode_estimate_priority_queue;

	basist::astc_ldr_t::fvec m_dct_work;

	shortlist_bucket_hash_t m_shortlist_hash0;
	shortlist_bucket_hash_t m_shortlist_hash1;

	basisu::vector<trial_surrogate> m_trial_surrogates;

	float m_sobel_energy;
	float m_max_std_dev;

	uint32_t m_block_complexity_index;  // [0,2]
	bool m_strong_edges;
	bool m_very_strong_edges;
	bool m_super_strong_edges;

	bool m_used_superbuckets;

	int m_best_parts2[2][MAX_BASE_PARTS2 * PART_ESTIMATE_STAGE1_MULTIPLIER]; // [rgb[a]direct/rgbs][est_part]
	int m_num_est_parts2[2];

	int m_best_parts3[2][MAX_BASE_PARTS3 * PART_ESTIMATE_STAGE1_MULTIPLIER]; // [rgb[a]direct/rgbs][est_part]
	int m_num_est_parts3[2];

	basisu::vector<ranked_shortlist_bucket> m_ranked_buckets;

	void clear()
	{
		m_trial_modes_to_estimate.resize(0);
		m_superbucket_hash.reset();
				
		m_trial_surrogates.resize(0);

		m_sobel_energy = 0;
		m_max_std_dev = 0;
		m_block_complexity_index = 0;
		m_strong_edges = false;
		m_very_strong_edges = false;
		m_super_strong_edges = false;

		m_used_superbuckets = false;

		clear_obj(m_best_parts2);
		clear_obj(m_num_est_parts2);

		clear_obj(m_best_parts3);
		clear_obj(m_num_est_parts3);

		m_ranked_buckets.resize(0);
	}

	bool init(
		const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		BASISU_NOTE_UNUSED(blur_id);
		BASISU_NOTE_UNUSED(out_blocks);
		BASISU_NOTE_UNUSED(stats);

		// TODO: This sums the *original* (not blurred) block's energy - precompute this? Replace with DCT?
		m_sobel_energy = 0.0f;
		for (uint32_t y = 0; y < p.m_block_height; y++)
		{
			for (uint32_t x = 0; x < p.m_block_width; x++)
			{
				const color_rgba& s = p.m_pOrig_img_sobel_xy_t->get_clamped(p.m_bx * p.m_block_width + x, p.m_by * p.m_block_height + y);

				// TODO: sum max of all channels instead?
				m_sobel_energy += s[0] * s[0] + s[1] * s[1] + s[2] * s[2] + s[3] * s[3];
			} // x 
		} // y

		m_sobel_energy /= (float)p.m_total_block_pixels;

		m_max_std_dev = 0.0f;
		for (uint32_t i = 0; i < 4; i++)
			m_max_std_dev = maximum(m_max_std_dev, pixel_stats.m_rgba_stats[i].m_std_dev);

		m_strong_edges = (m_max_std_dev > p.m_use_parts_std_dev_thresh) && (m_sobel_energy > p.m_sobel_energy_thresh1);
		m_very_strong_edges = (m_max_std_dev > p.m_use_parts_std_dev_thresh2) && (m_sobel_energy > p.m_sobel_energy_thresh2);
		m_super_strong_edges = (m_max_std_dev > p.m_use_parts_std_dev_thresh2) && (m_sobel_energy > p.m_sobel_energy_thresh3);

		m_block_complexity_index = m_super_strong_edges ? 2 : (m_very_strong_edges ? 1 : 0);

		return true;
	}

	bool partition_triage(
		const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		BASISU_NOTE_UNUSED(blur_id);
		BASISU_NOTE_UNUSED(out_blocks);

		clear_obj(m_num_est_parts2);
		clear_obj(m_num_est_parts3);

		if (!p.m_subsets_enabled)
			return true;

		if (p.m_subsets_edge_filtering)
		{
			if (!m_strong_edges)
				return true;
		}

		assert(p.m_base_parts2 <= MAX_BASE_PARTS2);
		assert(p.m_base_parts3 <= MAX_BASE_PARTS3);

		// 2 subsets
		int total_parts2 = m_super_strong_edges ? (p.m_base_parts2 * PART_ESTIMATE_STAGE1_MULTIPLIER) : (m_very_strong_edges ? (p.m_base_parts2 * 2) : p.m_base_parts2);
		total_parts2 = minimum<uint32_t>(total_parts2, MAX_BASE_PARTS2 * PART_ESTIMATE_STAGE1_MULTIPLIER);
		total_parts2 = minimum<uint32_t>(total_parts2, p.m_pPart_data_p2->m_total_unique_patterns);

		const uint32_t surrogate_encode_flags = 0;

		if (total_parts2)
		{
			int best_parts2_temp[MAX_BASE_PARTS2 * PART_ESTIMATE_STAGE1_MULTIPLIER];
			assert(total_parts2 <= (int)std::size(best_parts2_temp));

			// Stage 1: kmeans+vptree
			bool has_est_parts2 = estimate_partition2(
				p.m_block_width, p.m_block_height,
				pixel_stats,
				best_parts2_temp, total_parts2,
				p.m_pPart_data_p2, p.m_brute_force_est_parts);

			if (has_est_parts2)
			{
				// Always try direct, optionally base+scale cem's
				for (uint32_t s = 0; s < 2; s++)
				{
					if ((s) && (!p.m_use_base_scale_modes))
						continue;

					if (p.m_disable_part_est_stage2)
					{
						m_num_est_parts2[s] = total_parts2;
						memcpy(m_best_parts2[s], best_parts2_temp, m_num_est_parts2[s] * sizeof(int));
						continue;
					}

					uint32_t cem_to_surrogate_encode = p.m_alpha_cems ? astc_helpers::CEM_LDR_RGBA_DIRECT : astc_helpers::CEM_LDR_RGB_DIRECT;
					if (s)
						cem_to_surrogate_encode = p.m_alpha_cems ? astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A : astc_helpers::CEM_LDR_RGB_BASE_SCALE;

					// Stage 2: Analytic surrogate WSSE
					basisu::vector<float> part_sses(total_parts2);

					for (int i = 0; i < total_parts2; i++)
					{
						const astc_ldr::partitions_data* pPart_data = p.m_pPart_data_p2;

						const uint32_t unique_seed_index = best_parts2_temp[i];
						const uint32_t part_seed_index = pPart_data->m_unique_index_to_part_seed[unique_seed_index];

						const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[unique_seed_index];

						log_surrogate_astc_blk surrogate_log_blk;
						float sse;
						if (p.m_use_fast_part_est_stage2)
						{
							sse = estimate_partition_sse(cem_to_surrogate_encode, pixel_stats, p.m_block_width, p.m_block_height, 2, pPat);
						}
						else
						{
							sse = encode_surrogate_trial_subsets(
								p.m_block_width, p.m_block_height,
								pixel_stats,
								cem_to_surrogate_encode, 2, part_seed_index, pPat,
								astc_helpers::BISE_256_LEVELS, astc_helpers::BISE_64_LEVELS,
								p.m_block_width, p.m_block_height,
								surrogate_log_blk,
								*p.m_pEnc_params, surrogate_encode_flags);
							
							stats.m_total_surrogate_encodes++;
						}
												
						part_sses[i] = sse;
					} // i

					basisu::vector<uint32_t> part_sses_ranks(total_parts2);

					indirect_sort(total_parts2, part_sses_ranks.get_ptr(), part_sses.get_ptr());

					m_num_est_parts2[s] = maximum<int>(1, (total_parts2 + p.m_part2_fraction_to_keep - 1) / p.m_part2_fraction_to_keep);

					for (int i = 0; i < m_num_est_parts2[s]; i++)
					{
						const uint32_t rank_index = part_sses_ranks[i];
						const uint32_t unique_seed_unique = best_parts2_temp[rank_index];
						m_best_parts2[s][i] = unique_seed_unique;
					} // i

				} // s

			} // if (has_est_parts2)

		} // if (total_parts2)

		// 3 subsets
		int total_parts3 = m_super_strong_edges ? (p.m_base_parts3 * PART_ESTIMATE_STAGE1_MULTIPLIER) : (m_very_strong_edges ? (p.m_base_parts3 * 2) : p.m_base_parts3);
		total_parts3 = minimum<uint32_t>(total_parts3, MAX_BASE_PARTS3 * PART_ESTIMATE_STAGE1_MULTIPLIER);
		total_parts3 = minimum<uint32_t>(total_parts3, p.m_pPart_data_p3->m_total_unique_patterns);

		if (total_parts3)
		{
			int best_parts3_temp[MAX_BASE_PARTS3 * PART_ESTIMATE_STAGE1_MULTIPLIER];
			assert(total_parts3 <= (int)std::size(best_parts3_temp));

			// Stage 1: kmeans+vptree
			const bool has_est_parts3 = estimate_partition3(
				p.m_block_width, p.m_block_height,
				pixel_stats,
				best_parts3_temp, total_parts3,
				p.m_pPart_data_p3, p.m_brute_force_est_parts);

			if (has_est_parts3)
			{
				// Always try direct, optionally base+scale cem's
				for (uint32_t s = 0; s < 2; s++)
				{
					if ((s) && (!p.m_use_base_scale_modes))
						continue;

					if (p.m_disable_part_est_stage2)
					{
						m_num_est_parts3[s] = total_parts3;
						memcpy(m_best_parts3[s], best_parts3_temp, m_num_est_parts3[s] * sizeof(int));
						continue;
					}

					uint32_t cem_to_surrogate_encode = p.m_alpha_cems ? astc_helpers::CEM_LDR_RGBA_DIRECT : astc_helpers::CEM_LDR_RGB_DIRECT;
					if (s)
						cem_to_surrogate_encode = p.m_alpha_cems ? astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A : astc_helpers::CEM_LDR_RGB_BASE_SCALE;

					// Stage 2: Analytic surrogate WSSE
					basisu::vector<float> part_sses(total_parts3);
					for (int i = 0; i < total_parts3; i++)
					{
						const astc_ldr::partitions_data* pPart_data = p.m_pPart_data_p3;

						const uint32_t unique_seed_index = best_parts3_temp[i];
						const uint32_t part_seed_index = pPart_data->m_unique_index_to_part_seed[unique_seed_index];

						const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[unique_seed_index];

						log_surrogate_astc_blk surrogate_log_blk;
						float sse;
						if (p.m_use_fast_part_est_stage2)
						{
							sse = estimate_partition_sse(cem_to_surrogate_encode, pixel_stats, p.m_block_width, p.m_block_height, 3, pPat);
						}
						else
						{
							sse = encode_surrogate_trial_subsets(
								p.m_block_width, p.m_block_height,
								pixel_stats,
								cem_to_surrogate_encode, 3, part_seed_index, pPat,
								astc_helpers::BISE_256_LEVELS, astc_helpers::BISE_64_LEVELS,
								p.m_block_width, p.m_block_height,
								surrogate_log_blk,
								*p.m_pEnc_params, surrogate_encode_flags);
							
							stats.m_total_surrogate_encodes++;
						}
												
						part_sses[i] = sse;
					} // i

					basisu::vector<uint32_t> part_sses_ranks(total_parts3);

					indirect_sort(total_parts3, part_sses_ranks.get_ptr(), part_sses.get_ptr());

					m_num_est_parts3[s] = maximum<int>(1, (total_parts3 + p.m_part3_fraction_to_keep - 1) / p.m_part3_fraction_to_keep);

					for (int i = 0; i < m_num_est_parts3[s]; i++)
					{
						const uint32_t rank_index = part_sses_ranks[i];
						const uint32_t unique_seed_unique = best_parts3_temp[rank_index];
						m_best_parts3[s][i] = unique_seed_unique;
					} // i

				} // s

			} // if (has_est_parts3)

		} // if (total_parts3)

		return true;
	}

	bool trivial_triage(
		const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		BASISU_NOTE_UNUSED(pixel_stats);
		BASISU_NOTE_UNUSED(stats);
		BASISU_NOTE_UNUSED(out_blocks);
		BASISU_NOTE_UNUSED(blur_id);
				
		if (m_trial_modes_to_estimate.capacity() < 1024)
			m_trial_modes_to_estimate.reserve(1024);
		m_trial_modes_to_estimate.resize(0);

		assert((astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET + 1) == basist::astc_ldr_t::OTM_NUM_CEMS);

		for (uint32_t cem_index = astc_helpers::CEM_LDR_LUM_DIRECT; cem_index < basist::astc_ldr_t::OTM_NUM_CEMS; cem_index++)
		{
			if (astc_helpers::does_cem_have_alpha(cem_index) != p.m_alpha_cems)
				continue;

			const bool cem_has_alpha = astc_helpers::does_cem_have_alpha(cem_index);
			if (cem_has_alpha != p.m_use_alpha_or_opaque_modes)
				continue;

			bool accept_flag = false;
			switch (cem_index)
			{
			case astc_helpers::CEM_LDR_LUM_DIRECT:
			case astc_helpers::CEM_LDR_LUM_ALPHA_DIRECT:
			{
				accept_flag = p.m_use_lum_direct_modes;
				break;
			}
			case astc_helpers::CEM_LDR_RGB_DIRECT:
			case astc_helpers::CEM_LDR_RGBA_DIRECT:
			{
				accept_flag = p.m_use_direct_modes;
				break;
			}
			case astc_helpers::CEM_LDR_RGB_BASE_SCALE:
			case astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A:
			{
				accept_flag = p.m_use_base_scale_modes;
				break;
			}
			default:
				break;
			}

			if (!accept_flag)
				continue;

			const uint32_t s = astc_helpers::cem_is_ldr_base_scale(cem_index) ? 1 : 0;

			for (uint32_t subsets_index = 0; subsets_index < basist::astc_ldr_t::OTM_NUM_SUBSETS; subsets_index++)
			{
				if (subsets_index == 1)
				{
					if (!m_num_est_parts2[s])
						continue;
				}
				else if (subsets_index == 2)
				{
					if (!m_num_est_parts3[s])
						continue;
				}

				const uint32_t ccs_max_index = (p.m_use_dual_planes ? basist::astc_ldr_t::OTM_NUM_CCS : 1);
				for (uint32_t ccs_index = 0; ccs_index < ccs_max_index; ccs_index++)
				{
					if (ccs_index)
					{
						if (!p.m_dp_active_chans[ccs_index - 1])
							continue;
					}

					for (uint32_t grid_size_index = 0; grid_size_index < basist::astc_ldr_t::OTM_NUM_GRID_SIZES; grid_size_index++)
					{
						if (grid_size_index) 
						{
							// if "large" grid (gw>=(bw-1)) and (gh>=(bh-1)) - quite conservative filter
							if (p.m_use_small_grids_only)
								continue;
						}

						for (uint32_t grid_anisos_index = 0; grid_anisos_index < basist::astc_ldr_t::OTM_NUM_GRID_ANISOS; grid_anisos_index++)
						{
							if (p.m_grid_hv_filtering)
							{
								if (grid_anisos_index == 1)
								{
									// W_fract >= H_fract
									if (p.m_filter_horizontally_flag)
										continue;
								}
								else if (grid_anisos_index == 2)
								{
									// W_fract < H_fract
									if (!p.m_filter_horizontally_flag)
										continue;
								}
							}

							m_trial_modes_to_estimate.append(p.m_pGrouped_trial_modes->m_tm_groups[cem_index][subsets_index][ccs_index][grid_size_index][grid_anisos_index]);
							
						} // grid_aniso_index

					} // grid_size_index

				} // ccs_index

			} // subsets_index

		} // cem_iter

		if (!m_trial_modes_to_estimate.size())
		{
			assert(0);
			return false;
		}
				
		return true;
	}

	bool analytic_triage(
		const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		BASISU_NOTE_UNUSED(blur_id);
		BASISU_NOTE_UNUSED(out_blocks);

		//--------------------------------- superbucket analytical estimation

		shortlist_bucket_hash_t& shortlist_buckets = m_shortlist_hash0;
				
		if (m_shortlist_hash0.get_table_size() != EXPECTED_SHORTLIST_HASH_SIZE)
		{
			const bool was_allocated = m_shortlist_hash0.get_table_size() > 0;

			m_shortlist_hash0.clear();
			m_shortlist_hash0.reserve(EXPECTED_SHORTLIST_HASH_SIZE / 2);

			if ((g_devel_messages) && (was_allocated))
				fmt_debug_printf("shortlist hash0 thrash\n");
		}
		else
		{
			m_shortlist_hash0.reset();
		}

		m_used_superbuckets = false;

		if (p.m_use_superbuckets)
		{
			m_used_superbuckets = true;
			
			// This may thrash if it grows larger on another thread, but we must avoid determinism issues.
			if (m_superbucket_hash.get_table_size() != EXPECTED_SUPERBUCKET_HASH_SIZE)
			{
				const bool was_allocated = m_superbucket_hash.get_table_size() > 0;

				m_superbucket_hash.clear();
				m_superbucket_hash.reserve(EXPECTED_SUPERBUCKET_HASH_SIZE >> 1);

				if ((g_devel_messages) && (was_allocated))
					fmt_debug_printf("superbucket hash thrash\n");
			}
			else
			{
				m_superbucket_hash.reset();
			}
						
			trial_mode_estimate_superbucket_key new_key;
			new_key.clear();

			trial_mode_estimate_superbucket_value new_val;

			// Create superbuckets
			uint32_t max_superbucket_tm_indices = 0;

			trial_mode_estimate_superbucket_hash::insert_result superbucket_ins_res;
			bool superbucket_ins_res_is_valid = false;
			
			for (uint32_t j = 0; j < m_trial_modes_to_estimate.size(); j++)
			{
				const uint32_t trial_mode_iter = m_trial_modes_to_estimate[j];

				assert(trial_mode_iter < p.m_num_trial_modes);
				const basist::astc_ldr_t::trial_mode& tm = p.m_pTrial_modes[trial_mode_iter];

				new_key.m_cem_index = safe_cast_uint8(tm.m_cem);
				new_key.m_ccs_index = safe_cast_int8(tm.m_ccs_index);

				new_key.m_subset_unique_index = 0;
				new_key.m_num_subsets = (uint8_t)tm.m_num_parts;

				if (tm.m_num_parts == 1)
				{
					bool created_flag = false;
					if ((!superbucket_ins_res_is_valid) || (new_key != (superbucket_ins_res.first)->first))
					{
						superbucket_ins_res = m_superbucket_hash.insert(new_key, new_val);
						created_flag = superbucket_ins_res.second;
						superbucket_ins_res_is_valid = true;
					}

					assert(superbucket_ins_res.first->first.m_cem_index == tm.m_cem);
					assert(superbucket_ins_res.first->first.m_ccs_index == tm.m_ccs_index);
					assert(superbucket_ins_res.first->first.m_num_subsets == tm.m_num_parts);

					trial_mode_estimate_superbucket_value& v = (superbucket_ins_res.first)->second;

					if (created_flag)
						v.m_trial_mode_list.reserve(256);

					v.m_trial_mode_list.push_back(trial_mode_iter);

					max_superbucket_tm_indices = maximum(max_superbucket_tm_indices, v.m_trial_mode_list.size_u32());
				}
				else
				{
					//const astc_ldr::partitions_data* pPart_data = (tm.m_num_parts == 2) ? p.m_pPart_data_p2 : p.m_pPart_data_p3;

					const uint32_t s = astc_helpers::cem_is_ldr_base_scale(tm.m_cem) ? 1 : 0;
					const uint32_t num_est_parts_to_try = (tm.m_num_parts == 2) ? m_num_est_parts2[s] : m_num_est_parts3[s];

					for (uint32_t est_part_iter = 0; est_part_iter < num_est_parts_to_try; est_part_iter++)
					{
						const uint32_t part_unique_index = (tm.m_num_parts == 2) ? m_best_parts2[s][est_part_iter] : m_best_parts3[s][est_part_iter];

						new_key.m_subset_unique_index = safe_cast_uint16(part_unique_index);

						bool created_flag = false;
						if ((!superbucket_ins_res_is_valid) || (new_key != (superbucket_ins_res.first)->first))
						{
							superbucket_ins_res = m_superbucket_hash.insert(new_key, new_val);
							created_flag = superbucket_ins_res.second;
							superbucket_ins_res_is_valid = true;
						}

						assert(superbucket_ins_res.first->first.m_cem_index == tm.m_cem);
						assert(superbucket_ins_res.first->first.m_ccs_index == tm.m_ccs_index);
						assert(superbucket_ins_res.first->first.m_num_subsets == tm.m_num_parts);

						trial_mode_estimate_superbucket_value& v = (superbucket_ins_res.first)->second;
						if (created_flag)
							v.m_trial_mode_list.reserve(256);

						v.m_trial_mode_list.push_back(trial_mode_iter);

						max_superbucket_tm_indices = maximum(max_superbucket_tm_indices, v.m_trial_mode_list.size_u32());

					} // est_part_iter
				}

			} // j

			//fmt_debug_printf("Total superbucket entries: {}\n", m_superbucket_hash.size());
			//fmt_debug_printf("Max superbucket tm indices: {}\n", max_superbucket_tm_indices);

			const uint32_t total_block_texels = p.m_total_block_pixels;
			const float inv_total_block_texels = 1.0f / (float)total_block_texels;
						
			while (m_trial_mode_estimate_priority_queue.size())
				m_trial_mode_estimate_priority_queue.pop();

			const uint32_t max_priority_queue_size = p.m_superbucket_max_to_retain[m_block_complexity_index];

			const float SLAM_TO_LINE_WEIGHT = 1.5f; // upweight STL relative to other errors to give the estimator more of a signal especially for dual plane
			const float QUANT_ERROR_WEIGHT = 1.0f; // endpoint/weight quant error
			const float SCALE_ERROR_WEIGHT = 3.0f; // weight grid downsample (scale) error
						
			// Discount for blue contraction encoding and base+offset CEM's.
			const float BLUE_CONTRACTION_ENDPOINT_QUANT_DISCOUNT = .5f;

			// Iterate over all superbuckets, surrogate encode to compute slam to line error, DCT of weight grid(s) to estimate energy lost during weight grid downsampling.
			for (auto superbucket_iter = m_superbucket_hash.begin(); superbucket_iter != m_superbucket_hash.end(); ++superbucket_iter)
			{
				const trial_mode_estimate_superbucket_key& key = superbucket_iter->first;
				const trial_mode_estimate_superbucket_value& val = superbucket_iter->second;

				//const bool cem_has_alpha = astc_helpers::does_cem_have_alpha(key.m_cem_index);

				log_surrogate_astc_blk log_blk;

				const astc_ldr::partitions_data* pPart_data = nullptr;
				const astc_ldr::partition_pattern_vec* pPat = nullptr;

				//const uint32_t num_planes = (key.m_ccs_index >= 0) ? 2 : 1;

				const float worst_wsse_found_so_far = (m_trial_mode_estimate_priority_queue.size() >= max_priority_queue_size) ? m_trial_mode_estimate_priority_queue.top().m_wsse : 1e+9f;

				float slam_to_line_wsse = 0;
				if (key.m_num_subsets == 1)
				{
					slam_to_line_wsse = encode_surrogate_trial(
						p.m_block_width, p.m_block_height,
						pixel_stats,
						key.m_cem_index,
						key.m_ccs_index,
						astc_helpers::BISE_256_LEVELS, astc_helpers::BISE_64_LEVELS,
						p.m_block_width, p.m_block_height,
						log_blk,
						*p.m_pEnc_params,
						astc_ldr::cFlagDisableQuant);
				}
				else
				{
					pPart_data = (key.m_num_subsets == 3) ? p.m_pPart_data_p3 : p.m_pPart_data_p2;

					const uint32_t unique_seed_index = key.m_subset_unique_index;
					const uint32_t part_seed_index = pPart_data->m_unique_index_to_part_seed[unique_seed_index];

					pPat = &pPart_data->m_partition_pats[unique_seed_index];

					slam_to_line_wsse = encode_surrogate_trial_subsets(
						p.m_block_width, p.m_block_height,
						pixel_stats,
						key.m_cem_index, key.m_num_subsets, part_seed_index, pPat,
						astc_helpers::BISE_256_LEVELS, astc_helpers::BISE_64_LEVELS,
						p.m_block_width, p.m_block_height,
						log_blk,
						*p.m_pEnc_params,
						astc_ldr::cFlagDisableQuant);
				}

				stats.m_total_surrogate_encodes++;

				// Early out: Slam to line error is so high it's impossible for any blocks in this bucket to win.
				if ((SLAM_TO_LINE_WEIGHT * slam_to_line_wsse) >= worst_wsse_found_so_far)
					continue;
								
				bool can_use_base_ofs = false;
				if ((key.m_cem_index == astc_helpers::CEM_LDR_RGB_DIRECT) || (key.m_cem_index == astc_helpers::CEM_LDR_RGBA_DIRECT))
				{
					float max_span_size = 0.0f;

					for (uint32_t subset_index = 0; subset_index < key.m_num_subsets; subset_index++)
					{
						const vec4F subset_chan_spans(log_blk.m_endpoints[subset_index][1] - log_blk.m_endpoints[subset_index][0]);
						for (uint32_t c = 0; c < 4; c++)
						{
							float span_size = fabs(subset_chan_spans[c]);
							max_span_size = maximum(max_span_size, span_size);
						}
					}

					can_use_base_ofs = (max_span_size < .25f);
				}
								
				assert(p.m_pDCT2F);

				assert((p.m_pDCT2F->rows() == p.m_block_height) && (p.m_pDCT2F->cols() == p.m_block_width));

				float weight0_energy[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
				float weight1_energy[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

				float weight0_energy_prefix_sum[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
				float weight1_energy_prefix_sum[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

				basist::astc_ldr_t::fvec& dct_work = m_dct_work;

				// Forward DCT in normalized weight (surrogate) space
				p.m_pDCT2F->forward(log_blk.m_weights0, weight0_energy, dct_work);
				const float total_weight0_ac_energy = compute_ac_energy_from_dct(p.m_block_width, p.m_block_height, weight0_energy);
				prepare_dct_energy_prefix_table(p.m_block_width, p.m_block_height, weight0_energy, weight0_energy_prefix_sum);

				float total_weight1_ac_energy = 0.0f;

				if (key.m_ccs_index >= 0)
				{
					p.m_pDCT2F->forward(log_blk.m_weights1, weight1_energy, dct_work);

					total_weight1_ac_energy = compute_ac_energy_from_dct(p.m_block_width, p.m_block_height, weight1_energy);
					prepare_dct_energy_prefix_table(p.m_block_width, p.m_block_height, weight1_energy, weight1_energy_prefix_sum);
				}

				weight_terms weight0_terms, weight1_terms;
				weight_terms* pWeight0_terms = &weight0_terms;
				weight_terms* pWeight1_terms = nullptr;
				// TODO: These correction factors are not per-subset, but per-block. 
				weight0_terms.calc(total_block_texels, log_blk.m_weights0);
				if (key.m_ccs_index >= 0)
				{
					weight1_terms.calc(total_block_texels, log_blk.m_weights1);
					pWeight1_terms = &weight1_terms;
				}

				// Precompute subset span and total pixels info
				vec4F subset_spans[astc_helpers::MAX_PARTITIONS];
				uint32_t subset_pixels[astc_helpers::MAX_PARTITIONS];

				for (uint32_t subset_index = 0; subset_index < key.m_num_subsets; subset_index++)
				{
					subset_spans[subset_index] = log_blk.m_endpoints[subset_index][1] - log_blk.m_endpoints[subset_index][0];

					uint32_t total_subset_pixels = p.m_total_block_pixels;
					if (key.m_num_subsets > 1)
						total_subset_pixels = pPart_data->m_partition_pat_histograms[key.m_subset_unique_index].m_hist[subset_index];
					
					subset_pixels[subset_index] = total_subset_pixels;
				}

				// Loop through all trial modes in this superbucket. TODO: Sort by endpoint levels?
				for (uint32_t k = 0; k < val.m_trial_mode_list.size(); k++)
				{
					const uint32_t trial_mode_index = val.m_trial_mode_list[k];
					assert(trial_mode_index < p.m_num_trial_modes);

					const basist::astc_ldr_t::trial_mode& tm = p.m_pTrial_modes[trial_mode_index];

					assert(tm.m_cem == key.m_cem_index);
					assert(tm.m_ccs_index == key.m_ccs_index);
					assert(tm.m_num_parts == key.m_num_subsets);

					const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(p.m_block_width, p.m_block_height, tm.m_grid_width, tm.m_grid_height);

					const uint32_t total_endpoint_levels = astc_helpers::get_ise_levels(tm.m_endpoint_ise_range);
					const uint32_t total_weight_levels = astc_helpers::get_ise_levels(tm.m_weight_ise_range);

					const uint32_t num_effective_e_levels = can_use_base_ofs ? minimum<uint32_t>(total_endpoint_levels * 2, 256) : total_endpoint_levels;
					float qe0 = compute_quantized_channel_endpoint_mse_estimate(num_effective_e_levels);
					const float qe1 = (key.m_ccs_index >= 0) ? (qe0 * pWeight1_terms->m_endpoint_factor) : 0.0f;
					qe0 *= pWeight0_terms->m_endpoint_factor;

					float total_e_quant_wsse = 0.0f;

					for (uint32_t subset_index = 0; subset_index < key.m_num_subsets; subset_index++)
					{
						const vec4F& subset_chan_spans = subset_spans[subset_index];
						const uint32_t total_subset_pixels = subset_pixels[subset_index];

						for (uint32_t c = 0; c < 4; c++)
						{
							float span_size = fabs(subset_chan_spans[c]);

							if ((span_size == 0.0f) && ((log_blk.m_endpoints[subset_index][1][c] == 0.0f) || (log_blk.m_endpoints[subset_index][1][c] == 1.0f)))
								continue;
							
							// Scale channel MSE by chan weight and the # of subset pixels to get weighted SSE
							const float chan_N = (float)p.m_pEnc_params->m_comp_weights[c] * (float)total_subset_pixels;

							total_e_quant_wsse += ((key.m_ccs_index == (int)c) ? qe1 : qe0) * chan_N;

						} // chan_index
					}

					// TODO: perhaps we can rapidly predict when blue contraction can actually be applied based off each subset's endpoints.
					if ((tm.m_cem == astc_helpers::CEM_LDR_RGB_DIRECT) || (tm.m_cem == astc_helpers::CEM_LDR_RGBA_DIRECT))
						total_e_quant_wsse *= BLUE_CONTRACTION_ENDPOINT_QUANT_DISCOUNT;

					float total_wsse_so_far = (SLAM_TO_LINE_WEIGHT * slam_to_line_wsse) + (QUANT_ERROR_WEIGHT * total_e_quant_wsse);
					if (total_wsse_so_far >= worst_wsse_found_so_far)
						continue;

					//const float lost_weight_energy0 = compute_lost_dct_energy(p.m_block_width, p.m_block_height, weight0_energy, tm.m_grid_width, tm.m_grid_height) * inv_total_block_texels;
					//assert(basisu::equal_tol(lost_weight_energy0, compute_lost_dct_energy_orig(p.m_block_width, p.m_block_height, weight0_energy, tm.m_grid_width, tm.m_grid_height) * inv_total_block_texels, .0000125f));

					const float lost_weight_energy0_prefix_sum = compute_lost_dct_energy_prefix_sum(p.m_block_width, p.m_block_height, weight0_energy_prefix_sum, tm.m_grid_width, tm.m_grid_height, total_weight0_ac_energy) * inv_total_block_texels;
					//assert(basisu::equal_tol(lost_weight_energy0, lost_weight_energy0_prefix_sum, .00125f));
					const float lost_weight_energy0 = lost_weight_energy0_prefix_sum;

					float lost_weight_energy1 = 0;
					if (key.m_ccs_index >= 0)
					{
						//lost_weight_energy1 = compute_lost_dct_energy(p.m_block_width, p.m_block_height, weight1_energy, tm.m_grid_width, tm.m_grid_height) * inv_total_block_texels;
						//assert(basisu::equal_tol(lost_weight_energy1, compute_lost_dct_energy_orig(p.m_block_width, p.m_block_height, weight1_energy, tm.m_grid_width, tm.m_grid_height) * inv_total_block_texels, .0000125f));

						const float lost_weight_energy1_prefix_sum = compute_lost_dct_energy_prefix_sum(p.m_block_width, p.m_block_height, weight1_energy_prefix_sum, tm.m_grid_width, tm.m_grid_height, total_weight1_ac_energy) * inv_total_block_texels;
						//assert(basisu::equal_tol(lost_weight_energy1, lost_weight_energy1_prefix_sum, .00125f));
						
						lost_weight_energy1 = lost_weight_energy1_prefix_sum;
					}
										
					// Add up:
					// slam to line error WSSE (weighted sum of squared errors)
					// weight quant error WSSE
					// endpoint quant error WSSE
					// weight grid rescale error WSSE (scaled by span^2)
					if ((lost_weight_energy0 != 0.0f) || (lost_weight_energy1 != 0.0f))
					{
						float total_scale_wsse = 0.0f;

						for (uint32_t subset_index = 0; subset_index < key.m_num_subsets; subset_index++)
						{
							const vec4F& subset_chan_spans = subset_spans[subset_index];
							const uint32_t total_subset_pixels = subset_pixels[subset_index];

							for (uint32_t c = 0; c < 4; c++)
							{
								float span_size = fabs(subset_chan_spans[c]);

								if ((span_size == 0.0f) && ((log_blk.m_endpoints[subset_index][1][c] == 0.0f) || (log_blk.m_endpoints[subset_index][1][c] == 1.0f)))
								{
									// Won't have any E/W quant err at extremes (0.0 or 1.0 are always perfectly represented), no weight downsample error either.
									//chan_mse.m_ep = 0.0f;
									//chan_mse.m_wp = 0.0f;
								}
								else
								{
									// Scale channel MSE by chan weight and the # of subset pixels to get weighted SSE
									const float chan_N = (float)p.m_pEnc_params->m_comp_weights[c] * (float)total_subset_pixels;

									// sum in the plane's lost weight energy, scaled by span_size^2 * chan_weight * num_texels_covered
									if (key.m_ccs_index == (int)c)
										total_scale_wsse += lost_weight_energy1 * square(span_size) * chan_N;
									else
										total_scale_wsse += lost_weight_energy0 * square(span_size) * chan_N;
								}

							} // chan_index
						}

						total_wsse_so_far += (SCALE_ERROR_WEIGHT * total_scale_wsse);
						if (total_wsse_so_far >= worst_wsse_found_so_far)
							continue;
					}

					float total_w_quant_wsse = 0.0f;
					for (uint32_t subset_index = 0; subset_index < key.m_num_subsets; subset_index++)
					{
						const vec4F& subset_chan_spans = subset_spans[subset_index];
						const uint32_t total_subset_pixels = subset_pixels[subset_index];

						for (uint32_t c = 0; c < 4; c++)
						{
							float span_size = fabs(subset_chan_spans[c]);

							if ((span_size == 0.0f) && ((log_blk.m_endpoints[subset_index][1][c] == 0.0f) || (log_blk.m_endpoints[subset_index][1][c] == 1.0f)))
							{
								// Won't have any E/W quant err at extremes (0.0 or 1.0 are always perfectly represented), no weight downsample error either.
								//chan_mse.m_ep = 0.0f;
								//chan_mse.m_wp = 0.0f;
							}
							else
							{
								// span_size != 0 here - estimate weight/endpoint quantization errors
								float chan_w_mse = compute_quantized_channel_weight_mse_estimate(
									total_weight_levels, span_size,
									pGrid_data->m_weight_gamma, (key.m_ccs_index == (int)c) ? pWeight1_terms : pWeight0_terms);

								// Scale channel MSE by chan weight and the # of subset pixels to get weighted SSE
								const float chan_N = (float)p.m_pEnc_params->m_comp_weights[c] * (float)total_subset_pixels;

								total_w_quant_wsse += chan_w_mse * chan_N;
							}

						} // chan_index

					} // subset_index
															
					const float total_wsse = total_wsse_so_far + (QUANT_ERROR_WEIGHT * total_w_quant_wsse);
					
					if (m_trial_mode_estimate_priority_queue.size() >= max_priority_queue_size)
					{
						if (total_wsse < m_trial_mode_estimate_priority_queue.top().m_wsse)
						{
							m_trial_mode_estimate_priority_queue.pop();

							trial_mode_estimate est;
							est.m_superbucket_key = key;
							est.m_trial_mode_index = trial_mode_index;
							est.m_wsse = total_wsse;

							m_trial_mode_estimate_priority_queue.push(est);
						}
					}
					else
					{
						trial_mode_estimate est;
						est.m_superbucket_key = key;
						est.m_trial_mode_index = trial_mode_index;
						est.m_wsse = total_wsse;

						m_trial_mode_estimate_priority_queue.push(est);
					}

				} // k

			} // superbucket_iter

			stats.m_total_superbuckets_created += m_superbucket_hash.size_u32();

			const uint32_t total_estimates_to_retain = (uint32_t)m_trial_mode_estimate_priority_queue.size();
			assert(total_estimates_to_retain);
						
			for (uint32_t i = 0; i < total_estimates_to_retain; i++)
			{
				const trial_mode_estimate &est = m_trial_mode_estimate_priority_queue.top();
				
				const trial_mode_estimate_superbucket_key& key = est.m_superbucket_key;
				const uint32_t trial_mode_iter = est.m_trial_mode_index;

				assert(trial_mode_iter < p.m_num_trial_modes);
				const basist::astc_ldr_t::trial_mode& tm = p.m_pTrial_modes[trial_mode_iter];

				assert(tm.m_cem == key.m_cem_index);
				assert(tm.m_ccs_index == key.m_ccs_index);
				assert(tm.m_num_parts == key.m_num_subsets);

				const uint32_t part_unique_index = key.m_subset_unique_index;

				auto ins_res = shortlist_buckets.insert(shortlist_bucket(tm.m_grid_width, tm.m_grid_height, tm.m_cem, tm.m_ccs_index, tm.m_num_parts, part_unique_index));

				ins_res.first->second.push_back(safe_cast_uint16(trial_mode_iter));

				m_trial_mode_estimate_priority_queue.pop();
			}
		}
		else
		{
			for (uint32_t j = 0; j < m_trial_modes_to_estimate.size(); j++)
			{
				const uint32_t trial_mode_iter = m_trial_modes_to_estimate[j];

				assert(trial_mode_iter < p.m_num_trial_modes);
				const basist::astc_ldr_t::trial_mode& tm = p.m_pTrial_modes[trial_mode_iter];

				if (tm.m_num_parts > 1)
				{
					//const astc_ldr::partitions_data* pPart_data = (tm.m_num_parts == 2) ? p.m_pPart_data_p2 : p.m_pPart_data_p3;

					const uint32_t s = astc_helpers::cem_is_ldr_base_scale(tm.m_cem) ? 1 : 0;
					const uint32_t num_est_parts_to_try = (tm.m_num_parts == 2) ? m_num_est_parts2[s] : m_num_est_parts3[s];

					for (uint32_t est_part_iter = 0; est_part_iter < num_est_parts_to_try; est_part_iter++)
					{
						const uint32_t part_unique_index = (tm.m_num_parts == 2) ? m_best_parts2[s][est_part_iter] : m_best_parts3[s][est_part_iter];

						auto ins_res = shortlist_buckets.insert(shortlist_bucket(tm.m_grid_width, tm.m_grid_height, tm.m_cem, tm.m_ccs_index, tm.m_num_parts, part_unique_index));

						ins_res.first->second.push_back(safe_cast_uint16(trial_mode_iter));

					} // est_part_iter

				}
				else
				{
					auto ins_res = shortlist_buckets.insert(shortlist_bucket(tm.m_grid_width, tm.m_grid_height, tm.m_cem, tm.m_ccs_index, 1, 0));
					ins_res.first->second.push_back(safe_cast_uint16(trial_mode_iter));

				}
			}
		}

		stats.m_total_buckets_created += (uint32_t)shortlist_buckets.size();

#if 0
		// TEMP
		uint32_t max_bucket_tm_indices = 0;
		for (auto it = shortlist_buckets.begin(); it != shortlist_buckets.end(); ++it)
		{
			shortlist_bucket& bucket = it->first;
			trial_mode_index_vec& trial_mode_indices = it->second;
			max_bucket_tm_indices = maximum<uint32_t>(max_bucket_tm_indices, trial_mode_indices.size_u32());
		}

		fmt_debug_printf("max_bucket_tm_indices: {}\n", max_bucket_tm_indices);
#endif

		return true;
	}

	bool surrogate_encode_shortlist_bucket_representatives(
		const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		BASISU_NOTE_UNUSED(blur_id);
		BASISU_NOTE_UNUSED(out_blocks);

		shortlist_bucket_hash_t& shortlist_buckets = m_shortlist_hash0;

		// Surrogate encode a representative for each bucket.
		for (auto it = shortlist_buckets.begin(); it != shortlist_buckets.end(); ++it)
		{
			shortlist_bucket& bucket = it->first;
			//const uint_vec& trial_mode_indices = it->second;
			const trial_mode_index_vec& trial_mode_indices = it->second;

			// Choose bucket's largest endpoint/weight ise ranges (finest quant levels) - anything in the bucket will quite likely encode to worse SSE, which we can rapidly estimate.
			uint32_t max_endpoint_ise_range = 0, max_weight_ise_range = 0;
			for (uint32_t i = 0; i < trial_mode_indices.size(); i++)
			{
				const basist::astc_ldr_t::trial_mode& tm = p.m_pTrial_modes[trial_mode_indices[i]];

				max_endpoint_ise_range = maximum(max_endpoint_ise_range, tm.m_endpoint_ise_range);
				max_weight_ise_range = maximum(max_weight_ise_range, tm.m_weight_ise_range);
			}

			log_surrogate_astc_blk& log_block = bucket.m_surrogate_log_blk;

			if (bucket.m_num_parts == 1)
			{
				bucket.m_sse = encode_surrogate_trial(
					p.m_block_width, p.m_block_height,
					pixel_stats,
					bucket.m_cem_index,
					bucket.m_ccs_index,
					max_endpoint_ise_range, max_weight_ise_range,
					bucket.m_grid_width, bucket.m_grid_height,
					log_block,
					*p.m_pEnc_params, 0);

				stats.m_total_surrogate_encodes++;
			}
			else
			{
				const astc_ldr::partitions_data* pPart_data = (bucket.m_num_parts == 2) ? p.m_pPart_data_p2 : p.m_pPart_data_p3;

				const uint32_t part_seed_index = pPart_data->m_unique_index_to_part_seed[bucket.m_unique_seed_index];

				const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[bucket.m_unique_seed_index];

				bucket.m_sse = encode_surrogate_trial_subsets(
					p.m_block_width, p.m_block_height,
					pixel_stats,
					bucket.m_cem_index, bucket.m_num_parts, part_seed_index, pPat,
					max_endpoint_ise_range, max_weight_ise_range,
					bucket.m_grid_width, bucket.m_grid_height,
					log_block,
					*p.m_pEnc_params, 0);

				stats.m_total_surrogate_encodes++;
			}

			if ((bucket.m_cem_index == astc_helpers::CEM_LDR_RGB_DIRECT) || (bucket.m_cem_index == astc_helpers::CEM_LDR_RGBA_DIRECT))
			{
				// blue contraction/base+offset discount
				bucket.m_sse *= BLUE_CONTRACTION_BASE_OFS_DISCOUNT;
			}

		} // it

		return true;
	}

	bool prune_shortlist_buckets(
		const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		BASISU_NOTE_UNUSED(pixel_stats);
		BASISU_NOTE_UNUSED(stats);
		BASISU_NOTE_UNUSED(blur_id);
		BASISU_NOTE_UNUSED(out_blocks);

		shortlist_bucket_hash_t& shortlist_buckets = m_shortlist_hash0;

		if (p.m_bucket_pruning_passes)
		{
			shortlist_bucket_hash_t& new_shortlist_buckets = m_shortlist_hash1;
			
			if (m_shortlist_hash1.get_table_size() != EXPECTED_SHORTLIST_HASH_SIZE)
			{
				const bool was_allocated = m_shortlist_hash1.get_table_size() > 0;

				m_shortlist_hash1.clear();
				m_shortlist_hash1.reserve(EXPECTED_SHORTLIST_HASH_SIZE / 2);

				if ((g_devel_messages) && (was_allocated))
					fmt_debug_printf("shortlist hash1 thrash\n");
			}
			else
			{
				m_shortlist_hash1.reset();
			}

			const uint32_t NUM_PRUNE_PASSES = 3;
			for (uint32_t prune_pass = 0; prune_pass < NUM_PRUNE_PASSES; prune_pass++)
			{
				for (auto it = shortlist_buckets.begin(); it != shortlist_buckets.end(); ++it)
					it->first.m_examined_flag = false;

				new_shortlist_buckets.reset();

				for (auto it = shortlist_buckets.begin(); it != shortlist_buckets.end(); ++it)
				{
					shortlist_bucket& bucket = it->first;

					if (bucket.m_examined_flag)
						continue;

					if (prune_pass == 0)
					{
						// Prune pass 0: Dual plane groups: only accept best CCS index
						if (bucket.m_ccs_index >= 0)
						{
							shortlist_bucket_hash_t::iterator ccs_buckets[4];

							int best_ccs_index = -1;
							float best_ccs_err = BIG_FLOAT_VAL;

							bool skip_bucket = false;
							for (uint32_t c = 0; c < 4; c++)
							{
								auto ccs_res_it = shortlist_buckets.find(shortlist_bucket(bucket.m_grid_width, bucket.m_grid_height, bucket.m_cem_index, c, bucket.m_num_parts, bucket.m_unique_seed_index));
								ccs_buckets[c] = ccs_res_it;

								if (ccs_res_it == shortlist_buckets.end())
									continue;

								assert(!ccs_res_it->first.m_examined_flag);

								ccs_res_it->first.m_examined_flag = true;

								float ccs_sse_err = ccs_res_it->first.m_sse;
								if (ccs_sse_err < best_ccs_err)
								{
									best_ccs_err = ccs_sse_err;
									best_ccs_index = c;
								}
							} // c

							if (!skip_bucket)
							{
								assert(best_ccs_index >= 0);

								shortlist_bucket_hash_t::iterator best_ccs_it = ccs_buckets[best_ccs_index];
								assert(best_ccs_it != shortlist_buckets.end());

								new_shortlist_buckets.insert(best_ccs_it->first, best_ccs_it->second);
							}
						}
						else
						{
							new_shortlist_buckets.insert(it->first, it->second);
						}
					}
					else if (prune_pass == 1)
					{
						// Prune pass 1: Same # of weight samples, compare WxH vs. HxW
						if (bucket.m_grid_width != bucket.m_grid_height)
						{
							auto alt_res_it = shortlist_buckets.find(shortlist_bucket(bucket.m_grid_height, bucket.m_grid_width, bucket.m_cem_index, bucket.m_ccs_index, bucket.m_num_parts, bucket.m_unique_seed_index));
							if (alt_res_it == shortlist_buckets.end())
							{
								new_shortlist_buckets.insert(it->first, it->second);
							}
							else
							{
								assert(!alt_res_it->first.m_examined_flag);
								alt_res_it->first.m_examined_flag = true;

								const float fract = (bucket.m_sse > 0.0f) ? (alt_res_it->first.m_sse / bucket.m_sse) : 0.0f;

								const float ALT_RES_SSE_THRESH = .2f;
								if (fract < (1.0f - ALT_RES_SSE_THRESH))
									new_shortlist_buckets.insert(alt_res_it->first, alt_res_it->second);
								else if (fract > (1.0f + ALT_RES_SSE_THRESH))
									new_shortlist_buckets.insert(it->first, it->second);
								else
								{
									new_shortlist_buckets.insert(alt_res_it->first, alt_res_it->second);
									new_shortlist_buckets.insert(it->first, it->second);
								}
							}
						}
						else
						{
							new_shortlist_buckets.insert(it->first, it->second);
						}

					}
					else if (prune_pass == 2)
					{
						// Prune pass 2: RGB Direct vs. Scale bucket groups

						if ((bucket.m_cem_index == astc_helpers::CEM_LDR_RGB_DIRECT) || (bucket.m_cem_index == astc_helpers::CEM_LDR_RGB_BASE_SCALE) ||
							(bucket.m_cem_index == astc_helpers::CEM_LDR_RGBA_DIRECT) || (bucket.m_cem_index == astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A))
						{
							uint32_t alt_cem_index_to_find = astc_helpers::CEM_LDR_RGB_BASE_SCALE;

							// Check for pairs: CEM_LDR_RGB_DIRECT vs. CEM_LDR_RGB_BASE_SCALE, or CEM_LDR_RGBA_DIRECT vs. CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A.
							switch (bucket.m_cem_index)
							{
							case astc_helpers::CEM_LDR_RGB_DIRECT:
								alt_cem_index_to_find = astc_helpers::CEM_LDR_RGB_BASE_SCALE;
								break;
							case astc_helpers::CEM_LDR_RGB_BASE_SCALE:
								alt_cem_index_to_find = astc_helpers::CEM_LDR_RGB_DIRECT;
								break;
							case astc_helpers::CEM_LDR_RGBA_DIRECT:
								alt_cem_index_to_find = astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A;
								break;
							case astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A:
								alt_cem_index_to_find = astc_helpers::CEM_LDR_RGBA_DIRECT;
								break;
							default:
								assert(0);
								break;
							}

							auto alt_res_it = shortlist_buckets.find(shortlist_bucket(bucket.m_grid_width, bucket.m_grid_height, alt_cem_index_to_find, bucket.m_ccs_index, bucket.m_num_parts, bucket.m_unique_seed_index));

							if (alt_res_it == shortlist_buckets.end())
							{
								new_shortlist_buckets.insert(it->first, it->second);
							}
							else
							{
								assert(!alt_res_it->first.m_examined_flag);

								alt_res_it->first.m_examined_flag = true;

								// Compare the two buckets, decide if one or another can be tossed as not worth it.
								const float fract = (bucket.m_sse > 0.0f) ? (alt_res_it->first.m_sse / bucket.m_sse) : 0.0f;

								const float ALT_RES_SSE_THRESH = .1f;
								if (fract < (1.0f - ALT_RES_SSE_THRESH))
									new_shortlist_buckets.insert(alt_res_it->first, alt_res_it->second);
								else if (fract > (1.0f + ALT_RES_SSE_THRESH))
									new_shortlist_buckets.insert(it->first, it->second);
								else
								{
									new_shortlist_buckets.insert(alt_res_it->first, alt_res_it->second);
									new_shortlist_buckets.insert(it->first, it->second);
								}
							}
						}
						else
						{
							new_shortlist_buckets.insert(it->first, it->second);
						}

					} // if (prune_pass

					it->first.m_examined_flag = true;
				}

				new_shortlist_buckets.swap(shortlist_buckets);
			} // prune_pass
		} // if (g_bucket_pruning_passes)

		assert(shortlist_buckets.size());

		if (m_ranked_buckets.capacity() < shortlist_buckets.size())
			m_ranked_buckets.reserve(shortlist_buckets.size());

		for (auto it = shortlist_buckets.begin(); it != shortlist_buckets.end(); ++it)
		{
			shortlist_bucket& bucket = it->first;
			const trial_mode_index_vec& trial_mode_indices = it->second;

			ranked_shortlist_bucket* pDst = m_ranked_buckets.enlarge(1);
			pDst->m_bucket = bucket;
			pDst->m_trial_mode_indices = trial_mode_indices;
		}

		assert(m_ranked_buckets.size());

		// Sort the buckets by their surrogate encoded SSE to rank them.
		std::sort(m_ranked_buckets.begin(), m_ranked_buckets.end());

		return true;
	}

	bool rank_and_sort_shortlist_buckets(
		const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		BASISU_NOTE_UNUSED(blur_id);
		BASISU_NOTE_UNUSED(out_blocks);

		basisu::vector<trial_surrogate>& shortlist_trials = m_trial_surrogates;
		
		// TODO: Tune this further. Memory here adds up across all encoding threads.
		{
			//const float reserve_factor = (sizeof(void*) > 4) ? .5f : .25f;
			const uint32_t reserve_size = 64;// maximum(256, (int)(p.m_num_trial_modes * reserve_factor));

			if (shortlist_trials.capacity() < reserve_size)
				shortlist_trials.reserve(reserve_size);

			shortlist_trials.resize(0);
		}

		uint32_t num_buckets_to_examine = fast_roundf_int((float)m_ranked_buckets.size_u32() * p.m_shortlist_buckets_to_examine_fract);
		num_buckets_to_examine = clamp<uint32_t>(num_buckets_to_examine, p.m_shortlist_buckets_to_examine_min, p.m_shortlist_buckets_to_examine_max);

		num_buckets_to_examine = clamp<uint32_t>(num_buckets_to_examine, 1, m_ranked_buckets.size_u32());

		float best_err_so_far = BIG_FLOAT_VAL;

		for (uint32_t bucket_index = 0; bucket_index < num_buckets_to_examine; bucket_index++)
		{
			const shortlist_bucket& bucket = m_ranked_buckets[bucket_index].m_bucket;
			const trial_mode_index_vec& bucket_trial_mode_indices = m_ranked_buckets[bucket_index].m_trial_mode_indices;

			if (best_err_so_far != BIG_FLOAT_VAL)
			{
				if (bucket.m_sse > best_err_so_far * SKIP_IF_BUCKET_WORSE_MULTIPLIER)
					continue;
			}
			best_err_so_far = minimum(best_err_so_far, bucket.m_sse);

			if (bucket_trial_mode_indices.size() == 1)
			{
				// Bucket only contains 1 mode, so we've already encoded its surrogate.
				trial_surrogate& s = *shortlist_trials.try_enlarge(1);

				s.m_trial_mode_index = bucket_trial_mode_indices[0];
				s.m_err = bucket.m_sse;
				s.m_log_blk = bucket.m_surrogate_log_blk;
				continue;
			}

			//-----
			// We have a bucket sharing all config except for ISE weight/endpoint levels. Decide how many to place on the shortlist using analytic weighted MSE/SSE estimates.

			const uint32_t num_modes_in_bucket = bucket_trial_mode_indices.size_u32();

			uint32_t num_modes_in_bucket_to_shortlist = fast_roundf_pos_int(num_modes_in_bucket * p.m_num_similar_modes_in_bucket_to_shortlist_fract);

			num_modes_in_bucket_to_shortlist = clamp<uint32_t>(num_modes_in_bucket_to_shortlist, p.m_num_similar_modes_in_bucket_to_shortlist_fract_min, p.m_num_similar_modes_in_bucket_to_shortlist_fract_max);

			num_modes_in_bucket_to_shortlist = clamp<uint32_t>(num_modes_in_bucket_to_shortlist, 1, num_modes_in_bucket);

			basisu::vector<uint32_t> bucket_indices(num_modes_in_bucket);
			for (uint32_t i = 0; i < num_modes_in_bucket; i++)
				bucket_indices[i] = i;

			if (num_modes_in_bucket_to_shortlist < num_modes_in_bucket)
			{
				basisu::vector<float> sse_estimates(num_modes_in_bucket);

				const uint32_t bucket_surrogate_endpoint_levels = bucket.m_surrogate_log_blk.m_num_endpoint_levels;
				const uint32_t bucket_surrogate_weight_levels = bucket.m_surrogate_log_blk.m_num_weight_levels;
				const float bucket_surrogate_base_sse = bucket.m_sse;

				const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(p.m_block_width, p.m_block_height, bucket.m_grid_width, bucket.m_grid_height);
				const astc_ldr::partitions_data* pBucket_part_data = (bucket.m_num_parts == 1) ? nullptr : ((bucket.m_num_parts == 2) ? p.m_pPart_data_p2 : p.m_pPart_data_p3);

				bool can_use_base_ofs = false;
				if ((bucket.m_cem_index == astc_helpers::CEM_LDR_RGB_DIRECT) || (bucket.m_cem_index == astc_helpers::CEM_LDR_RGBA_DIRECT))
				{
					float max_span_size = 0.0f;
					for (uint32_t part_iter = 0; part_iter < bucket.m_num_parts; part_iter++)
					{
						for (uint32_t c = 0; c < 4; c++)
						{
							float span_size = fabs(bucket.m_surrogate_log_blk.m_endpoints[part_iter][1][c] - bucket.m_surrogate_log_blk.m_endpoints[part_iter][0][c]);
							max_span_size = maximum(max_span_size, span_size);
						}
					}

					can_use_base_ofs = max_span_size < .25f;
				}

				chan_mse_est bucket_sse_est(0.0f, 0.0f);
				for (uint32_t part_iter = 0; part_iter < bucket.m_num_parts; part_iter++)
				{
					uint32_t total_texels_in_part = p.m_block_width * p.m_block_height;
					if (bucket.m_num_parts > 1)
					{
						total_texels_in_part = pBucket_part_data->m_partition_pat_histograms[bucket.m_unique_seed_index].m_hist[part_iter];
						assert(total_texels_in_part && total_texels_in_part < p.m_block_width * p.m_block_height);
					}

					for (uint32_t c = 0; c < 4; c++)
					{
						float span_size = fabs(bucket.m_surrogate_log_blk.m_endpoints[part_iter][1][c] - bucket.m_surrogate_log_blk.m_endpoints[part_iter][0][c]);

						chan_mse_est chan_mse_est(compute_quantized_channel_mse_estimates(
							can_use_base_ofs ? minimum<uint32_t>(bucket_surrogate_endpoint_levels * 2, 256) : bucket_surrogate_endpoint_levels,
							bucket_surrogate_weight_levels,
							span_size, pGrid_data->m_weight_gamma));

						if (span_size == 0.0f)
						{
							if ((bucket.m_surrogate_log_blk.m_endpoints[part_iter][1][c] == 1.0f) || (bucket.m_surrogate_log_blk.m_endpoints[part_iter][1][c] == 0.0f))
							{
								chan_mse_est.m_ep = 0.0f;
								chan_mse_est.m_wp = 0.0f;
							}
						}

						bucket_sse_est.m_ep += chan_mse_est.m_ep * (float)p.m_pEnc_params->m_comp_weights[c] * total_texels_in_part;
						bucket_sse_est.m_wp += chan_mse_est.m_wp * (float)p.m_pEnc_params->m_comp_weights[c] * total_texels_in_part;
					} // c

				} // part_iter

#if 0
				fmt_debug_printf("----------------\n");

				fmt_debug_printf("bucket endpoint levels: {}, weight levels: {}, surrogate sse: {}, ep_est: {}, wp_est: {}, avg RGB subset0 span: {}\n",
					bucket_surrogate_endpoint_levels, bucket_surrogate_weight_levels,
					bucket.m_sse,
					bucket_sse_est.m_ep, bucket_sse_est.m_wp,
					(fabs(bucket.m_surrogate_log_blk.m_endpoints[0][1][0] - bucket.m_surrogate_log_blk.m_endpoints[0][0][0]) +
						fabs(bucket.m_surrogate_log_blk.m_endpoints[0][1][1] - bucket.m_surrogate_log_blk.m_endpoints[0][0][1]) +
						fabs(bucket.m_surrogate_log_blk.m_endpoints[0][1][2] - bucket.m_surrogate_log_blk.m_endpoints[0][0][2])) / 3.0f);
#endif

				for (uint32_t j = 0; j < bucket_trial_mode_indices.size(); j++)
				{
					const uint32_t trial_mode_index = bucket_trial_mode_indices[j];
					const basist::astc_ldr_t::trial_mode& tm = p.m_pTrial_modes[trial_mode_index];

					const uint32_t trial_mode_endpoint_levels = astc_helpers::get_ise_levels(tm.m_endpoint_ise_range);
					const uint32_t trial_mode_weight_levels = astc_helpers::get_ise_levels(tm.m_weight_ise_range);

					assert(trial_mode_endpoint_levels <= bucket_surrogate_endpoint_levels);
					assert(trial_mode_weight_levels <= bucket_surrogate_weight_levels);

					chan_mse_est mode_sse_est(0.0f, 0.0f);
					for (uint32_t part_iter = 0; part_iter < bucket.m_num_parts; part_iter++)
					{
						uint32_t total_texels_in_part = p.m_block_width * p.m_block_height;
						if (bucket.m_num_parts > 1)
						{
							total_texels_in_part = pBucket_part_data->m_partition_pat_histograms[bucket.m_unique_seed_index].m_hist[part_iter];
							assert(total_texels_in_part && total_texels_in_part < p.m_block_width * p.m_block_height);
						}

						for (uint32_t c = 0; c < 4; c++)
						{
							float span_size = fabs(bucket.m_surrogate_log_blk.m_endpoints[part_iter][1][c] - bucket.m_surrogate_log_blk.m_endpoints[part_iter][0][c]);

							chan_mse_est chan_mse_est(compute_quantized_channel_mse_estimates(
								can_use_base_ofs ? minimum<uint32_t>(trial_mode_endpoint_levels * 2, 256) : trial_mode_endpoint_levels,
								trial_mode_weight_levels,
								span_size, pGrid_data->m_weight_gamma));

							if (span_size == 0.0f)
							{
								if ((bucket.m_surrogate_log_blk.m_endpoints[part_iter][1][c] == 1.0f) || (bucket.m_surrogate_log_blk.m_endpoints[part_iter][1][c] == 0.0f))
								{
									chan_mse_est.m_ep = 0.0f;
									chan_mse_est.m_wp = 0.0f;
								}
							}

							mode_sse_est.m_ep += chan_mse_est.m_ep * (float)p.m_pEnc_params->m_comp_weights[c] * total_texels_in_part;
							mode_sse_est.m_wp += chan_mse_est.m_wp * (float)p.m_pEnc_params->m_comp_weights[c] * total_texels_in_part;
						} // c

					} // part_iter

					// Remove the bucket's base estimated endpoint/weight quant
					if (trial_mode_endpoint_levels == bucket_surrogate_endpoint_levels)
					{
						mode_sse_est.m_ep = 0.0f;
					}
					else
					{
						mode_sse_est.m_ep -= bucket_sse_est.m_ep;

						if (mode_sse_est.m_ep < 0.0f)
							mode_sse_est.m_ep = 0.0f;
					}

					if (trial_mode_weight_levels == bucket_surrogate_weight_levels)
					{
						mode_sse_est.m_wp = 0.0f;
					}
					else
					{
						mode_sse_est.m_wp -= bucket_sse_est.m_wp;

						if (mode_sse_est.m_wp < 0.0f)
							mode_sse_est.m_wp = 0.0f;
					}

					float mode_total_sse_est = bucket_surrogate_base_sse + mode_sse_est.m_ep + mode_sse_est.m_wp;

					sse_estimates[j] = mode_total_sse_est;

#if 0
					// TEMP comparison code
					float actual_sse = 0.0f;

					{
						log_surrogate_astc_blk temp_surrogate_log_blk;
						if (bucket.m_num_parts == 1)
						{
							actual_sse = encode_surrogate_trial(
								p.m_block_width, p.m_block_height,
								pixel_stats,
								bucket.m_cem_index,
								bucket.m_ccs_index,
								tm.m_endpoint_ise_range, tm.m_weight_ise_range,
								bucket.m_grid_width, bucket.m_grid_height,
								temp_surrogate_log_blk,
								*p.m_pEnc_params);
						}
						else
						{
							const astc_ldr::partitions_data* pPart_data = (bucket.m_num_parts == 2) ? p.m_pPart_data_p2 : p.m_pPart_data_p3;

							const uint32_t part_seed_index = pPart_data->m_unique_index_to_part_seed[bucket.m_unique_seed_index];

							const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[bucket.m_unique_seed_index];

							actual_sse = encode_surrogate_trial_subsets(
								p.m_block_width, p.m_block_height,
								pixel_stats,
								bucket.m_cem_index, bucket.m_num_parts, part_seed_index, pPat,
								tm.m_endpoint_ise_range, tm.m_weight_ise_range,
								bucket.m_grid_width, bucket.m_grid_height,
								temp_surrogate_log_blk,
								*p.m_pEnc_params, 0);
						}

						stats.m_total_surrogate_encodes++;
					}

					fmt_debug_printf("sse: {}, actual sse: {}, endpoint levels: {} weight levels: {}\n", sse_estimates[j], actual_sse, trial_mode_endpoint_levels, trial_mode_weight_levels);
#endif

				} // j

#if 0
				fmt_debug_printf("\n");
#endif

				indirect_sort(num_modes_in_bucket, bucket_indices.get_ptr(), sse_estimates.get_ptr());

			} // if (num_modes_in_bucket_to_shortlist < num_modes_in_bucket)

			// Surrogate encode the best looking modes in the bucket after factoring in estimate SSE errors.

			for (uint32_t q = 0; q < num_modes_in_bucket_to_shortlist; q++)
			{
				const uint32_t j = bucket_indices[q];

				trial_surrogate& s = *shortlist_trials.try_enlarge(1);

				const uint32_t trial_mode_index = bucket_trial_mode_indices[j];
				const basist::astc_ldr_t::trial_mode& tm = p.m_pTrial_modes[trial_mode_index];

				s.m_trial_mode_index = trial_mode_index;

				if (bucket.m_num_parts == 1)
				{
					s.m_err = encode_surrogate_trial(
						p.m_block_width, p.m_block_height,
						pixel_stats,
						bucket.m_cem_index,
						bucket.m_ccs_index,
						tm.m_endpoint_ise_range, tm.m_weight_ise_range,
						bucket.m_grid_width, bucket.m_grid_height,
						s.m_log_blk,
						*p.m_pEnc_params, 0);

					stats.m_total_surrogate_encodes++;
				}
				else
				{
					const astc_ldr::partitions_data* pPart_data = (bucket.m_num_parts == 2) ? p.m_pPart_data_p2 : p.m_pPart_data_p3;

					const uint32_t part_seed_index = pPart_data->m_unique_index_to_part_seed[bucket.m_unique_seed_index];

					const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[bucket.m_unique_seed_index];

					s.m_err = encode_surrogate_trial_subsets(
						p.m_block_width, p.m_block_height,
						pixel_stats,
						bucket.m_cem_index, bucket.m_num_parts, part_seed_index, pPat,
						tm.m_endpoint_ise_range, tm.m_weight_ise_range,
						bucket.m_grid_width, bucket.m_grid_height,
						s.m_log_blk,
						*p.m_pEnc_params, 0);

					stats.m_total_surrogate_encodes++;
				}

				if ((bucket.m_cem_index == astc_helpers::CEM_LDR_RGB_DIRECT) || (bucket.m_cem_index == astc_helpers::CEM_LDR_RGBA_DIRECT))
				{
					// blue contraction/base+offset discount
					s.m_err *= BLUE_CONTRACTION_BASE_OFS_DISCOUNT;
				}

			} // j

		} // bucket_index

		if (!shortlist_trials.size())
			return false;

		shortlist_trials.sort();

		stats.m_total_shortlist_candidates += shortlist_trials.size_u32();

		return true;
	}

	bool final_polish_encode_from_shortlist(
		const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		basisu::vector<trial_surrogate>& shortlist_trials = m_trial_surrogates;

		// TODO: Diversity selection
		const float shortlist_fract = p.m_final_shortlist_fraction[m_block_complexity_index];

		uint32_t max_shortlist_trials = (uint32_t)std::roundf((float)shortlist_trials.size_u32() * shortlist_fract);

		max_shortlist_trials = clamp<uint32_t>(max_shortlist_trials, p.m_final_shortlist_min_size[m_block_complexity_index], p.m_final_shortlist_max_size[m_block_complexity_index]);

		uint32_t total_shortlist_trials = clamp<uint32_t>(max_shortlist_trials, 1, shortlist_trials.size_u32());

		const uint32_t EARLY_STOP2_SHORTLIST_ITER_INDEX = 5;

		// Now do the real encodes on the top surrogate shortlist trials.
		for (uint32_t shortlist_iter = 0; shortlist_iter < total_shortlist_trials; shortlist_iter++)
		{
			const uint32_t trial_mode_index = shortlist_trials[shortlist_iter].m_trial_mode_index;
			const basist::astc_ldr_t::trial_mode& tm = p.m_pTrial_modes[trial_mode_index];

			astc_helpers::log_astc_block log_astc_blk;

			bool base_ofs_succeeded_flag = false;

			if ((p.m_final_encode_try_base_ofs) && 
				((tm.m_cem == astc_helpers::CEM_LDR_RGB_DIRECT) || (tm.m_cem == astc_helpers::CEM_LDR_RGBA_DIRECT)))
				//(tm.m_endpoint_ise_range < astc_helpers::BISE_256_LEVELS)) // although this check makes sense for quality, it may not make sense for rate-distortion performance
			{
				// Add RGB/RGBA BASE PLUS OFFSET variant.
				astc_helpers::log_astc_block log_astc_blk_alt;

				const uint32_t base_ofs_cem_index = (tm.m_cem == astc_helpers::CEM_LDR_RGB_DIRECT) ? astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET : astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET;

				bool try_direct_encoding_flag = false;

				bool alt_enc_trial_status;
				if (tm.m_num_parts > 1)
				{
					const astc_ldr::partitions_data* pPart_data = (tm.m_num_parts == 2) ? p.m_pPart_data_p2 : p.m_pPart_data_p3;

					const uint32_t part_seed_index = shortlist_trials[shortlist_iter].m_log_blk.m_seed_index;
					const uint32_t part_unique_index = pPart_data->m_part_seed_to_unique_index[part_seed_index];
					const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[part_unique_index];

					alt_enc_trial_status = encode_trial_subsets(
						p.m_block_width, p.m_block_height, pixel_stats, base_ofs_cem_index, tm.m_num_parts,
						part_seed_index, pPat,
						tm.m_endpoint_ise_range, tm.m_weight_ise_range,
						tm.m_grid_width, tm.m_grid_height, 
						p.m_encode_trial_subsets_early_out_thresh,
						log_astc_blk_alt, *p.m_pEnc_params, false,
						p.m_gradient_descent_flag, p.m_polish_weights_flag, p.m_qcd_enabled_flag,
						p.m_use_blue_contraction, &try_direct_encoding_flag);
					
					stats.m_total_full_encodes++;
				}
				else
				{
					alt_enc_trial_status = encode_trial(
						p.m_block_width, p.m_block_height, pixel_stats, base_ofs_cem_index,
						tm.m_ccs_index != -1, tm.m_ccs_index,
						tm.m_endpoint_ise_range, tm.m_weight_ise_range,
						tm.m_grid_width, tm.m_grid_height, p.m_encode_trial_early_out_thresh, 
						log_astc_blk_alt, *p.m_pEnc_params,
						p.m_gradient_descent_flag, p.m_polish_weights_flag, p.m_qcd_enabled_flag,
						p.m_use_blue_contraction, &try_direct_encoding_flag);
					
					stats.m_total_full_encodes++;
				}

				assert(alt_enc_trial_status);

				if (alt_enc_trial_status)
				{
					encode_block_output* pOut_block2 = out_blocks.enlarge(1);
					pOut_block2->clear();
					pOut_block2->m_trial_mode_index = safe_cast_int16(trial_mode_index);
					pOut_block2->m_log_blk = log_astc_blk_alt;
					pOut_block2->m_blur_id = safe_cast_uint16(blur_id);
					pOut_block2->m_sse = eval_error(p.m_block_width, p.m_block_height, log_astc_blk_alt, pixel_stats, *p.m_pEnc_params);

					if ((p.m_early_stop_wpsnr) || (p.m_early_stop2_wpsnr))
					{
						const float wpsnr = compute_psnr_from_wsse(p.m_block_width, p.m_block_height, pOut_block2->m_sse, p.m_pEnc_params->get_total_comp_weights());

						if ((p.m_early_stop_wpsnr) && (wpsnr >= p.m_early_stop_wpsnr))
							break;

						if (shortlist_iter >= EARLY_STOP2_SHORTLIST_ITER_INDEX)
						{
							if ((p.m_early_stop2_wpsnr) && (wpsnr >= p.m_early_stop2_wpsnr))
								break;
						}
					}

					base_ofs_succeeded_flag = !try_direct_encoding_flag;
				}

			} // (p.m_final_encode_try_base_ofs)

			if ((p.m_final_encode_always_try_rgb_direct) || (!base_ofs_succeeded_flag))
			{
				bool enc_trial_status;

				if (tm.m_num_parts > 1)
				{
					const astc_ldr::partitions_data* pPart_data = (tm.m_num_parts == 2) ? p.m_pPart_data_p2 : p.m_pPart_data_p3;

					const uint32_t part_seed_index = shortlist_trials[shortlist_iter].m_log_blk.m_seed_index;
					const uint32_t part_unique_index = pPart_data->m_part_seed_to_unique_index[part_seed_index];
					assert(part_unique_index < astc_helpers::NUM_PARTITION_PATTERNS);
					const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[part_unique_index];

					enc_trial_status = encode_trial_subsets(
						p.m_block_width, p.m_block_height, pixel_stats, tm.m_cem, tm.m_num_parts,
						part_seed_index, pPat,
						tm.m_endpoint_ise_range, tm.m_weight_ise_range,
						tm.m_grid_width, tm.m_grid_height, 
						p.m_encode_trial_subsets_early_out_thresh,
						log_astc_blk, *p.m_pEnc_params, false,
						p.m_gradient_descent_flag, p.m_polish_weights_flag, p.m_qcd_enabled_flag,
						p.m_use_blue_contraction);
					
					stats.m_total_full_encodes++;
				}
				else
				{
					enc_trial_status = encode_trial(
						p.m_block_width, p.m_block_height, pixel_stats, tm.m_cem,
						tm.m_ccs_index != -1, tm.m_ccs_index,
						tm.m_endpoint_ise_range, tm.m_weight_ise_range,
						tm.m_grid_width, tm.m_grid_height, p.m_encode_trial_early_out_thresh,
						log_astc_blk, *p.m_pEnc_params,
						p.m_gradient_descent_flag, p.m_polish_weights_flag, p.m_qcd_enabled_flag,
						p.m_use_blue_contraction);
					
					stats.m_total_full_encodes++;
				}

				assert(enc_trial_status);

				if (!enc_trial_status)
					return false;
								
				{
					encode_block_output* pOut_block1 = out_blocks.enlarge(1);
					pOut_block1->clear();
					pOut_block1->m_trial_mode_index = safe_cast_int16(trial_mode_index);
					pOut_block1->m_log_blk = log_astc_blk;
					pOut_block1->m_blur_id = safe_cast_uint16(blur_id);
					pOut_block1->m_sse = eval_error(p.m_block_width, p.m_block_height, log_astc_blk, pixel_stats, *p.m_pEnc_params);

					if ((p.m_early_stop_wpsnr) || (p.m_early_stop2_wpsnr))
					{
						const float wpsnr = compute_psnr_from_wsse(p.m_block_width, p.m_block_height, pOut_block1->m_sse, p.m_pEnc_params->get_total_comp_weights());

						if ((p.m_early_stop_wpsnr) && (wpsnr >= p.m_early_stop_wpsnr))
							break;

						if (shortlist_iter >= EARLY_STOP2_SHORTLIST_ITER_INDEX)
						{
							if ((p.m_early_stop2_wpsnr) && (wpsnr >= p.m_early_stop2_wpsnr))
								break;
						}
					}
				}

			} // if (!skip_encode_flag)

		} // shortlist_iter

		return true;
	}

	bool full_encode(const ldr_astc_lowlevel_block_encoder_params& p,
		const astc_ldr::pixel_stats_t& pixel_stats,
		basisu::vector<encode_block_output>& out_blocks,
		uint32_t blur_id,
		encode_block_stats& stats)
	{
		clear();

		if (!init(p, pixel_stats, out_blocks, blur_id, stats))
			return false;

		if (!partition_triage(p, pixel_stats, out_blocks, blur_id, stats))
			return false;

		if (!trivial_triage(p, pixel_stats, out_blocks, blur_id, stats))
			return false;

		if (!analytic_triage(p, pixel_stats, out_blocks, blur_id, stats))
			return false;

		if (!surrogate_encode_shortlist_bucket_representatives(p, pixel_stats, out_blocks, blur_id, stats))
			return false;

		if (!prune_shortlist_buckets(p, pixel_stats, out_blocks, blur_id, stats))
			return false;

		if (!rank_and_sort_shortlist_buckets(p, pixel_stats, out_blocks, blur_id, stats))
			return false;

		if (!final_polish_encode_from_shortlist(p, pixel_stats, out_blocks, blur_id, stats))
			return false;

		return true;
	}
};

class ldr_astc_lowlevel_block_encoder_pool
{
public:
	ldr_astc_lowlevel_block_encoder_pool()
	{
	}

	void init(uint32_t total_threads)
	{
		std::lock_guard g(m_mutex);

		m_pool.resize(total_threads);

		for (uint32_t i = 0; i < total_threads; i++)
			m_pool[i].m_used_flag = false;
	}

	void deinit()
	{
		std::lock_guard g(m_mutex);

		for (uint32_t i = 0; i < m_pool.size(); i++)
		{
			if (m_pool[i].m_used_flag)
			{
				assert(0);
				debug_printf("ldr_astc_lowlevel_block_encoder_pool::deinit: Pool entry still marked as used\n");
			}

			m_pool[i].m_used_flag = false;
		}

		m_pool.resize(0);
	}

	ldr_astc_lowlevel_block_encoder* acquire()
	{
		std::lock_guard g(m_mutex);

		assert(m_pool.size());

		ldr_astc_lowlevel_block_encoder* pRes = nullptr;

		for (uint32_t i = 0; i < m_pool.size(); i++)
		{
			if (!m_pool[i].m_used_flag)
			{
				pRes = &m_pool[i];
				pRes->m_used_flag = true;

				break;
			}
		}

		assert(pRes);

		return pRes;
	}

	bool release(ldr_astc_lowlevel_block_encoder* pTemps)
	{
		std::lock_guard g(m_mutex);

		assert(m_pool.size());

		if ((pTemps < m_pool.begin()) || (pTemps >= m_pool.end()))
		{
			assert(0);
			return false;
		}

		size_t idx = pTemps - m_pool.begin();
		if (idx >= m_pool.size())
		{
			assert(0);
			return false;
		}

		m_pool[idx].m_used_flag = false;

		return true;
	}

private:
	std::mutex m_mutex;
	basisu::vector<ldr_astc_lowlevel_block_encoder> m_pool;
};

class scoped_ldr_astc_lowlevel_block_encoder
{
public:
	scoped_ldr_astc_lowlevel_block_encoder(ldr_astc_lowlevel_block_encoder_pool& pool) :
		m_pool(pool)
	{
		m_pTemps = pool.acquire();
	}

	~scoped_ldr_astc_lowlevel_block_encoder()
	{
		m_pool.release(m_pTemps);
	}

	ldr_astc_lowlevel_block_encoder_pool& get_pool() const
	{
		return m_pool;
	}

	ldr_astc_lowlevel_block_encoder* get_ptr()
	{
		return m_pTemps;
	}

private:
	ldr_astc_lowlevel_block_encoder_pool& m_pool;
	ldr_astc_lowlevel_block_encoder* m_pTemps;
};


//-------------------------------------------------------------------

#pragma pack(push, 1)
struct trial_mode_desc
{
	uint8_t m_unique_cem_index; // LDR base CEM's, 0-5
	uint8_t m_ccs; // 0 if SP, 1-4 for DP
	uint8_t m_subsets; // 1-3
	uint8_t m_eise; // endpoint ise range, 4-20
	uint8_t m_wise; // weight ise range, 0-11
	uint8_t m_grid_w, m_grid_h; // grid resolution, 4-12
};
#pragma pack(pop)

[[maybe_unused]] static const int s_astc_cem_to_unique_ldr_index[16] =
{
	0, 	// CEM_LDR_LUM_DIRECT
	-1, // CEM_LDR_LUM_BASE_PLUS_OFS
	-1, // CEM_HDR_LUM_LARGE_RANGE
	-1, // CEM_HDR_LUM_SMALL_RANGE
	1,  // CEM_LDR_LUM_ALPHA_DIRECT
	-1, // CEM_LDR_LUM_ALPHA_BASE_PLUS_OFS
	2,  // CEM_LDR_RGB_BASE_SCALE
	-1, // CEM_HDR_RGB_BASE_SCALE
	3,  // CEM_LDR_RGB_DIRECT
	-1, // CEM_LDR_RGB_BASE_PLUS_OFFSET
	4,  // CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A
	-1, // CEM_HDR_RGB
	5,  // CEM_LDR_RGBA_DIRECT
	-1, // CEM_LDR_RGBA_BASE_PLUS_OFFSET
	-1, // CEM_HDR_RGB_LDR_ALPHA
	-1, // CEM_HDR_RGB_HDR_ALPHA
};

#if 0
static const int s_unique_ldr_index_to_astc_cem[6] =
{
	astc_helpers::CEM_LDR_LUM_DIRECT,
	astc_helpers::CEM_LDR_LUM_ALPHA_DIRECT,
	astc_helpers::CEM_LDR_RGB_BASE_SCALE,
	astc_helpers::CEM_LDR_RGB_DIRECT,
	astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A,
	astc_helpers::CEM_LDR_RGBA_DIRECT
};
#endif

#if 0
static uint32_t pack_tm_desc(
	uint32_t grid_width, uint32_t grid_height,
	uint32_t cem_index, uint32_t ccs_index, uint32_t num_subsets,
	uint32_t endpoint_ise_range, uint32_t weight_ise_range)
{
	assert((grid_width >= 2) && (grid_width <= 12));
	assert((grid_height >= 2) && (grid_height <= 12));
	assert((cem_index < 16) && astc_helpers::is_cem_ldr(cem_index));
	assert((num_subsets >= 1) && (num_subsets <= 3));
	assert(ccs_index <= 4); // 0 for SP, 1-4 for DP
	assert((endpoint_ise_range >= astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE) && (endpoint_ise_range <= astc_helpers::LAST_VALID_ENDPOINT_ISE_RANGE));
	assert((weight_ise_range >= astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE) && (weight_ise_range <= astc_helpers::LAST_VALID_WEIGHT_ISE_RANGE));

	grid_width -= 2;
	grid_height -= 2;
	assert((grid_width <= 10) && (grid_height <= 10));

	const int unique_cem_index = s_astc_cem_to_unique_ldr_index[cem_index];
	assert((unique_cem_index >= 0) && (unique_cem_index <= 5));
	assert(basist::astc_ldr_t::s_unique_ldr_index_to_astc_cem[unique_cem_index] == (int)cem_index);

	num_subsets--;

	endpoint_ise_range -= astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE;

	uint32_t cur_bit_ofs = 0;

#define BU_PACK_FIELD(val, bits) do { uint32_t v = (uint32_t)(val); assert(v < (1u << bits)); packed_id |= (v << cur_bit_ofs); cur_bit_ofs += (bits); } while(0)

	uint32_t packed_id = 0;
	BU_PACK_FIELD(endpoint_ise_range, basist::astc_ldr_t::CFG_PACK_EISE_BITS);
	BU_PACK_FIELD(weight_ise_range, basist::astc_ldr_t::CFG_PACK_WISE_BITS);
	BU_PACK_FIELD(ccs_index, basist::astc_ldr_t::CFG_PACK_CCS_BITS);
	BU_PACK_FIELD(num_subsets, basist::astc_ldr_t::CFG_PACK_SUBSETS_BITS);
	BU_PACK_FIELD(unique_cem_index, basist::astc_ldr_t::CFG_PACK_CEM_BITS);
	// must be at the top
	BU_PACK_FIELD(grid_width * 11 + grid_height, basist::astc_ldr_t::CFG_PACK_GRID_BITS);
#undef BU_PACK_FIELD

	assert(cur_bit_ofs == 24);

	return packed_id;
}
#endif

#if 0
static void create_encoder_trial_modes_full_eval(uint32_t block_width, uint32_t block_height,
	basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes, basist::astc_ldr_t::grouped_trial_modes& grouped_encoder_trial_modes,
	bool print_debug_info = true, bool print_modes = false)
{
	interval_timer itm;
	itm.start();

	encoder_trial_modes.resize(0);
	grouped_encoder_trial_modes.clear();

	uint32_t max_grid_width = 0, max_grid_height = 0;
	uint32_t total_evals = 0, total_partial_evals = 0, total_evals_succeeded = 0;
	uint32_t mode_index = 0;
	uint_vec packed_mode_ids;

	for (uint32_t alpha_iter = 0; alpha_iter < 2; alpha_iter++)
	{
		if (print_modes)
		{
			if (alpha_iter)
				fmt_debug_printf("ALPHA TRIAL MODES\n");
			else
				fmt_debug_printf("RGB TRIAL MODES\n");
		}

		astc_helpers::astc_block phys_block;

		for (uint32_t cem_mode_iter = 0; cem_mode_iter < 3; cem_mode_iter++)
		{
			const uint32_t s_rgb_cems[3] = { astc_helpers::CEM_LDR_LUM_DIRECT, astc_helpers::CEM_LDR_RGB_BASE_SCALE, astc_helpers::CEM_LDR_RGB_DIRECT };
			const uint32_t s_alpha_cems[3] = { astc_helpers::CEM_LDR_LUM_ALPHA_DIRECT, astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A, astc_helpers::CEM_LDR_RGBA_DIRECT };

			const uint32_t cem_index = alpha_iter ? s_alpha_cems[cem_mode_iter] : s_rgb_cems[cem_mode_iter];

			uint32_t num_dp_chans = 0;
			bool cem_supports_dual_plane = false;
			bool cem_supports_subsets = false;

			// base+ofs variants are automatically used later as alternates to RGB/RGBA direct modes
			switch (cem_index)
			{
			case astc_helpers::CEM_LDR_LUM_DIRECT:
				num_dp_chans = 0; // only a single component, so only a single plane
				cem_supports_dual_plane = false;
				cem_supports_subsets = true;
				break;
			case astc_helpers::CEM_LDR_LUM_ALPHA_DIRECT:
				num_dp_chans = 1; // CCS can only be 3
				cem_supports_dual_plane = true;
				cem_supports_subsets = true;
				break;
			case astc_helpers::CEM_LDR_RGB_DIRECT:
				num_dp_chans = 3;
				cem_supports_dual_plane = true;
				cem_supports_subsets = true;
				break;
			case astc_helpers::CEM_LDR_RGB_BASE_SCALE:
				num_dp_chans = 3;
				cem_supports_dual_plane = true;
				cem_supports_subsets = true;
				break;
			case astc_helpers::CEM_LDR_RGBA_DIRECT:
				num_dp_chans = 4;
				cem_supports_dual_plane = true;
				cem_supports_subsets = true;
				break;
			case astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A:
				num_dp_chans = 4;
				cem_supports_dual_plane = true;
				cem_supports_subsets = true;
				break;
			default:
				assert(0);
				break;
			}

			for (int dp = 0; dp < (cem_supports_dual_plane ? 2 : 1); dp++)
			{
				const bool use_subsets = !dp && cem_supports_subsets;

				for (int subsets = 1; subsets <= (use_subsets ? 3 : 1); subsets++)
				{
					for (uint32_t grid_height = 2; grid_height <= block_height; grid_height++)
					{
						for (uint32_t grid_width = 2; grid_width <= block_width; grid_width++)
						{
							for (uint32_t dp_chan_index = 0; dp_chan_index < (dp ? num_dp_chans : 1); dp_chan_index++)
							{
								astc_helpers::log_astc_block log_block;
								log_block.clear();

								log_block.m_grid_width = (uint8_t)grid_width;
								log_block.m_grid_height = (uint8_t)grid_height;

								log_block.m_num_partitions = (uint8_t)subsets;

								for (int i = 0; i < subsets; i++)
									log_block.m_color_endpoint_modes[i] = (uint8_t)cem_index;

								log_block.m_dual_plane = dp > 0;

								if (log_block.m_dual_plane)
								{
									uint32_t ccs_index = dp_chan_index;

									if (cem_index == astc_helpers::CEM_LDR_LUM_ALPHA_DIRECT)
									{
										// must be 3 for LA if DP is enabled
										ccs_index = 3;
									}

									log_block.m_color_component_selector = (uint8_t)ccs_index;
								}

								for (uint32_t weight_ise_range = astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE; weight_ise_range <= astc_helpers::LAST_VALID_WEIGHT_ISE_RANGE; weight_ise_range++)
								{
									log_block.m_weight_ise_range = (uint8_t)weight_ise_range;
									log_block.m_endpoint_ise_range = astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE; // dummy value

									total_partial_evals++;

									bool success = astc_helpers::pack_astc_block(phys_block, log_block, nullptr, nullptr, astc_helpers::cValidateEarlyOutAtEndpointISEChecks);
									if (!success)
										continue;

									// in reality only 1 endpoint ISE range is valid here
									for (uint32_t endpoint_ise_range = astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE; endpoint_ise_range <= astc_helpers::LAST_VALID_ENDPOINT_ISE_RANGE; endpoint_ise_range++)
									{
										log_block.m_endpoint_ise_range = (uint8_t)endpoint_ise_range;

										total_evals++;

										success = astc_helpers::pack_astc_block(phys_block, log_block, nullptr, nullptr, astc_helpers::cValidateSkipFinalEndpointWeightPacking);
										if (!success)
											continue;

										total_evals_succeeded++;

										if (print_modes)
										{
											fmt_debug_printf("{}: CEM: {} DP: {}, CCS: {}, SUBSETS: {}, GRID: {}x{}, ENDPOINTS: {}, WEIGHTS: {}\n",
												mode_index,
												log_block.m_color_endpoint_modes[0],
												log_block.m_dual_plane,
												log_block.m_color_component_selector,
												log_block.m_num_partitions,
												log_block.m_grid_width, log_block.m_grid_height,
												astc_helpers::get_ise_levels(log_block.m_endpoint_ise_range),
												astc_helpers::get_ise_levels(log_block.m_weight_ise_range));
										}

										basist::astc_ldr_t::trial_mode m;
										m.m_ccs_index = log_block.m_dual_plane ? log_block.m_color_component_selector : -1;
										m.m_cem = log_block.m_color_endpoint_modes[0];
										m.m_endpoint_ise_range = log_block.m_endpoint_ise_range;
										m.m_weight_ise_range = log_block.m_weight_ise_range;
										m.m_grid_width = grid_width;
										m.m_grid_height = grid_height;
										m.m_num_parts = log_block.m_num_partitions;

										uint32_t packed_index = pack_tm_desc(
											log_block.m_grid_width, log_block.m_grid_height,
											log_block.m_color_endpoint_modes[0], log_block.m_dual_plane ? (log_block.m_color_component_selector + 1) : 0, log_block.m_num_partitions,
											log_block.m_endpoint_ise_range, log_block.m_weight_ise_range);

										assert(packed_index <= 0xFFFFFF);
										packed_mode_ids.push_back(packed_index);

										grouped_encoder_trial_modes.add(block_width, block_height, m, encoder_trial_modes.size_u32());

										encoder_trial_modes.push_back(m);

										max_grid_width = maximum(max_grid_width, grid_width);
										max_grid_height = maximum(max_grid_height, grid_height);

										++mode_index;

									} // weight_ise_range
								} // endpoint_ise_range

							} // ccs_index

						} // grid_width

					} // grid_height

				} // subsets

			} // dp

		} // cem_mode_iter

	} // alpha_iter

#if 0
	packed_mode_ids.sort();

	for (uint32_t i = 0; i < packed_mode_ids.size(); i++)
	{
		uint32_t packed_index = packed_mode_ids[i];

		fmt_debug_printf("{},{},{},", packed_index & 0xFF, (packed_index >> 8) & 0xFF, (packed_index >> 16) & 0xFF);
		if ((i & 15) == 15)
			fmt_debug_printf("\n");
	}
#endif

	if (print_debug_info)
	{
		fmt_debug_printf("create_encoder_trial_modes_full_eval() time: {} secs\n", itm.get_elapsed_secs());

		fmt_debug_printf("create_encoder_trial_modes_full_eval() - ASTC {}x{} modes\n", block_width, block_height);
		fmt_debug_printf("total_evals: {}, total_partial_evals: {}, total_evals_succeeded: {}\n", total_evals, total_partial_evals, total_evals_succeeded);
		fmt_debug_printf("Total trial modes: {}\n", (uint32_t)encoder_trial_modes.size());
		fmt_debug_printf("Total used trial mode groups: {}\n", grouped_encoder_trial_modes.count_used_groups());
		fmt_debug_printf("Max ever grid dimensions: {}x{}\n", max_grid_width, max_grid_height);
	}

	// sanity check
	assert(encoder_trial_modes.size() < 11000);
}
#endif

const uint32_t TOTAL_RGBA_CHAN_PAIRS = 6;
//const uint32_t TOTAL_RGB_CHAN_PAIRS = 3;
static const uint8_t g_rgba_chan_pairs[TOTAL_RGBA_CHAN_PAIRS][2] =
{
	{ 0, 1 },
	{ 0, 2 },
	{ 1, 2 },
	{ 0, 3 },
	{ 1, 3 },
	{ 2, 3 }
};

#if 0
static bool encoder_trial_mode_test()
{
	for (uint32_t w = 4; w <= 12; w++)
	{
		for (uint32_t h = 4; h <= 12; h++)
		{
			if (!astc_helpers::is_valid_block_size(w, h))
				continue;

			basisu::vector<basist::astc_ldr_t::trial_mode> encoder_trial_modes_orig;
			basist::astc_ldr_t::grouped_trial_modes grouped_encoder_trial_modes_orig;

			create_encoder_trial_modes_full_eval(w, h,
				encoder_trial_modes_orig, grouped_encoder_trial_modes_orig,
				false, false);

			fmt_debug_printf("Testing block size {}x{}, {} total modes\n", w, h, encoder_trial_modes_orig.size_u32());

			basisu::hash_map<basist::astc_ldr_t::trial_mode> trial_mode_hash;
			for (uint32_t i = 0; i < encoder_trial_modes_orig.size(); i++)
			{
				trial_mode_hash.insert(encoder_trial_modes_orig[i]);
			}

			basisu::vector<basist::astc_ldr_t::trial_mode> encoder_trial_modes_new;
			basist::astc_ldr_t::grouped_trial_modes grouped_encoder_trial_modes_new;

			basist::astc_ldr_t::create_encoder_trial_modes_table(w, h,
				encoder_trial_modes_new, grouped_encoder_trial_modes_new,
				false, false);

			if (encoder_trial_modes_new.size() != encoder_trial_modes_orig.size())
			{
				fmt_error_printf("trial mode test failed!\n");

				assert(0);
				return false;
			}

			for (uint32_t i = 0; i < encoder_trial_modes_new.size(); i++)
			{
				const basist::astc_ldr_t::trial_mode& tm = encoder_trial_modes_new[i];
				if (trial_mode_hash.find(tm) == trial_mode_hash.end())
				{
					fmt_error_printf("trial mode test failed!\n");

					assert(0);
					return false;
				}
			}

		} // h
	} // w

	fmt_debug_printf("trial mode test succeeded\n");
	return true;
}
#endif

//----------------------------------------------------------------------------------

struct ldr_astc_block_encode_image_high_level_config
{
	uint32_t m_block_width = 6;
	uint32_t m_block_height = 6;

	bool m_second_superpass_refinement = true;
	float m_second_superpass_fract_to_recompress = .075f;

	bool m_third_superpass_try_neighbors = true;

	float m_base_q = 75.0f;
	bool m_use_dct = false;

	bool m_subsets_enabled = true;
	bool m_subsets_edge_filtering = true;

	bool m_filter_by_pca_angles_flag = true;
		
	float m_use_direct_angle_thresh = .25f;
	float m_use_base_scale_angle_thresh = 7.0f;
		
	bool m_force_all_dual_plane_chan_evals = false; // much slower, test on base
	bool m_disable_rgb_dual_plane = false; // DP can be on alpha only, if block has alpha
	float m_strong_dp_decorr_thresh_rgb = .998f;

	bool m_use_base_ofs = true;
	bool m_use_blue_contraction = true;

	bool m_grid_hv_filtering = true;
	bool m_low_freq_block_filtering = true;

	uint32_t m_superbucket_max_to_retain[3] = { 4, 8, 16 };

	float m_final_shortlist_fraction[3] = { .25f, .33f, .5f };
	uint32_t m_final_shortlist_min_size[3] = { 1, 1, 1 };
	uint32_t m_final_shortlist_max_size[3] = { 4096, 4096, 4096 };

	uint32_t m_part2_fraction_to_keep = 2;
	uint32_t m_part3_fraction_to_keep = 2;
	uint32_t m_base_parts2 = 32;
	uint32_t m_base_parts3 = 32;

	float m_early_stop_wpsnr = 0.0f;
	float m_early_stop2_wpsnr = 0.0f;

	bool m_blurring_enabled_p1 = false;
	bool m_blurring_enabled_p2 = false;

	bool m_gradient_descent_flag = true;
	bool m_polish_weights_flag = true;
	bool m_qcd_enabled_flag = true; // gradient descent must be enabled too
	bool m_bucket_pruning_passes = true;
	float m_encode_trial_early_out_thresh = .1f;
	float m_encode_trial_subsets_early_out_thresh = .1f;

	// 2nd superpass options
	uint32_t m_base_parts2_p2 = 64;
	uint32_t m_base_parts3_p2 = 64;
	uint32_t m_superbucket_max_to_retain_p2[3] = { 16, 32, 256 };
	uint32_t m_final_shortlist_max_size_p2[3] = { 4096, 4096, 4096 };
	uint32_t m_second_pass_total_weight_refine_passes = astc_ldr::WEIGHT_REFINER_MAX_PASSES;
	bool m_second_pass_force_subsets_enabled = true;
	bool m_force_all_dp_chans_p2 = false;
	bool m_final_encode_always_try_rgb_direct = false;
	bool m_filter_by_pca_angles_flag_p2 = true;
		
	bool m_try_simplified_latent_configs = false;

	bool m_debug_images = false;
	bool m_debug_output = false;
	bool m_debug_output_image_metrics = true;

	std::string m_debug_file_prefix;

	job_pool* m_pJob_pool;
		
	astc_ldr::cem_encode_params m_cem_enc_params;
};

struct ldr_astc_block_encode_image_output
{
	ldr_astc_block_encode_image_output()
	{
	}

	~ldr_astc_block_encode_image_output()
	{
		interval_timer itm;
		itm.start();

		const int num_blocks_x = m_image_block_info.get_width();
		const int num_blocks_y = m_image_block_info.get_height();

		for (int y = num_blocks_y - 1; y >= 0; --y)
		{
			for (int x = num_blocks_x - 1; x >= 0; --x)
			{
				auto& out_blocks = m_image_block_info(x, y).m_out_blocks;
				out_blocks.clear();
			}
		} // y

		//fmt_debug_printf("Cleared enc_out image block info: {3.3} secs\n", itm.get_elapsed_secs());
	}

	astc_ldr::partitions_data m_part_data_p2;
	astc_ldr::partitions_data m_part_data_p3;

	basisu::vector<basist::astc_ldr_t::trial_mode> m_encoder_trial_modes;
	basist::astc_ldr_t::grouped_trial_modes m_grouped_encoder_trial_modes;

	vector2D<astc_helpers::astc_block> m_packed_phys_blocks;
		
	struct block_info
	{
		block_info()
		{
			m_pixel_stats.clear();
		}

		astc_ldr::pixel_stats_t m_pixel_stats; // of original/input block

		basisu::vector<encode_block_output> m_out_blocks;

		uint32_t m_packed_out_block_index = 0; // index of chosen block (typically best out block by WSSE, but not necessarily)
			
		// TODO: Remove, lossy supercompression uses it, but not all compressors write them.
		bool m_low_freq_block_flag = false;
		bool m_super_strong_edges = false;
		bool m_very_strong_edges = false;
		bool m_strong_edges = false;
	};

	vector2D<block_info> m_image_block_info;

	struct block_info_superpass1
	{
		int m_config_reuse_neighbor_out_block_indices[basist::astc_ldr_t::cMaxConfigReuseNeighbors] = { cInvalidIndex, cInvalidIndex, cInvalidIndex };

		bool m_config_reuse_new_neighbor_out_block_flags[basist::astc_ldr_t::cMaxConfigReuseNeighbors] = { false, false, false };

		basisu::vector<encode_block_output> m_new_out_config_reuse_blocks;
		basisu::vector<encode_block_output> m_new_out_config_endpoint_reuse_blocks;
	};

	vector2D<block_info_superpass1> m_image_block_info_superpass2;

private:
	ldr_astc_block_encode_image_output(const ldr_astc_block_encode_image_output&);
	ldr_astc_block_encode_image_output& operator= (const ldr_astc_block_encode_image_output&);
};

constexpr bool selective_blurring = true;

struct encoder_config_manager
{
	float m_max_std_dev;
	float m_sobel_energy;
	bool m_is_lum_only;
	basisu::vector<float> m_block_dct_energy;
	bool m_filter_horizontally_flag;
	bool m_low_freq_block_flag;
	uint32_t m_total_active_chans;
	basisu::comparative_stats<float> m_cross_chan_stats[TOTAL_RGBA_CHAN_PAIRS];
	float m_chan_pair_correlations[6];
	bool m_active_chan_flags[4];
	float m_min_corr, m_max_corr;
	bool m_used_alpha_encoder_modes;
	astc_ldr::cem_encode_params m_temp_cem_enc_params;

	encoder_config_manager()
	{
		clear();
	}
	
	void clear()
	{
		m_max_std_dev = 0.0f;
		m_sobel_energy = 0.0f;
		m_is_lum_only = false;
		m_block_dct_energy.clear();
		m_filter_horizontally_flag = false;
		m_low_freq_block_flag = false;
		m_total_active_chans = 0;
		for (uint32_t i = 0; i < TOTAL_RGBA_CHAN_PAIRS; i++)
			m_cross_chan_stats[i].clear();
		clear_obj(m_chan_pair_correlations);
		clear_obj(m_active_chan_flags);
		m_min_corr = 0, m_max_corr = 0;
		m_used_alpha_encoder_modes = false;
		m_sobel_energy = 0.0f;
	}

	void init(
		uint32_t bx, uint32_t by,
		uint32_t block_width, uint32_t block_height, uint32_t total_block_pixels,
		const astc_ldr::pixel_stats_t& pixel_stats,
		const basist::astc_ldr_t::dct2f& dct,
		const ldr_astc_block_encode_image_high_level_config& enc_cfg,
		const image& sobel_xy,
		image& vis_dct_low_freq_block,
		uint32_t blur_id)
	{
		m_max_std_dev = 0.0f;
		for (uint32_t i = 0; i < 4; i++)
			m_max_std_dev = maximum(m_max_std_dev, pixel_stats.m_rgba_stats[i].m_std_dev);

		m_is_lum_only = true;

		for (uint32_t y = 0; y < block_height; y++)
		{
			for (uint32_t x = 0; x < block_width; x++)
			{
				const color_rgba& c = pixel_stats.m_pixels[x + y * block_width];
				bool is_lum_texel = (c.r == c.g) && (c.r == c.b);
				if (!is_lum_texel)
				{
					m_is_lum_only = false;
					break;
				}
			}
			if (m_is_lum_only)
				break;
		}

		// TODO: allocation
		m_block_dct_energy.resize(total_block_pixels);
		m_block_dct_energy.set_all(0);

		m_filter_horizontally_flag = false;
		m_low_freq_block_flag = false;

		{
			// TODO: allocations
			basisu::vector<float> block_floats(total_block_pixels);
			basisu::vector<float> block_dct(total_block_pixels);
			basist::astc_ldr_t::fvec work;

			for (uint32_t c = 0; c < 4; c++)
			{
				for (uint32_t i = 0; i < total_block_pixels; i++)
					block_floats[i] = pixel_stats.m_pixels_f[i][c];

				dct.forward(block_floats.data(), block_dct.data(), work);

				for (uint32_t y = 0; y < block_height; y++)
					for (uint32_t x = 0; x < block_width; x++)
						m_block_dct_energy[x + y * block_width] += (float)enc_cfg.m_cem_enc_params.m_comp_weights[c] * squaref(block_dct[x + y * block_width]);

			} // c

			// Wipe DC
			m_block_dct_energy[0] = 0.0f;

			float tot_energy = compute_preserved_dct_energy(block_width, block_height, m_block_dct_energy.get_ptr(), block_width, block_height);

			float h_energy_lost = compute_lost_dct_energy(block_width, block_height, m_block_dct_energy.get_ptr(), block_width / 2, block_height);
			float v_energy_lost = compute_lost_dct_energy(block_width, block_height, m_block_dct_energy.get_ptr(), block_width, block_height / 2);

			m_filter_horizontally_flag = h_energy_lost < v_energy_lost;

			float hv2_lost_energy_fract = compute_lost_dct_energy(block_width, block_height, m_block_dct_energy.get_ptr(), 2, 2);
			if (tot_energy)
				hv2_lost_energy_fract /= tot_energy;

			const float LOW_FREQ_BLOCK_LOST_ENERGY_FRACT_THRESH = .03f;
			const float LOW_FREQ_BLOCK_MAX_STD_DEV_TRESH = 1.0f / 255.0f;
			// Ultra-smooth block determination: Only look at small grids if the block is mostly low frequency energy OR it has a very low max standard deviation.
			if ((hv2_lost_energy_fract < LOW_FREQ_BLOCK_LOST_ENERGY_FRACT_THRESH) || (m_max_std_dev < LOW_FREQ_BLOCK_MAX_STD_DEV_TRESH))
				m_low_freq_block_flag = true;
		}

		if ((enc_cfg.m_debug_images) && (blur_id == 0))
			vis_dct_low_freq_block.fill_box(bx * block_width, by * block_height, block_width, block_height, m_low_freq_block_flag ? color_rgba(255, 0, 0, 255) : g_black_color);

		for (uint32_t i = 0; i < 4; i++)
			m_active_chan_flags[i] = false;

		// The number of channels with non-zero spans
		m_total_active_chans = 0;

		for (uint32_t i = 0; i < 4; i++)
		{
			if (pixel_stats.m_rgba_stats[i].m_range > 0.0f)
			{
				assert(pixel_stats.m_max[i] != pixel_stats.m_min[i]);

				m_active_chan_flags[i] = true;

				m_total_active_chans++;
			}
			else
			{
				assert(pixel_stats.m_max[i] == pixel_stats.m_min[i]);
			}
		}

		for (uint32_t i = 0; i < TOTAL_RGBA_CHAN_PAIRS; i++)
		{
			m_cross_chan_stats[i].clear();
			
			// def=max correlation for each channel pair (or 1 if one of the channels is inactive)
			m_chan_pair_correlations[i] = 1.0f;
		}
				
		// 0=0, 1
		// 1=0, 2
		// 2=1, 2
		// 3=0, 3
		// 4=1, 3
		// 5=2, 3

		m_min_corr = 1.0f;
		m_max_corr = 0.0f;

		for (uint32_t pair_index = 0; pair_index < TOTAL_RGBA_CHAN_PAIRS; pair_index++)
		{
			const uint32_t chanA = g_rgba_chan_pairs[pair_index][0];
			const uint32_t chanB = g_rgba_chan_pairs[pair_index][1];

			// If both channels were active, we've got usable correlation statistics.
			if (m_active_chan_flags[chanA] && m_active_chan_flags[chanB])
			{
				// TODO: This can be directly derived from the 3D/4D covariance matrix entries.
				m_cross_chan_stats[pair_index].calc_pearson(total_block_pixels,
					&pixel_stats.m_pixels_f[0][chanA],
					&pixel_stats.m_pixels_f[0][chanB],
					4, 4,
					&pixel_stats.m_rgba_stats[chanA],
					&pixel_stats.m_rgba_stats[chanB]);

				m_chan_pair_correlations[pair_index] = fabsf(m_cross_chan_stats[pair_index].m_pearson);

				const float c = fabsf((float)m_cross_chan_stats[pair_index].m_pearson);
				m_min_corr = minimum(m_min_corr, c);
				m_max_corr = maximum(m_max_corr, c);
			}
		}

		// min_cor will be 1.0f if all channels inactive (solid)
				
		m_used_alpha_encoder_modes = pixel_stats.m_has_alpha;

		m_sobel_energy = 0.0f;
		for (uint32_t y = 0; y < block_height; y++)
		{
			for (uint32_t x = 0; x < block_width; x++)
			{
				const color_rgba& s = sobel_xy.get_clamped(bx * block_width + x, by * block_height + y);
				m_sobel_energy += s[0] * s[0] + s[1] * s[1] + s[2] * s[2] + s[3] * s[3];
			} // x 
		} // y

		m_sobel_energy /= (float)total_block_pixels;
	}

	void select(
		ldr_astc_lowlevel_block_encoder_params& enc_blk_params,
		uint32_t superpass_index,
		uint32_t bx, uint32_t by,
		uint32_t block_width, uint32_t block_height, uint32_t total_block_pixels,
		const image& orig_img_sobel_xy,
		const basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes,
		basist::astc_ldr_t::grouped_trial_modes& grouped_encoder_trial_modes,
		astc_ldr::partitions_data* pPart_data_p2, astc_ldr::partitions_data* pPart_data_p3,
		const astc_ldr::pixel_stats_t& pixel_stats,
		const ldr_astc_block_encode_image_high_level_config& enc_cfg,
		const basist::astc_ldr_t::dct2f& dct,
		uint32_t blur_id)
	{
		BASISU_NOTE_UNUSED(blur_id);

		enc_blk_params.m_block_width = block_width;
		enc_blk_params.m_block_height = block_height;
		enc_blk_params.m_total_block_pixels = total_block_pixels;
		enc_blk_params.m_bx = bx;
		enc_blk_params.m_by = by;

		enc_blk_params.m_pOrig_img_sobel_xy_t = &orig_img_sobel_xy;

		enc_blk_params.m_num_trial_modes = encoder_trial_modes.size_u32();
		enc_blk_params.m_pTrial_modes = encoder_trial_modes.get_ptr();
		enc_blk_params.m_pGrouped_trial_modes = &grouped_encoder_trial_modes;

		enc_blk_params.m_pPart_data_p2 = pPart_data_p2;
		enc_blk_params.m_pPart_data_p3 = pPart_data_p3;
		enc_blk_params.m_pEnc_params = &enc_cfg.m_cem_enc_params;

		float ang_dot = saturate(pixel_stats.m_zero_rel_axis3.dot3(pixel_stats.m_mean_rel_axis3));
		const float pca_axis_angles = acosf(ang_dot) * (180.0f / (float)cPiD);

		enc_blk_params.m_use_alpha_or_opaque_modes = m_used_alpha_encoder_modes;
		enc_blk_params.m_use_lum_direct_modes = m_is_lum_only;

		const bool filter_by_pca_angles_flag = (superpass_index == 1) ? enc_cfg.m_filter_by_pca_angles_flag_p2 : enc_cfg.m_filter_by_pca_angles_flag;
		if (!filter_by_pca_angles_flag)
		{
			enc_blk_params.m_use_direct_modes = true;
			enc_blk_params.m_use_base_scale_modes = true;
		}
		else
		{
			// TODO: Make selective based off edge blocks?
			enc_blk_params.m_use_direct_modes = (!m_total_active_chans) || (pca_axis_angles > enc_cfg.m_use_direct_angle_thresh);
			enc_blk_params.m_use_base_scale_modes = (pca_axis_angles <= enc_cfg.m_use_base_scale_angle_thresh);
		}
				
		enc_blk_params.m_grid_hv_filtering = enc_cfg.m_grid_hv_filtering;
		enc_blk_params.m_filter_horizontally_flag = m_filter_horizontally_flag;

		enc_blk_params.m_use_small_grids_only = m_low_freq_block_flag && enc_cfg.m_low_freq_block_filtering;

		enc_blk_params.m_subsets_enabled = enc_cfg.m_subsets_enabled && (!m_low_freq_block_flag || !enc_cfg.m_subsets_edge_filtering);

		enc_blk_params.m_subsets_edge_filtering = enc_cfg.m_subsets_edge_filtering;

		enc_blk_params.m_use_blue_contraction = enc_cfg.m_use_blue_contraction;
		enc_blk_params.m_final_encode_try_base_ofs = enc_cfg.m_use_base_ofs;

		memcpy(enc_blk_params.m_superbucket_max_to_retain, enc_cfg.m_superbucket_max_to_retain, sizeof(enc_cfg.m_superbucket_max_to_retain));

		memcpy(enc_blk_params.m_final_shortlist_fraction, enc_cfg.m_final_shortlist_fraction, sizeof(enc_blk_params.m_final_shortlist_fraction));
		memcpy(enc_blk_params.m_final_shortlist_min_size, enc_cfg.m_final_shortlist_min_size, sizeof(enc_cfg.m_final_shortlist_min_size));
		memcpy(enc_blk_params.m_final_shortlist_max_size, enc_cfg.m_final_shortlist_max_size, sizeof(enc_blk_params.m_final_shortlist_max_size));

		enc_blk_params.m_part2_fraction_to_keep = enc_cfg.m_part2_fraction_to_keep;
		enc_blk_params.m_part3_fraction_to_keep = enc_cfg.m_part3_fraction_to_keep;
		enc_blk_params.m_base_parts2 = enc_cfg.m_base_parts2;
		enc_blk_params.m_base_parts3 = enc_cfg.m_base_parts3;
		enc_blk_params.m_gradient_descent_flag = enc_cfg.m_gradient_descent_flag;
		enc_blk_params.m_polish_weights_flag = enc_cfg.m_polish_weights_flag;
		enc_blk_params.m_qcd_enabled_flag = enc_cfg.m_qcd_enabled_flag;
		enc_blk_params.m_encode_trial_early_out_thresh = enc_cfg.m_encode_trial_early_out_thresh;
		enc_blk_params.m_encode_trial_subsets_early_out_thresh = enc_cfg.m_encode_trial_subsets_early_out_thresh;
		enc_blk_params.m_bucket_pruning_passes = enc_cfg.m_bucket_pruning_passes;

		enc_blk_params.m_alpha_cems = m_used_alpha_encoder_modes;

		enc_blk_params.m_early_stop_wpsnr = enc_cfg.m_early_stop_wpsnr;
		enc_blk_params.m_early_stop2_wpsnr = enc_cfg.m_early_stop2_wpsnr;

		enc_blk_params.m_final_encode_always_try_rgb_direct = enc_cfg.m_final_encode_always_try_rgb_direct;

		enc_blk_params.m_pDCT2F = &dct;

		// Determine DP usage
		if (enc_cfg.m_force_all_dual_plane_chan_evals)
		{
			for (uint32_t i = 0; i < 4; i++)
				enc_blk_params.m_dp_active_chans[i] = m_active_chan_flags[i];
		}
		else
		{
			for (uint32_t i = 0; i < 3; i++)
				enc_blk_params.m_dp_active_chans[i] = false;

			// Being very conservative with alpha here - always let the analytical evaluator consider it.
			enc_blk_params.m_dp_active_chans[3] = pixel_stats.m_has_alpha;

			if (!enc_cfg.m_disable_rgb_dual_plane)
			{
				const float rg_corr = m_chan_pair_correlations[0];
				const float rb_corr = m_chan_pair_correlations[1];
				const float gb_corr = m_chan_pair_correlations[2];

				int desired_dp_chan_rgb = -1;

				float min_p = minimum(rg_corr, rb_corr, gb_corr);

				if (min_p < enc_cfg.m_strong_dp_decorr_thresh_rgb)
				{
					const bool has_r = m_active_chan_flags[0], has_g = m_active_chan_flags[1];
					//const bool has_b = active_chan_flags[2];

					uint32_t total_active_chans_rgb = 0;
					for (uint32_t i = 0; i < 3; i++)
						total_active_chans_rgb += m_active_chan_flags[i];

					if (total_active_chans_rgb == 2)
					{
						if (!has_r)
							desired_dp_chan_rgb = 1;
						else if (!has_g)
							desired_dp_chan_rgb = 0;
						else
							desired_dp_chan_rgb = 0;
					}
					else if (total_active_chans_rgb == 3)
					{
						// see if rg/rb is weakly correlated vs. gb
						if ((rg_corr < gb_corr) && (rb_corr < gb_corr))
							desired_dp_chan_rgb = 0;
						// see if gr/gb is weakly correlated vs. rb
						else if ((rg_corr < rb_corr) && (gb_corr < rb_corr))
							desired_dp_chan_rgb = 1;
						// assume b is weakest
						else
							desired_dp_chan_rgb = 2;
					}
				}

				if (desired_dp_chan_rgb != -1)
				{
					assert(m_active_chan_flags[desired_dp_chan_rgb]);
					enc_blk_params.m_dp_active_chans[desired_dp_chan_rgb] = true;
				}
			}
		}

		if (!enc_blk_params.m_dp_active_chans[0] && !enc_blk_params.m_dp_active_chans[1] && !enc_blk_params.m_dp_active_chans[2] && !enc_blk_params.m_dp_active_chans[3])
		{
			enc_blk_params.m_use_dual_planes = false;
		}
				
		if (superpass_index == 1)
		{
			enc_blk_params.m_base_parts2 = enc_cfg.m_base_parts2_p2;
			enc_blk_params.m_base_parts3 = enc_cfg.m_base_parts3_p2;
			enc_blk_params.m_part2_fraction_to_keep = 1;
			enc_blk_params.m_part3_fraction_to_keep = 1;

			memcpy(enc_blk_params.m_superbucket_max_to_retain, enc_cfg.m_superbucket_max_to_retain_p2, sizeof(enc_cfg.m_superbucket_max_to_retain_p2));
			memcpy(enc_blk_params.m_final_shortlist_max_size, enc_cfg.m_final_shortlist_max_size_p2, sizeof(enc_cfg.m_final_shortlist_max_size_p2));

			if (enc_cfg.m_second_pass_force_subsets_enabled)
				enc_blk_params.m_subsets_enabled = true;
			enc_blk_params.m_subsets_edge_filtering = false;

			if (enc_cfg.m_force_all_dp_chans_p2)
			{
				enc_blk_params.m_dp_active_chans[0] = m_active_chan_flags[0];
				enc_blk_params.m_dp_active_chans[1] = m_active_chan_flags[1];
				enc_blk_params.m_dp_active_chans[2] = m_active_chan_flags[2];
				enc_blk_params.m_dp_active_chans[3] = m_active_chan_flags[3];
				enc_blk_params.m_use_dual_planes = true;

				if (!enc_blk_params.m_dp_active_chans[0] && !enc_blk_params.m_dp_active_chans[1] && !enc_blk_params.m_dp_active_chans[2] && !enc_blk_params.m_dp_active_chans[3])
				{
					enc_blk_params.m_use_dual_planes = false;
				}
			}

			enc_blk_params.m_gradient_descent_flag = true;
			enc_blk_params.m_polish_weights_flag = true;
						
			enc_blk_params.m_use_direct_modes = true;
			//enc_blk_params.m_use_base_scale_modes = true; // just leaving this alone now from the first pass, it will be disabled for good reasons now

			enc_blk_params.m_early_stop_wpsnr = enc_cfg.m_early_stop_wpsnr + 2.0f;
			enc_blk_params.m_early_stop2_wpsnr = enc_cfg.m_early_stop2_wpsnr + 2.0f;

			if (enc_cfg.m_second_pass_total_weight_refine_passes)
			{
				m_temp_cem_enc_params = enc_cfg.m_cem_enc_params;
				enc_blk_params.m_pEnc_params = &m_temp_cem_enc_params;

				m_temp_cem_enc_params.m_total_weight_refine_passes = enc_cfg.m_second_pass_total_weight_refine_passes;
				m_temp_cem_enc_params.m_worst_weight_nudging_flag = true;
				m_temp_cem_enc_params.m_endpoint_refinement_flag = true;
			}
		}
	}
};

static bool apply_weight_grid_dct(
	uint32_t block_width, uint32_t block_height,
	basist::astc_ldr_t::grid_weight_dct &grid_coder,
	encode_block_output& out_block,
	const astc_ldr::pixel_stats_t& pixel_stats, bool recalc_block_wsse,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	astc_ldr::partitions_data* pPart_data_p2, astc_ldr::partitions_data* pPart_data_p3,
	bool try_refining_endpoints)
{
	if (out_block.m_trial_mode_index < 0)
		return true;

	astc_helpers::log_astc_block& log_astc_blk = out_block.m_log_blk;
	
	assert(!log_astc_blk.m_solid_color_flag_ldr);

	const uint32_t num_planes = (log_astc_blk.m_dual_plane ? 2 : 1);
	const uint32_t total_grid_weights = log_astc_blk.m_grid_width * log_astc_blk.m_grid_height;

	basist::astc_ldr_t::fvec dct_temp;

	for (uint32_t plane_index = 0; plane_index < num_planes; plane_index++)
	{
		basist::astc_ldr_t::dct_syms& syms = out_block.m_packed_dct_plane_data[plane_index];

		code_block_weights(grid_coder, enc_cfg.m_base_q, plane_index, log_astc_blk, syms, dct_temp);

		// ensure existing weights get blown away 
		if (num_planes == 1)
		{
			memset(log_astc_blk.m_weights, 0, total_grid_weights);
		}
		else
		{
			for (uint32_t i = 0; i < total_grid_weights; i++)
				log_astc_blk.m_weights[i * num_planes + plane_index] = 0;
		}

		// decode the actual post-DCT/quant weights
		const bool status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, log_astc_blk, nullptr, nullptr, dct_temp, &syms);

		assert(status);
		if (!status)
		{
			error_printf("grid_coder.decode_block_weights() failed!\n");
			return false;
		}
	}

	if (!try_refining_endpoints)
	{
		if (recalc_block_wsse)
		{
			out_block.m_sse = eval_error(block_width, block_height, log_astc_blk, pixel_stats, enc_cfg.m_cem_enc_params);
		}
		return true;
	}

	uint64_t cur_err = eval_error(block_width, block_height, log_astc_blk, pixel_stats, enc_cfg.m_cem_enc_params);

	astc_helpers::log_astc_block new_log_astc_blk(log_astc_blk);

	bool status;
	if (log_astc_blk.m_num_partitions == 1)
	{
		status = encode_trial_refine_only(
			block_width, block_height,
			pixel_stats,
			new_log_astc_blk,
			enc_cfg.m_cem_enc_params);
	}
	else
	{
		astc_ldr::partitions_data* pPart_data = (new_log_astc_blk.m_num_partitions == 2) ? pPart_data_p2 : pPart_data_p3;

		const uint32_t part_seed_index = new_log_astc_blk.m_partition_id;
		const uint32_t part_unique_index = pPart_data->m_part_seed_to_unique_index[part_seed_index];
		const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[part_unique_index];

		status = encode_trial_subsets(
			block_width, block_height,
			pixel_stats,
			new_log_astc_blk.m_color_endpoint_modes[0], new_log_astc_blk.m_num_partitions,
			new_log_astc_blk.m_partition_id, pPat,
			new_log_astc_blk.m_endpoint_ise_range, new_log_astc_blk.m_weight_ise_range,
			new_log_astc_blk.m_grid_width, new_log_astc_blk.m_grid_height,
			enc_cfg.m_encode_trial_subsets_early_out_thresh,
			new_log_astc_blk,
			enc_cfg.m_cem_enc_params,
			true,
			enc_cfg.m_gradient_descent_flag, enc_cfg.m_polish_weights_flag, enc_cfg.m_qcd_enabled_flag,
			enc_cfg.m_use_blue_contraction);
	}

	if (status)
	{
		const uint32_t num_cem_endpoint_vals = astc_helpers::get_num_cem_values(new_log_astc_blk.m_color_endpoint_modes[0]);
		const uint32_t total_endpoint_vals = num_cem_endpoint_vals * new_log_astc_blk.m_num_partitions;
		
		const bool endpoints_differ = memcmp(log_astc_blk.m_endpoints, new_log_astc_blk.m_endpoints, total_endpoint_vals) != 0;

		if (endpoints_differ)
		{
			for (uint32_t plane_index = 0; plane_index < num_planes; plane_index++)
			{
				basist::astc_ldr_t::dct_syms& syms = out_block.m_packed_dct_plane_data[plane_index];

				// decode the actual post-DCT/quant weights
				status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, new_log_astc_blk, nullptr, nullptr, dct_temp, &syms);
				assert(status);

				if (!status)
				{
					error_printf("grid_coder.decode_block_weights() failed!\n");
					return false;
				}
			}

			uint64_t new_err = eval_error(block_width, block_height, new_log_astc_blk, pixel_stats, enc_cfg.m_cem_enc_params);

			if (new_err < cur_err)
			{
				cur_err = new_err;
				log_astc_blk = new_log_astc_blk;
			}
		}
	}

	if (recalc_block_wsse)
		out_block.m_sse = cur_err;
	
	return true;
}

static void filter_block(
	uint32_t block_width, uint32_t block_height, 
	uint32_t grid_width, uint32_t grid_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	color_rgba *pUpsampled_block)
{
	const uint32_t num_block_samples = block_width * block_height;
	const uint32_t num_grid_samples = grid_width * grid_height;

	const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, grid_width, grid_height);

	const basisu::vector<float>& d = pGrid_data->m_downsample_matrix; // rows=output num_grid_sampless, cols=input num_block_samples (texels)
	const basisu::vector2D<float>& u = pGrid_data->m_upsample_matrix; // rows=output num_block_samples (texels), cols=input num_grid_samples

	for (uint32_t c = 0; c < 4; c++)
	{
		float downsampled_block[astc_helpers::MAX_BLOCK_PIXELS]; // num_grid_samples 

		if ((c == 3) && (!pixel_stats.m_has_alpha))
		{
			for (uint32_t i = 0; i < num_block_samples; i++)
				pUpsampled_block[i].a = 255;
			break;
		}

		for (uint32_t g = 0; g < num_grid_samples; g++)
		{
			float sum = 0;
			for (uint32_t i = 0; i < num_block_samples; i++)
				sum += (float)pixel_stats.m_pixels[i][c] * d[g * num_block_samples + i];

			downsampled_block[g] = sum;
		}

		for (uint32_t k = 0; k < num_block_samples; k++)
		{
			float sum = 0;
			for (uint32_t i = 0; i < num_grid_samples; i++)
				sum += (float)downsampled_block[i] * u.at_row_col(k, i);

			pUpsampled_block[k][c] = (uint8_t)clamp<int>(basisu::fast_roundf_pos_int(sum), 0, 255);
		}
	} // c
}

// very slow
[[maybe_unused]] static int find_tm_index(const basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes, const astc_helpers::log_astc_block& log_blk)
{
	assert(astc_helpers::is_block_xuastc_ldr(log_blk));

	for (uint32_t i = 0; i < encoder_trial_modes.size(); i++)
	{
		const basist::astc_ldr_t::trial_mode& tm = encoder_trial_modes[i];

		if ((tm.m_cem == log_blk.m_color_endpoint_modes[0]) &&
			(tm.m_grid_width == log_blk.m_grid_width) && (tm.m_grid_height == log_blk.m_grid_height) &&
			(tm.m_num_parts == log_blk.m_num_partitions) &&
			(tm.m_weight_ise_range == log_blk.m_weight_ise_range) &&
			(tm.m_endpoint_ise_range == log_blk.m_endpoint_ise_range))
		{
			const bool tm_dp_flag = (tm.m_ccs_index >= 0);

			if (tm_dp_flag != log_blk.m_dual_plane)
				continue;

			if (tm_dp_flag)
			{
				if (tm.m_ccs_index != log_blk.m_color_component_selector)
					continue;
			}

			return i;
		}
	}

	return -1;
}

static bool encode_block_to_dc_latent(
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	basisu::vector<encode_block_output>& out_blocks,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	astc_ldr::partitions_data* pPart_data_p2, astc_ldr::partitions_data* pPart_data_p3,
	const basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes, 
	uint64_vec &best_2subset_seed_ids, // seed ID's in low 10 bits
	uint64_vec& best_3subset_seed_ids) // seed ID's in low 10 bits
{
	BASISU_NOTE_UNUSED(encoder_trial_modes);

	best_2subset_seed_ids.resize(0);
	best_2subset_seed_ids.reserve(pPart_data_p2->m_total_unique_patterns);

	best_3subset_seed_ids.resize(0);
	best_3subset_seed_ids.reserve(pPart_data_p3->m_total_unique_patterns);

	const uint32_t block_size_index = astc_helpers::get_block_size_index(block_width, block_height);
		
	const uint32_t total_block_pixels = block_width * block_height;
		
	astc_helpers::log_astc_block log_blk;
	log_blk.clear();
	log_blk.m_grid_width = 3;
	log_blk.m_grid_height = 2;
	log_blk.m_color_endpoint_modes[0] = 6;
	log_blk.m_color_endpoint_modes[1] = 6;
	log_blk.m_weight_ise_range = astc_helpers::BISE_16_LEVELS;
		
	for (uint32_t subsets = 2; subsets <= 3; subsets++)
	{
		encode_block_output& new_output_blk = *out_blocks.enlarge(1);
		new_output_blk.clear();

		astc_helpers::log_astc_block &best_log_blk = new_output_blk.m_log_blk;
		uint64_t best_err = UINT64_MAX;

		const astc_ldr::partitions_data* pPat_data = (subsets == 3) ? pPart_data_p3 : pPart_data_p2;

		log_blk.m_num_partitions = (uint8_t)subsets;
		log_blk.m_endpoint_ise_range = (uint8_t)((subsets == 3) ? astc_helpers::BISE_64_LEVELS : astc_helpers::BISE_256_LEVELS);
		log_blk.m_color_endpoint_modes[2] = (subsets == 3) ? 6 : 0;

		//const auto& weight_quant_tab = astc_helpers::g_dequant_tables.get_weight_tab(log_blk.m_weight_ise_range).m_val_to_ise;
		const auto& endpoint_quant_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(log_blk.m_endpoint_ise_range).m_val_to_ise;
				
		for (uint32_t unique_part_iter = 0; unique_part_iter < pPat_data->m_total_unique_patterns; unique_part_iter++)
		{
			const uint32_t part_seed = pPat_data->m_unique_index_to_part_seed[unique_part_iter];

			log_blk.m_partition_id = basisu::safe_cast_uint16(part_seed);

			vec4F sums[3] = { vec4F(0.0f), vec4F(0.0f), vec4F(0.0f) };
			uint32_t num[3] = { 0, 0, 0 };

			[[maybe_unused]] uint32_t p_hist[3] = { 0, 0, 0 };

			for (uint32_t y = 0; y < block_height; y++)
			{
				for (uint32_t x = 0; x < block_width; x++)
				{
					uint32_t p = astc_helpers::get_precomputed_texel_partition(block_width, block_height, part_seed, x, y, log_blk.m_num_partitions);
										
					sums[p][0] += pixel_stats.m_pixels_f[x + y * block_width][0];
					sums[p][1] += pixel_stats.m_pixels_f[x + y * block_width][1];
					sums[p][2] += pixel_stats.m_pixels_f[x + y * block_width][2];

					num[p]++;
				} // x
			} // y

			for (uint32_t p = 0; p < subsets; p++)
			{
				assert(num[p]);

				sums[p] /= (float)num[p];

				for (uint32_t c = 0; c < 3; c++)
					log_blk.m_endpoints[p * astc_helpers::NUM_MODE6_ENDPOINTS + c] = endpoint_quant_tab[clamp<int>((int)std::round(255.0f * sums[p][c]), 0, 255)];

				log_blk.m_endpoints[p * astc_helpers::NUM_MODE6_ENDPOINTS + 3] = endpoint_quant_tab[(subsets == 3) ? 248 : 255];
			}

			uint64_t best_k_err = UINT64_MAX;

			for (uint32_t k = (subsets == 3) ? 13 : 15; k < 16; k++)
			{
				for (uint32_t i = 0; i < 6; i++)
					log_blk.m_weights[i] = (uint8_t)k;

				color_rgba unpacked_pixels_astc[astc_helpers::MAX_BLOCK_PIXELS];
				bool astc_status = astc_helpers::decode_block_xuastc_ldr(log_blk, unpacked_pixels_astc, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
				assert(astc_status);
				if (!astc_status)
					return false;

				uint64_t total_err = 0;
				for (uint32_t i = 0; i < total_block_pixels; i++)
					total_err += weighted_color_error(pixel_stats.m_pixels[i], unpacked_pixels_astc[i], enc_cfg.m_cem_enc_params);

				best_k_err = minimum(best_k_err, total_err);

				if (total_err < best_err)
				{
					best_err = total_err;
					best_log_blk = log_blk;
				}
			} // k

			if (subsets == 2)
			{
				best_2subset_seed_ids.push_back((best_k_err << 10) | log_blk.m_partition_id);
			}
			else
			{
				best_3subset_seed_ids.push_back((best_k_err << 10) | log_blk.m_partition_id);
			}

		} // part_seed

		best_2subset_seed_ids.sort();
		best_3subset_seed_ids.sort();
		
		// XUASTC LDR trial mode indices
		static const uint32_t s_tm_index2[14] = { 675, 675, 974, 974,  1263, 974, 1263, 974,  1263, 1747, 1747, 2131,  2131, 2425 };
		static const uint32_t s_tm_index3[14] = { 679, 679, 978, 978,  1267, 978, 1267, 978,  1267, 1751, 1751, 2135,  2135, 2429 };

		new_output_blk.m_trial_mode_index = basisu::safe_cast_int16((best_log_blk.m_num_partitions == 2) ? s_tm_index2[block_size_index] : s_tm_index3[block_size_index]);

		assert(new_output_blk.m_trial_mode_index == find_tm_index(encoder_trial_modes, best_log_blk));

		new_output_blk.m_sse = best_err;
		new_output_blk.m_blur_id = BLUR_ID_DC_LATENT_BASE + best_log_blk.m_num_partitions - 2;
				
	} // subsets

	return true;
}

static bool encode_block_to_linear_latent(
	uint32_t num_patterns_to_try,
	uint32_t block_width, uint32_t block_height,
	const astc_ldr::pixel_stats_t& pixel_stats,
	basisu::vector<encode_block_output>& out_blocks,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	astc_ldr::partitions_data* pPart_data_p2, astc_ldr::partitions_data* pPart_data_p3,
	const basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes,
	uint64_vec& best_2subset_seed_ids,
	uint64_vec& best_3subset_seed_ids)
{
	BASISU_NOTE_UNUSED(encoder_trial_modes);
	BASISU_NOTE_UNUSED(pPart_data_p2);
	BASISU_NOTE_UNUSED(pPart_data_p3);

	static const uint32_t s_tm2_index_02[14] = { 717, 717, 1016, 1016, 1305, 1016, 1305, 1016, 1305, 1789, 1789, 2173, 2173, 2467 };
	static const uint32_t s_tm2_index_13[14] = { 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215 };

	static const uint32_t s_tm3_index_02[14] = { 721, 721, 1020, 1020, 1309, 1020, 1309, 1020,  1309,  1793, 1793, 2177, 2177, 2471 };
	static const uint32_t s_tm3_index_13[14] = { 219, 219, 219,  219, 219,   219,  219,  219,   219,   219,  219,  219,  219,  219 };

	const uint32_t block_size_index = astc_helpers::get_block_size_index(block_width, block_height);

	const uint32_t total_grid_samples = 6;

	const uint32_t weight_ise_range = astc_helpers::BISE_16_LEVELS;

	//const auto& weight_quant_tab = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_val_to_ise;
	const auto& weight_dequant_tab = astc_helpers::g_dequant_tables.get_weight_tab(weight_ise_range).m_ISE_to_val;

	const uint32_t NUM_WEIGHT_GRIDS = 4 * 4;

	static const uint32_t grid_width[NUM_WEIGHT_GRIDS] = { 3, 2, 3, 2,  3, 2, 3, 2,  3, 2, 3, 2,  3, 2, 3, 2 }, grid_height[NUM_WEIGHT_GRIDS] = { 2, 3, 2, 3,  2, 3, 2, 3,  2, 3, 2, 3,  2, 3, 2, 3 };

	static const uint8_t weight_grids[NUM_WEIGHT_GRIDS][3 * 2] =
	{
		{
			0, 0, 0,
			15, 15, 15
		},

		{
			0, 15,
			0, 15,
			0, 15
		},

		{
			15, 15, 15,
			0, 0, 0
		},

		{
			15, 0,
			15, 0,
			15, 0
		},

		// set 2
		{
			0, 0, 0,
			4, 8, 15
		},

		{
			0, 4,
			0, 8,
			0, 15
		},

		{
			15, 8, 4,
			0, 0, 0
		},

		{
			15, 0,
			8, 0,
			4, 0
		},
		
		// set 3
		{
			2, 4, 6,
			9, 11, 13
		},

		{
			2, 9,
			4, 11,
			6, 13
		},

		{
			9, 11, 13,
			2, 4, 6
		},

		{
			9, 2,
			11, 4,
			13, 6
		},

		// set 4
		{
			0, 7, 0,
			7, 15, 7
		},

		{
			0, 7,
			7, 15,
			0, 7
		},

		{
			15, 7, 15,
			7, 0, 7
		},

		{
			7, 0,
			15, 7,
			7, 0
		}
	};

	// TODO
	uint8_t dequantized_grid_weights[NUM_WEIGHT_GRIDS][astc_helpers::MAX_BLOCK_PIXELS];
	uint8_t dequantized_block_weights[NUM_WEIGHT_GRIDS][astc_helpers::MAX_BLOCK_PIXELS];

	for (uint32_t g = 0; g < NUM_WEIGHT_GRIDS; g++)
	{
		for (uint32_t i = 0; i < total_grid_samples; i++)
			dequantized_grid_weights[g][i] = weight_dequant_tab[weight_grids[g][i]];

		astc_helpers::upsample_weight_grid(
			block_width, block_height,		 // destination/to dimension
			grid_width[g], grid_height[g],		 // source/from dimension
			dequantized_grid_weights[g],		// these are dequantized [0,64] weights, NOT ISE symbols, [wy][wx]
			dequantized_block_weights[g]);		// [by][bx]
	}

	astc_ldr::cem_encode_params cem_enc_params;
	cem_enc_params.init();
	cem_enc_params.m_decode_mode_srgb = enc_cfg.m_cem_enc_params.m_decode_mode_srgb;
	cem_enc_params.m_max_ls_passes = 1;
		
	for (uint32_t subsets = 2; subsets <= 3; subsets++)
	//const uint32_t subsets = 2;
	{
		//const astc_ldr::partitions_data* pPat_data = (subsets == 3) ? pPart_data_p3 : pPart_data_p2;

		for (uint32_t grid_index = 0; grid_index < NUM_WEIGHT_GRIDS; grid_index++)
		{
			astc_helpers::log_astc_block log_blk;
			log_blk.clear();

			log_blk.m_grid_width = (uint8_t)grid_width[grid_index];
			log_blk.m_grid_height = (uint8_t)grid_height[grid_index];
			log_blk.m_num_partitions = (uint8_t)subsets;
			log_blk.m_color_endpoint_modes[0] = 8;
			log_blk.m_color_endpoint_modes[1] = 8;
			if (subsets == 3)
				log_blk.m_color_endpoint_modes[2] = 8;
			log_blk.m_endpoint_ise_range = (uint8_t)((subsets == 3) ? astc_helpers::BISE_16_LEVELS : astc_helpers::BISE_64_LEVELS);
			log_blk.m_weight_ise_range = (uint8_t)weight_ise_range;
			memcpy(log_blk.m_weights, weight_grids[grid_index], total_grid_samples);

			//const auto& endpoint_quant_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(log_blk.m_endpoint_ise_range).m_val_to_ise;

			uint64_t best_err = UINT64_MAX;
			astc_helpers::log_astc_block best_log_blk;
			best_log_blk.clear();

			//for (uint32_t unique_part_iter = 0; unique_part_iter < pPat_data->m_total_unique_patterns; unique_part_iter++)
			for (uint32_t pat_index = 0; pat_index < num_patterns_to_try; pat_index++)
			{
				//const uint32_t part_seed = pPat_data->m_unique_index_to_part_seed[unique_part_iter];
				const uint32_t part_seed = ((subsets == 2) ? best_2subset_seed_ids[pat_index] : best_3subset_seed_ids[pat_index]) & 1023;
							
				log_blk.m_partition_id = basisu::safe_cast_uint16(part_seed);

				color_rgba subset_pixels[3][astc_helpers::MAX_BLOCK_PIXELS];
				uint8_t subset_weights[3][astc_helpers::MAX_BLOCK_PIXELS];
				uint32_t num_subset_pixels[3] = { 0, 0, 0 };

				astc_ldr::pixel_stats_t stats[3];

				for (uint32_t y = 0; y < block_height; y++)
				{
					for (uint32_t x = 0; x < block_width; x++)
					{
						uint32_t s = astc_helpers::get_precomputed_texel_partition(block_width, block_height, part_seed, x, y, log_blk.m_num_partitions);

						const uint32_t n = num_subset_pixels[s];

						subset_pixels[s][n] = pixel_stats.m_pixels[y * block_width + x];
						subset_weights[s][n] = dequantized_block_weights[grid_index][y * block_width + x];

						num_subset_pixels[s] = n + 1;

					} // x
				} // y

				uint64_t total_err = 0;
				for (uint32_t s = 0; s < subsets; s++)
				{
					stats[s].init(num_subset_pixels[s], subset_pixels[s]);

					cem_enc_params.m_pForced_weight_vals0 = subset_weights[s];

					uint8_t temp_weights[astc_helpers::MAX_BLOCK_PIXELS];

					uint64_t subset_err = astc_ldr::cem_encode_pixels(8, -1, stats[s], cem_enc_params, log_blk.m_endpoint_ise_range, astc_helpers::BISE_64_LEVELS,
						&log_blk.m_endpoints[s * astc_helpers::NUM_MODE8_ENDPOINTS], temp_weights, nullptr, UINT64_MAX, true, nullptr);

					if (subset_err == UINT64_MAX)
					{
						total_err = UINT64_MAX;
						break;
					}

					total_err += subset_err;
					
					if (total_err > best_err)
						break;
				}

				if (total_err < best_err)
				{
					best_err = total_err;
					best_log_blk = log_blk;
				}
								
			} // part_seed

			if (best_err != UINT64_MAX)
			{
				encode_block_output& new_output_blk = *out_blocks.enlarge(1);

				new_output_blk.m_log_blk = best_log_blk;

				uint32_t tm_index;
				if (subsets == 3)
				{
					if (((grid_index & 3) == 0) || ((grid_index & 3) == 2))
						tm_index = s_tm3_index_02[block_size_index];
					else
						tm_index = s_tm3_index_13[block_size_index];
				}
				else
				{
					if (((grid_index & 3) == 0) || ((grid_index & 3) == 2))
						tm_index = s_tm2_index_02[block_size_index];
					else
						tm_index = s_tm2_index_13[block_size_index];
				}

				//fmt_printf("block_size_index: {}, grid_index: {}, tm: {}\n", block_size_index, grid_index & 3, find_tm_index(encoder_trial_modes, best_log_blk));
				assert((int)tm_index == find_tm_index(encoder_trial_modes, best_log_blk));
				
				new_output_blk.m_trial_mode_index = basisu::safe_cast_int16(tm_index);
				new_output_blk.m_sse = best_err;
				new_output_blk.m_blur_id = basisu::safe_cast_uint16(BLUR_ID_AC_LATENT_BASE + grid_index + (subsets - 2) * NUM_WEIGHT_GRIDS);// pat_index;
			}

		} // grid

	} // subsets
		
	return true;
}

static void enforce_max_candidate_limit(ldr_astc_block_encode_image_output::block_info& block_info_out, uint32_t max_candidates)
{
	assert(max_candidates >= 1);

	if (block_info_out.m_out_blocks.size() <= max_candidates)
		return;

	uint_vec sorted_indices(block_info_out.m_out_blocks.size());
	for (uint32_t i = 0; i < block_info_out.m_out_blocks.size(); i++)
		sorted_indices[i] = i;

	std::partial_sort(sorted_indices.begin(), sorted_indices.begin() + max_candidates, sorted_indices.end(),
		[&sorted_indices, &block_info_out](const uint32_t a, const uint32_t b) 
		{ 
			BASISU_NOTE_UNUSED(sorted_indices); // comparator only reads block_info_out
			if (block_info_out.m_out_blocks[a].m_sse < block_info_out.m_out_blocks[b].m_sse)
			{
				return true;
			}
			else if (block_info_out.m_out_blocks[a].m_sse == block_info_out.m_out_blocks[b].m_sse)
			{
				if (a < b)
					return true;
			}

			return false;
		} 
	);	

	basisu::vector<encode_block_output> new_blocks;
	
	int new_packed_out_block_index = -1;

	for (uint32_t i = 0; i < max_candidates; i++)
		if (sorted_indices[i] == block_info_out.m_packed_out_block_index)
			new_packed_out_block_index = i;

	if (new_packed_out_block_index < 0)
	{
		new_packed_out_block_index = 0;

		new_blocks.resize(max_candidates + 1); // one more than requested, not the end of the world
				
		new_blocks[0] = block_info_out.m_out_blocks[block_info_out.m_packed_out_block_index];

		for (uint32_t i = 0; i < max_candidates; i++)
			new_blocks[1 + i] = block_info_out.m_out_blocks[sorted_indices[i]];
	}
	else
	{
		new_blocks.resize(max_candidates);

		for (uint32_t i = 0; i < max_candidates; i++)
			new_blocks[i] = block_info_out.m_out_blocks[sorted_indices[i]];
	}

	block_info_out.m_out_blocks.swap(new_blocks);
	block_info_out.m_packed_out_block_index = new_packed_out_block_index;
}

static void display_candidate_statistics(const ldr_astc_block_encode_image_output &enc_out) 
{
	const auto& block_info = enc_out.m_image_block_info;

	running_stat rs;
	
	for (uint32_t by = 0; by < block_info.get_height(); by++)
	{
		for (uint32_t bx = 0; bx < block_info.get_width(); bx++)
		{
			rs.push((double)block_info(bx, by).m_out_blocks.size());
			
		} // bx
	} // by

	fmt_debug_printf("Encoder output candidate stats: total: {}, min: {}, max: {}, avg: {3.3}\n", rs.get_total(), rs.get_min(), rs.get_max(), rs.get_mean());
}

static bool ldr_astc_block_encode_image(
	const image& orig_img,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	ldr_astc_block_encode_image_output& enc_out,
	uint32_t max_candidate_limit)
{
	if (enc_cfg.m_debug_output)
		fmt_debug_printf("ldr_astc_block_encode_image:\n");

	const uint32_t block_width = enc_cfg.m_block_width, block_height = enc_cfg.m_block_height;
	const uint32_t width = orig_img.get_width(), height = orig_img.get_height();
	const uint32_t total_pixels = width * height;
	const uint32_t total_block_pixels = enc_cfg.m_block_width * enc_cfg.m_block_height;
	const uint32_t num_blocks_x = orig_img.get_block_width(enc_cfg.m_block_width);
	const uint32_t num_blocks_y = orig_img.get_block_height(enc_cfg.m_block_height);
	const uint32_t total_blocks = num_blocks_x * num_blocks_y;

	if (enc_cfg.m_debug_output)
	{
		fmt_debug_printf("\nASTC base bitrate: {3.3} bpp\n", 128.0f / (float)(enc_cfg.m_block_width * enc_cfg.m_block_height));

		fmt_debug_printf("ASTC block size: {}x{}\n", enc_cfg.m_block_width, enc_cfg.m_block_height);

		fmt_debug_printf("Image has alpha: {}\n", orig_img.has_alpha());
		
		fmt_debug_printf("max_candidate_limit: {}\n", max_candidate_limit);;
	}

	// TODO: The transcoder already creates all this stuff for each block size.
	astc_ldr::partitions_data* pPart_data_p2 = &enc_out.m_part_data_p2;
	pPart_data_p2->init(2, enc_cfg.m_block_width, enc_cfg.m_block_height, BASISU_USE_LSH2 == 0, BASISU_USE_LSH2 != 0);

	astc_ldr::partitions_data* pPart_data_p3 = &enc_out.m_part_data_p3;
	pPart_data_p3->init(3, enc_cfg.m_block_width, enc_cfg.m_block_height, BASISU_USE_LSH3 == 0, BASISU_USE_LSH3 != 0);

	// TODO: Make this optional/tune this, add only 2 level blurring support
	// TODO: Make configurable
	// (TOTAL_BLURRED_IMAGES is defined at module scope near the top of this file.)
	
	image orig_img_blurred[TOTAL_BLURRED_IMAGES];
	uint32_t total_blur_encodes_p1 = 0;
	uint32_t total_blurred_blocks_p1[2048] = { };
	uint32_t total_blur_encodes_p2 = 0;
	uint32_t total_blurred_blocks_p2[2048] = { };

	if ((enc_cfg.m_blurring_enabled_p1) || (enc_cfg.m_blurring_enabled_p2))
	{
		for (uint32_t i = 0; i < TOTAL_BLURRED_IMAGES; i++)
			orig_img_blurred[i].resize(orig_img.get_width(), orig_img.get_height());

		const bool srgb_flag = false;
				
		//if (TOTAL_BLURRED_IMAGES > 3)
		if(0)
		{
			for (uint32_t k = 0; k < TOTAL_BLURRED_IMAGES; k++)
			{
				int i = k / 3;
				int j = k % 3;

				const float f = (float)i / (float)basisu::maximum<int>(1, ((TOTAL_BLURRED_IMAGES - 1) / 3));

				float w;
				if (TOTAL_BLURRED_IMAGES <= 6)
					w = lerp(1.1f, 2.0f, f);
				else
					w = lerp(1.01f, 2.2f, f);

				float sx = 1.0f, sy = 1.0f;

				switch (j)
				{
				case 0:
					sx = sy = w; break;
				case 1:
					sx = w; break;
				case 2:
				default:
					sy = w; break;
				}

				image_resample(orig_img, orig_img_blurred[k], srgb_flag, "gaussian", sx, false, 0, 4, sy);
			}
		}
		else
		{
			for (uint32_t i = 0; i < TOTAL_BLURRED_IMAGES; i++)
			{
				const float f = (float)i / (float)(TOTAL_BLURRED_IMAGES - 1);

				float w;
				
				w = lerp(1.1f, 2.0f, f);

				float sx = w, sy = w;

				image_resample(orig_img, orig_img_blurred[i], srgb_flag, "gaussian", sx, false, 0, 4, sy);
			}
		}
	}

	if (enc_cfg.m_debug_images)
	{
		save_png(enc_cfg.m_debug_file_prefix + "dbg_astc_ldr_orig_img.png", orig_img);

		if ((enc_cfg.m_blurring_enabled_p1) || (enc_cfg.m_blurring_enabled_p2))
		{
			for (uint32_t i = 0; i < TOTAL_BLURRED_IMAGES; i++)
				save_png(enc_cfg.m_debug_file_prefix + fmt_string("vis_orig_blurred{}.png", i), orig_img_blurred[i]);
		}
	}

	if (enc_cfg.m_debug_output)
		fmt_debug_printf("Dimensions: {}x{}, Blocks: {}x{}, Total blocks: {}\n", width, height, num_blocks_x, num_blocks_y, total_blocks);

	image orig_img_sobel_x, orig_img_sobel_y;
	compute_sobel(orig_img, orig_img_sobel_x, &g_sobel_x[0][0]);
	compute_sobel(orig_img, orig_img_sobel_y, &g_sobel_y[0][0]);

	if (enc_cfg.m_debug_images)
	{
		save_png(enc_cfg.m_debug_file_prefix + "vis_orig_sobel_x.png", orig_img_sobel_x);
		save_png(enc_cfg.m_debug_file_prefix + "vis_orig_sobel_y.png", orig_img_sobel_y);
	}

	image orig_img_sobel_xy(width, height);
	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			const color_rgba& sx = orig_img_sobel_x(x, y);
			const color_rgba& sy = orig_img_sobel_y(x, y);

			orig_img_sobel_xy(x, y).set(
				iabs((int)sx.r - 128) + iabs((int)sy.r - 128),
				iabs((int)sx.g - 128) + iabs((int)sy.g - 128),
				iabs((int)sx.b - 128) + iabs((int)sy.b - 128),
				iabs((int)sx.a - 128) + iabs((int)sy.a - 128));
		}
	}

	if (enc_cfg.m_debug_images)
		save_png(enc_cfg.m_debug_file_prefix + "vis_orig_sobel_xy.png", orig_img_sobel_xy);

	vector2D<astc_helpers::astc_block>& packed_blocks = enc_out.m_packed_phys_blocks;
	packed_blocks.resize(num_blocks_x, num_blocks_y);
	memset(packed_blocks.get_ptr(), 0, packed_blocks.size_in_bytes());

	assert(enc_cfg.m_pJob_pool);
	job_pool& job_pool = *enc_cfg.m_pJob_pool;

	std::atomic<bool> encoder_failed_flag;
	encoder_failed_flag.store(false);

	std::mutex global_mutex;

	basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes = enc_out.m_encoder_trial_modes;
	encoder_trial_modes.reserve(4096);
		
	basist::astc_ldr_t::grouped_trial_modes& grouped_encoder_trial_modes = enc_out.m_grouped_encoder_trial_modes;
	basist::astc_ldr_t::create_encoder_trial_modes_table(block_width, block_height, encoder_trial_modes, grouped_encoder_trial_modes, enc_cfg.m_debug_output, false);

	if (enc_cfg.m_debug_output)
	{
		uint32_t total_actual_modes = encoder_trial_modes.size_u32();

		if (enc_cfg.m_use_base_ofs)
		{
			for (uint32_t i = 0; i < encoder_trial_modes.size(); i++)
			{
				const auto& tm = encoder_trial_modes[i];

				switch (tm.m_cem)
				{
				case astc_helpers::CEM_LDR_RGBA_DIRECT:
				case astc_helpers::CEM_LDR_RGB_DIRECT:
					// add base+ofs variant
					total_actual_modes++; 
					break;
				default:
					break;
				}
			} // i
		} 

		fmt_debug_printf("Base encoder trial modes: {}, grand total including base+ofs CEM's: {}\n", encoder_trial_modes.size_u32(), total_actual_modes);
	}
	
	uint_vec used_rgb_direct_count;
	used_rgb_direct_count.resize(encoder_trial_modes.size());

	uint_vec used_base_offset_count;
	used_base_offset_count.resize(encoder_trial_modes.size());

	uint32_t total_void_extent_blocks_skipped = 0;

	uint32_t total_superbuckets_created = 0;
	uint32_t total_buckets_created = 0;
	uint32_t total_surrogate_encodes = 0;
	uint32_t total_full_encodes = 0;
	uint32_t total_shortlist_candidates = 0;
	uint32_t total_full_encodes_pass1 = 0;
	uint32_t total_full_encodes_pass2 = 0;
		
	basist::astc_ldr_t::dct2f dct;
	dct.init(enc_cfg.m_block_height, enc_cfg.m_block_width);

	image vis_part_usage_img, vis_part_pat_img, vis_strong_edge, vis_dct_low_freq_block, vis_dp_img, vis_base_ofs_img;
	if (enc_cfg.m_debug_images)
	{
		vis_part_usage_img.resize(block_width * num_blocks_x, block_height * num_blocks_y);
		vis_part_pat_img.resize(block_width * num_blocks_x, block_height * num_blocks_y);
		vis_strong_edge.resize(block_width * num_blocks_x, block_height * num_blocks_y);
		vis_dct_low_freq_block.resize(block_width * num_blocks_x, block_height * num_blocks_y);
		vis_dp_img.resize(block_width * num_blocks_x, block_height * num_blocks_y);
		vis_base_ofs_img.resize(block_width * num_blocks_x, block_height * num_blocks_y);
	}

	ldr_astc_lowlevel_block_encoder_pool encoder_pool;
	assert(job_pool.get_total_threads());
	encoder_pool.init((uint32_t)job_pool.get_total_threads());
		
	basist::astc_ldr_t::grid_weight_dct grid_coder;
	grid_coder.init(block_width, block_height);

	struct output_block_devel_desc
	{
		const basist::astc_ldr_t::trial_mode* m_pTrial_modes;
		int m_trial_mode_index; // this is the index of the mode it tried to encode, but the actual output/enc block could have used base+ofs
		bool m_had_alpha;

		bool m_low_freq_block_flag;
		bool m_super_strong_edges;
		bool m_very_strong_edges;
		bool m_strong_edges;

		void clear()
		{
			clear_obj(*this);
		}
	};

	enc_out.m_image_block_info.resize(0, 0);
	enc_out.m_image_block_info.resize(num_blocks_x, num_blocks_y);

#if 0
	for (uint32_t y = 0; y < num_blocks_y; y++)
	{
		for (uint32_t x = 0; x < num_blocks_x; x++)
		{
			auto& out_blocks = enc_out.m_image_block_info(x, y).m_out_blocks;
			out_blocks.reserve(16);
			out_blocks.resize(0);
		}
	} // y
#endif

	vector2D<bool> superpass2_recompress_block_flags;

	if (enc_cfg.m_second_superpass_refinement)
		superpass2_recompress_block_flags.resize(num_blocks_x, num_blocks_y);

	if (enc_cfg.m_third_superpass_try_neighbors)
		enc_out.m_image_block_info_superpass2.resize(num_blocks_x, num_blocks_y);
		
	interval_timer itm;
	itm.start();

	//--------------------------------------------------------------------------------------
	// ASTC compression loop

	vector2D<output_block_devel_desc> output_block_devel_info(num_blocks_x, num_blocks_y);

	uint32_t total_superpasses = 1;
	if (enc_cfg.m_third_superpass_try_neighbors)
		total_superpasses = 3;
	else if (enc_cfg.m_second_superpass_refinement)
		total_superpasses = 2;
		
	uint32_t total_blocks_to_recompress = 0;
		
	//uint32_t max_candidates = 64;
	//if (total_blocks >= (512 * 512))
	//	max_candidates = 16; // save memory
		
	for (uint32_t superpass_index = 0; superpass_index < total_superpasses; superpass_index++)
	{
		if (superpass_index == 1)
		{
			if (!enc_cfg.m_second_superpass_refinement)
				continue;
			if (!total_blocks_to_recompress)
				continue;
		}

		if (enc_cfg.m_debug_output)
			fmt_debug_printf("ASTC packing superpass: {}\n", 1 + superpass_index);

		uint32_t total_blocks_done = 0;
		float last_printed_progress_val = -100.0f;

		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				job_pool.add_job([superpass_index,
					//width, height, 
					bx, by, 
					//num_blocks_x, num_blocks_y, 
					total_blocks, block_width, block_height, total_block_pixels, &packed_blocks, &global_mutex,
					&orig_img, &orig_img_sobel_xy, &orig_img_blurred, max_candidate_limit,
					&enc_cfg, &encoder_failed_flag, pPart_data_p2, pPart_data_p3, 
					&total_blocks_done, &total_superbuckets_created, &total_buckets_created, &total_surrogate_encodes, &total_full_encodes, &total_shortlist_candidates,
					&encoder_trial_modes,
					&total_blur_encodes_p1, &total_blurred_blocks_p1, &total_blur_encodes_p2, &total_blurred_blocks_p2,
					&total_full_encodes_pass1, &total_full_encodes_pass2,
					&dct, &vis_dct_low_freq_block, 
					&encoder_pool, &grid_coder, &grouped_encoder_trial_modes,
					&enc_out, &output_block_devel_info, &total_void_extent_blocks_skipped, &superpass2_recompress_block_flags, &total_blocks_to_recompress, &last_printed_progress_val]
					{
						//if ((bx == 0x35) && (by == 0xec))
						//	printf(".");

						if (encoder_failed_flag)
							return;

						//const uint32_t base_x = bx * block_width, base_y = by * block_height;

						color_rgba block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
						orig_img.extract_block_clamped(block_pixels, bx * block_width, by * block_height, block_width, block_height);

						if (superpass_index == 2)
						{
							// Superpass 2: Encode to best neighbor configurations
							const ldr_astc_block_encode_image_output::block_info& out_block_info = enc_out.m_image_block_info(bx, by);

							ldr_astc_block_encode_image_output::block_info_superpass1& out_block_info_superpass1 = enc_out.m_image_block_info_superpass2(bx, by);

							const astc_ldr::pixel_stats_t& pixel_stats = out_block_info.m_pixel_stats;

							const bool is_purely_solid_block = (pixel_stats.m_min == pixel_stats.m_max);

							// if void extent, just skip
							if (is_purely_solid_block)
								return;

							//const basisu::vector<encode_block_output>& out_blocks = out_block_info.m_out_blocks;

							for (uint32_t neighbor_index = 0; neighbor_index < basist::astc_ldr_t::cMaxConfigReuseNeighbors; neighbor_index++)
							{
								const ldr_astc_block_encode_image_output::block_info* pNeighbor_out_block_info = nullptr;

								if (neighbor_index == 0)
								{
									// Left
									if (bx)
										pNeighbor_out_block_info = &enc_out.m_image_block_info(bx - 1, by);
								}
								else if (neighbor_index == 1)
								{
									// Up
									if (by)
										pNeighbor_out_block_info = &enc_out.m_image_block_info(bx, by - 1);
								}
								else
								{
									assert(neighbor_index == 2);

									// Diagonal
									if ((bx) && (by))
										pNeighbor_out_block_info = &enc_out.m_image_block_info(bx - 1, by - 1);
								}

								if (!pNeighbor_out_block_info)
									continue;

								const encode_block_output& neighbor_output = pNeighbor_out_block_info->m_out_blocks[pNeighbor_out_block_info->m_packed_out_block_index];

								// Best neighbor was solid, skip it (TODO: reusing it is possible)
								if (neighbor_output.m_log_blk.m_solid_color_flag_ldr)
									continue;

								const uint32_t neighbor_tm_index = neighbor_output.m_trial_mode_index;
								assert(neighbor_tm_index < encoder_trial_modes.size());

								//const trial_mode& neighbor_tm = encoder_trial_modes[neighbor_tm_index]; // do not use the tm's cem, it may be base+ofs, use the log blk instead

								const astc_helpers::log_astc_block& neighbor_log_blk = neighbor_output.m_log_blk;
								assert(!neighbor_log_blk.m_solid_color_flag_ldr);

								const uint32_t neighbor_actual_cem = neighbor_log_blk.m_color_endpoint_modes[0];
								const uint32_t neighbor_partition_id = neighbor_log_blk.m_partition_id;

								// See if we've already encoded this full config
								int already_existing_out_block_index = cInvalidIndex;
								for (uint32_t i = 0; i < out_block_info.m_out_blocks.size(); i++)
								{
									if ((out_block_info.m_out_blocks[i].m_trial_mode_index == (int)neighbor_tm_index) &&
										(out_block_info.m_out_blocks[i].m_log_blk.m_color_endpoint_modes[0] == neighbor_actual_cem) &&
										(out_block_info.m_out_blocks[i].m_log_blk.m_partition_id == neighbor_partition_id))
									{
										already_existing_out_block_index = i;
										break;
									}
								}

								if (already_existing_out_block_index != cInvalidIndex)
								{
									// We already have an output block using this neighbor trial mode, skip
									out_block_info_superpass1.m_config_reuse_neighbor_out_block_indices[neighbor_index] = (uint32_t)already_existing_out_block_index;
									out_block_info_superpass1.m_config_reuse_new_neighbor_out_block_flags[neighbor_index] = false;
								}
								else
								{
									// Re-encode using the neighbor's full config (tm, base+ofs, partition ID)
									astc_helpers::log_astc_block new_log_block;

									bool status = false;

									if (neighbor_log_blk.m_num_partitions > 1)
									{
										const astc_ldr::partitions_data* pPart_data = (neighbor_log_blk.m_num_partitions == 2) ? pPart_data_p2 : pPart_data_p3;

										const uint32_t part_seed_index = neighbor_log_blk.m_partition_id;
										const uint32_t part_unique_index = pPart_data->m_part_seed_to_unique_index[part_seed_index];

										assert(part_unique_index < astc_helpers::NUM_PARTITION_PATTERNS);
										const astc_ldr::partition_pattern_vec* pPat = &pPart_data->m_partition_pats[part_unique_index];

										bool refine_only_flag = false;

										status = encode_trial_subsets(
											block_width, block_height,
											pixel_stats,
											neighbor_log_blk.m_color_endpoint_modes[0], neighbor_log_blk.m_num_partitions, neighbor_log_blk.m_partition_id, pPat,
											neighbor_log_blk.m_endpoint_ise_range, neighbor_log_blk.m_weight_ise_range,
											neighbor_log_blk.m_grid_width, neighbor_log_blk.m_grid_height,
											enc_cfg.m_encode_trial_subsets_early_out_thresh,
											new_log_block,
											enc_cfg.m_cem_enc_params,
											refine_only_flag,
											enc_cfg.m_gradient_descent_flag, enc_cfg.m_polish_weights_flag, enc_cfg.m_qcd_enabled_flag,
											enc_cfg.m_use_blue_contraction);
									}
									else
									{
										status = encode_trial(
											block_width, block_height,
											pixel_stats,
											neighbor_log_blk.m_color_endpoint_modes[0],
											neighbor_log_blk.m_dual_plane, neighbor_log_blk.m_dual_plane ? neighbor_log_blk.m_color_component_selector : -1,
											neighbor_log_blk.m_endpoint_ise_range, neighbor_log_blk.m_weight_ise_range,
											neighbor_log_blk.m_grid_width, neighbor_log_blk.m_grid_height,
											enc_cfg.m_encode_trial_early_out_thresh,
											new_log_block,
											enc_cfg.m_cem_enc_params,
											enc_cfg.m_gradient_descent_flag, enc_cfg.m_polish_weights_flag, enc_cfg.m_qcd_enabled_flag,
											enc_cfg.m_use_blue_contraction);
									}

									if (!status)
									{
										fmt_debug_printf("encode_trial/encode_trial_subsets failed in superpass 1!\n");
										encoder_failed_flag.store(true);
										return;
									}

									out_block_info_superpass1.m_config_reuse_neighbor_out_block_indices[neighbor_index] = out_block_info_superpass1.m_new_out_config_reuse_blocks.size_u32();
									out_block_info_superpass1.m_config_reuse_new_neighbor_out_block_flags[neighbor_index] = true;

									encode_block_output& new_output_blk = *out_block_info_superpass1.m_new_out_config_reuse_blocks.enlarge(1);

									new_output_blk.clear();

									if (enc_cfg.m_use_dct)
									{
										//const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, new_log_block.m_grid_width, new_log_block.m_grid_height);

										const uint32_t num_planes = (new_log_block.m_dual_plane ? 2 : 1);

										basist::astc_ldr_t::fvec dct_temp;

										for (uint32_t plane_index = 0; plane_index < num_planes; plane_index++)
										{
											basist::astc_ldr_t::dct_syms &syms = new_output_blk.m_packed_dct_plane_data[plane_index];
																						
											code_block_weights(grid_coder, enc_cfg.m_base_q, plane_index, new_log_block, syms, dct_temp);
																						
											// ensure existing weights get blown away 
											for (uint32_t i = 0; i < (uint32_t)(new_log_block.m_grid_width * new_log_block.m_grid_height); i++)
												new_log_block.m_weights[i * num_planes + plane_index] = 0;
																						
											bool dec_status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, new_log_block, nullptr, nullptr, dct_temp, &syms);
																						
											assert(dec_status);
											if (!dec_status)
											{
												error_printf("grid_coder.decode_block_weights() failed!\n");

												encoder_failed_flag.store(true);
												return;
											}
										}
									} // if (enc_cfg.m_use_dct)

									new_output_blk.m_trial_mode_index = safe_cast_int16(neighbor_tm_index);
									new_output_blk.m_log_blk = new_log_block;
									//new_output_blk.m_trial_surrogate.clear();

									new_output_blk.m_sse = eval_error(block_width, block_height, new_log_block, pixel_stats, enc_cfg.m_cem_enc_params);

									{
										std::lock_guard g(global_mutex);

										total_full_encodes_pass2++;
									}
								}  // if (already_existing_out_block_index != cInvalidIndex) 

								{
									// Re-encode using the neighbor's full config (tm, base+ofs, partition ID) AND its endpoints
									astc_helpers::log_astc_block new_log_block(neighbor_log_blk);

									// Start with fresh 0 weights, then polish them.
									clear_obj(new_log_block.m_weights);

									//const bool use_blue_contraction = enc_cfg.m_use_blue_contraction;

									bool improved_flag = false;

									const astc_ldr::partition_pattern_vec* pPat = nullptr;
									if (neighbor_log_blk.m_num_partitions > 1)
									{
										const astc_ldr::partitions_data* pPart_data = (neighbor_log_blk.m_num_partitions == 2) ? pPart_data_p2 : pPart_data_p3;

										const uint32_t part_seed_index = neighbor_log_blk.m_partition_id;
										const uint32_t part_unique_index = pPart_data->m_part_seed_to_unique_index[part_seed_index];

										assert(part_unique_index < astc_helpers::NUM_PARTITION_PATTERNS);
										pPat = &pPart_data->m_partition_pats[part_unique_index];
									}

									bool status = polish_block_weights(
										block_width, block_height,
										pixel_stats,
										new_log_block,
										enc_cfg.m_cem_enc_params, pPat, improved_flag,
										enc_cfg.m_gradient_descent_flag, enc_cfg.m_polish_weights_flag, enc_cfg.m_qcd_enabled_flag);

									if (!status)
									{
										fmt_error_printf("polish_block_weights failed in superpass 1!\n");
										encoder_failed_flag.store(true);
										return;
									}

									encode_block_output& new_output_blk = *out_block_info_superpass1.m_new_out_config_endpoint_reuse_blocks.enlarge(1);

									new_output_blk.clear();

									if (enc_cfg.m_use_dct)
									{
										//const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, new_log_block.m_grid_width, new_log_block.m_grid_height);

										const uint32_t num_planes = (new_log_block.m_dual_plane ? 2 : 1);

										basist::astc_ldr_t::fvec dct_temp;

										for (uint32_t plane_index = 0; plane_index < num_planes; plane_index++)
										{
											basist::astc_ldr_t::dct_syms &syms = new_output_blk.m_packed_dct_plane_data[plane_index];
																						
											code_block_weights(grid_coder, enc_cfg.m_base_q, plane_index, new_log_block, syms, dct_temp);

											// ensure existing weights get blown away
											for (uint32_t i = 0; i < (uint32_t)(new_log_block.m_grid_width * new_log_block.m_grid_height); i++)
												new_log_block.m_weights[i * num_planes + plane_index] = 0;
																																
											bool dec_status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, new_log_block, nullptr, nullptr, dct_temp, &syms);

											assert(dec_status);
											if (!dec_status)
											{
												error_printf("grid_coder.decode_block_weights() failed!\n");

												encoder_failed_flag.store(true);
												return;
											}
										}
									} // if (enc_cfg.m_use_dct)

									new_output_blk.m_trial_mode_index = safe_cast_int16(neighbor_tm_index);
									new_output_blk.m_log_blk = new_log_block;
									//new_output_blk.m_trial_surrogate.clear();

									new_output_blk.m_sse = eval_error(block_width, block_height, new_log_block, pixel_stats, enc_cfg.m_cem_enc_params);

									{
										std::lock_guard g(global_mutex);

										total_full_encodes_pass2++;
									}
								}

							} // neighbor_index
						}
						else
						{
							if (superpass_index == 1)
							{
								if (!superpass2_recompress_block_flags(bx, by))
									return;
							}

							// Superpass 0/2: core ASTC encoding
							basisu::vector<encode_block_output>& out_blocks = enc_out.m_image_block_info(bx, by).m_out_blocks;
							out_blocks.resize(0);

							astc_ldr::pixel_stats_t& pixel_stats = enc_out.m_image_block_info(bx, by).m_pixel_stats;

							if (superpass_index == 0)
								pixel_stats.init(total_block_pixels, block_pixels);

							const bool is_purely_solid_block = (pixel_stats.m_min == pixel_stats.m_max);

							// early out on totally solid blocks
							if (is_purely_solid_block)
							{
								encode_block_output* pOut = out_blocks.enlarge(1);
								pOut->clear();

								astc_helpers::log_astc_block& log_blk = pOut->m_log_blk;

								log_blk.clear();
								log_blk.m_solid_color_flag_ldr = true;

								for (uint32_t c = 0; c < 4; c++)
									log_blk.m_solid_color[c] = pixel_stats.m_min[c];

								// Expand each component to 16-bits
								for (uint32_t c = 0; c < 4; c++)
									log_blk.m_solid_color[c] |= (uint16_t)(log_blk.m_solid_color[c]) << 8u;

								pOut->m_sse = eval_error(block_width, block_height, log_blk, pixel_stats, enc_cfg.m_cem_enc_params);

								ldr_astc_block_encode_image_output::block_info& block_info_out = enc_out.m_image_block_info(bx, by);

								block_info_out.m_low_freq_block_flag = true;
								block_info_out.m_super_strong_edges = false;
								block_info_out.m_very_strong_edges = false;
								block_info_out.m_strong_edges = false;
								block_info_out.m_packed_out_block_index = 0;

								// Create packed ASTC block
								astc_helpers::astc_block& best_phys_block = packed_blocks(bx, by);
								bool pack_success = astc_helpers::pack_astc_block(best_phys_block, log_blk);
								if (!pack_success)
								{
									encoder_failed_flag.store(true);
									return;
								}

								output_block_devel_desc& out_devel_desc = output_block_devel_info(bx, by);
								out_devel_desc.m_low_freq_block_flag = true;
								out_devel_desc.m_super_strong_edges = false;
								out_devel_desc.m_very_strong_edges = false;
								out_devel_desc.m_strong_edges = false;

								{
									std::lock_guard g(global_mutex);

									total_void_extent_blocks_skipped++;

									total_blocks_done++;
								}

								return;
							}
							
							// Configure low-level block encoder.
							ldr_astc_lowlevel_block_encoder_params enc_blk_params;

							encoder_config_manager cfg_man;
							cfg_man.init(bx, by, block_width, block_height, total_block_pixels,
								pixel_stats,
								dct, enc_cfg, orig_img_sobel_xy, vis_dct_low_freq_block, 
								0);
														
							cfg_man.select(enc_blk_params, superpass_index, 
								bx, by, block_width, block_height, total_block_pixels,
								orig_img_sobel_xy, encoder_trial_modes, grouped_encoder_trial_modes,
								pPart_data_p2, pPart_data_p3,
								pixel_stats, enc_cfg,
								dct,
								0);

							scoped_ldr_astc_lowlevel_block_encoder scoped_block_encoder(encoder_pool);
							if (scoped_block_encoder.get_ptr() == nullptr)
							{
								error_printf("Failed allocating thread local encode block temps\n");
								encoder_failed_flag.store(true);
								return;
							}

							// solid color - must be first
							{
								encode_block_output* pOut = out_blocks.enlarge(1);
								pOut->clear();

								astc_helpers::log_astc_block& log_blk = pOut->m_log_blk;

								log_blk.clear();
								log_blk.m_solid_color_flag_ldr = true;

								for (uint32_t c = 0; c < 4; c++)
									log_blk.m_solid_color[c] = (uint16_t)clamp((int)std::round(pixel_stats.m_mean_f[c] * 255.0f), 0, 255);

								// Expand each component to 16-bits
								for (uint32_t c = 0; c < 4; c++)
									log_blk.m_solid_color[c] |= (uint16_t)(log_blk.m_solid_color[c]) << 8u;

								pOut->m_sse = eval_error(block_width, block_height, log_blk, pixel_stats, enc_cfg.m_cem_enc_params);
							}

							encode_block_stats enc_block_stats;
							bool enc_status;
														
							{
								enc_status = scoped_block_encoder.get_ptr()->full_encode(enc_blk_params, pixel_stats, out_blocks, 0, enc_block_stats);
								if (!enc_status)
								{
									encoder_failed_flag.store(true);
									return;
								}
							}

#if 0
							if (enc_cfg.m_debug_images)
							{
								const bool vis_flag = (scoped_block_encoder.get_ptr()->m_block_complexity_index == 0) && (max_std_dev < (6.0f / 255.0f));
								vis_dct_low_freq_block.fill_box(bx * block_width, by * block_height, block_width, block_height, vis_flag ? color_rgba(255, 0, 255, 255) : g_black_color);
							}
#endif

							// --------------------- BLOCK BLURRING
							//const float BLUR_STD_DEV_THRESH = (15.0f / 255.0f);
							//const float BLUR_SOBEL_ENERGY_THRESH = 15000.0f;

							//const float BLUR_STD_DEV_THRESH = (5.0f / 255.0f);
							//const float BLUR_SOBEL_ENERGY_THRESH = 5000.0f;
														
							const float BLUR_STD_DEV_THRESH = (10.0f / 255.0f);
							const float BLUR_SOBEL_ENERGY_THRESH = 10000.0f;
							const bool use_blurs = (enc_cfg.m_blurring_enabled_p1 && (!selective_blurring || ((cfg_man.m_max_std_dev > BLUR_STD_DEV_THRESH) && (cfg_man.m_sobel_energy > BLUR_SOBEL_ENERGY_THRESH)))) ||
								(enc_cfg.m_blurring_enabled_p2 && (superpass_index == 1));
							if (use_blurs)
							{
								for (uint32_t i = 0; i < TOTAL_BLURRED_IMAGES; i++)
								{
									const uint32_t blur_id = BLUR_ID_GAUSSIAN_ALTERNATE + i;
									const image& blurred_img = orig_img_blurred[i];
									assert(blurred_img.get_width());

									color_rgba block_pixels_blurred[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
									blurred_img.extract_block_clamped(block_pixels_blurred, bx * block_width, by * block_height, block_width, block_height);

									astc_ldr::pixel_stats_t pixel_stats_blurred;
									pixel_stats_blurred.init(total_block_pixels, block_pixels_blurred);

									cfg_man.init(bx, by, block_width, block_height, total_block_pixels,
										pixel_stats_blurred,
										dct, enc_cfg, orig_img_sobel_xy, vis_dct_low_freq_block, blur_id);

									ldr_astc_lowlevel_block_encoder_params enc_blk_params_blurred;

									cfg_man.select(enc_blk_params_blurred, 
										superpass_index,
										bx, by, block_width, block_height, total_block_pixels,
										orig_img_sobel_xy, encoder_trial_modes, grouped_encoder_trial_modes,
										pPart_data_p2, pPart_data_p3,
										pixel_stats_blurred, enc_cfg,
										dct, 
										blur_id);

									enc_status = scoped_block_encoder.get_ptr()->full_encode(enc_blk_params_blurred, pixel_stats_blurred, out_blocks, blur_id, enc_block_stats);
									if (!enc_status)
									{
										encoder_failed_flag.store(true);
										return;
									}
								}
							}

							// Grid dimension prefiltering (experimental)
							const bool BLOCK_PREFILTER_BY_GRID_DIM = false;
							if (BLOCK_PREFILTER_BY_GRID_DIM && (enc_cfg.m_blurring_enabled_p1 || enc_cfg.m_blurring_enabled_p2))
							{
								uint_vec grid_dims;

								for (uint32_t out_block_iter = 0; out_block_iter < out_blocks.size_u32(); out_block_iter++)
								{
									if (out_blocks[out_block_iter].m_trial_mode_index < 0)
										continue;

									astc_helpers::log_astc_block& log_astc_blk = out_blocks[out_block_iter].m_log_blk;
									if (log_astc_blk.m_solid_color_flag_ldr)
										continue;

									if ((log_astc_blk.m_grid_width == block_width) && (log_astc_blk.m_grid_height == block_height))
										continue;

									grid_dims.push_back(
										((uint32_t)log_astc_blk.m_grid_width << 8) | ((uint32_t)log_astc_blk.m_grid_height));
								}

								if (grid_dims.size())
								{
									grid_dims.unique();
									const uint32_t total_unique = grid_dims.size_u32();

									for (uint32_t b = 0; b < total_unique; b++)
									{
										const uint32_t grid_width = grid_dims[b] >> 8;
										const uint32_t grid_height = grid_dims[b] & 0xFF;
										
										color_rgba upsampled_block[astc_helpers::MAX_BLOCK_PIXELS]; // num_block_samples

										filter_block(block_width, block_height, grid_width, grid_height, pixel_stats, upsampled_block);

										const uint32_t blur_id = BLUR_ID_GRID_DIM_BASE + b;

										astc_ldr::pixel_stats_t pixel_stats_blurred;
										pixel_stats_blurred.init(total_block_pixels, upsampled_block);

										cfg_man.init(bx, by, block_width, block_height, total_block_pixels,
											pixel_stats_blurred,
											dct, enc_cfg, orig_img_sobel_xy, vis_dct_low_freq_block, blur_id);

										ldr_astc_lowlevel_block_encoder_params enc_blk_params_blurred;

										cfg_man.select(enc_blk_params_blurred,
											superpass_index,
											bx, by, block_width, block_height, total_block_pixels,
											orig_img_sobel_xy, encoder_trial_modes, grouped_encoder_trial_modes,
											pPart_data_p2, pPart_data_p3,
											pixel_stats_blurred, enc_cfg,
											dct,
											blur_id);

										enc_status = scoped_block_encoder.get_ptr()->full_encode(enc_blk_params_blurred, pixel_stats_blurred, out_blocks, blur_id, enc_block_stats);
										if (!enc_status)
										{
											encoder_failed_flag.store(true);
											return;
										}

									} // b
								}
							}

							// --------------------- EXPERIMENTAL - DC/LINEAR LATENT CODING
							if ((enc_cfg.m_try_simplified_latent_configs) &&
								(!pixel_stats.m_has_alpha) && 
								(cfg_man.m_max_std_dev > (2.0f / 255.0f)))
							{
								uint64_vec best_2subset_seed_ids, best_3subset_seed_ids;

								encode_block_to_dc_latent(
									block_width, block_height,
									pixel_stats,
									out_blocks,
									enc_cfg,
									pPart_data_p2, pPart_data_p3, encoder_trial_modes, 
									best_2subset_seed_ids, best_3subset_seed_ids);

								encode_block_to_linear_latent(8,
									block_width, block_height,
									pixel_stats,
									out_blocks,
									enc_cfg,
									pPart_data_p2, pPart_data_p3, encoder_trial_modes, 
									best_2subset_seed_ids, best_3subset_seed_ids);
							}

							// --------------------- WEIGHT GRID DCT CODING 
							if (enc_cfg.m_use_dct)
							{
								// apply DCT to weights
								for (uint32_t out_block_iter = 0; out_block_iter < out_blocks.size_u32(); out_block_iter++)
								{
									if (!apply_weight_grid_dct(block_width, block_height,
										grid_coder,
										out_blocks[out_block_iter],
										pixel_stats, true,
										enc_cfg, pPart_data_p2, pPart_data_p3, true))
									{
										encoder_failed_flag.store(true);
										return;
									}
								} // for

							} // use_dct
														
							// Find best output block
							uint64_t best_out_blocks_err = UINT64_MAX;
							uint32_t best_out_blocks_index = 0;
							astc_helpers::log_astc_block best_out_blocks_log_astc_blk;

							for (uint32_t out_block_iter = 0; out_block_iter < out_blocks.size_u32(); out_block_iter++)
							{
								const astc_helpers::log_astc_block& log_astc_blk = out_blocks[out_block_iter].m_log_blk;

								color_rgba dec_pixels[astc_helpers::MAX_BLOCK_PIXELS];
								const bool dec_status = astc_helpers::decode_block_xuastc_ldr(log_astc_blk, dec_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);

								assert(dec_status);
								if (!dec_status)
								{
									encoder_failed_flag.store(true);
									return;
								}

								uint64_t total_err = 0;
								for (uint32_t i = 0; i < total_block_pixels; i++)
									total_err += weighted_color_error(block_pixels[i], dec_pixels[i], enc_cfg.m_cem_enc_params);

								// if not blurred 
								if (out_blocks[out_block_iter].m_blur_id == 0)
								{
									if (out_blocks[out_block_iter].m_sse != total_err)
									{
										assert(0);
										fmt_error_printf("output block SSE invalid\n");
										encoder_failed_flag.store(true);
										return;
									}
								}

								// Replace m_sse with the actual WSSE vs. the original source block (in case it was blurred)
								out_blocks[out_block_iter].m_sse = total_err;

								if (total_err < best_out_blocks_err)
								{
									best_out_blocks_err = total_err;
									best_out_blocks_log_astc_blk = log_astc_blk;
									best_out_blocks_index = out_block_iter;
								}
							} // out_block_iter

							// try blurring best candidate found so far (very experimental)
							const bool BLOCK_POSTFILTERING = false;
							if (BLOCK_POSTFILTERING &&
								( ((enc_cfg.m_blurring_enabled_p1) || (enc_cfg.m_blurring_enabled_p2)) && (cfg_man.m_max_std_dev > (5.0f / 255.0f)) ) 
								)
							{
								encode_block_output &best_blk = out_blocks[best_out_blocks_index];
								
								const astc_helpers::log_astc_block& best_log_blk = best_blk.m_log_blk;

								if ((!best_log_blk.m_solid_color_flag_ldr) && 
									((best_log_blk.m_grid_width < block_width) || (best_log_blk.m_grid_height < block_height)))
								{
									color_rgba upsampled_block[astc_helpers::MAX_BLOCK_PIXELS]; // num_block_samples

									filter_block(block_width, block_height, best_log_blk.m_grid_width, best_log_blk.m_grid_height, pixel_stats, upsampled_block);

									const uint32_t blur_id = BLUR_ID_BEST_CANDIDATE;

									astc_ldr::pixel_stats_t pixel_stats_blurred;
									pixel_stats_blurred.init(total_block_pixels, upsampled_block);

									cfg_man.init(bx, by, block_width, block_height, total_block_pixels,
										pixel_stats_blurred,
										dct, enc_cfg, orig_img_sobel_xy, vis_dct_low_freq_block, blur_id);

									ldr_astc_lowlevel_block_encoder_params enc_blk_params_blurred;

									cfg_man.select(enc_blk_params_blurred,
										superpass_index,
										bx, by, block_width, block_height, total_block_pixels,
										orig_img_sobel_xy, encoder_trial_modes, grouped_encoder_trial_modes,
										pPart_data_p2, pPart_data_p3,
										pixel_stats_blurred, enc_cfg,
										dct,
										blur_id);

									const uint32_t cur_num_out_blocks = out_blocks.size_u32();

									enc_status = scoped_block_encoder.get_ptr()->full_encode(enc_blk_params_blurred, pixel_stats_blurred, out_blocks, blur_id, enc_block_stats);
									if (!enc_status)
									{
										encoder_failed_flag.store(true);
										return;
									}

									for (uint32_t out_block_iter = cur_num_out_blocks; out_block_iter < out_blocks.size_u32(); out_block_iter++)
									{
										if (!apply_weight_grid_dct(block_width, block_height,
											grid_coder,
											out_blocks[out_block_iter],
											pixel_stats, true,
											enc_cfg, pPart_data_p2, pPart_data_p3, true))
										{
											encoder_failed_flag.store(true);
											return;
										}
									}

									for (uint32_t out_block_iter = cur_num_out_blocks; out_block_iter < out_blocks.size_u32(); out_block_iter++)
									{
										const astc_helpers::log_astc_block& log_astc_blk = out_blocks[out_block_iter].m_log_blk;

										color_rgba dec_pixels[astc_helpers::MAX_BLOCK_PIXELS];
										bool dec_status = astc_helpers::decode_block(log_astc_blk, dec_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);

										assert(dec_status);
										if (!dec_status)
										{
											encoder_failed_flag.store(true);
											return;
										}

										uint64_t total_err = 0;
										for (uint32_t i = 0; i < total_block_pixels; i++)
											total_err += weighted_color_error(block_pixels[i], dec_pixels[i], enc_cfg.m_cem_enc_params);

										assert(out_blocks[out_block_iter].m_blur_id != 0);

										// Replace m_sse with the actual WSSE vs. the original source block (in case it was blurred)
										out_blocks[out_block_iter].m_sse = total_err;

										if (total_err < best_out_blocks_err)
										{
											best_out_blocks_err = total_err;
											best_out_blocks_log_astc_blk = log_astc_blk;
											best_out_blocks_index = out_block_iter;
										}
									} // out_block_iter

								}
							}

#if 0
							// TODO: Save memory, only minimally tested
							if (enc_cfg.m_save_single_result)
							{
								basisu::vector<encode_block_output> new_out_blocks(1);
								new_out_blocks[0] = out_blocks[best_out_blocks_index];

								std::swap(out_blocks, new_out_blocks);

								best_out_blocks_index = 0;
							}
#endif
							// TODO: limit max candidates to save memory here
														
							ldr_astc_block_encode_image_output::block_info& block_info_out = enc_out.m_image_block_info(bx, by);

							block_info_out.m_low_freq_block_flag = cfg_man.m_low_freq_block_flag;
							block_info_out.m_super_strong_edges = scoped_block_encoder.get_ptr()->m_super_strong_edges;
							block_info_out.m_very_strong_edges = scoped_block_encoder.get_ptr()->m_very_strong_edges;
							block_info_out.m_strong_edges = scoped_block_encoder.get_ptr()->m_strong_edges;

							block_info_out.m_packed_out_block_index = best_out_blocks_index;

							const uint64_t check_wsse = out_blocks[best_out_blocks_index].m_sse;

							enforce_max_candidate_limit(block_info_out, max_candidate_limit);

							best_out_blocks_index = block_info_out.m_packed_out_block_index;  // may have changed

							BASISU_NOTE_UNUSED(check_wsse);
							assert(check_wsse == out_blocks[best_out_blocks_index].m_sse);

							// Create packed ASTC block
							astc_helpers::astc_block& best_phys_block = packed_blocks(bx, by);
							bool pack_success = astc_helpers::pack_astc_block(best_phys_block, best_out_blocks_log_astc_blk);
							if (!pack_success)
							{
								encoder_failed_flag.store(true);
								return;
							}

							output_block_devel_desc& out_devel_desc = output_block_devel_info(bx, by);
							out_devel_desc.m_low_freq_block_flag = cfg_man.m_low_freq_block_flag;
							out_devel_desc.m_super_strong_edges = scoped_block_encoder.get_ptr()->m_super_strong_edges;
							out_devel_desc.m_very_strong_edges = scoped_block_encoder.get_ptr()->m_very_strong_edges;
							out_devel_desc.m_strong_edges = scoped_block_encoder.get_ptr()->m_strong_edges;

							// Critical Section
							{
								std::lock_guard g(global_mutex);
																
								if (superpass_index == 0)
								{
									if (use_blurs)
										total_blur_encodes_p1++;

									if (out_blocks[best_out_blocks_index].m_blur_id >= 1)
									{
										const int blur_id = out_blocks[best_out_blocks_index].m_blur_id - 1;
										if (blur_id < (int)std::size(total_blurred_blocks_p1))
											total_blurred_blocks_p1[blur_id]++;
									}
								}
								else if (superpass_index == 1)
								{
									if (use_blurs)
										total_blur_encodes_p2++;

									if (out_blocks[best_out_blocks_index].m_blur_id >= 1)
									{
										const int blur_id = out_blocks[best_out_blocks_index].m_blur_id - 1;
										if (blur_id < (int)std::size(total_blurred_blocks_p2))
											total_blurred_blocks_p2[blur_id]++;
									}
								}

								if (superpass_index == 0)
								{
									// TODO: Add 2nd pass statistics
									total_superbuckets_created += enc_block_stats.m_total_superbuckets_created;
									total_buckets_created += enc_block_stats.m_total_buckets_created;
									total_surrogate_encodes += enc_block_stats.m_total_surrogate_encodes;
									total_full_encodes += enc_block_stats.m_total_full_encodes;
									total_shortlist_candidates += enc_block_stats.m_total_shortlist_candidates;
								}
								else if (superpass_index == 1)
								{
									total_full_encodes_pass1 += enc_block_stats.m_total_full_encodes;
								}

								total_blocks_done++;
								if (enc_cfg.m_debug_output)
								{
									if (superpass_index == 1)
									{
										if ((total_blocks_done & 63) == 63)
										{
											float new_val = ((float)total_blocks_done * 100.0f) / (float)total_blocks_to_recompress;
											if ((new_val - last_printed_progress_val) >= 5.0f)
											{
												last_printed_progress_val = new_val;
												fmt_printf("{3.2}%\n", new_val);
											}
										}
									}
									else if ((total_blocks_done & 255) == 255)
									{
										float new_val = ((float)total_blocks_done * 100.0f) / (float)total_blocks;
										if ((new_val - last_printed_progress_val) >= 5.0f)
										{
											last_printed_progress_val = new_val;
											fmt_printf("{3.2}%\n", new_val);
										}
									}
								}

							} // lock_guard (global_mutex)

						} // if (superpass_index == ...)

					});

				if (encoder_failed_flag)
					break;

			} // bx 

			if (encoder_failed_flag)
				break;

		} // by

		if (encoder_failed_flag)
		{
			fmt_error_printf("Main compressor block loop failed!\n");
			return false;
		}

		job_pool.wait_for_all();

		if (encoder_failed_flag)
		{
			fmt_error_printf("Main compressor block loop failed!\n");
			return false;
		}

		if ((superpass_index == 0) && (enc_cfg.m_second_superpass_refinement) && (enc_cfg.m_second_superpass_fract_to_recompress > 0.0f))
		{
			uint_vec block_wsse_indices(total_blocks);

			float_vec block_wsses(total_blocks);
			for (uint32_t by = 0; by < num_blocks_y; by++)
			{
				for (uint32_t bx = 0; bx < num_blocks_x; bx++)
				{
					ldr_astc_block_encode_image_output::block_info& out_block_info = enc_out.m_image_block_info(bx, by);

					float wsse = (float)out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_sse;

					block_wsses[bx + by * num_blocks_x] = wsse;
				} // bx
			} // by

			indirect_sort(total_blocks, block_wsse_indices.data(), block_wsses.data());

			if (block_wsses[block_wsse_indices[total_blocks - 1]] > 0.0f)
			{
				total_blocks_to_recompress = clamp<uint32_t>((uint32_t)std::round((float)total_blocks * enc_cfg.m_second_superpass_fract_to_recompress), 0, total_blocks);

				image vis_recomp_img;
				if (enc_cfg.m_debug_images)
					vis_recomp_img.resize(width, height);

				for (uint32_t i = 0; i < total_blocks_to_recompress; i++)
				{
					const uint32_t block_index = block_wsse_indices[total_blocks - 1 - i];

					const uint32_t block_x = block_index % num_blocks_x;
					const uint32_t block_y = block_index / num_blocks_x;

					superpass2_recompress_block_flags(block_x, block_y) = true;

					if (enc_cfg.m_debug_images)
						vis_recomp_img.fill_box(block_x * block_width, block_y * block_height, block_width, block_height, color_rgba(255, 255, 255, 255));
				}

				if (enc_cfg.m_debug_images)
					save_png(enc_cfg.m_debug_file_prefix + "vis_recomp_img.png", vis_recomp_img);
			}
		}

	} // superpass_index

	if (enc_cfg.m_third_superpass_try_neighbors)
	{
		uint32_t total_superpass1_improved_blocks1 = 0;
		uint32_t total_superpass1_improved_blocks2 = 0;

		// Merge pass 2's output into pass 0's/1's output, which can be done safely now.
		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				ldr_astc_block_encode_image_output::block_info& out_block_info = enc_out.m_image_block_info(bx, by);

				const ldr_astc_block_encode_image_output::block_info_superpass1& out_block_info_superpass1 = enc_out.m_image_block_info_superpass2(bx, by);

				for (uint32_t neighbor_index = 0; neighbor_index < basist::astc_ldr_t::cMaxConfigReuseNeighbors; neighbor_index++)
				{
					const int new_neighbor_index = out_block_info_superpass1.m_config_reuse_neighbor_out_block_indices[neighbor_index];

					if (new_neighbor_index == cInvalidIndex)
					{
						// Can't reuse neighbor's best output block
						continue;
					}

					if (!out_block_info_superpass1.m_config_reuse_new_neighbor_out_block_flags[neighbor_index])
					{
						// Reuses an existing, already encoded output block which matches the neighbor
						assert((size_t)new_neighbor_index < out_block_info.m_out_blocks.size());
						continue;
					}

					const uint32_t new_out_block_index = out_block_info.m_out_blocks.size_u32();

					const encode_block_output& new_output_blk = out_block_info_superpass1.m_new_out_config_reuse_blocks[new_neighbor_index];

					out_block_info.m_out_blocks.push_back(new_output_blk);

#define BU_CHECK_NEIGHBOR_BEST (1)

#if BU_CHECK_NEIGHBOR_BEST
					// See if the solution has improved
					if (new_output_blk.m_sse < out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_sse)
					{
						total_superpass1_improved_blocks1++;

						// Warning: This invalidate the neighbor indices
						out_block_info.m_packed_out_block_index = new_out_block_index;

						//astc_helpers::astc_block& packed_block = enc_out.m_packed_phys_blocks(bx, by);

						bool pack_success = astc_helpers::pack_astc_block((astc_helpers::astc_block&)packed_blocks(bx, by), new_output_blk.m_log_blk);
						if (!pack_success)
						{
							fmt_error_printf("astc_helpers::pack_astc_block failed\n");

							return false;
						}
					}
#endif

				} // neighbor_index

				for (uint32_t j = 0; j < out_block_info_superpass1.m_new_out_config_endpoint_reuse_blocks.size(); j++)
				{
					const uint32_t new_out_block_index = out_block_info.m_out_blocks.size_u32();

					const encode_block_output& new_output_blk = out_block_info_superpass1.m_new_out_config_endpoint_reuse_blocks[j];

					out_block_info.m_out_blocks.push_back(new_output_blk);

#define BU_CHECK_NEIGHBOR_BEST (1)

#if BU_CHECK_NEIGHBOR_BEST
					// See if the solution has improved
					if (new_output_blk.m_sse < out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_sse)
					{
						total_superpass1_improved_blocks2++;

						// Warning: This invalidate the neighbor indices
						out_block_info.m_packed_out_block_index = new_out_block_index;

						//astc_helpers::astc_block& packed_block = enc_out.m_packed_phys_blocks(bx, by);

						bool pack_success = astc_helpers::pack_astc_block((astc_helpers::astc_block&)packed_blocks(bx, by), new_output_blk.m_log_blk);
						if (!pack_success)
						{
							fmt_error_printf("astc_helpers::pack_astc_block failed\n");

							return false;
						}
					}
#endif

				} // j

				enforce_max_candidate_limit(out_block_info, max_candidate_limit);

			} // bx
		} // by

		if (enc_cfg.m_debug_output)
		{
			fmt_debug_printf("Total superpass 1 improved blocks 1: {} {3.2}%\n", total_superpass1_improved_blocks1, ((float)total_superpass1_improved_blocks1 * 100.0f) / (float)(total_blocks));
			fmt_debug_printf("Total superpass 1 improved blocks 2: {} {3.2}%\n", total_superpass1_improved_blocks2, ((float)total_superpass1_improved_blocks2 * 100.0f) / (float)(total_blocks));
		}
	}

	if (ASTC_LDR_CONSISTENCY_CHECKING)
	{
		if (enc_cfg.m_debug_output)
			fmt_debug_printf("consistency checking\n");

		// Consistency/sanity cross checking
		//uint32_t total_blocks_using_neighbor_config = 0;
		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				const ldr_astc_block_encode_image_output::block_info& out_block_info = enc_out.m_image_block_info(bx, by);

				if (out_block_info.m_packed_out_block_index >= out_block_info.m_out_blocks.size())
				{
					fmt_error_printf("consistency check failed\n");
					assert(0);
					return false;
				}

#if BU_CHECK_NEIGHBOR_BEST
				uint64_t best_sse = UINT64_MAX;
				uint32_t best_out_block_index = 0;

				for (uint32_t i = 0; i < out_block_info.m_out_blocks.size(); i++)
				{
					if (out_block_info.m_out_blocks[i].m_sse < best_sse)
					{
						best_sse = out_block_info.m_out_blocks[i].m_sse;
						best_out_block_index = i;
					}
				} // i

				if (best_out_block_index != out_block_info.m_packed_out_block_index)
				{
					fmt_error_printf("consistency check failed\n");
					assert(0);
					return false;
				}
#endif

				if (out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_sse !=
					eval_error(block_width, block_height, out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_log_blk, out_block_info.m_pixel_stats, enc_cfg.m_cem_enc_params))
				{
					fmt_error_printf("consistency check failed\n");
					assert(0);
					return false;
				}

				// Ensure packed output block matches the expected best WSSE block.
				astc_helpers::astc_block packed_block;
				bool pack_success = astc_helpers::pack_astc_block(packed_block, out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_log_blk);
				if (!pack_success)
				{
					fmt_error_printf("astc_helpers::pack_astc_block failed\n");
					return false;
				}

				if (memcmp(&packed_block, &enc_out.m_packed_phys_blocks(bx, by), sizeof(astc_helpers::astc_block)) != 0)
				{
					fmt_error_printf("consistency check failed\n");
					assert(0);
					return false;
				}

				// DCT check
				if ((enc_cfg.m_use_dct) && (out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_trial_mode_index >= 0))
				{
					const auto& best_log_blk = out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_log_blk;
					if (best_log_blk.m_solid_color_flag_ldr)
					{
						fmt_error_printf("consistency check failed\n");
						assert(0);
						return false;
					}

					//const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, best_log_blk.m_grid_width, best_log_blk.m_grid_height);
					const uint32_t total_planes = best_log_blk.m_num_partitions ? (best_log_blk.m_dual_plane ? 2 : 1) : 0;

					astc_helpers::log_astc_block verify_log_blk(best_log_blk);

					basist::astc_ldr_t::fvec dct_temp;

					for (uint32_t plane_index = 0; plane_index < total_planes; plane_index++)
					{
						if (!out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_packed_dct_plane_data[plane_index].m_coeffs.size())
						{
							fmt_error_printf("consistency check failed\n");
							assert(0);
							return false;
						}
												
						bool dec_status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, verify_log_blk, nullptr, nullptr, dct_temp,
							&out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index].m_packed_dct_plane_data[plane_index]);

						if (!dec_status)
						{
							fmt_error_printf("consistency check failed\n");
							assert(0);
							return false;
						}

						for (uint32_t i = 0; i < (uint32_t)(best_log_blk.m_grid_width * best_log_blk.m_grid_height); i++)
						{
							if (best_log_blk.m_weights[i * total_planes + plane_index] != verify_log_blk.m_weights[i * total_planes + plane_index])
							{
								fmt_error_printf("consistency check failed\n");
								assert(0);
								return false;
							}
						}

					} // plane_index
				}

			} // bx
		} // by

		if (enc_cfg.m_debug_output)
			fmt_debug_printf("consistency checking PASSED\n");
	}

	//fmt_debug_printf("Total blocks using neighbor config: {} {3.2}%\n", total_blocks_using_neighbor_config, ((float)total_blocks_using_neighbor_config * 100.0f) / (float)(total_blocks));

	// Debug output
	uint_vec trial_mode_hist;
	trial_mode_hist.resize(encoder_trial_modes.size());
	uint32_t total_alpha_blocks = 0;

	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			const ldr_astc_block_encode_image_output::block_info& out_block_info = enc_out.m_image_block_info(bx, by);
			const astc_ldr::pixel_stats_t& pixel_stats = out_block_info.m_pixel_stats;

			const encode_block_output& best_out_block = out_block_info.m_out_blocks[out_block_info.m_packed_out_block_index];
			const astc_helpers::log_astc_block& best_out_blocks_log_astc_blk = best_out_block.m_log_blk;

			if (pixel_stats.m_has_alpha)
				total_alpha_blocks++;

			output_block_devel_desc& out_devel_desc = output_block_devel_info(bx, by);
			out_devel_desc.m_had_alpha = pixel_stats.m_has_alpha;
			out_devel_desc.m_trial_mode_index = best_out_block.m_trial_mode_index;
			out_devel_desc.m_pTrial_modes = encoder_trial_modes.data();

			if (out_devel_desc.m_trial_mode_index >= 0)
				trial_mode_hist[out_devel_desc.m_trial_mode_index]++;

			//const float total_astc_weight_bits = log2f((float)astc_helpers::get_ise_levels(best_out_block.m_log_blk.m_weight_ise_range)) *
			//	best_out_block.m_log_blk.m_grid_width * best_out_block.m_log_blk.m_grid_height * (best_out_block.m_log_blk.m_dual_plane ? 2 : 1);

			//bool used_blue_contraction = astc_ldr::used_blue_contraction(best_out_blocks_log_astc_blk.m_color_endpoint_modes[0], best_out_blocks_log_astc_blk.m_endpoints, best_out_blocks_log_astc_blk.m_endpoint_ise_range);

			if (enc_cfg.m_debug_images)
			{
				color_rgba vis_col(g_black_color);
				color_rgba vis2_col(g_black_color);
				color_rgba dp_vis(g_black_color);
				color_rgba base_ofs_vis(g_black_color);
				//color_rgba dct_bits_abs_vis(g_black_color);
				//color_rgba dct_bits_vs_astc_vis(g_black_color);

				const astc_ldr::partition_pattern_vec* pPat = nullptr;

				if (best_out_blocks_log_astc_blk.m_num_partitions == 2)
				{
					vis_col.set(0, 255, 0, 255);

					const astc_ldr::partitions_data* pPart_data = pPart_data_p2;

					const uint32_t part_seed_index = best_out_blocks_log_astc_blk.m_partition_id;
					const uint32_t part_unique_index = pPart_data->m_part_seed_to_unique_index[part_seed_index];

					pPat = &pPart_data->m_partition_pats[part_unique_index];
				}
				else if (best_out_blocks_log_astc_blk.m_num_partitions == 3)
				{
					vis_col.set(0, 0, 255, 255);

					const astc_ldr::partitions_data* pPart_data = pPart_data_p3;

					const uint32_t part_seed_index = best_out_blocks_log_astc_blk.m_partition_id;
					const uint32_t part_unique_index = pPart_data->m_part_seed_to_unique_index[part_seed_index];

					pPat = &pPart_data->m_partition_pats[part_unique_index];
				}

				//						vis_col.r = enc_blk_params.m_use_base_scale_modes ? 255 : 0;
				//						vis_col.g = enc_blk_params.m_use_direct_modes ? 255 : 0;

				if (!out_devel_desc.m_low_freq_block_flag)
				{
					if (out_devel_desc.m_super_strong_edges)
						vis2_col.set(255, 0, 255, 255);
					else if (out_devel_desc.m_very_strong_edges)
						vis2_col.set(255, 0, 0, 255);
					else if (out_devel_desc.m_strong_edges)
						vis2_col.set(0, 255, 0, 255);
				}

				if (pPat)
				{
					for (uint32_t y = 0; y < block_height; y++)
					{
						for (uint32_t x = 0; x < block_width; x++)
						{
							const uint32_t subset_idx = (*pPat)(x, y);

							color_rgba c(g_black_color);

							if (best_out_blocks_log_astc_blk.m_num_partitions == 2)
							{
								assert(subset_idx < 2);
								c = subset_idx ? color_rgba(255, 0, 0, 255) : color_rgba(0, 255, 0, 255);
							}
							else
							{
								assert(best_out_blocks_log_astc_blk.m_num_partitions == 3);
								assert(subset_idx < 3);

								if (subset_idx == 2)
									c = color_rgba(0, 0, 255, 255);
								else if (subset_idx == 1)
									c = color_rgba(32, 0, 190, 255);
								else
									c = color_rgba(64, 0, 64, 255);
							}

							vis_part_pat_img.set_clipped(bx * block_width + x, by * block_height + y, c);
						}
					}
				}

				if (best_out_blocks_log_astc_blk.m_dual_plane)
					dp_vis.g = 255;

				if ((best_out_blocks_log_astc_blk.m_color_endpoint_modes[0] == astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET) ||
					(best_out_blocks_log_astc_blk.m_color_endpoint_modes[0] == astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET))
				{
					base_ofs_vis.b = 255;
				}

				vis_part_usage_img.fill_box(bx * block_width, by * block_height, block_width, block_height, vis_col);
				vis_strong_edge.fill_box(bx * block_width, by * block_height, block_width, block_height, vis2_col);
				vis_dp_img.fill_box(bx * block_width, by * block_height, block_width, block_height, dp_vis);
				vis_base_ofs_img.fill_box(bx * block_width, by * block_height, block_width, block_height, base_ofs_vis);
			}

		} // bx

	}  // by

	const double total_enc_time = itm.get_elapsed_secs();

	if (enc_cfg.m_debug_output)
		fmt_debug_printf("ASTC packing complete\n");

	image unpacked_img(width, height);

	// Unpack packed image, validate ASTC data with several decoders.
	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			const astc_helpers::astc_block* pPhys_block = &packed_blocks(bx, by);

			astc_helpers::log_astc_block log_blk;
			bool status = astc_helpers::unpack_block(pPhys_block, log_blk, block_width, block_height);
			if (!status)
			{
				fmt_error_printf("unpack_block() failed\n");
				return false;
			}

			// Decode with our generic ASTC decoder.
			color_rgba block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
			status = astc_helpers::decode_block(log_blk, block_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
			if (!status)
			{
				fmt_error_printf("decode_block() failed\n");
				return false;
			}

			unpacked_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);

			// Decode with the Android testing framework ASTC decoder
			{
				uint8_t dec_pixels_android[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS * 4];

				bool android_success = basisu_astc::astc::decompress_ldr(dec_pixels_android, (const uint8_t*)pPhys_block, enc_cfg.m_cem_enc_params.m_decode_mode_srgb, block_width, block_height);
				if (!android_success)
				{
					fmt_error_printf("Android ASTC decoder failed!\n");
					return false;
				}

				if (memcmp(dec_pixels_android, block_pixels, total_block_pixels * 4) != 0)
				{
					fmt_error_printf("Android ASTC decoder mismatch!\n");
					return false;
				}
			}

			// Decode with our optimized XUASTC LDR decoder
			{
				color_rgba block_pixels_alt[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
				status = astc_helpers::decode_block_xuastc_ldr(log_blk, block_pixels_alt, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
				if (!status)
				{
					fmt_error_printf("decode_block_xuastc_ldr() failed\n");
					return false;
				}

				if (memcmp(block_pixels, block_pixels_alt, total_block_pixels * 4) != 0)
				{
					fmt_error_printf("XUASTC LDR ASTC decoder mismatch!\n");
					return false;
				}
			}

		} // bx
	} // by

	if (enc_cfg.m_debug_images)
	{
		save_png(enc_cfg.m_debug_file_prefix + "dbg_astc_ldr_unpacked_img.png", unpacked_img);
		
		if (vis_part_usage_img.is_valid())
			save_png(enc_cfg.m_debug_file_prefix + "vis_part_usage.png", vis_part_usage_img);

		if (vis_part_pat_img.is_valid())
			save_png(enc_cfg.m_debug_file_prefix + "vis_part_pat_img.png", vis_part_pat_img);

		if (vis_strong_edge.is_valid())
			save_png(enc_cfg.m_debug_file_prefix + "vis_strong_edge.png", vis_strong_edge);

		if (vis_dct_low_freq_block.is_valid())
			save_png(enc_cfg.m_debug_file_prefix + "vis_dct_low_freq_block.png", vis_dct_low_freq_block);

		if (vis_dp_img.is_valid())
			save_png(enc_cfg.m_debug_file_prefix + "vis_dp.png", vis_dp_img);
		
		if (vis_base_ofs_img.is_valid())
			save_png(enc_cfg.m_debug_file_prefix + "vis_base_ofs.png", vis_base_ofs_img);
	}

	if (enc_cfg.m_debug_output)
	{
		display_candidate_statistics(enc_out);

		uint32_t cem_used_hist[16] = { 0 };
		uint32_t cem_used_bc[16] = { 0 };
		uint32_t cem_used_subsets[16] = { 0 };
		uint32_t cem_used_dp[16] = { 0 };
		uint32_t total_dp = 0, total_base_ofs = 0;
		uint32_t subset_used_hist[4] = { 0 };
		uint32_t grid_usage_hist[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS * astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS + 1] = { 0 };

		uint32_t total_header_bits = 0;
		uint32_t total_weight_bits = 0;
		uint32_t total_endpoint_bits = 0;

		uint32_t total_void_extent = 0;

		uint32_t used_endpoint_levels_hist[astc_helpers::LAST_VALID_ENDPOINT_ISE_RANGE - astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE + 1] = { 0 };
		uint32_t used_weight_levels_hist[astc_helpers::LAST_VALID_WEIGHT_ISE_RANGE - astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE + 1] = { 0 };

		uint32_t total_blocks_using_subsets = 0;

		uint32_t total_used_bc = 0;

		uint32_t total_candidates = 0;

		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				const ldr_astc_block_encode_image_output::block_info& out_block_info = enc_out.m_image_block_info(bx, by);
				total_candidates += out_block_info.m_out_blocks.size_u32();

				const output_block_devel_desc& desc = output_block_devel_info(bx, by);

				const astc_helpers::astc_block* pPhys_block = &packed_blocks(bx, by);

				astc_helpers::log_astc_block log_blk;
				bool status = astc_helpers::unpack_block(pPhys_block, log_blk, block_width, block_height);
				if (!status)
				{
					fmt_error_printf("unpack_block() failed\n");
					return false;
				}

				if (desc.m_trial_mode_index < 0)
				{
					total_void_extent++;
					continue;
				}
				else
				{
					const basist::astc_ldr_t::trial_mode& tm = desc.m_pTrial_modes[desc.m_trial_mode_index];

					const uint32_t actual_cem = log_blk.m_color_endpoint_modes[0];
					//assert(tm.m_cem == log_blk.m_color_endpoint_modes[0]); // may differ due to base+ofs usage

					assert((tm.m_ccs_index >= 0) == log_blk.m_dual_plane);
					assert((!log_blk.m_dual_plane) || (tm.m_ccs_index == log_blk.m_color_component_selector));
					assert(tm.m_endpoint_ise_range == log_blk.m_endpoint_ise_range);
					assert(tm.m_weight_ise_range == log_blk.m_weight_ise_range);
					assert(tm.m_grid_width == log_blk.m_grid_width);
					assert(tm.m_grid_height == log_blk.m_grid_height);
					assert(tm.m_num_parts == log_blk.m_num_partitions);

					used_weight_levels_hist[open_range_check<int>(tm.m_weight_ise_range - astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE, std::size(used_weight_levels_hist))]++;
					used_endpoint_levels_hist[open_range_check<int>(tm.m_endpoint_ise_range - astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE, std::size(used_endpoint_levels_hist))]++;

					cem_used_hist[actual_cem]++;
					if (log_blk.m_dual_plane)
						total_dp++;

					subset_used_hist[open_range_check<size_t>(log_blk.m_num_partitions - 1, std::size(subset_used_hist))]++;

					bool used_bc = false;
					for (uint32_t i = 0; i < tm.m_num_parts; i++)
					{
						if (astc_helpers::used_blue_contraction(actual_cem, log_blk.m_endpoints + i * astc_helpers::get_num_cem_values(actual_cem), log_blk.m_endpoint_ise_range))
						{
							used_bc = true;
						}
					}

					if (used_bc)
					{
						cem_used_bc[actual_cem]++;
						total_used_bc++;
					}

					if (tm.m_num_parts > 1)
						cem_used_subsets[actual_cem]++;

					// TODO: add CCS index histogram per CEM
					if (log_blk.m_dual_plane)
						cem_used_dp[actual_cem]++;

					if ((actual_cem == astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET) ||
						(actual_cem == astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET))
					{
						total_base_ofs++;
					}

					grid_usage_hist[open_range_check<size_t>(log_blk.m_grid_width * log_blk.m_grid_height, std::size(grid_usage_hist))]++;

					if (tm.m_num_parts > 1)
						total_blocks_using_subsets++;
				}

				astc_helpers::pack_stats pack_stats;
				pack_stats.clear();

				astc_helpers::astc_block temp_phys_block;
				int expected_endpoint_range = 0;
				status = astc_helpers::pack_astc_block(temp_phys_block, log_blk, &expected_endpoint_range, &pack_stats);
				assert(status);

				total_header_bits += pack_stats.m_header_bits;
				total_weight_bits += pack_stats.m_weight_bits;
				total_endpoint_bits += pack_stats.m_endpoint_bits;

			} // bx
		} // by

		fmt_debug_printf("Avg candidates per block: {}\n", (float)total_candidates / (float)total_blocks);

		uint32_t total_used_modes = 0;

		fmt_debug_printf("--------------------- Trial Modes:\n");

		for (uint32_t i = 0; i < trial_mode_hist.size(); i++)
		{
			if (!trial_mode_hist[i])
				continue;

			if (trial_mode_hist[i])
				total_used_modes++;
			
#if 0
			const uint32_t total_mode_blocks = trial_mode_hist[i];

			const uint32_t num_subsets = encoder_trial_modes[i].m_num_parts;
			const uint32_t cem_index = encoder_trial_modes[i].m_cem;

			fmt_debug_printf("{}: {} {3.2}%: cem: {}, grid {}x{}, e: {} w: {}, ccs: {}, parts: {}, total base+ofs: {}, total direct: {}\n", i, total_mode_blocks, (float)total_mode_blocks * 100.0f / (float)total_blocks,
				encoder_trial_modes[i].m_cem,
				encoder_trial_modes[i].m_grid_width, encoder_trial_modes[i].m_grid_height,
				astc_helpers::get_ise_levels(encoder_trial_modes[i].m_endpoint_ise_range), astc_helpers::get_ise_levels(encoder_trial_modes[i].m_weight_ise_range),
				encoder_trial_modes[i].m_ccs_index,
				encoder_trial_modes[i].m_num_parts,
				used_base_offset_count[i],
				used_rgb_direct_count[i]);
#endif
		}

		fmt_debug_printf("\n");

		fmt_debug_printf("Used endpoint ISE levels:\n");
		for (uint32_t i = 0; i < std::size(used_endpoint_levels_hist); i++)
			fmt_debug_printf("{} levels: {}\n", astc_helpers::get_ise_levels(astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE + i), used_endpoint_levels_hist[i]);

		fmt_debug_printf("\nUsed weight ISE levels:\n");
		for (uint32_t i = 0; i < std::size(used_weight_levels_hist); i++)
			fmt_debug_printf("{} levels: {}\n", astc_helpers::get_ise_levels(astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE + i), used_weight_levels_hist[i]);

		const uint32_t total_blocks_excluding_void_extent = total_blocks - total_void_extent;

		fmt_debug_printf("\nTotal blocks: {}, excluding void extent: {}\n", total_blocks, total_blocks_excluding_void_extent);
		fmt_debug_printf("Total void extent blocks skipped by compressor: {}\n", total_void_extent_blocks_skipped);
		fmt_debug_printf("Total final void extent blocks: {}\n", total_void_extent);
		fmt_debug_printf("Total input blocks with alpha: {} {3.1}%\n", total_alpha_blocks, (float)total_alpha_blocks * 100.0f / (float)total_blocks);

		fmt_debug_printf("\nASTC phys avg block stats (including void extent):\n");
		fmt_debug_printf("Total header bits: {}, {} per block, {} per pixel\n", total_header_bits, (float)total_header_bits / (float)total_blocks, (float)total_header_bits / (float)(total_pixels));
		fmt_debug_printf("Total weight bits: {}, {} per block, {} per pixel\n", total_weight_bits, (float)total_weight_bits / (float)total_blocks, (float)total_weight_bits / (float)(total_pixels));
		fmt_debug_printf("Total endpoint bits: {}, {} per block, {} per pixel\n", total_endpoint_bits, (float)total_endpoint_bits / (float)total_blocks, (float)total_endpoint_bits / (float)(total_pixels));
		fmt_debug_printf("Total header+endpoint bits: {}, {} per block, {} per pixel\n", total_header_bits + total_endpoint_bits,
			(float)(total_header_bits + total_endpoint_bits) / (float)total_blocks, (float)(total_header_bits + total_endpoint_bits) / (float)(total_pixels));
		fmt_debug_printf("Total header+endpoint+weight bits: {}, {} per block, {} per pixel\n", total_header_bits + total_endpoint_bits + total_weight_bits,
			(float)(total_header_bits + total_endpoint_bits + total_weight_bits) / (float)total_blocks, (float)(total_header_bits + total_endpoint_bits + total_weight_bits) / (float)(total_pixels));

		fmt_debug_printf("\nEncoder stats:\n");
		fmt_debug_printf("Total utilized encoder trial modes: {} {3.2}%\n", total_used_modes, (float)total_used_modes * 100.0f / (float)encoder_trial_modes.size());

		uint32_t overall_blurred_blocks_p1 = 0, overall_blurred_blocks_p2 = 0;
		for (uint32_t i = 0; i < std::size(total_blurred_blocks_p1); i++)
		{
			overall_blurred_blocks_p1 += total_blurred_blocks_p1[i];
			overall_blurred_blocks_p2 += total_blurred_blocks_p2[i];
		}

		fmt_debug_printf("\nTotal blur encodes p1: {} ({3.2}%)\n", total_blur_encodes_p1, (float)total_blur_encodes_p1 * 100.0f / (float)total_blocks);
		fmt_debug_printf("Total blurred blocks p1: {} ({3.2}%)\n", overall_blurred_blocks_p1, (float)overall_blurred_blocks_p1 * 100.0f / (float)total_blocks);
						
		if (overall_blurred_blocks_p1)
		{
			for (uint32_t i = 0; i < std::size(total_blurred_blocks_p1); i++)
			{
				if (total_blurred_blocks_p1[i])
					fmt_debug_printf("Total blurred{} blocks p1: {} ({3.2}%)\n", i, total_blurred_blocks_p1[i], (float)total_blurred_blocks_p1[i] * 100.0f / (float)total_blocks);
			}
		}

		fmt_debug_printf("\nTotal blur encodes p2: {} ({3.2}%)\n", total_blur_encodes_p2, (float)total_blur_encodes_p2 * 100.0f / (float)total_blocks);
		fmt_debug_printf("Total blurred blocks p2: {} ({3.2}%)\n", overall_blurred_blocks_p2, (float)overall_blurred_blocks_p2 * 100.0f / (float)total_blocks);

		if (overall_blurred_blocks_p2)
		{
			for (uint32_t i = 0; i < std::size(total_blurred_blocks_p2); i++)
			{
				if (total_blurred_blocks_p2[i])
					fmt_debug_printf("Total blurred{} blocks p2: {} ({3.2}%)\n", i, total_blurred_blocks_p2[i], (float)total_blurred_blocks_p2[i] * 100.0f / (float)total_blocks);
			}
		}
		
		fmt_debug_printf("\nTotal superbuckets created: {} ({4.1} per block)\n", total_superbuckets_created, (float)total_superbuckets_created / (float)total_blocks);
		fmt_debug_printf("Total shortlist buckets created: {} ({4.1} per block)\n", total_buckets_created, (float)total_buckets_created / (float)total_blocks);
		fmt_debug_printf("Total surrogate encodes: {} ({4.1} per block)\n", total_surrogate_encodes, (float)total_surrogate_encodes / (float)total_blocks);
		fmt_debug_printf("Total shortlist candidates (before full encoding): {} ({4.1} per block)\n", total_shortlist_candidates, (float)total_shortlist_candidates / (float)total_blocks);
		fmt_debug_printf("Total full encodes on superpass 0: {} ({4.1} per block)\n", total_full_encodes, (float)total_full_encodes / (float)total_blocks);
		fmt_debug_printf("Total full encodes on superpass 1: {} ({4.1} per block)\n", total_full_encodes_pass1, (float)total_full_encodes_pass1 / (float)total_blocks);
		fmt_debug_printf("Total full encodes on superpass 2: {} ({4.1} per block)\n", total_full_encodes_pass2, (float)total_full_encodes_pass2 / (float)total_blocks);

		debug_printf("\nTotal final encoded ASTC blocks using blue contraction: %u (%.2f%%)\n", total_used_bc, 100.0f * (float)total_used_bc / (float)total_blocks);

		fmt_debug_printf("Total final encoded ASTC blocks using dual planes: {} {3.2}%\n", total_dp, (float)total_dp * 100.0f / (float)total_blocks);
		fmt_debug_printf("Total final encoded ASTC blocks using base+ofs: {} {3.2}%\n", total_base_ofs, (float)total_base_ofs * 100.0f / (float)total_blocks);
		fmt_debug_printf("Total final encoded ASTC blocks using subsets: {} {3.2}%\n", total_blocks_using_subsets, (float)total_blocks_using_subsets * 100.0f / (float)total_blocks);

		debug_printf("\nSubset usage histogram:\n");
		for (uint32_t i = 0; i < 4; i++)
			fmt_debug_printf("{} subsets: {} {3.2}%\n", i + 1, subset_used_hist[i], (float)subset_used_hist[i] * 100.0f / (float)total_blocks);
		debug_printf("\n");

		debug_printf("CEM usage histogram:\n");
		for (uint32_t i = 0; i < 16; i++)
		{
			if (astc_helpers::is_cem_hdr(i))
				continue;

			std::string n(astc_helpers::get_cem_name(i));
			while (n.size() < 40)
				n.push_back(' ');

			fmt_debug_printf("{}: {} {3.2}%, Used BC: {3.2}%, Used subsets: {3.2}%, Used DP: {3.2}%\n",
				n,
				cem_used_hist[i],
				(float)cem_used_hist[i] * 100.0f / (float)total_blocks,
				(float)cem_used_bc[i] * 100.0f / (float)total_blocks,
				(float)cem_used_subsets[i] * 100.0f / (float)total_blocks,
				(float)cem_used_dp[i] * 100.0f / (float)total_blocks);
		}
		debug_printf("\n");

		debug_printf("Grid samples histogram:\n");
		for (uint32_t i = 1; i <= block_width * block_height; i++)
		{
			if (grid_usage_hist[i])
				fmt_debug_printf("{} samples: {} {3.2}%\n", i, grid_usage_hist[i], (float)grid_usage_hist[i] * 100.0f / (float)total_blocks);
		}
		debug_printf("\n");

		if (enc_cfg.m_debug_output_image_metrics)
		{
			fmt_debug_printf("orig vs. ASTC compressed:\n");
			print_image_metrics(orig_img, unpacked_img);
		}

		fmt_debug_printf("Total encode time: {.3} secs, {.3} ms per block, {.1} blocks/sec\n", total_enc_time, total_enc_time * 1000.0f / total_blocks, total_blocks / total_enc_time);

		fmt_debug_printf("OK\n");
	}

	return true;
}

bool ldr_astc_block_encode_image_fast_4x4(
	const image& orig_img,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	const astc_ldr_encode_config& global_cfg,
	ldr_astc_block_encode_image_output& enc_out,
	uint32_t max_candidate_limit)
{
	BASISU_NOTE_UNUSED(max_candidate_limit);

	if (enc_cfg.m_debug_output)
		fmt_debug_printf("ldr_astc_block_encode_image_fast_4x4:\n");

	const uint32_t block_width = enc_cfg.m_block_width, block_height = enc_cfg.m_block_height;
	if ((block_width != 4) || (block_height != 4))
	{
		assert(0);
		return false;
	}

	//const uint32_t width = orig_img.get_width(), height = orig_img.get_height();
	//const uint32_t total_pixels = width * height;
	const uint32_t total_block_pixels = enc_cfg.m_block_width * enc_cfg.m_block_height;
	const uint32_t num_blocks_x = orig_img.get_block_width(enc_cfg.m_block_width);
	const uint32_t num_blocks_y = orig_img.get_block_height(enc_cfg.m_block_height);
	const uint32_t total_blocks = num_blocks_x * num_blocks_y;

	if (enc_cfg.m_debug_output)
	{
		fmt_debug_printf("\nASTC base bitrate: {3.3} bpp\n", 128.0f / (float)(enc_cfg.m_block_width * enc_cfg.m_block_height));

		fmt_debug_printf("ASTC block size: {}x{}\n", enc_cfg.m_block_width, enc_cfg.m_block_height);

		fmt_debug_printf("Image has alpha: {}\n", orig_img.has_alpha());

		fmt_debug_printf("max_candidate_limit: {}\n", max_candidate_limit);;
	}

	if ((enc_cfg.m_cem_enc_params.m_comp_weights[0] != 1) || (enc_cfg.m_cem_enc_params.m_comp_weights[1] != 1) ||
		(enc_cfg.m_cem_enc_params.m_comp_weights[2] != 1) || (enc_cfg.m_cem_enc_params.m_comp_weights[3] != 1))
	{
		printf("WARNING: Fast (effort 0) ASTC/XUASTC LDR 4x4 compressor doesn't support non-default channel weights\n");
	}

	// We don't use this here, but the supercompressors use these tables.
	// TODO: The transcoder already creates all this stuff for each block size.
	astc_ldr::partitions_data* pPart_data_p2 = &enc_out.m_part_data_p2;
	pPart_data_p2->init(2, enc_cfg.m_block_width, enc_cfg.m_block_height, BASISU_USE_LSH2 == 0, BASISU_USE_LSH2 != 0);

	astc_ldr::partitions_data* pPart_data_p3 = &enc_out.m_part_data_p3;
	pPart_data_p3->init(3, enc_cfg.m_block_width, enc_cfg.m_block_height, BASISU_USE_LSH3 == 0, BASISU_USE_LSH3 != 0);

	basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes = enc_out.m_encoder_trial_modes;
	encoder_trial_modes.reserve(4096);

	basist::astc_ldr_t::grouped_trial_modes& grouped_encoder_trial_modes = enc_out.m_grouped_encoder_trial_modes;
	basist::astc_ldr_t::create_encoder_trial_modes_table(block_width, block_height, encoder_trial_modes, grouped_encoder_trial_modes, enc_cfg.m_debug_output, false);
	
	uint32_t bc7f_override_flags = basist::bc7f::cPackBC7FlagPBitOpt | basist::bc7f::cPackBC7FlagPBitOptMode6 | basist::bc7f::cPackBC7FlagUseTrivialMode6 | 
		basist::bc7f::cPackBC7FlagUse2SubsetsRGB | basist::bc7f::cPackBC7FlagASTCCompatible |
		basist::bc7f::cPackBC7FlagUseDualPlaneRGB | basist::bc7f::cPackBC7FlagUseDualPlaneRGBA |
		basist::bc7f::cPackBC7FlagUse3SubsetsRGB | basist::bc7f::cPackBC7FlagUse2SubsetsRGBA;

	if (global_cfg.m_force_disable_subsets)
	{
		bc7f_override_flags &= ~(basist::bc7f::cPackBC7FlagUse2SubsetsRGB | basist::bc7f::cPackBC7FlagUse3SubsetsRGB | basist::bc7f::cPackBC7FlagUse2SubsetsRGBA);
	}

	if (global_cfg.m_force_disable_rgb_dual_plane)
	{
		bc7f_override_flags &= ~basist::bc7f::cPackBC7FlagUseDualPlaneRGB;
		bc7f_override_flags |= basist::bc7f::cPackBC7FlagDisableRGBDualPlane;
	}

	enc_out.m_image_block_info.resize(0, 0);
	enc_out.m_image_block_info.resize(num_blocks_x, num_blocks_y);

	enc_out.m_packed_phys_blocks.resize(num_blocks_x, num_blocks_y);

	basist::astc_ldr_t::grid_weight_dct grid_coder;
	if (enc_cfg.m_use_dct)
		grid_coder.init(block_width, block_height);

	assert(enc_cfg.m_pJob_pool);
	job_pool& job_pool = *enc_cfg.m_pJob_pool;
	
	const uint32_t num_threads = (uint32_t)job_pool.get_total_threads();
	assert(num_threads);
	
	std::atomic<int> cur_row;
	cur_row.store(0);

	std::atomic<bool> encoder_failed_flag;
	encoder_failed_flag.store(false);

	for (uint32_t job_index = 0; job_index < num_threads; job_index++)
	{
		job_pool.add_job([job_index, num_threads, num_blocks_x, num_blocks_y, block_width, block_height, total_blocks, total_block_pixels, bc7f_override_flags,
			&cur_row, &encoder_failed_flag,
			&orig_img, &enc_cfg, &encoder_trial_modes, &grid_coder, &grouped_encoder_trial_modes, &enc_out]
		{
			BASISU_NOTE_UNUSED(job_index); BASISU_NOTE_UNUSED(num_threads); BASISU_NOTE_UNUSED(total_blocks); BASISU_NOTE_UNUSED(total_block_pixels); BASISU_NOTE_UNUSED(encoder_trial_modes); BASISU_NOTE_UNUSED(grouped_encoder_trial_modes);
			basist::astc_ldr_t::fvec dct_temp;

			for ( ; ; )
			{
				if (encoder_failed_flag)
					return;

				const uint32_t by = cur_row.fetch_add(1);
				if (by >= num_blocks_y)
					break;

				for (uint32_t bx = 0; bx < num_blocks_x; bx++)
				{
					if (encoder_failed_flag)
						return;

					color_rgba block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
					orig_img.extract_block_clamped(block_pixels, bx * block_width, by * block_height, block_width, block_height);

					ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

					encode_block_output* pEnc_block_output = blk_info.m_out_blocks.enlarge(1);
					pEnc_block_output->clear();

					astc_helpers::log_astc_block& log_blk = pEnc_block_output->m_log_blk;

					if (!basist::bc7f::fast_pack_astc(log_blk, (basist::color_rgba*)block_pixels, bc7f_override_flags))
					{
						assert(0);
						encoder_failed_flag.store(true);
						return;
					}
										
					if (log_blk.m_solid_color_flag_ldr)
					{
						// weighted SSE should be zero, the block should always be solid here
						pEnc_block_output->m_trial_mode_index = -1;
						pEnc_block_output->m_sse = 0;
					}
					else
					{
						uint32_t cem_to_find = log_blk.m_color_endpoint_modes[0];
						if (cem_to_find == astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET)
							cem_to_find = astc_helpers::CEM_LDR_RGB_DIRECT;
						else if (cem_to_find == astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET)
							cem_to_find = astc_helpers::CEM_LDR_RGBA_DIRECT;

						const int ccs_to_find = log_blk.m_dual_plane ? log_blk.m_color_component_selector : -1;
						BASISU_NOTE_UNUSED(ccs_to_find);

						const basisu::uint_vec& tms = basist::astc_ldr_t::get_tm_candidates(basist::astc_ldr_t::g_grouped_encoder_trial_modes[0],
							cem_to_find, log_blk.m_num_partitions - 1, log_blk.m_dual_plane ? log_blk.m_color_component_selector + 1 : 0,
							1, basist::astc_ldr_t::calc_grid_aniso_val(log_blk.m_grid_width, log_blk.m_grid_height, 4, 4));

						uint32_t tm_index = 0, tms_index = 0;
						for (tms_index = 0; tms_index < tms.size(); tms_index++)
						{
							tm_index = tms[tms_index];

							const auto& tm = basist::astc_ldr_t::g_encoder_trial_modes[0][tm_index];

							assert(tm.m_cem == cem_to_find);
							assert(tm.m_num_parts == log_blk.m_num_partitions);
							assert(tm.m_ccs_index == ccs_to_find);

							if ((tm.m_endpoint_ise_range == log_blk.m_endpoint_ise_range) && (tm.m_weight_ise_range == log_blk.m_weight_ise_range))
							{
								if ((tm.m_grid_width == log_blk.m_grid_width) && (tm.m_grid_height == log_blk.m_grid_height))
								{
									break;
								}
							}
						} // tms_index

						if (tms_index == tms.size())
						{
							assert(0);
							encoder_failed_flag.store(true);
							return;
						}

						pEnc_block_output->m_trial_mode_index = basisu::safe_cast_int16(tm_index);

						if (enc_cfg.m_use_dct)
						{
							//const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, new_log_block.m_grid_width, new_log_block.m_grid_height);

							const uint32_t num_planes = (log_blk.m_dual_plane ? 2 : 1);

							uint32_t total_empty_planes = 0;

							for (uint32_t plane_index = 0; plane_index < num_planes; plane_index++)
							{
								basist::astc_ldr_t::dct_syms& syms = pEnc_block_output->m_packed_dct_plane_data[plane_index];

								code_block_weights(grid_coder, enc_cfg.m_base_q, plane_index, log_blk, syms, dct_temp);

								// ensure existing weights get blown away 
								for (uint32_t i = 0; i < (uint32_t)(log_blk.m_grid_width * log_blk.m_grid_height); i++)
									log_blk.m_weights[i * num_planes + plane_index] = 0;

								bool dec_status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, log_blk, nullptr, nullptr, dct_temp, &syms);

								assert(dec_status);
								if (!dec_status)
								{
									error_printf("grid_coder.decode_block_weights() failed!\n");
									encoder_failed_flag.store(true);
									return;
								}

								// check for all-zero AC's
								if (syms.m_coeffs.size() == 1)
								{
									if ((1 + syms.m_coeffs[0].m_num_zeros) == (log_blk.m_grid_width * log_blk.m_grid_height))
									{
										total_empty_planes++;
									}
								}
							}

							if ((log_blk.m_num_partitions == 1) && (total_empty_planes == num_planes))
							{
								// entire block post-quantization is DC only (no non-zero AC), switch to void-extent
								uint32_t sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
								for (uint32_t i = 0; i < 16; i++)
								{
									sum_r += block_pixels[i].r;
									sum_g += block_pixels[i].g;
									sum_b += block_pixels[i].b;
									sum_a += block_pixels[i].a;
								}

								sum_r = (sum_r + 8) >> 4;
								sum_g = (sum_g + 8) >> 4;
								sum_b = (sum_b + 8) >> 4;
								sum_a = (sum_a + 8) >> 4;

								astc_helpers::set_ldr_solid_block(log_blk, sum_r, sum_g, sum_b, sum_a);

								pEnc_block_output->m_trial_mode_index = -1;
							}

						} // if (enc_cfg.m_use_dct)

						{
							color_rgba dec_block_pixels[16];

							bool dec_status = astc_helpers::decode_block_xuastc_ldr(log_blk, dec_block_pixels, 4, 4,
								enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);

							if (!dec_status)
							{
								// Shouldn't ever happen
								assert(0);
								encoder_failed_flag.store(true);
								return;
							}

							uint64_t total_err = 0;

							for (uint32_t i = 0; i < 16; i++)
								total_err += weighted_color_error(dec_block_pixels[i], block_pixels[i], enc_cfg.m_cem_enc_params);

							pEnc_block_output->m_sse = total_err;
						}
					}

					bool pack_status = astc_helpers::pack_astc_block(enc_out.m_packed_phys_blocks(bx, by), log_blk);
					if (!pack_status)
					{
						assert(0);
						encoder_failed_flag.store(true);
						return;
					}

				} // bx

			} // for

		} );

	} // job_index

	job_pool.wait_for_all();

	if (encoder_failed_flag)
	{
		fmt_error_printf("ldr_astc_block_encode_image_fast_4x4: Main compressor block loop failed!\n");
		return false;
	}

	if (enc_cfg.m_debug_output)
	{
		display_candidate_statistics(enc_out);

		fmt_debug_printf("ldr_astc_block_encode_image_fast_4x4: done\n");
	}

	return true;
}

#if BASISU_SUPPORT_ASTCENC
static astcenc_context* comp_astc_init_ldr(
	uint32_t block_w,
	uint32_t block_h,
	bool srgb,
	float quality,  // ASTCENC_PRE_MEDIUM etc.
	uint32_t thread_count,
	uint32_t max_partitions,
	bool xuastc_ldr_flag,
	const uint32_t chan_weights[4],
	bool disable_rgb_dual_planes,
	uint32_t candidate_row_pitch, std::vector< std::vector<astcenc_physblk> > *pCandidates)
{
	astcenc_config config{};

	const astcenc_profile profile = srgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR;

	uint32_t flags = ASTCENC_FLG_USE_DECODE_UNORM8;

	// flags = 0 for ordinary LDR color compression
	astcenc_error status = astcenc_config_init(
		profile,
		block_w,
		block_h,
		1,
		quality,
		flags,
		&config);

	if (status != ASTCENC_SUCCESS)
	{
		std::printf("astcenc_config_init() failed\n");
		return nullptr;
	}

	config.cw_r_weight = (float)chan_weights[0];
	config.cw_g_weight = (float)chan_weights[1];
	config.cw_b_weight = (float)chan_weights[2];
	config.cw_a_weight = (float)chan_weights[3];

	config.m_xuastc_ldr_flag = xuastc_ldr_flag;

	config.tune_partition_count_limit = maximum(1u, max_partitions);
	config.m_disable_rgb_dual_planes = disable_rgb_dual_planes;

	if (pCandidates)
	{
		config.candidate_row_pitch = candidate_row_pitch;
		config.m_pCandidates = pCandidates;
	}

	astcenc_context* pContext = nullptr;

	status = astcenc_context_alloc(&config, thread_count, &pContext);

	if (status != ASTCENC_SUCCESS)
	{
		std::printf("astcenc_context_alloc() failed\n");
		return nullptr;
	}

	return pContext;
}

static void comp_astc_deinit(astcenc_context* pContext)
{
	astcenc_context_free(pContext);
}

static uint32_t get_astc_block_count(uint32_t dim, uint32_t block_dim)
{
	return (dim + block_dim - 1) / block_dim;
}

// Compress from tightly packed RGBA8 input.
// src_rgba must point to w * h * 4 bytes.
static bool comp_astc_image_u8(
	astcenc_context* pContext,
	const uint8_t* src_rgba,
	uint32_t w,
	uint32_t h,
	uint32_t block_w,
	uint32_t block_h,
	basisu::vector2D<astc_helpers::astc_block>& out_blocks,
	job_pool &job_pool)
{
	static const astcenc_swizzle swizzle{
		ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A
	};

	astcenc_image image{};
	image.dim_x = w;
	image.dim_y = h;
	image.dim_z = 1;
	image.data_type = ASTCENC_TYPE_U8;

	// astcenc_image::data is an array of pointers to 2D slices.
	// For a 2D image we provide one slice pointer.
	void* slices[1];
	slices[0] = const_cast<uint8_t*>(src_rgba);
	image.data = slices;

	out_blocks.resize(get_astc_block_count(w, block_w), get_astc_block_count(h, block_h));

	const uint32_t num_threads = (uint32_t)job_pool.get_total_threads();

	if (num_threads == 1)
	{
		astcenc_error status = astcenc_compress_image(
			pContext,
			&image,
			&swizzle,
			reinterpret_cast<uint8_t*>(out_blocks.get_ptr()),
			out_blocks.size_in_bytes(),
			0);

		return (status == ASTCENC_SUCCESS);
	}
	
	std::atomic<bool> any_failures;
	any_failures.store(false);

	for (uint32_t thread_index = 0; thread_index < num_threads; thread_index++)
	{
		job_pool.add_job([thread_index, pContext, &image, &out_blocks, &any_failures]
			{
				if (any_failures)
					return;

				astcenc_error status = astcenc_compress_image(
					pContext,
					&image,
					&swizzle,
					reinterpret_cast<uint8_t*>(out_blocks.get_ptr()),
					out_blocks.size_in_bytes(),
					thread_index);

				if (status != ASTCENC_SUCCESS)
				{
					any_failures.store(true);
				}
			}
		);
	}

	job_pool.wait_for_all();

	if (any_failures)
		return false;

	astcenc_compress_reset(pContext);

	return true;
}

static bool ldr_astc_block_encode_image_astcenc(
	const image& orig_img,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	const astc_ldr_encode_config& global_cfg,
	ldr_astc_block_encode_image_output& enc_out,
	uint32_t max_candidate_limit)
{
	if (enc_cfg.m_debug_output)
		fmt_debug_printf("ldr_astc_block_encode_image_astcenc:\n");

	const uint32_t block_width = enc_cfg.m_block_width, block_height = enc_cfg.m_block_height;

	//const uint32_t width = orig_img.get_width(), height = orig_img.get_height();
	//const uint32_t total_pixels = width * height;
	const uint32_t total_block_pixels = enc_cfg.m_block_width * enc_cfg.m_block_height;
	const uint32_t num_blocks_x = orig_img.get_block_width(enc_cfg.m_block_width);
	const uint32_t num_blocks_y = orig_img.get_block_height(enc_cfg.m_block_height);
	const uint32_t total_blocks = num_blocks_x * num_blocks_y;

	if (enc_cfg.m_debug_output)
	{
		fmt_debug_printf("\nASTC base bitrate: {3.3} bpp\n", 128.0f / (float)(enc_cfg.m_block_width * enc_cfg.m_block_height));

		fmt_debug_printf("ASTC block size: {}x{}\n", enc_cfg.m_block_width, enc_cfg.m_block_height);

		fmt_debug_printf("Image has alpha: {}\n", orig_img.has_alpha());

		fmt_debug_printf("max_candidate_limit: {}\n", max_candidate_limit);
	}

	// We don't use this here, but the supercompressors use these tables.
	// TODO: The transcoder already creates all this stuff for each block size.
	astc_ldr::partitions_data* pPart_data_p2 = &enc_out.m_part_data_p2;
	pPart_data_p2->init(2, enc_cfg.m_block_width, enc_cfg.m_block_height, BASISU_USE_LSH2 == 0, BASISU_USE_LSH2 != 0);

	astc_ldr::partitions_data* pPart_data_p3 = &enc_out.m_part_data_p3;
	pPart_data_p3->init(3, enc_cfg.m_block_width, enc_cfg.m_block_height, BASISU_USE_LSH3 == 0, BASISU_USE_LSH3 != 0);

	basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes = enc_out.m_encoder_trial_modes;
	encoder_trial_modes.reserve(4096);

	basist::astc_ldr_t::grouped_trial_modes& grouped_encoder_trial_modes = enc_out.m_grouped_encoder_trial_modes;
	basist::astc_ldr_t::create_encoder_trial_modes_table(block_width, block_height, encoder_trial_modes, grouped_encoder_trial_modes, enc_cfg.m_debug_output, false);

	const uint32_t block_size_index = astc_helpers::get_block_size_index(block_width, block_height);

	assert(enc_cfg.m_pJob_pool);
	job_pool& job_pool = *enc_cfg.m_pJob_pool;
	const uint32_t num_threads = (uint32_t)job_pool.get_total_threads();

	float astcenc_quality = ASTCENC_PRE_FASTEST;

	switch (global_cfg.m_effort_level)
	{
	case 0:
		astcenc_quality = ASTCENC_PRE_FASTEST;
		break;
	case 1:
		astcenc_quality = lerp(ASTCENC_PRE_FASTEST, ASTCENC_PRE_FAST, 1.0f / 3.0f);
		break;
	case 2:
		astcenc_quality = lerp(ASTCENC_PRE_FASTEST, ASTCENC_PRE_FAST, 2.0f / 3.0f);
		break;
	case 3:
		astcenc_quality = ASTCENC_PRE_FAST;
		break;
	case 4:
		astcenc_quality = (ASTCENC_PRE_FAST + ASTCENC_PRE_MEDIUM) * .5f;
		break;
	case 5:
		astcenc_quality = ASTCENC_PRE_MEDIUM;
		break;
	case 6:
		astcenc_quality = (ASTCENC_PRE_MEDIUM + ASTCENC_PRE_THOROUGH) * .5f;
		break;
	case 7:
		astcenc_quality = ASTCENC_PRE_THOROUGH;
		break;
	case 8:
		astcenc_quality = (ASTCENC_PRE_THOROUGH + ASTCENC_PRE_VERYTHOROUGH) * .5f;
		break;
	case 9:
		astcenc_quality = ASTCENC_PRE_VERYTHOROUGH;
		break;
	case 10:
	default:
		astcenc_quality = ASTCENC_PRE_EXHAUSTIVE;
		break;
	}

	std::vector< std::vector<astcenc_physblk> > block_candidates(total_blocks);

	astcenc_context* pContext = comp_astc_init_ldr(
		block_width,
		block_height,
		enc_cfg.m_cem_enc_params.m_decode_mode_srgb,
		astcenc_quality,
		num_threads,
		global_cfg.m_force_disable_subsets ? 1 : 3,
		true, enc_cfg.m_cem_enc_params.m_comp_weights,
		global_cfg.m_force_disable_rgb_dual_plane,
		num_blocks_x, &block_candidates);

	if (!pContext)
	{
		assert(0);
		return false;
	}
	
	if (enc_cfg.m_debug_output)
		fmt_debug_printf("Compressing with astcenc\n");

	interval_timer itm;
	itm.start();
		
	const bool comp_status = comp_astc_image_u8(
		pContext,
		(const uint8_t*)orig_img.get_ptr(),
		orig_img.get_width(),
		orig_img.get_height(),
		block_width,
		block_height,
		enc_out.m_packed_phys_blocks,
		job_pool);

	if (enc_cfg.m_debug_output)
		fmt_debug_printf("Total time: {} seconds\n", itm.get_elapsed_secs());

	comp_astc_deinit(pContext);
	pContext = nullptr;

	if (!comp_status)
	{
		fmt_error_printf("comp_astc_image_u8() failed!\n");

		assert(0);
		return false;
	}
			
	if (enc_cfg.m_debug_output)
		fmt_debug_printf("Compressing with astcenc: OK\n");

	enc_out.m_image_block_info.resize(0, 0);
	enc_out.m_image_block_info.resize(num_blocks_x, num_blocks_y);
			
	basist::astc_ldr_t::grid_weight_dct grid_coder;
	if (enc_cfg.m_use_dct)
		grid_coder.init(block_width, block_height);

	basist::astc_ldr_t::fvec dct_temp;

	uint32_t total_suboptimal_cem_encodings = 0;
	
	uint32_t min_candidates = UINT32_MAX, max_candidates = 0, total_candidates = 0;

	uint32_t total_bad_candidates = 0;
		
	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			color_rgba block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
			orig_img.extract_block_clamped(block_pixels, bx * block_width, by * block_height, block_width, block_height);

			const std::vector<astcenc_physblk>& cand_blocks = block_candidates[bx + by * num_blocks_x];
			
			if (!cand_blocks.size())
			{
				assert(0);
				fmt_error_printf("astcenc gave us no candidates for a block.\n");
				return false;
			}

			min_candidates = minimum<uint32_t>((uint32_t)cand_blocks.size(), min_candidates);
			max_candidates = maximum<uint32_t>((uint32_t)cand_blocks.size(), max_candidates);
			total_candidates += (uint32_t)cand_blocks.size();
						
			uint64_t best_sse = UINT64_MAX;
			uint32_t best_cand_index = 0;

			ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);
						
			for (uint32_t cand_index = 0; cand_index < cand_blocks.size(); cand_index++)
			{
				astc_helpers::astc_block* pOrig_packed_block = (astc_helpers::astc_block * )cand_blocks[cand_index].m_bytes;
								
				const uint32_t cur_out_block_index = blk_info.m_out_blocks.size_u32();

				encode_block_output* pEnc_block_output = blk_info.m_out_blocks.enlarge(1);
				pEnc_block_output->clear();

				pEnc_block_output->m_blur_id = BLUR_ID_ASTCENC;

				astc_helpers::log_astc_block& log_blk = pEnc_block_output->m_log_blk;

				if (!astc_helpers::unpack_block(pOrig_packed_block, log_blk, block_width, block_height, true))
				{
					fmt_error_printf("astcenc produced an invalid physical ASTC block!\n");

					assert(0);
					return false;
				}
								
				if (!astc_helpers::is_block_xuastc_ldr(log_blk))
				{
					// Ideally this doesn't happen if we've modified astcenc correctly to ignore non-XUASTC configs.
					if (!total_bad_candidates)
						fmt_printf("WARNING: astcenc produced a non-XUASTC LDR compliant candidate! (1). Ignoring it.\n");
					
					blk_info.m_out_blocks.pop_back();

					total_bad_candidates++;
					continue;
				}

				// We can slam this to disabled because if its otherwise valid XUASTC LDR it doesn't matter. (astcenc does use them, but most of the time they don't change the BISE endpoint/weight levels.)
				// Because we map to valid XUASTC LDR trial mode configs, if slamming this disabled results in an invalid non-suboptimal CEM config, we'll not be able to find the config.
				if (log_blk.m_uses_suboptimal_cem_encoding)
				{
					total_suboptimal_cem_encodings++;
					log_blk.m_uses_suboptimal_cem_encoding = false;
				}

				if (log_blk.m_solid_color_flag_ldr)
				{
					pEnc_block_output->m_trial_mode_index = -1;
				}
				else
				{
					uint32_t cem_to_find = log_blk.m_color_endpoint_modes[0];
					if (cem_to_find == astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET)
						cem_to_find = astc_helpers::CEM_LDR_RGB_DIRECT;
					else if (cem_to_find == astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET)
						cem_to_find = astc_helpers::CEM_LDR_RGBA_DIRECT;

					const int ccs_to_find = log_blk.m_dual_plane ? log_blk.m_color_component_selector : -1;
					BASISU_NOTE_UNUSED(ccs_to_find);

					const basisu::uint_vec& tms = basist::astc_ldr_t::get_tm_candidates(
						basist::astc_ldr_t::g_grouped_encoder_trial_modes[block_size_index],
						cem_to_find, log_blk.m_num_partitions - 1, log_blk.m_dual_plane ? log_blk.m_color_component_selector + 1 : 0,
						basist::astc_ldr_t::calc_grid_size_val(log_blk.m_grid_width, log_blk.m_grid_height, block_width, block_height),
						basist::astc_ldr_t::calc_grid_aniso_val(log_blk.m_grid_width, log_blk.m_grid_height, block_width, block_height));

					uint32_t tm_index = 0, tms_index = 0;
					for (tms_index = 0; tms_index < tms.size(); tms_index++)
					{
						tm_index = tms[tms_index];

						const auto& tm = basist::astc_ldr_t::g_encoder_trial_modes[block_size_index][tm_index];

						assert(tm.m_cem == cem_to_find);
						assert(tm.m_num_parts == log_blk.m_num_partitions);
						assert(tm.m_ccs_index == ccs_to_find);

						if ((tm.m_endpoint_ise_range == log_blk.m_endpoint_ise_range) && (tm.m_weight_ise_range == log_blk.m_weight_ise_range))
						{
							if ((tm.m_grid_width == log_blk.m_grid_width) && (tm.m_grid_height == log_blk.m_grid_height))
							{
								break;
							}
						}
					} // tms_index

					if (tms_index == tms.size())
					{
						// Shouldn't happen if we've properly modified astcenc correctly to use the XUASTC LDR compliant configs.
						if (!total_bad_candidates)
							fmt_printf("WARNING: astcenc produced a non-XUASTC LDR compliant candidate - but we couldn't find this block's config! (2) Ignoring it.\n");
						
						blk_info.m_out_blocks.pop_back();

						total_bad_candidates++;
						continue;
					}

					pEnc_block_output->m_trial_mode_index = basisu::safe_cast_int16(tm_index);

					if (enc_cfg.m_use_dct)
					{
						//const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, new_log_block.m_grid_width, new_log_block.m_grid_height);

						const uint32_t num_planes = (log_blk.m_dual_plane ? 2 : 1);

						uint32_t total_empty_planes = 0;

						for (uint32_t plane_index = 0; plane_index < num_planes; plane_index++)
						{
							basist::astc_ldr_t::dct_syms& syms = pEnc_block_output->m_packed_dct_plane_data[plane_index];

							code_block_weights(grid_coder, enc_cfg.m_base_q, plane_index, log_blk, syms, dct_temp);

							// ensure existing weights get blown away 
							for (uint32_t i = 0; i < (uint32_t)(log_blk.m_grid_width * log_blk.m_grid_height); i++)
								log_blk.m_weights[i * num_planes + plane_index] = 0;

							bool dec_status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, log_blk, nullptr, nullptr, dct_temp, &syms);

							assert(dec_status);
							if (!dec_status)
							{
								error_printf("grid_coder.decode_block_weights() failed!\n");
								return false;
							}

							// check for all-zero AC's
							if (syms.m_coeffs.size() == 1)
							{
								if ((1 + syms.m_coeffs[0].m_num_zeros) == (log_blk.m_grid_width * log_blk.m_grid_height))
								{
									total_empty_planes++;
								}
							}
						}

						if ((log_blk.m_num_partitions == 1) && (total_empty_planes == num_planes))
						{
							// entire block post-quantization is DC only (no non-zero AC), switch to void-extent
							uint32_t sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
							for (uint32_t i = 0; i < total_block_pixels; i++)
							{
								sum_r += block_pixels[i].r;
								sum_g += block_pixels[i].g;
								sum_b += block_pixels[i].b;
								sum_a += block_pixels[i].a;
							}

							sum_r = (sum_r + (total_block_pixels >> 1)) / total_block_pixels;
							sum_g = (sum_g + (total_block_pixels >> 1)) / total_block_pixels;
							sum_b = (sum_b + (total_block_pixels >> 1)) / total_block_pixels;
							sum_a = (sum_a + (total_block_pixels >> 1)) / total_block_pixels;

							astc_helpers::set_ldr_solid_block(log_blk, sum_r, sum_g, sum_b, sum_a);

							pEnc_block_output->m_trial_mode_index = -1;
						}
					}
				}

				uint64_t total_err = 0;

				{
					color_rgba dec_block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

					bool dec_status = astc_helpers::decode_block_xuastc_ldr(log_blk, dec_block_pixels, block_width, block_height,
						enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);

					if (!dec_status)
					{
						// Shouldn't ever happen
						assert(0);
						return false;
					}
										
					for (uint32_t i = 0; i < total_block_pixels; i++)
						total_err += weighted_color_error(dec_block_pixels[i], block_pixels[i], enc_cfg.m_cem_enc_params);

					pEnc_block_output->m_sse = total_err;
				}

				if (total_err < best_sse)
				{
					best_sse = total_err;
					best_cand_index = cur_out_block_index;
				}
												
			} // cand_index

			if (!blk_info.m_out_blocks.size())
			{
				//assert(0);
				error_printf("Couldn't find any compliant astcenc candidates at block {}x{}\n", bx, by);
				return false;
			}

			astc_helpers::astc_block* pFinal_packed_block = &enc_out.m_packed_phys_blocks(bx, by);

			bool pack_status = astc_helpers::pack_astc_block(*pFinal_packed_block, blk_info.m_out_blocks[best_cand_index].m_log_blk);
			if (!pack_status)
			{
				//assert(0);
				error_printf("Failed packing final physical ASTC block at {}x{}\n", bx, by);
				return false;
			}

			blk_info.m_packed_out_block_index = best_cand_index;

			const uint64_t check_wsse = blk_info.m_out_blocks[best_cand_index].m_sse;
			BASISU_NOTE_UNUSED(check_wsse);

			enforce_max_candidate_limit(blk_info, max_candidate_limit);
			
			best_cand_index = blk_info.m_packed_out_block_index; // may have changed

			assert(check_wsse == blk_info.m_out_blocks[best_cand_index].m_sse);
						
		} // bx

	} // by

	if (total_bad_candidates)
		fmt_printf("WARNING: ldr_astc_block_encode_image_astcenc ignored {} invalid (non-XUASTC) candidates.\n", total_bad_candidates);

	if (enc_cfg.m_debug_output)
	{
		display_candidate_statistics(enc_out);

		fmt_debug_printf("Total candidates: {}, Avg candidates per block: {}, Min candidates: {}, Max candidates: {}\n",
			total_candidates, (float)total_candidates / (float)total_blocks, min_candidates, max_candidates);

		if (total_suboptimal_cem_encodings)
			fmt_debug_printf("Total blocks using suboptimal CEM encodings: {}\n", total_suboptimal_cem_encodings);

		if (enc_cfg.m_debug_output)
			fmt_debug_printf("ldr_astc_block_encode_image_astcenc: done\n");
	}

	return true;
}
#endif // BASISU_SUPPORT_ASTCENC

// -1 for solid, -2 for error, >= 0 if found
static int find_tm_index(uint32_t block_width, uint32_t block_height, uint32_t block_size_index, const astc_helpers::log_astc_block &log_blk)
{
	assert(astc_helpers::is_block_xuastc_ldr(log_blk));

	if (log_blk.m_solid_color_flag_ldr)
	{
		// weighted SSE should be zero, the block should always be solid here
		return -1;
	}
		
	uint32_t cem_to_find = log_blk.m_color_endpoint_modes[0];
	if (cem_to_find == astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET)
		cem_to_find = astc_helpers::CEM_LDR_RGB_DIRECT;
	else if (cem_to_find == astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET)
		cem_to_find = astc_helpers::CEM_LDR_RGBA_DIRECT;

	const int ccs_to_find = log_blk.m_dual_plane ? log_blk.m_color_component_selector : -1;
	BASISU_NOTE_UNUSED(ccs_to_find);

	const basisu::uint_vec& tms = basist::astc_ldr_t::get_tm_candidates(
		basist::astc_ldr_t::g_grouped_encoder_trial_modes[block_size_index],
		cem_to_find, log_blk.m_num_partitions - 1, log_blk.m_dual_plane ? log_blk.m_color_component_selector + 1 : 0,
		basist::astc_ldr_t::calc_grid_size_val(log_blk.m_grid_width, log_blk.m_grid_height, block_width, block_height),
		basist::astc_ldr_t::calc_grid_aniso_val(log_blk.m_grid_width, log_blk.m_grid_height, block_width, block_height));

	uint32_t tm_index = 0, tms_index = 0;
	for (tms_index = 0; tms_index < tms.size(); tms_index++)
	{
		tm_index = tms[tms_index];

		const auto& tm = basist::astc_ldr_t::g_encoder_trial_modes[block_size_index][tm_index];

		assert(tm.m_cem == cem_to_find);
		assert(tm.m_num_parts == log_blk.m_num_partitions);
		assert(tm.m_ccs_index == ccs_to_find);

		if ((tm.m_endpoint_ise_range == log_blk.m_endpoint_ise_range) && (tm.m_weight_ise_range == log_blk.m_weight_ise_range))
		{
			if ((tm.m_grid_width == log_blk.m_grid_width) && (tm.m_grid_height == log_blk.m_grid_height))
			{
				break;
			}
		}
	} // tms_index

	if (tms_index == tms.size())
	{
		assert(0);
		return -2;
	}

	return tm_index;
}

static bool ldr_astc_block_encode_image_astcf(
	const image& orig_img,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	const astc_ldr_encode_config& global_cfg,
	ldr_astc_block_encode_image_output& enc_out,
	uint32_t max_candidate_limit)
{
	if (enc_cfg.m_debug_output)
		fmt_debug_printf("ldr_astc_block_encode_image_astcf:\n");

	const uint32_t block_width = enc_cfg.m_block_width, block_height = enc_cfg.m_block_height;

	const int block_dim_index = astc_helpers::find_astc_block_size_index(block_width, block_height);
	assert((block_dim_index >= 0) && (block_dim_index < (int)astc_helpers::NUM_ASTC_BLOCK_SIZES));

	const uint32_t width = orig_img.get_width(), height = orig_img.get_height();
	const uint32_t total_pixels = width * height;
	const uint32_t total_block_pixels = enc_cfg.m_block_width * enc_cfg.m_block_height;
	const uint32_t num_blocks_x = orig_img.get_block_width(enc_cfg.m_block_width);
	const uint32_t num_blocks_y = orig_img.get_block_height(enc_cfg.m_block_height);
	const uint32_t total_blocks = num_blocks_x * num_blocks_y;
	const bool has_alpha = orig_img.has_alpha();

	if (enc_cfg.m_debug_output)
	{
		fmt_debug_printf("\nASTC base bitrate: {3.3} bpp\n", 128.0f / (float)(enc_cfg.m_block_width * enc_cfg.m_block_height));
		
		fmt_debug_printf("ASTC block size: {}x{}\n", enc_cfg.m_block_width, enc_cfg.m_block_height);
		
		fmt_debug_printf("Image has alpha: {}\n", has_alpha);

		fmt_debug_printf("max_candidate_limit: {}\n", max_candidate_limit);;
	}

	// We don't use this here, but the supercompressors use these tables.
	// TODO: The transcoder already creates all this stuff for each block size.
	astc_ldr::partitions_data* pPart_data_p2 = &enc_out.m_part_data_p2;
	pPart_data_p2->init(2, enc_cfg.m_block_width, enc_cfg.m_block_height, BASISU_USE_LSH2 == 0, BASISU_USE_LSH2 != 0);

	astc_ldr::partitions_data* pPart_data_p3 = &enc_out.m_part_data_p3;
	pPart_data_p3->init(3, enc_cfg.m_block_width, enc_cfg.m_block_height, BASISU_USE_LSH3 == 0, BASISU_USE_LSH3 != 0);

	basisu::vector<basist::astc_ldr_t::trial_mode>& encoder_trial_modes = enc_out.m_encoder_trial_modes;
	encoder_trial_modes.reserve(4096);

	basist::astc_ldr_t::grouped_trial_modes& grouped_encoder_trial_modes = enc_out.m_grouped_encoder_trial_modes;
	basist::astc_ldr_t::create_encoder_trial_modes_table(block_width, block_height, encoder_trial_modes, grouped_encoder_trial_modes, enc_cfg.m_debug_output, false);

	vector2D<astc_helpers::astc_block>& packed_blocks = enc_out.m_packed_phys_blocks;
	packed_blocks.resize(num_blocks_x, num_blocks_y);
	memset(packed_blocks.get_ptr(), 0, packed_blocks.size_in_bytes());

	enc_out.m_image_block_info.resize(0, 0);
	enc_out.m_image_block_info.resize(num_blocks_x, num_blocks_y);
		
	uint32_t max_subsets = 1;
	bool use_subsets = false;
	float var_thresh_2subsets = squaref(6.0f);
	float var_thresh_3subsets = squaref(6.0f);
	uint32_t num_subset_carriers = 1, num_subset_pats = 1;
	uint32_t dot_thresh_fract_index_2subsets = 0;
		
	bool weight_polishing = (global_cfg.m_effort_level >= 2);
	uint32_t num_candidates = 1;

	const float feffort_level = global_cfg.m_effort_level * (1.0f / 10.0f);

#if 0
	if (block_width * block_height <= 25)
	{ 
		// tiny blocks make the DCT related candidate eval quite slow
		if (has_alpha)
			num_candidates = clamp<int>((int)std::round(lerp(8.0f, 24.0f, feffort_level)), 1, 64);
		else
			num_candidates = clamp<int>((int)std::round(lerp(1.0f, 16.0f, feffort_level)), 1, 64);
	}
	else
#endif
	{
		if (has_alpha)
			num_candidates = clamp<int>((int)std::round(lerp(16.0f, 48.0f, feffort_level)), 1, 64);
		else
			num_candidates = clamp<int>((int)std::round(lerp(1.0f, 48.0f, feffort_level)), 1, 64);
	}

	if (global_cfg.m_effort_level)
	{
		if (block_width * block_height <= 25)
		{
			num_subset_carriers = clamp<int>((int)std::round(lerp(1.0f, 2.0f, feffort_level)), 1, 3);
			num_subset_pats = clamp<int>((int)std::round(lerp(1.0f, 6.0f, feffort_level)), 1, 16);
		}
		else
		{
			num_subset_carriers = clamp<int>((int)std::round(lerp(1.0f, 3.0f, feffort_level)), 1, 3);
			num_subset_pats = clamp<int>((int)std::round(lerp(1.0f, 16.0f, feffort_level)), 1, 16);
		}
		
		if (global_cfg.m_effort_level == 8)
			dot_thresh_fract_index_2subsets = 1;
		else if (global_cfg.m_effort_level == 9)
			dot_thresh_fract_index_2subsets = 2;
		else if (global_cfg.m_effort_level == 10)
			dot_thresh_fract_index_2subsets = 3;

		use_subsets = (num_subset_carriers > 0) && (num_subset_pats > 0);
		if (use_subsets)
		{
			max_subsets = (global_cfg.m_effort_level > 1) ? 3 : 2;
		}
	}

	if (global_cfg.m_force_disable_subsets)
		use_subsets = false;
				
	basist::astc_ldr_t::grid_weight_dct grid_coder;
	if (enc_cfg.m_use_dct)
		grid_coder.init(block_width, block_height);

	assert(enc_cfg.m_pJob_pool);
	job_pool& job_pool = *enc_cfg.m_pJob_pool;
	const uint32_t num_threads = (uint32_t)job_pool.get_total_threads();

	std::atomic<int> cur_row;
	cur_row.store(0);

	std::atomic<bool> encoder_failed_flag;
	encoder_failed_flag.store(false);
		
	astc_ldrf::subset_enc_context ctx;
	
	bool status = astc_ldrf::init_single_subset_context(
		ctx,
		block_width, block_height,
		enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::decode_mode::cDecodeModeSRGB8 : astc_helpers::decode_mode::cDecodeModeLDR8,
		enc_cfg.m_cem_enc_params.m_comp_weights,
		num_candidates, 2, global_cfg.m_force_disable_rgb_dual_plane, has_alpha, weight_polishing);

	if (global_cfg.m_effort_level <= 2)
	{
		// faster/weaker subset encoding
		ctx.m_use_method2 = false;
	}

	if (global_cfg.m_effort_level <= 6)
	{
		// both very rarely worth the effort
		ctx.m_higher_effort_bc = false;
		ctx.m_try_base_ofs = false;
	}

	if ((status) && (use_subsets))
	{
		status = astc_ldrf::init_multi_subset_context(ctx, max_subsets, num_subset_carriers, num_subset_pats, 
			var_thresh_2subsets, dot_thresh_fract_index_2subsets, var_thresh_3subsets, 
			pPart_data_p2, pPart_data_p3);
	}

	if (!status)
	{
		fmt_error_printf("astc_ldrf::init_single_subset_context() failed!\n");
		return false;
	}

	if (enc_cfg.m_debug_output)
	{
		fmt_debug_printf("num_candidates: {}, use subsets: {}, num_2subset_carriers: {}, num_2subset_pats: {}, 2 subsets threshold var: {}, dot_thresh_fract_index: {}, 3 subsets threshold var: {}, use subsets method1: {}, use subsets method2: {}, higher effort BC: {}, try base ofs: {}, max subsets: {}:\n",
			num_candidates, use_subsets, num_subset_carriers, num_subset_pats, 
			var_thresh_2subsets, dot_thresh_fract_index_2subsets, var_thresh_3subsets,
			ctx.m_use_method1, ctx.m_use_method2, ctx.m_higher_effort_bc, ctx.m_try_base_ofs, ctx.m_max_subsets);
	}
					
	for (uint32_t job_index = 0; job_index < num_threads; job_index++)
	{
		job_pool.add_job([job_index, num_threads, has_alpha, width, height, total_pixels, num_blocks_x, num_blocks_y, block_width, block_height, block_dim_index, total_blocks, total_block_pixels,
			num_candidates, use_subsets, weight_polishing,
			&cur_row, &encoder_failed_flag, &ctx, max_candidate_limit,
			&orig_img, &enc_cfg, &encoder_trial_modes, &grid_coder, &grouped_encoder_trial_modes, &enc_out]
			{
				BASISU_NOTE_UNUSED(job_index); BASISU_NOTE_UNUSED(num_threads); BASISU_NOTE_UNUSED(has_alpha); BASISU_NOTE_UNUSED(width); BASISU_NOTE_UNUSED(height); BASISU_NOTE_UNUSED(total_pixels); BASISU_NOTE_UNUSED(total_blocks); BASISU_NOTE_UNUSED(weight_polishing); BASISU_NOTE_UNUSED(encoder_trial_modes); BASISU_NOTE_UNUSED(grouped_encoder_trial_modes);
				if (encoder_failed_flag)
					return;

				basist::astc_ldr_t::fvec dct_temp;

				astc_ldrf::astc_lblock_vec all_candidates;
				all_candidates.reserve(num_candidates);

				astc_ldrf::subset_enc_thread_context thread_ctx;

				uint_vec sorted_indices;
				sorted_indices.reserve(max_candidate_limit);
								
				for (; ; )
				{
					if (encoder_failed_flag)
						return;

					const uint32_t by = cur_row.fetch_add(1);
					if (by >= num_blocks_y)
						break;

					for (uint32_t bx = 0; bx < num_blocks_x; bx++)
					{
						if (encoder_failed_flag)
							return;

						ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

						color_rgba block_pixels[astc_helpers::MAX_BLOCK_PIXELS];
						orig_img.extract_block_clamped(block_pixels, bx * block_width, by * block_height, block_width, block_height);
												
						all_candidates.resize(0);
												
						astc_helpers::log_astc_block best_lblock;
						if (use_subsets)
						{
							astc_ldrf::compress_block_subsets(ctx, thread_ctx, (const uint8_t*)block_pixels, best_lblock, &all_candidates);
						}
						else
						{
							astc_ldrf::compress_single_subset(ctx, (const uint8_t*)block_pixels, best_lblock, &all_candidates, false);
						}

						if (encoder_failed_flag)
							return;
						
						if (!all_candidates.size())
						{
							fmt_error_printf("compress_block: returned no candidates!\n");

							encoder_failed_flag.store(true);
							return;
						}

						uint64_t best_cand_err = UINT64_MAX;
						uint32_t best_cand_index = 0;

						for (uint32_t cand_index = 0; cand_index < all_candidates.size(); cand_index++)
						{
							const astc_helpers::log_astc_block& candidate_log_blk = all_candidates[cand_index];
														
							encode_block_output* pEnc_block_output = blk_info.m_out_blocks.enlarge(1);
							pEnc_block_output->clear();

							pEnc_block_output->m_blur_id = BLUR_ID_ASTCF;
							
							astc_helpers::log_astc_block& log_blk = pEnc_block_output->m_log_blk;
							
							log_blk = candidate_log_blk;
							astc_ldrf::convert_rank_lblock_to_ise(log_blk);
							
							int tm_index = find_tm_index(block_width, block_height, block_dim_index, log_blk);
							if (tm_index == -2)
							{
								fmt_error_printf("compress_block: invalid candidate!\n");

								encoder_failed_flag.store(true);
								return;
							}
														
							if ((tm_index >= 0) && (enc_cfg.m_use_dct))
							{
								const uint32_t num_planes = (log_blk.m_dual_plane ? 2 : 1);

								uint32_t total_empty_planes = 0;

								for (uint32_t plane_index = 0; plane_index < num_planes; plane_index++)
								{
									basist::astc_ldr_t::dct_syms& syms = pEnc_block_output->m_packed_dct_plane_data[plane_index];

									code_block_weights(grid_coder, enc_cfg.m_base_q, plane_index, log_blk, syms, dct_temp);

									// ensure existing weights get blown away 
									for (uint32_t i = 0; i < (uint32_t)(log_blk.m_grid_width * log_blk.m_grid_height); i++)
										log_blk.m_weights[i * num_planes + plane_index] = 0;

									bool dec_status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, log_blk, nullptr, nullptr, dct_temp, &syms);

									assert(dec_status);
									if (!dec_status)
									{
										error_printf("grid_coder.decode_block_weights() failed!\n");
										encoder_failed_flag.store(true);
										return;
									}

									// check for all-zero AC's
									if (syms.m_coeffs.size() == 1)
									{
										if ((1 + syms.m_coeffs[0].m_num_zeros) == (log_blk.m_grid_width * log_blk.m_grid_height))
										{
											total_empty_planes++;
										}
									}
								}

								if ((log_blk.m_num_partitions == 1) && (total_empty_planes == num_planes))
								{
									// entire block post-quantization is DC only (no non-zero AC), switch to void-extent
									uint32_t sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
									for (uint32_t i = 0; i < total_block_pixels; i++)
									{
										sum_r += block_pixels[i].r;
										sum_g += block_pixels[i].g;
										sum_b += block_pixels[i].b;
										sum_a += block_pixels[i].a;
									}

									const uint32_t round = total_block_pixels >> 1;
									sum_r = (sum_r + round) / total_block_pixels;
									sum_g = (sum_g + round) / total_block_pixels;
									sum_b = (sum_b + round) / total_block_pixels;
									sum_a = (sum_a + round) / total_block_pixels;

									astc_helpers::set_ldr_solid_block(log_blk, sum_r, sum_g, sum_b, sum_a);

									tm_index = -1;
								}

							} // if (enc_cfg.m_use_dct)

							pEnc_block_output->m_trial_mode_index = basisu::safe_cast_int16(tm_index);

							// unpack the block and compute actual WSSE
							{
								color_rgba dec_block_pixels[astc_helpers::MAX_BLOCK_PIXELS];

								bool dec_status = astc_helpers::decode_block_xuastc_ldr(log_blk, dec_block_pixels, block_width, block_height,
									enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);

								if (!dec_status)
								{
									// Shouldn't ever happen
									assert(0);
									
									error_printf("decode_block_xuastc_ldr() failed!\n");

									encoder_failed_flag.store(true);
									return;
								}

								uint64_t total_err = 0;

								for (uint32_t i = 0; i < total_block_pixels; i++)
									total_err += weighted_color_error(dec_block_pixels[i], block_pixels[i], enc_cfg.m_cem_enc_params);

								pEnc_block_output->m_sse = total_err;

								if (total_err < best_cand_err)
								{
									best_cand_err = total_err;
									best_cand_index = cand_index;
								}
							}
														
						} // cand_index

						if (blk_info.m_out_blocks.size() > max_candidate_limit)
						{
							const uint64_t check_wsse = blk_info.m_out_blocks[best_cand_index].m_sse;
							BASISU_NOTE_UNUSED(check_wsse);

							// There were just too many candidates returned - we'll risk running out of RAM in WASM. So sort them and just keep the top X.
							sorted_indices.resize(blk_info.m_out_blocks.size_u32());

							for (uint32_t i = 0; i < sorted_indices.size(); i++)
								sorted_indices[i] = i;

							std::sort(sorted_indices.begin(), sorted_indices.end(),
								[&blk_info](const uint32_t a, const uint32_t b) 
								{
									if (blk_info.m_out_blocks[a].m_sse < blk_info.m_out_blocks[b].m_sse)
										return true;
									return false;
								}
							);

							basisu::vector<encode_block_output> shrunk_out_blocks(max_candidate_limit);
							for (uint32_t i = 0; i < max_candidate_limit; i++)
								shrunk_out_blocks[i] = blk_info.m_out_blocks[sorted_indices[i]];

							blk_info.m_out_blocks.swap(shrunk_out_blocks);
							best_cand_index = 0;
																												
							assert(check_wsse == blk_info.m_out_blocks[best_cand_index].m_sse);
						}

						blk_info.m_packed_out_block_index = best_cand_index;

						const astc_helpers::log_astc_block& best_log_blk = blk_info.m_out_blocks[best_cand_index].m_log_blk;

						bool pack_status = astc_helpers::pack_astc_block(enc_out.m_packed_phys_blocks(bx, by), best_log_blk);
						if (!pack_status)
						{
							assert(0);
							encoder_failed_flag.store(true);
							return;
						}
					} // bx

				} // for ( ; ; )

			} // lambda function
		);

	} // job_index

	job_pool.wait_for_all();
		
	if (encoder_failed_flag)
	{
		fmt_error_printf("ldr_astc_block_encode_image_astcf: Main compressor block loop failed!\n");
		return false;
	}
		
	if (enc_cfg.m_debug_output)
	{
		display_candidate_statistics(enc_out);
				
		fmt_debug_printf("ldr_astc_block_encode_image_astcf: OK\n");
	}

	return true;
}

const uint_vec& separate_tm_index(uint32_t block_width, uint32_t block_height, const basist::astc_ldr_t::grouped_trial_modes& grouped_enc_trial_modes, const basist::astc_ldr_t::trial_mode& tm,
	uint32_t& cem_index, uint32_t& subset_index, uint32_t& ccs_index, uint32_t& grid_size, uint32_t& grid_aniso)
{
	cem_index = tm.m_cem;
	assert(cem_index < basist::astc_ldr_t::OTM_NUM_CEMS);

	subset_index = tm.m_num_parts - 1;
	assert(subset_index < basist::astc_ldr_t::OTM_NUM_SUBSETS);

	ccs_index = tm.m_ccs_index + 1;
	assert(ccs_index < basist::astc_ldr_t::OTM_NUM_CCS);

	grid_size = (tm.m_grid_width >= (block_width - 1)) && (tm.m_grid_height >= (block_height - 1));
	grid_aniso = basist::astc_ldr_t::calc_grid_aniso_val(tm.m_grid_width, tm.m_grid_height, block_width, block_height);
		
	const uint_vec& modes = grouped_enc_trial_modes.m_tm_groups[cem_index][subset_index][ccs_index][grid_size][grid_aniso];
	return modes;
}

static bool compare_log_block_configs(const astc_helpers::log_astc_block& trial_log_blk, const astc_helpers::log_astc_block& neighbor_log_blk)
{
	assert(!trial_log_blk.m_solid_color_flag_ldr);

	if (neighbor_log_blk.m_solid_color_flag_ldr)
		return false;

	if ((trial_log_blk.m_color_endpoint_modes[0] == neighbor_log_blk.m_color_endpoint_modes[0]) &&
		(trial_log_blk.m_dual_plane == neighbor_log_blk.m_dual_plane) && (trial_log_blk.m_color_component_selector == neighbor_log_blk.m_color_component_selector) &&
		(trial_log_blk.m_num_partitions == neighbor_log_blk.m_num_partitions) && (trial_log_blk.m_partition_id == neighbor_log_blk.m_partition_id) &&
		(trial_log_blk.m_grid_width == neighbor_log_blk.m_grid_width) && (trial_log_blk.m_grid_height == neighbor_log_blk.m_grid_height) &&
		(trial_log_blk.m_endpoint_ise_range == neighbor_log_blk.m_endpoint_ise_range) && (trial_log_blk.m_weight_ise_range == neighbor_log_blk.m_weight_ise_range))
	{
		return true;
	}

	return false;
}

static bool compare_log_block_configs_and_endpoints(const astc_helpers::log_astc_block& trial_log_blk, const astc_helpers::log_astc_block& neighbor_log_blk)
{
	if (!compare_log_block_configs(trial_log_blk, neighbor_log_blk))
		return false;

	const uint32_t total_endpoint_vals = trial_log_blk.m_num_partitions * astc_helpers::get_num_cem_values(trial_log_blk.m_color_endpoint_modes[0]);
	if (memcmp(trial_log_blk.m_endpoints, neighbor_log_blk.m_endpoints, total_endpoint_vals) == 0)
		return true;

	return false;
}

static bool compare_log_blocks_for_equality(const astc_helpers::log_astc_block& trial_log_blk, const astc_helpers::log_astc_block& neighbor_log_blk)
{
	if (trial_log_blk.m_solid_color_flag_ldr)
	{
		if (!neighbor_log_blk.m_solid_color_flag_ldr)
			return false;

		for (uint32_t i = 0; i < 4; i++)
			if (trial_log_blk.m_solid_color[i] != neighbor_log_blk.m_solid_color[i])
				return false;

		return true;
	}
	else if (neighbor_log_blk.m_solid_color_flag_ldr)
	{
		return false;
	}

	assert(!trial_log_blk.m_solid_color_flag_ldr && !neighbor_log_blk.m_solid_color_flag_ldr);

	if ((trial_log_blk.m_color_endpoint_modes[0] == neighbor_log_blk.m_color_endpoint_modes[0]) &&
		(trial_log_blk.m_dual_plane == neighbor_log_blk.m_dual_plane) && (trial_log_blk.m_color_component_selector == neighbor_log_blk.m_color_component_selector) &&
		(trial_log_blk.m_num_partitions == neighbor_log_blk.m_num_partitions) && (trial_log_blk.m_partition_id == neighbor_log_blk.m_partition_id) &&
		(trial_log_blk.m_grid_width == neighbor_log_blk.m_grid_width) && (trial_log_blk.m_grid_height == neighbor_log_blk.m_grid_height) &&
		(trial_log_blk.m_endpoint_ise_range == neighbor_log_blk.m_endpoint_ise_range) && (trial_log_blk.m_weight_ise_range == neighbor_log_blk.m_weight_ise_range))
	{
		const uint32_t total_endpoint_vals = trial_log_blk.m_num_partitions * astc_helpers::get_num_cem_values(trial_log_blk.m_color_endpoint_modes[0]);
		if (memcmp(trial_log_blk.m_endpoints, neighbor_log_blk.m_endpoints, total_endpoint_vals) == 0)
		{
			const uint32_t total_weights = (trial_log_blk.m_dual_plane ? 2 : 1) * (trial_log_blk.m_grid_width * trial_log_blk.m_grid_height);
			return memcmp(trial_log_blk.m_weights, neighbor_log_blk.m_weights, total_weights) == 0;
		}
	}

	return false;
}

static void configure_encoder_effort_level(int level, ldr_astc_block_encode_image_high_level_config& cfg)
{
	switch (level)
	{
	case 10:
	{
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = true;

		cfg.m_encode_trial_early_out_thresh = 0.01f;
		cfg.m_encode_trial_subsets_early_out_thresh = 0.01f;

		cfg.m_force_all_dual_plane_chan_evals = true;
		cfg.m_filter_by_pca_angles_flag = false;

		cfg.m_superbucket_max_to_retain[0] = 256;
		cfg.m_superbucket_max_to_retain[1] = 256;
		cfg.m_superbucket_max_to_retain[2] = 256;

		cfg.m_base_parts2 = 128;
		cfg.m_base_parts3 = 128;
		cfg.m_part2_fraction_to_keep = 1;
		cfg.m_part3_fraction_to_keep = 1;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 128;
		cfg.m_final_shortlist_max_size[1] = 128;
		cfg.m_final_shortlist_max_size[2] = 128;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .075f;
		cfg.m_superbucket_max_to_retain_p2[0] = 1024;
		cfg.m_superbucket_max_to_retain_p2[1] = 1024;
		cfg.m_superbucket_max_to_retain_p2[2] = 1024;
		cfg.m_final_shortlist_max_size_p2[0] = 256;
		cfg.m_final_shortlist_max_size_p2[1] = 256;
		cfg.m_final_shortlist_max_size_p2[2] = 256;
		cfg.m_base_parts2_p2 = 128;
		cfg.m_base_parts3_p2 = 128;
		cfg.m_force_all_dp_chans_p2 = true;
		cfg.m_filter_by_pca_angles_flag_p2 = false;

		cfg.m_final_encode_always_try_rgb_direct = true;

		cfg.m_early_stop_wpsnr = 90.0f;
		cfg.m_early_stop2_wpsnr = 90.0f;
		cfg.m_grid_hv_filtering = false;
		cfg.m_low_freq_block_filtering = false;

		cfg.m_cem_enc_params.m_use_exhaustive_weight_eval = true;

		break;
	}
	case 9:
	{
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = true;

		cfg.m_encode_trial_early_out_thresh = 0.01f;
		cfg.m_encode_trial_subsets_early_out_thresh = 0.01f;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 16;
		cfg.m_superbucket_max_to_retain[1] = 32;
		cfg.m_superbucket_max_to_retain[2] = 64;

		cfg.m_base_parts2 = 32;
		cfg.m_base_parts3 = 32;
		cfg.m_part2_fraction_to_keep = 2;
		cfg.m_part3_fraction_to_keep = 2;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 16;
		cfg.m_final_shortlist_max_size[1] = 32;
		cfg.m_final_shortlist_max_size[2] = 64;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .15f;
		cfg.m_superbucket_max_to_retain_p2[0] = 16;
		cfg.m_superbucket_max_to_retain_p2[1] = 64;
		cfg.m_superbucket_max_to_retain_p2[2] = 256;
		cfg.m_final_shortlist_max_size_p2[0] = 32;
		cfg.m_final_shortlist_max_size_p2[1] = 64;
		cfg.m_final_shortlist_max_size_p2[2] = 128;
		cfg.m_base_parts2_p2 = 64;
		cfg.m_base_parts3_p2 = 64;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = false;

		cfg.m_final_encode_always_try_rgb_direct = false;

		cfg.m_early_stop_wpsnr = 75.0f;
		cfg.m_early_stop2_wpsnr = 70.0f;

		cfg.m_cem_enc_params.m_use_exhaustive_weight_eval = true;
								
		break;
	}
	case 8:
	{
#if 0
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = true;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 4;
		cfg.m_superbucket_max_to_retain[1] = 8;
		cfg.m_superbucket_max_to_retain[2] = 16;

		cfg.m_base_parts2 = 16;
		cfg.m_base_parts3 = 16;
		cfg.m_part2_fraction_to_keep = 2;
		cfg.m_part3_fraction_to_keep = 2;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 3;
		cfg.m_final_shortlist_max_size[1] = 8;
		cfg.m_final_shortlist_max_size[2] = 12;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .075f;
		cfg.m_superbucket_max_to_retain_p2[0] = 16;
		cfg.m_superbucket_max_to_retain_p2[1] = 64;
		cfg.m_superbucket_max_to_retain_p2[2] = 256;
		cfg.m_final_shortlist_max_size_p2[0] = 8;
		cfg.m_final_shortlist_max_size_p2[1] = 16;
		cfg.m_final_shortlist_max_size_p2[2] = 32;
		cfg.m_base_parts2_p2 = 64;
		cfg.m_base_parts3_p2 = 64;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = false;

		cfg.m_final_encode_always_try_rgb_direct = false;

		cfg.m_early_stop_wpsnr = 75.0f;
		cfg.m_early_stop2_wpsnr = 70.0f;
#endif

		// old 9
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = true;

		cfg.m_encode_trial_early_out_thresh = 0.01f;
		cfg.m_encode_trial_subsets_early_out_thresh = 0.01f;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 8;
		cfg.m_superbucket_max_to_retain[1] = 16;
		cfg.m_superbucket_max_to_retain[2] = 32;

		cfg.m_base_parts2 = 32;
		cfg.m_base_parts3 = 32;
		cfg.m_part2_fraction_to_keep = 2;
		cfg.m_part3_fraction_to_keep = 2;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 4;
		cfg.m_final_shortlist_max_size[1] = 12;
		cfg.m_final_shortlist_max_size[2] = 24;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .15f;
		cfg.m_superbucket_max_to_retain_p2[0] = 16;
		cfg.m_superbucket_max_to_retain_p2[1] = 64;
		cfg.m_superbucket_max_to_retain_p2[2] = 256;
		cfg.m_final_shortlist_max_size_p2[0] = 8;
		cfg.m_final_shortlist_max_size_p2[1] = 16;
		cfg.m_final_shortlist_max_size_p2[2] = 32;
		cfg.m_base_parts2_p2 = 64;
		cfg.m_base_parts3_p2 = 64;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = false;

		cfg.m_final_encode_always_try_rgb_direct = false;

		cfg.m_early_stop_wpsnr = 75.0f;
		cfg.m_early_stop2_wpsnr = 70.0f;

		cfg.m_cem_enc_params.m_use_exhaustive_weight_eval = true;
						
		//cfg.m_second_pass_total_weight_refine_passes = 0;

		break;
	}
	case 7:
	{
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = true;

		cfg.m_disable_rgb_dual_plane = false;
		cfg.m_strong_dp_decorr_thresh_rgb = .9f;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 3;
		cfg.m_superbucket_max_to_retain[1] = 7;
		cfg.m_superbucket_max_to_retain[2] = 12;

		cfg.m_base_parts2 = 12;
		cfg.m_base_parts3 = 12;
		cfg.m_part2_fraction_to_keep = 2;
		cfg.m_part3_fraction_to_keep = 2;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 2;
		cfg.m_final_shortlist_max_size[1] = 4;
		cfg.m_final_shortlist_max_size[2] = 8;

		cfg.m_gradient_descent_flag = true;
		cfg.m_polish_weights_flag = true;
		cfg.m_qcd_enabled_flag = true;

		cfg.m_bucket_pruning_passes = false;
		cfg.m_cem_enc_params.m_max_ls_passes = 1;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .075f;
		cfg.m_superbucket_max_to_retain_p2[0] = 4;
		cfg.m_superbucket_max_to_retain_p2[1] = 16;
		cfg.m_superbucket_max_to_retain_p2[2] = 32;
		cfg.m_final_shortlist_max_size_p2[0] = 4;
		cfg.m_final_shortlist_max_size_p2[1] = 16;
		cfg.m_final_shortlist_max_size_p2[2] = 32;
		cfg.m_base_parts2_p2 = 32;
		cfg.m_base_parts3_p2 = 8;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = true;

		cfg.m_early_stop_wpsnr = 65.0f;
		cfg.m_early_stop2_wpsnr = 60.0f; 
		break;
	}
	case 6:
	{
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = true;

		cfg.m_disable_rgb_dual_plane = false;
		cfg.m_strong_dp_decorr_thresh_rgb = .75f;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 2;
		cfg.m_superbucket_max_to_retain[1] = 5;
		cfg.m_superbucket_max_to_retain[2] = 10;

		cfg.m_base_parts2 = 12;
		cfg.m_base_parts3 = 10;
		cfg.m_part2_fraction_to_keep = 2;
		cfg.m_part3_fraction_to_keep = 2;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 1;
		cfg.m_final_shortlist_max_size[1] = 4;
		cfg.m_final_shortlist_max_size[2] = 8;

		cfg.m_gradient_descent_flag = true;
		cfg.m_polish_weights_flag = true;
		cfg.m_qcd_enabled_flag = true;

		cfg.m_bucket_pruning_passes = false;
		cfg.m_cem_enc_params.m_max_ls_passes = 1;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .075f;
		cfg.m_superbucket_max_to_retain_p2[0] = 2;
		cfg.m_superbucket_max_to_retain_p2[1] = 8;
		cfg.m_superbucket_max_to_retain_p2[2] = 16;
		cfg.m_final_shortlist_max_size_p2[0] = 2;
		cfg.m_final_shortlist_max_size_p2[1] = 8;
		cfg.m_final_shortlist_max_size_p2[2] = 16;
		cfg.m_base_parts2_p2 = 32;
		cfg.m_base_parts3_p2 = 8;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = true;

		cfg.m_early_stop_wpsnr = 65.0f;
		cfg.m_early_stop2_wpsnr = 60.0f;
		break;
	}
	case 5:
	{
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = true;

		cfg.m_disable_rgb_dual_plane = false;
		cfg.m_strong_dp_decorr_thresh_rgb = .75f;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 1;
		cfg.m_superbucket_max_to_retain[1] = 4;
		cfg.m_superbucket_max_to_retain[2] = 8;

		cfg.m_base_parts2 = 12;
		cfg.m_base_parts3 = 8;
		cfg.m_part2_fraction_to_keep = 2;
		cfg.m_part3_fraction_to_keep = 2;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 1;
		cfg.m_final_shortlist_max_size[1] = 4;
		cfg.m_final_shortlist_max_size[2] = 8;

		cfg.m_gradient_descent_flag = true;
		cfg.m_polish_weights_flag = true;
		cfg.m_qcd_enabled_flag = false;

		cfg.m_bucket_pruning_passes = false;
		cfg.m_cem_enc_params.m_max_ls_passes = 1;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .075f;
		cfg.m_superbucket_max_to_retain_p2[0] = 2;
		cfg.m_superbucket_max_to_retain_p2[1] = 8;
		cfg.m_superbucket_max_to_retain_p2[2] = 16;
		cfg.m_final_shortlist_max_size_p2[0] = 2;
		cfg.m_final_shortlist_max_size_p2[1] = 8;
		cfg.m_final_shortlist_max_size_p2[2] = 16;
		cfg.m_base_parts2_p2 = 32;
		cfg.m_base_parts3_p2 = 8;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = true;

		cfg.m_early_stop_wpsnr = 65.0f;
		cfg.m_early_stop2_wpsnr = 60.0f;
		break;
	}
	case 4:
	{
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = true;

		cfg.m_disable_rgb_dual_plane = false;
		cfg.m_strong_dp_decorr_thresh_rgb = .75f;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 1;
		cfg.m_superbucket_max_to_retain[1] = 4;
		cfg.m_superbucket_max_to_retain[2] = 8;

		cfg.m_base_parts2 = 8;
		cfg.m_base_parts3 = 4;
		cfg.m_part2_fraction_to_keep = 2;
		cfg.m_part3_fraction_to_keep = 2;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 1;
		cfg.m_final_shortlist_max_size[1] = 4;
		cfg.m_final_shortlist_max_size[2] = 8;

		cfg.m_gradient_descent_flag = true;
		cfg.m_polish_weights_flag = true;
		cfg.m_qcd_enabled_flag = false;

		cfg.m_bucket_pruning_passes = false;
		cfg.m_cem_enc_params.m_max_ls_passes = 1;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .075f;
		cfg.m_superbucket_max_to_retain_p2[0] = 2;
		cfg.m_superbucket_max_to_retain_p2[1] = 8;
		cfg.m_superbucket_max_to_retain_p2[2] = 16;
		cfg.m_final_shortlist_max_size_p2[0] = 2;
		cfg.m_final_shortlist_max_size_p2[1] = 8;
		cfg.m_final_shortlist_max_size_p2[2] = 16;
		cfg.m_base_parts2_p2 = 32;
		cfg.m_base_parts3_p2 = 8;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = true;

		cfg.m_early_stop_wpsnr = 65.0f;
		cfg.m_early_stop2_wpsnr = 60.0f;
		break;
	}
	default:
	case 3:
	{
		cfg.m_second_superpass_refinement = true;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = false;

		cfg.m_disable_rgb_dual_plane = false;
		cfg.m_strong_dp_decorr_thresh_rgb = .75f;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 1;
		cfg.m_superbucket_max_to_retain[1] = 4;
		cfg.m_superbucket_max_to_retain[2] = 8;

		cfg.m_base_parts2 = 4;
		cfg.m_base_parts3 = 2;
		cfg.m_part2_fraction_to_keep = 2;
		cfg.m_part3_fraction_to_keep = 2;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 1;
		cfg.m_final_shortlist_max_size[1] = 4;
		cfg.m_final_shortlist_max_size[2] = 8;

		cfg.m_gradient_descent_flag = true;
		cfg.m_polish_weights_flag = true;
		cfg.m_qcd_enabled_flag = false;

		cfg.m_bucket_pruning_passes = false;
		cfg.m_cem_enc_params.m_max_ls_passes = 1;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .075f;
		cfg.m_superbucket_max_to_retain_p2[0] = 2;
		cfg.m_superbucket_max_to_retain_p2[1] = 8;
		cfg.m_superbucket_max_to_retain_p2[2] = 16;
		cfg.m_final_shortlist_max_size_p2[0] = 2;
		cfg.m_final_shortlist_max_size_p2[1] = 8;
		cfg.m_final_shortlist_max_size_p2[2] = 16;
		cfg.m_base_parts2_p2 = 32;
		cfg.m_base_parts3_p2 = 8;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = true;

		cfg.m_early_stop_wpsnr = 65.0f;
		cfg.m_early_stop2_wpsnr = 60.0f;
		break;
	}
	case 2:
	{
		// Level 2+ have subsets and RGB dual-plane enabled
		cfg.m_second_superpass_refinement = false;
		cfg.m_third_superpass_try_neighbors = true;

		cfg.m_subsets_enabled = true;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = false;
		cfg.m_disable_rgb_dual_plane = false;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 1;
		cfg.m_superbucket_max_to_retain[1] = 2;
		cfg.m_superbucket_max_to_retain[2] = 3;

		cfg.m_base_parts2 = 1;
		cfg.m_base_parts3 = 0;
		cfg.m_part2_fraction_to_keep = 1;
		cfg.m_part3_fraction_to_keep = 1;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 1;
		cfg.m_final_shortlist_max_size[1] = 2;
		cfg.m_final_shortlist_max_size[2] = 3;

		cfg.m_gradient_descent_flag = false;
		cfg.m_polish_weights_flag = true;
		cfg.m_qcd_enabled_flag = false;

		cfg.m_bucket_pruning_passes = false;
		cfg.m_cem_enc_params.m_max_ls_passes = 1;

		// Second superpass
		cfg.m_second_superpass_fract_to_recompress = .04f;
		cfg.m_second_pass_force_subsets_enabled = true;
		cfg.m_superbucket_max_to_retain_p2[0] = 1;
		cfg.m_superbucket_max_to_retain_p2[1] = 2;
		cfg.m_superbucket_max_to_retain_p2[2] = 8;
		cfg.m_final_shortlist_max_size_p2[0] = 1;
		cfg.m_final_shortlist_max_size_p2[1] = 2;
		cfg.m_final_shortlist_max_size_p2[2] = 8;
		cfg.m_base_parts2_p2 = 16;
		cfg.m_base_parts3_p2 = 0;
		cfg.m_force_all_dp_chans_p2 = false;
		cfg.m_filter_by_pca_angles_flag_p2 = true;

		cfg.m_early_stop_wpsnr = 45.0f;
		cfg.m_early_stop2_wpsnr = 40.0f;
		break;
	}
	case 1:
	{
		cfg.m_second_superpass_refinement = false;
		cfg.m_third_superpass_try_neighbors = false;

		cfg.m_subsets_enabled = false;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = false;
		cfg.m_disable_rgb_dual_plane = true;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 1;
		cfg.m_superbucket_max_to_retain[1] = 1;
		cfg.m_superbucket_max_to_retain[2] = 1;

		cfg.m_base_parts2 = 0;
		cfg.m_base_parts3 = 0;
		cfg.m_part2_fraction_to_keep = 1;
		cfg.m_part3_fraction_to_keep = 1;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 1;
		cfg.m_final_shortlist_max_size[1] = 1;
		cfg.m_final_shortlist_max_size[2] = 1;

		cfg.m_gradient_descent_flag = false;
		cfg.m_polish_weights_flag = true;
		cfg.m_qcd_enabled_flag = false;

		cfg.m_bucket_pruning_passes = false;
		cfg.m_cem_enc_params.m_max_ls_passes = 1;

		cfg.m_early_stop_wpsnr = 45.0f;
		cfg.m_early_stop2_wpsnr = 40.0f;
		break;
	}
	case 0:
	{
		cfg.m_second_superpass_refinement = false;
		cfg.m_third_superpass_try_neighbors = false;

		cfg.m_subsets_enabled = false;
		cfg.m_use_blue_contraction = true;
		cfg.m_use_base_ofs = false;
		cfg.m_disable_rgb_dual_plane = true;

		cfg.m_force_all_dual_plane_chan_evals = false;
		cfg.m_filter_by_pca_angles_flag = true;

		cfg.m_superbucket_max_to_retain[0] = 1;
		cfg.m_superbucket_max_to_retain[1] = 1;
		cfg.m_superbucket_max_to_retain[2] = 1;

		cfg.m_base_parts2 = 0;
		cfg.m_base_parts3 = 0;
		cfg.m_part2_fraction_to_keep = 1;
		cfg.m_part3_fraction_to_keep = 1;

		cfg.m_final_shortlist_fraction[0] = 1.0f;
		cfg.m_final_shortlist_fraction[1] = 1.0f;
		cfg.m_final_shortlist_fraction[2] = 1.0f;

		cfg.m_final_shortlist_max_size[0] = 1;
		cfg.m_final_shortlist_max_size[1] = 1;
		cfg.m_final_shortlist_max_size[2] = 1;

		cfg.m_gradient_descent_flag = false;
		cfg.m_polish_weights_flag = false;
		cfg.m_qcd_enabled_flag = false;

		cfg.m_bucket_pruning_passes = false;
		cfg.m_cem_enc_params.m_max_ls_passes = 1;

		cfg.m_early_stop_wpsnr = 45.0f;
		cfg.m_early_stop2_wpsnr = 40.0f;
		break;
	}
	}
}

#if BASISD_SUPPORT_KTX2_ZSTD
static bool zstd_compress(const uint8_t* pData, size_t data_len, uint8_vec& comp_data, int zstd_level)
{
	if (!data_len)
	{
		comp_data.resize(0);
		return true;
	}

	assert(pData);

	comp_data.resize(ZSTD_compressBound(data_len));

	size_t result = ZSTD_compress(comp_data.data(), comp_data.size(), pData, data_len, zstd_level);

	if (ZSTD_isError(result))
	{
		comp_data.resize(0);
		return false;
	}

	if (result > UINT32_MAX)
	{
		comp_data.resize(0);
		return false;
	}

	comp_data.resize(result);
	return true;
}

static bool zstd_compress(const bitwise_coder& coder, uint8_vec& comp_data, int zstd_level)
{
	return zstd_compress(coder.get_bytes().data(), coder.get_bytes().size(), comp_data, zstd_level);
}

static bool zstd_compress(const uint8_vec& vec, uint8_vec& comp_data, int zstd_level)
{
	return zstd_compress(vec.data(), vec.size(), comp_data, zstd_level);
}

static uint32_t encode_values(bitwise_coder& coder, uint32_t total_values, const uint8_t* pVals, uint32_t endpoint_range)
{
	const uint32_t MAX_VALS = 64;
	uint32_t bit_values[MAX_VALS], tq_values[(MAX_VALS + 2) / 3];
	uint32_t total_tq_values = 0, tq_accum = 0, tq_mul = 1;

	assert((total_values) && (total_values <= MAX_VALS));

	const uint32_t ep_bits = astc_helpers::g_ise_range_table[endpoint_range][0];
	const uint32_t ep_trits = astc_helpers::g_ise_range_table[endpoint_range][1];
	const uint32_t ep_quints = astc_helpers::g_ise_range_table[endpoint_range][2];

	for (uint32_t i = 0; i < total_values; i++)
	{
		uint32_t val = pVals[i];

		uint32_t bits = val & ((1 << ep_bits) - 1);
		uint32_t tq = val >> ep_bits;

		bit_values[i] = bits;

		if (ep_trits)
		{
			assert(tq < 3);
			tq_accum += tq * tq_mul;
			tq_mul *= 3;
			if (tq_mul == 243)
			{
				assert(total_tq_values < BASISU_ARRAY_SIZE(tq_values));
				tq_values[total_tq_values++] = tq_accum;
				tq_accum = 0;
				tq_mul = 1;
			}
		}
		else if (ep_quints)
		{
			assert(tq < 5);
			tq_accum += tq * tq_mul;
			tq_mul *= 5;
			if (tq_mul == 125)
			{
				assert(total_tq_values < BASISU_ARRAY_SIZE(tq_values));
				tq_values[total_tq_values++] = tq_accum;
				tq_accum = 0;
				tq_mul = 1;
			}
		}
	}

	uint32_t total_bits_output = 0;

	for (uint32_t i = 0; i < total_tq_values; i++)
	{
		const uint32_t num_bits = ep_trits ? 8 : 7;
		coder.put_bits(tq_values[i], num_bits);
		total_bits_output += num_bits;
	}

	if (tq_mul > 1)
	{
		uint32_t num_bits;
		if (ep_trits)
		{
			if (tq_mul == 3)
				num_bits = 2;
			else if (tq_mul == 9)
				num_bits = 4;
			else if (tq_mul == 27)
				num_bits = 5;
			else //if (tq_mul == 81)
				num_bits = 7;
		}
		else
		{
			if (tq_mul == 5)
				num_bits = 3;
			else //if (tq_mul == 25)
				num_bits = 5;
		}
		coder.put_bits(tq_accum, num_bits);
		total_bits_output += num_bits;
	}

	for (uint32_t i = 0; i < total_values; i++)
	{
		coder.put_bits(bit_values[i], ep_bits);
		total_bits_output += ep_bits;
	}

	return total_bits_output;
}

static bool compress_image_full_zstd(
	const image& orig_img, uint8_vec& comp_data, vector2D<astc_helpers::log_astc_block>& coded_blocks,
	const astc_ldr_encode_config& global_cfg,
	job_pool& job_pool,
	ldr_astc_block_encode_image_high_level_config& enc_cfg,	const ldr_astc_block_encode_image_output& enc_out)
{
	BASISU_NOTE_UNUSED(job_pool);

	const uint32_t width = orig_img.get_width(), height = orig_img.get_height();

	const uint32_t block_width = global_cfg.m_astc_block_width;
	const uint32_t block_height = global_cfg.m_astc_block_height;
	const uint32_t total_block_pixels = block_width * block_height;

	const uint32_t total_pixels = width * height;
	const uint32_t num_blocks_x = (width + block_width - 1) / block_width;
	const uint32_t num_blocks_y = (height + block_height - 1) / block_height;
	const uint32_t total_blocks = num_blocks_x * num_blocks_y;
	const bool has_alpha = orig_img.has_alpha();

	// Mode
	uint8_vec mode_bytes;
	mode_bytes.reserve(8192);

	bitwise_coder raw_bits;
	raw_bits.init(8192);
	
	uint8_vec solid_dpcm_bytes;
	solid_dpcm_bytes.reserve(8192);
	
	// Endpoints
	uint8_vec endpoint_dpcm_reuse_indices;
	endpoint_dpcm_reuse_indices.reserve(8192);
	
	bitwise_coder use_bc_bits;
	use_bc_bits.init(1024);

	bitwise_coder endpoint_dpcm_3bit;
	endpoint_dpcm_3bit.init(1024);

	bitwise_coder endpoint_dpcm_4bit;
	endpoint_dpcm_4bit.init(1024);
				
	uint8_vec endpoint_dpcm_5bit;
	endpoint_dpcm_5bit.reserve(8192);

	uint8_vec endpoint_dpcm_6bit;
	endpoint_dpcm_6bit.reserve(8192);

	uint8_vec endpoint_dpcm_7bit;
	endpoint_dpcm_7bit.reserve(8192);

	uint8_vec endpoint_dpcm_8bit;
	endpoint_dpcm_8bit.reserve(8192);

	// Weights
	bitwise_coder mean0_bits;
	uint8_vec mean1_bytes;
	uint8_vec run_bytes;
	uint8_vec coeff_bytes;
	bitwise_coder sign_bits;
	bitwise_coder weight2_bits;
	bitwise_coder weight3_bits;
	bitwise_coder weight4_bits;
	uint8_vec weight8_bits;
		
	mean0_bits.init(1024);
	mean1_bytes.reserve(1024);
	run_bytes.reserve(8192);
	coeff_bytes.reserve(8192);
	sign_bits.init(1024);
	weight2_bits.init(1024);
	weight3_bits.init(1024);
	weight4_bits.init(1024);
	weight8_bits.reserve(8192);

	const float replacement_min_psnr = has_alpha ? global_cfg.m_replacement_min_psnr_alpha : global_cfg.m_replacement_min_psnr;
	const float psnr_trial_diff_thresh = has_alpha ? global_cfg.m_psnr_trial_diff_thresh_alpha : global_cfg.m_psnr_trial_diff_thresh;
	const float psnr_trial_diff_thresh_edge = has_alpha ? global_cfg.m_psnr_trial_diff_thresh_edge_alpha : global_cfg.m_psnr_trial_diff_thresh_edge;
	const float total_comp_weights = enc_cfg.m_cem_enc_params.get_total_comp_weights();

	basist::astc_ldr_t::grid_weight_dct grid_dct;
	grid_dct.init(block_width, block_height);

	coded_blocks.resize(num_blocks_x, num_blocks_y);
	for (uint32_t y = 0; y < num_blocks_y; y++)
		for (uint32_t x = 0; x < num_blocks_x; x++)
			coded_blocks(x, y).clear();

	vector2D<astc_helpers::log_astc_block> input_blocks;
	if (global_cfg.m_debug_images)
	{
		input_blocks.resize(num_blocks_x, num_blocks_y);

		for (uint32_t y = 0; y < num_blocks_y; y++)
			for (uint32_t x = 0; x < num_blocks_x; x++)
				input_blocks(x, y).clear();
	}
	
	vector2D<basist::astc_ldr_t::prev_block_state_full_zstd> prev_block_states(num_blocks_x, num_blocks_y);

	int part2_hash[basist::astc_ldr_t::PART_HASH_SIZE];
	std::fill(part2_hash, part2_hash + basist::astc_ldr_t::PART_HASH_SIZE, -1);

	int part3_hash[basist::astc_ldr_t::PART_HASH_SIZE];
	std::fill(part3_hash, part3_hash + basist::astc_ldr_t::PART_HASH_SIZE, -1);

	int tm_hash[basist::astc_ldr_t::TM_HASH_SIZE];
	std::fill(tm_hash, tm_hash + basist::astc_ldr_t::TM_HASH_SIZE, -1);

	const bool use_run_commands_global_enable = true;
	const bool endpoint_dpcm_global_enable = true;

	uint32_t cur_run_len = 0;

	uint32_t total_runs = 0, total_run_blocks = 0, total_nonrun_blocks = 0;
	uint32_t total_lossy_replacements = 0;
	uint32_t total_solid_blocks = 0;
	uint32_t total_full_reuse_commands = 0;
	uint32_t total_raw_commands = 0;
	uint32_t total_reuse_full_cfg_emitted = 0;
	uint32_t total_full_cfg_emitted = 0;
	uint32_t num_part_hash_probes = 0;
	uint32_t num_part_hash_hits = 0;
	uint32_t total_used_endpoint_dpcm = 0;
	uint32_t total_used_endpoint_raw = 0;
	uint32_t total_used_dct = 0;
	uint32_t total_used_weight_dpcm = 0;
	uint32_t num_tm_hash_hits = 0, num_tm_hash_probes = 0;

	raw_bits.put_bits(basist::astc_ldr_t::FULL_ZSTD_HEADER_MARKER, basist::astc_ldr_t::FULL_ZSTD_HEADER_MARKER_BITS);
	
	const int block_dim_index = astc_helpers::find_astc_block_size_index(block_width, block_height);
	assert((block_dim_index >= 0) && (block_dim_index < (int)astc_helpers::NUM_ASTC_BLOCK_SIZES));
		
	raw_bits.put_bits(block_dim_index, 4);

	raw_bits.put_bits(enc_cfg.m_cem_enc_params.m_decode_mode_srgb, 1);

	raw_bits.put_bits(width, 16);
	raw_bits.put_bits(height, 16);

	raw_bits.put_bits(has_alpha, 1);

	raw_bits.put_bits(enc_cfg.m_use_dct, 1);
	if (enc_cfg.m_use_dct)
	{
		const int int_q = clamp<int>((int)std::round(global_cfg.m_dct_quality * 2.0f), 0, 200);
		raw_bits.put_bits(int_q, 8);
	}

	const uint32_t FULL_ZSTD_MAX_RUN_LEN = 64;

	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		//const uint32_t base_y = by * block_height;

		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			//const uint32_t base_x = bx * block_width;
			//raw_bits.put_bits(0xA1, 8);

			basist::astc_ldr_t::prev_block_state_full_zstd& prev_state = prev_block_states(bx, by);
			
			const basist::astc_ldr_t::prev_block_state_full_zstd* pLeft_state = bx ? &prev_block_states(bx - 1, by) : nullptr;
			const basist::astc_ldr_t::prev_block_state_full_zstd* pUpper_state = by ? &prev_block_states(bx, by - 1) : nullptr;
			const basist::astc_ldr_t::prev_block_state_full_zstd* pDiag_state = (bx && by) ? &prev_block_states(bx - 1, by - 1) : nullptr;
			
			const ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

			uint32_t best_packed_out_block_index = blk_info.m_packed_out_block_index;

			if (global_cfg.m_debug_images)
			{
				input_blocks(bx, by) = blk_info.m_out_blocks[best_packed_out_block_index].m_log_blk;
			}

			// check for run 
			if ((use_run_commands_global_enable) && (bx || by))
			{
				const encode_block_output& blk_out = blk_info.m_out_blocks[best_packed_out_block_index];
				const astc_helpers::log_astc_block& cur_log_blk = blk_out.m_log_blk;

				const astc_helpers::log_astc_block& prev_log_blk = bx ? coded_blocks(bx - 1, by) : coded_blocks(0, by - 1);
				const basist::astc_ldr_t::prev_block_state_full_zstd* pPrev_block_state = bx ? pLeft_state : pUpper_state;

				assert(pPrev_block_state);

				if (compare_log_blocks_for_equality(cur_log_blk, prev_log_blk))
				{
					// Left or upper is exactly the same logical block, so expand the run.
					cur_run_len++;

					// Accept the previous block (left or upper) as if it's been coded normally.

					coded_blocks(bx, by) = prev_log_blk;

					//prev_state.m_was_solid_color = pPrev_block_state->m_was_solid_color;
					prev_state.m_tm_index = pPrev_block_state->m_tm_index;
					//prev_state.m_base_cem_index = pPrev_block_state->m_base_cem_index;

					if (cur_run_len == FULL_ZSTD_MAX_RUN_LEN)
					{
						total_runs++;
						total_run_blocks += cur_run_len;
						mode_bytes.push_back((uint8_t)((uint32_t)basist::astc_ldr_t::xuastc_zstd_mode::cMODE_RUN | ((cur_run_len - 1) << 2)));
						cur_run_len = 0;
					}
					
					continue;
				}
			}

			if (cur_run_len)
			{
				assert(cur_run_len <= FULL_ZSTD_MAX_RUN_LEN);

				total_runs++;
				total_run_blocks += cur_run_len;
				mode_bytes.push_back((uint8_t)((uint32_t)basist::astc_ldr_t::xuastc_zstd_mode::cMODE_RUN | ((cur_run_len - 1) << 2)));
				cur_run_len = 0;
			}

			total_nonrun_blocks++;

			// TODO: Move this to a prepass that's shared between arith/zstd
			const float ref_wmse = (float)blk_info.m_out_blocks[best_packed_out_block_index].m_sse / (total_comp_weights * (float)total_block_pixels);
			const float ref_wpsnr = (ref_wmse > 1e-5f) ? 20.0f * log10f(255.0f / sqrtf(ref_wmse)) : 10000.0f;
						
			if ((global_cfg.m_lossy_supercompression) && (ref_wpsnr >= replacement_min_psnr) &&
				(!blk_info.m_out_blocks[blk_info.m_packed_out_block_index].m_log_blk.m_solid_color_flag_ldr))
			{
				const float psnr_thresh = blk_info.m_strong_edges ? psnr_trial_diff_thresh_edge : psnr_trial_diff_thresh;

				float best_alt_wpsnr = 0.0f;
				bool found_alternative = false;

				// Pass: 0 consider full config+part ID endpoint reuse
				// Pass: 1 fall back to just full config+part ID reuse (no endpoints)
				for (uint32_t pass = 0; pass < 2; pass++)
				{
					// Iterate through all available alternative candidates
					for (uint32_t out_block_iter = 0; out_block_iter < blk_info.m_out_blocks.size(); out_block_iter++)
					{
						if (out_block_iter == blk_info.m_packed_out_block_index)
							continue;

						const float trial_wmse = (float)blk_info.m_out_blocks[out_block_iter].m_sse / (total_comp_weights * (float)total_block_pixels);
						const float trial_wpsnr = (trial_wmse > 1e-5f) ? 20.0f * log10f(255.0f / sqrtf(trial_wmse)) : 10000.0f;

						// Reject if PSNR too low
						if (trial_wpsnr < (ref_wpsnr - psnr_thresh))
							continue;

						// Reject if inferior than best found so far
						if (trial_wpsnr < best_alt_wpsnr)
							continue;

						const astc_helpers::log_astc_block& trial_log_blk = blk_info.m_out_blocks[out_block_iter].m_log_blk;

						if (trial_log_blk.m_solid_color_flag_ldr)
							continue;

						// Examine nearby neighbors
						for (uint32_t i = 0; i < basist::astc_ldr_t::cMaxConfigReuseNeighbors; i++)
						{
							int dx = 0, dy = 0;
							switch (i)
							{
							case 0: dx = -1; break;
							case 1: dy = -1; break;
							case 2: dx = -1; dy = -1; break;
							default: assert(0); break;
							}

							const int n_bx = bx + dx, n_by = by + dy;
							if ((n_bx < 0) || (n_by < 0))
								continue;

							astc_helpers::log_astc_block& neighbor_log_blk = coded_blocks(n_bx, n_by);

							if (neighbor_log_blk.m_solid_color_flag_ldr)
								continue;

							bool accept_flag = false;
							if (pass == 0)
							{
								// prefer full config+endpoint equality first
								accept_flag = compare_log_block_configs_and_endpoints(trial_log_blk, neighbor_log_blk);
							}
							else
							{
								// next check for just config equality
								accept_flag = compare_log_block_configs(trial_log_blk, neighbor_log_blk);
							}

							if (accept_flag)
							{
								best_alt_wpsnr = trial_wpsnr;
								best_packed_out_block_index = out_block_iter;
								found_alternative = true;
								break;
							}

						} // i

					} // out_block_iter

					if (found_alternative)
						break;

				} // pass

				if (best_packed_out_block_index != blk_info.m_packed_out_block_index)
					total_lossy_replacements++;

			} // global_cfg.m_lossy_supercompression

			const encode_block_output& blk_out = blk_info.m_out_blocks[best_packed_out_block_index];

			astc_helpers::log_astc_block& cur_log_blk = coded_blocks(bx, by);

			cur_log_blk = blk_out.m_log_blk;

			// Solid color/void extent
			if (blk_out.m_trial_mode_index < 0)
			{
				assert(cur_log_blk.m_solid_color_flag_ldr);

				total_solid_blocks++;

				mode_bytes.push_back((uint8_t)basist::astc_ldr_t::xuastc_zstd_mode::cMODE_SOLID);

				uint32_t cur_solid_color[4];
				for (uint32_t i = 0; i < 4; i++)
					cur_solid_color[i] = blk_out.m_log_blk.m_solid_color[i] >> 8;

				uint32_t prev_solid_color[4] = { 0 };

				const uint32_t num_comps = has_alpha ? 4 : 3;

				astc_helpers::log_astc_block* pPrev_log_blk = bx ? &coded_blocks(bx - 1, by) : (by ? &coded_blocks(bx, by - 1) : nullptr);
				if (pPrev_log_blk)
				{
					if (pPrev_log_blk->m_solid_color_flag_ldr)
					{
						prev_solid_color[0] = pPrev_log_blk->m_solid_color[0] >> 8;
						prev_solid_color[1] = pPrev_log_blk->m_solid_color[1] >> 8;
						prev_solid_color[2] = pPrev_log_blk->m_solid_color[2] >> 8;
						prev_solid_color[3] = pPrev_log_blk->m_solid_color[3] >> 8;
					}
					else
					{
						// Decode previous block's first CEM, use the halfway point as the predictor.
						color_rgba prev_l, prev_h;
						decode_endpoints(pPrev_log_blk->m_color_endpoint_modes[0], pPrev_log_blk->m_endpoints, pPrev_log_blk->m_endpoint_ise_range, prev_l, prev_h);

						prev_solid_color[0] = (prev_l[0] + prev_h[0] + 1) >> 1;
						prev_solid_color[1] = (prev_l[1] + prev_h[1] + 1) >> 1;
						prev_solid_color[2] = (prev_l[2] + prev_h[2] + 1) >> 1;
						prev_solid_color[3] = (prev_l[3] + prev_h[3] + 1) >> 1;
					}
				}

				for (uint32_t i = 0; i < num_comps; i++)
				{
					const uint32_t delta = (cur_solid_color[i] - prev_solid_color[i]) & 0xFF;
					solid_dpcm_bytes.push_back((uint8_t)delta);
				}
				
				//prev_state.m_was_solid_color = true;
				prev_state.m_tm_index = -1;
				//prev_state.m_base_cem_index = astc_helpers::CEM_LDR_RGB_DIRECT;

				continue;
			}

			assert(!cur_log_blk.m_solid_color_flag_ldr);

			int full_cfg_endpoint_reuse_index = -1;

			for (uint32_t i = 0; i < basist::astc_ldr_t::cMaxConfigReuseNeighbors; i++)
			{
				int dx = 0, dy = 0;
				switch (i)
				{
				case 0: dx = -1; break;
				case 1: dy = -1; break;
				case 2: dx = -1; dy = -1; break;
				default: assert(0); break;
				}

				const int n_bx = bx + dx, n_by = by + dy;
				if ((n_bx < 0) || (n_by < 0))
					continue;

				astc_helpers::log_astc_block& neighbor_log_blk = coded_blocks(n_bx, n_by);

				if (neighbor_log_blk.m_solid_color_flag_ldr)
					continue;

				if (compare_log_block_configs_and_endpoints(cur_log_blk, neighbor_log_blk))
				{
					full_cfg_endpoint_reuse_index = i;
					break;
				}
			} // i

			if (full_cfg_endpoint_reuse_index >= 0)
			{
				// Reused full config, part ID and endpoint values from an immediate neighbor
				mode_bytes.push_back((uint8_t)((uint32_t)basist::astc_ldr_t::xuastc_zstd_mode::cMODE_REUSE_CFG_ENDPOINTS_LEFT + (full_cfg_endpoint_reuse_index << 2)));

				total_full_reuse_commands++;

				const basist::astc_ldr_t::prev_block_state_full_zstd* pReused_cfg_state = nullptr;

				switch (full_cfg_endpoint_reuse_index)
				{
				case 0: pReused_cfg_state = pLeft_state; break;
				case 1: pReused_cfg_state = pUpper_state; break;
				case 2: pReused_cfg_state = pDiag_state; break;
				default: assert(0); break;
				}

				if (!pReused_cfg_state)
				{
					assert(0);
					fmt_error_printf("encoding internal failure\n");
					return false;
				}

				assert(pReused_cfg_state->m_tm_index == blk_out.m_trial_mode_index);

				prev_state.m_tm_index = blk_out.m_trial_mode_index;
			}
			else
			{
				// No nearby full config+part ID+endpoint reuse, so send raw command
				// Must send endpoints too.
				total_raw_commands++;

				// Format of mode byte (UD bit used in modes other than raw)
				// 7  6  5  4  3  2  1  0
				// UD C  ED HH BO I  I  M
				
				// MMM=mode
				// II=neighbor reuse index [0,3], 3=no reuse
				// BO=base offset flag
				// HH=partition hash hit flag
				// ED=endpoint DPCM flag
				// C=config hash table hit
				// UD=use DCT flag

				mode_bytes.push_back((uint8_t)basist::astc_ldr_t::xuastc_zstd_mode::cMODE_RAW);

				const uint32_t cur_actual_cem = cur_log_blk.m_color_endpoint_modes[0];
				const uint32_t total_endpoint_vals = astc_helpers::get_num_cem_values(cur_actual_cem);

				// DO NOT use tm.m_cem because the encoder may have selected a base+ofs variant instead. Use cur_actual_cem.
				const basist::astc_ldr_t::trial_mode& tm = enc_out.m_encoder_trial_modes[blk_out.m_trial_mode_index];

				// Check for config+part ID neighbor reuse (partial refuse)
				int neighbor_cfg_match_index = -1;
				for (uint32_t i = 0; i < basist::astc_ldr_t::cMaxConfigReuseNeighbors; i++)
				{
					const basist::astc_ldr_t::prev_block_state_full_zstd* pNeighbor_state = nullptr;

					int dx = 0, dy = 0;
					switch (i)
					{
					case 0: dx = -1; pNeighbor_state = pLeft_state; break;
					case 1: dy = -1; pNeighbor_state = pUpper_state; break;
					case 2: dx = -1; dy = -1; pNeighbor_state = pDiag_state; break;
					default: assert(0); break;
					}

					if (!pNeighbor_state)
						continue;

					const int n_bx = bx + dx, n_by = by + dy;
					assert((n_bx >= 0) && (n_by >= 0));

					astc_helpers::log_astc_block& neighbor_log_blk = coded_blocks(n_bx, n_by);

					if (pNeighbor_state->m_tm_index != blk_out.m_trial_mode_index)
						continue;

					if (neighbor_log_blk.m_color_endpoint_modes[0] != cur_log_blk.m_color_endpoint_modes[0])
						continue;

					if (neighbor_log_blk.m_partition_id != cur_log_blk.m_partition_id)
						continue;

					assert(neighbor_log_blk.m_dual_plane == cur_log_blk.m_dual_plane);
					assert(neighbor_log_blk.m_color_component_selector == cur_log_blk.m_color_component_selector);
					assert(neighbor_log_blk.m_num_partitions == cur_log_blk.m_num_partitions);
					assert(neighbor_log_blk.m_grid_width == cur_log_blk.m_grid_width);
					assert(neighbor_log_blk.m_grid_height == cur_log_blk.m_grid_height);
					assert(neighbor_log_blk.m_endpoint_ise_range == cur_log_blk.m_endpoint_ise_range);
					assert(neighbor_log_blk.m_weight_ise_range == cur_log_blk.m_weight_ise_range);

					neighbor_cfg_match_index = i;
					break;
				}

				if (neighbor_cfg_match_index >= 0)
				{
					// Partial reuse (config+partition ID, but not endpoints).
					// OR 2-bits into the mode byte
					mode_bytes.back() |= (uint8_t)(neighbor_cfg_match_index << 1);
										
					const basist::astc_ldr_t::prev_block_state_full_zstd* pReused_cfg_state = nullptr;

					switch (neighbor_cfg_match_index)
					{
					case 0: pReused_cfg_state = pLeft_state; break;
					case 1: pReused_cfg_state = pUpper_state; break;
					case 2: pReused_cfg_state = pDiag_state; break;
					default: assert(0); break;
					}

					if (!pReused_cfg_state)
					{
						assert(0);
						fmt_error_printf("encoding internal failure\n");
						return false;
					}

					assert(pReused_cfg_state->m_tm_index == blk_out.m_trial_mode_index);

					prev_state.m_tm_index = blk_out.m_trial_mode_index;
															
					total_reuse_full_cfg_emitted++;
				}
				else
				{
					// No reuse - must send config, so pack it. Then send endpoints.
					total_full_cfg_emitted++;

					// OR 2-bits into the mode byte (so now 5 bits total)
					mode_bytes.back() |= (uint8_t)(((uint32_t)basist::astc_ldr_t::cMaxConfigReuseNeighbors) << 1);

					// Pack tm index (ASTC base config)
					{
						num_tm_hash_probes++;

						uint32_t tm_h = basist::astc_ldr_t::tm_hash_index(blk_out.m_trial_mode_index);

						if (tm_hash[tm_h] == blk_out.m_trial_mode_index)
						{
							num_tm_hash_hits++;

							mode_bytes.back() |= (uint8_t)basist::astc_ldr_t::XUASTC_LDR_MODE_BYTE_TM_HASH_HIT_FLAG; // tm hash hit flag

							raw_bits.put_bits(tm_h, basist::astc_ldr_t::TM_HASH_BITS);
						}
						else
						{
							raw_bits.put_truncated_binary(blk_out.m_trial_mode_index, (uint32_t)enc_out.m_encoder_trial_modes.size());

							tm_hash[tm_h] = blk_out.m_trial_mode_index;
						}
					}

					prev_state.m_tm_index = blk_out.m_trial_mode_index;
					
					// Send base_ofs bit if the tm is direct
					if ((tm.m_cem == astc_helpers::CEM_LDR_RGB_DIRECT) || (tm.m_cem == astc_helpers::CEM_LDR_RGBA_DIRECT))
					{
						const bool is_base_ofs = (cur_log_blk.m_color_endpoint_modes[0] == astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET) ||
							(cur_log_blk.m_color_endpoint_modes[0] == astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET);

						if (is_base_ofs)
							mode_bytes.back() |= basist::astc_ldr_t::XUASTC_LDR_MODE_BYTE_IS_BASE_OFS_FLAG; // base_ofs bit
					}

					if (tm.m_num_parts > 1)
					{
						// Send unique part pattern ID
						const astc_ldr::partitions_data* pPart_data = (tm.m_num_parts == 2) ? &enc_out.m_part_data_p2 : &enc_out.m_part_data_p3;

						const uint32_t astc_pat_index = cur_log_blk.m_partition_id;
						const uint32_t unique_pat_index = pPart_data->m_part_seed_to_unique_index[astc_pat_index];
						const uint32_t total_unique_indices = pPart_data->m_total_unique_patterns;
						assert(unique_pat_index < total_unique_indices);

						num_part_hash_probes++;

						int* pPart_hash = (tm.m_num_parts == 2) ? part2_hash : part3_hash;

						const uint32_t h = basist::astc_ldr_t::part_hash_index(unique_pat_index);

						if (pPart_hash[h] != (int)unique_pat_index)
						{
#if defined(_DEBUG) || defined(DEBUG)
							// sanity
							for (uint32_t i = 0; i < basist::astc_ldr_t::PART_HASH_SIZE; i++)
							{
								assert(pPart_hash[i] != (int)unique_pat_index);
							}
#endif

							raw_bits.put_truncated_binary(unique_pat_index, total_unique_indices);
						}
						else
						{
							num_part_hash_hits++;

							mode_bytes.back() |= basist::astc_ldr_t::XUASTC_LDR_MODE_BYTE_PART_HASH_HIT; // hash pat_index hit bit
							raw_bits.put_bits(h, basist::astc_ldr_t::PART_HASH_BITS);
						}

						pPart_hash[basist::astc_ldr_t::part_hash_index(unique_pat_index)] = unique_pat_index;
					}
				}
				
				// Send endpoints
				const int num_endpoint_levels = astc_helpers::get_ise_levels(cur_log_blk.m_endpoint_ise_range);
				const auto& endpoint_ise_to_rank = astc_helpers::g_dequant_tables.get_endpoint_tab(cur_log_blk.m_endpoint_ise_range).m_ISE_to_rank;

				bool endpoints_use_bc[astc_helpers::MAX_PARTITIONS] = { false };

				if (astc_helpers::cem_supports_bc(cur_actual_cem))
				{
					for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
					{
						const bool cur_uses_bc = astc_helpers::used_blue_contraction(cur_actual_cem, cur_log_blk.m_endpoints + part_iter * total_endpoint_vals, cur_log_blk.m_endpoint_ise_range);

						endpoints_use_bc[part_iter] = cur_uses_bc;

					} // part_iter
				}

				int best_reuse_bx = -1, best_reuse_by = -1;
				uint32_t best_reuse_index = 0;
				const astc_helpers::log_astc_block* pEndpoint_pred_log_blk = nullptr;

				if (endpoint_dpcm_global_enable)
				{
					int64_t best_trial_delta2 = INT64_MAX;
					float best_trial_bits = BIG_FLOAT_VAL;

					// TODO: Decide if DPCM is even worth it.
					const float N = (float)(total_endpoint_vals * tm.m_num_parts);

					for (uint32_t reuse_index = 0; reuse_index < basist::astc_6x6_hdr::NUM_REUSE_XY_DELTAS; reuse_index++)
					{
						const int rx = (int)bx + basist::astc_6x6_hdr::g_reuse_xy_deltas[reuse_index].m_x;
						const int ry = (int)by + basist::astc_6x6_hdr::g_reuse_xy_deltas[reuse_index].m_y;
						if ((rx < 0) || (ry < 0) || (rx >= (int)num_blocks_x) || (ry >= (int)num_blocks_y))
							continue;

						const astc_helpers::log_astc_block* pTrial_log_blk = &coded_blocks(rx, ry);
						if (pTrial_log_blk->m_solid_color_flag_ldr)
							continue;

						uint8_t trial_predicted_endpoints[astc_helpers::MAX_PARTITIONS][astc_helpers::MAX_CEM_ENDPOINT_VALS] = { };

						uint32_t part_iter;
						for (part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
						{
							const bool always_repack_flag = false;
							bool blue_contraction_clamped_flag = false, try_direct_encoding_flag = false;

							bool conv_status = basist::astc_ldr_t::convert_endpoints_across_cems(
								pTrial_log_blk->m_color_endpoint_modes[0], pTrial_log_blk->m_endpoint_ise_range, pTrial_log_blk->m_endpoints,
								cur_actual_cem, cur_log_blk.m_endpoint_ise_range, trial_predicted_endpoints[part_iter],
								always_repack_flag,
								endpoints_use_bc[part_iter], false,
								blue_contraction_clamped_flag, try_direct_encoding_flag);

							if (!conv_status)
								break;
						} // part_iter

						if (part_iter < tm.m_num_parts)
							continue; // failed

						int64_t trial_endpoint_delta2 = 0;
						for (part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
						{
							for (uint32_t val_iter = 0; val_iter < total_endpoint_vals; val_iter++)
							{
								int cur_e_rank = endpoint_ise_to_rank[cur_log_blk.m_endpoints[part_iter * total_endpoint_vals + val_iter]];
								int prev_e_rank = endpoint_ise_to_rank[trial_predicted_endpoints[part_iter][val_iter]];

								int e_delta = cur_e_rank - prev_e_rank;

								trial_endpoint_delta2 += e_delta * e_delta;

							} // val_iter

						} // part_iter
														
						const float mse = (float)trial_endpoint_delta2 / N;

						// Gaussian entropy estimate - precomputed 0.5 * log2(2*pi*e) = ~2.0470956f
						const float k_const = 2.0470956f;

						float bits_per_sym = 0.5f * log2f(basisu::maximum(mse, 1e-9f)) + k_const;

						bits_per_sym = clamp(bits_per_sym, 0.05f, 8.0f);

						// total est bits for this block’s endpoints
						float total_est_bits = bits_per_sym * N;

						if (total_est_bits < best_trial_bits)
						{
							best_trial_delta2 = trial_endpoint_delta2;
							best_trial_bits = total_est_bits;

							best_reuse_bx = rx;
							best_reuse_by = ry;
							best_reuse_index = reuse_index;

							if (!best_trial_delta2)
								break;
						}

					} // reuse_index

					if (best_reuse_bx >= 0)
					{
						pEndpoint_pred_log_blk = &coded_blocks(best_reuse_bx, best_reuse_by);

						assert(!pEndpoint_pred_log_blk->m_solid_color_flag_ldr);
					}

				} // if (endpoint_dpcm_global_enable)

				uint8_t predicted_endpoints[astc_helpers::MAX_PARTITIONS][astc_helpers::MAX_CEM_ENDPOINT_VALS] = { };

				bool use_dpcm_endpoints = false;

				if (pEndpoint_pred_log_blk)
				{
					use_dpcm_endpoints = true;

					assert(cur_log_blk.m_num_partitions == tm.m_num_parts);

					for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
					{
						const bool always_repack_flag = false;
						bool blue_contraction_clamped_flag = false, try_direct_encoding_flag = false;

						bool conv_status = basist::astc_ldr_t::convert_endpoints_across_cems(
							pEndpoint_pred_log_blk->m_color_endpoint_modes[0], pEndpoint_pred_log_blk->m_endpoint_ise_range, pEndpoint_pred_log_blk->m_endpoints,
							cur_actual_cem, cur_log_blk.m_endpoint_ise_range, predicted_endpoints[part_iter],
							always_repack_flag,
							endpoints_use_bc[part_iter], false,
							blue_contraction_clamped_flag, try_direct_encoding_flag);

						if (!conv_status)
						{
							// In practice, should never happen
							use_dpcm_endpoints = false;
							break;
						}
					}
				}

				// TODO: Decide what is cheaper, endpoint DPCM vs. raw

				if (use_dpcm_endpoints)
				{
					// DPCM flag bit
					mode_bytes.back() |= basist::astc_ldr_t::XUASTC_LDR_MODE_BYTE_DPCM_ENDPOINTS_FLAG;

					endpoint_dpcm_reuse_indices.push_back((uint8_t)best_reuse_index);

					if (astc_helpers::cem_supports_bc(cur_actual_cem))
					{
						for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
						{
							use_bc_bits.put_bits(endpoints_use_bc[part_iter], 1);

						} // part_iter
					}

					for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
					{
						for (uint32_t val_iter = 0; val_iter < total_endpoint_vals; val_iter++)
						{
							int cur_e_rank = endpoint_ise_to_rank[cur_log_blk.m_endpoints[part_iter * total_endpoint_vals + val_iter]];
							int prev_e_rank = endpoint_ise_to_rank[predicted_endpoints[part_iter][val_iter]];

							int e_val = imod(cur_e_rank - prev_e_rank, num_endpoint_levels);
																
							if (num_endpoint_levels <= 8)
								endpoint_dpcm_3bit.put_bits(e_val, 4);
							else if (num_endpoint_levels <= 16)
								endpoint_dpcm_4bit.put_bits(e_val, 4);
							else if (num_endpoint_levels <= 32)
								endpoint_dpcm_5bit.push_back((uint8_t)e_val);
							else if (num_endpoint_levels <= 64)
								endpoint_dpcm_6bit.push_back((uint8_t)e_val);
							else if (num_endpoint_levels <= 128)
								endpoint_dpcm_7bit.push_back((uint8_t)e_val);
							else if (num_endpoint_levels <= 256)
								endpoint_dpcm_8bit.push_back((uint8_t)e_val);

						} // val_iter

					} // part_iter

					total_used_endpoint_dpcm++;
				}
				else
				{
					encode_values(raw_bits, tm.m_num_parts * total_endpoint_vals, cur_log_blk.m_endpoints, cur_log_blk.m_endpoint_ise_range);

					total_used_endpoint_raw++;
				} // if (use_dpcm_endpoints)
							
			} // if (full_cfg_endpoint_reuse_index >= 0)

			// ------------------------------------ Send weights

			const uint32_t total_planes = cur_log_blk.m_dual_plane ? 2 : 1;
			const uint32_t total_weights = cur_log_blk.m_grid_width * cur_log_blk.m_grid_height;

			const int num_weight_levels = astc_helpers::get_ise_levels(cur_log_blk.m_weight_ise_range);
			const auto& weight_ise_to_rank = astc_helpers::g_dequant_tables.get_weight_tab(cur_log_blk.m_weight_ise_range).m_ISE_to_rank;

			bool use_dct = enc_cfg.m_use_dct;

			// TODO - tune this threshold
			const uint32_t SWITCH_TO_DPCM_NUM_COEFF_THRESH = (cur_log_blk.m_grid_width * cur_log_blk.m_grid_height * 45 + 64) >> 7;

			if (use_dct)
			{
				for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
				{
					const basist::astc_ldr_t::dct_syms& syms = blk_out.m_packed_dct_plane_data[plane_iter];

					if (!syms.m_coeffs.size())
					{
						fmt_error_printf("compress_image_full_zstd: internal error - no DCT coeffs\n");
						return false;
					}

					if (syms.m_max_coeff_mag > basist::astc_ldr_t::DCT_MAX_ARITH_COEFF_MAG)
					{
						use_dct = false;
						break;
					}

					if (syms.m_coeffs.size() > SWITCH_TO_DPCM_NUM_COEFF_THRESH)
					{
						use_dct = false;
						break;
					}
				}
			}

			// MSB of mode byte=use DCT
			if (enc_cfg.m_use_dct)
			{
				assert((mode_bytes.back() & basist::astc_ldr_t::XUASTC_LDR_MODE_BYTE_USE_DCT) == 0);

				if (use_dct)
					mode_bytes.back() |= basist::astc_ldr_t::XUASTC_LDR_MODE_BYTE_USE_DCT;
			}

			if (use_dct)
			{
				total_used_dct++;

				if (total_planes > 1)
				{
					assert(blk_out.m_packed_dct_plane_data[0].m_num_dc_levels == blk_out.m_packed_dct_plane_data[1].m_num_dc_levels);
				}

				for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
				{
					const basist::astc_ldr_t::dct_syms& syms = blk_out.m_packed_dct_plane_data[plane_iter];

					if (syms.m_num_dc_levels == basist::astc_ldr_t::DCT_MEAN_LEVELS1)
						mean1_bytes.push_back((uint8_t)syms.m_dc_sym);
					else
					{
						assert(syms.m_num_dc_levels == basist::astc_ldr_t::DCT_MEAN_LEVELS0);
						mean0_bits.put_bits(syms.m_dc_sym, 4);
					}

					for (uint32_t i = 0; i < syms.m_coeffs.size(); i++)
					{
						if (syms.m_coeffs[i].m_coeff == INT16_MAX)
						{
							run_bytes.push_back(basist::astc_ldr_t::DCT_RUN_LEN_EOB_SYM_INDEX);
						}
						else
						{
							run_bytes.push_back((uint8_t)syms.m_coeffs[i].m_num_zeros);

							sign_bits.put_bits(syms.m_coeffs[i].m_coeff < 0, 1);

							assert((syms.m_coeffs[i].m_coeff != 0) && (iabs(syms.m_coeffs[i].m_coeff) <= 255));

							coeff_bytes.push_back((uint8_t)(iabs(syms.m_coeffs[i].m_coeff) - 1));
						}
					}

				} // plane_iter
			}
			else
			{
				total_used_weight_dpcm++;

				for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
				{
					int prev_w = num_weight_levels / 2;

					for (uint32_t weight_iter = 0; weight_iter < total_weights; weight_iter++)
					{
						int ise_w = cur_log_blk.m_weights[plane_iter + weight_iter * total_planes];
						int w = weight_ise_to_rank[ise_w];

						int w_to_code = w;
						w_to_code = imod(w - prev_w, num_weight_levels);

						prev_w = w;

						if (num_weight_levels <= 4)
							weight2_bits.put_bits((uint8_t)w_to_code, 2);
						else if (num_weight_levels <= 8)
							weight3_bits.put_bits((uint8_t)w_to_code, 4);
						else if (num_weight_levels <= 16)
							weight4_bits.put_bits((uint8_t)w_to_code, 4);
						else
							weight8_bits.push_back((uint8_t)w_to_code);

					} // weight_iter

				} // plane_iter
			}

		} // bx

		if (cur_run_len)
		{
			assert(cur_run_len <= FULL_ZSTD_MAX_RUN_LEN);

			total_runs++;
			total_run_blocks += cur_run_len;
			mode_bytes.push_back((uint8_t)((uint32_t)basist::astc_ldr_t::xuastc_zstd_mode::cMODE_RUN | ((cur_run_len - 1) << 2)));
			cur_run_len = 0;
		}

	} // by

	raw_bits.put_bits(basist::astc_ldr_t::FINAL_SYNC_MARKER, basist::astc_ldr_t::FINAL_SYNC_MARKER_BITS);
			
	raw_bits.flush();
	endpoint_dpcm_3bit.flush();
	endpoint_dpcm_4bit.flush();
	use_bc_bits.flush();
		
	mean0_bits.flush();
	sign_bits.flush();
	weight2_bits.flush();
	weight3_bits.flush();
	weight4_bits.flush();

	// TODO: Make this configurable
	const uint32_t zstd_level = (global_cfg.m_effort_level >= 3) ? 19 : 9;
	
	uint8_vec comp_mode, comp_solid_dpcm, comp_endpoint_dpcm_reuse_indices;
	uint8_vec comp_use_bc_bits, comp_endpoint_dpcm_3bit, comp_endpoint_dpcm_4bit, comp_endpoint_dpcm_5bit, comp_endpoint_dpcm_6bit, comp_endpoint_dpcm_7bit, comp_endpoint_dpcm_8bit;

	// Mode
	if (!zstd_compress(mode_bytes, comp_mode, zstd_level)) return false;
	if (!zstd_compress(solid_dpcm_bytes, comp_solid_dpcm, zstd_level)) return false;
	
	// Endpoints
	if (!zstd_compress(endpoint_dpcm_reuse_indices, comp_endpoint_dpcm_reuse_indices, zstd_level)) return false;
	if (!zstd_compress(use_bc_bits, comp_use_bc_bits, zstd_level)) return false;
	if (!zstd_compress(endpoint_dpcm_3bit, comp_endpoint_dpcm_3bit, zstd_level)) return false;
	if (!zstd_compress(endpoint_dpcm_4bit, comp_endpoint_dpcm_4bit, zstd_level)) return false;
	if (!zstd_compress(endpoint_dpcm_5bit, comp_endpoint_dpcm_5bit, zstd_level)) return false;
	if (!zstd_compress(endpoint_dpcm_6bit, comp_endpoint_dpcm_6bit, zstd_level)) return false;
	if (!zstd_compress(endpoint_dpcm_7bit, comp_endpoint_dpcm_7bit, zstd_level)) return false;
	if (!zstd_compress(endpoint_dpcm_8bit, comp_endpoint_dpcm_8bit, zstd_level)) return false;

	// Weights
	uint8_vec comp_mean0, comp_mean1, comp_run, comp_coeff, comp_weight2, comp_weight3, comp_weight4, comp_weight8;
		
	if (!zstd_compress(mean0_bits, comp_mean0, zstd_level)) return false;
	if (!zstd_compress(mean1_bytes, comp_mean1, zstd_level)) return false;
	if (!zstd_compress(run_bytes, comp_run, zstd_level)) return false;
	if (!zstd_compress(coeff_bytes, comp_coeff, zstd_level)) return false;
	if (!zstd_compress(weight2_bits, comp_weight2, zstd_level)) return false;
	if (!zstd_compress(weight3_bits, comp_weight3, zstd_level)) return false;
	if (!zstd_compress(weight4_bits, comp_weight4, zstd_level)) return false;
	if (!zstd_compress(weight8_bits, comp_weight8, zstd_level)) return false;

	basist::astc_ldr_t::xuastc_ldr_full_zstd_header hdr;
	clear_obj(hdr);

	hdr.m_flags = (uint8_t)basist::astc_ldr_t::xuastc_ldr_syntax::cFullZStd;

	hdr.m_raw_bits_len = (uint32_t)raw_bits.get_bytes().size();
	hdr.m_mode_bytes_len = (uint32_t)comp_mode.size();
	hdr.m_solid_dpcm_bytes_len = (uint32_t)comp_solid_dpcm.size();

	hdr.m_endpoint_dpcm_reuse_indices_len = (uint32_t)comp_endpoint_dpcm_reuse_indices.size();
	hdr.m_use_bc_bits_len = (uint32_t)comp_use_bc_bits.size();
	hdr.m_endpoint_dpcm_3bit_len = (uint32_t)comp_endpoint_dpcm_3bit.size();
	hdr.m_endpoint_dpcm_4bit_len = (uint32_t)comp_endpoint_dpcm_4bit.size();
	hdr.m_endpoint_dpcm_5bit_len = (uint32_t)comp_endpoint_dpcm_5bit.size();
	hdr.m_endpoint_dpcm_6bit_len = (uint32_t)comp_endpoint_dpcm_6bit.size();
	hdr.m_endpoint_dpcm_7bit_len = (uint32_t)comp_endpoint_dpcm_7bit.size();
	hdr.m_endpoint_dpcm_8bit_len = (uint32_t)comp_endpoint_dpcm_8bit.size();
		
	hdr.m_mean0_bits_len = (uint32_t)comp_mean0.size();
	hdr.m_mean1_bytes_len = (uint32_t)comp_mean1.size();
	hdr.m_run_bytes_len = (uint32_t)comp_run.size();
	hdr.m_coeff_bytes_len = (uint32_t)comp_coeff.size();
	hdr.m_sign_bits_len = (uint32_t)sign_bits.get_bytes().size();
	hdr.m_weight2_bits_len = (uint32_t)comp_weight2.size();
	hdr.m_weight3_bits_len = (uint32_t)comp_weight3.size();
	hdr.m_weight4_bits_len = (uint32_t)comp_weight4.size();
	hdr.m_weight8_bytes_len = (uint32_t)comp_weight8.size();

	comp_data.reserve(8192);
	
	comp_data.resize(sizeof(hdr));
	memcpy(comp_data.data(), &hdr, sizeof(hdr));

	comp_data.append(raw_bits.get_bytes());
	comp_data.append(comp_mode);
	comp_data.append(comp_solid_dpcm);
	
	comp_data.append(comp_endpoint_dpcm_reuse_indices);
	comp_data.append(comp_use_bc_bits);
	comp_data.append(comp_endpoint_dpcm_3bit);
	comp_data.append(comp_endpoint_dpcm_4bit);
	comp_data.append(comp_endpoint_dpcm_5bit);
	comp_data.append(comp_endpoint_dpcm_6bit);
	comp_data.append(comp_endpoint_dpcm_7bit);
	comp_data.append(comp_endpoint_dpcm_8bit);

	comp_data.append(comp_mean0);
	comp_data.append(comp_mean1);
	comp_data.append(comp_run);
	comp_data.append(comp_coeff);
	comp_data.append(sign_bits.get_bytes());
	comp_data.append(comp_weight2);
	comp_data.append(comp_weight3);
	comp_data.append(comp_weight4);
	comp_data.append(comp_weight8);

	if (comp_data.size() > UINT32_MAX)
		return false;

	if ((global_cfg.m_debug_images) || (global_cfg.m_debug_output))
	{
		image input_img(width, height);
		image coded_img(width, height);

		vector2D<astc_helpers::astc_block> phys_blocks(num_blocks_x, num_blocks_y);

		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				const astc_helpers::log_astc_block& log_blk = coded_blocks(bx, by);

				color_rgba block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

				bool status = astc_helpers::decode_block(log_blk, block_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
				if (!status)
				{
					fmt_error_printf("astc_helpers::decode_block() failed (1)\n");
					return false;
				}

				// Be positive the logical block can be unpacked correctly as XUASTC LDR.
				color_rgba block_pixels_alt[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
				bool status_alt = astc_helpers::decode_block_xuastc_ldr(log_blk, block_pixels_alt, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
				if (!status_alt)
				{
					fmt_error_printf("astc_helpers::decode_block_xuastc_ldr() failed\n");
					return false;
				}

				if (memcmp(block_pixels, block_pixels_alt, sizeof(color_rgba) * block_width * block_height) != 0)
				{
					fmt_error_printf("astc_helpers::decode_block_xuastc_ldr() decode pixel mismatch\n");
					return false;
				}

				coded_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);
								
				if (global_cfg.m_debug_images)
				{
					// input image

					status = astc_helpers::decode_block(input_blocks(bx, by), block_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
					if (!status)
					{
						fmt_error_printf("astc_helpers::decode_block() failed (2)\n");
						return false;
					}

					input_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);
				}

			} // bx

		} //by 

		if (global_cfg.m_debug_images)
		{
			save_png(global_cfg.m_debug_file_prefix + "input_img.png", input_img);
			debug_printf("Wrote input_img.png\n");

			save_png(global_cfg.m_debug_file_prefix + "coded_img.png", coded_img);
			debug_printf("Wrote coded_img.png\n");
		}

		if ((global_cfg.m_debug_output) && (global_cfg.m_debug_output_image_metrics))
		{
			debug_printf("Orig image vs. coded img:\n");
			print_image_metrics(orig_img, coded_img);

			debug_printf("display_astc_statistics:\n");
			display_astc_statistics(coded_blocks, block_width, block_height, orig_img.get_width(), orig_img.get_height(), false);
		}
	}

	if (global_cfg.m_debug_output)
	{
		fmt_debug_printf("Zstd compressed sizes:\n");

		fmt_debug_printf(" Raw bytes: {}\n", (uint64_t)raw_bits.get_bytes().size());
		fmt_debug_printf(" Mode bytes: {}, comp size: {}\n", (uint64_t)mode_bytes.size(), (uint64_t)comp_mode.size());
		fmt_debug_printf(" Solid DPCM bytes: {}, comp size: {}\n", (uint64_t)solid_dpcm_bytes.size(), (uint64_t)comp_solid_dpcm.size());
		
		fmt_debug_printf(" \n Endpoint DPCM Reuse Bytes: {}, comp size: {}\n", (uint64_t)endpoint_dpcm_reuse_indices.size(), (uint64_t)comp_endpoint_dpcm_reuse_indices.size());
		fmt_debug_printf(" Use BC bits bytes: {}, comp_size: {}\n", (uint64_t)use_bc_bits.get_bytes().size(), (uint64_t)comp_use_bc_bits.size());
		fmt_debug_printf(" Endpoint DPCM 3 bits: {}, comp size: {}\n", (uint64_t)endpoint_dpcm_3bit.get_bytes().size(), (uint64_t)comp_endpoint_dpcm_3bit.size());
		fmt_debug_printf(" Endpoint DPCM 4 bits: {}, comp size: {}\n", (uint64_t)endpoint_dpcm_4bit.get_bytes().size(), (uint64_t)comp_endpoint_dpcm_4bit.size());
		fmt_debug_printf(" Endpoint DPCM 5 bits: {}, comp size: {}\n", (uint64_t)endpoint_dpcm_5bit.size(), (uint64_t)comp_endpoint_dpcm_5bit.size());
		fmt_debug_printf(" Endpoint DPCM 6 bits: {}, comp size: {}\n", (uint64_t)endpoint_dpcm_6bit.size(), (uint64_t)comp_endpoint_dpcm_6bit.size());
		fmt_debug_printf(" Endpoint DPCM 7 bits: {}, comp size: {}\n", (uint64_t)endpoint_dpcm_7bit.size(), (uint64_t)comp_endpoint_dpcm_7bit.size());
		fmt_debug_printf(" Endpoint DPCM 8 bits: {}, comp size: {}\n", (uint64_t)endpoint_dpcm_8bit.size(), (uint64_t)comp_endpoint_dpcm_8bit.size());

		fmt_debug_printf(" \n Mean0 bytes: {} comp size: {}\n", (uint64_t)mean0_bits.get_bytes().size(), (uint64_t)comp_mean0.size());
		fmt_debug_printf(" Mean1 bytes: {} comp size: {}\n", (uint64_t)mean1_bytes.size(), (uint64_t)comp_mean1.size());
		fmt_debug_printf(" Run bytes: {} comp size: {}\n", (uint64_t)run_bytes.size(), (uint64_t)comp_run.size());
		fmt_debug_printf(" Coeff bytes: {} comp size: {}\n", (uint64_t)coeff_bytes.size(), (uint64_t)comp_coeff.size());
		fmt_debug_printf(" Sign bytes: {}\n", (uint64_t)sign_bits.get_bytes().size());
		fmt_debug_printf(" Weight2 bytes: {} comp size: {}\n", (uint64_t)weight2_bits.get_bytes().size(), (uint64_t)comp_weight2.size());
		fmt_debug_printf(" Weight3 bytes: {} comp size: {}\n", (uint64_t)weight3_bits.get_bytes().size(), (uint64_t)comp_weight3.size());
		fmt_debug_printf(" Weight4 bytes: {} comp size: {}\n", (uint64_t)weight4_bits.get_bytes().size(), (uint64_t)comp_weight4.size());
		fmt_debug_printf(" Weight8 bytes: {} comp size: {}\n", (uint64_t)weight8_bits.size(), (uint64_t)comp_weight8.size());

		fmt_debug_printf("\nTotal blocks: {}\n", total_blocks);
		fmt_debug_printf("Total runs: {}, run blocks: {}, non-run blocks: {}\n", total_runs, total_run_blocks, total_nonrun_blocks);
		fmt_debug_printf("Total lossy replacements: {}\n", total_lossy_replacements);
		fmt_debug_printf("Total solid blocks: {}\n", total_solid_blocks);
		fmt_debug_printf("Total full reuse commands: {}\n", total_full_reuse_commands);
		fmt_debug_printf("Total raw commands: {}\n", total_raw_commands);
		fmt_debug_printf("Total reuse full cfg emitted: {}\n", total_reuse_full_cfg_emitted);
		fmt_debug_printf("Total full cfg emitted: {}\n", total_full_cfg_emitted);
		fmt_debug_printf("Num part hash probes: {}, num part hash hits: {}\n", num_part_hash_probes, num_part_hash_hits);
		fmt_debug_printf("Total used endpoint dpcm: {}, total used endpoint raw: {}\n", total_used_endpoint_dpcm, total_used_endpoint_raw);
		fmt_debug_printf("Total used weight DCT: {}, total used weight DPCM: {}\n", total_used_dct, total_used_weight_dpcm);
		fmt_debug_printf("Total tm hash probes: {}, total tm hash_hits: {}\n", num_tm_hash_probes, num_tm_hash_hits);

		fmt_debug_printf("\nCompressed to {} bytes, {3.3}bpp\n\n", comp_data.size_u32(), ((float)comp_data.size() * 8.0f) / (float)total_pixels);
	}

	return true;
}
#endif

static uint64_t calc_block_wsse(
	const image& orig_img, int sx, int sy, int block_width, int block_height, 
	const image& tile_img, int tx, int ty,
	const astc_ldr::cem_encode_params& p)
{
	assert(((int)tile_img.get_width() == block_width) && ((int)tile_img.get_height() == block_height));

	uint64_t total_err = 0;

	for (int y = 0; y < block_height; y++)
	{
		const int oy = sy + y;
		if ((oy < 0) || (oy >= (int)orig_img.get_height()))
			continue;

		for (int x = 0; x < block_width; x++)
		{
			const int ox = sx + x;
			if ((ox < 0) || (ox >= (int)orig_img.get_width()))
				continue;
			
			total_err += weighted_color_error(orig_img(ox, oy), tile_img(tx + x, ty + y), p);
		} // x

	} // y

	return total_err;
}

static inline vec4F calc_color_delta(const color_rgba& a, const color_rgba& b)
{
	return vec4F(
		(float)(a.r - b.r),
		(float)(a.g - b.g),
		(float)(a.b - b.b),
		(float)(a.a - b.a));
}

static inline double calc_penalty(const vec4F& orig, const vec4F& cand, const vec4F &comp_weights)
{
	vec4F delta(orig - cand);
	delta = vec4F::component_mul(delta, delta);
	delta = vec4F::component_mul(delta, comp_weights);
	return delta[0] + delta[1] + delta[2] + delta[3];
}

static convar g_astc_refine_cross_block_penalty_weight("astc_refine_cross_block_penalty_weight", 2.5f, 0.0f, 1000.0f);

static uint64_t calc_cross_block_boundary_delta_mismatch(const image& orig_img, const image& candidate_img, uint32_t block_width, uint32_t block_height, uint32_t bx, uint32_t by, const astc_ldr::cem_encode_params& p)
{
	const int ofs_x = block_width * bx, ofs_y = block_height * by;

	const vec4F comp_weights((float)p.m_comp_weights[0], (float)p.m_comp_weights[1], (float)p.m_comp_weights[2], (float)p.m_comp_weights[3]);

	double penalty = 0.0f;

	// TODO: Compute in integer
	for (int x = 0; x < (int)block_width; x++)
	{
		vec4F orig_delta_top(calc_color_delta(orig_img.get_clamped(ofs_x + x, ofs_y), orig_img.get_clamped(ofs_x + x, ofs_y - 1)));
		vec4F cand_delta_top(calc_color_delta(candidate_img.get_clamped(ofs_x + x, ofs_y), candidate_img.get_clamped(ofs_x + x, ofs_y - 1)));
		penalty += calc_penalty(orig_delta_top, cand_delta_top, comp_weights);

		vec4F orig_delta_bot = calc_color_delta(orig_img.get_clamped(ofs_x + x, ofs_y + block_height - 1), orig_img.get_clamped(ofs_x + x, ofs_y + block_height - 1 + 1));
		vec4F cand_delta_bot = calc_color_delta(candidate_img.get_clamped(ofs_x + x, ofs_y + block_height - 1), candidate_img.get_clamped(ofs_x + x, ofs_y + block_height - 1 + 1));
		penalty += calc_penalty(orig_delta_bot, cand_delta_bot, comp_weights);
	} // x

	for (int y = 0; y < (int)block_height; y++)
	{
		vec4F orig_delta_left(calc_color_delta(orig_img.get_clamped(ofs_x, ofs_y + y), orig_img.get_clamped(ofs_x - 1, ofs_y + y)));
		vec4F cand_delta_left(calc_color_delta(candidate_img.get_clamped(ofs_x, ofs_y + y), candidate_img.get_clamped(ofs_x - 1, ofs_y + y)));
		penalty += calc_penalty(orig_delta_left, cand_delta_left, comp_weights);

		vec4F orig_delta_right(calc_color_delta(orig_img.get_clamped(ofs_x + block_width - 1, ofs_y + y), orig_img.get_clamped(ofs_x + block_width - 1 + 1, ofs_y + y)));
		vec4F cand_delta_right(calc_color_delta(candidate_img.get_clamped(ofs_x + block_width - 1, ofs_y + y), candidate_img.get_clamped(ofs_x + block_width - 1 + 1, ofs_y + y)));
		penalty += calc_penalty(orig_delta_right, cand_delta_right, comp_weights);
	} // x

	//const float PENALTY_WEIGHT = 2.5f;
	const float PENALTY_WEIGHT = g_astc_refine_cross_block_penalty_weight.get_float();
	return (uint64_t)std::round(penalty * PENALTY_WEIGHT);
}

static inline vec3F calc_ycbcr(const vec3F& c)
{
	const float r = c[0], g = c[1], b = c[2];

	float Y =  r *  0.212600f + g *  0.715200f + b *  0.072200f;
	float Cb = r * -0.114572f + g * -0.385428f + b *  0.500000f;
	float Cr = r *  0.500000f + g * -0.454153f + b * -0.045847f;
	
	return vec3F(Y, Cb, Cr);
}

static uint64_t calc_chroma_loss_penalty(const image& orig_img, const image& candidate_img, uint32_t block_width, uint32_t block_height, uint32_t bx, uint32_t by, const astc_ldr::cem_encode_params& p)
{
	color_rgba orig_block[astc_helpers::MAX_BLOCK_PIXELS];
	color_rgba cand_block[astc_helpers::MAX_BLOCK_PIXELS];

	orig_img.extract_block_clamped(orig_block, bx * block_width, by * block_height, block_width, block_height);
	candidate_img.extract_block_clamped(cand_block, bx * block_width, by * block_height, block_width, block_height);

	const uint32_t total_block_pixels = block_width * block_height;

	vec4F avg_orig(0.0f), avg_cand(0.0f);
	for (uint32_t i = 0; i < total_block_pixels; i++)
	{
		avg_orig += orig_block[i].get_vec4F();
		avg_cand += cand_block[i].get_vec4F();
	}

	avg_orig *= (1.0f / (float)total_block_pixels);
	avg_cand *= (1.0f / (float)total_block_pixels);
		
	vec3F o(calc_ycbcr(avg_orig));
	vec3F c(calc_ycbcr(avg_cand));

	vec3F delta(o - c);

	float penalty = (delta[1] * delta[1]) + (delta[2] * delta[2]);

	const float PENALTY_WEIGHT = ((float)total_block_pixels * .25f) * (float)p.m_comp_weights[1] * (14.0f * 14.0f);
	penalty *= PENALTY_WEIGHT;

	return (uint64_t)std::round(penalty);
}

static uint64_t calc_block_sse(
	uint32_t block_width, uint32_t block_height, 
	const color_rgba *pBlock_pixels,
	const astc_helpers::log_astc_block& log_astc_blk, 
	const ldr_astc_block_encode_image_high_level_config& enc_cfg)
{
	color_rgba dec_pixels[astc_helpers::MAX_BLOCK_PIXELS];
	bool dec_status = astc_helpers::decode_block(log_astc_blk, dec_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);

	assert(dec_status);
	if (!dec_status)
		return UINT64_MAX;

	const uint32_t total_block_pixels = block_width * block_height;

	uint64_t total_err = 0;
	for (uint32_t i = 0; i < total_block_pixels; i++)
		total_err += weighted_color_error(pBlock_pixels[i], dec_pixels[i], enc_cfg.m_cem_enc_params);

	return total_err;
}

struct deblocking_thread_state
{
	image m_deblock_orig;		// bw*3 x by*3
	image m_deblock_staging;	// bw*3 x by*3
	image m_deblock_temp;		// (bw+2) x (by+2)
};

// true on success
static bool deblocking_find_best_candidate(
	uint32_t block_width, uint32_t block_height, uint32_t num_blocks_x, uint32_t num_blocks_y,
	uint32_t pass, uint32_t bx, uint32_t by,
	deblocking_thread_state &thread_state,
	const image& candidate_img,
	const image& orig_img,
	const astc_ldr_encode_config& global_cfg,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	ldr_astc_block_encode_image_output& enc_out,
	uint32_t &new_best_packed_block_index, uint64_t &best_err)
{
	BASISU_NOTE_UNUSED(pass);

	best_err = UINT64_MAX;

	const astc_helpers::decode_mode dec_mode = global_cfg.m_astc_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8;
		
	ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

	const basisu::vector<encode_block_output>& out_blocks = blk_info.m_out_blocks;

	new_best_packed_block_index = blk_info.m_packed_out_block_index;
		
	const uint64_t cur_sse = out_blocks[blk_info.m_packed_out_block_index].m_sse;
	if (!cur_sse)
	{
		best_err = 0;
		return true;
	}

	// if a candidate has a WSSE worse than this, ignore it
	//const uint64_t skip_sse_thresh = (cur_sse * 4) / 3;
	const uint64_t skip_sse_thresh = UINT64_MAX;
		
	uint32_t best_cand_index = 0;

	// always 3x3 blocks centered around our current block
	image& deblock_orig_img = thread_state.m_deblock_orig;
	image& deblock_staging_img = thread_state.m_deblock_staging;
	image& deblock_temp_img = thread_state.m_deblock_temp;

	// grab 3x3 block region around block from the original image and the current output, place into thread local temporary buffer
	// this makes candidate evaluation simpler
	deblock_orig_img.blit_clamped(
		orig_img,
		((int)bx - 1) * (int)block_width, ((int)by - 1) * (int)block_height,
		block_width * 3, block_height * 3,
		0, 0);
		
	deblock_staging_img.blit_clamped(
		candidate_img,
		((int)bx - 1) * (int)block_width, ((int)by - 1) * (int)block_height, 
		block_width * 3, block_height * 3,
		0, 0);
		
	uint32_t total_neighbors_base_ofs = 0, total_neighbors_examined = 0;
	for (int k = 0; k < 4; k++)
	{
		int nbx = 0, nby = 0;

		if (k < 2)
			nbx = (int)bx + ((k & 1) ? -1 : 1);
		else
			nby = (int)by + (((k - 2) & 1) ? -1 : 1);

		if ((nbx < 0) || (nbx >= (int)num_blocks_x))
			continue;
		if ((nby < 0) || (nby >= (int)num_blocks_y))
			continue;

		ldr_astc_block_encode_image_output::block_info& neighbor_blk_info = enc_out.m_image_block_info(nbx, nby);

		const astc_helpers::log_astc_block& neighbor_block = neighbor_blk_info.m_out_blocks[neighbor_blk_info.m_packed_out_block_index].m_log_blk;

		if (neighbor_block.m_solid_color_flag_ldr)
			continue;

		if (astc_helpers::cem_is_ldr_base_scale(neighbor_block.m_color_endpoint_modes[0]))
			total_neighbors_base_ofs++;

		total_neighbors_examined++;
	}

	bool penalize_isolated_base_ofs_blocks = false;
	if (total_neighbors_examined)
	{
		const uint32_t neighbors_base_ofs_fract = (total_neighbors_base_ofs << 4) / total_neighbors_examined;

		const uint32_t PENALIZE_ISOLATED_BASE_OFS_FRACT = 8; // div 16

		penalize_isolated_base_ofs_blocks = neighbors_base_ofs_fract < PENALIZE_ISOLATED_BASE_OFS_FRACT;
	}

	[[maybe_unused]] bool applied_base_ofs_penalty = false;

	color_rgba block_pixels[astc_helpers::MAX_BLOCK_PIXELS];

	for (uint32_t cand_index = 0; cand_index < out_blocks.size(); cand_index++)
	{
		const encode_block_output& out_blk = out_blocks[cand_index];

		const uint64_t cand_sse = out_blocks[cand_index].m_sse;

		uint64_t block_wsse = UINT64_MAX, block_artifact_penalty_wsse = 0, chroma_loss_penalty_wsse = 0;
		uint64_t cand_err = UINT64_MAX;

		// skip if the pre-deblock error would just increase too much, likely not worth it
		if (cand_sse > skip_sse_thresh)
			continue;
			
		if (!astc_helpers::decode_block_xuastc_ldr(out_blk.m_log_blk, block_pixels, block_width, block_height, dec_mode))
		{
			assert(0);
			return false;
		}

		// insert candidate block into the center of our 3x3 block staging image
		deblock_staging_img.set_block_clipped(block_pixels, 1 * block_width, 1 * block_height, block_width, block_height);

		if (global_cfg.m_scd_will_postfilter)
		{
			// deblocks a region of (block_width+2)x(block_height+2), with source pixel offset of (-1,-1), output into deblock_temp_img
			deblock_block_region(block_width, block_height, deblock_staging_img, 1 * block_width, 1 * block_height, deblock_temp_img);

			block_wsse = calc_block_wsse(
				deblock_orig_img, 1 * block_width - 1, 1 * block_height - 1, block_width + 2, block_height + 2, 
				deblock_temp_img, 0, 0,
				enc_cfg.m_cem_enc_params);
		}
		else
		{
			block_wsse = 0;

			for (int y = 0; y < (int)block_height; y++)
				for (int x = 0; x < (int)block_width; x++)
					block_wsse += weighted_color_error(deblock_orig_img.get_clamped(1 * block_width + x, 1 * block_height + y), block_pixels[x + y * block_width], enc_cfg.m_cem_enc_params);

			// CPU/GPU postfilter is NOT going to be applied, but they want to deblock anyway. No impact to texels around this block.
			const uint32_t num_filtered_texels = (block_width + 2) * (block_height + 2);
			const uint32_t num_unfiltered_texels = block_width * block_height;

			// boost this wsse so the other penalties (tuned with filtering enabled) are roughly relative to it
			block_wsse = (block_wsse * num_filtered_texels) / num_unfiltered_texels;
		}

		block_artifact_penalty_wsse = calc_cross_block_boundary_delta_mismatch(deblock_orig_img, deblock_staging_img, block_width, block_height, 1, 1, enc_cfg.m_cem_enc_params);

		chroma_loss_penalty_wsse = global_cfg.m_scd_preserve_chroma ? calc_chroma_loss_penalty(deblock_orig_img, deblock_staging_img, block_width, block_height, 1, 1, enc_cfg.m_cem_enc_params) : 0;

		cand_err = block_wsse;

		if (penalize_isolated_base_ofs_blocks)
		{
			if (!out_blk.m_log_blk.m_solid_color_flag_ldr && astc_helpers::cem_is_ldr_base_scale(out_blk.m_log_blk.m_color_endpoint_modes[0]))
			{
				// penalize base-ofs candidates if there are not enough base-ofs neighbors, as they will likely be more visible
				cand_err = (cand_err * 5) / 4;
				//total_base_ofs_penalties++;
				applied_base_ofs_penalty = true;
			}
		}

		cand_err += block_artifact_penalty_wsse + chroma_loss_penalty_wsse;

		if ((cand_index != blk_info.m_packed_out_block_index) && (out_blk.m_log_blk.m_solid_color_flag_ldr))
		{
			// don't switch to solid unless it REALLY seems to help
			cand_err = cand_err * 8;
		}

		if (cand_err < best_err)
		{
			best_err = cand_err;
			best_cand_index = cand_index;
		}

	} // cand_index
				
	new_best_packed_block_index = best_cand_index;
		
	return true;
}

// ------

// true on success
static bool deblocking_compute_error(
	uint32_t block_width, uint32_t block_height, uint32_t num_blocks_x, uint32_t num_blocks_y,
	uint32_t bx, uint32_t by,
	image& candidate_img,
	const image& orig_img,
	const astc_ldr_encode_config& global_cfg,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	const ldr_astc_block_encode_image_output& enc_out,
	uint64_t& cur_err,
	image &deblock_orig, image &deblock_temp)
{
	assert(deblock_orig.get_width() == block_width);
	assert(deblock_orig.get_height() == block_height);

	assert(deblock_temp.get_width() == (block_width + 2));
	assert(deblock_temp.get_height() == (block_height + 2));

	const astc_helpers::decode_mode dec_mode = global_cfg.m_astc_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8;

	cur_err = UINT64_MAX;

	const ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

	const basisu::vector<encode_block_output>& out_blocks = blk_info.m_out_blocks;
				
	deblock_orig.blit(candidate_img,
		bx * block_width, by * block_height, block_width, block_height,
		0, 0,
		true);

	color_rgba block_pixels[astc_helpers::MAX_BLOCK_PIXELS];

	uint32_t total_neighbors_base_ofs = 0, total_neighbors_examined = 0;
	for (int k = 0; k < 4; k++)
	{
		int nbx = 0, nby = 0;

		if (k < 2)
			nbx = (int)bx + ((k & 1) ? -1 : 1);
		else
			nby = (int)by + (((k - 2) & 1) ? -1 : 1);

		if ((nbx < 0) || (nbx >= (int)num_blocks_x))
			continue;
		if ((nby < 0) || (nby >= (int)num_blocks_y))
			continue;

		const ldr_astc_block_encode_image_output::block_info& neighbor_blk_info = enc_out.m_image_block_info(nbx, nby);

		const astc_helpers::log_astc_block& neighbor_block = neighbor_blk_info.m_out_blocks[neighbor_blk_info.m_packed_out_block_index].m_log_blk;

		if (neighbor_block.m_solid_color_flag_ldr)
			continue;

		if (astc_helpers::cem_is_ldr_base_scale(neighbor_block.m_color_endpoint_modes[0]))
			total_neighbors_base_ofs++;

		total_neighbors_examined++;
	}

	bool penalize_isolated_base_ofs_blocks = false;
	if (total_neighbors_examined)
	{
		const uint32_t neighbors_base_ofs_fract = (total_neighbors_base_ofs << 4) / total_neighbors_examined;

		const uint32_t PENALIZE_ISOLATED_BASE_OFS_FRACT = 8; // div 16

		penalize_isolated_base_ofs_blocks = neighbors_base_ofs_fract < PENALIZE_ISOLATED_BASE_OFS_FRACT;
	}
		
	const uint32_t cand_index = blk_info.m_packed_out_block_index;

	{
		const encode_block_output& out_blk = out_blocks[cand_index];

		//const uint64_t cand_sse = out_blocks[cand_index].m_sse;

		uint64_t block_wsse = 0, block_artifact_penalty_wsse = 0, chroma_loss_penalty_wsse = 0;
		
		// skip if the pre-deblock error would just increase too much, likely not worth it

		if (!astc_helpers::decode_block_xuastc_ldr(out_blk.m_log_blk, block_pixels, block_width, block_height, dec_mode))
		{
			assert(0);
			return false;
		}

		candidate_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);

		if (global_cfg.m_scd_will_postfilter)
		{
			deblock_block_region(block_width, block_height, candidate_img, bx * block_width, by * block_height, deblock_temp);

			block_wsse = calc_block_wsse(orig_img, bx * block_width - 1, by * block_height - 1, block_width + 2, block_height + 2, deblock_temp, 0, 0, enc_cfg.m_cem_enc_params);
		}
		else
		{
			block_wsse = 0;

			const int sx = bx * block_width, sy = by * block_height;
			for (int y = 0; y < (int)block_height; y++)
				for (int x = 0; x < (int)block_width; x++)
					block_wsse += weighted_color_error(orig_img.get_clamped(sx + x, sy + y), block_pixels[x + y * block_width], enc_cfg.m_cem_enc_params);

			// CPU/GPU postfilter is NOT going to be applied, but they want to deblock anyway. No impact to texels around this block.
			const uint32_t num_filtered_texels = (block_width + 2) * (block_height + 2);
			const uint32_t num_unfiltered_texels = block_width * block_height;

			// boost this wsse so the other penalties (tuned with filtering enabled) are roughly relative to it
			block_wsse = (block_wsse * num_filtered_texels) / num_unfiltered_texels;
		}

		block_artifact_penalty_wsse = calc_cross_block_boundary_delta_mismatch(orig_img, candidate_img, block_width, block_height, bx, by, enc_cfg.m_cem_enc_params);

		chroma_loss_penalty_wsse = global_cfg.m_scd_preserve_chroma ? calc_chroma_loss_penalty(orig_img, candidate_img, block_width, block_height, bx, by, enc_cfg.m_cem_enc_params) : 0;

		uint64_t cand_err = block_wsse;

		if (penalize_isolated_base_ofs_blocks)
		{
			if (!out_blk.m_log_blk.m_solid_color_flag_ldr && astc_helpers::cem_is_ldr_base_scale(out_blk.m_log_blk.m_color_endpoint_modes[0]))
			{
				// penalize base-ofs candidates if there are not enough base-ofs neighbors, as they will likely be more visible
				cand_err = (cand_err * 5) / 4;
				//total_base_ofs_penalties++;
			}
		}

		cand_err += block_artifact_penalty_wsse + chroma_loss_penalty_wsse;

		cur_err = cand_err;
	}

	candidate_img.blit(deblock_orig,
		0, 0, block_width, block_height,
		bx * block_width, by * block_height,
		true);

	return true;
}

static uint64_t deblocking_compute_overall_error(
	uint32_t block_width, uint32_t block_height, uint32_t num_blocks_x, uint32_t num_blocks_y,
	const image& orig_img,
	const astc_ldr_encode_config& global_cfg,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	const ldr_astc_block_encode_image_output& enc_out)
{
	const uint32_t width = orig_img.get_width(), height = orig_img.get_height();

	image candidate_img;
	candidate_img.resize(width, height);

	const astc_helpers::decode_mode dec_mode = global_cfg.m_astc_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8;

	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			const ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);
			const encode_block_output& best_blk = blk_info.m_out_blocks[blk_info.m_packed_out_block_index];

			const astc_helpers::log_astc_block& log_blk = best_blk.m_log_blk;

			color_rgba block_pixels[astc_helpers::MAX_BLOCK_PIXELS];

			if (!astc_helpers::decode_block_xuastc_ldr(log_blk, block_pixels, block_width, block_height, dec_mode))
				return false;

			candidate_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);
			
		} // bx

	} // by

	const image orig_candidate_img(candidate_img);

	image deblock_orig(block_width, block_height);
	image deblock_temp(block_width + 2, block_height + 2);

	uint64_t overall_err = 0;

	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			uint64_t cur_err = 0;

			if (!deblocking_compute_error(
				block_width, block_height, num_blocks_x, num_blocks_y,
				bx, by,
				candidate_img,
				orig_img,
				global_cfg,
				enc_cfg,
				enc_out,
				cur_err,
				deblock_orig, deblock_temp))
			{
				return UINT64_MAX;
			}
			
			overall_err += cur_err;

		} // bx
	} // by

	fmt_printf("Overall image error: {}, {3.3} per block\n", overall_err, (double)overall_err / (double)(num_blocks_x * num_blocks_y));

	uint32_t total_cand_differences = 0;

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			if (candidate_img(x, y) != orig_candidate_img(x, y))
				total_cand_differences++;
		}
	}

	fmt_printf("Total cand differences: {}\n", total_cand_differences);
	return overall_err;
}

// ------

static convar g_astc_refine_max_new_blocks("astc_refine_max_new_blocks", 16, 1, 256);

static convar g_astc_refine_try_nonprimary_candidates("astc_refine_try_nonprimary_candidates", 1, 0, 1);
static convar g_astc_refine_mutate_part_id_prob("astc_refine_mutate_part_id_prob", 10, 0, 100);
static convar g_astc_refine_mutate_endpoint_edge("astc_refine_mutate_endpoint_edge", 1, 0, 1);
static convar g_astc_refine_mutate_dct("astc_refine_mutate_dct", 1, 0, 1);
static convar g_astc_refine_mutate_endpoints("astc_refine_mutate_endpoints", 1, 0, 1);
static convar g_astc_refine_base_seed("astc_refine_base_seed", 0, INT_MIN, INT_MAX);

static bool mutate_candidates(
	uint32_t pass, 
	uint32_t block_width, uint32_t block_height, uint32_t num_blocks_x, uint32_t num_blocks_y, uint32_t total_blocks,
	const uint32_t max_new_blocks, const uint_vec* const pJob_block_list, const vector2D<float> &block_std_dev,
	const uint32_t NUM_SIMILAR_PATS, const vector2D<uint16_t> similar_pats[2], const vector2D<uint8_t> similar_pat_perm_index[2],
	const image& orig_img, const image& candidate_img,
	uint32_t num_threads, job_pool &jp, 
	const astc_ldr_encode_config& global_cfg, const ldr_astc_block_encode_image_high_level_config& enc_cfg, ldr_astc_block_encode_image_output& enc_out)
{
	assert(num_threads);

	basist::astc_ldr_t::grid_weight_dct grid_coder;
	grid_coder.init(block_width, block_height);

	float rnd_mag = 1.7f;

	if (pass >= 16)
		rnd_mag = .9f;
	else if (pass >= 10)
		rnd_mag = 1.5f;
	else if (pass >= 6)
		rnd_mag = 1.7f;
	else if (pass >= 4)
		rnd_mag = 2.5f;

	std::atomic<uint32_t> cur_block_index;
	cur_block_index.store(0);

	std::atomic<bool> encoder_failed_flag;
	encoder_failed_flag.store(false);

	for (uint32_t job_index = 0; job_index < num_threads; job_index++)
	{
		jp.add_job([job_index, num_threads,
			num_blocks_x, num_blocks_y, block_width, block_height, total_blocks, &grid_coder,
			pJob_block_list, &block_std_dev, max_new_blocks, rnd_mag,
			pass,
			&cur_block_index, &encoder_failed_flag,
			&candidate_img, &similar_pats, &similar_pat_perm_index, NUM_SIMILAR_PATS,
			&orig_img,
			&global_cfg, &enc_cfg, &enc_out]
			{
				BASISU_NOTE_UNUSED(job_index); BASISU_NOTE_UNUSED(num_threads); BASISU_NOTE_UNUSED(num_blocks_y); BASISU_NOTE_UNUSED(pJob_block_list);
				// Thread locals
				basist::astc_ldr_t::fvec dct_temp;

				const uint32_t block_size_index = astc_helpers::find_astc_block_size_index(block_width, block_height);
				[[maybe_unused]] const uint32_t num_unique_subset_pats[2] = { basist::astc_ldr_t::get_total_unique_patterns(block_size_index, 2), basist::astc_ldr_t::get_total_unique_patterns(block_size_index, 3) };

				uint64_vec sorted_candidates;
				sorted_candidates.reserve(1024);

				for (; ;)
				{
					// Asynchronously fetch the next block index to process.
					const uint32_t block_index = cur_block_index.fetch_add(1);
					if (block_index >= total_blocks)
						break;

					const uint32_t bx = block_index % num_blocks_x;
					const uint32_t by = block_index / num_blocks_x;

					sorted_candidates.resize(0);

					// first retire old mutated candidates (older than 4 passes)
					{
						ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

						basisu::vector<encode_block_output>& cur_out_blocks = blk_info.m_out_blocks;

						basisu::vector<encode_block_output> new_out_blocks;
						new_out_blocks.reserve(1 + (cur_out_blocks.size_u32() / 2));

						int new_packed_out_block_index = -1;

						for (uint32_t i = 0; i < cur_out_blocks.size(); i++)
						{
							const encode_block_output& blk = cur_out_blocks[i];

							bool should_remove = false;

							if (i == blk_info.m_packed_out_block_index)
							{
								new_packed_out_block_index = new_out_blocks.size_u32();
							}
							else
							{
								if (blk.m_blur_id >= BLUR_ID_EXP)
								{
									int blk_pass = blk.m_blur_id - BLUR_ID_EXP;

									if (blk_pass <= (int)(pass - 4))
										should_remove = true;
								}
							}

							if (!should_remove)
							{
								sorted_candidates.push_back((blk.m_sse << 16u) | new_out_blocks.size());

								new_out_blocks.push_back(blk);
							}

						} // i

						assert(new_packed_out_block_index != -1);

						blk_info.m_packed_out_block_index = new_packed_out_block_index;
						blk_info.m_out_blocks.swap(new_out_blocks);
					}

					sorted_candidates.sort();

					// generate new candidates via mutation
					{
						basisu::rand rnd;

						const uint64_t h = 1ull + pass * (4096ull * 4096ull) + block_index;
						rnd.seed(g_astc_refine_base_seed.get_int() + basist::hash_hsieh(reinterpret_cast<const uint8_t*>(&h), sizeof(h))); // endianness (not a big deal here)

						ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

						basisu::vector<encode_block_output>& out_blocks = blk_info.m_out_blocks;

						{
							const encode_block_output& best_blk = out_blocks[blk_info.m_packed_out_block_index];
							const astc_helpers::log_astc_block& log_blk = best_blk.m_log_blk;
							if (log_blk.m_solid_color_flag_ldr)
								continue;

							//uint32_t cem_index = log_blk.m_color_endpoint_modes[0];
							// TODO
							//if ((cem_index != 6) && (cem_index != 8) && (cem_index != 9))
							//	continue;
						}

						color_rgba orig_block[astc_helpers::MAX_BLOCK_PIXELS];
						orig_img.extract_block_clamped(orig_block, bx * block_width, by * block_height, block_width, block_height);

						const uint32_t num_rand_blocks = clamp<int>((int)std::round(block_std_dev(bx, by) * .25f), 1, max_new_blocks) + 2;

						for (uint32_t q = 0; q < num_rand_blocks; q++)
						{
							const encode_block_output* pBest_blk = &out_blocks[blk_info.m_packed_out_block_index];

							bool choose_nonprimary_cand_flag = false;

							if (g_astc_refine_try_nonprimary_candidates.get_bool())
							{
								if ((q >= (num_rand_blocks - 2)) && (sorted_candidates.size_u32() >= 2))
								{
									uint32_t ix = rnd.irand(0, minimum(10u, sorted_candidates.size_u32()) - 1);

									uint32_t rnd_cand_index = sorted_candidates[ix] & UINT16_MAX;
									pBest_blk = &out_blocks[rnd_cand_index];
								
									if (pBest_blk->m_log_blk.m_solid_color_flag_ldr)
									{
										rnd_cand_index = sorted_candidates[(ix + 1) % sorted_candidates.size_u32()] & UINT16_MAX;
										pBest_blk = &out_blocks[rnd_cand_index];
									}
									
									choose_nonprimary_cand_flag = true;
								}
							}

							const encode_block_output& best_blk = *pBest_blk;

							if (best_blk.m_log_blk.m_solid_color_flag_ldr)
								continue;

							const astc_helpers::log_astc_block& log_blk = best_blk.m_log_blk;
							
							const uint32_t cem_index = log_blk.m_color_endpoint_modes[0];
							const uint32_t num_cem_vals = astc_helpers::get_num_cem_values(cem_index);

							astc_helpers::log_astc_block new_log_blk(log_blk);

							const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(log_blk.m_endpoint_ise_range);

							bool any_changed = false;

							basist::astc_ldr_t::dct_syms new_packed_dct_plane_data[2];
							bool has_new_packed_dct_plane_data = false;

							if (choose_nonprimary_cand_flag)
								new_log_blk.m_user_mode |= 8;

							if (g_astc_refine_mutate_part_id_prob.get_int() && (new_log_blk.m_num_partitions >= 2) && rnd.iprob(g_astc_refine_mutate_part_id_prob.get_int()))
							{
								// partition pattern mutation
								const uint32_t r = rnd.irand(0, NUM_SIMILAR_PATS - 1);

								const uint32_t k = similar_pats[new_log_blk.m_num_partitions - 2](new_log_blk.m_partition_id, r);

								if (k != UINT16_MAX)
								{
									new_log_blk.m_partition_id = safe_cast_uint16(k);

									const uint32_t perm_index = similar_pat_perm_index[new_log_blk.m_num_partitions - 2](new_log_blk.m_partition_id, r);

									// now permute the endpoints if necessary so they roughly match
									if (perm_index)
									{
										//const uint32_t num_cem_vals = astc_helpers::get_num_cem_values(new_log_blk.m_color_endpoint_modes[0]);

										uint8_t cur_endpoints[astc_helpers::MAX_ENDPOINT_VALS];
										memcpy(cur_endpoints, new_log_blk.m_endpoints, num_cem_vals * new_log_blk.m_num_partitions);

										for (uint32_t src_index = 0; src_index < new_log_blk.m_num_partitions; src_index++)
										{
											uint32_t dst_index;
											if (new_log_blk.m_num_partitions == 2)
												dst_index = src_index ^ 1;
											else
												dst_index = g_part3_mapping[perm_index][src_index];

											memcpy(new_log_blk.m_endpoints + dst_index * num_cem_vals, cur_endpoints + src_index * num_cem_vals, num_cem_vals);
										}
									}

									new_log_blk.m_user_mode |= 1;

									any_changed = true;
								}
							}

							if (!any_changed && g_astc_refine_mutate_endpoint_edge.get_bool() && rnd.iprob(10))
							{
								// lerp endpoint towards adjacent block edge mutation
								const uint32_t subset_index = (new_log_blk.m_num_partitions == 1) ? 0 : rnd.irand(0, new_log_blk.m_num_partitions - 1);

								vec4F avg_edge(0.0f);
								const uint32_t edge_index = rnd.irand(0, 3);

								if ((edge_index == 0) || (edge_index == 2))
								{
									// top (0) or bottom (2)
									for (uint32_t x = 0; x < block_width; x++)
									{
										const int src_x = bx * block_width + x;
										const int src_y = ((int)by * (int)block_height) + ((edge_index == 0) ? -1 : (int)block_height);
										avg_edge += candidate_img.get_clamped(src_x, src_y).get_vec4F();
									}
									avg_edge *= (1.0f / (float)block_width);
								}
								else
								{
									// right (1) or left (3)
									for (uint32_t y = 0; y < block_height; y++)
									{
										const int src_x = ((int)bx * (int)block_width) + ((edge_index == 1) ? (int)block_width : -1);
										const int src_y = by * block_height + y;
										avg_edge += candidate_img.get_clamped(src_x, src_y).get_vec4F();
									}
									avg_edge *= (1.0f / (float)block_height);
								}

								uint8_t* pDst_endpoints = new_log_blk.m_endpoints + num_cem_vals * subset_index;

								color_rgba le, he;
								decode_endpoints(cem_index, pDst_endpoints, new_log_blk.m_endpoint_ise_range, le, he, nullptr);

								const float lrp = rnd.frand(0.0f, .3f);
								const uint32_t ei = rnd.irand(0, 2);

								for (uint32_t i = 0; i < 4; i++)
								{
									if ((ei == 0) || (ei == 2))
										le[i] = (uint8_t)clamp(basisu::fast_roundf_int(lerp<float>((float)le[i], avg_edge[i], lrp)), 0, 255);

									if ((ei == 1) || (ei == 2))
										he[i] = (uint8_t)clamp(basisu::fast_roundf_int(lerp<float>((float)he[i], avg_edge[i], lrp)), 0, 255);
								} // i

								const float fendpoints[8] = {
									(float)le[0], (float)he[0],
									(float)le[1], (float)he[1],
									(float)le[2], (float)he[2],
									(float)le[3], (float)he[3] };

								uint8_t new_endpoints[8] = { };

								astc_ldrf::cem_encode(cem_index, fendpoints, new_log_blk.m_endpoint_ise_range, new_endpoints, true, false);

								if (memcmp(pDst_endpoints, new_endpoints, num_cem_vals) != 0)
								{
									memcpy(pDst_endpoints, new_endpoints, num_cem_vals);
									new_log_blk.m_user_mode |= 2;
									any_changed = true;
								}
							}

							if (!any_changed && global_cfg.m_use_dct && g_astc_refine_mutate_dct.get_bool() && (rnd.iprob(10)))
							{
								// DCT coefficient mutation
								if (rnd.iprob(10))
								{
									// DC
									const uint32_t plane_to_change = new_log_blk.m_dual_plane ? rnd.irand(0, 1) : 0;

									for (uint32_t plane_index = 0; plane_index < (new_log_blk.m_dual_plane ? 2u : 1u); plane_index++)
									{
										auto& new_coeffs = new_packed_dct_plane_data[plane_index];

										new_coeffs = best_blk.m_packed_dct_plane_data[plane_index];

										if (plane_to_change == plane_index)
										{
											int new_dc_sym = new_coeffs.m_dc_sym;

											new_dc_sym += rnd.bit() ? -1 : 1; //basisu::fast_roundf_int((float)new_dc_sym + rnd.gaussian(0.0f, 1.0f));

											new_dc_sym = clamp<int>(new_dc_sym, 0, new_coeffs.m_num_dc_levels - 1);

											if (new_dc_sym != (int)new_coeffs.m_dc_sym)
											{
												new_coeffs.m_dc_sym = new_dc_sym;

												any_changed = true;
												has_new_packed_dct_plane_data = true;
												new_log_blk.m_user_mode |= 16;
											}
										}
									}
								}

								if (!any_changed)
								{
									// AC
									for (uint32_t plane_index = 0; plane_index < (new_log_blk.m_dual_plane ? 2u : 1u); plane_index++)
									{
										auto& new_coeffs = new_packed_dct_plane_data[plane_index];

										new_coeffs = best_blk.m_packed_dct_plane_data[plane_index];

										uint32_t total_coeffs = new_coeffs.m_coeffs.size_u32();

										if ((total_coeffs) && (new_coeffs.m_coeffs[total_coeffs - 1].m_coeff == INT16_MAX))
											total_coeffs--;

										if (!total_coeffs)
											continue;

										int rnd_coeff_index = rnd.irand(0, total_coeffs - 1);

										int cur_coeff = new_coeffs.m_coeffs[rnd_coeff_index].m_coeff;

										if (rnd.iprob(10))
											cur_coeff = -cur_coeff;
										else if (rnd.bit())
											cur_coeff++;
										else
											cur_coeff--;

										if ((!cur_coeff) || (iabs(cur_coeff) > basist::astc_ldr_t::DCT_MAX_ARITH_COEFF_MAG))
											continue;

										new_coeffs.m_coeffs[rnd_coeff_index].m_coeff = safe_cast_int16(cur_coeff);
										new_coeffs.m_max_coeff_mag = basisu::maximum(new_coeffs.m_max_coeff_mag, iabs(cur_coeff));

										any_changed = true;
										has_new_packed_dct_plane_data = true;
										new_log_blk.m_user_mode |= 4;

									} // plane_index
								} // if (!any_changed)
							}

							if ((!any_changed) && g_astc_refine_mutate_endpoints.get_bool())
							{
								// gaussian endpoint CEM value mutation
								const uint32_t subset_index = (log_blk.m_num_partitions == 1) ? 0 : rnd.irand(0, log_blk.m_num_partitions - 1);
								const uint32_t e = rnd.bit();

								const bool cem_has_alpha = astc_helpers::does_cem_have_alpha(cem_index);

								bool mutate_rgb = true, mutate_alpha = false;

								if (cem_has_alpha)
								{
									uint32_t c = rnd.irand(0, 2);
									mutate_rgb = (c == 0) || (c == 2);
									mutate_alpha = (c == 1) || (c == 2);
								}

								if (mutate_rgb)
								{
									uint32_t e_ofs = subset_index * num_cem_vals, e_stride = 0, nc = 0;
									
									switch (cem_index)
									{
									case astc_helpers::CEM_LDR_LUM_DIRECT:
									case astc_helpers::CEM_LDR_LUM_ALPHA_DIRECT:
									{
										e_ofs += e;
										e_stride = 2;
										nc = 1;
										break;
									}
									case astc_helpers::CEM_LDR_RGB_BASE_SCALE:
									case astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A:
									{
										// e=0 -> scale, e=1 -> base rgb
										e_ofs += (e ? 0 : 3);
										e_stride = 1;
										nc = e ? 3 : 1;
										break;
									}
									case astc_helpers::CEM_LDR_RGB_DIRECT:
									case astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET:
									case astc_helpers::CEM_LDR_RGBA_DIRECT:
									case astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET:
									{
										e_ofs += e;
										e_stride = 2;
										nc = 3;
										break;
									}
									default:
									{
										assert(0);
										break;
									}
									}

									for (uint32_t c = 0; c < nc; c++)
									{
										assert((e_ofs + c * e_stride) < (num_cem_vals * new_log_blk.m_num_partitions));
										const int cur_rank = endpoint_tab.m_ISE_to_rank[new_log_blk.m_endpoints[e_ofs + c * e_stride]];

										int new_rank = clamp<int>((int)std::round(cur_rank + rnd.gaussian(0.0f, rnd_mag)), 0, astc_helpers::get_ise_levels(log_blk.m_endpoint_ise_range) - 1);

										if (new_rank != cur_rank)
											any_changed = true;

										new_log_blk.m_endpoints[e_ofs + c * e_stride] = endpoint_tab.m_rank_to_ISE[new_rank];
									}

								} // mutate_rgb

								if (mutate_alpha)
								{
									uint32_t e_ofs = subset_index * num_cem_vals;
									
									switch (cem_index)
									{
									case astc_helpers::CEM_LDR_LUM_ALPHA_DIRECT:
									{
										e_ofs += 2 + e;
										break;
									}
									case astc_helpers::CEM_LDR_RGB_BASE_SCALE_PLUS_TWO_A:
									{
										e_ofs += 4 + e;
										break;
									}
									case astc_helpers::CEM_LDR_RGBA_DIRECT:
									case astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET:
									{
										e_ofs += 6 + e;
										break;
									}
									default:
									{
										assert(0);
										break;
									}
									}

									{
										assert(e_ofs < num_cem_vals * new_log_blk.m_num_partitions);
										const int cur_rank = endpoint_tab.m_ISE_to_rank[new_log_blk.m_endpoints[e_ofs]];

										int new_rank = clamp<int>((int)std::round(cur_rank + rnd.gaussian(0.0f, rnd_mag)), 0, astc_helpers::get_ise_levels(log_blk.m_endpoint_ise_range) - 1);

										if (new_rank != cur_rank)
											any_changed = true;

										new_log_blk.m_endpoints[e_ofs] = endpoint_tab.m_rank_to_ISE[new_rank];
									}

								} // mutate_alpha

							}

							if (!any_changed)
								continue;

							encode_block_output new_out_block;
							new_out_block.m_trial_mode_index = best_blk.m_trial_mode_index;
							new_out_block.m_blur_id = safe_cast_uint16(BLUR_ID_EXP + pass);

							if (enc_cfg.m_use_dct)
							{
								if (has_new_packed_dct_plane_data)
								{
									new_out_block.m_packed_dct_plane_data[0] = new_packed_dct_plane_data[0];
									new_out_block.m_packed_dct_plane_data[1] = new_packed_dct_plane_data[1];
								}
								else
								{
									new_out_block.m_packed_dct_plane_data[0] = best_blk.m_packed_dct_plane_data[0];
									new_out_block.m_packed_dct_plane_data[1] = best_blk.m_packed_dct_plane_data[1];
								}

								for (uint32_t plane_index = 0; plane_index < (new_log_blk.m_dual_plane ? 2u : 1u); plane_index++)
								{
									basist::astc_ldr_t::dct_syms& syms = new_out_block.m_packed_dct_plane_data[plane_index];

									bool status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, new_log_blk, nullptr, nullptr, dct_temp, &syms);
									assert(status);

									if (!status)
									{
										encoder_failed_flag.store(true);
										return;
									}
								}
							}

							new_out_block.m_log_blk = new_log_blk;

							new_out_block.m_sse = calc_block_sse(block_width, block_height, orig_block, new_log_blk, enc_cfg);

							out_blocks.push_back(new_out_block);
						} // q
					}

				} // for (; ;)

			}
		);

	} // job_index

	jp.wait_for_all();

	if (encoder_failed_flag)
	{
		fmt_error_printf("refine_output_for_deblocking: Threaded deblocking pass failed! (2)\n");
		return false;
	}

	return true;
}

static bool refine_output_for_deblocking(
	const image& orig_img,
	const astc_ldr_encode_config& global_cfg,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg,
	ldr_astc_block_encode_image_output& enc_out)
{
	if (global_cfg.m_debug_output)
		fmt_debug_printf("------------------- refine_output_for_deblocking:\n");

	const uint32_t width = orig_img.get_width();
	const uint32_t height = orig_img.get_height();
	const uint32_t block_width = global_cfg.m_astc_block_width;
	const uint32_t block_height = global_cfg.m_astc_block_height;
	const uint32_t total_block_pixels = block_width * block_height;

	//const uint32_t total_pixels = width * height;
	const uint32_t num_blocks_x = (width + block_width - 1) / block_width;
	const uint32_t num_blocks_y = (height + block_height - 1) / block_height;
	const uint32_t total_blocks = num_blocks_x * num_blocks_y;

	uint64_t initial_deblocked_err = 0;
	if (global_cfg.m_debug_output)
	{
		initial_deblocked_err = deblocking_compute_overall_error(
			block_width, block_height, num_blocks_x, num_blocks_y,
			orig_img,
			global_cfg,
			enc_cfg,
			enc_out);
	}

	image candidate_img;
	candidate_img.resize(width, height);

	const astc_helpers::decode_mode dec_mode = global_cfg.m_astc_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8;

	uint32_t num_min_candidates = UINT32_MAX, num_max_candidates = 1;
	uint32_t total_overall_candidates = 0;

	vector2D<float> block_std_dev(num_blocks_x, num_blocks_y);

	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);
			const encode_block_output& best_blk = blk_info.m_out_blocks[blk_info.m_packed_out_block_index];

			const astc_helpers::log_astc_block& log_blk = best_blk.m_log_blk;

			color_rgba block_pixels[astc_helpers::MAX_BLOCK_PIXELS];

			if (!astc_helpers::decode_block_xuastc_ldr(log_blk, block_pixels, block_width, block_height, dec_mode))
				return false;

			vec4F sum(0.0f), sum2(0.0f);
			for (uint32_t i = 0; i < total_block_pixels; i++)
			{
				vec4F p(block_pixels[i].get_vec4F());
				sum += p;
				sum2 += vec4F::component_mul(p, p);
			}

			const float oo_total_texels = 1.0f / (float)total_block_pixels;

			const vec4F var = vec4F::component_max(sum2 - vec4F::component_mul(sum, sum) * oo_total_texels, vec4F(0.0f)) * oo_total_texels;
			const vec4F std_dev = vec4F::component_sqrt(var);

			const float block_stddev = basisu::maximum(std_dev[0], std_dev[1], std_dev[2], std_dev[3]);
			for (int dy = -1; dy <= 1; dy++)
			{
				const int y = by + dy;
				if ((y < 0) || (y >= (int)num_blocks_y))
					continue;
				for (int dx = -1; dx <= 1; dx++)
				{
					const int x = bx + dx;
					if ((x < 0) || (x >= (int)num_blocks_x))
						continue;
					block_std_dev(x, y) = basisu::maximum<float>(block_std_dev(x, y), block_stddev);
				} // dx 
			} // dy

			candidate_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);

			num_min_candidates = minimum(num_min_candidates, blk_info.m_out_blocks.size_u32());
			num_max_candidates = maximum(num_max_candidates, blk_info.m_out_blocks.size_u32());

			total_overall_candidates += blk_info.m_out_blocks.size_u32();
		} // bx

	} // by

	if (global_cfg.m_debug_images)
	{
		image std_dev_vis(width, height);
		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				const float std_dev = block_std_dev(bx, by);

				const float STD_DEV_SCALE = 4.0f;
				std_dev_vis.fill_box(bx * block_width, by * block_height, block_width, block_height, color_rgba((uint8_t)std::min(255.0f, std_dev * STD_DEV_SCALE), 255));
			}
		}

		save_png("deblock_std_dev_vis.png", std_dev_vis);
		fmt_printf("Wrote deblock_std_dev_vis.png\n");
	}

	if (global_cfg.m_debug_output)
	{
		fmt_debug_printf("Total candidates: {}, Avg candidates per block: {}, Min candidates: {}, Max candidates: {}\n",
			total_overall_candidates,
			(float)total_overall_candidates / (float)total_blocks,
			num_min_candidates, num_max_candidates);
	}

	{
		image candidate_img_deblocked;
		deblock_image(block_width, block_height, candidate_img, candidate_img_deblocked);

		if (global_cfg.m_debug_images)
		{
			save_png(global_cfg.m_debug_file_prefix + "deblock_initial_candidate_img.png", candidate_img);
			save_png(global_cfg.m_debug_file_prefix + "deblock_initial_candidate_img_deblocked.png", candidate_img_deblocked);
		}

		if ((global_cfg.m_debug_output) && (global_cfg.m_debug_output_image_metrics))
		{
			fmt_debug_printf("orig vs. initial candidate image:\n");
			print_image_metrics(orig_img, candidate_img);

			fmt_debug_printf("\norig vs. initial candidate image deblocked:\n");
			print_image_metrics(orig_img, candidate_img_deblocked);
		}
	}

	if (num_max_candidates == 1)
	{
		if (global_cfg.m_debug_output)
			fmt_debug_printf("No candidates to try for any block, exiting.\n");
		return true;
	}

	image deblock_orig(block_width * 3, block_height * 3);
	image deblock_temp(block_width + 2, block_height + 2);

#if 0
	image debug_block_image;
	uint32_t debug_block_cur_x = 0, debug_block_cur_y = 0;
	const int DEBUG_IMG_COL_WIDTH = 680;
	if (global_cfg.m_debug_block_x != -1)
	{
		debug_block_image.resize(3072, 2048);

		debug_block_image.blit(orig_img,
			global_cfg.m_debug_block_x * block_width, global_cfg.m_debug_block_y * block_height, block_width, block_height,
			debug_block_cur_x, debug_block_cur_y,
			true);

		debug_block_cur_y += block_height + 2;

		debug_printf("Debug block coordinate {}x{}\n", global_cfg.m_debug_block_x, global_cfg.m_debug_block_y);
	}
#endif
		
	uint_vec white_list, black_list;
	white_list.reserve(total_blocks);
	black_list.reserve(total_blocks);
	for (uint32_t y = 0; y < num_blocks_y; y++)
	{
		for (uint32_t x = 0; x < num_blocks_x; x++)
		{
			const uint32_t color_index = (x ^ y) & 1;
			(color_index ? black_list : white_list).push_back(x + y * num_blocks_x);
		} // x
	} // y

	vector2D<uint64_t> block_best_err(num_blocks_x, num_blocks_y);
	block_best_err.set_all(UINT64_MAX);

	const uint32_t max_new_blocks = ((width * height) >= (2048 * 2048)) ? ((g_astc_refine_max_new_blocks.get_int() + 1) / 2) : g_astc_refine_max_new_blocks.get_int();

	assert(enc_cfg.m_pJob_pool);
	job_pool& job_pool = *enc_cfg.m_pJob_pool;

	const uint32_t num_threads = (uint32_t)job_pool.get_total_threads();
	assert(num_threads);
		
	const uint32_t num_passes = clamp<uint32_t>(global_cfg.m_num_scd_passes, 2, 256);
		
	// TODO
	const bool mutation_enabled = true;

	vector2D<uint32_t> orig_best_block_indices(num_blocks_x, num_blocks_y);
	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

			orig_best_block_indices(bx, by) = blk_info.m_packed_out_block_index;
		}
	}

#if 1
	// TODO: Move to init, this is per-mipmap
	const uint32_t NUM_SIMILAR_PATS = 16;
	vector2D<uint16_t> similar_pats[2];
	vector2D<uint8_t> similar_pat_perm_index[2];
	for (uint32_t p = 0; p < 2; p++)
	{
		similar_pats[p].resize(1024, NUM_SIMILAR_PATS);
		similar_pat_perm_index[p].resize(1024, NUM_SIMILAR_PATS);

		similar_pats[p].set_all(UINT16_MAX);
	}
	
	const uint32_t block_size_index = astc_helpers::find_astc_block_size_index(block_width, block_height);
		
	for (uint32_t num_subsets = 2; num_subsets <= 3; num_subsets++)
	{
		const uint32_t num_unique_subset_pats = basist::astc_ldr_t::get_total_unique_patterns(block_size_index, num_subsets);
		
		const partitions_data& pat_data = (num_subsets == 2) ? enc_out.m_part_data_p2 : enc_out.m_part_data_p3;

		assert(num_unique_subset_pats == pat_data.m_total_unique_patterns);

		for (uint32_t unique_pat_index = 0; unique_pat_index < num_unique_subset_pats; unique_pat_index++)
		{
			const uint32_t seed_index = pat_data.m_unique_index_to_part_seed[unique_pat_index];

			const partition_pattern_vec& desired_pat_vec = pat_data.m_partition_pats[unique_pat_index];
			
			uint32_t results[NUM_SIMILAR_PATS + 1];

			uint32_t num_results = pat_data.m_part_lhs_map.find(desired_pat_vec, results, NUM_SIMILAR_PATS + 1, false);
			
			uint32_t dst_index = 0;

			for (uint32_t i = 0; i < num_results; i++)
			{
				assert(results[i] < num_unique_subset_pats);
				if (results[i] == unique_pat_index)
					continue;

				const uint32_t similar_unique_pat_index = results[i];

				similar_pats[num_subsets - 2](seed_index, dst_index) = safe_cast_uint16(pat_data.m_unique_index_to_part_seed[similar_unique_pat_index]);

				const uint32_t total_perms = (num_subsets == 2) ? 2 : NUM_PART3_MAPPINGS;
				uint32_t best_dist = UINT32_MAX, best_perm_index = 0;

				const partition_pattern_vec& similar_pat_vec = pat_data.m_partition_pats[similar_unique_pat_index];

				for (uint32_t perm_index = 0; perm_index < total_perms; perm_index++)
				{
					partition_pattern_vec desired_pat_vec_permuted = (num_subsets == 2) ? desired_pat_vec.get_permuted2(perm_index) : desired_pat_vec.get_permuted3(perm_index);

					uint32_t dist = desired_pat_vec_permuted.get_squared_distance(similar_pat_vec);
					if (dist < best_dist)
					{
						best_dist = dist;
						best_perm_index = perm_index;
					}
				} // p

				similar_pat_perm_index[num_subsets - 2](seed_index, dst_index) = safe_cast_uint8(best_perm_index);

				dst_index++;
				if (dst_index >= NUM_SIMILAR_PATS)
					break;
			} // i

		} // unique_pat_index

	} // num_subsets
#endif

	for (uint32_t pass = 0; pass < num_passes; pass++)
	{
		const bool final_pass_flag = (pass == (num_passes - 1));

		if (global_cfg.m_debug_output)
			fmt_debug_printf("Pass: {}\n", pass);
						
		uint_vec* const pJob_block_list = (pass & 1) ? &black_list : &white_list;

		vector2D<int> new_best_packed_block_indices(num_blocks_x, num_blocks_y);
		new_best_packed_block_indices.set_all(-1);

		std::atomic<uint64_t> total_err;
		total_err.store(0);

		if (pJob_block_list->size())
		{
			std::atomic<uint32_t> cur_block_list_index;
			cur_block_list_index.store(0);

			std::atomic<bool> encoder_failed_flag;
			encoder_failed_flag.store(false);
												
			for (uint32_t job_index = 0; job_index < num_threads; job_index++)
			{
				job_pool.add_job([job_index, num_threads,
					num_blocks_x, num_blocks_y, block_width, block_height, total_blocks,
					pJob_block_list,
					pass, &block_best_err, &new_best_packed_block_indices, &total_err,
					&cur_block_list_index, &encoder_failed_flag,
					&candidate_img,
					&orig_img,
					&global_cfg, &enc_cfg, &enc_out]
					{
						BASISU_NOTE_UNUSED(job_index); BASISU_NOTE_UNUSED(num_threads); BASISU_NOTE_UNUSED(total_blocks);
						deblocking_thread_state thread_state;
						thread_state.m_deblock_orig.resize(block_width * 3, block_height * 3);
						thread_state.m_deblock_staging.resize(block_width * 3, block_height * 3);
						thread_state.m_deblock_temp.resize(block_width + 2, block_height + 2);

						for (; ; )
						{
							if (encoder_failed_flag)
								return;

							const uint32_t block_list_index = cur_block_list_index.fetch_add(1);
							if (block_list_index >= pJob_block_list->size_u32())
								break;

							const uint32_t block_index = (*pJob_block_list)[block_list_index];
							const uint32_t bx = block_index % num_blocks_x;
							const uint32_t by = block_index / num_blocks_x;
														
							uint32_t new_best_packed_block_index = 0;
							uint64_t best_err = 0;

							if (!deblocking_find_best_candidate(
								block_width, block_height, num_blocks_x, num_blocks_y,
								pass, bx, by,
								thread_state,
								candidate_img,
								orig_img,
								global_cfg,
								enc_cfg,
								enc_out,
								new_best_packed_block_index, best_err))
							{
								encoder_failed_flag.store(true);
								return;
							}

							total_err.fetch_add(best_err, std::memory_order_relaxed);
																												
							new_best_packed_block_indices(bx, by) = new_best_packed_block_index;

							block_best_err(bx, by) = best_err;

						} // for

					});

			} // job_index

			job_pool.wait_for_all();

			if (encoder_failed_flag)
			{
				fmt_error_printf("refine_output_for_deblocking: Threaded deblocking pass failed!\n");
				return false;
			}
		}
				
		uint32_t total_blocks_changed = 0;

		// now commit changed blocks to candidate_img
		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				const int new_best_packed_block_index = new_best_packed_block_indices(bx, by);

				if (new_best_packed_block_index < 0)
					continue;

				ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

				if ((int)blk_info.m_packed_out_block_index == new_best_packed_block_index)
					continue;

				const basisu::vector<encode_block_output>& out_blocks = blk_info.m_out_blocks;
								
				total_blocks_changed++;

				blk_info.m_packed_out_block_index = new_best_packed_block_index;

				const encode_block_output& out_blk = out_blocks[new_best_packed_block_index];

				color_rgba block_pixels[astc_helpers::MAX_BLOCK_PIXELS];

				if (!astc_helpers::decode_block_xuastc_ldr(out_blk.m_log_blk, block_pixels, block_width, block_height, dec_mode))
				{
					assert(0);
					return false;
				}

				candidate_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);

			} // bx
		} // by

#if 0
		if ((pass == 0) && (global_cfg.m_debug_block_x != -1))
		{
			save_png(global_cfg.m_debug_file_prefix + "debug_block_image.png", debug_block_image);
		}
#endif

		if (global_cfg.m_debug_output)
		{
			fmt_debug_printf("Pass total error: {}\n", total_err);
			fmt_debug_printf("Total blocks changed: {} {3.2}% (rel to all blocks)\n", total_blocks_changed, (float)total_blocks_changed * 100.0f / total_blocks);
		}

		if ((pass & 1) && (global_cfg.m_debug_output))
		{
			uint64_t img_total_err = 0;
			for (uint32_t by = 0; by < num_blocks_y; by++)
			{
				for (uint32_t bx = 0; bx < num_blocks_x; bx++)
				{
					uint64_t block_err = block_best_err(bx, by);
					assert(block_err != UINT64_MAX);
					img_total_err += block_err;
				} // bx
			} // by

			fmt_printf("Total image error: {}\n", img_total_err);
		}

		image final_candidate_img_deblocked;
		if ((global_cfg.m_debug_images) || 
			((global_cfg.m_debug_output) && (global_cfg.m_debug_output_image_metrics)))
		{
			deblock_image(block_width, block_height, candidate_img, final_candidate_img_deblocked);

			if ((final_pass_flag) && (global_cfg.m_debug_images))
			{
				save_png(global_cfg.m_debug_file_prefix + "deblock_final_candidate_img.png", candidate_img);
				save_png(global_cfg.m_debug_file_prefix + "deblock_final_candidate_img_deblocked.png", final_candidate_img_deblocked);
			}
		}

		if ((global_cfg.m_debug_output) && (global_cfg.m_debug_output_image_metrics))
		{
			fmt_debug_printf("\norig vs. final candidate image:\n");
			print_image_metrics(orig_img, candidate_img);

			if (final_candidate_img_deblocked.get_total_pixels())
			{
				fmt_debug_printf("\norig vs. final candidate image deblocked:\n");
				print_image_metrics(orig_img, final_candidate_img_deblocked);
			}
		}

		if ((mutation_enabled) && (((int)pass >= 4) && (pass < (num_passes - 2))))
		{
			if (!mutate_candidates(
				pass,
				block_width, block_height, num_blocks_x, num_blocks_y, total_blocks,
				max_new_blocks, pJob_block_list, block_std_dev,
				NUM_SIMILAR_PATS, similar_pats, similar_pat_perm_index,
				orig_img,
				candidate_img,
				num_threads,
				job_pool,
				global_cfg,
				enc_cfg,
				enc_out))
			{
				return false;
			}
		}

	} // pass

	if (global_cfg.m_debug_output)
	{
		uint32_t total_changed_blocks = 0, total_mutated_blocks = 0, total_part_id_blocks = 0, total_endpoint_lerp_blocks = 0, total_mutated_dct_ac_blocks = 0, total_mutated_dct_dc_blocks = 0, total_nonprimary_candidate_blocks = 0;

		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

				if (orig_best_block_indices(bx, by) != blk_info.m_packed_out_block_index)
					total_changed_blocks++;

				const encode_block_output& best_blk = blk_info.m_out_blocks[blk_info.m_packed_out_block_index];
				if (best_blk.m_blur_id >= BLUR_ID_EXP)
					total_mutated_blocks++;

				if (best_blk.m_log_blk.m_user_mode & 1)
					total_part_id_blocks++;
				
				if (best_blk.m_log_blk.m_user_mode & 2)
					total_endpoint_lerp_blocks++;

				if (best_blk.m_log_blk.m_user_mode & 4)
					total_mutated_dct_ac_blocks++;

				if (best_blk.m_log_blk.m_user_mode & 8)
					total_nonprimary_candidate_blocks++;

				if (best_blk.m_log_blk.m_user_mode & 16)
					total_mutated_dct_dc_blocks++;
			}
		}

		fmt_printf("------ SCD complete:\n");
		fmt_printf("Total candidate ID changed blocks: {} {3.2}%\n", total_changed_blocks, (total_changed_blocks * 100.0f) / (float)total_blocks);
		fmt_printf("Total mutated blocks: {} {3.2}%\n", total_mutated_blocks, (total_mutated_blocks * 100.0f) / (float)total_blocks);
		fmt_printf("Total mutated part id blocks: {} {3.2}%\n", total_part_id_blocks, (total_part_id_blocks * 100.0f) / (float)total_blocks);
		fmt_printf("Total mutated endpoint lerp blocks: {} {3.2}%\n", total_endpoint_lerp_blocks, (total_endpoint_lerp_blocks * 100.0f) / (float)total_blocks);
		fmt_printf("Total mutated DCT AC blocks: {} {3.2}%\n", total_mutated_dct_ac_blocks, (total_mutated_dct_ac_blocks * 100.0f) / (float)total_blocks);
		fmt_printf("Total mutated DCT DC blocks: {} {3.2}%\n", total_mutated_dct_dc_blocks, (total_mutated_dct_dc_blocks * 100.0f) / (float)total_blocks);
		fmt_printf("Total non-primary candidate blocks: {} {3.2}%\n", total_nonprimary_candidate_blocks, (total_nonprimary_candidate_blocks * 100.0f) / (float)total_blocks);

		const uint64_t final_deblocked_err = deblocking_compute_overall_error(
			block_width, block_height, num_blocks_x, num_blocks_y,
			orig_img,
			global_cfg,
			enc_cfg,
			enc_out);

		fmt_printf("Cross check: Initial error: {} {3.3} per block, final error: {} {3.3} per block, Avg. change per block: {3.3}\n",
			initial_deblocked_err, (double)initial_deblocked_err / (double)total_blocks,
			final_deblocked_err, (double)final_deblocked_err / (double)total_blocks,
			((double)final_deblocked_err - (double)initial_deblocked_err) / (double)total_blocks);

		fmt_debug_printf("------------------- refine_output_for_deblocking: OK\n");
	}
		
	return true;
}

static bool sharpen_image(const image& in, image& out, const astc_ldr_encode_config& global_cfg)
{
	const float amount = global_cfg.m_sharpen_amount;

	if ((in.get_width() <= 4) && (in.get_height() <= 4))
	{
		out = in;
		return true;
	}

	if (global_cfg.m_debug_output)
		fmt_debug_printf("sharpen_image: DoG amount {} (note this modifies the original image and impacts downstream PSNR's)\n", amount);

	image blur1, blur2;
	blur1.match_dimensions(in);
	blur2.match_dimensions(in);

	if (!image_resample(in, blur1, false, "gaussian", 1.05f))
		return false;
	if (!image_resample(in, blur2, false, "gaussian", 1.3f))
		return false;
		
	out.match_dimensions(in);

	for (uint32_t y = 0; y < in.get_height(); y++)
	{
		for (uint32_t x = 0; x < in.get_width(); x++)
		{
			const color_rgba& i = in(x, y);
			const color_rgba& b1 = blur1(x, y);
			const color_rgba& b2 = blur2(x, y);

			color_rgba o(0, 0, 0, i.a);

			for (int c = 0; c < 3; c++)
			{
				int k = (int)std::round((float)i[c] + amount * ((float)b1[c] - (float)b2[c]));
				o[c] = (uint8_t)clamp<int>(k, 0, 255);
			}

			out(x, y) = o;
		} // x 
	} // y

	return true;
}

// merges output candidates (with no de-dup) from enc_out2 to enc_out, returns # of better blocks found in enc_out2 vs. enc_out
static uint32_t merge_output_candidates(ldr_astc_block_encode_image_output& enc_out, const ldr_astc_block_encode_image_output& enc_out2)
{
	uint32_t total_better_blocks = 0;

	// Merge the two outputs now
	for (uint32_t by = 0; by < enc_out2.m_image_block_info.get_height(); by++)
	{
		for (uint32_t bx = 0; bx < enc_out2.m_image_block_info.get_width(); bx++)
		{
			ldr_astc_block_encode_image_output::block_info& blk_info1 = enc_out.m_image_block_info(bx, by);
			const ldr_astc_block_encode_image_output::block_info& blk_info2 = enc_out2.m_image_block_info(bx, by);

			const uint32_t orig_out_block_size = blk_info1.m_out_blocks.size_u32();

			blk_info1.m_out_blocks.append(blk_info2.m_out_blocks);

#if 0
			for (uint32_t i = 0; i < blk_info2.m_out_blocks.size(); i++)
			{
				blk_info1.m_out_blocks[orig_out_block_size + i].m_blur_id = 768;
			}
#endif

			const uint64_t best_err1 = blk_info1.m_out_blocks[blk_info1.m_packed_out_block_index].m_sse;
			const uint64_t best_err2 = blk_info2.m_out_blocks[blk_info2.m_packed_out_block_index].m_sse;

			if (best_err2 < best_err1)
			{
				total_better_blocks++;

				blk_info1.m_packed_out_block_index = orig_out_block_size + blk_info2.m_packed_out_block_index;

				enc_out.m_packed_phys_blocks(bx, by) = enc_out2.m_packed_phys_blocks(bx, by);
			}

		} // bx
	} // by

	return total_better_blocks;
}

static bool cross_check_dct(
	uint32_t block_width, uint32_t block_height, uint32_t num_blocks_x, uint32_t num_blocks_y,
	const ldr_astc_block_encode_image_output &enc_out,
	const astc_ldr_encode_config& global_cfg,
	const ldr_astc_block_encode_image_high_level_config& enc_cfg)
{
	if (global_cfg.m_debug_output)
		fmt_debug_printf("cross_check_dct:\n");

	basist::astc_ldr_t::grid_weight_dct grid_coder;
	grid_coder.init(block_width, block_height);

	basist::astc_ldr_t::fvec dct_temp;

	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			const ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

			const uint32_t best_packed_out_block_index = blk_info.m_packed_out_block_index;

			if (best_packed_out_block_index >= blk_info.m_out_blocks.size())
			{
				fmt_error_printf("astc_ldr::cross_check_dct(): best_packed_out_block_index is invalid\n");
				return false;
			}

			const auto& out_blk = blk_info.m_out_blocks[best_packed_out_block_index];

			const auto& log_blk = out_blk.m_log_blk;
			if (log_blk.m_solid_color_flag_ldr)
				continue;

			astc_helpers::log_astc_block temp_log_blk(log_blk);

			for (uint32_t plane_index = 0; plane_index < (log_blk.m_dual_plane ? 2u : 1u); plane_index++)
			{
				const basist::astc_ldr_t::dct_syms& syms = out_blk.m_packed_dct_plane_data[plane_index];

				bool status = grid_coder.decode_block_weights(enc_cfg.m_base_q, plane_index, temp_log_blk, nullptr, nullptr, dct_temp, &syms);
				assert(status);

				if (!status)
				{
					fmt_error_printf("astc_ldr::cross_check_dct(): failed decoding weight grids\n");
					return false;
				}

				int max_coeff = 0;

				for (uint32_t i = 0; i < syms.m_coeffs.size(); i++)
				{
					if (syms.m_coeffs[i].m_coeff != INT16_MAX)
						max_coeff = basisu::maximum<int>(max_coeff, iabs(syms.m_coeffs[i].m_coeff));
				}

				// TODO: the DCT mutator can only boost the max not reduce it, so it's not entirely accurate now but safe enough for supercompression purposes
				if (max_coeff > (int)syms.m_max_coeff_mag)
				{
					fmt_error_printf("astc_ldr::cross_check_dct(): m_max_coeff_mag is invalid\n");
					return false;
				}
			}
			
			// Ensure the decoded weight grid matches the stored weight grids, or there's a serious problem.
			const uint32_t total_grid_weights = astc_helpers::get_total_weights(temp_log_blk);
			if (memcmp(log_blk.m_weights, temp_log_blk.m_weights, total_grid_weights) != 0)
			{
				fmt_error_printf("astc_ldr::cross_check_dct(): decoded weight grid mismatch\n");
				return false;
			}

		} // bx

	} // by

	if (global_cfg.m_debug_output)
		fmt_debug_printf("Weight grid DCT decode cross-check OK\n");

	return true;
}

bool compress_image(
	const image& actual_orig_img, uint8_vec& comp_data, vector2D<astc_helpers::log_astc_block>& coded_blocks,
	const astc_ldr_encode_config& orig_global_cfg,
	job_pool& job_pool)
{
	assert(g_initialized);

	astc_ldr_encode_config global_cfg(orig_global_cfg);

	//global_cfg.m_debug_block_x = 17;
	//global_cfg.m_debug_block_y = 26;
		
#if !BASISU_SUPPORT_ASTCENC
	if (global_cfg.m_use_astcenc)
	{
		fmt_error_printf("Can't use astcenc as it hasn't been enabled at compilation time (BASISU_SUPPORT_ASTCENC is 0)\n");
		global_cfg.m_use_astcenc = false;
	}
#endif
		
	if (global_cfg.m_scd_enabled)
	{
		fmt_debug_printf("Disabling lossy supercompression: SCD is incompatible with it\n");
		global_cfg.m_lossy_supercompression = false;
	}

	if (global_cfg.m_debug_output)
	{
		fmt_debug_printf("\n------------------- astc_ldr::compress_image\n");

		fmt_debug_printf("\nglobal_cfg:\n");
		global_cfg.debug_print();
		fmt_debug_printf("\n");
	}

	comp_data.resize(0);

	if (!g_initialized)
		return false;

	image sharpened_img;
	const image *pOrig_img = &actual_orig_img;

	if ((global_cfg.m_sharpen_flag) && (global_cfg.m_sharpen_amount > 0.0f))
	{
		if (!sharpen_image(actual_orig_img, sharpened_img, global_cfg))
			return false;

		pOrig_img = &sharpened_img;
	}

	const image& orig_img = *pOrig_img;

	const uint32_t width = orig_img.get_width(), height = orig_img.get_height();

	if (!is_in_range(width, 1, (int)MAX_WIDTH) || !is_in_range(height, 1, (int)MAX_HEIGHT))
		return false;

	if (!astc_helpers::is_valid_block_size(global_cfg.m_astc_block_width, global_cfg.m_astc_block_height))
		return false;

	const uint32_t block_width = global_cfg.m_astc_block_width;
	const uint32_t block_height = global_cfg.m_astc_block_height;
	const uint32_t total_block_pixels = block_width * block_height;

	const uint32_t total_pixels = width * height;
	const uint32_t num_blocks_x = (width + block_width - 1) / block_width;
	const uint32_t num_blocks_y = (height + block_height - 1) / block_height;
	const uint32_t total_blocks = num_blocks_x * num_blocks_y;
	const bool has_alpha = orig_img.has_alpha();

	if (global_cfg.m_debug_output)
	{
		fmt_debug_printf("Alternative compressors active: astcenc: {}, astcf: {}\n", global_cfg.m_use_astcenc, global_cfg.m_use_astcf);
		fmt_debug_printf("Encoding image dimensions {}x{}, has alpha: {}\n", orig_img.get_width(), orig_img.get_height(), has_alpha);
	}

	ldr_astc_block_encode_image_high_level_config enc_cfg;

	enc_cfg.m_block_width = block_width;
	enc_cfg.m_block_height = block_height;
	enc_cfg.m_pJob_pool = &job_pool;

	enc_cfg.m_use_dct = global_cfg.m_use_dct;

	if (!is_in_range(global_cfg.m_dct_quality, 1.0f, 100.0f))
		return false;

	const int int_q = clamp<int>((int)std::round(global_cfg.m_dct_quality * 2.0f), 0, 200);
	enc_cfg.m_base_q = (float)int_q / 2.0f;

	if (global_cfg.m_debug_output)
		fmt_debug_printf("Use DCT: {}, base q: {}, lossy supercompression: {}\n", enc_cfg.m_use_dct, enc_cfg.m_base_q, global_cfg.m_lossy_supercompression);

	const float replacement_min_psnr = has_alpha ? global_cfg.m_replacement_min_psnr_alpha : global_cfg.m_replacement_min_psnr;
	const float psnr_trial_diff_thresh = has_alpha ? global_cfg.m_psnr_trial_diff_thresh_alpha : global_cfg.m_psnr_trial_diff_thresh;
	const float psnr_trial_diff_thresh_edge = has_alpha ? global_cfg.m_psnr_trial_diff_thresh_edge_alpha : global_cfg.m_psnr_trial_diff_thresh_edge;

	enc_cfg.m_blurring_enabled_p1 = global_cfg.m_block_blurring_p1;
	enc_cfg.m_blurring_enabled_p2 = global_cfg.m_block_blurring_p2;

	enc_cfg.m_try_simplified_latent_configs = global_cfg.m_try_simplified_latent_configs;

	for (uint32_t i = 0; i < 4; i++)
	{
		enc_cfg.m_cem_enc_params.m_comp_weights[i] = global_cfg.m_comp_weights[i];

		if (!is_in_range(global_cfg.m_comp_weights[i], 1, 256))
		{
			fmt_error_printf("astc_ldr::compress_image: m_comp_weights[] are out of range\n");
			return false;
		}
	}

	int cfg_effort_level = global_cfg.m_effort_level;
	if (global_cfg.m_debug_output)
		fmt_debug_printf("Using cfg effort level: {}\n", cfg_effort_level);

	configure_encoder_effort_level(cfg_effort_level, enc_cfg);

	if (global_cfg.m_force_disable_subsets)
	{
		enc_cfg.m_subsets_enabled = false;
		enc_cfg.m_second_pass_force_subsets_enabled = false;
	}

	if (global_cfg.m_force_disable_rgb_dual_plane)
	{
		enc_cfg.m_disable_rgb_dual_plane = true;
		enc_cfg.m_force_all_dp_chans_p2 = false;
	}

	enc_cfg.m_cem_enc_params.m_decode_mode_srgb = global_cfg.m_astc_decode_mode_srgb;

	enc_cfg.m_debug_output = global_cfg.m_debug_output;
	enc_cfg.m_debug_output_image_metrics = global_cfg.m_debug_output_image_metrics;
	enc_cfg.m_debug_images = global_cfg.m_debug_images;
	enc_cfg.m_debug_file_prefix = global_cfg.m_debug_file_prefix;

	//ldr_astc_block_encode_image_output enc_out;

	std::unique_ptr<ldr_astc_block_encode_image_output> pEnc_out = std::make_unique<ldr_astc_block_encode_image_output>();
	ldr_astc_block_encode_image_output& enc_out = *pEnc_out;

	bool enc_status = false;

	bool encoded_flag = false; // has the input been encoded yet

	// Candidates are expensive memory wise, so try to place sane limits on them (especially important for WASM).
	// This is a per-encoder limit (not total after merging).
	uint32_t max_candidate_limit = 16;

	if (global_cfg.m_scd_enabled)
	{
		if (total_blocks >= (512 * 512))
		{
			if (global_cfg.m_merge_basisu_into_output)
				max_candidate_limit = 8;
			else
				max_candidate_limit = 16;
		}
		else if (total_blocks >= (256 * 256))
		{
			if (global_cfg.m_merge_basisu_into_output)
				max_candidate_limit = 16;
			else
				max_candidate_limit = 32;
		}
		else
		{
			if (global_cfg.m_merge_basisu_into_output)
				max_candidate_limit = 32;
			else
				max_candidate_limit = 64;
		}
	}
				
#if BASISU_SUPPORT_ASTCENC
	if (global_cfg.m_use_astcenc)
	{
		// TODO: If this somehow fails, we could fall back to our encoder.
		enc_status = ldr_astc_block_encode_image_astcenc(orig_img, enc_cfg, global_cfg, enc_out, max_candidate_limit);
		if (!enc_status)
		{
			fmt_error_printf("ldr_astc_block_encode_image_astcenc() failed!\n");
			return false;
		}

		if (global_cfg.m_use_astcf)
		{
			std::unique_ptr<ldr_astc_block_encode_image_output> pEnc_out_astcf = std::make_unique<ldr_astc_block_encode_image_output>();
			
			enc_status = ldr_astc_block_encode_image_astcf(orig_img, enc_cfg, global_cfg, *pEnc_out_astcf, max_candidate_limit);
			if (!enc_status)
			{
				fmt_error_printf("ldr_astc_block_encode_image_astcf() failed!\n");
				return false;
			}

			if (global_cfg.m_debug_output)
				fmt_debug_printf("Merging outputs from astcenc and astcf encoders\n");

			const uint32_t total_better_blocks_astcf = merge_output_candidates(enc_out, *pEnc_out_astcf);

			if (global_cfg.m_debug_output)
				fmt_debug_printf("Total astcf blocks better than astcenc's: {} {3.2}%, out of {} total blocks\n", total_better_blocks_astcf, ((float)total_better_blocks_astcf * 100.0f) / (float)total_blocks, total_blocks);
		}

		encoded_flag = true;
	}
#endif

	if ((!encoded_flag) && (global_cfg.m_use_astcf))
	{
		enc_status = ldr_astc_block_encode_image_astcf(orig_img, enc_cfg, global_cfg, enc_out, max_candidate_limit);
		if (!enc_status)
		{
			fmt_error_printf("ldr_astc_block_encode_image_astcf() failed!\n");
			return false;
		}
				
		encoded_flag = true;
	}

	// combine the outputs now
	if ((encoded_flag) && (global_cfg.m_merge_basisu_into_output))
	{
		std::unique_ptr<ldr_astc_block_encode_image_output> pEnc_out_basisu = std::make_unique<ldr_astc_block_encode_image_output>();

		if ((global_cfg.m_effort_level == 0) && (global_cfg.m_astc_block_width == 4) && (global_cfg.m_astc_block_height == 4))
			enc_status = ldr_astc_block_encode_image_fast_4x4(orig_img, enc_cfg, global_cfg, *pEnc_out_basisu, max_candidate_limit);
		else
			enc_status = ldr_astc_block_encode_image(orig_img, enc_cfg, *pEnc_out_basisu, max_candidate_limit);

		if (global_cfg.m_debug_output)
			fmt_debug_printf("ldr_astc_block_encode_image: {}\n", enc_status);

		if (!enc_status)
		{
			fmt_error_printf("ldr_astc_block_encode_image_fast_4x4() or ldr_astc_block_encode_image() failed!\n");
			return false;
		}

		if (global_cfg.m_debug_output)
			fmt_debug_printf("Merging basisu output\n");

		const uint32_t total_better_blocks_basisu = merge_output_candidates(enc_out, *pEnc_out_basisu);

		if (global_cfg.m_debug_output)
			fmt_debug_printf("Total better basisu blocks: {} {3.2}%, out of {} total blocks\n", total_better_blocks_basisu, ((float)total_better_blocks_basisu * 100.0f) / (float)total_blocks, total_blocks);
	}
	else if (!encoded_flag)
	{
		if ((global_cfg.m_effort_level == 0) && (global_cfg.m_astc_block_width == 4) && (global_cfg.m_astc_block_height == 4))
		{
			enc_status = ldr_astc_block_encode_image_fast_4x4(orig_img, enc_cfg, global_cfg, enc_out, max_candidate_limit);
		}
		else
		{
			enc_status = ldr_astc_block_encode_image(orig_img, enc_cfg, enc_out, max_candidate_limit);
		}

		if (global_cfg.m_debug_output)
			fmt_debug_printf("ldr_astc_block_encode_image: {}\n", enc_status);

		if (!enc_status)
		{
			fmt_error_printf("ldr_astc_block_encode_image_fast_4x4() or ldr_astc_block_encode_image() failed!\n");
			return false;
		}

		encoded_flag = true;
	}

	assert(encoded_flag);

	if (global_cfg.m_debug_output)
		display_candidate_statistics(enc_out);

	if (global_cfg.m_scd_enabled)
	{
		if (!refine_output_for_deblocking(orig_img, global_cfg, enc_cfg, enc_out))
			return false;
	}

	// sanity check decoded weight grids
	if (global_cfg.m_use_dct)
	{
		if (!cross_check_dct(block_width, block_height, num_blocks_x, num_blocks_y, enc_out, global_cfg, enc_cfg))
			return false;
	}

	if (global_cfg.m_debug_output)
		display_candidate_statistics(enc_out);

	basist::astc_ldr_t::xuastc_ldr_syntax syntax = global_cfg.m_compressed_syntax;
	
	if (syntax >= basist::astc_ldr_t::xuastc_ldr_syntax::cTotal)
	{
		assert(0);
		return false;
	}

	// Switch to full adaptive arithmetic coding on the smallest mipmaps to avoid ZStd overhead.
	const uint32_t DISABLE_FASTER_FORMAT_TOTAL_BLOCKS_THRESH = 64;
	if (total_blocks <= DISABLE_FASTER_FORMAT_TOTAL_BLOCKS_THRESH)
		syntax = basist::astc_ldr_t::xuastc_ldr_syntax::cFullArith;

	if (syntax == basist::astc_ldr_t::xuastc_ldr_syntax::cFullZStd)
	{
#if BASISD_SUPPORT_KTX2_ZSTD
		// Full ZStd syntax is so different we'll move that to another function.
		return compress_image_full_zstd(
			orig_img, comp_data, coded_blocks,
			global_cfg,
			job_pool,
			enc_cfg, enc_out);
#else
		fmt_error_printf("Full ZStd syntax not supported in this build (set BASISD_SUPPORT_KTX2_ZSTD to 1)\n");
		return false;
#endif
	}

	const bool use_faster_format = (syntax == basist::astc_ldr_t::xuastc_ldr_syntax::cHybridArithZStd);

#if !BASISD_SUPPORT_KTX2_ZSTD
	if (use_faster_format)
	{
		fmt_error_printf("Full ZStd syntax not supported in this build (set BASISD_SUPPORT_KTX2_ZSTD to 1)\n");
		return false;
	}
#endif
			
	// Either full arithmetic, or hybrid arithmetic+ZStd for weight symbols.
	basist::astc_ldr_t::xuastc_ldr_arith_header hdr;
	clear_obj(hdr);

	bitwise_coder mean0_bits;
	uint8_vec mean1_bytes;
	uint8_vec run_bytes;
	uint8_vec coeff_bytes;
	bitwise_coder sign_bits;
	bitwise_coder weight2_bits;
	bitwise_coder weight3_bits;
	bitwise_coder weight4_bits;
	uint8_vec weight8_bits;
				
	if (use_faster_format)
	{
		mean0_bits.init(1024);
		mean1_bytes.reserve(1024);
		run_bytes.reserve(8192);
		coeff_bytes.reserve(8192);
		sign_bits.init(1024);
		weight2_bits.init(1024);
		weight3_bits.init(1024);
		weight4_bits.init(1024);
		weight8_bits.reserve(8192);
	}
					
	interval_timer itm;
	itm.start();

	basist::arith::arith_enc enc;
	enc.init(1024 * 1024);

	enc.put_bits(basist::astc_ldr_t::ARITH_HEADER_MARKER, basist::astc_ldr_t::ARITH_HEADER_MARKER_BITS);
				
	const int block_dim_index = astc_helpers::find_astc_block_size_index(block_width, block_height);
	assert((block_dim_index >= 0) && (block_dim_index < (int)astc_helpers::NUM_ASTC_BLOCK_SIZES));

	enc.put_bits(block_dim_index, 4);

	enc.put_bit(enc_cfg.m_cem_enc_params.m_decode_mode_srgb);

	enc.put_bits(width, 16);
	enc.put_bits(height, 16);

	enc.put_bit(has_alpha);

	enc.put_bits(enc_cfg.m_use_dct, 1);
	if (enc_cfg.m_use_dct)
		enc.put_bits(int_q, 8);

	basist::arith::arith_data_model mode_model((uint32_t)basist::astc_ldr_t::xuastc_mode::cMODE_TOTAL);

	basist::arith::arith_data_model solid_color_dpcm_model[4];
	for (uint32_t i = 0; i < 4; i++)
		solid_color_dpcm_model[i].init(256, true);

	basist::arith::arith_data_model raw_endpoint_models[astc_helpers::TOTAL_ENDPOINT_ISE_RANGES];
	for (uint32_t i = 0; i < astc_helpers::TOTAL_ENDPOINT_ISE_RANGES; i++)
		raw_endpoint_models[i].init(astc_helpers::get_ise_levels(astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE + i));

	basist::arith::arith_data_model dpcm_endpoint_models[astc_helpers::TOTAL_ENDPOINT_ISE_RANGES];
	for (uint32_t i = 0; i < astc_helpers::TOTAL_ENDPOINT_ISE_RANGES; i++)
		dpcm_endpoint_models[i].init(astc_helpers::get_ise_levels(astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE + i));

	basist::arith::arith_data_model raw_weight_models[astc_helpers::TOTAL_WEIGHT_ISE_RANGES];
	for (uint32_t i = 0; i < astc_helpers::TOTAL_WEIGHT_ISE_RANGES; i++)
		raw_weight_models[i].init(astc_helpers::get_ise_levels(astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE + i));

	basist::arith::arith_bit_model is_base_ofs_model;
	basist::arith::arith_bit_model use_dct_model[4];
	basist::arith::arith_bit_model use_dpcm_endpoints_model;

	basist::arith::arith_data_model cem_index_model[8];
	for (uint32_t i = 0; i < 8; i++)
		cem_index_model[i].init(basist::astc_ldr_t::OTM_NUM_CEMS);

	basist::arith::arith_data_model subset_index_model[basist::astc_ldr_t::OTM_NUM_SUBSETS];
	for (uint32_t i = 0; i < basist::astc_ldr_t::OTM_NUM_SUBSETS; i++)
		subset_index_model[i].init(basist::astc_ldr_t::OTM_NUM_SUBSETS);

	basist::arith::arith_data_model ccs_index_model[basist::astc_ldr_t::OTM_NUM_CCS];
	for (uint32_t i = 0; i < basist::astc_ldr_t::OTM_NUM_CCS; i++)
		ccs_index_model[i].init(basist::astc_ldr_t::OTM_NUM_CCS);

	basist::arith::arith_data_model grid_size_model[basist::astc_ldr_t::OTM_NUM_GRID_SIZES];
	for (uint32_t i = 0; i < basist::astc_ldr_t::OTM_NUM_GRID_SIZES; i++)
		grid_size_model[i].init(basist::astc_ldr_t::OTM_NUM_GRID_SIZES);

	basist::arith::arith_data_model grid_aniso_model[basist::astc_ldr_t::OTM_NUM_GRID_ANISOS];
	for (uint32_t i = 0; i < basist::astc_ldr_t::OTM_NUM_GRID_ANISOS; i++)
		grid_aniso_model[i].init(basist::astc_ldr_t::OTM_NUM_GRID_ANISOS);

	basist::arith::arith_data_model dct_run_len_model(65); // [0,63] or 64=EOB
	basist::arith::arith_data_model dct_coeff_mag(255); // [1,255] (blocks with larger mags go DPCM)

	double total_header_bits = 0.0f, total_weight_bits = 0.0f, total_endpoint_bits = 0.0f;

	uint32_t total_solid_blocks = 0, total_used_dct = 0, total_used_weight_dpcm = 0;

	basist::astc_ldr_t::grid_weight_dct grid_dct;
	grid_dct.init(block_width, block_height);

	vector2D<basist::astc_ldr_t::prev_block_state> prev_block_states(num_blocks_x, num_blocks_y);
		
	coded_blocks.resize(num_blocks_x, num_blocks_y);
	for (uint32_t y = 0; y < num_blocks_y; y++)
		for (uint32_t x = 0; x < num_blocks_x; x++)
			coded_blocks(x, y).clear();

	vector2D<astc_helpers::log_astc_block> input_blocks;
	if (global_cfg.m_debug_images)
	{
		input_blocks.resize(num_blocks_x, num_blocks_y);

		for (uint32_t y = 0; y < num_blocks_y; y++)
			for (uint32_t x = 0; x < num_blocks_x; x++)
				input_blocks(x, y).clear();
	}

	const bool endpoint_dpcm_global_enable = true;
	uint32_t total_used_endpoint_dpcm = 0, total_used_endpoint_raw = 0;

	basist::arith::arith_data_model submode_models[basist::astc_ldr_t::OTM_NUM_CEMS][basist::astc_ldr_t::OTM_NUM_SUBSETS][basist::astc_ldr_t::OTM_NUM_CCS][basist::astc_ldr_t::OTM_NUM_GRID_SIZES][basist::astc_ldr_t::OTM_NUM_GRID_ANISOS];

	basist::arith::arith_bit_model endpoints_use_bc_models[4];

	basist::arith::arith_data_model endpoint_reuse_delta_model(basist::astc_6x6_hdr::NUM_REUSE_XY_DELTAS);

	basist::arith::arith_data_model weight_mean_models[2];
	weight_mean_models[0].init(basist::astc_ldr_t::DCT_MEAN_LEVELS0);
	weight_mean_models[1].init(basist::astc_ldr_t::DCT_MEAN_LEVELS1);

	basist::arith::arith_data_model config_reuse_model[4];
	for (uint32_t i = 0; i < 4; i++)
		config_reuse_model[i].init(basist::astc_ldr_t::cMaxConfigReuseNeighbors + 1);

	uint32_t total_reuse_full_cfg_emitted = 0, total_full_cfg_emitted = 0;

	// TODO: check weights for >= 0
	const float total_comp_weights = enc_cfg.m_cem_enc_params.get_total_comp_weights();

	uint32_t total_lossy_replacements = 0;
	uint32_t total_full_reuse_commands = 0;
	uint32_t total_raw_commands = 0;

	if (global_cfg.m_debug_output)
		fmt_debug_printf("Supercompressor init time: {} secs\n", itm.get_elapsed_secs());

	uint32_t total_runs = 0, total_run_blocks = 0;
	uint32_t cur_run_len = 0;
	const bool use_run_commands = true;
	uint32_t total_nonrun_blocks = 0;

	int part2_hash[basist::astc_ldr_t::PART_HASH_SIZE];
	std::fill(part2_hash, part2_hash + basist::astc_ldr_t::PART_HASH_SIZE, -1);

	int part3_hash[basist::astc_ldr_t::PART_HASH_SIZE];
	std::fill(part3_hash, part3_hash + basist::astc_ldr_t::PART_HASH_SIZE, -1);

	basist::arith::arith_bit_model use_part_hash_model[4];
	basist::arith::arith_data_model part2_hash_index_model(basist::astc_ldr_t::PART_HASH_SIZE, true);
	basist::arith::arith_data_model part3_hash_index_model(basist::astc_ldr_t::PART_HASH_SIZE, true);

	uint32_t num_part_hash_probes = 0, num_part_hash_hits = 0;
	uint32_t total_dct_syms = 0, total_dpcm_syms = 0;

	basist::arith::arith_gamma_contexts m_run_len_contexts;

	image vis_img;
	if (global_cfg.m_debug_images)
	{
		vis_img.resize(width, height);
	}

	itm.start();

	for (uint32_t by = 0; by < num_blocks_y; by++)
	{
		const uint32_t base_y = by * block_height;

		for (uint32_t bx = 0; bx < num_blocks_x; bx++)
		{
			const uint32_t base_x = bx * block_width;

			basist::astc_ldr_t::prev_block_state& prev_state = prev_block_states(bx, by);
			const basist::astc_ldr_t::prev_block_state* pLeft_state = bx ? &prev_block_states(bx - 1, by) : nullptr;
			const basist::astc_ldr_t::prev_block_state* pUpper_state = by ? &prev_block_states(bx, by - 1) : nullptr;
			const basist::astc_ldr_t::prev_block_state* pDiag_state = (bx && by) ? &prev_block_states(bx - 1, by - 1) : nullptr;
			const basist::astc_ldr_t::prev_block_state* pPred_state = pLeft_state ? pLeft_state : pUpper_state; // left or upper, or nullptr on first block

			const ldr_astc_block_encode_image_output::block_info& blk_info = enc_out.m_image_block_info(bx, by);

			uint32_t best_packed_out_block_index = blk_info.m_packed_out_block_index;

			if (global_cfg.m_debug_images)
			{
				input_blocks(bx, by) = blk_info.m_out_blocks[best_packed_out_block_index].m_log_blk;
			}

			// check for run 
			if ((use_run_commands) && (bx || by))
			{
				const encode_block_output& blk_out = blk_info.m_out_blocks[best_packed_out_block_index];
				const astc_helpers::log_astc_block& cur_log_blk = blk_out.m_log_blk;

				const astc_helpers::log_astc_block& prev_log_blk = bx ? coded_blocks(bx - 1, by) : coded_blocks(0, by - 1);
				const basist::astc_ldr_t::prev_block_state* pPrev_block_state = bx ? pLeft_state : pUpper_state;

				assert(pPrev_block_state);

				if (compare_log_blocks_for_equality(cur_log_blk, prev_log_blk))
				{
					// Left or upper is exactly the same logical block, so expand the run.
					cur_run_len++;

					// Accept the previous block (left or upper) as if it's been coded normally.

					coded_blocks(bx, by) = prev_log_blk;

					prev_state.m_was_solid_color = pPrev_block_state->m_was_solid_color;
					prev_state.m_used_weight_dct = pPrev_block_state->m_used_weight_dct;
					prev_state.m_first_endpoint_uses_bc = pPrev_block_state->m_first_endpoint_uses_bc;
					prev_state.m_reused_full_cfg = true;
					prev_state.m_used_part_hash = pPrev_block_state->m_used_part_hash;
					prev_state.m_tm_index = pPrev_block_state->m_tm_index;
					prev_state.m_base_cem_index = pPrev_block_state->m_base_cem_index;
					prev_state.m_subset_index = pPrev_block_state->m_subset_index;
					prev_state.m_ccs_index = pPrev_block_state->m_ccs_index;
					prev_state.m_grid_size = pPrev_block_state->m_grid_size;
					prev_state.m_grid_aniso = pPrev_block_state->m_grid_aniso;

					continue;
				}
			}

			if (cur_run_len)
			{
				total_runs++;
				total_run_blocks += cur_run_len;

				total_header_bits += enc.encode_and_return_price((uint32_t)basist::astc_ldr_t::xuastc_mode::cMODE_RUN, mode_model);
				total_header_bits += enc.put_gamma_and_return_price(cur_run_len, m_run_len_contexts);
				cur_run_len = 0;
			}

			total_nonrun_blocks++;

			const float ref_wmse = (float)blk_info.m_out_blocks[best_packed_out_block_index].m_sse / (total_comp_weights * (float)total_block_pixels);
			const float ref_wpsnr = (ref_wmse > 1e-5f) ? 20.0f * log10f(255.0f / sqrtf(ref_wmse)) : 10000.0f;

			if ((global_cfg.m_lossy_supercompression) && (ref_wpsnr >= replacement_min_psnr) &&
				(!blk_info.m_out_blocks[blk_info.m_packed_out_block_index].m_log_blk.m_solid_color_flag_ldr))
			{
				// TODO: recompute m_strong_edges? Not all encoders set it.
				const float psnr_thresh = blk_info.m_strong_edges ? psnr_trial_diff_thresh_edge : psnr_trial_diff_thresh;

				float best_alt_wpsnr = 0.0f;
				bool found_alternative = false;

				// Pass: 0 consider full config+part ID endpoint reuse
				// Pass: 1 fall back to just full config+part ID reuse (no endpoints)
				for (uint32_t pass = 0; pass < 2; pass++)
				{
					// Iterate through all available alternative candidates
					for (uint32_t out_block_iter = 0; out_block_iter < blk_info.m_out_blocks.size(); out_block_iter++)
					{
						if (out_block_iter == blk_info.m_packed_out_block_index)
							continue;

						const float trial_wmse = (float)blk_info.m_out_blocks[out_block_iter].m_sse / (total_comp_weights * (float)total_block_pixels);
						const float trial_wpsnr = (trial_wmse > 1e-5f) ? 20.0f * log10f(255.0f / sqrtf(trial_wmse)) : 10000.0f;

						// Reject if PSNR too low
						if (trial_wpsnr < (ref_wpsnr - psnr_thresh))
							continue;

						// Reject if inferior than best found so far
						if (trial_wpsnr < best_alt_wpsnr)
							continue;

						const astc_helpers::log_astc_block& trial_log_blk = blk_info.m_out_blocks[out_block_iter].m_log_blk;

						if (trial_log_blk.m_solid_color_flag_ldr)
							continue;

						// Examine nearby neighbors
						for (uint32_t i = 0; i < basist::astc_ldr_t::cMaxConfigReuseNeighbors; i++)
						{
							int dx = 0, dy = 0;
							switch (i)
							{
							case 0: dx = -1; break;
							case 1: dy = -1; break;
							case 2: dx = -1; dy = -1; break;
							default: assert(0); break;
							}

							const int n_bx = bx + dx, n_by = by + dy;
							if ((n_bx < 0) || (n_by < 0))
								continue;

							astc_helpers::log_astc_block& neighbor_log_blk = coded_blocks(n_bx, n_by);

							if (neighbor_log_blk.m_solid_color_flag_ldr)
								continue;

							bool accept_flag = false;
							if (pass == 0)
							{
								// prefer full config+endpoint equality first
								accept_flag = compare_log_block_configs_and_endpoints(trial_log_blk, neighbor_log_blk);
							}
							else
							{
								// next check for just config equality
								accept_flag = compare_log_block_configs(trial_log_blk, neighbor_log_blk);
							}

							if (accept_flag)
							{
								best_alt_wpsnr = trial_wpsnr;
								best_packed_out_block_index = out_block_iter;
								found_alternative = true;
								break;
							}

						} // i

					} // out_block_iter

					if (found_alternative)
						break;

				} // pass

				if (best_packed_out_block_index != blk_info.m_packed_out_block_index)
					total_lossy_replacements++;

			} // global_cfg.m_lossy_supercompression

			const encode_block_output& blk_out = blk_info.m_out_blocks[best_packed_out_block_index];

			astc_helpers::log_astc_block& cur_log_blk = coded_blocks(bx, by);

			cur_log_blk = blk_out.m_log_blk;

			// TODO: Add mode model context

			if (blk_out.m_trial_mode_index < 0)
			{
				assert(cur_log_blk.m_solid_color_flag_ldr);

				total_solid_blocks++;

				//total_header_bits += mode_model.get_price(cMODE_SOLID) + (float)(8 * (has_alpha ? 4 : 3));
				total_header_bits += mode_model.get_price((uint32_t)basist::astc_ldr_t::xuastc_mode::cMODE_SOLID);
				enc.encode((uint32_t)basist::astc_ldr_t::xuastc_mode::cMODE_SOLID, mode_model);

				uint32_t cur_solid_color[4];
				for (uint32_t i = 0; i < 4; i++)
					cur_solid_color[i] = blk_out.m_log_blk.m_solid_color[i] >> 8;

				uint32_t prev_solid_color[4] = { 0 };

				const uint32_t num_comps = has_alpha ? 4 : 3;

				astc_helpers::log_astc_block* pPrev_log_blk = bx ? &coded_blocks(bx - 1, by) : (by ? &coded_blocks(bx, by - 1) : nullptr);
				if (pPrev_log_blk)
				{
					if (pPrev_log_blk->m_solid_color_flag_ldr)
					{
						prev_solid_color[0] = pPrev_log_blk->m_solid_color[0] >> 8;
						prev_solid_color[1] = pPrev_log_blk->m_solid_color[1] >> 8;
						prev_solid_color[2] = pPrev_log_blk->m_solid_color[2] >> 8;
						prev_solid_color[3] = pPrev_log_blk->m_solid_color[3] >> 8;
					}
					else
					{
#if 0
						color_rgba prev_block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
						bool dec_status = astc_helpers::decode_block(*pPrev_log_blk, prev_block_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
						if (!dec_status)
						{
							fmt_error_printf("decode_block() failed\n");
							return false;
						}

						for (uint32_t i = 0; i < total_block_pixels; i++)
						{
							for (uint32_t j = 0; j < num_comps; j++)
								prev_solid_color[j] += prev_block_pixels[i][j];
						}

						for (uint32_t j = 0; j < num_comps; j++)
							prev_solid_color[j] = (prev_solid_color[j] + (total_block_pixels / 2)) / total_block_pixels;
#endif
						// Decode previous block's first CEM, use the halfway point as the predictor.
						color_rgba prev_l, prev_h;
						decode_endpoints(pPrev_log_blk->m_color_endpoint_modes[0], pPrev_log_blk->m_endpoints, pPrev_log_blk->m_endpoint_ise_range, prev_l, prev_h);

						prev_solid_color[0] = (prev_l[0] + prev_h[0] + 1) >> 1;
						prev_solid_color[1] = (prev_l[1] + prev_h[1] + 1) >> 1;
						prev_solid_color[2] = (prev_l[2] + prev_h[2] + 1) >> 1;
						prev_solid_color[3] = (prev_l[3] + prev_h[3] + 1) >> 1;
					}
				}

				for (uint32_t i = 0; i < num_comps; i++)
				{
					const uint32_t delta = (cur_solid_color[i] - prev_solid_color[i]) & 0xFF;

					total_header_bits += enc.encode_and_return_price(delta, solid_color_dpcm_model[i]);
				}

				// Bias the statistics towards using DCT (most common case).
				prev_state.m_was_solid_color = true;
				prev_state.m_used_weight_dct = enc_cfg.m_use_dct;
				prev_state.m_first_endpoint_uses_bc = true;
				prev_state.m_tm_index = -1;
				prev_state.m_base_cem_index = astc_helpers::CEM_LDR_RGB_DIRECT;
				prev_state.m_subset_index = 0;
				prev_state.m_ccs_index = 0;
				prev_state.m_grid_size = 0;
				prev_state.m_grid_aniso = 0;
				prev_state.m_reused_full_cfg = false;
				prev_state.m_used_part_hash = true; // bias to true

				continue;
			}

			//--------------------------------------------
			// for (uint32_t out_block_iter = 0; out_block_iter < blk_info.m_out_blocks.size(); out_block_iter++)
			int full_cfg_endpoint_reuse_index = -1;

			for (uint32_t i = 0; i < basist::astc_ldr_t::cMaxConfigReuseNeighbors; i++)
			{
				int dx = 0, dy = 0;
				switch (i)
				{
				case 0: dx = -1; break;
				case 1: dy = -1; break;
				case 2: dx = -1; dy = -1; break;
				default: assert(0); break;
				}

				const int n_bx = bx + dx, n_by = by + dy;
				if ((n_bx < 0) || (n_by < 0))
					continue;

				astc_helpers::log_astc_block& neighbor_log_blk = coded_blocks(n_bx, n_by);

				if (neighbor_log_blk.m_solid_color_flag_ldr)
					continue;

				if (compare_log_block_configs_and_endpoints(cur_log_blk, neighbor_log_blk))
				{
					full_cfg_endpoint_reuse_index = i;
					break;
				}
			} // i
			//--------------------------------------------

			if (full_cfg_endpoint_reuse_index >= 0)
			{
				// Reused full config, part ID and endpoint values from an immediate neighbor
				total_header_bits += enc.encode_and_return_price((uint32_t)basist::astc_ldr_t::xuastc_mode::cMODE_REUSE_CFG_ENDPOINTS_LEFT + full_cfg_endpoint_reuse_index, mode_model);

				total_full_reuse_commands++;

				const basist::astc_ldr_t::prev_block_state* pReused_cfg_state = nullptr;

				switch (full_cfg_endpoint_reuse_index)
				{
				case 0: pReused_cfg_state = pLeft_state; break;
				case 1: pReused_cfg_state = pUpper_state; break;
				case 2: pReused_cfg_state = pDiag_state; break;
				default: assert(0); break;
				}

				if (!pReused_cfg_state)
				{
					assert(0);
					fmt_error_printf("encoding internal failure\n");
					return false;
				}

				assert(pReused_cfg_state->m_tm_index == blk_out.m_trial_mode_index);

				prev_state.m_tm_index = blk_out.m_trial_mode_index;
				prev_state.m_base_cem_index = pReused_cfg_state->m_base_cem_index;
				prev_state.m_subset_index = pReused_cfg_state->m_subset_index;
				prev_state.m_ccs_index = pReused_cfg_state->m_ccs_index;
				prev_state.m_grid_size = pReused_cfg_state->m_grid_size;
				prev_state.m_grid_aniso = pReused_cfg_state->m_grid_aniso;
				prev_state.m_used_part_hash = pReused_cfg_state->m_used_part_hash;
				prev_state.m_reused_full_cfg = true;

				const uint32_t cur_actual_cem = cur_log_blk.m_color_endpoint_modes[0];

				if (astc_helpers::cem_supports_bc(cur_actual_cem))
				{
					prev_state.m_first_endpoint_uses_bc = astc_helpers::used_blue_contraction(cur_actual_cem, cur_log_blk.m_endpoints, cur_log_blk.m_endpoint_ise_range);
					assert(prev_state.m_first_endpoint_uses_bc == pReused_cfg_state->m_first_endpoint_uses_bc);
				}
			}
			else
			{
				total_raw_commands++;

				// Send mode
				total_header_bits += mode_model.get_price((uint32_t)basist::astc_ldr_t::xuastc_mode::cMODE_RAW);
				enc.encode((uint32_t)basist::astc_ldr_t::xuastc_mode::cMODE_RAW, mode_model);

				const uint32_t cur_actual_cem = cur_log_blk.m_color_endpoint_modes[0];
				//const bool actual_cem_supports_bc = astc_helpers::cem_supports_bc(cur_actual_cem);
				const uint32_t total_endpoint_vals = astc_helpers::get_num_cem_values(cur_actual_cem);

				// DO NOT use tm.m_cem because the encoder may have selected a base+ofs variant instead. Use cur_actual_cem.
				const basist::astc_ldr_t::trial_mode& tm = enc_out.m_encoder_trial_modes[blk_out.m_trial_mode_index];

				// Check for config+part ID neighbor reuse
				int neighbor_cfg_match_index = -1;
				for (uint32_t i = 0; i < basist::astc_ldr_t::cMaxConfigReuseNeighbors; i++)
				{
					const basist::astc_ldr_t::prev_block_state* pNeighbor_state = nullptr;

					int dx = 0, dy = 0;
					switch (i)
					{
					case 0: dx = -1; pNeighbor_state = pLeft_state; break;
					case 1: dy = -1; pNeighbor_state = pUpper_state; break;
					case 2: dx = -1; dy = -1; pNeighbor_state = pDiag_state; break;
					default: assert(0); break;
					}

					if (!pNeighbor_state)
						continue;

					const int n_bx = bx + dx, n_by = by + dy;
					assert((n_bx >= 0) && (n_by >= 0));

					astc_helpers::log_astc_block& neighbor_log_blk = coded_blocks(n_bx, n_by);

					if (pNeighbor_state->m_tm_index != blk_out.m_trial_mode_index)
						continue;

					if (neighbor_log_blk.m_color_endpoint_modes[0] != cur_log_blk.m_color_endpoint_modes[0])
						continue;

					if (neighbor_log_blk.m_partition_id != cur_log_blk.m_partition_id)
						continue;

					assert(neighbor_log_blk.m_dual_plane == cur_log_blk.m_dual_plane);
					assert(neighbor_log_blk.m_color_component_selector == cur_log_blk.m_color_component_selector);
					assert(neighbor_log_blk.m_num_partitions == cur_log_blk.m_num_partitions);
					assert(neighbor_log_blk.m_grid_width == cur_log_blk.m_grid_width);
					assert(neighbor_log_blk.m_grid_height == cur_log_blk.m_grid_height);
					assert(neighbor_log_blk.m_endpoint_ise_range == cur_log_blk.m_endpoint_ise_range);
					assert(neighbor_log_blk.m_weight_ise_range == cur_log_blk.m_weight_ise_range);

					neighbor_cfg_match_index = i;
					break;
				}

				uint32_t reuse_full_cfg_model_index = 0;
				if (pLeft_state)
					reuse_full_cfg_model_index = pLeft_state->m_reused_full_cfg;
				else
					reuse_full_cfg_model_index = 1;

				if (pUpper_state)
					reuse_full_cfg_model_index |= pUpper_state->m_reused_full_cfg ? 2 : 0;
				else
					reuse_full_cfg_model_index |= 2;

				if (neighbor_cfg_match_index >= 0)
				{
					total_header_bits += enc.encode_and_return_price(neighbor_cfg_match_index, config_reuse_model[reuse_full_cfg_model_index]);

					const basist::astc_ldr_t::prev_block_state* pReused_cfg_state = nullptr;

					switch (neighbor_cfg_match_index)
					{
					case 0: pReused_cfg_state = pLeft_state; break;
					case 1: pReused_cfg_state = pUpper_state; break;
					case 2: pReused_cfg_state = pDiag_state; break;
					default: assert(0); break;
					}

					if (!pReused_cfg_state)
					{
						assert(0);
						fmt_error_printf("encoding internal failure\n");
						return false;
					}

					assert(pReused_cfg_state->m_tm_index == blk_out.m_trial_mode_index);

					prev_state.m_tm_index = blk_out.m_trial_mode_index;
					prev_state.m_base_cem_index = pReused_cfg_state->m_base_cem_index;
					prev_state.m_subset_index = pReused_cfg_state->m_subset_index;
					prev_state.m_ccs_index = pReused_cfg_state->m_ccs_index;
					prev_state.m_grid_size = pReused_cfg_state->m_grid_size;
					prev_state.m_grid_aniso = pReused_cfg_state->m_grid_aniso;
					prev_state.m_used_part_hash = pReused_cfg_state->m_used_part_hash;
					prev_state.m_reused_full_cfg = true;

					total_reuse_full_cfg_emitted++;
				}
				else
				{
					total_full_cfg_emitted++;

					total_header_bits += enc.encode_and_return_price(basist::astc_ldr_t::cMaxConfigReuseNeighbors, config_reuse_model[reuse_full_cfg_model_index]);

					// ------------------------------------------- Set TM index
					{
						uint32_t cem_index, subset_index, ccs_index, grid_size, grid_aniso;

						const uint_vec& submodes = separate_tm_index(block_width, block_height, enc_out.m_grouped_encoder_trial_modes, tm,
							cem_index, subset_index, ccs_index, grid_size, grid_aniso);

						// TODO: sort this
						uint32_t submode_index;
						for (submode_index = 0; submode_index < submodes.size(); submode_index++)
							if (submodes[submode_index] == (uint32_t)blk_out.m_trial_mode_index)
								break;

						if (submode_index == submodes.size_u32())
						{
							assert(0);
							fmt_error_printf("Failed finding mode\n");
							return false;
						}

						uint32_t prev_cem_index = astc_helpers::CEM_LDR_RGB_DIRECT;
						uint32_t prev_subset_index = 0;
						uint32_t prev_ccs_index = 0;
						uint32_t prev_grid_size = 0;
						uint32_t prev_grid_aniso = 0;

						if (pPred_state)
						{
							prev_cem_index = pPred_state->m_base_cem_index;
							prev_subset_index = pPred_state->m_subset_index;
							prev_ccs_index = pPred_state->m_ccs_index;
							prev_grid_size = pPred_state->m_grid_size;
							prev_grid_aniso = pPred_state->m_grid_aniso;
						}

						const uint32_t ldrcem_index = basist::astc_ldr_t::cem_to_ldrcem_index(prev_cem_index);

						total_header_bits += cem_index_model[ldrcem_index].get_price(cem_index);
						enc.encode(cem_index, cem_index_model[ldrcem_index]);

						total_header_bits += subset_index_model[prev_subset_index].get_price(subset_index);
						enc.encode(subset_index, subset_index_model[prev_subset_index]);

						total_header_bits += ccs_index_model[prev_ccs_index].get_price(ccs_index);
						enc.encode(ccs_index, ccs_index_model[prev_ccs_index]);

						total_header_bits += grid_size_model[prev_grid_size].get_price(grid_size);
						enc.encode(grid_size, grid_size_model[prev_grid_size]);

						total_header_bits += grid_aniso_model[prev_grid_aniso].get_price(grid_aniso);
						enc.encode(grid_aniso, grid_aniso_model[prev_grid_aniso]);

						if (submodes.size() > 1)
						{
							basist::arith::arith_data_model& submode_model = submode_models[cem_index][subset_index][ccs_index][grid_size][grid_aniso];
							if (!submode_model.get_num_data_syms())
								submode_model.init(submodes.size_u32(), true);

							total_header_bits += submode_model.get_price(submode_index);
							enc.encode(submode_index, submode_model);
						}

						prev_state.m_tm_index = blk_out.m_trial_mode_index;
						prev_state.m_base_cem_index = cem_index;
						prev_state.m_subset_index = subset_index;
						prev_state.m_ccs_index = ccs_index;
						prev_state.m_grid_size = grid_size;
						prev_state.m_grid_aniso = grid_aniso;
						prev_state.m_reused_full_cfg = false;
					}

					// Send base_ofs bit if the tm is direct
					if ((tm.m_cem == astc_helpers::CEM_LDR_RGB_DIRECT) || (tm.m_cem == astc_helpers::CEM_LDR_RGBA_DIRECT))
					{
						const bool is_base_ofs = (cur_log_blk.m_color_endpoint_modes[0] == astc_helpers::CEM_LDR_RGB_BASE_PLUS_OFFSET) ||
							(cur_log_blk.m_color_endpoint_modes[0] == astc_helpers::CEM_LDR_RGBA_BASE_PLUS_OFFSET);

						total_header_bits += is_base_ofs_model.get_price(is_base_ofs);
						enc.encode(is_base_ofs, is_base_ofs_model);
					}

					if (tm.m_num_parts > 1)
					{
						// Send unique part pattern ID
						astc_ldr::partitions_data* pPart_data = (tm.m_num_parts == 2) ? &enc_out.m_part_data_p2 : &enc_out.m_part_data_p3;

						const uint32_t astc_pat_index = cur_log_blk.m_partition_id;
						const uint32_t unique_pat_index = pPart_data->m_part_seed_to_unique_index[astc_pat_index];
						const uint32_t total_unique_indices = pPart_data->m_total_unique_patterns;
						assert(unique_pat_index < total_unique_indices);

						num_part_hash_probes++;

						uint32_t use_part_model_index = 0;
						if (pLeft_state)
							use_part_model_index = pLeft_state->m_used_part_hash;
						else
							use_part_model_index = 1;
						if (pUpper_state)
							use_part_model_index |= pUpper_state->m_used_part_hash ? 2 : 0;
						else
							use_part_model_index |= 2;

						int* pPart_hash = (tm.m_num_parts == 2) ? part2_hash : part3_hash;

						const uint32_t h = basist::astc_ldr_t::part_hash_index(unique_pat_index);
						
						if (pPart_hash[h] != (int)unique_pat_index)
						{
#if defined(_DEBUG) || defined(DEBUG)
							// sanity
							for (uint32_t i = 0; i < basist::astc_ldr_t::PART_HASH_SIZE; i++)
							{
								assert(pPart_hash[i] != (int)unique_pat_index);
							}
#endif

							total_header_bits += enc.encode_and_return_price(0, use_part_hash_model[use_part_model_index]);
							total_header_bits += enc.put_truncated_binary(unique_pat_index, total_unique_indices);

							if (global_cfg.m_debug_images)
							{
								vis_img.fill_box(base_x, base_y, block_width, block_height, color_rgba(0, 0, 255, 255));
							}

							prev_state.m_used_part_hash = false;
						}
						else
						{
							num_part_hash_hits++;

							if (global_cfg.m_debug_images)
							{
								vis_img.fill_box(base_x, base_y, block_width, block_height, color_rgba(255, 0, 0, 255));
							}

							total_header_bits += enc.encode_and_return_price(1, use_part_hash_model[use_part_model_index]);
							total_header_bits += enc.encode_and_return_price(h, (tm.m_num_parts == 2) ? part2_hash_index_model : part3_hash_index_model);

							prev_state.m_used_part_hash = true;
						}

						pPart_hash[basist::astc_ldr_t::part_hash_index(unique_pat_index)] = unique_pat_index;
					}
					else
					{
						prev_state.m_used_part_hash = true; // bias to true
					}

				} // if (neighbor_cfg_match_index >= 0)

				// ----------------------------------------- Send endpoints
				const int num_endpoint_levels = astc_helpers::get_ise_levels(cur_log_blk.m_endpoint_ise_range);
				const auto& endpoint_ise_to_rank = astc_helpers::g_dequant_tables.get_endpoint_tab(cur_log_blk.m_endpoint_ise_range).m_ISE_to_rank;

				uint32_t bc_model_index = 0;
				if (pLeft_state)
					bc_model_index = pLeft_state->m_first_endpoint_uses_bc;
				else
					bc_model_index = 1;

				if (pUpper_state)
					bc_model_index |= pUpper_state->m_first_endpoint_uses_bc ? 2 : 0;
				else
					bc_model_index |= 2;

				bool endpoints_use_bc[astc_helpers::MAX_PARTITIONS] = { false };

				if (astc_helpers::cem_supports_bc(cur_actual_cem))
				{
					for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
					{
						const bool cur_uses_bc = astc_helpers::used_blue_contraction(cur_actual_cem, cur_log_blk.m_endpoints + part_iter * total_endpoint_vals, cur_log_blk.m_endpoint_ise_range);

						endpoints_use_bc[part_iter] = cur_uses_bc;

					} // part_iter

					prev_state.m_first_endpoint_uses_bc = endpoints_use_bc[0];
				}

				int best_reuse_bx = -1, best_reuse_by = -1;
				uint32_t best_reuse_index = 0;
				const astc_helpers::log_astc_block* pEndpoint_pred_log_blk = nullptr;

				if (endpoint_dpcm_global_enable)
				{
					int64_t best_trial_delta2 = INT64_MAX;
					float best_trial_bits = BIG_FLOAT_VAL;

					//auto& trial_dpcm_model = dpcm_endpoint_models[cur_log_blk.m_endpoint_ise_range - astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE];

					for (uint32_t reuse_index = 0; reuse_index < basist::astc_6x6_hdr::NUM_REUSE_XY_DELTAS; reuse_index++)
					{
						const int rx = (int)bx + basist::astc_6x6_hdr::g_reuse_xy_deltas[reuse_index].m_x;
						const int ry = (int)by + basist::astc_6x6_hdr::g_reuse_xy_deltas[reuse_index].m_y;
						if ((rx < 0) || (ry < 0) || (rx >= (int)num_blocks_x) || (ry >= (int)num_blocks_y))
							continue;

						const astc_helpers::log_astc_block* pTrial_log_blk = &coded_blocks(rx, ry);
						if (pTrial_log_blk->m_solid_color_flag_ldr)
							continue;
												
						uint8_t trial_predicted_endpoints[astc_helpers::MAX_PARTITIONS][astc_helpers::MAX_CEM_ENDPOINT_VALS] = { };

						uint32_t part_iter;
						for (part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
						{
							const bool always_repack_flag = false;
							bool blue_contraction_clamped_flag = false, try_direct_encoding_flag = false;

							bool conv_status = basist::astc_ldr_t::convert_endpoints_across_cems(
								pTrial_log_blk->m_color_endpoint_modes[0], pTrial_log_blk->m_endpoint_ise_range, pTrial_log_blk->m_endpoints,
								cur_actual_cem, cur_log_blk.m_endpoint_ise_range, trial_predicted_endpoints[part_iter],
								always_repack_flag,
								endpoints_use_bc[part_iter], false,
								blue_contraction_clamped_flag, try_direct_encoding_flag);

							if (!conv_status)
								break;
						} // part_iter

						if (part_iter < tm.m_num_parts)
							continue; // failed

						int64_t trial_endpoint_delta2 = 0;
						for (part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
						{
							for (uint32_t val_iter = 0; val_iter < total_endpoint_vals; val_iter++)
							{
								int cur_e_rank = endpoint_ise_to_rank[cur_log_blk.m_endpoints[part_iter * total_endpoint_vals + val_iter]];
								int prev_e_rank = endpoint_ise_to_rank[trial_predicted_endpoints[part_iter][val_iter]];

								int e_delta = cur_e_rank - prev_e_rank;

								trial_endpoint_delta2 += e_delta * e_delta;

							} // val_iter
														
						} // part_iter

						const float N = (float)(total_endpoint_vals * tm.m_num_parts);
						const float mse = (float)trial_endpoint_delta2 / N;

						// Gaussian entropy estimate - precomputed 0.5 * log2(2*pi*e) = ~2.0470956f
						const float k_const = 2.0470956f;

						float bits_per_sym = 0.5f * log2f(basisu::maximum(mse, 1e-9f)) + k_const;

						bits_per_sym = clamp(bits_per_sym, 0.05f, 8.0f);

						// total est bits for this block’s endpoints
						float total_est_bits = bits_per_sym * N;

						total_est_bits += endpoint_reuse_delta_model.get_price(reuse_index);

						if (total_est_bits < best_trial_bits)
						{
							best_trial_delta2 = trial_endpoint_delta2;
							best_trial_bits = total_est_bits;

							best_reuse_bx = rx;
							best_reuse_by = ry;
							best_reuse_index = reuse_index;

							if (!best_trial_delta2)
								break;
						}

					} // reuse_index

					if (best_reuse_bx >= 0)
					{
						pEndpoint_pred_log_blk = &coded_blocks(best_reuse_bx, best_reuse_by);

						assert(!pEndpoint_pred_log_blk->m_solid_color_flag_ldr);
					}

				} // if (endpoint_dpcm_global_enable)
								
				uint8_t predicted_endpoints[astc_helpers::MAX_PARTITIONS][astc_helpers::MAX_CEM_ENDPOINT_VALS] = { };

				bool use_dpcm_endpoints = false;

				if (pEndpoint_pred_log_blk)
				{
					use_dpcm_endpoints = true;

					assert(cur_log_blk.m_num_partitions == tm.m_num_parts);

					for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
					{
						const bool always_repack_flag = false;
						bool blue_contraction_clamped_flag = false, try_direct_encoding_flag = false;

						bool conv_status = basist::astc_ldr_t::convert_endpoints_across_cems(
							pEndpoint_pred_log_blk->m_color_endpoint_modes[0], pEndpoint_pred_log_blk->m_endpoint_ise_range, pEndpoint_pred_log_blk->m_endpoints,
							cur_actual_cem, cur_log_blk.m_endpoint_ise_range, predicted_endpoints[part_iter],
							always_repack_flag,
							endpoints_use_bc[part_iter], false,
							blue_contraction_clamped_flag, try_direct_encoding_flag);

						if (!conv_status)
						{
							// In practice, should never happen
							use_dpcm_endpoints = false;
							break;
						}
					}
				}

				// TODO: Decide what is cheaper, endpoint DPCM vs. raw

				if (use_dpcm_endpoints)
				{
					total_endpoint_bits += enc.encode_and_return_price(1, use_dpcm_endpoints_model);

					total_endpoint_bits += enc.encode_and_return_price(best_reuse_index, endpoint_reuse_delta_model);

					if (astc_helpers::cem_supports_bc(cur_actual_cem))
					{
						for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
						{
							total_endpoint_bits += enc.encode_and_return_price(endpoints_use_bc[part_iter], endpoints_use_bc_models[bc_model_index]);

						} // part_iter
					}

					// TODO: Perhaps separate DPCM models by CEM, entry index
					auto& dpcm_model = dpcm_endpoint_models[cur_log_blk.m_endpoint_ise_range - astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE];

					for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
					{
						for (uint32_t val_iter = 0; val_iter < total_endpoint_vals; val_iter++)
						{
							int cur_e_rank = endpoint_ise_to_rank[cur_log_blk.m_endpoints[part_iter * total_endpoint_vals + val_iter]];
							int prev_e_rank = endpoint_ise_to_rank[predicted_endpoints[part_iter][val_iter]];

							int e_val = imod(cur_e_rank - prev_e_rank, num_endpoint_levels);

							total_endpoint_bits += dpcm_model.get_price(e_val);
							enc.encode(e_val, dpcm_model);

						} // val_iter

					} // part_iter

					total_used_endpoint_dpcm++;
				}
				else
				{
					total_endpoint_bits += enc.encode_and_return_price(0, use_dpcm_endpoints_model);

					for (uint32_t part_iter = 0; part_iter < tm.m_num_parts; part_iter++)
					{
						for (uint32_t val_iter = 0; val_iter < total_endpoint_vals; val_iter++)
						{
							auto& model = raw_endpoint_models[cur_log_blk.m_endpoint_ise_range - astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE];
							uint32_t e_val = cur_log_blk.m_endpoints[part_iter * total_endpoint_vals + val_iter];

							total_endpoint_bits += model.get_price(e_val);
							enc.encode(e_val, model);

						} // val_iter

					} // part_iter

					total_used_endpoint_raw++;
				}

			} // if (full_cfg_endpoint_reuse_index >= 0)

			// ------------------------------------ Send weights
			const uint32_t total_planes = cur_log_blk.m_dual_plane ? 2 : 1;
			const uint32_t total_weights = cur_log_blk.m_grid_width * cur_log_blk.m_grid_height;

			const int num_weight_levels = astc_helpers::get_ise_levels(cur_log_blk.m_weight_ise_range);
			const auto& weight_ise_to_rank = astc_helpers::g_dequant_tables.get_weight_tab(cur_log_blk.m_weight_ise_range).m_ISE_to_rank;

			uint32_t use_dct_model_index = 0;

			if (enc_cfg.m_use_dct)
			{
				if (pLeft_state)
					use_dct_model_index = pLeft_state->m_used_weight_dct;
				else
					use_dct_model_index = 1;

				if (pUpper_state)
					use_dct_model_index |= pUpper_state->m_used_weight_dct ? 2 : 0;
				else
					use_dct_model_index |= 2;
			}

			if (use_faster_format)
			{
				bool use_dct = enc_cfg.m_use_dct;
				
				// TODO - tune this threshold
				//const uint32_t SWITCH_TO_DPCM_NUM_COEFF_THRESH = (cur_log_blk.m_grid_width * cur_log_blk.m_grid_height * 102 + 64) >> 7;
				const uint32_t SWITCH_TO_DPCM_NUM_COEFF_THRESH = (cur_log_blk.m_grid_width * cur_log_blk.m_grid_height * 45 + 64) >> 7;

				if (use_dct)
				{
					for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
					{
						const basist::astc_ldr_t::dct_syms& syms = blk_out.m_packed_dct_plane_data[plane_iter];
						
						if (!syms.m_coeffs.size())
						{
							fmt_error_printf("compress_image: internal error - no DCT coeffs\n");
							return false;
						}

						if (syms.m_max_coeff_mag > basist::astc_ldr_t::DCT_MAX_ARITH_COEFF_MAG)
						{
							use_dct = false;
							break;
						}

						if (syms.m_coeffs.size() > SWITCH_TO_DPCM_NUM_COEFF_THRESH)
						{
							use_dct = false;
							break;
						}
					}
				}

				if (enc_cfg.m_use_dct)
				{
					total_weight_bits += use_dct_model[use_dct_model_index].get_price(use_dct);
					enc.encode(use_dct, use_dct_model[use_dct_model_index]);
				}

				if (use_dct)
				{
					prev_state.m_used_weight_dct = true;

					total_used_dct++;

					if (total_planes > 1)
					{
						assert(blk_out.m_packed_dct_plane_data[0].m_num_dc_levels == blk_out.m_packed_dct_plane_data[1].m_num_dc_levels);
					}

					for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
					{
						const basist::astc_ldr_t::dct_syms& syms = blk_out.m_packed_dct_plane_data[plane_iter];
																		
						if (syms.m_num_dc_levels == basist::astc_ldr_t::DCT_MEAN_LEVELS1)
							mean1_bytes.push_back((uint8_t)syms.m_dc_sym);
						else
						{
							assert(syms.m_num_dc_levels == basist::astc_ldr_t::DCT_MEAN_LEVELS0);
							mean0_bits.put_bits(syms.m_dc_sym, 4);
						}

						for (uint32_t i = 0; i < syms.m_coeffs.size(); i++)
						{
							if (syms.m_coeffs[i].m_coeff == INT16_MAX)
							{
								run_bytes.push_back(basist::astc_ldr_t::DCT_RUN_LEN_EOB_SYM_INDEX);
							}
							else
							{
								run_bytes.push_back((uint8_t)syms.m_coeffs[i].m_num_zeros);
								
								sign_bits.put_bits(syms.m_coeffs[i].m_coeff < 0, 1);

								assert((syms.m_coeffs[i].m_coeff != 0) && (iabs(syms.m_coeffs[i].m_coeff) <= 255));

								coeff_bytes.push_back((uint8_t)(iabs(syms.m_coeffs[i].m_coeff) - 1));
							}
						}

					} // plane_iter
				}
				else
				{
					total_used_weight_dpcm++;
										
					for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
					{
						int prev_w = num_weight_levels / 2;

						for (uint32_t weight_iter = 0; weight_iter < total_weights; weight_iter++)
						{
							int ise_w = cur_log_blk.m_weights[plane_iter + weight_iter * total_planes];
							int w = weight_ise_to_rank[ise_w];

							int w_to_code = w;
							w_to_code = imod(w - prev_w, num_weight_levels);

							prev_w = w;
							
							if (num_weight_levels <= 4)
								weight2_bits.put_bits((uint8_t)w_to_code, 2);
							else if (num_weight_levels <= 8)
								weight3_bits.put_bits((uint8_t)w_to_code, 4);
							else if (num_weight_levels <= 16)
								weight4_bits.put_bits((uint8_t)w_to_code, 4);
							else
								weight8_bits.push_back((uint8_t)w_to_code);
														
						} // weight_iter

					} // plane_iter
				}
			}
			else
			{
				float total_dpcm_bits = 0.0f, total_dct_bits = 0.0f;
				const float FORBID_DCT_BITS = 1e+8f;

				for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
				{
					int prev_w = num_weight_levels / 2;

					for (uint32_t weight_iter = 0; weight_iter < total_weights; weight_iter++)
					{
						const auto& model = raw_weight_models[cur_log_blk.m_weight_ise_range - astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE];

						int ise_w = cur_log_blk.m_weights[plane_iter + weight_iter * total_planes];
						int w = weight_ise_to_rank[ise_w];

						int w_to_code = w;
						w_to_code = imod(w - prev_w, num_weight_levels);

						prev_w = w;

						total_dpcm_bits += model.get_price(w_to_code);

					} // weight_iter

				} // plane_iter

				if (enc_cfg.m_use_dct)
				{
					for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
					{
						const basist::astc_ldr_t::dct_syms& syms = blk_out.m_packed_dct_plane_data[plane_iter];
						if (syms.m_max_coeff_mag > basist::astc_ldr_t::DCT_MAX_ARITH_COEFF_MAG)
						{
							total_dct_bits = FORBID_DCT_BITS;
							break;
						}
					}

					if (total_dct_bits < FORBID_DCT_BITS)
					{
						for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
						{
							const basist::astc_ldr_t::dct_syms& syms = blk_out.m_packed_dct_plane_data[plane_iter];

							assert((syms.m_num_dc_levels == basist::astc_ldr_t::DCT_MEAN_LEVELS0) || (syms.m_num_dc_levels == basist::astc_ldr_t::DCT_MEAN_LEVELS1));

							total_dct_bits += weight_mean_models[(syms.m_num_dc_levels == basist::astc_ldr_t::DCT_MEAN_LEVELS1) ? 1 : 0].get_price(syms.m_dc_sym);

							for (uint32_t i = 0; i < syms.m_coeffs.size(); i++)
							{
								if (syms.m_coeffs[i].m_coeff == INT16_MAX)
								{
									total_dct_bits += dct_run_len_model.get_price(basist::astc_ldr_t::DCT_RUN_LEN_EOB_SYM_INDEX);
								}
								else
								{
									assert(syms.m_coeffs[i].m_num_zeros < basist::astc_ldr_t::DCT_RUN_LEN_EOB_SYM_INDEX);

									total_dct_bits += dct_run_len_model.get_price(syms.m_coeffs[i].m_num_zeros);

									total_dct_bits += 1.0f; // sign bit
									assert((syms.m_coeffs[i].m_coeff != 0) && (iabs(syms.m_coeffs[i].m_coeff) <= 255));
									total_dct_bits += dct_coeff_mag.get_price(iabs(syms.m_coeffs[i].m_coeff) - 1);
								}
							} // i
						} // plane_iter
					}
				}
								
				// TODO: Check if any DCT coeff overflows 8-bit mags, switch to DPCM. (In practice, not needed.)
				bool use_dct = false;
				if ((enc_cfg.m_use_dct) &&
					(total_dct_bits < FORBID_DCT_BITS) &&
					((total_dct_bits + use_dct_model[use_dct_model_index].get_price(1)) <= (total_dpcm_bits + use_dct_model[use_dct_model_index].get_price(0))))
				{
					use_dct = true;
				}

				if (enc_cfg.m_use_dct)
				{
					total_weight_bits += use_dct_model[use_dct_model_index].get_price(use_dct);
					enc.encode(use_dct, use_dct_model[use_dct_model_index]);
				}

				if (use_dct)
				{
					prev_state.m_used_weight_dct = true;

					total_used_dct++;

					if (total_planes > 1)
					{
						assert(blk_out.m_packed_dct_plane_data[0].m_num_dc_levels == blk_out.m_packed_dct_plane_data[1].m_num_dc_levels);
					}

					for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
					{
						const basist::astc_ldr_t::dct_syms& syms = blk_out.m_packed_dct_plane_data[plane_iter];

						total_weight_bits += enc.encode_and_return_price(syms.m_dc_sym, weight_mean_models[(syms.m_num_dc_levels == basist::astc_ldr_t::DCT_MEAN_LEVELS1) ? 1 : 0]);

						for (uint32_t i = 0; i < syms.m_coeffs.size(); i++)
						{
							if (syms.m_coeffs[i].m_coeff == INT16_MAX)
							{
								total_weight_bits += enc.encode_and_return_price(basist::astc_ldr_t::DCT_RUN_LEN_EOB_SYM_INDEX, dct_run_len_model);
							
								total_dct_syms++;
							}
							else
							{
								total_weight_bits += enc.encode_and_return_price(syms.m_coeffs[i].m_num_zeros, dct_run_len_model);
							
								total_dct_syms++;

								enc.put_bit(syms.m_coeffs[i].m_coeff < 0);
								total_weight_bits += 1.0f;

								assert((syms.m_coeffs[i].m_coeff != 0) && (iabs(syms.m_coeffs[i].m_coeff) <= 255));
								total_weight_bits += enc.encode_and_return_price(iabs(syms.m_coeffs[i].m_coeff) - 1, dct_coeff_mag);
							
								total_dct_syms++;
							}
						}

					} // plane_iter
				}
				else
				{
					total_used_weight_dpcm++;
					auto& model = raw_weight_models[cur_log_blk.m_weight_ise_range - astc_helpers::FIRST_VALID_WEIGHT_ISE_RANGE];

					for (uint32_t plane_iter = 0; plane_iter < total_planes; plane_iter++)
					{
						int prev_w = num_weight_levels / 2;

						for (uint32_t weight_iter = 0; weight_iter < total_weights; weight_iter++)
						{
							int ise_w = cur_log_blk.m_weights[plane_iter + weight_iter * total_planes];
							int w = weight_ise_to_rank[ise_w];

							int w_to_code = w;
							w_to_code = imod(w - prev_w, num_weight_levels);

							prev_w = w;

							total_weight_bits += model.get_price(w_to_code);
							enc.encode(w_to_code, model);

							total_dpcm_syms++;

						} // weight_iter

					} // plane_iter
				}

			} // use_faster_format

		} // bx

		if (cur_run_len)
		{
			total_runs++;
			total_run_blocks += cur_run_len;

			total_header_bits += enc.encode_and_return_price((uint32_t)basist::astc_ldr_t::xuastc_mode::cMODE_RUN, mode_model);
			total_header_bits += enc.put_gamma_and_return_price(cur_run_len, m_run_len_contexts);
			cur_run_len = 0;
		}

	} // by
		
	enc.put_bits(basist::astc_ldr_t::FINAL_SYNC_MARKER, basist::astc_ldr_t::FINAL_SYNC_MARKER_BITS);

	enc.flush();

	if (global_cfg.m_debug_output)
	{
		fmt_debug_printf("Supercompression (backend) encoding time: {} secs\n", itm.get_elapsed_secs());
	}

	if (global_cfg.m_debug_images)
	{
		save_png(global_cfg.m_debug_file_prefix + "vis_img.png", vis_img);
	}

	if ((global_cfg.m_debug_images) || (global_cfg.m_debug_output))
	{
		image input_img(width, height);
		image coded_img(width, height);

		vector2D<astc_helpers::astc_block> phys_blocks(num_blocks_x, num_blocks_y);

		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				const astc_helpers::log_astc_block& log_blk = coded_blocks(bx, by);

				color_rgba block_pixels[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];

				bool status = astc_helpers::decode_block(log_blk, block_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
				if (!status)
				{
					fmt_error_printf("astc_helpers::decode_block() failed (3)\n");
					return false;
				}

				// Be positive the logical block can be unpacked correctly as XUASTC LDR.
				color_rgba block_pixels_alt[astc_ldr::ASTC_LDR_MAX_BLOCK_PIXELS];
				bool status_alt = astc_helpers::decode_block_xuastc_ldr(log_blk, block_pixels_alt, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
				if (!status_alt)
				{
					fmt_error_printf("astc_helpers::decode_block_xuastc_ldr() failed\n");
					return false;
				}

				if (memcmp(block_pixels, block_pixels_alt, sizeof(color_rgba) * block_width * block_height) != 0)
				{
					fmt_error_printf("astc_helpers::decode_block_xuastc_ldr() decode pixel mismatch\n");
					return false;
				}

				coded_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);

				if (global_cfg.m_debug_images)
				{
					// input image

					status = astc_helpers::decode_block(input_blocks(bx, by), block_pixels, block_width, block_height, enc_cfg.m_cem_enc_params.m_decode_mode_srgb ? astc_helpers::cDecodeModeSRGB8 : astc_helpers::cDecodeModeLDR8);
					if (!status)
					{
						fmt_error_printf("astc_helpers::decode_block() failed (4)\n");
						return false;
					}

					input_img.set_block_clipped(block_pixels, bx * block_width, by * block_height, block_width, block_height);
				}

			} // bx

		} // by

		if (global_cfg.m_debug_images)
		{
			save_png(global_cfg.m_debug_file_prefix + "input_img.png", input_img);
			debug_printf("Wrote input_img.png\n");

			save_png(global_cfg.m_debug_file_prefix + "coded_img.png", coded_img);
			debug_printf("Wrote coded_img.png\n");
		}

		if ((global_cfg.m_debug_output) && (global_cfg.m_debug_output_image_metrics))
		{
			if ((global_cfg.m_sharpen_flag) && (global_cfg.m_sharpen_amount > 0.0f))
				debug_printf("Sharepened orig image vs. coded img:\n");
			else
				debug_printf("Orig image vs. coded img:\n");

			print_image_metrics(orig_img, coded_img);

			debug_printf("display_astc_statistics:\n");
			display_astc_statistics(coded_blocks, block_width, block_height, orig_img.get_width(), orig_img.get_height(), false);
		}
	}
		
	const uint64_t comp_data_size = enc.get_data_buf().size();
	if (comp_data_size > UINT32_MAX)
		return false;
	
	uint8_vec suffix_bytes;
		
	if (use_faster_format)
	{
#if !BASISD_SUPPORT_KTX2_ZSTD
		fmt_error_printf("Full ZStd syntax not supported in this build (set BASISD_SUPPORT_KTX2_ZSTD to 1)\n");
		return false;
#else
		suffix_bytes.reserve(8192);

		mean0_bits.flush();
		sign_bits.flush();
		weight2_bits.flush();
		weight3_bits.flush();
		weight4_bits.flush();

		const uint32_t zstd_level = 9;

		uint8_vec comp_mean0, comp_mean1, comp_run, comp_coeff, comp_weight2, comp_weight3, comp_weight4, comp_weight8;
		
		if (!zstd_compress(mean0_bits.get_bytes().data(), mean0_bits.get_bytes().size(), comp_mean0, zstd_level))
			return false;
		if (!zstd_compress(mean1_bytes.data(), mean1_bytes.size(), comp_mean1, zstd_level))
			return false;
		if (!zstd_compress(run_bytes.data(), run_bytes.size(), comp_run, zstd_level)) 
			return false;
		if (!zstd_compress(coeff_bytes.data(), coeff_bytes.size(), comp_coeff, zstd_level)) 
			return false;
		if (!zstd_compress(weight2_bits.get_bytes().data(), weight2_bits.get_bytes().size(), comp_weight2, zstd_level))
			return false;
		if (!zstd_compress(weight3_bits.get_bytes().data(), weight3_bits.get_bytes().size(), comp_weight3, zstd_level))
			return false;
		if (!zstd_compress(weight4_bits.get_bytes().data(), weight4_bits.get_bytes().size(), comp_weight4, zstd_level))
			return false;
		if (!zstd_compress(weight8_bits.data(), weight8_bits.size(), comp_weight8, zstd_level))
			return false;
				
		hdr.m_flags = (uint8_t)basist::astc_ldr_t::xuastc_ldr_syntax::cHybridArithZStd;

		hdr.m_arith_bytes_len = (uint32_t)comp_data_size;
		hdr.m_mean0_bits_len = (uint32_t)comp_mean0.size();
		hdr.m_mean1_bytes_len = (uint32_t)comp_mean1.size();
		hdr.m_run_bytes_len = (uint32_t)comp_run.size();
		hdr.m_coeff_bytes_len = (uint32_t)comp_coeff.size();
		hdr.m_sign_bits_len = (uint32_t)sign_bits.get_bytes().size();
		hdr.m_weight2_bits_len = (uint32_t)comp_weight2.size();
		hdr.m_weight3_bits_len = (uint32_t)comp_weight3.size();
		hdr.m_weight4_bits_len = (uint32_t)comp_weight4.size();
		hdr.m_weight8_bytes_len = (uint32_t)comp_weight8.size();

		suffix_bytes.append(comp_mean0);
		suffix_bytes.append(comp_mean1);
		suffix_bytes.append(comp_run);
		suffix_bytes.append(comp_coeff);
		suffix_bytes.append(sign_bits.get_bytes());
		suffix_bytes.append(comp_weight2);
		suffix_bytes.append(comp_weight3);
		suffix_bytes.append(comp_weight4);
		suffix_bytes.append(comp_weight8);

		if (global_cfg.m_debug_output)
		{
			fmt_debug_printf("Zstd compressed sizes:\n");
			fmt_debug_printf(" Mean0 bytes: {} comp size: {}\n", (uint64_t)mean0_bits.get_bytes().size(), (uint64_t)comp_mean0.size());
			fmt_debug_printf(" Mean1 bytes: {} comp size: {}\n", (uint64_t)mean1_bytes.size(), (uint64_t)comp_mean1.size());
			fmt_debug_printf(" Run bytes: {} comp size: {}\n", (uint64_t)run_bytes.size(), (uint64_t)comp_run.size());
			fmt_debug_printf(" Coeff bytes: {} comp size: {}\n", (uint64_t)coeff_bytes.size(), (uint64_t)comp_coeff.size());
			fmt_debug_printf(" Sign bytes: {}\n", (uint64_t)sign_bits.get_bytes().size());
			fmt_debug_printf(" Weight2 bytes: {} comp size: {}\n", (uint64_t)weight2_bits.get_bytes().size(), (uint64_t)comp_weight2.size());
			fmt_debug_printf(" Weight3 bytes: {} comp size: {}\n", (uint64_t)weight3_bits.get_bytes().size(), (uint64_t)comp_weight3.size());
			fmt_debug_printf(" Weight4 bytes: {} comp size: {}\n", (uint64_t)weight4_bits.get_bytes().size(), (uint64_t)comp_weight4.size());
			fmt_debug_printf(" Weight8 bytes: {} comp size: {}\n", (uint64_t)weight8_bits.size(), (uint64_t)comp_weight8.size());
		}
#endif
	}
		
	assert(comp_data.size() == 0);
	if (use_faster_format)
	{
		comp_data.resize(sizeof(hdr));
		memcpy(comp_data.data(), &hdr, sizeof(hdr));
	}
	else
	{
		comp_data.push_back((uint8_t)basist::astc_ldr_t::xuastc_ldr_syntax::cFullArith);
	}

	comp_data.append(enc.get_data_buf());

	comp_data.append(suffix_bytes);

	if (comp_data.size() > UINT32_MAX)
		return false;
				
	if (global_cfg.m_debug_output)
	{
		fmt_debug_printf("Total blocks: {}\n", total_blocks);
		fmt_debug_printf("Total lossy replacements made by supercompression layer: {} {3.2}%\n", total_lossy_replacements, (float)total_lossy_replacements * 100.0f / (float)total_blocks);
		fmt_debug_printf("Total runs: {}, total run blocks: {} {3.2}%\n", total_runs, total_run_blocks, (float)total_run_blocks * 100.0f / (float)total_blocks);
		fmt_debug_printf("Total blocks coded (not inside runs): {} {3.2}%\n", total_nonrun_blocks, (float)total_nonrun_blocks * 100.0f / (float)total_blocks);
		fmt_debug_printf("num_part_hash_probes: {}, num_part_hash_hits: {} {3.2}%\n", num_part_hash_probes, num_part_hash_hits, num_part_hash_probes ? ((float)num_part_hash_hits * 100.0f / (float)num_part_hash_probes) : 0);
		fmt_debug_printf("Total DCT syms: {}, DPCM syms: {}\n", total_dct_syms, total_dpcm_syms);
		
		const uint32_t total_non_void_extent_blocks = total_blocks - total_solid_blocks;

		fmt_debug_printf("Total blocks using void extent: {} {3.2}%\n",
			total_solid_blocks, (float)total_solid_blocks * 100.0f / (float)total_blocks);

		fmt_debug_printf("Total non void-extent blocks: {} {3.2}%\n",
			total_non_void_extent_blocks, (float)total_non_void_extent_blocks * 100.0f / (float)total_blocks);

		fmt_debug_printf("Total full cfg+part ID+endpoint reuse commands: {} {3.2}%\n",
			total_full_reuse_commands, (float)total_full_reuse_commands * 100.0f / (float)total_blocks);

		fmt_debug_printf("Total raw commands: {} {3.2}%\n",
			total_raw_commands, (float)total_raw_commands * 100.0f / (float)total_blocks);

		fmt_debug_printf("Total reuse cfg+part ID emitted: {} {3.2}%, Total full cfg emitted: {} {3.2}%\n",
			total_reuse_full_cfg_emitted, (float)total_reuse_full_cfg_emitted * 100.0f / (float)total_blocks,
			total_full_cfg_emitted, (float)total_full_cfg_emitted * 100.0f / (float)total_blocks);

		fmt_debug_printf("Total coded endpoints using DPCM: {} {3.2}%\n",
			total_used_endpoint_dpcm, (float)total_used_endpoint_dpcm * 100.0f / (float)total_non_void_extent_blocks);

		fmt_debug_printf("Total coded endpoints using RAW: {} {3.2}%\n",
			total_used_endpoint_raw, (float)total_used_endpoint_raw * 100.0f / (float)total_non_void_extent_blocks);

		fmt_debug_printf("Total coded blocks using weight DCT: {} {3.2}%, total blocks using weight DPCM: {} {3.2}%\n",
			total_used_dct, (float)total_used_dct * 100.0f / total_non_void_extent_blocks,
			total_used_weight_dpcm, (float)total_used_weight_dpcm * 100.0f / (float)total_non_void_extent_blocks);

		fmt_debug_printf("Total header bits: {} bytes: {}, bpp: {}, bits per non-void extent block: {}\nTotal endpoint bits: {}, bytes: {}, bpp: {}, bits per non-void extent block: {}\nTotal weight bits: {}, bytes: {}, bpp: {}, bits per non-void extent block: {}\nTotal_bits: {} bytes: {}, bpp {}, bits per non-void extent block: {}\n",
			total_header_bits, total_header_bits / 8.0f, total_header_bits / (double)total_pixels, total_header_bits / (double)total_non_void_extent_blocks,
			total_endpoint_bits, total_endpoint_bits / 8.0f, total_endpoint_bits / (double)total_pixels, total_endpoint_bits / (double)total_non_void_extent_blocks,
			total_weight_bits, total_weight_bits / 8.0f, total_weight_bits / (double)total_pixels, total_weight_bits / (double)total_non_void_extent_blocks,
			total_header_bits + total_endpoint_bits + total_weight_bits,
			(total_header_bits + total_endpoint_bits + total_weight_bits) / 8.0f,
			(total_header_bits + total_endpoint_bits + total_weight_bits) / (double)total_pixels,
			(total_header_bits + total_endpoint_bits + total_weight_bits) / (double)total_non_void_extent_blocks);

		fmt_debug_printf("Compressed to {} bytes, {3.3}bpp\n\n", comp_data.size_u32(), ((float)comp_data.size() * 8.0f) / (float)total_pixels);

#if 0
		for (uint32_t i = 0; i < 4; i++)
		{
			solid_color_dpcm_model[i].print_prices(fmt_string("solid_color_dpcm_model[{}]:\n\n", i).c_str());
		}
#endif
	}
		
	return true;
}

void encoder_init()
{
	if (g_initialized)
		return;
		
	g_initialized = true;

	for (uint32_t h_iter = 0; h_iter < basist::astc_ldr_t::astc_block_grid_data_hash_t::LUT_SIZE; ++h_iter)
	{
		const uint32_t hash_index = basist::astc_ldr_t::g_astc_block_grid_data_hash.m_hash[h_iter];
		if (!hash_index)
			continue;
		
		uint32_t block_width, block_height, grid_width, grid_height;
		basist::astc_ldr_t::astc_block_grid_data_hash_t::astc_cfg_from_index(h_iter, block_width, block_height, grid_width, grid_height);

		basist::astc_ldr_t::astc_block_grid_data& grid_data = basist::astc_ldr_t::g_astc_block_grid_data_hash.m_grid_data[hash_index - 1];

#if defined(DEBUG) || defined(_DEBUG)
		assert(grid_data.m_bw == block_width);
		assert(grid_data.m_bh == block_height);
		assert(grid_data.m_gw == grid_width);
		assert(grid_data.m_gh == grid_height);
#endif
						
		const uint32_t num_block_samples = block_width * block_height;
		const uint32_t num_grid_samples = grid_width * grid_height;

		grid_data.m_upsample_weights.resize(num_block_samples);
		astc_helpers::compute_upsample_weights(block_width, block_height, grid_width, grid_height, grid_data.m_upsample_weights.get_ptr());

		grid_data.m_grid_to_texel_influence_list.resize(num_grid_samples);
		for (uint32_t grid_sample = 0; grid_sample < num_grid_samples; grid_sample++)
		{
			for (uint32_t block_sample = 0; block_sample < num_block_samples; block_sample++)
			{
				const float weight = grid_data.m_downsample_matrix[grid_sample * num_block_samples + block_sample];
				if (weight == 0.0f)
					continue;

				const uint32_t texel_x = block_sample % block_width;
				const uint32_t texel_y = block_sample / block_width;
				
				// Create a list of texels that influence each grid sample, for fast lookup during encoding.
				grid_data.m_grid_to_texel_influence_list[grid_sample].push_back(basisu::safe_cast_uint16(texel_x | (texel_y << 8)));
			}
			
		} // grid_sample
		
		// used for gradient descent
		compute_upsample_matrix_transposed(grid_data.m_unweighted_downsample_matrix, block_width, block_height, grid_width, grid_height);

		grid_data.m_one_over_diag_AtA.resize(num_grid_samples);
		compute_diag_AtA_vector(block_width, block_height, grid_width, grid_height, grid_data.m_upsample_matrix, grid_data.m_one_over_diag_AtA.get_ptr());
		for (uint32_t i = 0; i < num_grid_samples; i++)
			grid_data.m_one_over_diag_AtA[i] = 1.0f / grid_data.m_one_over_diag_AtA[i];
		
	} // it
}

#if 0
void deblock_filter(uint32_t filter_block_width, uint32_t filter_block_height, const image& src_img, image& dst_img, bool stronger_filtering, int SKIP_THRESH)
{
	image temp_img(src_img);

	for (int y = 0; y < (int)src_img.get_height(); y++)
	{
		for (int x = filter_block_width; x < (int)src_img.get_width(); x += filter_block_width)
		{
			color_rgba ll(src_img.get_clamped(x - 2, y));
			color_rgba l(src_img.get_clamped(x - 1, y));
			color_rgba r(src_img.get_clamped(x, y));
			color_rgba rr(src_img.get_clamped(x + 1, y));

			if (SKIP_THRESH < 256)
			{
				bool skip_flag = false;
				for (uint32_t c = 0; c < 4; c++)
				{
					int delta = iabs((int)l[c] - (int)r[c]);
					if (delta > SKIP_THRESH)
					{
						skip_flag = true;
						break;
					}
				}

				if (skip_flag)
					continue;
			}

			color_rgba ml, mr;
			for (uint32_t c = 0; c < 4; c++)
			{
				if (stronger_filtering)
				{
					ml[c] = (3 * l[c] + 2 * r[c] + ll[c] + 3) / 6;
					mr[c] = (3 * r[c] + 2 * l[c] + rr[c] + 3) / 6;
				}
				else
				{
					ml[c] = (5 * l[c] + 2 * r[c] + ll[c] + 4) / 8;
					mr[c] = (5 * r[c] + 2 * l[c] + rr[c] + 4) / 8;
				}
			}

			temp_img.set_clipped(x - 1, y, ml);
			temp_img.set_clipped(x, y, mr);

		} // x

	} // y

	dst_img = temp_img;

	for (int x = 0; x < (int)temp_img.get_width(); x++)
	{
		for (int y = filter_block_height; y < (int)temp_img.get_height(); y += filter_block_height)
		{
			color_rgba uu(temp_img.get_clamped(x, y - 2));
			color_rgba u(temp_img.get_clamped(x, y - 1));
			color_rgba d(temp_img.get_clamped(x, y));
			color_rgba dd(temp_img.get_clamped(x, y + 1));

			if (SKIP_THRESH < 256)
			{
				bool skip_flag = false;
				for (uint32_t c = 0; c < 4; c++)
				{
					int delta = iabs((int)u[c] - (int)d[c]);
					if (delta > SKIP_THRESH)
					{
						skip_flag = true;
						break;
					}
				}

				if (skip_flag)
					continue;
			}

			color_rgba mu, md;
			for (uint32_t c = 0; c < 4; c++)
			{
				if (stronger_filtering)
				{
					mu[c] = (3 * u[c] + 2 * d[c] + uu[c] + 3) / 6;
					md[c] = (3 * d[c] + 2 * u[c] + dd[c] + 3) / 6;
				}
				else
				{
					mu[c] = (5 * u[c] + 2 * d[c] + uu[c] + 4) / 8;
					md[c] = (5 * d[c] + 2 * u[c] + dd[c] + 4) / 8;
				}
			}

			dst_img.set_clipped(x, y - 1, mu);
			dst_img.set_clipped(x, y, md);

		} // x

	} // y
}
#endif

} // namespace astc_ldr
}  // namespace basisu

