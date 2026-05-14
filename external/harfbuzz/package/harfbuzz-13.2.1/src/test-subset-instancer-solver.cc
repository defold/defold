/*
 * Copyright © 2023  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Qunxin Liu
 */

#include <math.h>
#include "hb-subset-instancer-solver.hh"

static inline bool approx (Triple a, Triple b)
{
  return abs (a.minimum - b.minimum) < 0.000001 &&
         abs (a.middle - b.middle) < 0.000001 &&
         abs (a.maximum - b.maximum) < 0.000001;
}

static inline bool approx (double a, double b)
{ return abs (a - b) < 0.000001; }

/* tests ported from
 * https://github.com/fonttools/fonttools/blob/main/Tests/varLib/instancer/solver_test.py */
int
main (int argc, char **argv)
{
  TripleDistances default_axis_distances{1.0, 1.0};
  /* Case 1 */
  {
    /* pin axis*/
    Triple tent (0.0, 1.0, 1.0);
    Triple axis_range (0.0, 0.0, 0.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 0);
  }

  {
    /* pin axis*/
    Triple tent (0.0, 1.0, 1.0);
    Triple axis_range (0.5, 0.5, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 0.5);
    hb_always_assert (out[0].second == Triple ());
  }

  {
    /* tent falls outside the new axis range */
    Triple tent (0.3, 0.5, 0.8);
    Triple axis_range (0.1, 0.2, 0.3);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 0);
  }

  {
    /* malformed axis range should be rejected, not abort */
    Triple tent (0.3, 0.5, 0.8);
    Triple axis_range (0.4, 0.2, 0.3);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 0);
  }

  {
    /* malformed triple should not abort in renormalization */
    Triple malformed_range (0.4, 0.2, 0.3);
    double normalized = renormalizeValue (0.25, malformed_range, default_axis_distances, true);
    hb_always_assert (approx (normalized, 0.25));
  }

  /* Case 2 */
  {
    Triple tent (0.0, 1.0, 1.0);
    Triple axis_range (-1.0, 0.0, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 0.5);
    hb_always_assert (out[0].second == Triple (0.0, 1.0, 1.0));
  }

  /* Case 2 */
  {
    Triple tent (0.0, 1.0, 1.0);
    Triple axis_range (-1.0, 0.0, 0.75);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 0.75);
    hb_always_assert (out[0].second == Triple (0.0, 1.0, 1.0));
  }

  /* Without gain: */
  /* Case 3 */
  {
    Triple tent (0.0, 0.2, 1.0);
    Triple axis_range (-1.0, 0.0, 0.8);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 2);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (0.0, 0.25, 1.0));
    hb_always_assert (approx (out[1].first, 0.250));
    hb_always_assert (out[1].second == Triple (0.25, 1.0, 1.0));
  }

  /* Case 3 boundary */
  {
    Triple tent (0.0, 0.4, 1.0);
    Triple axis_range (-1.0, 0.0, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 2);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (0.0, 0.8, 1.0));
    hb_always_assert (approx (out[1].first, 2.5/3));
    hb_always_assert (out[1].second == Triple (0.8, 1.0, 1.0));
  }

  /* Case 4 */
  {
    Triple tent (0.0, 0.25, 1.0);
    Triple axis_range (-1.0, 0.0, 0.4);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 2);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (0.0, 0.625, 1.0));
    hb_always_assert (approx (out[1].first, 0.80));
    hb_always_assert (out[1].second == Triple (0.625, 1.0, 1.0));
  }

  /* Case 4 */
  {
    Triple tent (0.25, 0.3, 1.05);
    Triple axis_range (0.0, 0.2, 0.4);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 2);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (approx (out[0].second, Triple (0.25, 0.5, 1.0)));
    hb_always_assert (approx (out[1].first, 2.6 / 3));
    hb_always_assert (approx (out[1].second, Triple (0.5, 1.0, 1.0)));
  }

  /* Case 4 boundary */
  {
    Triple tent (0.25, 0.5, 1.0);
    Triple axis_range (0.0, 0.25, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (0.0, 1.0, 1.0));
  }

  /* With gain */
  /* Case 3a/1neg */
  {
    Triple tent (0.0, 0.5, 1.0);
    Triple axis_range (0.0, 0.5, 1.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 3);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == -1.0);
    hb_always_assert (out[1].second == Triple (0.0, 1.0, 1.0));
    hb_always_assert (out[2].first == -1.0);
    hb_always_assert (out[2].second == Triple (-1.0, -1.0, 0.0));
  }

  {
    Triple tent (0.0, 0.5, 1.0);
    Triple axis_range (0.0, 0.5, 0.75);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 3);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == -0.5);
    hb_always_assert (out[1].second == Triple (0.0, 1.0, 1.0));
    hb_always_assert (out[2].first == -1.0);
    hb_always_assert (out[2].second == Triple (-1.0, -1.0, 0.0));
  }

  {
    Triple tent (0.0, 0.50, 1.0);
    Triple axis_range (0.0, 0.25, 0.8);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 4);
    hb_always_assert (out[0].first == 0.5);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == 0.5);
    hb_always_assert (approx (out[1].second, Triple (0.0, 0.454545, 0.909091)));
    hb_always_assert (approx (out[2].first, -0.1));
    hb_always_assert (approx (out[2].second, Triple (0.909091, 1.0, 1.0)));
    hb_always_assert (out[3].first == -0.5);
    hb_always_assert (out[3].second == Triple (-1.0, -1.0, 0.0));
  }

  /* Case 3a/1neg */
  {
    Triple tent (0.0, 0.5, 2.0);
    Triple axis_range (0.2, 0.5, 0.8);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 3);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (approx (out[1].first, -0.2));
    hb_always_assert (out[1].second == Triple (0.0, 1.0, 1.0));
    hb_always_assert (approx (out[2].first, -0.6));
    hb_always_assert (out[2].second == Triple (-1.0, -1.0, 0.0));
  }

  /* Case 3a/1neg */
  {
    Triple tent (0.0, 0.5, 2.0);
    Triple axis_range (0.2, 0.5, 1.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 3);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (approx (out[1].first, -1.0/3));
    hb_always_assert (out[1].second == Triple (0.0, 1.0, 1.0));
    hb_always_assert (approx (out[2].first, -0.6));
    hb_always_assert (out[2].second == Triple (-1.0, -1.0, 0.0));
  }

  /* Case 3 */
  {
    Triple tent (0.0, 0.5, 1.0);
    Triple axis_range (0.25, 0.25, 0.75);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 2);
    hb_always_assert (out[0].first == 0.5);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == 0.5);
    hb_always_assert (out[1].second == Triple (0.0, 0.5, 1.0));
  }

  /* Case 1neg */
  {
    Triple tent (0.0, 0.5, 1.0);
    Triple axis_range (0.0, 0.25, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 3);
    hb_always_assert (out[0].first == 0.5);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == 0.5);
    hb_always_assert (out[1].second == Triple (0.0, 1.0, 1.0));
    hb_always_assert (out[2].first == -0.5);
    hb_always_assert (out[2].second == Triple (-1.0, -1.0, 0.0));
  }

  /* Case 2neg */
  {
    Triple tent (0.05, 0.55, 1.0);
    Triple axis_range (0.0, 0.25, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 4);
    hb_always_assert (approx (out[0].first, 0.4));
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (approx (out[1].first, 0.5));
    hb_always_assert (out[1].second == Triple (0.0, 1.0, 1.0));
    hb_always_assert (approx (out[2].first, -0.4));
    hb_always_assert (out[2].second == Triple (-1.0, -0.8, 0.0));
    hb_always_assert (approx (out[3].first, -0.4));
    hb_always_assert (out[3].second == Triple (-1.0, -1.0, -0.8));
  }

  /* Case 2neg, other side */
  {
    Triple tent (-1.0, -0.55, -0.05);
    Triple axis_range (-0.5, -0.25, 0.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 4);
    hb_always_assert (approx (out[0].first, 0.4));
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (approx (out[1].first, 0.5));
    hb_always_assert (out[1].second == Triple (-1.0, -1.0, 0.0));
    hb_always_assert (approx (out[2].first, -0.4));
    hb_always_assert (out[2].second == Triple (0.0, 0.8, 1.0));
    hb_always_assert (approx (out[3].first, -0.4));
    hb_always_assert (out[3].second == Triple (0.8, 1.0, 1.0));
  }

  /* Misc corner cases */
  {
    Triple tent (0.5, 0.5, 0.5);
    Triple axis_range (0.5, 0.5, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
  }

  {
    Triple tent (0.3, 0.5, 0.7);
    Triple axis_range (0.1, 0.5, 0.9);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 5);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == -1.0);
    hb_always_assert (approx(out[1].second, Triple (0.0, 0.5, 1.0)));
    hb_always_assert (out[2].first == -1.0);
    hb_always_assert (approx(out[2].second, Triple (0.5, 1.0, 1.0)));
    hb_always_assert (out[3].first == -1.0);
    hb_always_assert (approx (out[3].second, Triple (-1.0, -0.5, 0.0)));
    hb_always_assert (out[4].first == -1.0);
    hb_always_assert (approx (out[4].second, Triple (-1.0, -1.0, -0.5)));
  }

  {
    Triple tent (0.5, 0.5, 0.5);
    Triple axis_range (0.25, 0.25, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (1.0, 1.0, 1.0));
  }

  {
    Triple tent (0.5, 0.5, 0.5);
    Triple axis_range (0.25, 0.35, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (1.0, 1.0, 1.0));
  }

  {
    Triple tent (0.5, 0.5, 0.55);
    Triple axis_range (0.25, 0.35, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (1.0, 1.0, 1.0));
  }

  {
    Triple tent (0.5, 0.5, 1.0);
    Triple axis_range (0.5, 0.5, 1.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 2);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == -1.0);
    hb_always_assert (out[1].second == Triple (0.0, 1.0, 1.0));
  }

  {
    Triple tent (0.25, 0.5, 1.0);
    Triple axis_range (0.5, 0.5, 1.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 2);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == -1.0);
    hb_always_assert (out[1].second == Triple (0.0, 1.0, 1.0));
  }

  {
    Triple tent (0.0, 0.2, 1.0);
    Triple axis_range (0.0, 0.0, 0.5);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 2);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (0.0, 0.4, 1.0));
    hb_always_assert (out[1].first == 0.625);
    hb_always_assert (out[1].second == Triple (0.4, 1.0, 1.0));
  }


  {
    Triple tent (0.0, 0.5, 1.0);
    Triple axis_range (-1.0, 0.25, 1.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 5);
    hb_always_assert (out[0].first == 0.5);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == 0.5);
    hb_always_assert (out[1].second == Triple (0.0, 1.0/3, 2.0/3));
    hb_always_assert (out[2].first == -0.5);
    hb_always_assert (out[2].second == Triple (2.0/3, 1.0, 1.0));
    hb_always_assert (out[3].first == -0.5);
    hb_always_assert (out[3].second == Triple (-1.0, -0.2, 0.0));
    hb_always_assert (out[4].first == -0.5);
    hb_always_assert (out[4].second == Triple (-1.0, -1.0, -0.2));
  }

  {
    Triple tent (0.5, 0.5, 0.5);
    Triple axis_range (0.0, 0.5, 1.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 5);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple ());
    hb_always_assert (out[1].first == -1.0);
    hb_always_assert (out[1].second == Triple (0.0, 2/(double) (1 << 14), 1.0));
    hb_always_assert (out[2].first == -1.0);
    hb_always_assert (out[2].second == Triple (2/(double) (1 << 14), 1.0, 1.0));
    hb_always_assert (out[3].first == -1.0);
    hb_always_assert (out[3].second == Triple (-1.0, -2/(double) (1 << 14), 0.0));
    hb_always_assert (out[4].first == -1.0);
    hb_always_assert (out[4].second == Triple (-1.0, -1.0, -2/(double) (1 << 14)));
  }

  {
    Triple tent (0.0, 1.0, 1.0);
    Triple axis_range (-1.0, -0.5, 1.0);
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, default_axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (1.0/3, 1.0, 1.0));
  }

  {
    Triple tent (0.0, 1.0, 1.0);
    Triple axis_range (-1.0, -0.5, 1.0);
    TripleDistances axis_distances{2.0, 1.0};
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (out[0].second == Triple (0.5, 1.0, 1.0));
  }

  {
    Triple tent (0.6, 0.7, 0.8);
    Triple axis_range (-1.0, 0.2, 1.0);
    TripleDistances axis_distances{1.0, 1.0};
    rebase_tent_result_t out, scratch;
    rebase_tent (tent, axis_range, axis_distances, out, scratch);
    hb_always_assert (out.length == 1);
    hb_always_assert (out[0].first == 1.0);
    hb_always_assert (approx (out[0].second, Triple (0.5, 0.625, 0.75)));
  }
}
