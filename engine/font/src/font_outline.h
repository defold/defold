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

#ifndef DM_FONT_OUTLINE_H
#define DM_FONT_OUTLINE_H

#include <stdint.h>

#include <dmsdk/font/font.h>

/*# font outline storage type
 * @enum
 * @name FontOutlineType
 * @member FONT_OUTLINE_TYPE_UNKNOWN no supported outline table was found
 * @member FONT_OUTLINE_TYPE_GLYF TrueType quadratic outlines in glyf and loca tables
 * @member FONT_OUTLINE_TYPE_CFF1 PostScript cubic outlines in a CFF table
 * @member FONT_OUTLINE_TYPE_CFF2 PostScript cubic variable outlines in a CFF2 table
 */
enum FontOutlineType
{
    FONT_OUTLINE_TYPE_UNKNOWN,
    FONT_OUTLINE_TYPE_GLYF,
    FONT_OUTLINE_TYPE_CFF1,
    FONT_OUTLINE_TYPE_CFF2,
};

/*# glyph outline command type
 * @enum
 * @name FontOutlineCommandType
 * @member FONT_OUTLINE_MOVE_TO Start a contour at m_Points[0].
 * @member FONT_OUTLINE_LINE_TO Draw a line from the current point to m_Points[0].
 * @member FONT_OUTLINE_QUADRATIC_TO Draw a quadratic Bezier using control point m_Points[0] and endpoint m_Points[1].
 * @member FONT_OUTLINE_CUBIC_TO Draw a cubic Bezier using control points m_Points[0] and m_Points[1], and endpoint m_Points[2].
 * @member FONT_OUTLINE_CLOSE Close the current contour. The points are unused.
 */
enum FontOutlineCommandType
{
    FONT_OUTLINE_MOVE_TO,
    FONT_OUTLINE_LINE_TO,
    FONT_OUTLINE_QUADRATIC_TO,
    FONT_OUTLINE_CUBIC_TO,
    FONT_OUTLINE_CLOSE,
};

/*# point in unscaled font coordinates
 * @struct
 * @name FontOutlinePoint
 * @member m_X [type: float] horizontal coordinate
 * @member m_Y [type: float] vertical coordinate, increasing upwards
 */
struct FontOutlinePoint
{
    float m_X;
    float m_Y;
};

/*# one glyph path operation
 * @struct
 * @name FontOutlineCommand
 * @member m_Type [type: FontOutlineCommandType] operation and number of points used
 * @member m_Points [type: FontOutlinePoint[3]] control points and endpoint for the operation
 */
struct FontOutlineCommand
{
    FontOutlineCommandType m_Type;
    FontOutlinePoint       m_Points[3];
};

/*# decoded path of one glyph
 * A decoded outline owns m_Commands and must be released with
 * FontFreeGlyphOutline.
 * @struct
 * @name FontOutline
 * @member m_Commands [type: FontOutlineCommand*] ordered path commands, or null for an empty glyph
 * @member m_CommandCount [type: uint32_t] number of commands
 */
struct FontOutline
{
    FontOutlineCommand* m_Commands;
    uint32_t            m_CommandCount;
};

/*# incremental exact outline bounds
 * Accumulates bounds directly from outline drawing operations. This allows
 * outline decoders to calculate bounds without first allocating a
 * FontOutline command array.
 * @struct
 * @name FontOutlineBounds
 */
struct FontOutlineBounds
{
    FontOutlinePoint m_Current;
    float            m_X0;
    float            m_Y0;
    float            m_X1;
    float            m_Y1;
    bool             m_HasBounds;
};

/*# begin a contour and include its first point in the bounds */
void FontOutlineBoundsMoveTo(FontOutlineBounds* bounds, FontOutlinePoint to);

/*# include a line endpoint in the bounds */
void FontOutlineBoundsLineTo(FontOutlineBounds* bounds, FontOutlinePoint to);

/*# include a quadratic Bezier endpoint and its exact interior extrema */
void FontOutlineBoundsQuadraticTo(FontOutlineBounds* bounds, FontOutlinePoint control, FontOutlinePoint to);

/*# include a cubic Bezier endpoint and its exact interior extrema */
void FontOutlineBoundsCubicTo(FontOutlineBounds* bounds, FontOutlinePoint control_1, FontOutlinePoint control_2, FontOutlinePoint to);

/*# include translated source bounds in a destination accumulator */
void FontOutlineBoundsMerge(FontOutlineBounds* bounds, const FontOutlineBounds* source, float dx, float dy);

/*# copy accumulated bounds to outputs, returning false when no path exists */
bool FontOutlineBoundsGet(const FontOutlineBounds* bounds, float* x0, float* y0, float* x1, float* y1);

/*# split an outline into y-monotonic Bezier curves
 * Replaces the owned command array with an equivalent path where every
 * quadratic and cubic Bezier is monotonic along the y axis. Lines, moves and
 * contour closing commands are preserved.
 * @name FontOutlineMakeYMonotonic
 * @param outline [type: FontOutline*] outline to normalize in place
 * @return result [type: FontResult] FONT_RESULT_OK on success
 */
FontResult FontOutlineMakeYMonotonic(FontOutline* outline);

/*# release a glyph outline
 * Frees the command array and clears the outline. The FontOutline value itself
 * remains caller-owned.
 * @name FontFreeGlyphOutline
 * @param outline [type: FontOutline*] outline to release
 */
void FontFreeGlyphOutline(FontOutline* outline);

/*# calculate glyph outline bounds
 * Calculates exact bounds in unscaled font coordinates, including quadratic
 * and cubic Bezier extrema.
 * @name FontGetOutlineBounds
 * @param outline [type: const FontOutline*] outline to measure
 * @param x0 [type: float*] (out) minimum horizontal coordinate
 * @param y0 [type: float*] (out) minimum vertical coordinate
 * @param x1 [type: float*] (out) maximum horizontal coordinate
 * @param y1 [type: float*] (out) maximum vertical coordinate
 * @return has_bounds [type: bool] false if the outline has no points
 */
bool FontGetOutlineBounds(const FontOutline* outline, float* x0, float* y0, float* x1, float* y1);

#endif // DM_FONT_OUTLINE_H
