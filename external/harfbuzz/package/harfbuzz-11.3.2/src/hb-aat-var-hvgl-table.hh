#ifndef HB_AAT_VAR_HVGL_TABLE_HH
#define HB_AAT_VAR_HVGL_TABLE_HH

#include "hb-bit-vector.hh"
#include "hb-draw.hh"
#include "hb-geometry.hh"
#include "hb-ot-var-common.hh"

/*
 * `hvgl` table
 */

#ifndef HB_NO_VAR_HVF

#define HB_AAT_TAG_hvgl HB_TAG('h','v','g','l')


namespace AAT {

using namespace OT;

struct hb_hvgl_scratch_t
{
  hb_vector_t<double> coords_f;
  hb_vector_t<hb_transform_t<double>> transforms;
  hb_vector_t<double> points;
};

using hb_hvgl_parts_sanitized_t = hb_bit_vector_t<true>;

struct hvgl;

struct hb_hvgl_context_t
{
  const hvgl &hvgl_table;
  hb_draw_session_t *draw_session;
  hb_extents_t<> *extents;
  hb_hvgl_scratch_t &scratch;
  hb_sanitize_context_t &sanitizer;
  hb_hvgl_parts_sanitized_t &parts_sanitized;
  mutable signed nodes_left;
  mutable signed edges_left;
  mutable signed depth_left;
};

namespace hvgl_impl {

struct coordinates_t
{
  friend struct PartShape;

  public:

  unsigned get_size (unsigned segment_count) const
  { return coords.get_size (4 * segment_count); }

  hb_array_t<const HBFLOAT64LE>
  get_coords(unsigned segment_count) const
  { return coords.as_array (segment_count * 4); }

  bool sanitize (hb_sanitize_context_t *c,
		 unsigned segment_count) const
  {
    TRACE_SANITIZE (this);
    unsigned count;
    return_trace (!hb_unsigned_mul_overflows (segment_count, 4, &count) &&
		  coords.sanitize (c, count));
  }

  protected:
  UnsizedArrayOf<HBFLOAT64LE> coords; // length: SegmentCount * 4

  public:
  DEFINE_SIZE_MIN (0);
};

struct deltas_t
{
  public:

  unsigned get_size (unsigned axis_count, unsigned segment_count) const
  { return matrix.get_size (2 * 4 * axis_count * segment_count); }

  hb_array_t<const HBFLOAT64LE>
  get_column(unsigned column_index, unsigned axis_count, unsigned segment_count) const
  {
    const unsigned rows = 4 * segment_count;
    const unsigned start = column_index * rows;
    return get_matrix (axis_count, segment_count).sub_array (start, rows);
  }

  hb_array_t<const HBFLOAT64LE>
  get_matrix(unsigned axis_count, unsigned segment_count) const
  { return matrix.as_array (2 * 4 * axis_count * segment_count); }

  bool sanitize (hb_sanitize_context_t *c,
		 unsigned axis_count,
		 unsigned segment_count) const
  {
    TRACE_SANITIZE (this);
    unsigned count;
    return_trace (!hb_unsigned_mul_overflows (2 * 4, axis_count, &count) &&
		  !hb_unsigned_mul_overflows (count, segment_count, &count) &&
		  matrix.sanitize (c, count));
  }

  protected:
  UnsizedArrayOf<HBFLOAT64LE> matrix; // column-major: AxisCount * 2 columns, DeltaSegmentCount * 4 rows

  public:
  DEFINE_SIZE_MIN (0);
};

struct PartShape
{
  public:

  unsigned get_total_num_axes () const
  { return axisCount; }

  unsigned get_total_num_parts () const
  { return 1; }

  HB_INTERNAL void
  get_path_at (const hb_hvgl_context_t *c,
	       hb_array_t<const double> coords,
	       hb_array_t<hb_transform_t<double>> transforms) const;

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this))) return_trace (false);
    hb_barrier ();

    const auto &blendTypes = StructAfter<decltype (blendTypesX)> (segmentCountPerPath, pathCount);
    const auto &padding = StructAfter<decltype (paddingX)> (blendTypes, segmentCount);

    if (unlikely (!(((const char *) this <= (const char *) &padding) &&
		    c->check_range (this, (unsigned ((const char *) &padding - (const char *) this))))))
      return_trace (false);

    const auto &coordinates = StructAfter<decltype (coordinatesX)> (padding, this);
    if (unlikely (!coordinates.sanitize (c, segmentCount))) return_trace (false);

    const auto &deltas = StructAfter<decltype (deltasX)> (coordinates, segmentCount);
    if (unlikely (!deltas.sanitize (c, axisCount, segmentCount))) return_trace (false);

    return_trace (true);
  }

  protected:
  HBUINT16LE flags; // 0x0001 for shape part
  HBUINT16LE axisCount;
  HBUINT16LE pathCount;
  HBUINT16LE segmentCount;
  UnsizedArrayOf<HBUINT16LE> segmentCountPerPath; // length: PathCount; Sizes of paths
  UnsizedArrayOf<HBUINT8> blendTypesX; // length: SegmentCount; Blend types for all segments
  Align<8> paddingX; // Pad to float64le alignment
  coordinates_t coordinatesX; // Master coordinate vector
  deltas_t deltasX; // Delta coordinate matrix
  public:
  DEFINE_SIZE_MIN (8);
};

struct SubPart
{
  public:

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBUINT32LE partIndex; // Index of part that this subpart renders
  HBUINT16LE treeTransformIndex; // Row index of data in transform vector/matrix
  HBUINT16LE treeAxisIndex; // Row index of data in axis vector/matrix

  public:
  DEFINE_SIZE_STATIC (8);
};

using SubParts = UnsizedArrayOf<SubPart>; // length: subPartCount. Immediate subparts

struct ExtremumColumnStarts
{
  friend struct PartComposite;

  public:

  bool sanitize (hb_sanitize_context_t *c,
		 unsigned axis_count,
		 unsigned sparse_master_axis_value_count,
		 unsigned sparse_extremum_axis_value_count) const
  {
    TRACE_SANITIZE (this);

    if (unlikely (!extremumColumnStart.sanitize (c, axis_count))) return_trace (false);

    unsigned count;
    if (unlikely (hb_unsigned_mul_overflows (axis_count, 2, &count))) return_trace (false);
    if (unlikely (count + 1 < count)) return_trace (false);
    count++;
    const auto &masterRowIndex = StructAfter<decltype (masterRowIndexX)> (extremumColumnStart, count);
    if (unlikely (!masterRowIndex.sanitize (c, sparse_master_axis_value_count))) return_trace (false);

    const auto &extremumRowIndex = StructAfter<decltype (extremumRowIndexX)> (masterRowIndex, sparse_master_axis_value_count);
    if (unlikely (!extremumRowIndex.sanitize (c, sparse_extremum_axis_value_count))) return_trace (false);

    return_trace (true);
  }

  protected:
  UnsizedArrayOf<HBUINT16LE> extremumColumnStart; // length: axisCount; Extremum column starts
  UnsizedArrayOf<HBUINT16LE> masterRowIndexX; // length: sparseMasterAxisValueCount; Master row indices
  UnsizedArrayOf<HBUINT16LE> extremumRowIndexX; // length: sparseExtremumAxisValueCount; Extremum row indices
  Align<4> paddingX; // Pad to uint32le alignment

  public:
  DEFINE_SIZE_MIN (0);
};

using MasterAxisValueDeltas = UnsizedArrayOf<HBFLOAT32LE>; // length: sparseMasterAxisValueCount. Master axis value deltas

using ExtremumAxisValueDeltas = UnsizedArrayOf<HBFLOAT32LE>; // length: sparseExtremumAxisValueCount. Extremum axis value deltas

struct TranslationDelta
{
  public:

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBFLOAT32LE x; // Translation delta X
  HBFLOAT32LE y; // Translation delta Y

  public:
  DEFINE_SIZE_STATIC (8);
};

struct MatrixIndex
{
  public:

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBUINT16LE row; // Row index
  HBUINT16LE column; // Column index

  public:
  DEFINE_SIZE_STATIC (4);
};

struct AllTranslations
{
  friend struct PartComposite;

  public:

  bool sanitize (hb_sanitize_context_t *c,
		 unsigned sparse_master_translation_count,
		 unsigned sparse_extremum_translation_count) const
  {
    TRACE_SANITIZE (this);

    const auto &extremumTranslationDelta = StructAfter<decltype (extremumTranslationDeltaX)> (masterTranslationDelta, sparse_master_translation_count);
    const auto &extremumTranslationIndex = StructAfter<decltype (extremumTranslationIndexX)> (extremumTranslationDelta, sparse_extremum_translation_count);
    const auto &masterTranslationIndex = StructAfter<decltype (masterTranslationIndexX)> (extremumTranslationIndex, sparse_extremum_translation_count);
    const auto &padding = StructAfter<decltype (paddingX)> (masterTranslationIndex, sparse_master_translation_count);

    return_trace (((const char *) this <= (const char *) &padding) &&
		  c->check_range (this, (unsigned ((const char *) &padding - (const char *) this))));
  }

  protected:
  UnsizedArrayOf<TranslationDelta> masterTranslationDelta; // length: sparse_master_translation_count; Master translation deltas
  UnsizedArrayOf<TranslationDelta> extremumTranslationDeltaX; // length: sparse_extremum_translation_count; Extremum translation deltas
  UnsizedArrayOf<MatrixIndex> extremumTranslationIndexX; // length: sparse_extremum_translation_count; Extremum translation indices
  UnsizedArrayOf<HBUINT16LE> masterTranslationIndexX; // length: sparse_master_translation_count; Master translation indices
  Align<4> paddingX; // Pad to float32le alignment

  public:
  DEFINE_SIZE_MIN (0);
};

struct AllRotations
{
  friend struct PartComposite;

  public:

  bool sanitize (hb_sanitize_context_t *c,
		 unsigned sparse_master_rotation_count,
		 unsigned sparse_extremum_rotation_count) const
  {
    TRACE_SANITIZE (this);

    const auto &extremumRotationDelta = StructAfter<decltype (extremumRotationDeltaX)> (masterRotationDelta, sparse_master_rotation_count);
    const auto &extremumRotationIndex = StructAfter<decltype (extremumRotationIndexX)> (extremumRotationDelta, sparse_extremum_rotation_count);
    const auto &masterRotationIndex = StructAfter<decltype (masterRotationIndexX)> (extremumRotationIndex, sparse_extremum_rotation_count);
    const auto &padding = StructAfter<decltype (paddingX)> (masterRotationIndex, sparse_master_rotation_count);

    return_trace (((const char *) this <= (const char *) &padding) &&
		  c->check_range (this, (unsigned ((const char *) &padding - (const char *) this))));
  }

  protected:
  UnsizedArrayOf<HBFLOAT32LE> masterRotationDelta; // length: sparse_master_rotation_count; Master rotation deltas
  UnsizedArrayOf<HBFLOAT32LE> extremumRotationDeltaX; // length: sparse_extremum_rotation_count; Extremum rotation deltas
  UnsizedArrayOf<MatrixIndex> extremumRotationIndexX; // length: sparse_extremum_rotation_count; Extremum rotation indices
  UnsizedArrayOf<HBUINT16LE> masterRotationIndexX; // length: sparse_master_rotation_count; Master rotation indices
  Align<4> paddingX; // Pad to float32le alignment

  public:
  DEFINE_SIZE_MIN (0);
};

using Offset16LEMul4NN = Offset<HBUINT16LE, false>;

struct PartComposite
{
  public:

  HB_INTERNAL void apply_to_coords (hb_array_t<double> out_coords,
				    hb_array_t<const double> coords) const;

  HB_INTERNAL void apply_to_transforms (hb_array_t<hb_transform_t<double>> transforms,
					hb_array_t<const double> coords) const;

  unsigned get_total_num_axes () const
  { return totalNumAxes; }

  unsigned get_total_num_parts () const
  { return totalNumParts; }

  HB_INTERNAL void
  get_path_at (const hb_hvgl_context_t *c,
	       hb_array_t<double> coords,
	       hb_array_t<hb_transform_t<double>> transforms) const;

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);

    if (unlikely (!c->check_struct (this))) return_trace (false);

    const auto &subParts = StructAtOffset<SubParts> (this, subPartsOff4 * 4);
    const auto &extremumColumnStarts = StructAtOffset<ExtremumColumnStarts> (this, extremumColumnStartsOff4 * 4);
    const auto &masterAxisValueDeltas = StructAtOffset<MasterAxisValueDeltas> (this, masterAxisValueDeltasOff4 * 4);
    const auto &extremumAxisValueDeltas = StructAtOffset<ExtremumAxisValueDeltas> (this, extremumAxisValueDeltasOff4 * 4);
    const auto &allTranslations = StructAtOffset<AllTranslations> (this, allTranslationsOff4 * 4);
    const auto &allRotations = StructAtOffset<AllRotations> (this, allRotationsOff4 * 4);

    return_trace (likely (subParts.sanitize (c, subPartCount) &&
			  extremumColumnStarts.sanitize (c, axisCount, sparseMasterAxisValueCount, sparseExtremumAxisValueCount) &&
			  masterAxisValueDeltas.sanitize (c, sparseMasterAxisValueCount) &&
			  extremumAxisValueDeltas.sanitize (c, sparseExtremumAxisValueCount) &&
			  allTranslations.sanitize (c, sparseMasterTranslationCount, sparseExtremumTranslationCount) &&
			  allRotations.sanitize (c, sparseMasterRotationCount, sparseExtremumRotationCount)));
  }

  protected:
  HBUINT16LE flags; // 0x0001 for composite part
  HBUINT16LE axisCount; // Number of axes
  HBUINT16LE subPartCount; // Number of direct subparts
  HBUINT16LE totalNumParts; // Number of nodes including root
  HBUINT16LE totalNumAxes; // Sum of axis count for all nodes including root
  HBUINT16LE maxNumExtremes; // Maximum number of extremes (2*AxisCount) in all nodes
  HBUINT16LE sparseMasterAxisValueCount; // Count of non-zero axis value deltas for master
  HBUINT16LE sparseExtremumAxisValueCount; // Count of non-zero axis value deltas for extrema
  HBUINT16LE sparseMasterTranslationCount; // Count of non-zero translations for master
  HBUINT16LE sparseMasterRotationCount; // Count of non-zero rotations for master
  HBUINT16LE sparseExtremumTranslationCount; // Count of non-zero translations for extrema
  HBUINT16LE sparseExtremumRotationCount; // Count of non-zero rotations for extrema
  Offset16LEMul4NN/*To<SubParts>*/ subPartsOff4; // Offset to subpart array/4
  Offset16LEMul4NN/*To<ExtremumColumnStarts>*/ extremumColumnStartsOff4; // Offset to extremum column starts/4
  Offset16LEMul4NN/*To<MasterAxisValueDeltas>*/ masterAxisValueDeltasOff4; // Offset to master axis value deltas/4
  Offset16LEMul4NN/*To<ExtremumAxisValueDeltas>*/ extremumAxisValueDeltasOff4; // Offset to extremum axis value deltas/4
  Offset16LEMul4NN/*To<AllTranslations>*/ allTranslationsOff4; // Offset to all translations/4
  Offset16LEMul4NN/*To<AllRotations>*/ allRotationsOff4; // Offset to all rotations/4

  public:
  DEFINE_SIZE_STATIC (36);
};

struct Part
{
  public:

  unsigned get_total_num_axes () const
  {
    switch (u.flags & 1) {
    case 0: hb_barrier(); return u.shape.get_total_num_axes ();
    case 1: hb_barrier(); return u.composite.get_total_num_axes ();
    default: return 0;
    }
  }

  unsigned get_total_num_parts () const
  {
    switch (u.flags & 1) {
    case 0: hb_barrier(); return u.shape.get_total_num_parts ();
    case 1: hb_barrier(); return u.composite.get_total_num_parts ();
    default: return 0;
    }
  }

  void
  get_path_at (const hb_hvgl_context_t *c,
	       hb_array_t<double> coords,
	       hb_array_t<hb_transform_t<double>> transforms) const
  {
    switch (u.flags & 1) {
    case 0: hb_barrier(); u.shape.get_path_at (c, coords, transforms); break;
    case 1: hb_barrier(); u.composite.get_path_at (c, coords, transforms); break;
    }
  }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    if (unlikely (!c->may_dispatch (this, &u.flags))) return c->no_dispatch_return_value ();
    TRACE_DISPATCH (this, u.flags);
    switch (u.flags & 1) {
    case 0: hb_barrier (); return_trace (c->dispatch (u.shape, std::forward<Ts> (ds)...));
    case 1: hb_barrier (); return_trace (c->dispatch (u.composite, std::forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  protected:
  union {
  HBUINT16LE	flags;	/* Flag identifier */
  PartShape	shape;
  PartComposite	composite;
  } u;
  public:
  // A null flags will imply PartShape. So, our null size is the size of PartShape::min_size.
  DEFINE_SIZE_MIN (PartShape::min_size);
};

struct Index
{
  public:

  hb_bytes_t get (unsigned index, unsigned count) const
  {
    if (unlikely (index >= count)) return hb_bytes_t ();
    hb_barrier ();
    unsigned offset0 = offsets[index];
    unsigned offset1 = offsets[index + 1];
    if (unlikely (offset1 < offset0 || offset1 > offsets[count]))
      return hb_bytes_t ();
    return hb_bytes_t ((const char *) this + offset0, offset1 - offset0);
  }

  unsigned int get_size (unsigned count) const { return offsets[count]; }

  bool sanitize (hb_sanitize_context_t *c, unsigned count) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (count < count + 1u &&
			  offsets.sanitize (c, count + 1) &&
			  hb_barrier () &&
			  offsets[count] >= offsets.get_size (count + 1) &&
			  c->check_range (this, offsets[count])));
  }

  protected:
  UnsizedArrayOf<HBUINT32LE>	offsets;	/* The array of (count + 1) offsets. */
  public:
  DEFINE_SIZE_MIN (4);
};

template <typename Type>
struct IndexOf : Index
{
  public:

  const Type &get (unsigned index, unsigned count,
		   hb_sanitize_context_t &c, hb_hvgl_parts_sanitized_t &parts_sanitized) const
  {
    if (unlikely (index >= count)) return Null(Type);

    hb_bytes_t data = Index::get (index, count);
    const Type &item = *reinterpret_cast<const Type *> (data.begin ());

    bool sanitized = parts_sanitized.has (index);
    if (unlikely (!sanitized))
    {
      c.start_processing (data.begin (), data.end ());
      bool sane = c.dispatch (item);
      c.end_processing ();
      if (unlikely (!sane)) return Null(Type);
    }
    parts_sanitized.add (index);

    return item;
  }

  bool sanitize (hb_sanitize_context_t *c, unsigned count) const
  { return Index::sanitize (c, count); }
};

using PartsIndex = IndexOf<Part>;

} // namespace hvgl_impl

struct hvgl
{
  friend struct hvgl_impl::PartComposite;

  static constexpr hb_tag_t tableTag = HB_TAG ('h', 'v', 'g', 'l');

  protected:

  bool
  get_part_path_at (const hb_hvgl_context_t *c,
		    hb_codepoint_t part_id,
		    hb_array_t<double> coords,
		    hb_array_t<hb_transform_t<double>> transforms) const
  {
    if (unlikely (c->edges_left <= 0))
      return true;
    c->edges_left--;

    if (unlikely (c->nodes_left <= 0))
      return true;
    c->nodes_left--;

    if (unlikely (c->depth_left <= 0))
      return true;
    c->depth_left--;

    const auto &parts = StructAtOffset<hvgl_impl::PartsIndex> (this, partsOff);
    const auto &part = parts.get (part_id, partCount, c->sanitizer, c->parts_sanitized);

    part.get_path_at (c, coords, transforms);

    c->depth_left++;

    return true;
  }

  public:

  bool
  get_path_at (hb_font_t *font,
	       hb_codepoint_t gid,
	       hb_draw_session_t *draw_session,
	       hb_extents_t<> *extents,
	       hb_array_t<const int> coords,
	       hb_hvgl_scratch_t &scratch,
	       hb_hvgl_parts_sanitized_t &parts_sanitized) const
  {
    if (unlikely (gid >= numGlyphs)) return false;

    hb_sanitize_context_t sanitizer;

    const auto &parts = StructAtOffset<hvgl_impl::PartsIndex> (this, partsOff);
    const auto &part = parts.get (gid, partCount, sanitizer, parts_sanitized);

    auto &coords_f = scratch.coords_f;
    coords_f.clear ();
    coords_f.resize (part.get_total_num_axes ());
    if (unlikely (coords_f.in_error ())) return true;
    unsigned count = hb_min (coords.length, coords_f.length);
    for (unsigned i = 0; i < count; i++)
      coords_f.arrayZ[i] = double (coords.arrayZ[i]) * (1. / (1 << 14));

    auto &transforms = scratch.transforms;
    unsigned total_num_parts = part.get_total_num_parts ();
    transforms.clear ();
    transforms.resize (total_num_parts);
    if (unlikely (transforms.in_error ())) return true;
    transforms[0] = hb_transform_t<double>{(double) font->x_multf, 0, 0, (double) font->y_multf, 0, 0};

    hb_hvgl_context_t c = {*this, draw_session, extents,
			   scratch,
			   sanitizer, parts_sanitized,
			   (int) total_num_parts, HB_MAX_GRAPH_EDGE_COUNT, HB_MAX_NESTING_LEVEL};

    scratch.points.alloc (128);

    return get_part_path_at (&c, gid, coords_f, transforms);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this))) return_trace (false);
    hb_barrier ();

    if (unlikely (versionMajor != 3 ||
		  (versionMajor == 3 && versionMinor < 1))) return_trace (false);


    const auto &parts = StructAtOffset<hvgl_impl::PartsIndex> (this, partsOff);
    if (unlikely (!parts.sanitize (c, (unsigned) partCount))) return_trace (false);

    return_trace (true);
  }

  struct accelerator_t
  {
    accelerator_t (hb_face_t *face)
      : table (hb_sanitize_context_t ().reference_table<hvgl> (face)),
        parts_sanitized (0, table->partCount) {}
    ~accelerator_t ()
    {
      auto *scratch = cached_scratch.get_relaxed ();
      if (scratch)
      {
	scratch->~hb_hvgl_scratch_t ();
	hb_free (scratch);
      }

      table.destroy ();
    }

    bool
    get_path_at (hb_font_t *font,
		 hb_codepoint_t gid,
		 hb_draw_session_t &draw_session,
		 hb_array_t<const int> coords) const
    {
      if (!table->has_data ()) return false;

      hb_hvgl_scratch_t *scratch = acquire_scratch ();
      if (unlikely (!scratch)) return true;
      bool ret = table->get_path_at (font, gid,
				     &draw_session, nullptr,
				     coords,
				     *scratch,
				     parts_sanitized);
      release_scratch (scratch);

      return ret;
    }

    bool
    get_path (hb_font_t *font,
	      hb_codepoint_t gid,
	      hb_draw_session_t &draw_session) const
    {
      return get_path_at (font, gid, draw_session, hb_array (font->coords, font->num_coords));
    }

    bool
    get_extents_at (hb_font_t *font,
		    hb_codepoint_t gid,
		    hb_glyph_extents_t *extents,
		    hb_array_t<const int> coords) const
    {
      if (!table->has_data ()) return false;

      hb_extents_t<> f_extents;

      auto *scratch = acquire_scratch ();
      if (unlikely (!scratch)) return true;
      bool ret = table->get_path_at (font, gid,
				     nullptr, &f_extents,
				     coords,
				     *scratch,
				     parts_sanitized);
      release_scratch (scratch);

      if (ret)
	*extents = f_extents.to_glyph_extents (font->x_scale < 0, font->y_scale < 0);

      return ret;
    }

    bool
    get_extents (hb_font_t *font,
		 hb_codepoint_t gid,
		 hb_glyph_extents_t *extents) const
    {
      return get_extents_at (font, gid, extents,
			     hb_array (font->coords, font->num_coords));
    }

    private:

    hb_hvgl_scratch_t *acquire_scratch () const
    {
      hb_hvgl_scratch_t *scratch = cached_scratch.get_acquire ();

      if (!scratch || unlikely (!cached_scratch.cmpexch (scratch, nullptr)))
      {
	scratch = (hb_hvgl_scratch_t *) hb_calloc (1, sizeof (hb_hvgl_scratch_t));
	if (unlikely (!scratch))
	  return nullptr;
      }

      return scratch;
    }
    void release_scratch (hb_hvgl_scratch_t *scratch) const
    {
      if (!cached_scratch.cmpexch (nullptr, scratch))
      {
	scratch->~hb_hvgl_scratch_t ();
	hb_free (scratch);
      }
    }

    private:
    hb_blob_ptr_t<hvgl> table;
    mutable hb_atomic_t<hb_hvgl_scratch_t *> cached_scratch;
    mutable hb_hvgl_parts_sanitized_t parts_sanitized;
  };

  bool has_data () const { return versionMajor != 0; }

  protected:
  HBUINT16LE	versionMajor;	/* Major version of the hvgl table, currently 3 */
  HBUINT16LE	versionMinor;	/* Minor version of the hvgl table, currently 1 */
  HBUINT32LE	flags;		/* Flags; currently all zero */
  HBUINT32LE	partCount;	/* Number of all shapes and composites */
  Offset<HBUINT32LE>  partsOff;	/* To: Index. length: partCount. Parts */
  HBUINT32LE	numGlyphs;	/* Number of externally visible parts */
  HBUINT32LE	reserved;	/* Reserved; currently zero */
  public:
  DEFINE_SIZE_STATIC (24);
};

struct hvgl_accelerator_t : hvgl::accelerator_t {
  hvgl_accelerator_t (hb_face_t *face) : hvgl::accelerator_t (face) {}
};

} // namespace AAT

#endif // HB_NO_VAR_HVF

#endif  /* HB_AAT_VAR_HVGL_TABLE_HH */
