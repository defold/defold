/*
 * Copyright © 2025 Behdad Esfahbod
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
 * Author(s): Behdad Esfahbod
 */

#include "hb.hh"
#include "hb-decycler.hh"

static void
tree_recurse_binary (unsigned value,
		     unsigned max_value,
		     hb_decycler_t &decycler)
{
  if (value >= max_value)
		return;

  hb_decycler_node_t node (decycler);

  bool ret = node.visit (value);
  hb_always_assert (ret);

  tree_recurse_binary (value * 2 + 1, max_value, decycler);
  tree_recurse_binary (value * 2 + 2, max_value, decycler);
}

static void
tree_recurse_tertiary (unsigned value,
		       unsigned max_value,
		       hb_decycler_t &decycler)
{
  /* This function implements an alternative way to use the
   * decycler. It checks for each node before visiting it.
   * It demonstrates reusing a node for multiple visits. */

  if (value >= max_value)
    return;

  hb_decycler_node_t node (decycler);

  value *= 3;

  for (unsigned i = 1; i <= 3; i++)
  {
    bool ret = node.visit (value + i);
    hb_always_assert (ret);

    tree_recurse_tertiary (value + i, max_value, decycler);
  }
}

static void
test_tree ()
{
  hb_decycler_t decycler;
  tree_recurse_binary (0, 64, decycler);
  tree_recurse_tertiary (0, 1000, decycler);
}

static void
cycle_recurse (signed value,
	       signed cycle_length,
	       hb_decycler_t &decycler)
{
  hb_always_assert (cycle_length > 0);

  hb_decycler_node_t node (decycler);

  if (!node.visit (value))
    return;

  if (value >= cycle_length)
    value = value % cycle_length;

  cycle_recurse (value + 1, cycle_length, decycler);
}

static void
test_cycle ()
{
  hb_decycler_t decycler;
  cycle_recurse (0, 1, decycler);
  cycle_recurse (0, 2, decycler);
  cycle_recurse (2, 3, decycler);
  cycle_recurse (-20, 8, decycler);
}

int
main (int argc, char **argv)
{
  test_tree ();
  test_cycle ();

  return 0;
}

