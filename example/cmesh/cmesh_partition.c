/*
  This file is part of t8code.
  t8code is a C library to manage a collection (a forest) of multiple
  connected adaptive space-trees of general element types in parallel.

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

#include <sc_refcount.h>
#include <t8_default.h>
#include <t8_forest/t8_forest_adapt.h>
#include <t8_forest.h>
#include <t8_cmesh_vtk.h>
#include <t8_cmesh/t8_cmesh_partition.h>
#include <p4est_connectivity.h>
#include <p8est_connectivity.h>
#include <sc_shmem.h>

/* TODO: rename this file to t8_something */

static void
t8_random_partition (int level)
{
  t8_cmesh_t          cmesh, cmesh_part, cmesh_part2;
  char                file[BUFSIZ];
  p8est_connectivity_t *conn;
  int                 mpirank, mpiret, mpisize;

  mpiret = sc_MPI_Comm_rank (sc_MPI_COMM_WORLD, &mpirank);
  SC_CHECK_MPI (mpiret);
  mpiret = sc_MPI_Comm_size (sc_MPI_COMM_WORLD, &mpisize);
  SC_CHECK_MPI (mpiret);

  conn = p8est_connectivity_new_brick (2, 2, 2, 0, 0, 0);

  cmesh = t8_cmesh_new_from_p8est (conn, sc_MPI_COMM_WORLD, 0, 1);
  p8est_connectivity_destroy (conn);
  snprintf (file, BUFSIZ, "t8_brick_random_%04d", mpirank);
  t8_cmesh_vtk_write_file (cmesh, file, 1.);

  t8_cmesh_init (&cmesh_part);
  //t8_cmesh_set_partition_from (cmesh_part, cmesh, level, NULL);

  t8_cmesh_set_derive (cmesh_part, cmesh);
  t8_cmesh_set_partition_offsets (cmesh_part,
                                  t8_cmesh_offset_random
                                  (sc_MPI_COMM_WORLD,
                                   t8_cmesh_get_num_trees (cmesh), 0));
  t8_cmesh_commit (cmesh_part, sc_MPI_COMM_WORLD);

  if (mpisize > 1 && 1) {
    t8_cmesh_init (&cmesh_part2);

    t8_cmesh_set_derive (cmesh_part2, cmesh_part);
    t8_cmesh_set_partition_offsets (cmesh_part2,
                                    t8_cmesh_offset_random
                                    (sc_MPI_COMM_WORLD,
                                     t8_cmesh_get_num_trees (cmesh), 1));
    t8_cmesh_commit (cmesh_part2, sc_MPI_COMM_WORLD);

    snprintf (file, BUFSIZ, "t8_brick_partition_random2_%04d", mpirank);
    t8_cmesh_vtk_write_file (cmesh_part2, file, 1.0);
  }
  else {
    cmesh_part2 = cmesh_part;
  }
  snprintf (file, BUFSIZ, "t8_brick_partition_random_%04d", mpirank);
  t8_cmesh_vtk_write_file (cmesh_part, file, 1.0);
  t8_cmesh_unref (&cmesh_part2, sc_MPI_COMM_WORLD);
}

/* Create a coarse mesh from a p4est brick connectivity.
 * Then derive a new partitioned cmesh from it according to
 * a uniform refinement of a given level.
 * If partition_from is nonzero then the initial coarse mesh
 * will also be partitioned. Otherwise replicated.
 */
static void
t8_partition (int level, int partition_from)
{
  t8_cmesh_t          cmesh, cmesh_part, cmesh_part2;
  char                file[BUFSIZ];
  p4est_connectivity_t *conn;
  int                 mpirank, mpiret, mpisize;

  mpiret = sc_MPI_Comm_rank (sc_MPI_COMM_WORLD, &mpirank);
  SC_CHECK_MPI (mpiret);
  mpiret = sc_MPI_Comm_size (sc_MPI_COMM_WORLD, &mpisize);
  SC_CHECK_MPI (mpiret);

  conn = p4est_connectivity_new_brick (3, 2, 0, 0);
  cmesh =
    t8_cmesh_new_from_p4est (conn, sc_MPI_COMM_WORLD, 0, partition_from);
  p4est_connectivity_destroy (conn);
  snprintf (file, BUFSIZ, "t8_brick_%04d", mpirank);
  t8_cmesh_vtk_write_file (cmesh, file, 1.);

  t8_cmesh_init (&cmesh_part);
  t8_cmesh_set_derive (cmesh_part, cmesh);
  t8_cmesh_set_partition_uniform (cmesh_part, level);
  t8_cmesh_commit (cmesh_part, sc_MPI_COMM_WORLD);
  if (mpisize > 1 && 1) {
    t8_cmesh_init (&cmesh_part2);
    t8_cmesh_set_derive (cmesh_part2, cmesh_part);
    t8_cmesh_set_partition_offsets (cmesh_part2,
                                    t8_cmesh_offset_concentrate
                                    (1, sc_MPI_COMM_WORLD,
                                     t8_cmesh_get_num_trees (cmesh)));
    t8_cmesh_commit (cmesh_part2, sc_MPI_COMM_WORLD);
    snprintf (file, BUFSIZ, "t8_brick_partition2_%04d", mpirank);
    t8_cmesh_vtk_write_file (cmesh_part2, file, 1.0);
  }
  else {
    cmesh_part2 = cmesh_part;
  }
  snprintf (file, BUFSIZ, "t8_brick_partition_%04d", mpirank);
  t8_cmesh_vtk_write_file (cmesh_part, file, 1.0);
  t8_cmesh_unref (&cmesh_part2, sc_MPI_COMM_WORLD);
}

int
main (int argc, char **argv)
{
  int                 mpiret, loop;
  int                 level;

  mpiret = sc_MPI_Init (&argc, &argv);
  SC_CHECK_MPI (mpiret);

  sc_init (sc_MPI_COMM_WORLD, 1, 1, NULL, SC_LP_ESSENTIAL);
  t8_init (SC_LP_DEFAULT);

  level = 1;

  t8_partition (level, 1);
  for (loop = 0; loop < 1; loop++) {
    t8_random_partition (level);
  }

  t8_partition (level, 0);

  sc_finalize ();

  mpiret = sc_MPI_Finalize ();
  SC_CHECK_MPI (mpiret);
  return 0;
}