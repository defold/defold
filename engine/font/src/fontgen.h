// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_FONTGEN_H
#define DM_FONTGEN_H

#include <dmsdk/font/font.h>
#include <dmsdk/extension/extension.h>
#include <dmsdk/font/text_layout.h>

#include <dlib/jobsystem.h>

/*# Runtime font glyph generation
 *
 * Asynchronously generates glyphs and hands them to a caller-owned cache.
 * Glyph insertion and completion callbacks run on the job context owner, or on
 * the caller of FontGenUpdateGlyphs for a batch started with FontGenStartGlyphs.
 * Cache lookup runs while preparing the batch. The caller context must remain
 * valid until the completion callback has run or the job has been cancelled.
 *
 * @document
 * @name Font Generation
 * @language C++
 */

/*# Tests whether runtime glyph generation is linked into the engine.
 * @name FontGenIsSupported
 * @return supported [type: bool] True when the full font_gen library is linked.
 */
bool FontGenIsSupported();

/*# Looks up a glyph in the caller-owned cache.
 * @typedef
 * @name FFontGenIsGlyphCached
 */
typedef bool (*FFontGenIsGlyphCached)(void* context, HFont font, uint32_t glyph_index);

/*# Inserts a generated glyph into the caller-owned cache.
 *
 * Returning FONT_RESULT_OK transfers ownership of the glyph to the callback.
 * For every other result, font_gen retains ownership and frees the glyph.
 *
 * @typedef
 * @name FFontGenAddGlyph
 */
typedef FontResult (*FFontGenAddGlyph)(void* context, HFont font, FontGlyph* glyph);

/*# Reports completion of a glyph generation batch.
 * @typedef
 * @name FFontGenComplete
 */
typedef void (*FFontGenComplete)(void* context, int result, const char* error_message);

/*# Runtime glyph generation parameters.
 * @struct
 * @name FontGenParams
 * @member m_UserContext [type: void*] Opaque context passed to all callbacks.
 * @member m_IsGlyphCached [type: FFontGenIsGlyphCached] Cache lookup callback.
 * @member m_AddGlyph [type: FFontGenAddGlyph] Glyph insertion callback.
 * @member m_Complete [type: FFontGenComplete] Batch completion callback.
 * @member m_Jobs [type: HJobContext] Job system used for asynchronous generation.
 * @member m_Size [type: float] Glyph generation size in pixels.
 * @member m_OutlineWidth [type: float] Outline width in pixels.
 * @member m_ShadowBlur [type: float] Shadow blur spread in pixels.
 * @member m_IsSdf [type: uint8_t] Non-zero when SDF output is requested.
 * @member m_IsVector [type: uint8_t] Non-zero when Vector glyph output is requested.
 * @member m_BitmapEffects [type: uint8_t] Generate RGB vector effect bitmaps on the CPU.
 * @member m_HasOutline [type: uint8_t] Include the authored outline in the effect bitmaps.
 * @member m_HasShadow [type: uint8_t] Generate the authored shadow bitmap.
 */
struct FontGenParams
{
    void*                   m_UserContext;
    FFontGenIsGlyphCached   m_IsGlyphCached;
    FFontGenAddGlyph        m_AddGlyph;
    FFontGenComplete        m_Complete;
    HJobContext             m_Jobs;
    float                   m_Size;
    float                   m_OutlineWidth;
    float                   m_ShadowBlur;
    uint8_t                 m_IsSdf;
    uint8_t                 m_IsVector;
    uint8_t                 m_BitmapEffects;
    uint8_t                 m_HasOutline;
    uint8_t                 m_HasShadow;
};

dmExtension::Result FontGenInitialize(dmExtension::Params* params);
dmExtension::Result FontGenFinalize(dmExtension::Params* params);

float FontGenGetBasePadding(); // E.g. 3
float FontGenGetEdgeValue(); // [0 .. 255]

struct FontGenJobData;

/*# Allocates scratch data for one generation batch.
 * @name FontGenCreateJobData
 * @param params [type: FontGenParams*] Generation settings and callbacks. The values are copied.
 * @param num_glyphs [type: uint32_t] Maximum number of glyphs in the batch.
 * @return job_data [type: FontGenJobData*] Job data, or null when unsupported.
 */
FontGenJobData* FontGenCreateJobData(const FontGenParams* params, uint32_t num_glyphs);

/*# Releases a generation batch and its untransferred glyphs.
 * Batches started with FontGenStartGlyphs cancel and wait for their own jobs.
 * Other batches must already have finished or been canceled by the caller.
 * @name FontGenDestroyJobData
 * @param jobdata [type: FontGenJobData*] Batch to release.
 */
void FontGenDestroyJobData(FontGenJobData* jobdata);

HJob FontGenAddGlyphByIndex(FontGenJobData* jobdata, HFont font, uint32_t glyph_index);
HJob FontGenAddGlyphs(FontGenJobData* jobdata, TextGlyph* glyphs, uint32_t num_glyphs);

/*# Starts a batch whose results are consumed by its caller.
 * Worker jobs generate glyphs without invoking callbacks. Call FontGenUpdateGlyphs
 * on the resource's creation thread to install the results. That update can take
 * over queued glyphs, including when the caller occupies the last worker. With
 * no workers it generates all glyphs. The job context must outlive the batch.
 * @name FontGenStartGlyphs
 * @param jobdata [type: FontGenJobData*] Fresh batch; keep alive until completion or destruction.
 * @param glyphs [type: ::TextGlyph*] Input glyphs, copied during this call.
 * @param num_glyphs [type: uint32_t] Number of input glyphs.
 * @return started [type: bool] False when runtime generation is unsupported.
 */
bool FontGenStartGlyphs(FontGenJobData* jobdata, TextGlyph* glyphs, uint32_t num_glyphs);

/*# Consumes results for a batch started with FontGenStartGlyphs.
 * Generates queued glyphs or consumes finished results without dispatching other
 * jobs or callbacks. Running jobs keep ownership until they finish. The completion
 * callback may destroy the batch. Calls for one batch must be serialized by its owner.
 * @name FontGenUpdateGlyphs
 * @param jobdata [type: FontGenJobData*] Batch to advance.
 * @param time_limit [type: uint64_t] Soft time limit in microseconds; 0 processes at most one glyph.
 * @return complete [type: bool] True once all glyphs and the completion callback have been processed.
 */
bool FontGenUpdateGlyphs(FontGenJobData* jobdata, uint64_t time_limit);

#endif // DM_FONTGEN_H
