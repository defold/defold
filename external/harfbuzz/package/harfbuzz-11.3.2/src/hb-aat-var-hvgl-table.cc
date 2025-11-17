#include "hb-aat-var-hvgl-table.hh"

#ifdef __APPLE__
// For endianness
#include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(HB_NO_SIMD)
#define HB_NO_APPLE_SIMD
#endif

#if !defined(HB_NO_APPLE_SIMD) && !(defined(__APPLE__) && \
  (!defined(MAC_OS_X_VERSION_MIN_REQUIRED) || MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) \
)
#define HB_NO_APPLE_SIMD
#endif

#ifndef HB_NO_APPLE_SIMD
#include <simd/simd.h> // Apple SIMD https://developer.apple.com/documentation/accelerate/simd
#endif

#ifndef HB_NO_SIMD
#ifdef __AVX2__
#include <immintrin.h>
#endif
#endif


#ifndef HB_NO_VAR_HVF

namespace AAT {

namespace hvgl_impl {


enum
segment_point_t
{
  SEGMENT_POINT_ON_CURVE_X = 0,
  SEGMENT_POINT_ON_CURVE_Y = 1,
  SEGMENT_POINT_OFF_CURVE_X = 2,
  SEGMENT_POINT_OFF_CURVE_Y = 3,
};

enum
blend_type_t
{
  BLEND_TYPE_CURVE = 0,
  BLEND_TYPE_CORNER = 1,
  BLEND_TYPE_TANGENT = 2,
  BLEND_TYPE_TANGENT_PAIR_FIRST = 3,
  BLEND_TYPE_TANGENT_PAIR_SECOND = 4,
};

using segment_t = double*;

static void
project_on_curve_to_tangent (const segment_t offcurve1,
			     segment_t oncurve,
			     const segment_t offcurve2)
{
  double &x = oncurve[SEGMENT_POINT_ON_CURVE_X];
  double &y = oncurve[SEGMENT_POINT_ON_CURVE_Y];

  double x1 = offcurve1[SEGMENT_POINT_OFF_CURVE_X];
  double y1 = offcurve1[SEGMENT_POINT_OFF_CURVE_Y];
  double x2 = offcurve2[SEGMENT_POINT_OFF_CURVE_X];
  double y2 = offcurve2[SEGMENT_POINT_OFF_CURVE_Y];

  double dx = x2 - x1;
  double dy = y2 - y1;

  double l2 = dx * dx + dy * dy;
  double t = l2 ? (dx * (x - x1) + dy * (y - y1)) / l2 : 0;
  t = hb_clamp (t, 0, 1);

  x = x1 + dx * t;
  y = y1 + dy * t;
}

#ifndef HB_NO_SIMD
#ifdef __AVX2__
__attribute__((target("avx2")))
#endif
#ifdef __FMA__
__attribute__((target("fma")))
#endif
#endif
void
PartShape::get_path_at (const hb_hvgl_context_t *c,
		        hb_array_t<const double> coords,
			hb_array_t<hb_transform_t<double>> transforms) const
{
  hb_transform_t<double> transform = transforms[0];

  const auto &blendTypes = StructAfter<decltype (blendTypesX)> (segmentCountPerPath, pathCount);

  const auto &padding = StructAfter<decltype (paddingX)> (blendTypes, segmentCount);
  const auto &coordinates = StructAfter<decltype (coordinatesX)> (padding, this);

  auto a = coordinates.get_coords (segmentCount);
  auto &v = c->scratch.points;

#ifdef __BYTE_ORDER
  constexpr bool le = __BYTE_ORDER == __LITTLE_ENDIAN;
#elif defined(__APPLE__)
  bool le = CFByteOrderGetCurrent () == CFByteOrderLittleEndian;
#else
  constexpr bool le = false;
#endif

  if (le)
  {
    // Endianness matches; Faster to memcpy().
    v.resize (a.length, false);
    memcpy (v.arrayZ, a.arrayZ, v.length * sizeof (v[0]));
  }
  else
  {
    v.resize (0);
    v.extend (a);
  }

  if (unlikely (v.in_error ()))
    return;

  coords = coords.sub_array (0, axisCount);
  // Apply deltas
  if (coords)
  {
    unsigned rows_count = v.length;
    const auto &deltas = StructAfter<decltype (deltasX)> (coordinates, segmentCount);
    const auto matrix = deltas.get_matrix (axisCount, segmentCount).arrayZ;
    unsigned axis_count = coords.length;
    unsigned axis_index = 0;
    HB_UNUSED bool src_aligned = (uintptr_t) matrix % 8 == 0;

#ifndef HB_NO_APPLE_SIMD
    // APPLE SIMD

    // dest is always aligned.
    if (le && src_aligned)
    {
      hb_barrier ();
      simd_double4 coords4;
      unsigned column_idx[4];
      while (axis_index < axis_count)
      {
	unsigned j;
	for (j = 0; j < 4; j++)
	{
	  while (axis_index < axis_count && !coords.arrayZ[axis_index])
	    axis_index++;
	  if (axis_index >= axis_count)
	  {
	    if (!j)
	      break;
	    for (; j < 4; j++)
	    {
	      coords4[j] = 0.;
	      column_idx[j] = 0;
	    }
	    break;
	  }
	  double coord = (double) coords.arrayZ[axis_index];
	  coords4[j] = coord;
	  bool pos = coord > 0.;
	  column_idx[j] = axis_index * 2 + pos;
	  axis_index++;
	}
	if (!j)
	  break;

	simd_double4 scalar4 = simd_abs (coords4);
	auto *delta0 = matrix + column_idx[0] * rows_count;
	auto *delta1 = matrix + column_idx[1] * rows_count;
	auto *delta2 = matrix + column_idx[2] * rows_count;
	auto *delta3 = matrix + column_idx[3] * rows_count;

	// Note: Count is always a multiple of 4
	for (unsigned i = 0; i + 4 <= rows_count; i += 4)
	{
	  const auto &src0 = * (const simd_packed_double4 *) (void *) (delta0 + i);
	  const auto &src1 = * (const simd_packed_double4 *) (void *) (delta1 + i);
	  const auto &src2 = * (const simd_packed_double4 *) (void *) (delta2 + i);
	  const auto &src3 = * (const simd_packed_double4 *) (void *) (delta3 + i);
	  const auto matrix = simd_matrix (src0, src1, src2, src3);

	  * (simd_packed_double4 *) (v.arrayZ + i) += simd_mul (matrix, scalar4);
	}
      }
    }
#endif

    for (; axis_index < axis_count; axis_index++)
    {
      double coord = coords.arrayZ[axis_index];
      if (!coord) continue;
      bool pos = coord > 0.;
      unsigned column_idx = axis_index * 2 + pos;
      double scalar = fabs(coord);

      const auto *src = matrix + column_idx * rows_count;
      auto *dest = v.arrayZ;
      unsigned i = 0;

#ifndef HB_NO_SIMD
      if (le && src_aligned && rows_count > 4)
      {
#ifdef __AVX2__
	{
	  __m256d scalar_vec = _mm256_set1_pd(scalar);
	  for (; i + 4 <= rows_count; i += 4)
	  {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
	    __m256d src_vec = _mm256_loadu_pd ((double *) &src[i]);
#pragma GCC diagnostic pop
	    __m256d dest_vec = _mm256_loadu_pd (&dest[i]);
	    __m256d result =
#ifdef __FMA__
	      true ? _mm256_fmadd_pd (src_vec, scalar_vec, dest_vec) :
#endif
	      _mm256_add_pd (_mm256_mul_pd(src_vec, scalar_vec), dest_vec);

	    _mm256_storeu_pd (&dest[i], result);
	  }
	}
#endif
      }
#endif

      // This loop is really hot
      for (; i + 4 <= rows_count; i += 4)
      {
	dest[i] += src[i] * scalar;
	dest[i + 1] += src[i + 1] * scalar;
	dest[i + 2] += src[i + 2] * scalar;
	dest[i + 3] += src[i + 3] * scalar;
      }
      // Note: Count is always a multiple of 4
      // So, the following not needed.
      if (false)
	for (; i < rows_count; i++)
	  dest[i] += src[i] * scalar;
    }
  }

  // Resolve blend types, one path at a time, and draw.
  unsigned start = 0;
  for (unsigned pathSegmentCount : segmentCountPerPath.as_array (pathCount))
  {
    unsigned end = start + pathSegmentCount;

    if (unlikely (end * 4 > v.length))
      break;

    if (unlikely (start == end))
      continue;

    // Resolve blend types
    {
      segment_t segment = &v.arrayZ[(end - 1) * 4];
      for (unsigned i = start; i < end; i++)
      {
	unsigned blendType = blendTypes.arrayZ[i];
	const segment_t prev_segment = segment;
	segment = &v.arrayZ[i * 4];

	switch (blendType)
	{
	default:
	  break;

	case BLEND_TYPE_CURVE:
	  {
	    double t = segment[SEGMENT_POINT_ON_CURVE_X];
	    t = hb_clamp (t, 0, 1);

	    /* Interpolate between the off-curve points */
	    double x = prev_segment[SEGMENT_POINT_OFF_CURVE_X] + (segment[SEGMENT_POINT_OFF_CURVE_X] - prev_segment[SEGMENT_POINT_OFF_CURVE_X]) * t;
	    double y = prev_segment[SEGMENT_POINT_OFF_CURVE_Y] + (segment[SEGMENT_POINT_OFF_CURVE_Y] - prev_segment[SEGMENT_POINT_OFF_CURVE_Y]) * t;

	    segment[SEGMENT_POINT_ON_CURVE_X] = x;
	    segment[SEGMENT_POINT_ON_CURVE_Y] = y;
	  }
	  break;

	case BLEND_TYPE_CORNER:
	  break;

	case BLEND_TYPE_TANGENT:
	  {
	    /* Project onto the line between the off-curve point
	     * of the previous segment and the off-curve point of
	     * this segment */
	    project_on_curve_to_tangent (prev_segment, segment, segment);
	  }
	  break;

	case BLEND_TYPE_TANGENT_PAIR_FIRST:
	  {
	    unsigned next_i = i == end - 1 ? start : i + 1;
	    segment_t next_segment = &v.arrayZ[next_i * 4];

	    project_on_curve_to_tangent (prev_segment, segment, next_segment);
	    project_on_curve_to_tangent (prev_segment, next_segment, next_segment);
	  }
	  break;
	}
      }
    }

    // Draw
    {
      segment_t next_segment = &v.arrayZ[start * 4];
      double x0 = next_segment[SEGMENT_POINT_ON_CURVE_X];
      double y0 = next_segment[SEGMENT_POINT_ON_CURVE_Y];
      transform.transform_point (x0, y0);
      if (c->draw_session)
	c->draw_session->move_to ((float) x0, (float) y0);
      else if (c->extents)
	c->extents->add_point ((float) x0, (float) y0);
      for (unsigned i = start; i < end; i++)
      {
	segment_t segment = next_segment;
	unsigned next_i = i == end - 1 ? start : i + 1;
	next_segment = &v.arrayZ[next_i * 4];

	double x1 = segment[SEGMENT_POINT_OFF_CURVE_X];
	double y1 = segment[SEGMENT_POINT_OFF_CURVE_Y];
	double x2 = next_segment[SEGMENT_POINT_ON_CURVE_X];
	double y2 = next_segment[SEGMENT_POINT_ON_CURVE_Y];
	transform.transform_point (x1, y1);
	transform.transform_point (x2, y2);
	if (c->draw_session)
	  c->draw_session->quadratic_to ((float) x1, (float) y1, (float) x2, (float) y2);
	else if (c->extents)
	{
	  c->extents->add_point ((float) x1, (float) y1);
	  c->extents->add_point ((float) x2, (float) y2);
	}
      }
      if (c->draw_session)
	c->draw_session->close_path ();
    }

    start = end;
  }
}

void
PartComposite::apply_to_coords (hb_array_t<double> out_coords,
				hb_array_t<const double> coords) const
{
  const auto &ecs = StructAtOffset<ExtremumColumnStarts> (this, extremumColumnStartsOff4 * 4);
  const auto &extremumColumnStart = ecs.extremumColumnStart;
  const auto &masterRowIndex = StructAfter<decltype (ecs.masterRowIndexX)> (ecs.extremumColumnStart, 2 * axisCount + 1);
  const auto &extremumRowIndex = StructAfter<decltype (ecs.extremumRowIndexX)> (masterRowIndex, sparseMasterAxisValueCount);

  const auto &masterAxisValueDeltas = StructAtOffset<MasterAxisValueDeltas> (this, masterAxisValueDeltasOff4 * 4);
  hb_array_t<const HBFLOAT32LE> master_axis_value_deltas = masterAxisValueDeltas.as_array (sparseMasterAxisValueCount);
  const auto &extremumAxisValueDeltas = StructAtOffset<ExtremumAxisValueDeltas> (this, extremumAxisValueDeltasOff4 * 4);
  hb_array_t<const HBFLOAT32LE> extremum_axis_value_deltas = extremumAxisValueDeltas.as_array (sparseExtremumAxisValueCount);

  unsigned count = master_axis_value_deltas.length;
  for (unsigned i = 0; i < count; i++)
    out_coords[masterRowIndex.arrayZ[i]] += (double) master_axis_value_deltas.arrayZ[i];

  unsigned axis_count = hb_min (axisCount, coords.length);
  for (unsigned axis_idx = 0; axis_idx < axis_count; axis_idx++)
  {
    double coord = coords.arrayZ[axis_idx];
    if (!coord) continue;
    bool pos = coord > 0.;
    unsigned column_idx = axis_idx * 2 + pos;

    unsigned sparse_row_start = extremumColumnStart.arrayZ[column_idx];
    unsigned sparse_row_end = extremumColumnStart.arrayZ[column_idx + 1];
    if (sparse_row_start == sparse_row_end)
      continue;

    double scalar = fabs (coord);
    sparse_row_end = hb_min (sparse_row_end, extremum_axis_value_deltas.length);
    for (unsigned row_idx = sparse_row_start; row_idx < sparse_row_end; row_idx++)
    {
      unsigned row = extremumRowIndex.arrayZ[row_idx];
      double delta = (double) extremum_axis_value_deltas.arrayZ[row_idx];
      out_coords[row] += delta * scalar;
    }
  }
}

void
PartComposite::apply_to_transforms (hb_array_t<hb_transform_t<double>> transforms,
				    hb_array_t<const double> coords) const
{
  const auto &allTranslations = StructAtOffset<AllTranslations> (this, allTranslationsOff4 * 4);
  const auto &masterTranslationDelta = allTranslations.masterTranslationDelta;
  const auto &extremumTranslationDelta = StructAfter<decltype (allTranslations.extremumTranslationDeltaX)> (masterTranslationDelta, sparseMasterTranslationCount);
  const auto &extremumTranslationIndex = StructAfter<decltype (allTranslations.extremumTranslationIndexX)> (extremumTranslationDelta, sparseExtremumTranslationCount);
  const auto &masterTranslationIndex = StructAfter<decltype (allTranslations.masterTranslationIndexX)> (extremumTranslationIndex, sparseExtremumTranslationCount);

  const auto &allRotations = StructAtOffset<AllRotations> (this, allRotationsOff4 * 4);
  const auto &masterRotationDelta = allRotations.masterRotationDelta;
  const auto &extremumRotationDelta = StructAfter<decltype (allRotations.extremumRotationDeltaX)> (masterRotationDelta, sparseMasterRotationCount);
  const auto &extremumRotationIndex = StructAfter<decltype (allRotations.extremumRotationIndexX)> (extremumRotationDelta, sparseExtremumRotationCount);
  const auto &masterRotationIndex = StructAfter<decltype (allRotations.masterRotationIndexX)> (extremumRotationIndex, sparseExtremumRotationCount);

  /* Note that the spec says walk four iterators together.
   * But with careful consideration, we have figured out the order
   * to walk two, then one, then one. This seems to work for all
   * glyphs in PingFangUI just fine.
   *
   * Moreover, for walking the two (extremum ones), if there is
   * no rotation, we use a separate, faster, loop that just walks
   * extremum translations.
   *
   * See the following commits:
   *
   *   [hvgl/transforms] Break up the four-iterator loop again
   *   [hvgl/transforms] Break up some more
   *   [hvgl] Fast-path when no extremum rotations are present
   *
   */

  auto extremum_translation_indices = extremumTranslationIndex.arrayZ;
  auto extremum_translation_deltas = extremumTranslationDelta.arrayZ;
  unsigned extremum_translation_count = sparseExtremumTranslationCount;
  auto extremum_rotation_indices = extremumRotationIndex.arrayZ;
  auto extremum_rotation_deltas = extremumRotationDelta.arrayZ;
  unsigned extremum_rotation_count = sparseExtremumRotationCount;

  if (!extremum_rotation_count)
  {
    for (unsigned i = 0; i < extremum_translation_count; i++)
    {
      unsigned column = extremum_translation_indices[i].column;

      unsigned axis_idx = column / 2;
      double coord = coords[axis_idx];
      if (!coord) continue;
      bool pos = column & 1;
      if (pos != (coord > 0)) continue;
      double scalar = fabs (coord);

      unsigned row = extremum_translation_indices[i].row;
      if (unlikely (row >= transforms.length)) break;

      transforms.arrayZ[row].translate ((double) extremum_translation_deltas[i].x * scalar,
					(double) extremum_translation_deltas[i].y * scalar,
					true);
    }
  }
  else
  {
    while (true)
    {
      unsigned row = transforms.length;
      if (extremum_translation_count)
	row = hb_min (row, extremum_translation_indices->row);
      if (extremum_rotation_count)
	row = hb_min (row, extremum_rotation_indices->row);
      if (row == transforms.length)
	break;

      hb_transform_t<double> transform;
      bool is_translate_only = true;

      while (true)
      {
	bool has_row_translation = extremum_translation_count &&
				   extremum_translation_indices->row == row;
	bool has_row_rotation = extremum_rotation_count &&
				extremum_rotation_indices->row == row;

	unsigned column = 2 * axisCount;
	if (has_row_translation)
	  column = hb_min (column, extremum_translation_indices->column);
	if (has_row_rotation)
	  column = hb_min (column, extremum_rotation_indices->column);
	if (column == 2 * axisCount)
	  break;

	const auto *extremum_translation_delta = &Null(TranslationDelta);
	double extremum_rotation_delta = 0.;

	if (has_row_translation &&
	    extremum_translation_indices->column == column)
	{
	  extremum_translation_delta = extremum_translation_deltas;
	  extremum_translation_count--;
	  extremum_translation_indices++;
	  extremum_translation_deltas++;
	}
	if (has_row_rotation &&
	    extremum_rotation_indices->column == column)
	{
	  extremum_rotation_delta = (double) *extremum_rotation_deltas;
	  extremum_rotation_count--;
	  extremum_rotation_indices++;
	  extremum_rotation_deltas++;
	}

	unsigned axis_idx = column / 2;
	double coord = coords[axis_idx];
	if (!coord) continue;
	bool pos = column & 1;
	if (pos != (coord > 0)) continue;
	double scalar = fabs (coord);

	if (extremum_rotation_delta)
	{
	  double center_x = (double) extremum_translation_delta->x;
	  double center_y = (double) extremum_translation_delta->y;
	  double angle = extremum_rotation_delta;
	  if (center_x || center_y)
	  {
	    // The paper has formula for this in terms of complex numbers.
	    // This is translated to real numbers, partly using ChatGPT.
	    double s, c;
	    hb_sincos ((double) angle, s, c);
	    double _1_minus_c = 1 - c;
	    if (likely (_1_minus_c))
	    {
	      double s_over_1_minus_c = s / _1_minus_c;

	      double new_center_x = (center_x - center_y * s_over_1_minus_c) * .5;
	      double new_center_y = (center_y + center_x * s_over_1_minus_c) * .5;

	      center_x = new_center_x;
	      center_y = new_center_y;
	    }
	  }
	  angle *= scalar;
	  transform.rotate_around_center (angle, center_x, center_y);
	  is_translate_only = false;
	}
	else
	{
	  // No rotation, just scale the translate
	  transform.translate ((double) extremum_translation_delta->x * scalar,
			       (double) extremum_translation_delta->y * scalar,
			       is_translate_only);
	}
      }

      if (is_translate_only)
	transforms.arrayZ[row].translate (transform.x0, transform.y0, true);
      else
	transforms.arrayZ[row].transform (transform, true);
    }
  }

  auto master_rotation_indices = masterRotationIndex.arrayZ;
  auto master_rotation_deltas = masterRotationDelta.arrayZ;
  unsigned master_rotation_count = sparseMasterRotationCount;
  for (unsigned i = 0; i < master_rotation_count; i++)
  {
    unsigned row = master_rotation_indices[i];
    if (unlikely (row >= transforms.length)) break;
    transforms.arrayZ[row].rotate ((double) master_rotation_deltas[i], true);
  }

  auto master_translation_indices = masterTranslationIndex.arrayZ;
  auto master_translation_deltas = masterTranslationDelta.arrayZ;
  unsigned master_translation_count = sparseMasterTranslationCount;
  for (unsigned i = 0; i < master_translation_count; i++)
  {
    unsigned row = master_translation_indices[i];
    if (unlikely (row >= transforms.length)) break;
    transforms.arrayZ[row].translate ((double) master_translation_deltas[i].x,
				      (double) master_translation_deltas[i].y,
				      true);
  }
}

void
PartComposite::get_path_at (const hb_hvgl_context_t *c,
			    hb_array_t<double> coords,
			    hb_array_t<hb_transform_t<double>> transforms) const
{
  const auto &subParts = StructAtOffset<SubParts> (this, subPartsOff4 * 4);

  coords = coords.sub_array (0, totalNumAxes);
  auto coords_head = coords.sub_array (0, axisCount);
  auto coords_tail = coords.sub_array (axisCount);

  apply_to_coords (coords_tail, coords_head);

  transforms = transforms.sub_array (0, totalNumParts);
  auto &transforms_head = transforms[0];
  auto transforms_tail = transforms.sub_array (1);

  apply_to_transforms (transforms_tail, coords_head);

  for (const auto &subPart : subParts.as_array (subPartCount))
  {
    auto &this_transform = transforms_tail[subPart.treeTransformIndex];

    if (likely (this_transform.is_translation ()))
    {
      double dx = this_transform.x0;
      double dy = this_transform.y0;
      this_transform = transforms_head;
      this_transform.translate (dx, dy);
    }
    else
      this_transform.transform (transforms_head, true);

    c->hvgl_table.get_part_path_at (c,
				    subPart.partIndex,
				    coords_tail.sub_array (subPart.treeAxisIndex),
				    transforms_tail.sub_array (subPart.treeTransformIndex));
  }
}


} // namespace hvgl_impl
} // namespace AAT

#endif
