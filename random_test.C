/******************************************************************************
 *
 * Copyright (c) 2013-2016, Lawrence Livermore National Security, LLC.
 * Produced at the Lawrence Livermore National Laboratory
 * Written by William Arrighi wjarrighi@llnl.gov
 * CODE-686965
 * All rights reserved.
 *
 * This file is part of libROM.
 * For details, see https://computation.llnl.gov/librom
 * Please also read README_BSD_NOTICE.
 *
 * Redistribution and use in source and binary forms, with or without
 * modifications, are permitted provided that the following conditions are met:
 *
 *    o Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the disclaimer below.
 *    o Redistribution in binary form must reproduce the above copyright
 *      notice, this list of conditions and the disclaimer (as noted below) in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    o Neither the name of the LLNS/LLNL nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY,
 * LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OR SUCH DAMAGE.
 *
 *****************************************************************************/

// Description: A test of both the static and incremental fast update
//              algorithms and samplers.  Random numbers are generated as the
//              state vectors in such a way that the global state vector is the
//              same for all processor decompositions when
//              dim * number of processors is constant.

#include "IncrementalSVDBasisGenerator.h"
#include "StaticSVDBasisGenerator.h"

#include "mpi.h"

#include <stdio.h>

CAROM::Matrix*
transposeMult(
   const CAROM::Matrix* umat,
   const CAROM::Matrix* dmat)
{
   int rank;
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   int umat_num_cols = umat->numColumns();
   int dmat_num_cols = dmat->numColumns();
   int dmat_num_rows = dmat->numRows();
   int new_mat_size = umat_num_cols*dmat_num_cols;
   CAROM::Matrix* local_result = new CAROM::Matrix(umat_num_cols,
      dmat_num_cols,
      false);
   for (int umat_col = 0; umat_col < umat_num_cols; ++umat_col) {
      for (int dmat_col = 0; dmat_col < dmat_num_cols; ++dmat_col) {
         int umat_row = dmat_num_rows*rank;
         local_result->item(umat_col, dmat_col) = 0.0;
         for (int entry = 0; entry < dmat_num_rows; ++entry) {
            local_result->item(umat_col, dmat_col) +=
               umat->item(umat_row, umat_col)*dmat->item(entry, dmat_col);
            ++umat_row;
         }
      }
   }
   CAROM::Matrix* result;
   int num_procs;
   MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
   if (num_procs > 1) {
      result = new CAROM::Matrix(umat_num_cols, dmat_num_cols, false);
      MPI_Allreduce(&local_result->item(0, 0),
         &result->item(0, 0),
         new_mat_size,
         MPI_DOUBLE,
         MPI_SUM,
         MPI_COMM_WORLD);
      delete local_result;
   }
   else {
      result = local_result;
   }
   return result;
}

int
main(
   int argc,
   char* argv[])
{
   // Initialize MPI and get the number of processors and this processor's
   // rank.
   MPI_Init(&argc, &argv);
   int size;
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   int rank;
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);

   // Given the number of processors and the rank of this processor set the
   // dimension of the problem.
   int dim;
   if (size == 1) {
      dim = 100;
   }
   else if (size == 2) {
      dim = 50;
   }
   else if (size == 4) {
      dim = 25;
   }
   else if (size == 5) {
      dim = 20;
   }
   else if (size == 10) {
      dim = 10;
   }
   else if (size == 20) {
      dim = 5;
   }
   else if (size == 25) {
      dim = 4;
   }
   else if (size == 50) {
      dim = 2;
   }
   else if (size == 100) {
      dim = 1;
   }
   else {
      printf("Illegal number of procs\n");
      printf("Allowed number of proces is 1, 2, 4, 5, 10, 20, 25, 50, 100.\n");
      return 1;
   }

   int num_samples = 10;
   int num_lin_dep_samples = 2;
   int num_lin_indep_samples = num_samples - num_lin_dep_samples;

   // Construct the incremental basis generator to use the fast update
   // incremental algorithm and the incremental sampler.
   CAROM::IncrementalSVDBasisGenerator inc_basis_generator(dim,
      1.0e-6,
      false,
      true,
      1.0e-6,
      num_samples,
      1.0e-2,
      0.001,
      "",
      CAROM::Database::HDF5,
      0.1,
      0.8,
      5.0,
      true);

   // Construct the static basis generator for the static algorithm and the
   // static sampler.
   CAROM::StaticSVDBasisGenerator static_basis_generator(dim,
      num_samples,
      "",
      true);

   // Initialize random number generator.
   srand(1);

   // Allocate an array for each sample.
   double** M = new double* [num_samples];
   for (int i = 0; i < num_samples; ++i) {
      M[i] = new double [dim];
   }

   // Call the random number generator enough times so that this processor
   // generates it's part of the global sample.
   for (int i = 0; i < dim*num_samples*rank; ++i) {
      double random = rand();
      random = random/RAND_MAX;
   }

   // Fill in the samples.
   for (int i = 0; i < dim; ++i) {
      for (int j = 0; j < num_samples; ++j) {
         double random = rand();
         random = random/RAND_MAX;
         M[j][i] = random;
      }
   }

   // Now call the random number generator enough times so that each processor
   // has called it the same number of times after this is done.
   for (int i = 0; i < dim*num_samples*(size-rank-1); ++i) {
      double random = rand();
      random = random/RAND_MAX;
   }

   // Now overwrite the linearly depdendent samples with linear combinations of
   // the linearly independent samples.  The coefficients are other random
   // numbers.  Since each processor has called the random number generator the
   // same number of times when we get here, the random coefficients will be
   // the same on each processor which is what is needed.
   for (int i = 0; i < num_lin_dep_samples; ++i) {
      int col = num_samples - i - 1;
      for (int j = 0; j < dim; ++j) {
         M[col][j] = 0;
      }
      for (int j = 0; j < num_lin_indep_samples; ++j) {
         double random = rand();
         random = random/RAND_MAX;
         for (int k = 0; k < dim; ++k) {
            M[col][k] += random*M[j][k];
         }
      }
   }

   // Take the samples.
   bool status = true;
   for (int i = 0; i < num_samples; ++i) {
      if (inc_basis_generator.isNextSample(0.01*i)) {
         status = inc_basis_generator.takeSample(M[i], 0.01*i, 0.01);
         if (!status) {
            break;
         }
         inc_basis_generator.computeNextSampleTime(M[i], M[i], 0.01*i);
      }
      if (i < num_lin_indep_samples &&
          static_basis_generator.isNextSample(0.01*i)) {
         status = static_basis_generator.takeSample(M[i], 0.01*i, 0.01);
         if (!status) {
            break;
         }
         static_basis_generator.computeNextSampleTime(M[i], M[i], 0.01*i);
      }
   }
   if (status) {
      inc_basis_generator.endSamples();
      static_basis_generator.endSamples();

      // Get the basis vectors from the 2 different algorithms.
      const CAROM::Matrix* inc_basis = inc_basis_generator.getBasis();
      const CAROM::Matrix* static_basis = static_basis_generator.getBasis();

      // Compute the product of the transpose of the static basis and the
      // incremental basis.  This should be a unitary matrix.
      CAROM::Matrix* test = transposeMult(static_basis, inc_basis);
      if (rank == 0) {
         for (int row = 0; row < test->numRows(); ++row) {
            for (int col = 0; col < test->numColumns(); ++col) {
               printf("%.16e ", test->item(row, col));
            }
            printf("\n");
         }
      }
      delete test;
   }

   // Clean up.
   for (int i = 0; i < num_samples; ++i) {
      delete [] M[i];
   }
   delete [] M;

   // Finalize MPI and return.
   MPI_Finalize();
   return !status;
}
