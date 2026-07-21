/*
 * Copyright © 2021  Behdad Esfahbod
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
 */

#include "hb.hh"
#include "hb-map.hh"
#include "hb-set.hh"
#include <string>

int
main (int argc, char **argv)
{

  /* Test copy constructor. */
  {
    hb_map_t v1;
    v1.set (1, 2);
    hb_map_t v2 {v1};
    hb_always_assert (v1.get_population () == 1);
    hb_always_assert (v2.get_population () == 1);
    hb_always_assert (v1[1] == 2);
    hb_always_assert (v2[1] == 2);
  }

  /* Test copy assignment. */
  {
    hb_map_t v1;
    v1.set (1, 2);
    hb_map_t v2 = v1;
    hb_always_assert (v1.get_population () == 1);
    hb_always_assert (v2.get_population () == 1);
    hb_always_assert (v1[1] == 2);
    hb_always_assert (v2[1] == 2);
  }

  /* Test move constructor. */
  {
    hb_map_t s {};
    s.set (1, 2);
    hb_map_t v (std::move (s));
    hb_always_assert (s.get_population () == 0);
    hb_always_assert (v.get_population () == 1);
  }

  /* Test move assignment. */
  {
    hb_map_t s {};
    s.set (1, 2);
    hb_map_t v;
    v = std::move (s);
    hb_always_assert (s.get_population () == 0);
    hb_always_assert (v.get_population () == 1);
  }

  /* Test initializing from iterable. */
  {
    hb_map_t s;

    s.set (1, 2);
    s.set (3, 4);

    hb_vector_t<hb_codepoint_pair_t> v (s);
    hb_map_t v0 (v);
    hb_map_t v1 (s);
    hb_map_t v2 (std::move (s));

    hb_always_assert (s.get_population () == 0);
    hb_always_assert (v0.get_population () == 2);
    hb_always_assert (v1.get_population () == 2);
    hb_always_assert (v2.get_population () == 2);
  }

  /* Test call fini() twice. */
  {
    hb_map_t s;
    for (int i = 0; i < 16; i++)
      s.set(i, i+1);
    s.fini();
  }

  /* Test initializing from iterator. */
  {
    hb_map_t s;

    s.set (1, 2);
    s.set (3, 4);

    hb_map_t v (hb_iter (s));

    hb_always_assert (v.get_population () == 2);
  }

  /* Test initializing from initializer list and swapping. */
  {
    using pair_t = hb_codepoint_pair_t;
    hb_map_t v1 {pair_t{1,2}, pair_t{4,5}};
    hb_map_t v2 {pair_t{3,4}};
    hb_swap (v1, v2);
    hb_always_assert (v1.get_population () == 1);
    hb_always_assert (v2.get_population () == 2);
  }

  /* Test class key / value types. */
  {
    hb_hashmap_t<hb_bytes_t, int> m1;
    hb_hashmap_t<int, hb_bytes_t> m2;
    hb_hashmap_t<hb_bytes_t, hb_bytes_t> m3;
    hb_always_assert (m1.get_population () == 0);
    hb_always_assert (m2.get_population () == 0);
    hb_always_assert (m3.get_population () == 0);
  }

  {
    hb_hashmap_t<int, int> m0;
    hb_hashmap_t<std::string, int> m1;
    hb_hashmap_t<int, std::string> m2;
    hb_hashmap_t<std::string, std::string> m3;

    std::string s;
    for (unsigned i = 1; i < 1000; i++)
    {
      s += "x";
      m0.set (i, i);
      m1.set (s, i);
      m2.set (i, s);
      m3.set (s, s);
    }
  }

  /* Test hashing maps. */
  {
    using pair = hb_codepoint_pair_t;

    hb_hashmap_t<hb_map_t, hb_map_t> m1;

    m1.set (hb_map_t (), hb_map_t {});
    m1.set (hb_map_t (), hb_map_t {pair (1u, 2u)});
    m1.set (hb_map_t {pair (1u, 2u)}, hb_map_t {pair (2u, 3u)});

    hb_always_assert (m1.get (hb_map_t ()) == hb_map_t {pair (1u, 2u)});
    hb_always_assert (m1.get (hb_map_t {pair (1u, 2u)}) == hb_map_t {pair (2u, 3u)});
  }

  /* Test hashing sets. */
  {
    hb_hashmap_t<hb_set_t, hb_set_t> m1;

    m1.set (hb_set_t (), hb_set_t ());
    m1.set (hb_set_t (), hb_set_t {1});
    m1.set (hb_set_t {1, 1000}, hb_set_t {2});

    hb_always_assert (m1.get (hb_set_t ()) == hb_set_t {1});
    hb_always_assert (m1.get (hb_set_t {1000, 1}) == hb_set_t {2});
  }

  /* Test hashing vectors. */
  {
    using vector_t = hb_vector_t<unsigned>;

    hb_hashmap_t<vector_t, vector_t> m1;

    m1.set (vector_t (), vector_t {1});
    m1.set (vector_t {1}, vector_t {2});

    m1 << hb_pair_t<vector_t, vector_t> {vector_t {2}, vector_t ()};

    hb_always_assert (m1.get (vector_t ()) == vector_t {1});
    hb_always_assert (m1.get (vector_t {1}) == vector_t {2});
  }

  /* Test moving values */
  {
    using vector_t = hb_vector_t<unsigned>;

    hb_hashmap_t<vector_t, vector_t> m1;
    vector_t v {3};
    hb_always_assert (v.length == 1);
    m1 << hb_pair_t<vector_t, vector_t> {vector_t {3}, v};
    hb_always_assert (v.length == 1);
    m1 << hb_pair_t<vector_t, vector_t&&> {vector_t {4}, std::move (v)};
    hb_always_assert (v.length == 0);
    m1 << hb_pair_t<vector_t&&, vector_t> {vector_t {4}, vector_t {5}};
    m1 << hb_pair_t<vector_t&&, vector_t&&> {vector_t {4}, vector_t {5}};

    hb_hashmap_t<vector_t, vector_t> m2;
    vector_t v2 {3};
    m2.set (vector_t {4}, v2);
    hb_always_assert (v2.length == 1);
    m2.set (vector_t {5}, std::move (v2));
    hb_always_assert (v2.length == 0);
  }

  /* Test hb::shared_ptr. */
  {
    hb_hashmap_t<hb::shared_ptr<hb_set_t>, hb::shared_ptr<hb_set_t>> m;

    m.set (hb::shared_ptr<hb_set_t> (hb_set_get_empty ()),
	   hb::shared_ptr<hb_set_t> (hb_set_get_empty ()));
    m.get (hb::shared_ptr<hb_set_t> (hb_set_get_empty ()));
    m.iter ();
    m.keys ();
    m.values ();
    m.iter_ref ();
    m.keys_ref ();
    m.values_ref ();
  }
  /* Test hb::unique_ptr. */
  {
    hb_hashmap_t<hb::unique_ptr<hb_set_t>, hb::unique_ptr<hb_set_t>> m;

    m.set (hb::unique_ptr<hb_set_t> (hb_set_get_empty ()),
           hb::unique_ptr<hb_set_t> (hb_set_get_empty ()));
    m.get (hb::unique_ptr<hb_set_t> (hb_set_get_empty ()));
    hb::unique_ptr<hb_set_t> *v;
    m.has (hb::unique_ptr<hb_set_t> (hb_set_get_empty ()), &v);
    m.iter_ref ();
    m.keys_ref ();
    m.values_ref ();
  }
  /* Test hashmap with complex shared_ptrs as keys. */
  {
    hb_hashmap_t<hb::shared_ptr<hb_map_t>, unsigned> m;

    hb_map_t *m1 = hb_map_create ();
    hb_map_t *m2 = hb_map_create ();
    m1->set (1,3);
    m2->set (1,3);

    hb::shared_ptr<hb_map_t> p1 {m1};
    hb::shared_ptr<hb_map_t> p2 {m2};
    m.set (p1,1);

    hb_always_assert (m.has (p2));

    m1->set (2,4);
    hb_always_assert (!m.has (p2));
  }
  /* Test value type with hb_bytes_t. */
  {
    hb_hashmap_t<int, hb_bytes_t> m;
    char c_str[] = "Test";
    hb_bytes_t bytes (c_str);

    m.set (1, bytes);
    hb_always_assert (m.has (1));
    hb_always_assert (m.get (1) == hb_bytes_t {"Test"});
  }
  /* Test operators. */
  {
    hb_map_t m1, m2, m3;
    m1.set (1, 2);
    m1.set (2, 4);
    m2.set (1, 2);
    m2.set (2, 4);
    m3.set (1, 3);
    m3.set (3, 5);

    hb_always_assert (m1 == m2);
    hb_always_assert (m1 != m3);
    hb_always_assert (!(m2 == m3));

    m2 = m3;
    hb_always_assert (m2.has (1));
    hb_always_assert (!m2.has (2));
    hb_always_assert (m2.has (3));

    hb_always_assert (m3.has (3));
  }
  /* Test reset. */
  {
    hb_hashmap_t<int, hb_set_t> m;
    m.set (1, hb_set_t {1, 2, 3});
    m.reset ();
  }
  /* Test iteration. */
  {
    hb_map_t m;
    m.set (1, 1);
    m.set (4, 3);
    m.set (5, 5);
    m.set (2, 1);
    m.set (3, 2);
    m.set (6, 8);

    hb_codepoint_t k;
    hb_codepoint_t v;
    unsigned pop = 0;
    for (signed i = -1;
	 m.next (&i, &k, &v);)
    {
      pop++;
           if (k == 1) hb_always_assert (v == 1);
      else if (k == 2) hb_always_assert (v == 1);
      else if (k == 3) hb_always_assert (v == 2);
      else if (k == 4) hb_always_assert (v == 3);
      else if (k == 5) hb_always_assert (v == 5);
      else if (k == 6) hb_always_assert (v == 8);
      else hb_always_assert (false);
    }
    hb_always_assert (pop == m.get_population ());
  }
  /* Test update */
  {
    hb_map_t m1, m2;
    m1.set (1, 2);
    m1.set (2, 4);
    m2.set (1, 3);

    m1.update (m2);
    hb_always_assert (m1.get_population () == 2);
    hb_always_assert (m1[1] == 3);
    hb_always_assert (m1[2] == 4);
  }
  /* Test keys / values */
  {
    hb_map_t m;
    m.set (1, 1);
    m.set (4, 3);
    m.set (5, 5);
    m.set (2, 1);
    m.set (3, 2);
    m.set (6, 8);

    hb_set_t keys;
    hb_set_t values;

    hb_copy (m.keys (), keys);
    hb_copy (m.values (), values);

    hb_always_assert (keys.is_equal (hb_set_t ({1, 2, 3, 4, 5, 6})));
    hb_always_assert (values.is_equal (hb_set_t ({1, 1, 2, 3, 5, 8})));

    hb_always_assert (keys.is_equal (hb_set_t (m.keys ())));
    hb_always_assert (values.is_equal (hb_set_t (m.values ())));
  }

  return 0;
}
