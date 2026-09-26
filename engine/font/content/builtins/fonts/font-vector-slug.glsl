/*
MIT License

Copyright (c) 2017 Eric Lengyel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
// GLSL port of Eric Lengyel's Slug reference fragment algorithm (MIT).
// Original: Eric Lengyel’s Slug reference pixel shader.
// Packed R32UI carries the reference's two unsigned 16-bit band fields.
float saturate(float x) { return clamp(x, 0.0, 1.0); }
uniform highp sampler2D curve_texture;
#ifdef SLUG_LEGACY_GL
// The editor's GL 2 profile reads the same band records as exact float32 pairs.
uniform sampler2D band_texture;
uniform vec4 curve_texture_size_recip;
uniform vec4 band_texture_size_recip;
ivec2 LoadBand(ivec2 location)
{
    return ivec2(texture2D(band_texture, (vec2(location) + 0.5) * band_texture_size_recip.xy).rg);
}
#else
uniform highp utexture2D band_texture;
ivec2 LoadBand(ivec2 location)
{
    uint band_data = texelFetch(band_texture, location, 0).x;
    return ivec2(band_data & 65535U, band_data >> 16U);
}
#endif

// ===================================================
// Reference pixel shader for the Slug algorithm.
// This code is made available under the MIT License.
// Copyright 2017, by Eric Lengyel.
// ===================================================


// The curve and band textures use a fixed width of 4096 texels.

#define kLogBandTextureWidth 12

// It's convenient to have a texel load function to aid in translation to other shader languages.

#ifdef SLUG_LEGACY_GL
#define TexelLoad2D(x, y) texture2D(x, (vec2(y) + 0.5) * curve_texture_size_recip.xy)
#else
#define TexelLoad2D(x, y) texelFetch(x, y, 0)
#endif


int CalcRootCode(float y1, float y2, float y3)
{
#ifdef SLUG_LEGACY_GL
    // The reference's eight sign combinations, without GLSL integer bit ops.
    int signs = (y1 < 0.0 ? 1 : 0) + (y2 < 0.0 ? 2 : 0) + (y3 < 0.0 ? 4 : 0);
    if (signs == 1 || signs == 3) return 256;
    if (signs == 2 || signs == 5) return 257;
    if (signs == 4 || signs == 6) return 1;
    return 0;
#else
	// Calculate the root eligibility code for a sample-relative quadratic Bézier curve.
	// Extract the signs of the y coordinates of the three control points.

	uint i1 = floatBitsToUint(y1) >> 31U;
	uint i2 = floatBitsToUint(y2) >> 30U;
	uint i3 = floatBitsToUint(y3) >> 29U;

	uint shift = (i2 & 2U) | (i1 & ~2U);
	shift = (i3 & 4U) | (shift & ~4U);

	// Eligibility is returned in bits 0 and 8.

	return int((0x2E74U >> shift) & 0x0101U);
#endif
}

vec2 SolveHorizPoly(vec4 p12, vec2 p3)
{
	// Solve for the values of t where the curve crosses y = 0.
	// The quadratic polynomial in t is given by
	//
	//     a t^2 - 2b t + c,
	//
	// where a = p1.y - 2 p2.y + p3.y, b = p1.y - p2.y, and c = p1.y.
	// The discriminant b^2 - ac is clamped to zero, and imaginary
	// roots are treated as a double root at the global minimum
	// where t = b / a.

	vec2 a = p12.xy - p12.zw * 2.0 + p3;
	vec2 b = p12.xy - p12.zw;
	float ra = 1.0 / a.y;
	float rb = 0.5 / b.y;

	float d = sqrt(max(b.y * b.y - a.y * p12.y, 0.0));
	float t1 = (b.y - d) * ra;
	float t2 = (b.y + d) * ra;

	// If the polynomial is nearly linear, then solve -2b t + c = 0.

	if (abs(a.y) < 1.0 / 65536.0) t1 = t2 = p12.y * rb;

	// Return the x coordinates where C(t) = 0.

	return (vec2((a.x * t1 - b.x * 2.0) * t1 + p12.x, (a.x * t2 - b.x * 2.0) * t2 + p12.x));
}

vec2 SolveVertPoly(vec4 p12, vec2 p3)
{
	// Solve for the values of t where the curve crosses x = 0.

	vec2 a = p12.xy - p12.zw * 2.0 + p3;
	vec2 b = p12.xy - p12.zw;
	float ra = 1.0 / a.x;
	float rb = 0.5 / b.x;

	float d = sqrt(max(b.x * b.x - a.x * p12.x, 0.0));
	float t1 = (b.x - d) * ra;
	float t2 = (b.x + d) * ra;

	// If the polynomial is nearly linear, then solve -2b t + c = 0.

	if (abs(a.x) < 1.0 / 65536.0) t1 = t2 = p12.x * rb;

	// Return the y coordinates where C(t) = 0.

	return (vec2((a.y * t1 - b.y * 2.0) * t1 + p12.y, (a.y * t2 - b.y * 2.0) * t2 + p12.y));
}

ivec2 CalcBandLoc(ivec2 glyphLoc, int offset)
{
#ifdef SLUG_LEGACY_GL
    float x = float(glyphLoc.x + offset);
    return ivec2(mod(x, 4096.0), float(glyphLoc.y) + floor(x / 4096.0));
#else
	// If the offset causes the x coordinate to exceed the texture width, then wrap to the next line.

	ivec2 bandLoc = ivec2(glyphLoc.x + int(offset), glyphLoc.y);
	bandLoc.y += bandLoc.x >> kLogBandTextureWidth;
	bandLoc.x &= (1 << kLogBandTextureWidth) - 1;
	return (bandLoc);
#endif
}

float CalcCoverage(float xcov, float ycov, float xwgt, float ywgt, int flags)
{
	// Combine coverages from the horizontal and vertical rays using their weights.
	// Absolute values ensure that either winding direction convention works.

	float coverage = max(abs(xcov * xwgt + ycov * ywgt) / max(xwgt + ywgt, 1.0 / 65536.0), min(abs(xcov), abs(ycov)));

	// If SLUG_EVENODD is defined during compilation, then check E flag in tex.w. (See vertex shader.)

	#if defined(SLUG_EVENODD)

		if ((flags & 0x1000) == 0)
		{

	#endif

			// Using nonzero fill rule here.

			coverage = saturate(coverage);

	#if defined(SLUG_EVENODD)

		}
		else
		{
			// Using even-odd fill rule here.

			coverage = 1.0 - abs(1.0 - fract(coverage * 0.5) * 2.0);
		}

	#endif

	// If SLUG_WEIGHT is defined during compilation, then take a square root to boost optical weight.

	#if defined(SLUG_WEIGHT)

		coverage = sqrt(coverage);

	#endif

	return (coverage);
}

float SlugRender(vec2 renderCoord, vec2 emsPerPixel, vec4 bandTransform, ivec4 glyphData)
{
	int curveIndex;

	// The effective pixel dimensions of the em square are computed
	// independently for x and y directions with texcoord derivatives supplied
	// by the caller before branching on the glyph layer.

	vec2 pixelsPerEm = 1.0 / emsPerPixel;

	ivec2 bandMax = glyphData.zw;
	#ifdef SLUG_LEGACY_GL
    bandMax.y = int(mod(float(bandMax.y), 256.0));
    #else
    bandMax.y &= 0x00FF;
    #endif

	// Determine what bands the current pixel lies in by applying a scale and offset
	// to the render coordinates. The scales are given by bandTransform.xy, and the
	// offsets are given by bandTransform.zw. Band indexes are clamped to [0, bandMax.xy].

	#ifdef SLUG_LEGACY_GL
    ivec2 bandIndex = ivec2(clamp(renderCoord * bandTransform.xy + bandTransform.zw, vec2(0.0), vec2(bandMax)));
    #else
    ivec2 bandIndex = clamp(ivec2(renderCoord * bandTransform.xy + bandTransform.zw), ivec2(0, 0), bandMax);
    #endif
	ivec2 glyphLoc = glyphData.xy;

	float xcov = 0.0;
	float xwgt = 0.0;

	// Fetch data for the horizontal band from the index texture. The number
	// of curves intersecting the band is in the x component, and the offset
	// to the list of locations for those curves is in the y component.

	ivec2 hbandData = LoadBand(ivec2(glyphLoc.x + bandIndex.y, glyphLoc.y)).xy;
	ivec2 hbandLoc = CalcBandLoc(glyphLoc, hbandData.y);

	// Loop over all curves in the horizontal band.

	for (curveIndex = 0; curveIndex < int(hbandData.x); curveIndex++)
	{
		// Fetch the location of the current curve from the index texture.

		ivec2 curveLoc = ivec2(LoadBand(ivec2(hbandLoc.x + curveIndex, hbandLoc.y)).xy);

		// Fetch the three 2D control points for the current curve from the curve texture.
		// The first texel contains both p1 and p2 in the (x,y) and (z,w) components, respectively,
		// and the the second texel contains p3 in the (x,y) components. Subtracting the render
		// coordinates makes the curve relative to the sample position. The quadratic Bézier curve
		// C(t) is given by
		//
		//     C(t) = (1 - t)^2 p1 + 2t(1 - t) p2 + t^2 p3

		vec4 p12 = TexelLoad2D(curve_texture, curveLoc) - vec4(renderCoord, renderCoord);
		vec2 p3 = TexelLoad2D(curve_texture, ivec2(curveLoc.x + 1, curveLoc.y)).xy - renderCoord;

		// If the largest x coordinate among all three control points falls
		// left of the current pixel, then there are no more curves in the
		// horizontal band that can influence the result, so exit the loop.
		// (The curves are sorted in descending order by max x coordinate.)

		if (max(max(p12.x, p12.z), p3.x) * pixelsPerEm.x < -0.5) break;

		int code = CalcRootCode(p12.y, p12.w, p3.y);
		if (code != 0)
		{
			// At least one root makes a contribution. Calculate them and scale so
			// that the current pixel corresponds to the range [0,1].

			vec2 r = SolveHorizPoly(p12, p3) * pixelsPerEm.x;

			// Bits in code tell which roots make a contribution.

			if (code == 1 || code == 257)
			{
				xcov += saturate(r.x + 0.5);
				xwgt = max(xwgt, saturate(1.0 - abs(r.x) * 2.0));
			}

			if (code > 1)
			{
				xcov -= saturate(r.y + 0.5);
				xwgt = max(xwgt, saturate(1.0 - abs(r.y) * 2.0));
			}
		}
	}

	float ycov = 0.0;
	float ywgt = 0.0;

	// Fetch data for the vertical band from the index texture. This follows
	// the data for all horizontal bands, so we have to add bandMax.y + 1.

	ivec2 vbandData = LoadBand(ivec2(glyphLoc.x + bandMax.y + 1 + bandIndex.x, glyphLoc.y)).xy;
	ivec2 vbandLoc = CalcBandLoc(glyphLoc, vbandData.y);

	// Loop over all curves in the vertical band.

	for (curveIndex = 0; curveIndex < int(vbandData.x); curveIndex++)
	{
		ivec2 curveLoc = ivec2(LoadBand(ivec2(vbandLoc.x + curveIndex, vbandLoc.y)).xy);
		vec4 p12 = TexelLoad2D(curve_texture, curveLoc) - vec4(renderCoord, renderCoord);
		vec2 p3 = TexelLoad2D(curve_texture, ivec2(curveLoc.x + 1, curveLoc.y)).xy - renderCoord;

		// If the largest y coordinate among all three control points falls
		// below the current pixel, then there are no more curves in the
		// vertical band that can influence the result, so exit the loop.
		// (The curves are sorted in descending order by max y coordinate.)

		if (max(max(p12.y, p12.w), p3.y) * pixelsPerEm.y < -0.5) break;

		int code = CalcRootCode(p12.x, p12.z, p3.x);
		if (code != 0)
		{
			vec2 r = SolveVertPoly(p12, p3) * pixelsPerEm.y;

			if (code == 1 || code == 257)
			{
				ycov -= saturate(r.x + 0.5);
				ywgt = max(ywgt, saturate(1.0 - abs(r.x) * 2.0));
			}

			if (code > 1)
			{
				ycov += saturate(r.y + 0.5);
				ywgt = max(ywgt, saturate(1.0 - abs(r.y) * 2.0));
			}
		}
	}

	return (CalcCoverage(xcov, ycov, xwgt, ywgt, glyphData.w));
}
