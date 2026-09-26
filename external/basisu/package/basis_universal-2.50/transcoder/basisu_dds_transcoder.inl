// basisu_dds_transcoder.inl
//
// Included at the END of basisu_transcoder.cpp (after basisu_xbc7_decoder.inl), OUTSIDE the file's
// `namespace basist {` block -- this .inl reopens namespace basist itself.
//
// Two things live here, both in namespace basist:
//  1. Plain, vendor-neutral BC1/BC3/BC4/BC5 (DXT1/DXT5/BC4/BC5) block UNPACKERS in namespace bcu
//     ("bcu" == BC Unpack), moved here from the encoder's reference unpackers in
//     encoder/basisu_gpu_texture.cpp (NOT the NV/AMD-specific variants, which stay encoder-only).
//     color_rgba here resolves to basist::color_rgba.
//  2. The DDS reader/transcoder (basist::dds_transcoder), further below.
// Both let the transcoder decode these formats with zero dependency on the encoder library.

namespace basist
{
namespace bcu
{
	//--------------------------------------------------------------------------------------------------
	// BC1
	
	// Decodes a BC1-layout color block; returns true if it used 3-color punchthrough mode. force_4color=true forces
	// 4-color mode (the color0<=color1 punchthrough switch is BC1-only; BC2/BC3 ignore it, per the D3D/S3TC specs).
	bool unpack_bc1(const void* pBlock_bits, color_rgba* pPixels, bool set_alpha, bool force_4color)
	{
		static_assert(sizeof(bc1_block) == 8, "sizeof(bc1_block) == 8");

		const bc1_block* pBlock = static_cast<const bc1_block*>(pBlock_bits);

		const uint32_t l = pBlock->get_low_color();
		const uint32_t h = pBlock->get_high_color();

		color_rgba c[4];

		uint32_t r0, g0, b0, r1, g1, b1;
		bc1_block::unpack_color(l, r0, g0, b0);
		bc1_block::unpack_color(h, r1, g1, b1);

		c[0].set_noclamp_rgba(r0, g0, b0, 255);
		c[1].set_noclamp_rgba(r1, g1, b1, 255);

		bool used_punchthrough = false;

		if ((l > h) || force_4color)   // BC2/BC3 force 4-color mode; BC1 uses it only when color0 > color1
		{
			c[2].set_noclamp_rgba((r0 * 2 + r1) / 3, (g0 * 2 + g1) / 3, (b0 * 2 + b1) / 3, 255);
			c[3].set_noclamp_rgba((r1 * 2 + r0) / 3, (g1 * 2 + g0) / 3, (b1 * 2 + b0) / 3, 255);
		}
		else
		{
			c[2].set_noclamp_rgba((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
			c[3].set_noclamp_rgba(0, 0, 0, 0);
			used_punchthrough = true;
		}

		if (set_alpha)
		{
			for (uint32_t y = 0; y < 4; y++, pPixels += 4)
			{
				pPixels[0] = c[pBlock->get_selector(0, y)];
				pPixels[1] = c[pBlock->get_selector(1, y)];
				pPixels[2] = c[pBlock->get_selector(2, y)];
				pPixels[3] = c[pBlock->get_selector(3, y)];
			}
		}
		else
		{
			for (uint32_t y = 0; y < 4; y++, pPixels += 4)
			{
				pPixels[0].set_rgb(c[pBlock->get_selector(0, y)]);
				pPixels[1].set_rgb(c[pBlock->get_selector(1, y)]);
				pPixels[2].set_rgb(c[pBlock->get_selector(2, y)]);
				pPixels[3].set_rgb(c[pBlock->get_selector(3, y)]);
			}
		}

		return used_punchthrough;
	}

	//--------------------------------------------------------------------------------------------------
	// BC3-5

	struct bc4_block
	{
		enum { cBC4SelectorBits = 3, cTotalSelectorBytes = 6, cMaxSelectorValues = 8 };
		uint8_t m_endpoints[2];

		uint8_t m_selectors[cTotalSelectorBytes];

		inline uint32_t get_low_alpha() const { return m_endpoints[0]; }
		inline uint32_t get_high_alpha() const { return m_endpoints[1]; }
		inline bool is_alpha6_block() const { return get_low_alpha() <= get_high_alpha(); }

		inline uint64_t get_selector_bits() const
		{
			return ((uint64_t)((uint32_t)m_selectors[0] | ((uint32_t)m_selectors[1] << 8U) | ((uint32_t)m_selectors[2] << 16U) | ((uint32_t)m_selectors[3] << 24U))) |
				(((uint64_t)m_selectors[4]) << 32U) |
				(((uint64_t)m_selectors[5]) << 40U);
		}

		inline uint32_t get_selector(uint32_t x, uint32_t y, uint64_t selector_bits) const
		{
			assert((x < 4U) && (y < 4U));
			return (selector_bits >> (((y * 4) + x) * cBC4SelectorBits)) & (cMaxSelectorValues - 1);
		}

		static inline uint32_t get_block_values6(uint8_t* pDst, uint32_t l, uint32_t h)
		{
			pDst[0] = static_cast<uint8_t>(l);
			pDst[1] = static_cast<uint8_t>(h);
			pDst[2] = static_cast<uint8_t>((l * 4 + h) / 5);
			pDst[3] = static_cast<uint8_t>((l * 3 + h * 2) / 5);
			pDst[4] = static_cast<uint8_t>((l * 2 + h * 3) / 5);
			pDst[5] = static_cast<uint8_t>((l + h * 4) / 5);
			pDst[6] = 0;
			pDst[7] = 255;
			return 6;
		}

		static inline uint32_t get_block_values8(uint8_t* pDst, uint32_t l, uint32_t h)
		{
			pDst[0] = static_cast<uint8_t>(l);
			pDst[1] = static_cast<uint8_t>(h);
			pDst[2] = static_cast<uint8_t>((l * 6 + h) / 7);
			pDst[3] = static_cast<uint8_t>((l * 5 + h * 2) / 7);
			pDst[4] = static_cast<uint8_t>((l * 4 + h * 3) / 7);
			pDst[5] = static_cast<uint8_t>((l * 3 + h * 4) / 7);
			pDst[6] = static_cast<uint8_t>((l * 2 + h * 5) / 7);
			pDst[7] = static_cast<uint8_t>((l + h * 6) / 7);
			return 8;
		}

		static inline uint32_t get_block_values(uint8_t* pDst, uint32_t l, uint32_t h)
		{
			if (l > h)
				return get_block_values8(pDst, l, h);
			else
				return get_block_values6(pDst, l, h);
		}
	};

	// Writes a single channel per pixel (the caller selects which via pPixels/stride).
	void unpack_bc4(const void* pBlock_bits, uint8_t* pPixels, uint32_t stride)
	{
		static_assert(sizeof(bc4_block) == 8, "sizeof(bc4_block) == 8");

		const bc4_block* pBlock = static_cast<const bc4_block*>(pBlock_bits);

		uint8_t sel_values[8];
		bc4_block::get_block_values(sel_values, pBlock->get_low_alpha(), pBlock->get_high_alpha());

		const uint64_t selector_bits = pBlock->get_selector_bits();

		for (uint32_t y = 0; y < 4; y++, pPixels += (stride * 4U))
		{
			pPixels[0] = sel_values[pBlock->get_selector(0, y, selector_bits)];
			pPixels[stride * 1] = sel_values[pBlock->get_selector(1, y, selector_bits)];
			pPixels[stride * 2] = sel_values[pBlock->get_selector(2, y, selector_bits)];
			pPixels[stride * 3] = sel_values[pBlock->get_selector(3, y, selector_bits)];
		}
	}

	// BC3 = BC4 alpha block + BC1-layout color block (always 4-color, see unpack_bc1's force_4color). Alpha comes
	// solely from the BC4 block.
	void unpack_bc3(const void* pBlock_bits, color_rgba* pPixels)
	{
		unpack_bc1((const uint8_t*)pBlock_bits + sizeof(bc4_block), pPixels, true, /*force_4color*/ true);
		unpack_bc4(pBlock_bits, &pPixels[0].a, sizeof(color_rgba));
	}

	// BC2 (DXT2/DXT3) = 8 bytes of EXPLICIT 4-bit-per-texel alpha, then a BC1-layout color block decoded ALWAYS in
	// 4-color mode (same color path as BC3). 4-bit alpha a expands to 8-bit as a*17 == (a<<4)|a. Decode alpha first,
	// then the color with set_alpha=false so the color path doesn't clobber it.
	void unpack_bc2(const void* pBlock_bits, color_rgba* pPixels)
	{
		const uint8_t* pBlock = static_cast<const uint8_t*>(pBlock_bits);
		// Bytes 0-7: one little-endian 16-bit word per row (y); texel x is nibble (word >> 4*x) & 0xF.
		for (uint32_t y = 0; y < 4; y++)
		{
			const uint32_t row = (uint32_t)pBlock[y * 2] | ((uint32_t)pBlock[y * 2 + 1] << 8);
			for (uint32_t x = 0; x < 4; x++)
			{
				const uint32_t a4 = (row >> (x * 4)) & 0xFu;
				pPixels[y * 4 + x].a = (uint8_t)(a4 * 17u); // 4-bit -> 8-bit replication
			}
		}
		// Bytes 8-15: BC1-layout color, forced 4-color, set_alpha=false to preserve the explicit alpha above.
		unpack_bc1(pBlock + 8, pPixels, /*set_alpha*/ false, /*force_4color*/ true);
	}


	// Writes only the R and G channels per pixel.
	void unpack_bc5(const void* pBlock_bits, color_rgba* pPixels)
	{
		unpack_bc4(pBlock_bits, &pPixels[0].r, sizeof(color_rgba));
		unpack_bc4((const uint8_t*)pBlock_bits + sizeof(bc4_block), &pPixels[0].g, sizeof(color_rgba));
	}

} // namespace bcu
} // namespace basist


// ==============================================================================================
// DDS transcoder (basist::dds_transcoder) -- moved here from the former CLI basisu_dds_transcoder.cpp.
// ==============================================================================================

namespace basist
{
	namespace
	{
		// ---- DDS on-disk constants ----
		const uint32_t DDS_MAGIC = 0x20534444; // "DDS "
		const uint32_t DDS_HEADER_SIZE = 124;
		const uint32_t DDS_PIXELFORMAT_SIZE = 32;
		const uint32_t DDS_DX10_HEADER_SIZE = 20;

		// DDS_HEADER dwFlags (only DDSD_PITCH is acted on; others are advisory per the MS guide)
		const uint32_t DDSD_PITCH = 0x8;

		// DDS_PIXELFORMAT dwFlags
		const uint32_t DDPF_ALPHAPIXELS = 0x1;
		const uint32_t DDPF_ALPHA = 0x2;	// alpha-only (D3DFMT_A8)
		const uint32_t DDPF_FOURCC = 0x4;
		const uint32_t DDPF_RGB = 0x40;
		const uint32_t DDPF_LUMINANCE = 0x20000;

		// dwCaps2
		const uint32_t DDSCAPS2_CUBEMAP = 0x200;
		const uint32_t DDSCAPS2_CUBEMAP_ALLFACES = 0xFC00; // all six POSITIVEX..NEGATIVEZ bits
		const uint32_t DDSCAPS2_VOLUME = 0x200000;

		// DX10 D3D10_RESOURCE_DIMENSION
		const uint32_t DDS_DIMENSION_TEXTURE1D = 2;
		const uint32_t DDS_DIMENSION_TEXTURE2D = 3;
		const uint32_t DDS_DIMENSION_TEXTURE3D = 4;
		// DX10 miscFlag
		const uint32_t DDS_RESOURCE_MISC_TEXTURECUBE = 0x4;

		inline uint32_t make_fourcc(char a, char b, char c, char d)
		{
			return (uint32_t)(uint8_t)a | ((uint32_t)(uint8_t)b << 8) | ((uint32_t)(uint8_t)c << 16) | ((uint32_t)(uint8_t)d << 24);
		}

		// DXGI_FORMAT subset we recognize
		enum
		{
			DXGI_R8G8_UNORM = 49,
			DXGI_R8_UNORM = 61,
			DXGI_A8_UNORM = 65,
			DXGI_R8G8B8A8_UNORM = 28,
			DXGI_R8G8B8A8_UNORM_SRGB = 29,
			DXGI_BC1_UNORM = 71,
			DXGI_BC1_UNORM_SRGB = 72,
			DXGI_BC2_UNORM = 74,
			DXGI_BC2_UNORM_SRGB = 75,
			DXGI_BC3_UNORM = 77,
			DXGI_BC3_UNORM_SRGB = 78,
			DXGI_BC4_UNORM = 80,
			DXGI_BC4_SNORM = 81,
			DXGI_BC5_UNORM = 83,
			DXGI_BC5_SNORM = 84,
			DXGI_B5G6R5_UNORM = 85,
			DXGI_B5G5R5A1_UNORM = 86,
			DXGI_B8G8R8A8_UNORM = 87,
			DXGI_B8G8R8X8_UNORM = 88,
			DXGI_B8G8R8A8_UNORM_SRGB = 91,
			DXGI_B8G8R8X8_UNORM_SRGB = 93,
			DXGI_BC7_UNORM = 98,
			DXGI_BC7_UNORM_SRGB = 99,
			DXGI_B4G4R4A4_UNORM = 115
		};

		inline uint32_t rd_u32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
		inline uint32_t rd_u24(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16); }
		inline uint32_t rd_u16(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }

		inline uint32_t pop_count(uint32_t v) { uint32_t n = 0; while (v) { n += (v & 1); v >>= 1; } return n; }
		inline uint32_t trail_zeros(uint32_t v) { if (!v) return 0; uint32_t n = 0; while (!(v & 1)) { v >>= 1; n++; } return n; }

		// Extract the masked field and expand to 8 bits by bit replication (e.g. 5-bit: (v<<3)|(v>>2)) -- the
		// GPU/DirectXTex UNORM convention (0->0, max->255, monotonic). Absent channel (m_channel_bits==0) => 255.
		// shift/bit-width are precomputed in init() (see dds_uncompressed_channel).
		inline uint32_t decode_channel(uint32_t raw, const dds_uncompressed_channel& chan)
		{
			if (!chan.m_channel_bits)
				return 255;
			const uint32_t channel_value = (raw & chan.m_mask) >> chan.m_shift;
			if (chan.m_channel_bits >= 8)
				return (channel_value >> (chan.m_channel_bits - 8)) & 0xFFu;
			// Replicate the m_channel_bits-wide pattern across the 8-bit output, MSB-first.
			uint32_t result = 0;
			for (int shift = 8 - (int)chan.m_channel_bits; shift > -(int)chan.m_channel_bits; shift -= (int)chan.m_channel_bits)
				result |= (shift >= 0) ? (channel_value << shift) : (channel_value >> (-shift));
			return result & 0xFFu;
		}

		// Byte offset of a clean 8-bit channel mask -- i.e. 0xFF positioned on a byte boundary, so the mask is
		// one of 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000. Returns -1 if the mask isn't byte-aligned (so
		// the caller must fall back to the generic decode_channel path). Used once per image (see init()) to
		// precompute the uncompressed fast-path channel offsets.
		inline int chan_byte_offset(uint32_t mask)
		{
			if (!mask) return -1;
			const uint32_t shift = trail_zeros(mask);
			if ((shift & 7u) || (mask != (0xFFu << shift))) return -1;
			return (int)(shift >> 3);
		}

		// Accepted DX9 DDPF_RGB uncompressed mask layouts. We decode any of these with the generic mask
		// decoder, but admit ONLY this known-good list (anything else -> reject) so exotic/garbage masks
		// can't slip through. Masks are the standard D3DFMT layouts; the effective alpha mask is 0 unless
		// DDPF_ALPHAPIXELS is set (handled by the caller). Every entry is decodable by code already tested.
		struct rgb_mask_layout { uint32_t m_bits, m_r, m_g, m_b, m_a; };
		const rgb_mask_layout k_accepted_rgb_layouts[] =
		{
			// 16-bit
			{ 16, 0xF800u,     0x07E0u,     0x001Fu,     0x0000u     }, // R5G6B5
			{ 16, 0x7C00u,     0x03E0u,     0x001Fu,     0x8000u     }, // A1R5G5B5
			{ 16, 0x7C00u,     0x03E0u,     0x001Fu,     0x0000u     }, // X1R5G5B5
			{ 16, 0x0F00u,     0x00F0u,     0x000Fu,     0xF000u     }, // A4R4G4B4
			{ 16, 0x0F00u,     0x00F0u,     0x000Fu,     0x0000u     }, // X4R4G4B4
			// 24-bit
			{ 24, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0x00000000u }, // R8G8B8  (B,G,R in memory)
			{ 24, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0x00000000u }, // B8G8R8  (R,G,B in memory)
			// 32-bit
			{ 32, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u }, // A8R8G8B8 (BGRA in memory)
			{ 32, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0x00000000u }, // X8R8G8B8 (BGRX in memory)
			{ 32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u }, // A8B8G8R8 (RGBA in memory)
			{ 32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0x00000000u }, // X8B8G8R8 (RGBX in memory)
		};

		bool is_accepted_rgb_layout(uint32_t bits, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
		{
			for (const rgb_mask_layout& L : k_accepted_rgb_layouts)
				if ((L.m_bits == bits) && (L.m_r == r) && (L.m_g == g) && (L.m_b == b) && (L.m_a == a))
					return true;
			return false;
		}

		// Map the detected source kind + (for uncompressed) the committed bit count and channel masks to the
		// exact physical dds_format. DX9 and DX10 sources converge to the same masks, so this is source-agnostic.
		// KEEP IN SYNC with k_accepted_rgb_layouts above: every uncompressed layout init() accepts must map to a
		// non-cInvalid value here (otherwise get_dds_format() silently reports cInvalid on a successfully-parsed file).
		dds_format classify_dds_format(dds_transcoder::source_kind kind, uint32_t bits, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
		{
			typedef dds_transcoder::source_kind sk;
			switch (kind)
			{
			case sk::cBC1: return dds_format::cBC1;
			case sk::cBC2: return dds_format::cBC2;
			case sk::cBC3: return dds_format::cBC3;
			case sk::cBC4: return dds_format::cBC4;
			case sk::cBC5: return dds_format::cBC5;
			case sk::cBC7: return dds_format::cBC7;
			case sk::cUncompressed: break;
			default: return dds_format::cInvalid;
			}
			// Uncompressed: identify by bit count + channel masks (mirrors k_accepted_rgb_layouts).
			if (bits == 16)
			{
				if ((r == 0xF800u) && (g == 0x07E0u) && (b == 0x001Fu)) return dds_format::cR5G6B5;
				if ((r == 0x7C00u) && (g == 0x03E0u) && (b == 0x001Fu)) return a ? dds_format::cA1R5G5B5 : dds_format::cX1R5G5B5;
				if ((r == 0x0F00u) && (g == 0x00F0u) && (b == 0x000Fu)) return a ? dds_format::cA4R4G4B4 : dds_format::cX4R4G4B4;
			}
			else if (bits == 24)
			{
				if (r == 0x00FF0000u) return dds_format::cR8G8B8;
				if (r == 0x000000FFu) return dds_format::cB8G8R8;
			}
			else if (bits == 32)
			{
				if (r == 0x00FF0000u) return a ? dds_format::cA8R8G8B8 : dds_format::cX8R8G8B8;
				if (r == 0x000000FFu) return a ? dds_format::cA8B8G8R8 : dds_format::cX8B8G8R8;
			}
			return dds_format::cInvalid;
		}

		// How a target format is produced from decoded source texels.
		enum class repack_mode
		{
			cBlock4x4,	// per-4x4 block via transcode_4x4_block (BC1/3/4/5/7, ETC1/ETC2/EAC, 32/16bpp uncompressed)
			cASTC4x4,	// per-4x4 block via bc7f::fast_pack_astc (ASTC LDR 4x4)
			cPVRTC1		// whole-image via encode_pvrtc1 (PVRTC1 4bpp, power-of-2 only)
		};

		// Target description for a transcode destination format.
		struct target_info
		{
			block_format m_bf;
			uint32_t m_out_bytes;	// bytes per output block (compressed/astc/pvrtc) or per pixel (uncompressed)
			bool m_uncompressed;
			repack_mode m_mode;
			int m_def_channel0, m_def_channel1;
		};

		bool get_target_info(transcoder_texture_format fmt, target_info& ti)
		{
			ti.m_bf = block_format::cBC1; ti.m_out_bytes = 16; ti.m_uncompressed = false; ti.m_mode = repack_mode::cBlock4x4; ti.m_def_channel0 = -1; ti.m_def_channel1 = -1;
			switch (fmt)
			{
			case transcoder_texture_format::cTFETC1_RGB:		ti.m_bf = block_format::cETC1; ti.m_out_bytes = 8; break;
			case transcoder_texture_format::cTFETC2_RGBA:		ti.m_bf = block_format::cETC2_RGBA; ti.m_out_bytes = 16; break;
			case transcoder_texture_format::cTFETC2_EAC_R11:	ti.m_bf = block_format::cETC2_EAC_R11; ti.m_out_bytes = 8; ti.m_def_channel0 = 0; break;
			case transcoder_texture_format::cTFETC2_EAC_RG11:	ti.m_bf = block_format::cETC2_EAC_RG11; ti.m_out_bytes = 16; ti.m_def_channel0 = 0; ti.m_def_channel1 = 1; break;
			case transcoder_texture_format::cTFBC1_RGB:			ti.m_bf = block_format::cBC1; ti.m_out_bytes = 8; break;
			case transcoder_texture_format::cTFBC3_RGBA:		ti.m_bf = block_format::cBC3; ti.m_out_bytes = 16; break;
			case transcoder_texture_format::cTFBC4_R:			ti.m_bf = block_format::cBC4; ti.m_out_bytes = 8; ti.m_def_channel0 = 0; break;
			case transcoder_texture_format::cTFBC5_RG:			ti.m_bf = block_format::cBC5; ti.m_out_bytes = 16; ti.m_def_channel0 = 0; ti.m_def_channel1 = 1; break;
			case transcoder_texture_format::cTFBC7_RGBA:		ti.m_bf = block_format::cBC7; ti.m_out_bytes = 16; break;
			case transcoder_texture_format::cTFASTC_4x4_RGBA:	ti.m_bf = block_format::cASTC_LDR_4x4; ti.m_out_bytes = 16; ti.m_mode = repack_mode::cASTC4x4; break;
			case transcoder_texture_format::cTFPVRTC1_4_RGB:	ti.m_bf = block_format::cPVRTC1_4_RGB; ti.m_out_bytes = 8; ti.m_mode = repack_mode::cPVRTC1; break;
			case transcoder_texture_format::cTFPVRTC1_4_RGBA:	ti.m_bf = block_format::cPVRTC1_4_RGBA; ti.m_out_bytes = 8; ti.m_mode = repack_mode::cPVRTC1; break;
			case transcoder_texture_format::cTFRGBA32:			ti.m_bf = block_format::cRGBA32; ti.m_out_bytes = 4; ti.m_uncompressed = true; break;
			case transcoder_texture_format::cTFRGB565:			ti.m_bf = block_format::cRGB565; ti.m_out_bytes = 2; ti.m_uncompressed = true; break;
			case transcoder_texture_format::cTFRGBA4444:		ti.m_bf = block_format::cRGBA4444; ti.m_out_bytes = 2; ti.m_uncompressed = true; break;
			default:
				return false;
			}
			return true;
		}

	} // anonymous namespace

	dds_transcoder::dds_transcoder()
	{
		clear();
	}

	void dds_transcoder::clear()
	{
		m_pData = nullptr;
		m_data_size = 0;
		m_init_succeeded = false;
		m_width = m_height = 0;
		m_levels = 0;
		m_layers = 0;
		m_faces = 1;
		m_has_alpha = false;
		m_is_srgb = false;
		m_format = transcoder_texture_format::cTFRGBA32;
		m_block_width = m_block_height = 1;
		m_bytes_per_block_or_pixel = 0;
		m_src_kind = source_kind::cInvalid;
		m_dds_format = dds_format::cInvalid;
		m_rgb_bit_count = 0;
		for (uint32_t channel_index = 0; channel_index < 4; channel_index++)
			m_uncomp_channels[channel_index] = dds_uncompressed_channel{ 0, 0, 0, -2 }; // 0 mask => absent => byte_offset -2
		m_uncompressed_is_canonical_rgba8 = false;
		m_uncompressed_byte_aligned = false;
		for (uint32_t c = 0; c < 4; c++) { m_swizzle[c] = 0; m_and_mask[c] = 0; m_or_mask[c] = 0; }
		m_slices.clear();
	}

	bool dds_transcoder::init(const void* pData, uint32_t data_size)
	{
		clear();

		if (!pData)
			return false;

		const uint8_t* p = static_cast<const uint8_t*>(pData);

		// Need magic (4) + DDS_HEADER (124) at minimum.
		if (data_size < 4 + DDS_HEADER_SIZE)
			return false;

		if (rd_u32(p) != DDS_MAGIC)
			return false;

		const uint8_t* h = p + 4; // DDS_HEADER

		const uint32_t dwSize = rd_u32(h + 0);
		if (dwSize != DDS_HEADER_SIZE)
			return false;

		const uint32_t dw_flags = rd_u32(h + 4);
		const uint32_t height = rd_u32(h + 8);
		const uint32_t width = rd_u32(h + 12);
		const uint32_t pitch_or_linear_size = rd_u32(h + 16);
		const uint32_t depth = rd_u32(h + 20);
		const uint32_t mip_count_field = rd_u32(h + 24);

		// DDS_PIXELFORMAT at offset 72
		const uint8_t* pf = h + 72;
		const uint32_t pf_size = rd_u32(pf + 0);
		if (pf_size != DDS_PIXELFORMAT_SIZE)
			return false;
		const uint32_t pf_flags = rd_u32(pf + 4);
		const uint32_t pf_fourcc = rd_u32(pf + 8);
		const uint32_t pf_rgb_bit_count = rd_u32(pf + 12);
		const uint32_t pf_r_mask = rd_u32(pf + 16);
		const uint32_t pf_g_mask = rd_u32(pf + 20);
		const uint32_t pf_b_mask = rd_u32(pf + 24);
		const uint32_t pf_a_mask = rd_u32(pf + 28);

		const uint32_t caps2 = rd_u32(h + 108);

		// Basic dimension sanity (parser hardening).
		const uint32_t MAX_DIM = 32768;
		if ((width == 0) || (height == 0) || (width > MAX_DIM) || (height > MAX_DIM))
			return false;

		// Reject volume textures.
		if ((caps2 & DDSCAPS2_VOLUME) || (depth > 1))
			return false;

		// Is this a DX10-extended file?
		const bool is_dx10 = (pf_flags & DDPF_FOURCC) && (pf_fourcc == make_fourcc('D', 'X', '1', '0'));

		uint32_t data_start = 4 + DDS_HEADER_SIZE;
		uint32_t faces = 1;
		uint32_t eff_layers = 1;	// physical array element count (>=1)
		bool reported_array = false;

		// Format detection outputs.
		source_kind kind = source_kind::cInvalid;
		transcoder_texture_format contained = transcoder_texture_format::cTFRGBA32;
		// --- sRGB flag policy (m_is_srgb), best-effort: DDS only signals sRGB on DX10/DXGI (the _UNORM vs
		// _UNORM_SRGB variants); DX9 carries no signal. ---
		//   * is_srgb = false (known linear): DX10 formats stored as the _UNORM variant (we honor the explicit
		//     signal, set below via "is_srgb = (dxgi_format == ..._SRGB)"), and BC4/BC5 (single/two-channel data
		//     -- no sRGB concept, forced linear in their cases below).
		//   * is_srgb = true (the default here): everything else -- all DX9 sources, and DX10 16-bit color with no
		//     _SRGB variant. sRGB is the safer guess for the common albedo case.
		// So the same BC format can report different is_srgb() per container (DX10 BC1_UNORM=false vs DX9 DXT1=true)
		// -- by design (DX10 can signal, DX9 can't). Caveat: real DX10 files often mis-tag sRGB content as _UNORM,
		// so the signal isn't fully reliable; consumers that care expose a manual override. is_srgb() never changes
		// decoded pixel values -- it's a reported hint only (callers may propagate it into output file metadata).
		bool has_alpha = false, is_srgb = true;
		uint32_t bytes_per = 0, blk_w = 4, blk_h = 4;
		uint32_t rgb_bits = 0, m_r = 0, m_g = 0, m_b = 0, m_a = 0;
		bool canonical_rgba8 = false;
		// Byte-swizzle decode tables (built below for byte-aligned layouts incl. R8/R8G8/A8/L8/A8L8). dds_fmt_override
		// names the exact format for the special byte formats since classify_dds_format() is mask-based and can't ID them.
		bool is_byte_swizzle = false;
		uint8_t swiz[4] = { 0, 0, 0, 0 }, andm[4] = { 0, 0, 0, 0 }, orm[4] = { 0, 0, 0, 0 };
		dds_format dds_fmt_override = dds_format::cInvalid;

		auto setup_uncompressed = [&](uint32_t bits, uint32_t rmask, uint32_t gmask, uint32_t bmask, uint32_t amask)
		{
			kind = source_kind::cUncompressed;
			rgb_bits = bits;
			m_r = rmask; m_g = gmask; m_b = bmask; m_a = amask;
			bytes_per = bits / 8;
			blk_w = 1; blk_h = 1;
			has_alpha = (amask != 0);
			// Choose a contained transcoder format that's the closest size/shape match.
			if (bits >= 24)
			{
				// 24-bit (R8G8B8 / B8G8R8) and 32-bit both decode to RGBA8. Only true memory-order R8G8B8A8
				// (32-bit) qualifies for a raw-memcpy passthrough; 24-bit always decode->repacks.
				contained = transcoder_texture_format::cTFRGBA32;
				canonical_rgba8 = (bits == 32) && (rmask == 0x000000FFu) && (gmask == 0x0000FF00u) && (bmask == 0x00FF0000u) && (amask == 0xFF000000u);
			}
			else
			{
				// 16-bit: 6-bit green => 565, else 4444 bucket (covers 1555/4444). Never a raw-memcpy passthrough.
				contained = (pop_count(gmask) == 6) ? transcoder_texture_format::cTFRGB565 : transcoder_texture_format::cTFRGBA4444;
				canonical_rgba8 = false;
			}
		};

		// set_byte_swizzle(): builds the byte-remap tables (swiz/andm/orm) from 4 channel source codes. Used by
		// setup_byte_aligned_format() (R8/R8G8/A8/L8/A8L8) and the channel-mask byte-aligned path further down.
		// Codes: >=0 = source byte index; cForce255 = opaque (also the m_byte_offset "absent alpha" -2 sentinel); cForce0 = zero.
		const int cForce255 = -2, cForce0 = -3;
		auto set_byte_swizzle = [&](int sr, int sg, int sb, int sa)
		{
			const int codes[4] = { sr, sg, sb, sa };
			for (uint32_t c = 0; c < 4; c++)
			{
				if (codes[c] >= 0)                { swiz[c] = (uint8_t)codes[c]; andm[c] = 0xFF; orm[c] = 0x00; }
				else if (codes[c] == cForce255)   { swiz[c] = 0; andm[c] = 0x00; orm[c] = 0xFF; }
				else                              { swiz[c] = 0; andm[c] = 0x00; orm[c] = 0x00; } // cForce0 / absent color
			}
			is_byte_swizzle = true;
		};

		// Byte-oriented single/dual-channel + luminance/alpha (R8/R8G8/A8/L8/A8L8): no channel masks; decode is the
		// byte remap set by set_byte_swizzle. All decode to RGBA8 so the contained transcoder format is cTFRGBA32.
		auto setup_byte_aligned_format = [&](uint32_t bytes, bool alpha, bool srgb, dds_format dfmt, int sr, int sg, int sb, int sa)
		{
			kind = source_kind::cUncompressed;
			rgb_bits = bytes * 8;
			bytes_per = bytes;
			blk_w = 1; blk_h = 1;
			has_alpha = alpha;
			is_srgb = srgb;
			contained = transcoder_texture_format::cTFRGBA32;
			canonical_rgba8 = false;
			dds_fmt_override = dfmt;
			set_byte_swizzle(sr, sg, sb, sa);
		};

		if (is_dx10)
		{
			if (data_size < data_start + DDS_DX10_HEADER_SIZE)
				return false;
			const uint8_t* dx10 = p + data_start;
			const uint32_t dxgi_format = rd_u32(dx10 + 0);
			const uint32_t resource_dim = rd_u32(dx10 + 4);
			const uint32_t misc_flag = rd_u32(dx10 + 8);
			const uint32_t array_size = rd_u32(dx10 + 12);
			data_start += DDS_DX10_HEADER_SIZE;

			if (resource_dim == DDS_DIMENSION_TEXTURE1D)
				return false;
			if (resource_dim == DDS_DIMENSION_TEXTURE3D)
				return false;
			if (resource_dim != DDS_DIMENSION_TEXTURE2D)
				return false;

			// Note: the DX10 spec says arraySize must be >= 1, but tinydds (and the basisu DDS writer that
			// uses it) store 0 for a plain non-array texture. Treat 0 as 1 for robustness.
			uint32_t array_size_eff = array_size ? array_size : 1;
			if (array_size_eff > 65536)
				return false;

			const bool is_cube = (misc_flag & DDS_RESOURCE_MISC_TEXTURECUBE) != 0;
			faces = is_cube ? 6 : 1;
			eff_layers = array_size_eff;				// DX10: array_size counts cubes (for cubemaps) or 2D slices
			reported_array = (array_size_eff > 1);

			switch (dxgi_format)
			{
			case DXGI_BC1_UNORM:
			case DXGI_BC1_UNORM_SRGB:
				// has_alpha = true: BC1 may carry per-block 1-bit punchthrough alpha (we decode it; DXGI BC1_UNORM is
				// a 4-component format). Reported conservatively without scanning blocks; opaque BC1 decodes alpha=255.
				kind = source_kind::cBC1; contained = transcoder_texture_format::cTFBC1_RGB; bytes_per = 8; has_alpha = true;
				is_srgb = (dxgi_format == DXGI_BC1_UNORM_SRGB); break;
			case DXGI_BC2_UNORM:
			case DXGI_BC2_UNORM_SRGB:
				// BC2 (explicit 4-bit alpha + BC1 color) is decode-only: no transcoder_texture_format exists for it, so
				// `contained` reports cTFBC3_RGBA as a closest-match hint (compatibility kludge). Passthrough is disabled
				// for cBC2 below (BC2 != BC3 bytewise) so it always decode->repacks; get_dds_format() reports the exact cBC2.
				kind = source_kind::cBC2; contained = transcoder_texture_format::cTFBC3_RGBA; bytes_per = 16; has_alpha = true;
				is_srgb = (dxgi_format == DXGI_BC2_UNORM_SRGB); break;
			case DXGI_BC3_UNORM:
			case DXGI_BC3_UNORM_SRGB:
				kind = source_kind::cBC3; contained = transcoder_texture_format::cTFBC3_RGBA; bytes_per = 16; has_alpha = true;
				is_srgb = (dxgi_format == DXGI_BC3_UNORM_SRGB); break;
			case DXGI_BC4_UNORM:
				kind = source_kind::cBC4; contained = transcoder_texture_format::cTFBC4_R; bytes_per = 8; has_alpha = false; is_srgb = false; break;	// single-channel data: always linear (override the sRGB default)
			case DXGI_BC5_UNORM:
				kind = source_kind::cBC5; contained = transcoder_texture_format::cTFBC5_RG; bytes_per = 16; has_alpha = false; is_srgb = false; break;	// two-channel data: always linear (override the sRGB default)
			case DXGI_BC7_UNORM:
			case DXGI_BC7_UNORM_SRGB:
				kind = source_kind::cBC7; contained = transcoder_texture_format::cTFBC7_RGBA; bytes_per = 16; has_alpha = true;
				is_srgb = (dxgi_format == DXGI_BC7_UNORM_SRGB); break;
			case DXGI_R8G8B8A8_UNORM:
			case DXGI_R8G8B8A8_UNORM_SRGB:
				setup_uncompressed(32, 0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u);
				is_srgb = (dxgi_format == DXGI_R8G8B8A8_UNORM_SRGB); break;
			case DXGI_B8G8R8A8_UNORM:
			case DXGI_B8G8R8A8_UNORM_SRGB:
				setup_uncompressed(32, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
				is_srgb = (dxgi_format == DXGI_B8G8R8A8_UNORM_SRGB); break;
			case DXGI_B8G8R8X8_UNORM:
			case DXGI_B8G8R8X8_UNORM_SRGB:
				setup_uncompressed(32, 0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0x00000000u);
				is_srgb = (dxgi_format == DXGI_B8G8R8X8_UNORM_SRGB); break;
			case DXGI_B5G6R5_UNORM:
				// DXGI names list channels LSB-first, so B5G6R5 has B in the low bits and R in the high bits:
				// R=0xF800, G=0x07E0, B=0x001F. (This converges with D3D9 R5G6B5, which lists MSB-first, and
				// matches tinydds's encode table -- so it round-trips correctly.)
				setup_uncompressed(16, 0xF800u, 0x07E0u, 0x001Fu, 0x0000u); break;
			case DXGI_B5G5R5A1_UNORM:
				setup_uncompressed(16, 0x7C00u, 0x03E0u, 0x001Fu, 0x8000u); break;
			case DXGI_B4G4R4A4_UNORM:
				setup_uncompressed(16, 0x0F00u, 0x00F0u, 0x000Fu, 0xF000u); break;
			case DXGI_R8_UNORM:
				setup_byte_aligned_format(1, false, false, dds_format::cR8, 0, cForce0, cForce0, cForce255); break;		// (R,0,0,255), linear (like BC4)
			case DXGI_R8G8_UNORM:
				setup_byte_aligned_format(2, false, false, dds_format::cR8G8, 0, 1, cForce0, cForce255); break;			// (R,G,0,255), linear (like BC5)
			case DXGI_A8_UNORM:
				setup_byte_aligned_format(1, true, false, dds_format::cA8, cForce0, cForce0, cForce0, 0); break;			// (0,0,0,A)
			default:
				// BC4_SNORM/BC5_SNORM/BC6H/float/etc. - unsupported.
				return false;
			}
		}
		else
		{
			// DX9 path.
			if (caps2 & DDSCAPS2_CUBEMAP)
			{
				// Require all six faces; reject partial cubemaps.
				if ((caps2 & DDSCAPS2_CUBEMAP_ALLFACES) != DDSCAPS2_CUBEMAP_ALLFACES)
					return false;
				faces = 6;
			}
			eff_layers = 1; // DX9 has no texture arrays
			reported_array = false;

			if (pf_flags & DDPF_FOURCC)
			{
				if (pf_fourcc == make_fourcc('D', 'X', 'T', '1'))
				{
					// has_alpha = true: DXT1/BC1 may carry per-block punchthrough alpha (see DX10 BC1 above).
					kind = source_kind::cBC1; contained = transcoder_texture_format::cTFBC1_RGB; bytes_per = 8; has_alpha = true;
				}
				else if ((pf_fourcc == make_fourcc('D', 'X', 'T', '5')) || (pf_fourcc == make_fourcc('D', 'X', 'T', '4')))
				{
					// DXT4 (premultiplied) and DXT5 share the BC3 byte layout (BC4 alpha + BC1 color); we decode the bytes
					// identically (premultiplication is a consumer semantic), mirroring DXT2/DXT3 -> BC2 above.
					kind = source_kind::cBC3; contained = transcoder_texture_format::cTFBC3_RGBA; bytes_per = 16; has_alpha = true;
				}
				else if ((pf_fourcc == make_fourcc('D', 'X', 'T', '3')) || (pf_fourcc == make_fourcc('D', 'X', 'T', '2')))
				{
					// DXT2 (premultiplied) and DXT3 share the BC2 byte layout (explicit 4-bit alpha + BC1 color). Decode-only;
					// `contained` reports cTFBC3 as a hint (see DX10 BC2). is_srgb stays at the DX9 default (true, no signal).
					kind = source_kind::cBC2; contained = transcoder_texture_format::cTFBC3_RGBA; bytes_per = 16; has_alpha = true;
				}
				else if ((pf_fourcc == make_fourcc('A', 'T', 'I', '1')) || (pf_fourcc == make_fourcc('B', 'C', '4', 'U')))
				{
					kind = source_kind::cBC4; contained = transcoder_texture_format::cTFBC4_R; bytes_per = 8; has_alpha = false; is_srgb = false; // single-channel data: always linear (override the sRGB default)
				}
				else if ((pf_fourcc == make_fourcc('A', 'T', 'I', '2')) || (pf_fourcc == make_fourcc('B', 'C', '5', 'U')))
				{
					kind = source_kind::cBC5; contained = transcoder_texture_format::cTFBC5_RG; bytes_per = 16; has_alpha = false; is_srgb = false; // two-channel data: always linear (override the sRGB default)
				}
				else
				{
					// A numeric D3DFMT code (float/16-bit-per-channel/etc.) - unsupported.
					return false;
				}
			}
			else if (pf_flags & DDPF_RGB)
			{
				// Accept only known-good mask layouts: 16-bit (565/1555/4444), 24-bit (R8G8B8 / B8G8R8),
				// 32-bit (A8R8G8B8 / X8R8G8B8 / A8B8G8R8 / X8B8G8R8). Reject anything else outright.
				const uint32_t eff_a = (pf_flags & DDPF_ALPHAPIXELS) ? pf_a_mask : 0;
				if (!is_accepted_rgb_layout(pf_rgb_bit_count, pf_r_mask, pf_g_mask, pf_b_mask, eff_a))
					return false;
				setup_uncompressed(pf_rgb_bit_count, pf_r_mask, pf_g_mask, pf_b_mask, eff_a);
			}
			else if (pf_flags & DDPF_LUMINANCE)
			{
				// D3DFMT_L8 (8bpp, L=0xFF) -> (L,L,L,255); D3DFMT_A8L8 (16bpp, L=0xFF, A=0xFF00) -> (L,L,L,A).
				const uint32_t eff_a = (pf_flags & DDPF_ALPHAPIXELS) ? pf_a_mask : 0;
				if ((pf_rgb_bit_count == 8) && (pf_r_mask == 0x000000FFu) && (eff_a == 0))
					setup_byte_aligned_format(1, false, true, dds_format::cL8, 0, 0, 0, cForce255);		// L8 -> (L,L,L,255)
				else if ((pf_rgb_bit_count == 16) && (pf_r_mask == 0x000000FFu) && (eff_a == 0x0000FF00u))
					setup_byte_aligned_format(2, true, true, dds_format::cA8L8, 0, 0, 0, 1);			// A8L8 -> (L,L,L,A)
				else
					return false;
			}
			else if (pf_flags & DDPF_ALPHA)
			{
				// D3DFMT_A8 (8bpp, alpha only) -> (0,0,0,A).
				if ((pf_rgb_bit_count == 8) && (pf_a_mask == 0x000000FFu))
					setup_byte_aligned_format(1, true, false, dds_format::cA8, cForce0, cForce0, cForce0, 0);	// A8 -> (0,0,0,A)
				else
					return false;
			}
			else
			{
				// DDPF_YUV / unknown - unsupported.
				return false;
			}
		}

		if (kind == source_kind::cInvalid)
			return false;

		const bool compressed = (kind != source_kind::cUncompressed);
		blk_w = compressed ? 4 : 1;
		blk_h = compressed ? 4 : 1;

		// Uncompressed row pitch: default tightly packed (matches DirectXTex's loader and the basisu writer).
		// If the header declares DDSD_PITCH with a DWORD-aligned pitch (legacy D3DX style), honor that row
		// padding for every level. Any other declared pitch is treated as unreliable and ignored (tight used).
		bool dword_align_rows = false;
		if (!compressed && (dw_flags & DDSD_PITCH))
		{
			const uint32_t tight0 = width * bytes_per;
			const uint32_t aligned0 = (tight0 + 3u) & ~3u;
			if ((pitch_or_linear_size == aligned0) && (aligned0 != tight0))
				dword_align_rows = true;
		}

		// Mip count. 0 means 1. Validate against the maximum possible chain (parser hardening).
		uint32_t max_dim = (width > height) ? width : height;
		uint32_t max_mips = 1;
		while (max_dim > 1) { max_dim >>= 1; max_mips++; }

		uint32_t levels = (mip_count_field == 0) ? 1 : mip_count_field;
		if (levels > max_mips)
			return false;

		// ---- Compute and validate every (layer, face, level) slice offset/size ----
		// Disk order: layer major, then face, then mip (matches the basisu DDS writer). Overflow/anti-DoS guard:
		// count images in 64-bit and reject up front if the file is too small for even one bytes_per per slice
		// (stops a tiny hostile header, e.g. arraySize=65536 faces=6, from forcing a huge allocation).
		const uint64_t total64 = (uint64_t)eff_layers * faces * levels;
		if (total64 > 0xFFFFFFFFu)
			return false; // defensive: keep the slice count in uint32 range regardless of bytes_per
		if (((uint64_t)data_start + total64 * bytes_per) > (uint64_t)data_size)
			return false;
		const uint32_t total = (uint32_t)total64; // safe: bounded by the checks above (data_size <= UINT32_MAX)

		basisu::vector<slice_desc> slices;
		if (!slices.try_resize(total))
			return false;

		uint64_t ofs = data_start;
		for (uint32_t layer = 0; layer < eff_layers; layer++)
		{
			for (uint32_t face = 0; face < faces; face++)
			{
				for (uint32_t level = 0; level < levels; level++)
				{
					const uint32_t mw = (width >> level) ? (width >> level) : 1;
					const uint32_t mh = (height >> level) ? (height >> level) : 1;
					const uint32_t nbx = (mw + blk_w - 1) / blk_w;
					const uint32_t nby = (mh + blk_h - 1) / blk_h;

					uint64_t size;
					uint32_t row_pitch = 0;
					if (compressed)
						// DDS stores exact texel dims, which for BC may be non-multiples of 4 (a 3x3 BC7 = 3x3 texels,
						// 1 block). Storage is always whole 4x4 blocks: block count = ceil(dim/4) (nbx/nby above). A
						// transcode to an uncompressed target clips back to the exact dims (see transcode_image_level).
						size = (uint64_t)nbx * nby * bytes_per;
					else
					{
						row_pitch = dword_align_rows ? (((mw * bytes_per) + 3u) & ~3u) : (mw * bytes_per);
						size = (uint64_t)row_pitch * mh; // honors DWORD row padding when the file declares it
					}

					// Every slice (incl. tiny tail mips, which still occupy a full 4x4 block) must be fully present.
					// This rejects truncated files AND ancient "bad-tail" DDS that stored sub-4x4 tails with less than
					// a full block (texconv's --bad-tails case): our full-block size won't fit, so we bail rather than
					// mis-read the tail. No --bad-tails tolerance.
					if ((ofs + size) > (uint64_t)data_size)
						return false;

					const uint32_t idx = (layer * faces + face) * levels + level;
					slice_desc& sd = slices[idx];
					sd.m_ofs = ofs;
					sd.m_size = (uint32_t)size;
					sd.m_width = mw;
					sd.m_height = mh;
					sd.m_num_blocks_x = (mw + 3) / 4; // 4x4 grid used by the transcode loop, regardless of source kind
					sd.m_num_blocks_y = (mh + 3) / 4;
					sd.m_row_pitch = row_pitch; // bytes per row for uncompressed; 0 for compressed (unused)

					ofs += size;
				}
			}
		}

		// Commit.
		m_pData = p;
		m_data_size = data_size;
		m_width = width;
		m_height = height;
		m_levels = levels;
		m_layers = reported_array ? eff_layers : 0;	// ktx2 convention: 0 == not an array
		m_faces = faces;
		m_has_alpha = has_alpha;
		m_is_srgb = is_srgb;
		m_format = contained;
		m_block_width = blk_w;
		m_block_height = blk_h;
		m_bytes_per_block_or_pixel = bytes_per;
		m_src_kind = kind;
		// Byte-oriented special formats (R8/R8G8/A8/L8/A8L8) name themselves via dds_fmt_override; classify_dds_format is mask-based.
		m_dds_format = (dds_fmt_override != dds_format::cInvalid) ? dds_fmt_override : classify_dds_format(kind, rgb_bits, m_r, m_g, m_b, m_a);
		m_rgb_bit_count = rgb_bits;
		// Precompute each channel's mask/shift/bit-width/byte-offset once (image-constant), so the per-pixel decode
		// doesn't recompute them. Order R,G,B,A. Compressed sources have 0 masks (never read; kind != cUncompressed).
		const uint32_t channel_masks[4] = { m_r, m_g, m_b, m_a };
		for (uint32_t channel_index = 0; channel_index < 4; channel_index++)
		{
			const uint32_t mask = channel_masks[channel_index];
			dds_uncompressed_channel& chan = m_uncomp_channels[channel_index];
			chan.m_mask = mask;
			chan.m_shift = trail_zeros(mask);
			chan.m_channel_bits = pop_count(mask);               // contiguous (whitelist-enforced); 0 => absent
			chan.m_byte_offset = mask ? chan_byte_offset(mask) : -2; // >=0 byte index; -1 not byte-aligned; -2 absent (opaque)
		}
		m_uncompressed_is_canonical_rgba8 = canonical_rgba8;

		// Build the byte-swizzle tables for channel-mask layouts (the R8/R8G8/A8/L8/A8L8 specials already set them).
		// A layout is byte-aligned iff 24/32-bit with every RGB channel a clean byte in range (alpha a byte or absent);
		// if so, derive the swizzle from the per-channel byte offsets. Sub-byte (565/1555/4444) -> generic mask path.
		if (!is_byte_swizzle)
		{
			const int bytes_per_pixel = (int)(rgb_bits / 8);
			bool aligned = (bytes_per_pixel == 3) || (bytes_per_pixel == 4);
			for (uint32_t c = 0; c < 3; c++) // R,G,B must each be a real byte in the pixel
				aligned = aligned && (m_uncomp_channels[c].m_byte_offset >= 0) && (m_uncomp_channels[c].m_byte_offset < bytes_per_pixel);
			const int a_offset = m_uncomp_channels[3].m_byte_offset; // >=0 byte, or -2 (== cForce255) when alpha absent
			aligned = aligned && ((a_offset == cForce255) || ((a_offset >= 0) && (a_offset < bytes_per_pixel)));
			if (aligned)
				set_byte_swizzle(m_uncomp_channels[0].m_byte_offset, m_uncomp_channels[1].m_byte_offset, m_uncomp_channels[2].m_byte_offset, a_offset);
		}
		m_uncompressed_byte_aligned = is_byte_swizzle;
		for (uint32_t c = 0; c < 4; c++) { m_swizzle[c] = swiz[c]; m_and_mask[c] = andm[c]; m_or_mask[c] = orm[c]; }

		m_slices.swap(slices);
		m_init_succeeded = true;
		return true;
	}

	bool dds_transcoder::start_transcoding()
	{
		return m_init_succeeded;
	}

	bool dds_transcoder::get_image_level_info(ktx2_image_level_info& level_info, uint32_t level_index, uint32_t layer_index, uint32_t face_index) const
	{
		if (!m_init_succeeded)
			return false;

		const uint32_t eff_layers = m_layers ? m_layers : 1;
		if ((level_index >= m_levels) || (layer_index >= eff_layers) || (face_index >= m_faces))
			return false;

		const slice_desc& sd = m_slices[slice_index(level_index, layer_index, face_index)];

		// Describe the CONTAINED format: BC* are 4x4 blocks; uncompressed is 1x1 (raster).
		const uint32_t bw = m_block_width, bh = m_block_height;
		const uint32_t nbx = (sd.m_width + bw - 1) / bw;
		const uint32_t nby = (sd.m_height + bh - 1) / bh;

		level_info.m_level_index = level_index;
		level_info.m_layer_index = layer_index;
		level_info.m_face_index = face_index;
		level_info.m_orig_width = sd.m_width;
		level_info.m_orig_height = sd.m_height;
		level_info.m_width = nbx * bw;
		level_info.m_height = nby * bh;
		level_info.m_num_blocks_x = nbx;
		level_info.m_num_blocks_y = nby;
		level_info.m_block_width = bw;
		level_info.m_block_height = bh;
		level_info.m_total_blocks = nbx * nby;
		level_info.m_alpha_flag = m_has_alpha;
		level_info.m_iframe_flag = false;
		return true;
	}

	// Advanced: expose a slice's stored byte range + geometry so callers can handle the raw data themselves.
	bool dds_transcoder::get_slice_desc(slice_desc& out, uint32_t level_index, uint32_t layer_index, uint32_t face_index) const
	{
		if (!m_init_succeeded)
			return false;
		const uint32_t eff_layers = m_layers ? m_layers : 1;
		if ((level_index >= m_levels) || (layer_index >= eff_layers) || (face_index >= m_faces))
			return false;
		out = m_slices[slice_index(level_index, layer_index, face_index)];
		return true;
	}

	bool dds_transcoder::is_transcode_format_supported(transcoder_texture_format fmt) const
	{
		// Takes the same transcoder_texture_format the transcoder uses. BC1-7, ETC1/ETC2/EAC, ASTC LDR 4x4,
		// and the uncompressed formats (RGBA32/565/4444) are always supported; PVRTC1 is supported only when
		// this texture's dimensions are powers of 2. Anything not in our target set is unsupported.
		target_info ti;
		if (!get_target_info(fmt, ti))
			return false;
		if (ti.m_mode == repack_mode::cPVRTC1)
			return basisu::is_pow2(m_width) && basisu::is_pow2(m_height);
		return true;
	}

#if BASISD_SUPPORT_XUASTC
	// Decodes source 4x4 block (bx,by) of a slice into 16 RGBA texels. A member (not a free helper) so the per-block
	// call site stays short. Only compiled when BASISD_SUPPORT_XUASTC is enabled (BC7 decode needs bc7u::unpack_bc7).
	void dds_transcoder::decode_source_block(
		const uint8_t* pSrc, uint32_t slice_w, uint32_t slice_h, uint32_t nbx, uint32_t row_pitch,
		uint32_t bx, uint32_t by, color32* pTexels) const
	{
		// The canonical-RGBA8 fast path memcpy's color32 rows straight from the source -- assumes color32 is a
		// packed 4-byte RGBA (this guards a future layout change).
		static_assert(sizeof(color32) == 4, "decode_source_block canonical memcpy assumes color32 is a packed 4-byte RGBA");

		const uint32_t bytes_per_texel = m_bytes_per_block_or_pixel;

		if (m_src_kind == source_kind::cUncompressed)
		{
			// ---- Byte-aligned uncompressed: a pure byte remap to color32. One swizzle loop covers RGBA/BGRA/RGBX,
			// 24-bit RGB/BGR, and R8/R8G8/A8/L8/A8L8: out = (src[swizzle] & and) | or (built in init()). Canonical
			// R8G8B8A8 gets a specialized per-row memcpy. Sub-byte (565/1555/4444) falls to the mask path below. ----
			if (m_uncompressed_byte_aligned)
			{
				const bool x_interior = ((bx * 4 + 4) <= slice_w);
				if (m_uncompressed_is_canonical_rgba8 && x_interior)
				{
					// Specialized hot path: canonical R8G8B8A8, fully-interior block -> one 16-byte memcpy per row
					// (compiles to a vector move). Bottom-edge blocks of a non-multiple-of-4 mip still clamp sy per row.
					for (uint32_t ty = 0; ty < 4; ty++)
					{
						uint32_t sy = by * 4 + ty;
						if (sy >= slice_h) sy = slice_h - 1;
						memcpy(&pTexels[ty * 4], pSrc + (uint64_t)sy * row_pitch + (uint64_t)bx * 4 * bytes_per_texel, 16);
					}
					return;
				}

				// General byte remap. The swizzle indices are in [0, bytes_per_texel) and forced channels read byte 0
				// (then get masked away), so every pp[] access is in range regardless of layout.
				const uint8_t s0 = m_swizzle[0], s1 = m_swizzle[1], s2 = m_swizzle[2], s3 = m_swizzle[3];
				const uint8_t a0 = m_and_mask[0], a1 = m_and_mask[1], a2 = m_and_mask[2], a3 = m_and_mask[3];
				const uint8_t o0 = m_or_mask[0], o1 = m_or_mask[1], o2 = m_or_mask[2], o3 = m_or_mask[3];
				for (uint32_t ty = 0; ty < 4; ty++)
				{
					uint32_t sy = by * 4 + ty;
					if (sy >= slice_h) sy = slice_h - 1; // clamp at bottom edge (mips not multiple of 4)
					const uint8_t* pRow = pSrc + (uint64_t)sy * row_pitch;
					for (uint32_t tx = 0; tx < 4; tx++)
					{
						uint32_t sx = bx * 4 + tx;
						if (sx >= slice_w) sx = slice_w - 1; // clamp at right edge (mips not multiple of 4)
						const uint8_t* pp = pRow + (uint64_t)sx * bytes_per_texel;

						color32& c = pTexels[ty * 4 + tx];
						c.r = (uint8_t)((pp[s0] & a0) | o0);
						c.g = (uint8_t)((pp[s1] & a1) | o1);
						c.b = (uint8_t)((pp[s2] & a2) | o2);
						c.a = (uint8_t)((pp[s3] & a3) | o3);
					}
				}
				return;
			}

			// ---- Generic path (slow): any non-byte-aligned uncompressed layout (16-bit 565/1555/4444, or any odd mask).
			// Expands sub-8-bit channels to 8 bits via bit replication inside decode_channel. ----
			for (uint32_t ty = 0; ty < 4; ty++)
			{
				uint32_t sy = by * 4 + ty;
				if (sy >= slice_h) sy = slice_h - 1; // clamp at edges (mips not multiple of 4)
				for (uint32_t tx = 0; tx < 4; tx++)
				{
					uint32_t sx = bx * 4 + tx;
					if (sx >= slice_w) sx = slice_w - 1;

					// Address via the row pitch (honors DWORD-padded rows), not width*bpp.
					const uint8_t* pp = pSrc + (uint64_t)sy * row_pitch + (uint64_t)sx * bytes_per_texel;
					const uint32_t raw = (bytes_per_texel == 4) ? rd_u32(pp) : ((bytes_per_texel == 3) ? rd_u24(pp) : rd_u16(pp));

					color32& c = pTexels[ty * 4 + tx];
					c.r = (uint8_t)decode_channel(raw, m_uncomp_channels[0]);
					c.g = (uint8_t)decode_channel(raw, m_uncomp_channels[1]);
					c.b = (uint8_t)decode_channel(raw, m_uncomp_channels[2]);
					c.a = (uint8_t)decode_channel(raw, m_uncomp_channels[3]);
				}
			}
			return;
		}

		// Compressed source: one physical block.
		const uint8_t* pBlock = pSrc + ((uint64_t)by * nbx + bx) * bytes_per_texel;

		switch (m_src_kind)
		{
		case dds_transcoder::source_kind::cBC1:
		{
			bcu::unpack_bc1(pBlock, reinterpret_cast<color_rgba*>(pTexels), true);
			break;
		}
		case dds_transcoder::source_kind::cBC2:
		{
			bcu::unpack_bc2(pBlock, reinterpret_cast<color_rgba*>(pTexels));
			break;
		}
		case dds_transcoder::source_kind::cBC3:
		{
			bcu::unpack_bc3(pBlock, reinterpret_cast<color_rgba*>(pTexels));
			break;
		}
		case dds_transcoder::source_kind::cBC4:
		{
			// BC4 decoder writes only one channel, so pre-fill RGBA with opaque black first.
			color32 black_color(0, 0, 0, 255);
			for (uint32_t i = 0; i < 16; i++)
				pTexels[i] = black_color;

			bcu::unpack_bc4(pBlock, (uint8_t *)pTexels, sizeof(color32));

			break;
		}
		case dds_transcoder::source_kind::cBC5:
		{
			// BC5 decoder writes only R and G, so pre-fill the other channels first.
			color32 black_color(0, 0, 0, 255);
			for (uint32_t i = 0; i < 16; i++)
				pTexels[i] = black_color;

			bcu::unpack_bc5(pBlock, reinterpret_cast<color_rgba*>(pTexels));

			break;
		}
		case dds_transcoder::source_kind::cBC7:
		{
			bc7u::unpack_bc7(pBlock, reinterpret_cast<color_rgba*>(pTexels));
			break;
		}
		default:
			assert(0);
			break;
		}
	}
#endif // BASISD_SUPPORT_XUASTC

	// ============================================================================================================
	// transcode_image_level: output_row_pitch_in_blocks_or_pixels / output_rows_in_pixels CONTRACT
	//
	// DDS has no "original" (pre-block-padding) dimensions, so the caller supplies them via output pitch/rows as a
	// proxy. Both default to 0 == the tight image size. By destination kind:
	//
	//  UNCOMPRESSED (cTFRGBA32/RGB565/RGBA4444, incl. BGRA/X8/16-bpp/byte swizzles): pitch = dest stride in PIXELS,
	//    output_rows = rows. The dest may be SMALLER than the image (we clip the blit to the top-left overlap, e.g.
	//    a 4x4 DDS -> 3x3 buffer) or LARGER (expanded pitch/rows). We decode ALL source blocks; boundary blocks
	//    partial-write, fully-outside blocks are skipped; row writes never exceed the image height. Canonical RGBA8
	//    -> cTFRGBA32 is a passthrough copy.
	//
	//  COMPRESSED (BC/ETC/EAC/ASTC4x4/PVRTC1): pitch = dest stride in BLOCKS (output_rows unused). Pitch may EXPAND
	//    beyond num_blocks_x but can't shrink (every block is emitted). PVRTC1 is whole-image (tight pitch only). A
	//    same-format passthrough (e.g. BC7->BC7) copies stored blocks verbatim, NEVER decoding to texels.
	//
	// In all cases a non-zero pitch/rows is obeyed and the output buffer is bounds-checked before any store.
	// ============================================================================================================
	bool dds_transcoder::transcode_image_level(
		uint32_t level_index, uint32_t layer_index, uint32_t face_index,
		void* pOutput_blocks, uint32_t output_blocks_buf_size_in_blocks_or_pixels,
		transcoder_texture_format fmt,
		uint32_t decode_flags,
		uint32_t output_row_pitch_in_blocks_or_pixels,
		uint32_t output_rows_in_pixels,
		int channel0, int channel1)
	{
		if (!m_init_succeeded || !pOutput_blocks)
			return false;

		const uint32_t eff_layers = m_layers ? m_layers : 1;
		if ((level_index >= m_levels) || (layer_index >= eff_layers) || (face_index >= m_faces))
			return false;

		target_info ti;
		if (!get_target_info(fmt, ti))
			return false;

		const slice_desc& sd = m_slices[slice_index(level_index, layer_index, face_index)];
		const uint8_t* pSrc = m_pData + sd.m_ofs;
		const uint32_t width = sd.m_width, height = sd.m_height;
		const uint32_t num_blocks_x = sd.m_num_blocks_x, num_blocks_y = sd.m_num_blocks_y;
		const bool src_compressed = (m_src_kind != source_kind::cUncompressed);

		// ---- Passthrough (memcpy) when the requested format is exactly what's stored and the byte
		// layout matches a transcoder output format. BGRA / X8 / 16-bpp never qualify (need swizzle/repack). ----
		bool passthrough = false;
		if (fmt == m_format)
		{
			// cBC2 reports cTFBC3 as a compatibility hint but is NOT byte-compatible with BC3 (explicit vs
			// interpolated alpha), so it must NEVER passthrough -- it always decode->repacks below.
			if (src_compressed && (m_src_kind != source_kind::cBC2))
				passthrough = true;
			else if ((fmt == transcoder_texture_format::cTFRGBA32) && m_uncompressed_is_canonical_rgba8)
				passthrough = true;
		}

		if (passthrough)
		{
			// No format change (see the contract above). Source is tightly packed. Compressed dest: copy every block,
			// pitch may expand but not shrink. Uncompressed canonical-RGBA8 dest: clip (smaller) or expand. Either
			// way, one whole-image memcpy when fully contiguous, else one memcpy per row/block-row at the dest pitch.
			const uint32_t unit_bytes          = m_bytes_per_block_or_pixel;              // per block (compressed) or per pixel (uncompressed)
			const uint32_t img_units_per_row   = src_compressed ? num_blocks_x : width;   // image blocks/row or pixels/row
			const uint32_t img_rows            = src_compressed ? num_blocks_y : height;  // image block-rows or pixel-rows
			const uint32_t src_row_pitch_bytes = src_compressed ? (num_blocks_x * unit_bytes) : sd.m_row_pitch;

			const uint32_t out_pitch_units = output_row_pitch_in_blocks_or_pixels ? output_row_pitch_in_blocks_or_pixels : img_units_per_row;
			const uint32_t out_rows        = src_compressed ? img_rows : (output_rows_in_pixels ? output_rows_in_pixels : height);

			uint32_t copy_units_per_row, copy_rows;
			if (src_compressed)
			{
				// Compressed: the destination pitch must hold every block (>= num_blocks_x) or larger; it can't shrink.
				if (out_pitch_units < img_units_per_row)
					return false;
				copy_units_per_row = img_units_per_row;
				copy_rows          = img_rows;
			}
			else
			{
				// Uncompressed: copy only the overlapping top-left region (clip if the destination is smaller).
				copy_units_per_row = basisu::minimum(img_units_per_row, out_pitch_units);
				copy_rows          = basisu::minimum(img_rows, out_rows);
			}

			// The destination buffer must hold its declared extent (out_pitch_units * out_rows units).
			if ((uint64_t)output_blocks_buf_size_in_blocks_or_pixels < (uint64_t)out_pitch_units * out_rows)
				return false;

			uint8_t* pDst = static_cast<uint8_t*>(pOutput_blocks);
			const size_t row_copy_bytes = (size_t)copy_units_per_row * unit_bytes;
			// Whole-image memcpy when source & destination are both tightly packed and fully copied; else per-scanline.
			if ((out_pitch_units == img_units_per_row) && (copy_units_per_row == img_units_per_row) &&
				(copy_rows == img_rows) && (src_row_pitch_bytes == (uint64_t)img_units_per_row * unit_bytes))
			{
				memcpy(pDst, pSrc, row_copy_bytes * copy_rows);
			}
			else
			{
				for (uint32_t r = 0; r < copy_rows; r++)
					memcpy(pDst + (uint64_t)r * out_pitch_units * unit_bytes, pSrc + (uint64_t)r * src_row_pitch_bytes, row_copy_bytes);
			}
			return true;
		}

#if BASISD_SUPPORT_XUASTC
		// ---- Decode -> repack ----
		const bool high_quality = (decode_flags & cDecodeFlagsHighQuality) != 0;
		const uint32_t bc7f_flags = high_quality ? bc7f::cPackBC7FlagDefaultPartiallyAnalytical : bc7f::cPackBC7FlagDefault;
		const bool from_alpha = m_has_alpha && ((decode_flags & cDecodeFlagsTranscodeAlphaDataToOpaqueFormats) != 0);
		// has_alpha hint for the block encoders: 1 = potentially has alpha, 0 = definitely opaque. BC1 reports
		// m_has_alpha == true (it may carry per-block punchthrough alpha), so it correctly lands on 1 here.
		const int has_alpha_arg = m_has_alpha ? 1 : 0;

		// Resolve effective channels: caller override (>=0) wins, else the target's sensible default.
		const int c0 = (channel0 >= 0) ? channel0 : ti.m_def_channel0;
		const int c1 = (channel1 >= 0) ? channel1 : ti.m_def_channel1;

		color32 texels[16];

		if (ti.m_mode == repack_mode::cPVRTC1)
		{
			// PVRTC1: whole-image. Requires power-of-2 dimensions.
			if (!basisu::is_pow2(width) || !basisu::is_pow2(height))
				return false;
			// encode_pvrtc1 writes blocks contiguously, so a non-tight output pitch can't be honored -- reject it.
			if (output_row_pitch_in_blocks_or_pixels && (output_row_pitch_in_blocks_or_pixels != num_blocks_x))
				return false;
			if (output_blocks_buf_size_in_blocks_or_pixels < (num_blocks_x * num_blocks_y))
				return false;

			basisu::vector2D<color32> temp_image;
			if (!temp_image.try_resize(num_blocks_x * 4, num_blocks_y * 4))
				return false;

			for (uint32_t by = 0; by < num_blocks_y; by++)
			{
				for (uint32_t bx = 0; bx < num_blocks_x; bx++)
				{
					decode_source_block(pSrc, width, height, num_blocks_x, sd.m_row_pitch, bx, by, texels);
					for (uint32_t y = 0; y < 4; y++)
						for (uint32_t x = 0; x < 4; x++)
							temp_image(bx * 4 + x, by * 4 + y) = texels[y * 4 + x];
				}
			}

			encode_pvrtc1(ti.m_bf, pOutput_blocks, temp_image, num_blocks_x, num_blocks_y, from_alpha);
			return true;
		}

		if (ti.m_mode == repack_mode::cASTC4x4)
		{
			// ASTC LDR 4x4. A BC7 source feeds its physical block into the bc7f BC7->ASTC latent path (high fidelity;
			// falls back to a full re-encode only for the rare 2/3-subset patterns). Other sources unpack to texels
			// and re-encode via fast_pack_astc(pixels, bc7f_flags). Honor the output row pitch (in blocks).
			const uint32_t out_pitch_blocks = output_row_pitch_in_blocks_or_pixels ? output_row_pitch_in_blocks_or_pixels : num_blocks_x;
			if (out_pitch_blocks < num_blocks_x)
				return false;
			const uint64_t max_index = (uint64_t)(num_blocks_y - 1) * out_pitch_blocks + (num_blocks_x - 1);
			if ((uint64_t)output_blocks_buf_size_in_blocks_or_pixels <= max_index)
				return false;

			// Note: from_alpha is only meaningful for ETC1 and PVRTC1 (handled by transcode_4x4_block /
			// encode_pvrtc1); it has no useful meaning for ASTC, so it's ignored here.
			const bool bc7_source = (m_src_kind == source_kind::cBC7);

			for (uint32_t by = 0; by < num_blocks_y; by++)
			{
				for (uint32_t bx = 0; bx < num_blocks_x; bx++)
				{
					uint8_t* pDst = static_cast<uint8_t*>(pOutput_blocks) + ((uint64_t)by * out_pitch_blocks + bx) * 16;

					if (bc7_source)
					{
						// BC7 latent to ASTC latent, when possible, otherwise it'll use bc7f from texels.
						const basist::bc7_block* pSrc_bc7 = reinterpret_cast<const basist::bc7_block*>(pSrc + ((uint64_t)by * num_blocks_x + bx) * m_bytes_per_block_or_pixel);
						if (!bc7f::fast_pack_astc(pDst, *pSrc_bc7, bc7f_flags))
							return false;
					}
					else
					{
						// Not BC7 - decode to texels, then use bc7f
						decode_source_block(pSrc, width, height, num_blocks_x, sd.m_row_pitch, bx, by, texels);

						if (!bc7f::fast_pack_astc(pDst, reinterpret_cast<const color_rgba*>(texels), bc7f_flags))
							return false;
					}
				}
			}
			return true;
		}

		// repack_mode::cBlock4x4: transcode_4x4_block targets (BC1/3/4/5/7, ETC1/ETC2/EAC, 32/16bpp uncompressed).
		uint32_t out_row_pitch, out_rows;
		if (ti.m_uncompressed)
		{
			// UNCOMPRESSED dest: clip (smaller) or expand (larger) per the contract above. We decode all source
			// blocks; the per-block write below clips boundary blocks and skips fully-outside ones.
			out_row_pitch = output_row_pitch_in_blocks_or_pixels ? output_row_pitch_in_blocks_or_pixels : width;
			const uint32_t dst_rows = output_rows_in_pixels ? output_rows_in_pixels : height;
			// Rows actually written: never past the image's own rows nor past the destination's rows.
			out_rows = basisu::minimum(height, dst_rows);
			// Writes stay within rows [0, out_rows) and columns [0, out_row_pitch); bound the buffer to that extent.
			const uint64_t max_index = (uint64_t)(out_rows - 1) * out_row_pitch + (out_row_pitch - 1);
			if ((uint64_t)output_blocks_buf_size_in_blocks_or_pixels <= max_index)
				return false;
		}
		else
		{
			// COMPRESSED dest: pitch (in blocks) may expand but can't shrink (see contract); every block emitted whole.
			out_row_pitch = output_row_pitch_in_blocks_or_pixels ? output_row_pitch_in_blocks_or_pixels : num_blocks_x;
			out_rows = 0;
			// A pitch smaller than the block-row width would make blocks in a row collide; reject loudly.
			if (out_row_pitch < num_blocks_x)
				return false;
			const uint64_t max_index = (uint64_t)(num_blocks_y - 1) * out_row_pitch + (num_blocks_x - 1);
			if ((uint64_t)output_blocks_buf_size_in_blocks_or_pixels <= max_index)
				return false;
		}

		etc1f::pack_etc1_state etc1_pack_state;

		// Decode block to texels, real-time encode to target format.
		for (uint32_t by = 0; by < num_blocks_y; by++)
		{
			for (uint32_t bx = 0; bx < num_blocks_x; bx++)
			{
				decode_source_block(pSrc, width, height, num_blocks_x, sd.m_row_pitch, bx, by, texels);

				// Uncompressed clip: skip blocks whose top-left is at/beyond the dest edge. transcode_4x4_block's
				// max_x/max_y are unsigned (pitch/rows - bx/by*4) and would wrap to a huge OOB bound if negative;
				// straddling blocks keep a positive max and partial-write. (Compressed pitch >= num_blocks_x, never skipped.)
				if (ti.m_uncompressed && ((bx * 4 >= out_row_pitch) || (by * 4 >= out_rows)))
					continue;

				// Block-addressed destination: used directly by compressed targets; uncompressed targets ignore it and
				// recompute the pixel pointer from pOutput_blocks + block_x/block_y (so it's harmless there).
				uint8_t* pDst_block_u8 = static_cast<uint8_t*>(pOutput_blocks) + (uint64_t)by * out_row_pitch * ti.m_out_bytes + (uint64_t)bx * ti.m_out_bytes;

				if (!transcode_4x4_block(
					ti.m_bf, bx, by,
					pOutput_blocks, pDst_block_u8,
					texels,
					ti.m_out_bytes, out_row_pitch, out_rows,
					c0, c1,
					high_quality, from_alpha,
					bc7f_flags, etc1_pack_state, has_alpha_arg))
				{
					return false;
				}
			}
		}

		return true;
#else
		// Re-encode paths require the XUASTC/bc7f/etc1f real-time encoders.
		(void)output_row_pitch_in_blocks_or_pixels; (void)output_rows_in_pixels; (void)decode_flags;
		(void)channel0; (void)channel1; (void)pSrc; (void)width; (void)height; (void)num_blocks_x; (void)num_blocks_y; (void)ti;
		return false;
#endif
	}

} // namespace basist
