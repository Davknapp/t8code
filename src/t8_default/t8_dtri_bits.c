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
#ifndef T8_DTRI_TO_DTET
#include "t8_dtri_bits.h"
#include "t8_dtri_connectivity.h"
#else
#include "t8_dtet_bits.h"
#include "t8_dtet_connectivity.h"
#endif

typedef int8_t      t8_dtri_cube_id_t;

/* Compute the cube-id of t's ancestor of level "level" in constant time.
 * If "level" is greater then t->level then the cube-id 0 is returned. */
static              t8_dtri_cube_id_t
compute_cubeid (const t8_dtri_t * t, int level)
{
  t8_dtri_cube_id_t   id = 0;
  t8_dtri_coord_t     h;

  /* TODO: assert that 0 < level? This may simplify code elsewhere */

  T8_ASSERT (0 <= level && level <= T8_DTRI_MAXLEVEL);
  h = T8_DTRI_LEN (level);

  if (level == 0) {
    return 0;
  }

  id |= ((t->x & h) ? 0x01 : 0);
  id |= ((t->y & h) ? 0x02 : 0);
#ifdef T8_DTRI_TO_DTET
  id |= ((t->z & h) ? 0x04 : 0);
#endif

  return id;
}

/* A routine to compute the type of t's ancestor of level "level".
 * If "level" equals t's level then t's type is returned.
 * It is not allowed to call this function with "level" greater than t->level.
 * This method runs in O(t->level - level).
 */
static              t8_dtri_type_t
compute_type (const t8_dtri_t * t, int level)
{
  int8_t              type = t->type;
  t8_dtri_cube_id_t   cid;
  int                 tlevel = t->level;
  int                 i;

  T8_ASSERT (0 <= level && level <= tlevel);
  if (level == tlevel) {
    return t->type;
  }
  if (level == 0) {
    /* TODO: the level of the root tet is hardcoded to 0
     *       maybe once we want to allow the root tet to have different types */
    return 0;
  }
  for (i = tlevel; i < level; i++) {
    cid = compute_cubeid (t, i);
    /* compute type as the type of T^{i+1}, that is T's ancestor of level i+1 */
    type = t8_dtri_cid_type_to_parenttype[cid][type];
  }
  return type;
}

void
t8_dtri_copy (const t8_dtri_t * t, t8_dtri_t * dest)
{
  memcpy (dest, t, sizeof (t8_dtri_t));
}

void
t8_dtri_parent (const t8_dtri_t * t, t8_dtri_t * parent)
{
  t8_dtri_cube_id_t   cid;
  t8_dtri_coord_t     h;

  T8_ASSERT (t->level > 0);

#ifdef T8_DTRI_TO_DTET
  parent->eclass = t->eclass;
#endif

  h = T8_DTRI_LEN (t->level);
  /* Compute type of parent */
  cid = compute_cubeid (t, t->level);
  parent->type = t8_dtri_cid_type_to_parenttype[cid][t->type];
  /* Set coordinates of parent */
  parent->x = t->x & ~h;
  parent->y = t->y & ~h;
#ifdef T8_DTRI_TO_DTET
  parent->z = t->z & ~h;
#endif
  parent->level = t->level - 1;
}

void
t8_dtri_ancestor (const t8_dtri_t * t, int level, t8_dtri_t * ancestor)
{
  /* TODO: find out, at which level difference it is faster to use    *
   * the arithmetic computation of ancestor type
   * opposed to iteratively computing the parent type.
   */
  t8_dtri_coord_t     delta_x, delta_y, diff_xy;
#ifdef T8_DTRI_TO_DTET
  t8_dtri_coord_t     delta_z, diff_xz, diff_yz;
  t8_dtet_type_t      possible_types[6] = { 1, 1, 1, 1, 1, 1 };
  int                 i;
#ifdef T8_ENABLE_DEBUG
  int                 set_type = 0;
#endif
#endif /* T8_DTRI_TO_DTET */

  /* delta_{x,y} = t->{x,y} - ancestor->{x,y}
   * the difference of the coordinates.
   * Needed to compute the type of the ancestor. */
  delta_x = t->x & (T8_DTRI_LEN (level) - 1);
  delta_y = t->y & (T8_DTRI_LEN (level) - 1);
#ifdef T8_DTRI_TO_DTET
  delta_z = t->z & (T8_DTRI_LEN (level) - 1);
#endif

  /* The coordinates of the ancestor. It is necessary
   * to compute the delta first, since ancestor and t
   * could point to the same triangle. */
  ancestor->x = t->x & ~(T8_DTRI_LEN (level) - 1);
  ancestor->y = t->y & ~(T8_DTRI_LEN (level) - 1);
#ifdef T8_DTRI_TO_DTET
  ancestor->z = t->z & ~(T8_DTRI_LEN (level) - 1);
#endif

#ifndef T8_DTRI_TO_DTET
  /* The type of the ancestor depends on delta_x - delta_y */
  diff_xy = delta_x - delta_y;
  if (diff_xy > 0) {
    ancestor->type = 0;
  }
  else if (diff_xy < 0) {
    ancestor->type = 1;
  }
  else {
    T8_ASSERT (diff_xy == 0);
    ancestor->type = t->type;
  }

  ancestor->n = t->n;
#else
/* The sign of each diff reduces the number of possible types
 * for the ancestor. At the end only one possible type is left,
 * this type's entry in the possible_types array will be positive.
 */

  diff_xy = delta_x - delta_y;
  diff_xz = delta_x - delta_z;
  diff_yz = delta_y - delta_z;

/* delta_x - delta_y */
  if (diff_xy > 0) {
    possible_types[2] = possible_types[3] = possible_types[4] = 0;
  }
  else if (diff_xy < 0) {
    possible_types[0] = possible_types[1] = possible_types[5] = 0;
  }
  else {
    T8_ASSERT (diff_xy == 0);
    if (t->type == 0 || t->type == 1 || t->type == 5) {
      possible_types[2] = possible_types[3] = possible_types[4] = 0;
    }
    else {
      possible_types[0] = possible_types[1] = possible_types[5] = 0;
    }
  }

/* delta_x - delta_z */
  if (diff_xz > 0) {
    possible_types[3] = possible_types[4] = possible_types[5] = 0;
  }
  else if (diff_xz < 0) {
    possible_types[0] = possible_types[1] = possible_types[2] = 0;
  }
  else {
    T8_ASSERT (diff_xz == 0);
    if (t->type == 0 || t->type == 1 || t->type == 2) {
      possible_types[3] = possible_types[4] = possible_types[5] = 0;
    }
    else {
      possible_types[0] = possible_types[1] = possible_types[2] = 0;
    }
  }

/* delta_y - delta_z */
  if (diff_yz > 0) {
    possible_types[0] = possible_types[4] = possible_types[5] = 0;
  }
  else if (diff_yz < 0) {
    possible_types[1] = possible_types[2] = possible_types[3] = 0;
  }
  else {
    T8_ASSERT (diff_yz == 0);
    if (t->type == 1 || t->type == 2 || t->type == 3) {
      possible_types[0] = possible_types[4] = possible_types[5] = 0;
    }
    else {
      possible_types[1] = possible_types[2] = possible_types[3] = 0;
    }
  }

  /* Got through possible_types array and find the only entry
   * that is nonzero
   */
  for (i = 0; i < 6; i++) {
    T8_ASSERT (possible_types[i] == 0 || possible_types[i] == 1);
    if (possible_types[i] == 1) {
      ancestor->type = i;
#ifdef T8_ENABLE_DEBUG
      T8_ASSERT (set_type != 1);
      set_type = 1;
#endif
    }
  }
  T8_ASSERT (set_type == 1);
#endif /* T8_DTRI_TO_DTET */
  ancestor->level = level;
}

void
t8_dtri_compute_coords (const t8_dtri_t * t, int vertex,
                        t8_dtri_coord_t coordinates[T8_DTRI_DIM])
{
  t8_dtri_type_t      type;
  int                 ei;
#ifdef T8_DTRI_TO_DTET
  int                 ej;
#endif
  t8_dtri_coord_t     h;

  T8_ASSERT (0 <= vertex && vertex < T8_DTRI_FACES);

  type = t->type;
  h = T8_DTRI_LEN (t->level);
#ifndef T8_DTRI_TO_DTET
  ei = type;
#else
  ei = type / 2;
  ej = (ei + ((type % 2 == 0) ? 2 : 1)) % 3;
#endif

  coordinates[0] = t->x;
  coordinates[1] = t->y;
#ifdef T8_DTRI_TO_DTET
  coordinates[2] = t->z;
#endif
  if (vertex == 0) {
    return;
  }
  coordinates[ei] += h;
#ifndef T8_DTRI_TO_DTET
  if (vertex == 2) {
    coordinates[1 - ei] += h;
    return;
  }
#else
  if (vertex == 2) {
    coordinates[ej] += h;
    return;
  }
  if (vertex == 3) {
    coordinates[(ei + 1) % 3] += h;
    coordinates[(ei + 2) % 3] += h;
  }
  /* done 3D */
#endif
}

void
t8_dtri_compute_all_coords (const t8_dtri_t * t,
                            t8_dtri_coord_t
                            coordinates[T8_DTRI_FACES][T8_DTRI_DIM])
{
  t8_dtri_type_t      type;
  int                 ei;
#ifdef T8_DTRI_TO_DTET
  int                 ej;
#endif
  int                 i;
  t8_dtri_coord_t     h;

  type = t->type;
  h = T8_DTRI_LEN (t->level);
#ifndef T8_DTRI_TO_DTET
  ei = type;
#else
  ei = type / 2;
  ej = (ei + ((type % 2 == 0) ? 2 : 1)) % 3;
#endif

  coordinates[0][0] = t->x;
  coordinates[0][1] = t->y;
#ifdef T8_DTRI_TO_DTET
  coordinates[0][2] = t->z;
#endif
  for (i = 0; i < T8_DTRI_DIM; i++) {
    coordinates[1][i] = coordinates[0][i];
#ifndef T8_DTRI_TO_DTET
    coordinates[2][i] = coordinates[0][i] + h;
#else
    coordinates[2][i] = coordinates[0][i];
    coordinates[3][i] = coordinates[0][i] + h;
#endif
  }
  coordinates[1][ei] += h;
#ifdef T8_DTRI_TO_DTET
  coordinates[2][ei] += h;
  coordinates[2][ej] += h;
#endif
}

/* The childid here is the Morton child id
 * (TODO: define this)
 * It is possible that the function is called with
 * elem = child */
void
t8_dtri_child (const t8_dtri_t * elem, int childid, t8_dtri_t * child)
{
  const t8_dtri_t    *t = (const t8_dtri_t *) elem;
  t8_dtri_t          *c = (t8_dtri_t *) child;
  t8_dtri_coord_t     t_coordinates[T8_DTRI_DIM];
  int                 vertex;
  int                 Bey_cid;

  T8_ASSERT (t->level < T8_DTRI_MAXLEVEL);
  T8_ASSERT (0 <= childid && childid < T8_DTRI_CHILDREN);

  Bey_cid = t8_dtri_index_to_bey_number[elem->type][childid];

  /* Compute anchor coordinates of child */
  if (Bey_cid == 0) {
    /* TODO: would it be better do drop this if and
     *       capture it with (t->x+t->x)>>1 below? */
    c->x = t->x;
    c->y = t->y;
#ifdef T8_DTRI_TO_DTET
    c->z = t->z;
#endif
  }
  else {
    vertex = t8_dtri_beyid_to_vertex[Bey_cid];
    /* i-th anchor coordinate of child is (X_(0,i)+X_(vertex,i))/2
     * where X_(i,j) is the j-th coordinate of t's ith node */
    t8_dtri_compute_coords (t, vertex, t_coordinates);
    c->x = (t->x + t_coordinates[0]) >> 1;
    c->y = (t->y + t_coordinates[1]) >> 1;
#ifdef T8_DTRI_TO_DTET
    c->z = (t->z + t_coordinates[2]) >> 1;
#endif
  }

  /* Compute type of child */
  c->type = t8_dtri_type_of_child[t->type][Bey_cid];

  c->level = t->level + 1;
}

void
t8_dtri_childrenpv (const t8_dtri_t * t, t8_dtri_t * c[T8_DTRI_CHILDREN])
{
  t8_dtri_coord_t     t_coordinates[T8_DTRI_FACES][T8_DTRI_DIM];
  const int8_t        level = t->level + 1;
  int                 i;
  int                 Bey_cid;
  int                 vertex;

  T8_ASSERT (t->level < T8_DTRI_MAXLEVEL);
  t8_dtri_compute_all_coords (t, t_coordinates);
  c[0]->x = t->x;
  c[0]->y = t->y;
#ifdef T8_DTRI_TO_DTET
  c[0]->z = t->z;
#endif
  c[0]->type = t->type;
  c[0]->level = level;
  for (i = 1; i < T8_DTRI_CHILDREN; i++) {
    Bey_cid = t8_dtri_index_to_bey_number[t->type][i];
    vertex = t8_dtri_beyid_to_vertex[Bey_cid];
    /* i-th anchor coordinate of child is (X_(0,i)+X_(vertex,i))/2
     * where X_(i,j) is the j-th coordinate of t's ith node */
    c[i]->x = (t->x + t_coordinates[vertex][0]) >> 1;
    c[i]->y = (t->y + t_coordinates[vertex][1]) >> 1;
#ifdef T8_DTRI_TO_DTET
    c[i]->z = (t->z + t_coordinates[vertex][2]) >> 1;
#endif
    c[i]->type = t8_dtri_type_of_child[t->type][Bey_cid];
    c[i]->level = level;
  }
}

#ifndef T8_DTRI_TO_DTET
int
t8_dtri_is_familypv (const t8_dtri_t * f[])
{
  const int8_t        level = f[0]->level;
  t8_dtri_coord_t     coords0[T8_DTRI_CHILDREN];
  t8_dtri_coord_t     coords1[T8_DTRI_CHILDREN];
  t8_dtri_coord_t     inc;
  int                 type;
  int                 dir1;

  if (level == 0 || level != f[1]->level ||
      level != f[2]->level || level != f[3]->level) {
    return 0;
  }
  /* check whether the types are correct */
  type = f[0]->type;
  if (f[1]->type != 0 && f[2]->type != 1 && f[3]->type != type) {
    return 0;
  }
  /* check whether the coordinates are correct
   * triangles 1 and 2 have to have the same coordinates */
  if (f[1]->x != f[2]->x || f[1]->y != f[2]->y) {
    return 0;
  }
  dir1 = type;
  inc = T8_DTRI_LEN (level);
  coords0[0] = f[0]->x;
  coords0[1] = f[0]->y;
  coords1[0] = f[1]->x;
  coords1[1] = f[1]->y;
  return (coords1[dir1] == coords0[dir1] + inc
          && coords1[1 - dir1] == coords0[1 - dir1]
          && f[3]->x == f[0]->x + inc && f[3]->y == f[0]->y + inc);
}
#endif

/* The sibid here is the Morton child id of the parent.
 * TODO: Implement this algorithm directly w/o using
 * parent and child
 * TODO: CB agrees, make this as non-redundant as possible */
void
t8_dtri_sibling (const t8_dtri_t * elem, int sibid, t8_dtri_t * sibling)
{
  T8_ASSERT (0 <= sibid && sibid < T8_DTRI_CHILDREN);
  T8_ASSERT (((const t8_dtri_t *) elem)->level > 0);
  t8_dtri_parent (elem, sibling);
  t8_dtri_child (sibling, sibid, sibling);
}

/* Saves the neighbour of T along face "face" in N
 * returns the facenumber of N along which T is its neighbour */
int
t8_dtri_face_neighbour (const t8_dtri_t * t, int face, t8_dtri_t * n)
{
  /* TODO: document what happens if outside of root tet */
  int                 type_new, type_old;
#ifdef T8_DTRI_TO_DTET
  int                 sign;
#endif
  int                 ret = -1;
  int8_t              level;
  t8_dtri_coord_t     coords[3];

  T8_ASSERT (0 <= face && face < T8_DTRI_FACES);

  level = t->level;
  type_old = t->type;
  type_new = type_old;
  coords[0] = t->x;
  coords[1] = t->y;
#ifdef T8_DTRI_TO_DTET
  coords[2] = t->z;
#endif

#ifndef T8_DTRI_TO_DTET
  /* 2D */
  ret = 2 - face;
  type_new = 1 - type_old;
  if (face == 0) {
    coords[type_old] += T8_DTRI_LEN (level);
  }
  else if (face == 2) {
    coords[1 - type_old] -= T8_DTRI_LEN (level);
  }
  /* 2D end */
#else
  /* 3D */
  type_new += 6;                /* We want to compute modulo six and dont want negative numbers */
  if (face == 1 || face == 2) {
    sign = (type_new % 2 == 0 ? 1 : -1);
    sign *= (face % 2 == 0 ? 1 : -1);
    type_new += sign;
    type_new %= 6;
    ret = face;
  }
  else {
    if (face == 0) {
      /* type: 0,1 --> x+1
       *       2,3 --> y+1
       *       4,5 --> z+1 */
      coords[type_old / 2] += T8_DTRI_LEN (level);
      type_new += (type_new % 2 == 0 ? 4 : 2);
    }
    else {                      /* face == 3 */

      /* type: 1,2 --> z-1
       *       3,4 --> x-1
       *       5,0 --> y-1 */
      coords[((type_new + 3) % 6) / 2] -= T8_DTRI_LEN (level);
      type_new += (type_new % 2 == 0 ? 2 : 4);
    }
    type_new %= 6;
    ret = 3 - face;
  }
  /* 3D end */
#endif
  n->x = coords[0];
  n->y = coords[1];
#ifdef T8_DTRI_TO_DTET
  n->z = coords[2];
#endif
  n->level = level;
  n->type = type_new;
  return ret;
}

void
t8_dtri_nearest_common_ancestor (const t8_dtri_t * t1,
                                 const t8_dtri_t * t2, t8_dtri_t * r)
{
  int                 maxlevel, r_level;
  uint32_t            exclorx, exclory;
#ifdef T8_DTRI_TO_DTET
  uint32_t            exclorz;
#endif
  uint32_t            maxclor;

  exclorx = t1->x ^ t2->x;
  exclory = t1->y ^ t2->y;
#ifdef T8_DTRI_TO_DTET
  exclorz = t1->z ^ t2->z;

  maxclor = exclorx | exclory | exclorz;
#else
  maxclor = exclorx | exclory;
#endif
  maxlevel = SC_LOG2_32 (maxclor) + 1;

  T8_ASSERT (maxlevel <= T8_DTRI_MAXLEVEL);

  r_level = (int8_t) SC_MIN (T8_DTRI_MAXLEVEL - maxlevel,
                             (int) SC_MIN (t1->level, t2->level));
  t8_dtri_ancestor (t1, r_level, r);
#if 0
  /* Find the correct type of r by testing with
   * which type it becomes an ancestor of t1. */
  for (r->type = 0; r->type < 6; r->type++) {
    if (t8_dtri_is_ancestor (r, t1)) {
      return;
    }
  }
  SC_ABORT_NOT_REACHED ();
#endif
}

int
t8_dtri_is_inside_root (const t8_dtri_t * t)
{
  int                 is_inside;

  is_inside = (t->x >= 0 && t->x < T8_DTRI_ROOT_LEN) && (t->y >= 0) &&
#ifdef T8_DTRI_TO_DTET
    (t->z >= 0) &&
#endif
#ifndef T8_DTRI_TO_DTET
    (t->y - t->x <= 0) && (t->y == t->x ? t->type == 0 : 1) &&
#else
    (t->z - t->x <= 0) && (t->y - t->z <= 0) &&
    /* If y and z are equal, the types 0, 4 and 5 are allowed */
    (t->y == t->z ? (t->type == 0 || 4 <= t->type) : 1) &&
    /* If x and z are equal, the types 0, 1 and 2 are allowed */
    (t->x == t->z ? t->type <= 2 : 1) &&
#endif
    1;
  return is_inside;
}

int
t8_dtri_is_equal (const t8_dtri_t * t1, const t8_dtri_t * t2)
{
  return (t1->level == t1->level && t1->type == t2->type &&
          t1->x == t1->x && t1->y == t1->y
#ifdef T8_DTRI_TO_DTET
          && t1->z == t1->z
#endif
    );
}

/* we check if t1 and t2 lie in the same subcube and have
 * the same level and parent type */
int
t8_dtri_is_sibling (const t8_dtri_t * t1, const t8_dtri_t * t2)
{
  t8_dtri_coord_t     exclorx, exclory;
#ifdef T8_DTRI_TO_DTET
  t8_dtri_coord_t     exclorz;
#endif

  t8_dtri_cube_id_t   cid1, cid2;

  if (t1->level == 0) {
    return t2->level == 0 && t1->x == t2->x && t1->y == t2->y &&
#ifdef T8_DTRI_TO_DTET
      t1->z == t2->z &&
#endif
      1;
  }

  exclorx = t1->x ^ t2->x;
  exclory = t1->y ^ t2->y;
#ifdef T8_DTRI_TO_DTET
  exclorz = t1->z ^ t2->z;
#endif
  cid1 = compute_cubeid (t1, t1->level);
  cid2 = compute_cubeid (t2, t2->level);

  return
    (t1->level == t2->level) &&
    ((exclorx & ~T8_DTRI_LEN (t1->level)) == 0) &&
    ((exclory & ~T8_DTRI_LEN (t1->level)) == 0) &&
#ifdef T8_DTRI_TO_DTET
    ((exclorz & ~T8_DTRI_LEN (t1->level)) == 0) &&
#endif
    t8_dtri_cid_type_to_parenttype[cid1][t1->type] ==
    t8_dtri_cid_type_to_parenttype[cid2][t2->type];
}

int
t8_dtri_is_parent (const t8_dtri_t * t, const t8_dtri_t * c)
{
  t8_dtri_cube_id_t   cid;

  cid = compute_cubeid (c, c->level);
  return
    (t->level + 1 == c->level) &&
    (t->x == (c->x & ~T8_DTRI_LEN (c->level))) &&
    (t->y == (c->y & ~T8_DTRI_LEN (c->level))) &&
#ifdef T8_DTRI_TO_DTET
    (t->z == (c->z & ~T8_DTRI_LEN (c->level))) &&
#endif
    t->type == t8_dtri_cid_type_to_parenttype[cid][c->type] && 1;
}

int
t8_dtri_is_ancestor (const t8_dtri_t * t, const t8_dtri_t * c)
{
  t8_dtri_coord_t     n1, n2;
  t8_dtri_coord_t     exclorx;
  t8_dtri_coord_t     exclory;
#ifdef T8_DTRI_TO_DTET
  t8_dtri_coord_t     exclorz;
  t8_dtri_coord_t     dir3;
  int                 sign;
#endif
  int8_t              type_t;

  if (t->level > c->level) {
    return 0;
  }
  if (t->level == c->level) {
    return t8_dtri_is_equal (t, c);
  }

  exclorx = (t->x ^ c->x) >> (T8_DTRI_MAXLEVEL - t->level);
  exclory = (t->y ^ c->y) >> (T8_DTRI_MAXLEVEL - t->level);
#ifdef T8_DTRI_TO_DTET
  exclorz = (t->z ^ c->z) >> (T8_DTRI_MAXLEVEL - t->level);
#endif

  if (exclorx == 0 && exclory == 0
#ifdef T8_DTRI_TO_DTET
      && exclorz == 0
#endif
    ) {
    /* t and c have the same cube as ancestor.
     * Now check if t has the correct type to be c's ancestor. */
    type_t = t->type;
#ifndef T8_DTRI_TO_DTET
    /* 2D */
    n1 = (type_t == 0) ? c->x - t->x : c->y - t->y;
    n2 = (type_t == 0) ? c->y - t->y : c->x - t->x;

    return !(n1 >= T8_DTRI_LEN (t->level) || n2 < 0 || n2 - n1 > 0
             || (n2 == n1 && c->type == 1 - type_t));
#else
    /* 3D */
      /* *INDENT-OFF* */
      n1 = type_t / 2 == 0 ? c->x - t->x :
           type_t / 2 == 2 ? c->z - t->z : c->y - t->y;
      n2 = (type_t + 3) % 6 == 0 ? c->x - t->x :
           (type_t + 3) % 6 == 2 ? c->z - t->z : c->y - t->y;
      dir3 = type_t % 3 == 2 ? c->x - t->x :
             type_t % 3 == 0 ? c->z - t->z : c->y - t->y;
      sign = (type_t % 2 == 0) ? 1 : -1;
      /* *INDENT-ON* */

    type_t += 6;                /* We need to compute modulo six and want
                                   to avoid negative numbers when substracting from type_t. */
    return !(n1 >= T8_DTRI_LEN (t->level) || n2 < 0 || dir3 - n1 > 0
             || n2 - dir3 > 0 || (dir3 == n1
                                  && (c->type == (type_t + sign * 1) % 6
                                      || t->type == (type_t + sign * 2) % 6
                                      || t->type == (type_t + sign * 3) % 6))
             || (dir3 == n2
                 && (c->type == (type_t - sign * 1) % 6
                     || t->type == (type_t - sign * 2) % 6
                     || t->type == (type_t - sign * 3) % 6)));
#endif
  }
  else {
    return 0;
  }
}

/* Compute the linear id of the first descendant of a triangle/tet */
static uint64_t
t8_dtri_linear_id_first_desc (const t8_dtri_t * t)
{
  /* The id of the first descendant is the id of t in a uniform maxlevel
   * refinement */
  return t8_dtri_linear_id (t, T8_DTRI_MAXLEVEL);
}

/* Compute the linear id of the last descendant of a triangle/tet */
static              uint64_t
t8_dtri_linear_id_last_desc (const t8_dtri_t * t)
{
  uint64_t            id = 0, t_id;
  int                 exponent;

  /* The id of the last descendant consists of the id of t in
   * the first digits and then the local ids of all last children
   * (3 in 2d, 7 in 3d)
   */
  t_id = t8_dtri_linear_id (t, t->level);
  exponent = T8_DTRI_MAXLEVEL - t->level;
  /* Set the last bits to the local ids of always choosing the last child
   * of t */
  id = (((uint64_t) 1) << T8_DTRI_DIM * exponent) - 1;
  /* Set the first bits of id to the id of t itself */
  id |= t_id << exponent;
  return id;
}

uint64_t
t8_dtri_linear_id (const t8_dtri_t * t, int level)
{
  uint64_t            id = 0;
  int8_t              type_temp = 0;
  t8_dtri_cube_id_t   cid;
  int                 i;
  int                 exponent;
  int                 my_level;

  T8_ASSERT (0 <= level && level <= T8_DTRI_MAXLEVEL);
  my_level = t->level;
  exponent = 0;
  /* If the given level is bigger than t's level
   * we first fill up with the ids of t's descendants at t's
   * origin with the same type as t */
  if (level > my_level) {
    exponent = (level - my_level) * T8_DTRI_DIM;
  }
  level = my_level;
  type_temp = compute_type (t, level);
  for (i = level; i > 0; i--) {
    cid = compute_cubeid (t, i);
    id |= ((uint64_t) t8_dtri_type_cid_to_Iloc[type_temp][cid]) << exponent;
    exponent += T8_DTRI_DIM;    /* multiply with 4 (2d) resp. 8  (3d) */
    type_temp = t8_dtri_cid_type_to_parenttype[cid][type_temp];
  }
  return id;
}

void
t8_dtri_init_linear_id (t8_dtri_t * t, uint64_t id, int level)
{
  int                 i;
  int                 offset_coords, offset_index;
  const int           children_m1 = T8_DTRI_CHILDREN - 1;
  uint64_t            local_index;
  t8_dtri_cube_id_t   cid;
  t8_dtri_type_t      type;

  T8_ASSERT (0 <= id && id <= ((uint64_t) 1) << (T8_DTRI_DIM * level));

  t->level = level;
  t->x = 0;
  t->y = 0;
#ifdef T8_DTRI_TO_DTET
  t->z = 0;
#else
  t->n = 0;
#endif
  type = 0;                     /* This is the type of the root triangle */
  for (i = 1; i <= level; i++) {
    offset_coords = T8_DTRI_MAXLEVEL - i;
    offset_index = level - i;
    /* Get the local index of T's ancestor on level i */
    local_index = (id >> (T8_DTRI_DIM * offset_index)) & children_m1;
    /* Get the type and cube-id of T's ancestor on level i */
    cid = t8_dtri_parenttype_Iloc_to_cid[type][local_index];
    type = t8_dtri_parenttype_Iloc_to_type[type][local_index];
    t->x |= (cid & 1) ? 1 << offset_coords : 0;
    t->y |= (cid & 2) ? 1 << offset_coords : 0;
#ifdef T8_DTRI_TO_DTET
    t->z |= (cid & 4) ? 1 << offset_coords : 0;
#endif
  }
  t->type = type;
}

void
t8_dtri_init_root (t8_dtri_t * t)
{
  t->level = 0;
  t->type = 0;
  t->x = 0;
  t->y = 0;
#ifdef T8_DTRI_TO_DTET
  t->z = 0;
#else
  t->n = 0;
#endif
}

/* Stores in s the triangle that is obtained from t by going 'increment' positions
 * along the SFC of a uniform refinement of level 'level'.
 * 'increment' must be greater than -4 (-8) and smaller than +4 (+8).
 * Before calling this function s should store the same entries as t. */
static void
t8_dtri_succ_pred_recursion (const t8_dtri_t * t, t8_dtri_t * s, int level,
                             int increment)
{
  t8_dtri_type_t      type_level, type_level_p1;
  t8_dtri_cube_id_t   cid;
  int                 local_index;
  int                 sign;

  /* We exclude the case level = 0, because the root triangle does
   * not have a successor. */
  T8_ASSERT (1 <= level && level <= t->level);
  T8_ASSERT (-T8_DTRI_CHILDREN < increment && increment < T8_DTRI_CHILDREN);

  if (increment == 0) {
    t8_dtri_copy (t, s);
    return;
  }
  cid = compute_cubeid (t, level);
  type_level = compute_type (t, level);
  local_index = t8_dtri_type_cid_to_Iloc[type_level][cid];
  local_index =
    (local_index + T8_DTRI_CHILDREN + increment) % T8_DTRI_CHILDREN;
  if (local_index == 0) {
    sign = increment < 0 ? -1 : increment > 0;
    t8_dtri_succ_pred_recursion (t, s, level - 1, sign);
    type_level_p1 = s->type;    /* We stored the type of s at level-1 in s->type */
  }
  else {
    type_level_p1 = t8_dtri_cid_type_to_parenttype[cid][type_level];
  }
  type_level = t8_dtri_parenttype_Iloc_to_type[type_level_p1][local_index];
  cid = t8_dtri_parenttype_Iloc_to_cid[type_level_p1][local_index];
  s->type = type_level;
  s->level = level;
  /* Set the x,y(,z) coordinates at level to the cube-id. */
  /* TODO: check if we set the correct bits here! */
  s->x =
    (cid & 1 ? s->x | 1 << (T8_DTRI_MAXLEVEL - level) :
     s->x & ~(1 << (T8_DTRI_MAXLEVEL - level)));
  s->y =
    (cid & 2 ? s->y | 1 << (T8_DTRI_MAXLEVEL - level) :
     s->y & ~(1 << (T8_DTRI_MAXLEVEL - level)));
#ifdef T8_DTRI_TO_DTET
  s->z =
    (cid & 4 ? s->z | 1 << (T8_DTRI_MAXLEVEL - level) :
     s->z & ~(1 << (T8_DTRI_MAXLEVEL - level)));
#endif
}

void
t8_dtri_successor (const t8_dtri_t * t, t8_dtri_t * s, int level)
{
  t8_dtri_copy (t, s);
  t8_dtri_succ_pred_recursion (t, s, level, 1);
}

void
t8_dtri_first_descendant (const t8_dtri_t * t, t8_dtri_t * s)
{
  uint64_t            id;

  /* Compute the linear id of the first descendant */
  id = t8_dtri_linear_id_first_desc (t);
  /* The first descendant has exactly this id */
  t8_dtri_init_linear_id (s, id, T8_DTRI_MAXLEVEL);
}

void
t8_dtri_last_descendant (const t8_dtri_t * t, t8_dtri_t * s)
{
  uint64_t            id;

  /* Compute the linear id of t's last descendant */
  id = t8_dtri_linear_id_last_desc (t);
  /* Set s to math this linear id */
  t8_dtri_init_linear_id (s, id, T8_DTRI_MAXLEVEL);
}

void
t8_dtri_predecessor (const t8_dtri_t * t, t8_dtri_t * s, int level)
{
  t8_dtri_copy (t, s);
  t8_dtri_succ_pred_recursion (t, s, level, -1);
}

int
t8_dtri_ancestor_id (const t8_dtri_t * t, int level)
{
  t8_dtri_cube_id_t   cid;
  t8_dtri_type_t      type;

  T8_ASSERT (0 <= level && level <= T8_DTRI_MAXLEVEL);
  T8_ASSERT (level <= t->level);

  cid = compute_cubeid (t, level);
  type = compute_type (t, level);
  return t8_dtri_type_cid_to_Iloc[type][cid];
}

int
t8_dtri_child_id (const t8_dtri_t * t)
{
  return t8_dtri_type_cid_to_Iloc[t->type][compute_cubeid (t, t->level)];
}

int
t8_dtri_get_level (const t8_dtri_t * t)
{
  return t->level;
}
