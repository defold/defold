// basisu_dds_export.cpp
// Copyright (C) 2019-2025 Binomial LLC. All Rights Reserved.
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
//
// See basisu_dds_export.h for an overview.
#include "basisu_dds_export.h"

#include "basisu_comp.h"
#include "basisu_bc7e_scalar.h"
#include "../transcoder/basisu_transcoder_internal.h"
#include "../transcoder/basisu_transcoder_uastc.h"

namespace basisu
{
	// --- DXGI format codes (only the ones we emit). ---
	enum
	{
		DXGI_FORMAT_R8G8B8A8_UNORM = 28,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
		DXGI_FORMAT_R8G8_UNORM = 49,
		DXGI_FORMAT_R8_UNORM = 61,
		DXGI_FORMAT_BC1_UNORM = 71,
		DXGI_FORMAT_BC1_UNORM_SRGB = 72,
		DXGI_FORMAT_BC2_UNORM = 74,
		DXGI_FORMAT_BC2_UNORM_SRGB = 75,
		DXGI_FORMAT_BC3_UNORM = 77,
		DXGI_FORMAT_BC3_UNORM_SRGB = 78,
		DXGI_FORMAT_BC4_UNORM = 80,
		DXGI_FORMAT_BC5_UNORM = 83,
		DXGI_FORMAT_B5G6R5_UNORM = 85,
		DXGI_FORMAT_B5G5R5A1_UNORM = 86,
		DXGI_FORMAT_B8G8R8A8_UNORM = 87,
		DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
		DXGI_FORMAT_BC7_UNORM = 98,
		DXGI_FORMAT_BC7_UNORM_SRGB = 99,
		DXGI_FORMAT_B4G4R4A4_UNORM = 115
	};

	// --- DDS header flag/cap constants. ---
	enum
	{
		DDSD_CAPS = 0x1, DDSD_HEIGHT = 0x2, DDSD_WIDTH = 0x4, DDSD_PITCH = 0x8,
		DDSD_PIXELFORMAT = 0x1000, DDSD_MIPMAPCOUNT = 0x20000, DDSD_LINEARSIZE = 0x80000,

		DDPF_FOURCC = 0x4,

		DDSCAPS_COMPLEX = 0x8, DDSCAPS_TEXTURE = 0x1000, DDSCAPS_MIPMAP = 0x400000,

		DDSCAPS2_CUBEMAP = 0x200, DDSCAPS2_CUBEMAP_ALLFACES = 0xFC00,

		DDS_MAGIC = 0x20534444,				// "DDS "
		DDS_DX10_FOURCC = 0x30315844,		// "DX10"

		DDS_DIMENSION_TEXTURE2D = 3,
		DDS_RESOURCE_MISC_TEXTURECUBE = 0x4
	};

	// Per-format static info.
	struct dds_format_info
	{
		const char* m_pToken;
		bool m_block_compressed;
		uint32_t m_bytes;			// bytes per 4x4 block (compressed) or bytes per pixel (uncompressed)
		uint32_t m_dxgi_unorm;
		uint32_t m_dxgi_srgb;		// 0 if no sRGB variant exists
	};

	static const dds_format_info g_dds_format_info[cDDSFmtTotal] =
	{
		// token        block  bytes  unorm                          srgb
		{ "bc1",        true,  8,     DXGI_FORMAT_BC1_UNORM,         DXGI_FORMAT_BC1_UNORM_SRGB },
		{ "bc2",        true,  16,    DXGI_FORMAT_BC2_UNORM,         DXGI_FORMAT_BC2_UNORM_SRGB },
		{ "bc3",        true,  16,    DXGI_FORMAT_BC3_UNORM,         DXGI_FORMAT_BC3_UNORM_SRGB },
		{ "bc4",        true,  8,     DXGI_FORMAT_BC4_UNORM,         0 },
		{ "bc5",        true,  16,    DXGI_FORMAT_BC5_UNORM,         0 },
		{ "bc7",        true,  16,    DXGI_FORMAT_BC7_UNORM,         DXGI_FORMAT_BC7_UNORM_SRGB },
		{ "a8r8g8b8",   false, 4,     DXGI_FORMAT_B8G8R8A8_UNORM,    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB },
		{ "a8b8g8r8",   false, 4,     DXGI_FORMAT_R8G8B8A8_UNORM,    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
		{ "r8",         false, 1,     DXGI_FORMAT_R8_UNORM,          0 },
		{ "r8g8",       false, 2,     DXGI_FORMAT_R8G8_UNORM,        0 },
		{ "r5g6b5",     false, 2,     DXGI_FORMAT_B5G6R5_UNORM,      0 },
		{ "a1r5g5b5",   false, 2,     DXGI_FORMAT_B5G5R5A1_UNORM,    0 },
		{ "a4r4g4b4",   false, 2,     DXGI_FORMAT_B4G4R4A4_UNORM,    0 }
	};

	bool parse_dds_output_format(const char* pToken, dds_output_format& fmt)
	{
		fmt = cDDSFmtInvalid;
		if (!pToken)
			return false;

		for (int i = 0; i < (int)cDDSFmtTotal; i++)
		{
			if (strcasecmp(pToken, g_dds_format_info[i].m_pToken) == 0)
			{
				fmt = (dds_output_format)i;
				return true;
			}
		}

		// Tolerate the (geometrically impossible) "a1r5g6b5" spelling as an alias for a1r5g5b5.
		if (strcasecmp(pToken, "a1r5g6b5") == 0)
		{
			fmt = cDDSFmtA1R5G5B5;
			return true;
		}

		return false;
	}

	const char* get_dds_output_format_string(dds_output_format fmt)
	{
		if ((fmt < 0) || (fmt >= cDDSFmtTotal))
			return "?";
		return g_dds_format_info[fmt].m_pToken;
	}

	bool dds_output_format_has_srgb_variant(dds_output_format fmt)
	{
		if ((fmt < 0) || (fmt >= cDDSFmtTotal))
			return false;
		return g_dds_format_info[fmt].m_dxgi_srgb != 0;
	}

	// --- Little-endian append helpers. ---
	static inline void append_u16(uint8_vec& v, uint32_t x)
	{
		v.push_back((uint8_t)(x & 0xFF));
		v.push_back((uint8_t)((x >> 8) & 0xFF));
	}
	static inline void append_u32(uint8_vec& v, uint32_t x)
	{
		v.push_back((uint8_t)(x & 0xFF));
		v.push_back((uint8_t)((x >> 8) & 0xFF));
		v.push_back((uint8_t)((x >> 16) & 0xFF));
		v.push_back((uint8_t)((x >> 24) & 0xFF));
	}

	// Packs a single RGBA8 texel into the uncompressed output bytes for fmt (truncating 8->N bits, which is
	// the exact inverse of the reader's bit-replication expansion). Channel byte orders match the DXGI formats.
	static void pack_uncompressed_pixel(uint8_vec& out, const color_rgba& c, dds_output_format fmt)
	{
		switch (fmt)
		{
		case cDDSFmtA8R8G8B8:		// DXGI B8G8R8A8 -> memory order B,G,R,A
			out.push_back(c.b); out.push_back(c.g); out.push_back(c.r); out.push_back(c.a);
			break;
		case cDDSFmtA8B8G8R8:		// DXGI R8G8B8A8 -> memory order R,G,B,A
			out.push_back(c.r); out.push_back(c.g); out.push_back(c.b); out.push_back(c.a);
			break;
		case cDDSFmtR8:				// DXGI R8 -> just R
			out.push_back(c.r);
			break;
		case cDDSFmtR8G8:			// DXGI R8G8 -> source R into R, source G into G (matches BC5; swizzle input for other mappings)
			out.push_back(c.r); out.push_back(c.g);
			break;
		case cDDSFmtR5G6B5:
			append_u16(out, (uint32_t)(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3)));
			break;
		case cDDSFmtA1R5G5B5:
			append_u16(out, (uint32_t)((c.a >= 128 ? 0x8000 : 0) | ((c.r >> 3) << 10) | ((c.g >> 3) << 5) | (c.b >> 3)));
			break;
		case cDDSFmtA4R4G4B4:
			append_u16(out, (uint32_t)(((c.a >> 4) << 12) | ((c.r >> 4) << 8) | ((c.g >> 4) << 4) | (c.b >> 4)));
			break;
		default:
			assert(0);
			break;
		}
	}

	// Prebuilt BC7 packing context (built once per build_dds, shared read-only across slices).
	struct bc7_pack_context
	{
		dds_bc7_encoder m_encoder;
		uint32_t m_bc7f_flags;									// bc7f packer flags (when m_encoder == bc7f)
		bc7e_scalar::bc7e_compress_block_params m_bc7e_params;	// initialized only when m_encoder == bc7e_scalar
	};

	// Packs one prepared slice image into the bytes for fmt, using logical dims (orig_width/orig_height).
	// For block formats this iterates whole 4x4 blocks (the slice image is already block-padded); for
	// uncompressed it emits tight rows of exactly orig_width*orig_height pixels.
	static void pack_slice(uint8_vec& out, const image& img, uint32_t orig_width, uint32_t orig_height, dds_output_format fmt, const bc7_pack_context& bc7ctx)
	{
		const dds_format_info& info = g_dds_format_info[fmt];

		if (info.m_block_compressed)
		{
			const uint32_t blocks_x = (orig_width + 3) / 4;
			const uint32_t blocks_y = (orig_height + 3) / 4;

			for (uint32_t by = 0; by < blocks_y; by++)
			{
				for (uint32_t bx = 0; bx < blocks_x; bx++)
				{
					color_rgba blk[16];
					img.extract_block_clamped(blk, bx * 4, by * 4, 4, 4);

					uint8_t dst[16];
					switch (fmt)
					{
					case cDDSFmtBC1:
						basist::encode_bc1(dst, (const uint8_t*)blk, basist::cEncodeBC1HighQuality);
						break;
					case cDDSFmtBC2:
						// 8 bytes explicit 4-bit alpha (one LE 16-bit word per row, texel x at nibble x), then a
						// BC1 color block (encode_bc1 emits 4-color blocks, which BC2's always-4-color decode needs).
						for (uint32_t ry = 0; ry < 4; ry++)
						{
							uint32_t row = 0;
							for (uint32_t rx = 0; rx < 4; rx++)
								row |= ((uint32_t)(blk[ry * 4 + rx].a >> 4)) << (rx * 4);
							dst[ry * 2] = (uint8_t)(row & 0xFF);
							dst[ry * 2 + 1] = (uint8_t)(row >> 8);
						}
						basist::encode_bc1(dst + 8, (const uint8_t*)blk, basist::cEncodeBC1HighQuality);
						break;
					case cDDSFmtBC3:
						basist::encode_bc4(dst, &blk[0].a, sizeof(color_rgba));
						basist::encode_bc1(dst + 8, (const uint8_t*)blk, basist::cEncodeBC1HighQuality);
						break;
					case cDDSFmtBC4:
						basist::encode_bc4(dst, &blk[0].r, sizeof(color_rgba));
						break;
					case cDDSFmtBC5:
						basist::encode_bc4(dst, &blk[0].r, sizeof(color_rgba));
						basist::encode_bc4(dst + 8, &blk[0].g, sizeof(color_rgba));
						break;
					case cDDSFmtBC7:
						// basist::color_rgba and basisu::color_rgba have identical layout (r,g,b,a uint8_t).
						if (bc7ctx.m_encoder == cDDSBC7Encoder_BC7E_Scalar)
						{
							uint64_t blk64[2];
							bc7e_scalar::bc7e_compress_blocks(1, blk64, reinterpret_cast<const uint32_t*>(blk), &bc7ctx.m_bc7e_params, nullptr);
							memcpy(dst, blk64, 16);
						}
						else
						{
							basist::bc7f::fast_pack_bc7_auto_rgba(dst, reinterpret_cast<const basist::color_rgba*>(blk), bc7ctx.m_bc7f_flags);
						}
						break;
					default:
						assert(0);
						break;
					}

					out.append(dst, info.m_bytes);
				}
			}
		}
		else
		{
			for (uint32_t y = 0; y < orig_height; y++)
				for (uint32_t x = 0; x < orig_width; x++)
					pack_uncompressed_pixel(out, img(x, y), fmt);
		}
	}

	// Builds the DDS magic + DDS_HEADER + DDS_HEADER_DXT10 (148 bytes total).
	static void build_dx10_header(uint8_vec& out, uint32_t width, uint32_t height, uint32_t mip_count,
		uint32_t array_size, bool is_cubemap, uint32_t dxgi_format, const dds_format_info& info)
	{
		// dwPitchOrLinearSize (informational; readers generally recompute).
		uint32_t pitch_or_linear_size;
		uint32_t flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
		if (info.m_block_compressed)
		{
			pitch_or_linear_size = ((width + 3) / 4) * ((height + 3) / 4) * info.m_bytes;
			flags |= DDSD_LINEARSIZE;
		}
		else
		{
			pitch_or_linear_size = width * info.m_bytes;
			flags |= DDSD_PITCH;
		}
		if (mip_count > 1)
			flags |= DDSD_MIPMAPCOUNT;

		uint32_t caps = DDSCAPS_TEXTURE;
		if ((mip_count > 1) || is_cubemap || (array_size > 1))
			caps |= DDSCAPS_COMPLEX;
		if (mip_count > 1)
			caps |= DDSCAPS_MIPMAP;

		const uint32_t caps2 = is_cubemap ? (DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES) : 0;

		append_u32(out, DDS_MAGIC);

		// DDS_HEADER (124 bytes).
		append_u32(out, 124);				// dwSize
		append_u32(out, flags);				// dwFlags
		append_u32(out, height);			// dwHeight
		append_u32(out, width);				// dwWidth
		append_u32(out, pitch_or_linear_size);
		append_u32(out, 0);					// dwDepth
		append_u32(out, mip_count);			// dwMipMapCount
		for (uint32_t i = 0; i < 11; i++)	// dwReserved1[11]
			append_u32(out, 0);

		// DDS_PIXELFORMAT (32 bytes) - DX10 indirection.
		append_u32(out, 32);				// ddspf.dwSize
		append_u32(out, DDPF_FOURCC);		// ddspf.dwFlags
		append_u32(out, DDS_DX10_FOURCC);	// ddspf.dwFourCC = "DX10"
		append_u32(out, 0);					// dwRGBBitCount
		append_u32(out, 0);					// dwRBitMask
		append_u32(out, 0);					// dwGBitMask
		append_u32(out, 0);					// dwBBitMask
		append_u32(out, 0);					// dwABitMask

		append_u32(out, caps);				// dwCaps
		append_u32(out, caps2);				// dwCaps2
		append_u32(out, 0);					// dwCaps3
		append_u32(out, 0);					// dwCaps4
		append_u32(out, 0);					// dwReserved2

		// DDS_HEADER_DXT10 (20 bytes).
		append_u32(out, dxgi_format);		// dxgiFormat
		append_u32(out, DDS_DIMENSION_TEXTURE2D);
		append_u32(out, is_cubemap ? DDS_RESOURCE_MISC_TEXTURECUBE : 0);	// miscFlag
		append_u32(out, array_size);		// arraySize (number of array elements; cubes for a cubemap)
		append_u32(out, 0);					// miscFlags2
	}

	bool build_dds(uint8_vec& dds_data, const basis_compressor& comp, const dds_export_params& params,
		std::string& error_msg,
		uint32_t* pOut_width, uint32_t* pOut_height,
		uint32_t* pOut_levels, uint32_t* pOut_layers, uint32_t* pOut_faces)
	{
		error_msg.clear();
		dds_data.resize(0);

		const dds_output_format fmt = params.m_format;

		if ((fmt < 0) || (fmt >= cDDSFmtTotal))
		{
			error_msg = "invalid output format";
			return false;
		}

		const dds_format_info& info = g_dds_format_info[fmt];

		const basisu::vector<image>& slices = comp.get_slice_images();
		const basisu_backend_slice_desc_vec& descs = comp.get_slice_descs();

		if (!slices.size() || (slices.size() != descs.size()))
		{
			error_msg = "no prepared slices (was process_source_images() run successfully?)";
			return false;
		}

		const bool is_cubemap = (comp.get_params().m_tex_type == basist::cBASISTexTypeCubemapArray);

		// Determine base dims, layer count, mip level count, and face count - mirrors create_ktx2_file().
		uint32_t base_width = 0, base_height = 0, total_layers = 0, total_levels = 0, total_faces = 1;
		for (uint32_t i = 0; i < descs.size(); i++)
		{
			if ((descs[i].m_mip_index == 0) && (!base_width))
			{
				base_width = descs[i].m_orig_width;
				base_height = descs[i].m_orig_height;
			}

			total_layers = maximum<uint32_t>(total_layers, descs[i].m_source_file_index + 1);

			if (!descs[i].m_source_file_index)
				total_levels = maximum<uint32_t>(total_levels, descs[i].m_mip_index + 1);
		}

		if (is_cubemap)
		{
			if ((total_layers % 6) != 0)
			{
				error_msg = "cubemap source image count is not a multiple of 6";
				return false;
			}
			total_layers /= 6;
			total_faces = 6;
		}

		if (!base_width || !base_height || !total_layers || !total_levels)
		{
			error_msg = "could not determine texture dimensions from the prepared slices";
			return false;
		}

		// Build a (layer, face, level) -> slice index map using the same decomposition as create_ktx2_file().
		const uint32_t total_subresources = total_layers * total_faces * total_levels;
		basisu::vector<int> slice_map(total_subresources);
		for (uint32_t i = 0; i < total_subresources; i++)
			slice_map[i] = -1;

		for (uint32_t i = 0; i < descs.size(); i++)
		{
			// Note: descs[i].m_alpha just flags that the slice contains alpha; in the XUBC7 (m_uastc) path each
			// slice holds full RGBA, so it is NOT a separate alpha-only slice (that only happens for ETC1S).

			const uint32_t level_index = descs[i].m_mip_index;
			uint32_t layer_index = descs[i].m_source_file_index;
			uint32_t face_index = 0;
			if (is_cubemap)
			{
				face_index = layer_index % 6;
				layer_index /= 6;
			}

			if ((layer_index >= total_layers) || (face_index >= total_faces) || (level_index >= total_levels))
			{
				error_msg = "slice descriptor out of range (internal error)";
				return false;
			}

			const uint32_t map_index = (layer_index * total_faces + face_index) * total_levels + level_index;

			// Two slices mapping to the same subresource would indicate RGB/alpha slice splitting (ETC1S), which
			// we don't expect here since we force the XUBC7 path.
			if (slice_map[map_index] >= 0)
			{
				error_msg = "multiple slices map to the same (layer, face, level) - RGB/alpha slice splitting is not supported";
				return false;
			}

			slice_map[map_index] = (int)i;
		}

		for (uint32_t i = 0; i < total_subresources; i++)
		{
			if (slice_map[i] < 0)
			{
				error_msg = "missing slice for a (layer, face, level) - source images are not all the same size / mip count";
				return false;
			}
		}

		// Select the DXGI variant (UNORM vs sRGB).
		const bool want_srgb = comp.get_params().m_ktx2_and_basis_srgb_transfer_function;
		const uint32_t dxgi_format = (want_srgb && info.m_dxgi_srgb) ? info.m_dxgi_srgb : info.m_dxgi_unorm;
		if (want_srgb && !info.m_dxgi_srgb && comp.get_params().m_status_output)
			printf("Note: format %s has no sRGB DXGI variant; writing UNORM (texel data is unchanged regardless).\n", info.m_pToken);

		// Build the BC7 packing context once (only relevant for the bc7 format).
		bc7_pack_context bc7ctx;
		memset(&bc7ctx, 0, sizeof(bc7ctx));
		bc7ctx.m_encoder = params.m_bc7_encoder;
		switch (clamp<int>(params.m_bc7f_level, cDDSBC7FLevel_Analytical, cDDSBC7FLevel_NonAnalytical))
		{
		case cDDSBC7FLevel_Analytical:    bc7ctx.m_bc7f_flags = basist::bc7f::cPackBC7FlagDefault; break;
		case cDDSBC7FLevel_NonAnalytical: bc7ctx.m_bc7f_flags = basist::bc7f::cPackBC7FlagDefaultNonAnalytical; break;
		default:                          bc7ctx.m_bc7f_flags = basist::bc7f::cPackBC7FlagDefaultPartiallyAnalytical; break;
		}
		if ((fmt == cDDSFmtBC7) && (params.m_bc7_encoder == cDDSBC7Encoder_BC7E_Scalar))
		{
			bc7e_scalar::bc7e_compress_block_init();

			typedef void (*bc7e_init_func)(bc7e_scalar::bc7e_compress_block_params*, bool);
			static const bc7e_init_func s_bc7e_level_init[7] =
			{
				&bc7e_scalar::bc7e_compress_block_params_init_ultrafast,	// 0
				&bc7e_scalar::bc7e_compress_block_params_init_veryfast,	// 1
				&bc7e_scalar::bc7e_compress_block_params_init_fast,		// 2
				&bc7e_scalar::bc7e_compress_block_params_init_basic,		// 3
				&bc7e_scalar::bc7e_compress_block_params_init_slow,		// 4
				&bc7e_scalar::bc7e_compress_block_params_init_veryslow,	// 5
				&bc7e_scalar::bc7e_compress_block_params_init_slowest,	// 6
			};
			const int lvl = clamp<int>(params.m_bc7e_scalar_level, 0, 6);

			// Mirrors the XUBC7 path: for perceptual (sRGB) sources run bc7e in its built-in perceptual error
			// mode (it ignores m_weights then); for linear sources run linear and hand it the same RGBA channel
			// weights XUASTC LDR / XUBC7 use.
			const bool perceptual = comp.get_params().m_perceptual;
			s_bc7e_level_init[lvl](&bc7ctx.m_bc7e_params, perceptual);

			if (!perceptual)
			{
				for (uint32_t i = 0; i < 4; i++)
					bc7ctx.m_bc7e_params.m_weights[i] = comp.get_params().m_ldr_channel_weights[i];
			}
		}

		// Optional debug logging (gated by the compressor's m_debug param, same flag -debug/-verbose set).
		const bool debug = comp.get_params().m_debug;

		if (debug)
		{
			const bool wrote_srgb_dbg = want_srgb && (info.m_dxgi_srgb != 0);
			debug_printf("DDS export: format \"%s\" (%s), DXGI=%u, %ux%u, %u level(s), %u layer(s), %u face(s) (%s), %u subresource(s)\n",
				info.m_pToken, wrote_srgb_dbg ? "sRGB" : "UNORM", dxgi_format, base_width, base_height,
				total_levels, total_layers, total_faces, is_cubemap ? "cubemap" : "2D", total_subresources);
			debug_printf("DDS export: %s, %u byte(s) per %s\n",
				info.m_block_compressed ? "block-compressed" : "uncompressed", info.m_bytes,
				info.m_block_compressed ? "block" : "pixel");

			if (fmt == cDDSFmtBC7)
			{
				// Print the FULL BC7 configuration (both encoders' settings) regardless of which encoder
				// is active, so the actual values used can be verified at a glance. The "encoder=" line
				// states which one is in effect; the line for the other encoder is informational.
				const bool using_bc7e = (params.m_bc7_encoder == cDDSBC7Encoder_BC7E_Scalar);

				const int bc7f_lvl = clamp<int>(params.m_bc7f_level, cDDSBC7FLevel_Analytical, cDDSBC7FLevel_NonAnalytical);
				const char* pBc7fLvl = (bc7f_lvl == cDDSBC7FLevel_Analytical) ? "analytical" :
					(bc7f_lvl == cDDSBC7FLevel_NonAnalytical) ? "non-analytical" : "partially-analytical";

				const int bc7e_lvl = clamp<int>(params.m_bc7e_scalar_level, 0, 6);
				const bool perceptual = comp.get_params().m_perceptual;
				const uint32_t* pW = comp.get_params().m_ldr_channel_weights;

				debug_printf("DDS export: BC7 encoder=%s\n", using_bc7e ? "bc7e_scalar" : "bc7f");
				debug_printf("DDS export:   bc7f level=%d (%s), pack flags=0x%X%s\n",
					bc7f_lvl, pBc7fLvl, bc7ctx.m_bc7f_flags, using_bc7e ? " (inactive)" : " (active)");
				debug_printf("DDS export:   bc7e_scalar level=%d, perceptual=%u, weights=[%u %u %u %u]%s%s\n",
					bc7e_lvl, (uint32_t)perceptual, pW[0], pW[1], pW[2], pW[3],
					perceptual ? " (weights ignored in perceptual mode)" : "",
					using_bc7e ? " (active)" : " (inactive)");
			}
		}

		// Assemble the file into the caller's buffer: header then subresources in DDS order (layer, face, mip).
		build_dx10_header(dds_data, base_width, base_height, total_levels, total_layers, is_cubemap, dxgi_format, info);

		if (comp.get_params().m_status_output)
			printf("Writing DDS (format \"%s\", %ux%u): %u subresource(s) [%u level(s), %u layer(s), %u face(s)]\n",
				info.m_pToken, base_width, base_height, total_subresources, total_levels, total_layers, total_faces);

		for (uint32_t layer = 0; layer < total_layers; layer++)
		{
			for (uint32_t face = 0; face < total_faces; face++)
			{
				for (uint32_t level = 0; level < total_levels; level++)
				{
					const uint32_t map_index = (layer * total_faces + face) * total_levels + level;
					const int slice_index = slice_map[map_index];

					const basisu_backend_slice_desc& desc = descs[slice_index];

					if (comp.get_params().m_status_output)
						printf("DDS: packing subresource %u/%u (layer %u, face %u, level %u): %ux%u\n",
							map_index + 1, total_subresources, layer, face, level, desc.m_orig_width, desc.m_orig_height);

					const size_t before_size = dds_data.size();
					pack_slice(dds_data, slices[slice_index], desc.m_orig_width, desc.m_orig_height, fmt, bc7ctx);

					if (debug)
						debug_printf("  subresource [layer %u, face %u, level %u]: %ux%u -> %u byte(s)\n",
							layer, face, level, desc.m_orig_width, desc.m_orig_height, (uint32_t)(dds_data.size() - before_size));
				}
			}
		}

		if (pOut_width) *pOut_width = base_width;
		if (pOut_height) *pOut_height = base_height;
		if (pOut_levels) *pOut_levels = total_levels;
		if (pOut_layers) *pOut_layers = total_layers;
		if (pOut_faces) *pOut_faces = total_faces;

		return true;
	}

} // namespace basisu
