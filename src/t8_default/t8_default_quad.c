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

#include <p4est_bits.h>
#include "t8_default_common.h"
#include "t8_default_quad.h"

/* This function is used by other element functions and we thus need to
 * declare it up here */
static uint64_t     t8_default_quad_get_linear_id (const t8_element_t * elem,
                                                   int level);

#ifdef T8_ENABLE_DEBUG

static int
t8_default_quad_surround_matches (const p4est_quadrant_t * q,
                                  const p4est_quadrant_t * r)
{
  return T8_QUAD_GET_TDIM (q) == T8_QUAD_GET_TDIM (r) &&
    (T8_QUAD_GET_TDIM (q) == -1 ||
     (T8_QUAD_GET_TNORMAL (q) == T8_QUAD_GET_TNORMAL (r) &&
      T8_QUAD_GET_TCOORD (q) == T8_QUAD_GET_TCOORD (r)));
}

#endif /* T8_ENABLE_DEBUG */

static              size_t
t8_default_quad_size (void)
{
  return sizeof (t8_pquad_t);
}

static int
t8_default_quad_maxlevel (void)
{
  return P4EST_QMAXLEVEL;
}

static              t8_eclass_t
t8_default_quad_child_eclass (int childid)
{
  T8_ASSERT (0 <= childid && childid < P4EST_CHILDREN);

  return T8_ECLASS_QUAD;
}

static int
t8_default_quad_level (const t8_element_t * elem)
{
  return (int) ((const p4est_quadrant_t *) elem)->level;
}

static void
t8_default_quad_copy_surround (const p4est_quadrant_t * q,
                               p4est_quadrant_t * r)
{
  T8_QUAD_SET_TDIM (r, T8_QUAD_GET_TDIM (q));
  if (T8_QUAD_GET_TDIM (q) == 3) {
    T8_QUAD_SET_TNORMAL (r, T8_QUAD_GET_TNORMAL (q));
    T8_QUAD_SET_TCOORD (r, T8_QUAD_GET_TCOORD (q));
  }
}

static void
t8_default_quad_copy (const t8_element_t * source, t8_element_t * dest)
{
  const p4est_quadrant_t *q = (const p4est_quadrant_t *) source;
  p4est_quadrant_t   *r = (p4est_quadrant_t *) dest;

  *r = *q;
  t8_default_quad_copy_surround (q, r);
}

static int
t8_default_quad_compare (const t8_element_t * elem1,
                         const t8_element_t * elem2)
{
  int                 maxlvl;
  uint64_t            id1, id2;

  /* Compute the bigger level of the two */
  maxlvl = SC_MAX (t8_default_quad_level (elem1),
                   t8_default_quad_level (elem2));
  /* Compute the linear ids of the elements */
  id1 = t8_default_quad_get_linear_id (elem1, maxlvl);
  id2 = t8_default_quad_get_linear_id (elem2, maxlvl);
  /* return negativ if id1 < id2, zero if id1 = id2, positive if id1 > id2 */
  return id1 < id2 ? -1 : id1 != id2;
}

static void
t8_default_quad_parent (const t8_element_t * elem, t8_element_t * parent)
{
  const p4est_quadrant_t *q = (const p4est_quadrant_t *) elem;
  p4est_quadrant_t   *r = (p4est_quadrant_t *) parent;

  p4est_quadrant_parent (q, r);
  t8_default_quad_copy_surround (q, r);
}

static void
t8_default_quad_sibling (const t8_element_t * elem,
                         int sibid, t8_element_t * sibling)
{
  const p4est_quadrant_t *q = (const p4est_quadrant_t *) elem;
  p4est_quadrant_t   *r = (p4est_quadrant_t *) sibling;

  p4est_quadrant_sibling (q, r, sibid);
  t8_default_quad_copy_surround (q, r);
}

static void
t8_default_quad_child (const t8_element_t * elem,
                       int childid, t8_element_t * child)
{
  const p4est_quadrant_t *q = (const p4est_quadrant_t *) elem;
  const p4est_qcoord_t shift = P4EST_QUADRANT_LEN (q->level + 1);
  p4est_quadrant_t   *r = (p4est_quadrant_t *) child;

  T8_ASSERT (p4est_quadrant_is_extended (q));
  T8_ASSERT (q->level < P4EST_QMAXLEVEL);
  T8_ASSERT (childid >= 0 && childid < P4EST_CHILDREN);

  r->x = childid & 0x01 ? (q->x | shift) : q->x;
  r->y = childid & 0x02 ? (q->y | shift) : q->y;
  r->level = q->level + 1;
  T8_ASSERT (p4est_quadrant_is_parent (q, r));

  t8_default_quad_copy_surround (q, r);
}

static void
t8_default_quad_children (const t8_element_t * elem,
                          int length, t8_element_t * c[])
{
  const p4est_quadrant_t *q = (const p4est_quadrant_t *) elem;
  int                 i;

  T8_ASSERT (length == P4EST_CHILDREN);

  p4est_quadrant_childrenpv (q, (p4est_quadrant_t **) c);
  for (i = 0; i < P4EST_CHILDREN; ++i) {
    t8_default_quad_copy_surround (q, (p4est_quadrant_t *) c[i]);
  }
}

static int
t8_default_quad_child_id (const t8_element_t * elem)
{
  return p4est_quadrant_child_id ((p4est_quadrant_t *) elem);
}

static int
t8_default_quad_is_family (t8_element_t ** fam)
{
  return p4est_quadrant_is_familypv ((p4est_quadrant_t **) fam);
}

/* TODO: implement quad and hex face neighbor */

static void
t8_default_quad_set_linear_id (t8_element_t * elem, int level, uint64_t id)
{
  T8_ASSERT (0 <= level && level <= P4EST_QMAXLEVEL);
  T8_ASSERT (0 <= id && id < ((uint64_t) 1) << P4EST_DIM * level);

  p4est_quadrant_set_morton ((p4est_quadrant_t *) elem, level, id);
  T8_QUAD_SET_TDIM ((p4est_quadrant_t *) elem, 2);
}

static              uint64_t
t8_default_quad_get_linear_id (const t8_element_t * elem, int level)
{
  T8_ASSERT (0 <= level && level <= P4EST_QMAXLEVEL);

  return p4est_quadrant_linear_id ((p4est_quadrant_t *) elem, level);
}

static void
t8_default_quad_first_descendant (const t8_element_t * elem,
                                  t8_element_t * desc)
{
  p4est_quadrant_first_descendant ((p4est_quadrant_t *) elem,
                                   (p4est_quadrant_t *) desc,
                                   P4EST_QMAXLEVEL);
}

static void
t8_default_quad_last_descendant (const t8_element_t * elem,
                                 t8_element_t * desc)
{
  p4est_quadrant_last_descendant ((p4est_quadrant_t *) elem,
                                  (p4est_quadrant_t *) desc, P4EST_QMAXLEVEL);
}

static void
t8_default_quad_successor (const t8_element_t * elem1,
                           t8_element_t * elem2, int level)
{
  uint64_t            id;
  T8_ASSERT (0 <= level && level <= P4EST_QMAXLEVEL);

  id = p4est_quadrant_linear_id ((const p4est_quadrant_t *) elem1, level);
  T8_ASSERT (id + 1 < ((uint64_t) 1) << P4EST_DIM * level);
  p4est_quadrant_set_morton ((p4est_quadrant_t *) elem2, level, id + 1);
  t8_default_quad_copy_surround ((const p4est_quadrant_t *) elem1,
                                 (p4est_quadrant_t *) elem2);
}

static void
t8_default_quad_nca (const t8_element_t * elem1, const t8_element_t * elem2,
                     t8_element_t * nca)
{
  const p4est_quadrant_t *q1 = (const p4est_quadrant_t *) elem1;
  const p4est_quadrant_t *q2 = (const p4est_quadrant_t *) elem2;
  p4est_quadrant_t   *r = (p4est_quadrant_t *) nca;

  T8_ASSERT (t8_default_quad_surround_matches (q1, q2));

  p4est_nearest_common_ancestor (q1, q2, r);
  t8_default_quad_copy_surround (q1, r);
}

static void
t8_default_quad_boundary (const t8_element_t * elem,
                          int min_dim, int length, t8_element_t ** boundary)
{
#ifdef T8_ENABLE_DEBUG
  int                 per_eclass[T8_ECLASS_COUNT];
#endif

  T8_ASSERT (length ==
             t8_eclass_count_boundary (T8_ECLASS_QUAD, min_dim, per_eclass));

  /* TODO: write this function */
  SC_ABORT_NOT_REACHED ();
}

static void
t8_default_quad_anchor (const t8_element_t * elem, int coord[3])
{
  p4est_quadrant_t   *q;

  q = (p4est_quadrant_t *) elem;
  coord[0] = q->x;
  coord[1] = q->y;
  coord[2] = 0;
}

static int
t8_default_quad_root_len (const t8_element_t * elem)
{
  return P4EST_ROOT_LEN;
}

static int
t8_default_quad_inside_root (const t8_element_t * elem)
{
  return p4est_quadrant_is_inside_root ((const p4est_quadrant_t *) elem);
}

t8_eclass_scheme_t *
t8_default_scheme_new_quad (void)
{
  t8_eclass_scheme_t *ts;

  ts = T8_ALLOC (t8_eclass_scheme_t, 1);
  ts->eclass = T8_ECLASS_QUAD;

  ts->elem_size = t8_default_quad_size;
  ts->elem_maxlevel = t8_default_quad_maxlevel;
  ts->elem_child_eclass = t8_default_quad_child_eclass;

  ts->elem_level = t8_default_quad_level;
  ts->elem_copy = t8_default_quad_copy;
  ts->elem_compare = t8_default_quad_compare;
  ts->elem_parent = t8_default_quad_parent;
  ts->elem_sibling = t8_default_quad_sibling;
  ts->elem_child = t8_default_quad_child;
  ts->elem_children = t8_default_quad_children;
  ts->elem_child_id = t8_default_quad_child_id;
  ts->elem_is_family = t8_default_quad_is_family;
  ts->elem_nca = t8_default_quad_nca;
  ts->elem_boundary = t8_default_quad_boundary;
  ts->elem_set_linear_id = t8_default_quad_set_linear_id;
  ts->elem_get_linear_id = t8_default_quad_get_linear_id;
  ts->elem_first_desc = t8_default_quad_first_descendant;
  ts->elem_last_desc = t8_default_quad_last_descendant;
  ts->elem_successor = t8_default_quad_successor;
  ts->elem_anchor = t8_default_quad_anchor;
  ts->elem_root_len = t8_default_quad_root_len;
  ts->elem_inside_root = t8_default_quad_inside_root;

  ts->elem_new = t8_default_mempool_alloc;
  ts->elem_destroy = t8_default_mempool_free;

  ts->ts_destroy = t8_default_scheme_mempool_destroy;
  ts->ts_context = sc_mempool_new (sizeof (t8_pquad_t));

  return ts;
}
