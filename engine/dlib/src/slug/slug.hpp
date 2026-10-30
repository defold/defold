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

#ifndef DM_SLUG_HPP
#define DM_SLUG_HPP

#include <dlib/array.h>
#include <dlib/vmath.h>

// Path preparation and CPU encoding for the shared Slug numeric texture format.
// This module borrows path inputs and owns no font, resource or GPU objects.
namespace dmSlug
{
    static const uint32_t TEXTURE_WIDTH = 4096;
    static const uint32_t MAX_TEXTURE_HEIGHT = 256;

    enum OutlineCommandType
    {
        OUTLINE_MOVE_TO,
        OUTLINE_LINE_TO,
        OUTLINE_QUADRATIC_TO,
        OUTLINE_CUBIC_TO,
        OUTLINE_CLOSE,
    };

    struct Point
    {
        float m_X;
        float m_Y;
    };

    // A cubic uses all three points. A quadratic uses control and endpoint;
    // moves and lines use only the first point. Close has no points.
    struct OutlineCommand
    {
        OutlineCommandType m_Type;
        Point              m_Points[3];
    };

    struct Outline
    {
        OutlineCommand* m_Commands;
        uint32_t        m_CommandCount;
    };

    // Zero-initialize before accumulating exact line/Bezier bounds.
    struct OutlineBounds
    {
        Point m_Current;
        float m_X0;
        float m_Y0;
        float m_X1;
        float m_Y1;
        bool  m_HasBounds;
    };

    enum CurveType
    {
        CURVE_MOVE_TO,
        CURVE_LINE_TO,
        CURVE_QUADRATIC_TO,
        CURVE_CLOSE,
    };

    struct CurveCommand
    {
        Point    m_Points[2];
        uint8_t  m_Type;
        uint8_t  m_Reserved0;
        uint16_t m_Reserved1;
    };

    // Per-path addresses and transform into its horizontal/vertical bands.
    struct Path
    {
        uint32_t m_BandTexel;
        uint32_t m_CurveCount;
        float    m_BandTransform[4];
    };

    // Owns CPU data: RGBA16F curves and packed R32UI band headers/references.
    // Configure m_MaxTexels before appending. Records stay within texture rows.
    struct Data
    {
        Data();
        dmArray<uint16_t> m_Curves;
        dmArray<uint32_t> m_Bands;
        bool              m_Overflow;
        uint32_t          m_MaxTexels;
        uint32_t          m_CurveTexels;
        uint32_t          m_BandTexels;
        uint32_t          m_CurveCount;
        uint32_t          m_BandReferences;
        uint32_t          m_MaxBandCurves;
    };

    struct PaintPath
    {
        uint32_t         m_Id; // Caller-defined identity, retained by font cache adapters.
        uint32_t         m_EvenOdd;
        Path             m_Slug;
        dmVMath::Vector4 m_Bounds;
    };

    struct PaintLayer
    {
        uint32_t         m_Path;
        uint32_t         m_Paint;
        uint32_t         m_CompositeCount; // On last layer: paints sharing coverage.
        dmVMath::Matrix4 m_Transform;
        dmVMath::Matrix4 m_PaintTransform;
        dmVMath::Vector4 m_Clip;
    };

    struct PaintImage
    {
        uint32_t         m_Id; // Caller-defined image identity.
        uint32_t         m_LayerCount;
        uint32_t         m_Paint; // Header followed by six texels per layer.
        dmVMath::Vector4 m_Bounds;
    };

    // Incremental exact path bounds. All pointers are borrowed.
    void OutlineBoundsMoveTo(OutlineBounds* bounds, Point to);
    void OutlineBoundsLineTo(OutlineBounds* bounds, Point to);
    void OutlineBoundsQuadraticTo(OutlineBounds* bounds, Point control, Point to);
    void OutlineBoundsCubicTo(OutlineBounds* bounds, Point control_1, Point control_2, Point to);
    void OutlineBoundsMerge(OutlineBounds* bounds, const OutlineBounds* source, float dx, float dy);
    bool OutlineBoundsGet(const OutlineBounds* bounds, float* x0, float* y0, float* x1, float* y1);
    bool GetOutlineBounds(const Outline* outline, float* x0, float* y0, float* x1, float* y1);

    // Borrows input; replaces caller-owned commands on success. Cubic error is
    // bounded by tolerance in input coordinates, before scale is applied.
    bool OutlineToQuadratics(const Outline* outline, float scale, float tolerance, dmArray<CurveCommand>* commands);

    // Clears data and counters while retaining CPU array capacity.
    void Begin(Data* data);
    // Appends borrowed quadratic commands. Coordinates are divided by scale.
    // Failure preserves existing records and counters; overflow is reported.
    bool AddPath(Data* data, const CurveCommand* commands, uint32_t command_count, float scale, uint32_t bands, Path* path);
    // Appends precompiled curves (eight floats each, last two reserved). Input
    // may be unaligned and is borrowed. Coordinates are already normalized.
    bool AddEncodedCurves(Data* data, const void* curves, uint32_t count, uint32_t bands, Path* path);

    // Appends paint records to the caller-owned array. Inputs are borrowed;
    // image supplies layer count and receives its paint address and bounds.
    // Grows the paint array for one header and six texels per layer.
    // sRGB interpolation/compositing is selected per image; COLR uses linear.
    void EncodePaint(dmArray<dmVMath::Vector4>* paints, const PaintPath* paths, const PaintLayer* layers, PaintImage* image, bool srgb);
} // namespace dmSlug

#endif
