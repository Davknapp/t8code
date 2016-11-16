/*
  This file is part of t8code.
  t8code is a C library to manage a collection (a forest) of multiple
  connected adaptive space-trees of general element classes in parallel.

  Copyright (C) 2015 the developers

  t8code is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  t8code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with t8code; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "t8_default_common.h"
#include "t8_default_tri.h"
#include "t8_dtri_bits.h"

typedef t8_dtri_t   t8_default_tri_t;

/* This function is used by other element functions and we thus need to
 * declare it up here */
static uint64_t     t8_default_tri_get_linear_id (const t8_element_t * elem,
                                                  int level);

static              size_t
t8_default_tri_size (void)
{
  return sizeof (t8_default_tri_t);
}

static int
t8_default_tri_maxlevel (void)
{
  return T8_DTRI_MAXLEVEL;
}

static int
t8_default_tri_level (const t8_element_t * elem)
{
  return t8_dtri_get_level ((t8_dtri_t *) elem);
}

static void
t8_default_tri_copy (const t8_element_t * source, t8_element_t * dest)
{
  t8_dtri_copy ((const t8_dtri_t *) source, (t8_dtri_t *) dest);
}

static int
t8_default_tri_compare (const t8_element_t * elem1,
                        const t8_element_t * elem2)
{
  int                 maxlvl;
  uint64_t            id1, id2;

  /* Compute the bigger level of the two */
  maxlvl = SC_MAX (t8_default_tri_level (elem1),
                   t8_default_tri_level (elem2));
  /* Compute the linear ids of the elements */
  id1 = t8_default_tri_get_linear_id (elem1, maxlvl);
  id2 = t8_default_tri_get_linear_id (elem2, maxlvl);
  /* return negativ if id1 < id2, zero if id1 = id2, positive if id1 > id2 */
  return id1 < id2 ? -1 : id1 != id2;
}

static void
t8_default_tri_parent (const t8_element_t * elem, t8_element_t * parent)
{
  const t8_default_tri_t *t = (const t8_default_tri_t *) elem;
  t8_default_tri_t   *p = (t8_default_tri_t *) parent;

  t8_dtri_parent (t, p);
}

static void
t8_default_tri_sibling (const t8_element_t * elem,
                        int sibid, t8_element_t * sibling)
{
  const t8_default_tri_t *t = (const t8_default_tri_t *) elem;
  t8_default_tri_t   *s = (t8_default_tri_t *) sibling;

  t8_dtri_sibling (t, sibid, s);
}

static void
t8_default_tri_child (const t8_element_t * elem,
                      int childid, t8_element_t * child)
{
  const t8_default_tri_t *t = (const t8_default_tri_t *) elem;
  t8_default_tri_t   *c = (t8_default_tri_t *) child;

  t8_dtri_child (t, childid, c);
}

static void
t8_default_tri_children (const t8_element_t * elem,
                         int length, t8_element_t * c[])
{
  T8_ASSERT (length == T8_DTRI_CHILDREN);

  t8_dtri_childrenpv ((const t8_dtri_t *) elem, (t8_dtri_t **) c);
}

static int
t8_default_tri_child_id (const t8_element_t * elem)
{
  return t8_dtri_child_id ((t8_dtri_t *) elem);
}

static int
t8_default_tri_is_family (t8_element_t ** fam)
{
  return t8_dtri_is_familypv ((const t8_dtri_t **) fam);
}

static int
t8_default_tri_face_neighbor (const t8_element_t * elem,
                              t8_element_t * neighbor, int face)
{
  return t8_dtri_face_neighbour ((const t8_dtri_t *) elem, face,
                                 (t8_dtri_t *) neighbor);
}

static void
t8_default_tri_nca (const t8_element_t * elem1,
                    const t8_element_t * elem2, t8_element_t * nca)
{
  const t8_default_tri_t *t1 = (const t8_default_tri_t *) elem1;
  const t8_default_tri_t *t2 = (const t8_default_tri_t *) elem1;
  t8_default_tri_t   *c = (t8_default_tri_t *) nca;

  t8_dtri_nearest_common_ancestor (t1, t2, c);
}

static void
t8_default_tri_set_linear_id (t8_element_t * elem, int level, uint64_t id)
{
  T8_ASSERT (0 <= level && level <= T8_DTRI_MAXLEVEL);
  T8_ASSERT (0 <= id && id < ((uint64_t) 1) << (2 * level));

  t8_dtri_init_linear_id ((t8_default_tri_t *) elem, id, level);
}

static uint64_t
t8_default_tri_get_linear_id (const t8_element_t * elem, int level)
{
  T8_ASSERT (0 <= level && level <= T8_DTRI_MAXLEVEL);

  return t8_dtri_linear_id ((t8_default_tri_t *) elem, level);
}

static void
t8_default_tri_first_descendant (const t8_element_t * elem,
                                 t8_element_t * desc)
{
  t8_dtri_first_descendant ((t8_dtri_t *) elem, (t8_dtri_t *) desc);
}

static void
t8_default_tri_last_descendant (const t8_element_t * elem,
                                t8_element_t * desc)
{
  t8_dtri_last_descendant ((t8_dtri_t *) elem, (t8_dtri_t *) desc);
}

static void
t8_default_tri_successor (const t8_element_t * elem1,
                          t8_element_t * elem2, int level)
{
  T8_ASSERT (0 <= level && level <= T8_DTRI_MAXLEVEL);

  t8_dtri_successor ((const t8_default_tri_t *) elem1,
                     (t8_default_tri_t *) elem2, level);
}

static void
t8_default_tri_anchor (const t8_element_t * elem, int anchor[3])
{
  t8_dtri_t          *tri = (t8_dtri_t *) elem;

  anchor[0] = tri->x;
  anchor[1] = tri->y;
  anchor[2] = 0;
}

static int
t8_default_tri_root_len (const t8_element_t * elem)
{
  return T8_DTRI_ROOT_LEN;
}

t8_eclass_scheme_t *
t8_default_scheme_new_tri (void)
{
  t8_eclass_scheme_t *ts;

  ts = T8_ALLOC_ZERO (t8_eclass_scheme_t, 1);
  ts->eclass = T8_ECLASS_TRIANGLE;

  ts->elem_size = t8_default_tri_size;
  ts->elem_maxlevel = t8_default_tri_maxlevel;

  ts->elem_level = t8_default_tri_level;
  ts->elem_copy = t8_default_tri_copy;
  ts->elem_compare = t8_default_tri_compare;
  ts->elem_parent = t8_default_tri_parent;
  ts->elem_sibling = t8_default_tri_sibling;
  ts->elem_child = t8_default_tri_child;
  ts->elem_children = t8_default_tri_children;
  ts->elem_is_family = t8_default_tri_is_family;
  ts->elem_face_neighbor = t8_default_tri_face_neighbor;
  ts->elem_child_id = t8_default_tri_child_id;
  ts->elem_nca = t8_default_tri_nca;
  ts->elem_set_linear_id = t8_default_tri_set_linear_id;
  ts->elem_get_linear_id = t8_default_tri_get_linear_id;
  ts->elem_first_desc = t8_default_tri_first_descendant;
  ts->elem_last_desc = t8_default_tri_last_descendant;
  ts->elem_successor = t8_default_tri_successor;
  ts->elem_anchor = t8_default_tri_anchor;
  ts->elem_root_len = t8_default_tri_root_len;

  ts->elem_new = t8_default_mempool_alloc;
  ts->elem_destroy = t8_default_mempool_free;

  ts->ts_destroy = t8_default_scheme_mempool_destroy;
  ts->ts_context = sc_mempool_new (sizeof (t8_default_tri_t));

  return ts;
}
