// @HEADER
//
// ***********************************************************************
//
//           Amesos2: Templated Direct Sparse Solver Package
//                  Copyright 2010 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ***********************************************************************
//
// @HEADER

/**
  \file   Amesos2_Util_decl.hpp
  \author Eric T Bavier <etbavier@sandia.gov>
  \date   Thu May 27 13:11:13 CDT 2010

  \brief  Utility functions for Amesos2
*/

#ifndef AMESOS2_UTIL_DECL_HPP
#define AMESOS2_UTIL_DECL_HPP

#include "Amesos2_config.h"

#include <Teuchos_RCP.hpp>
#include <Teuchos_BLAS_types.hpp>
#include <Teuchos_ArrayView.hpp>
#include <Teuchos_FancyOStream.hpp>

#ifdef HAVE_AMESOS2_EPETRA
#  include <Epetra_Map.h>
#  include <Epetra_Comm.h>
#  include <Epetra_SerialComm.h>
#  ifdef HAVE_MPI
#    include <Epetra_MpiComm.h>
#  endif
#  include <Epetra_BlockMap.h>
#endif

#include <Tpetra_Map.hpp>

#include "Amesos2_Util_is_same.hpp"

namespace Amesos {

  namespace Util {

    using Teuchos::RCP;
    using Teuchos::ArrayView;

    /** \brief Indicates that the concrete class has a special
     * implementation that should be called.
     *
     * Matrix Adapters \c typedef this as \c {get_ccs|get_crs}_spec
     * indicating that the concrete adapter has a special
     * implementation for either the \c getCrs or \c getCcs functions.
     */
    struct has_special_impl {};
    struct no_special_impl {};

    /// Indicates that the object of an adapter provides row access to its data.
    struct row_access {};
    /// Indicates that the object of an adapter provides column access to its data.
    struct col_access {};

    /** \enum EDistribution
     *
     * An enum of this type is expected by the Matrix adapters' getCrs
     * and getCcs functions to describe the layout of the
     * representation on the calling processors.
     */
    typedef enum {
      Distributed,                /**< no processor has a view of the entire matrix, only local pieces */
      Distributed_No_Overlap,     /**< no row or column may be present on more than one processor */
      Globally_Replicated,        /**< each processor has a view of the entire matrix */
      Rooted                      /**< only \c rank=0 has a full view, all others have nothing. */
    } EDistribution;

    /** \enum EStorage_Ordering
     *
     * This enum also used by the matrix adapters to indicate whether
     * the indices of the representation must be in sorted order or
     * can have an arbitrary order.
     */
    typedef enum {
      Sorted_Indices,             /**< row/col indices need to appear in sorted order */
      Arbitrary                   /**< index order can be arbitrary */
    } EStorage_Ordering;

#ifdef HAVE_AMESOS2_EPETRA
    /**
     * \brief Transform an Epetra_Map object into a Tpetra::Map
     */
    template <typename LO, typename GO, typename GS, typename Node>
    RCP<Tpetra::Map<LO,GO,Node> >
    epetra_map_to_tpetra_map(const Epetra_BlockMap& map);

    /**
     * \brief Transform a Tpetra::Map object into an Epetra_Map
     */
    template <typename LO, typename GO, typename GS, typename Node>
    RCP<Epetra_Map>
    tpetra_map_to_epetra_map(const Tpetra::Map<LO,GO,Node>& map);

    /**
     * \brief Transform an Epetra_Comm object into a Teuchos::Comm object
     */
    const RCP<const Teuchos::Comm<int> > to_teuchos_comm(RCP<const Epetra_Comm> c);

    /**
     * \brief Transfrom a Teuchos::Comm object into an Epetra_Comm object
     */
    const RCP<const Epetra_Comm> to_epetra_comm(RCP<const Teuchos::Comm<int> > c);

#endif	// HAVE_AMESOS2_EPETRA

    /**
     * Transposes the compressed sparse matrix representation.
     */
    template <typename Scalar,
	      typename GlobalOrdinal,
	      typename GlobalSizeT>
    void transpose(ArrayView<Scalar> vals,
		   ArrayView<GlobalOrdinal> indices,
		   ArrayView<GlobalSizeT> ptr,
		   ArrayView<Scalar> trans_vals,
		   ArrayView<GlobalOrdinal> trans_indices,
		   ArrayView<GlobalSizeT> trans_ptr);

    /**
     * \brief Scales a 1-D representation of a multivector.
     *
     * \param [in/out] vals The values of the multi-vector.  On exit will contain the scaled values.
     * \param [in] l The length of each vector in the multivector
     * \param [in] ld The leading dimension of the multivector
     * \param [in] s Contains the scaling factors of the diagonal scaling matrix
     *
     * The first vector will be scaled by \c s[0] , the second vector
     * by \c s[1] , etc.
     */
    template <typename Scalar1, typename Scalar2>
    void scale(ArrayView<Scalar1> vals, size_t l,
	       size_t ld, ArrayView<Scalar2> s);

    /**
     * \brief Scales a 1-D representation of a multivector.
     *
     * \param [in/out] vals The values of the multi-vector.  On exit will contain the scaled values.
     * \param [in] l The length of each vector in the multivector
     * \param [in] ld The leading dimension of the multivector
     * \param [in] s Contains the scaling factors of the diagonal scaling matrix
     *
     * Scales each vector by diag(s), with the scaling multiplication
     * being performed by the `binary_op' parameter.  BinaryOp is some
     * class that defines a \c operator() method as
     *
     * \code
     * Scalar1 operator()(Scalar1 x, Scalar2 y){  }
     * \endcode
     */
    template <typename Scalar1, typename Scalar2, class BinaryOp>
    void scale(ArrayView<Scalar1> vals, size_t l,
	       size_t ld, ArrayView<Scalar2> s, BinaryOp binary_op);


    /**
     * \brief Computes the true residual
     *
     * Computes the true residual, \f$ B - A*X \f$ , and prints the results.
     *
     * \param A Matrix
     * \param X Vector
     * \param B Vector
     * \param trans  \c true = use transpose for matrix-vector multiply
     * \param prefix string prefix to prints before rest of output
     *
     */
    template <typename Matrix,
	      typename Vector>
    void computeTrueResidual(const RCP<Matrix>& A,
			     const RCP<Vector>& X,
			     const RCP<Vector>& B,
			     const Teuchos::ETransp trans=Teuchos::NO_TRANS,
			     const std::string prefix="");


    /* We assume that Matrix and Vector are some instance of a
     * Amesos::MatrixAdapter or a Amesos::MultiVecAdapter, or at least implement
     * the required methods
     */
    template< typename Matrix,
	      typename Vector>
    void computeVectorNorms(const RCP<Matrix> X,
			    const RCP<Vector> B,
			    std::string prefix="");


    /// Uses a heuristic to set the maximum number of processors
    template <typename Matrix>
    void setMaxProcesses(const RCP<Matrix>& A,
			 int& maxProcesses);


    /// Prints a line of 70 "-"s on std::cout.
    void printLine( Teuchos::FancyOStream &out );

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    /*
     * The following represents a general way of getting a CRS or CCS
     * representation of a matrix with implicit type conversions.  The
     * \c get_crs_helper and \c get_ccs_helper classes are templated
     * on 4 types:
     *
     * - A Matrix type (conforming to the Amesos2 MatrixAdapter interface)
     * - A scalar type
     * - A global ordinal type
     * - A global size type
     *
     * The last three template types correspond to the input argument
     * types.  For example, if the scalar type is \c double , then we
     * require that the \c nzvals argument is a \c
     * Teuchos::ArrayView<double> type.
     *
     * These helpers perform any type conversions that must be
     * performed to go between the Matrix's types and the input types.
     * If no conversions are necessary the static functions can be
     * effectively inlined.
     */

    template <class M, typename S, typename GO, typename GS, class Op>
    struct same_gs_helper
    {
      static void do_get(const Teuchos::Ptr<M> mat,
			 const ArrayView<typename M::scalar_t> nzvals,
			 const ArrayView<typename M::global_ordinal_t> indices,
			 const ArrayView<GS> pointers,
			 GS& nnz, EDistribution distribution,
			 EStorage_Ordering ordering)
      {
	Op::apply(mat, nzvals, indices, pointers, nnz, distribution, ordering);
      }
    };

    template <class M, typename S, typename GO, typename GS, class Op>
    struct diff_gs_helper
    {
      static void do_get(const Teuchos::Ptr<M> mat,
			 const ArrayView<typename M::scalar_t> nzvals,
			 const ArrayView<typename M::global_ordinal_t> indices,
			 const ArrayView<GS> pointers,
			 GS& nnz, EDistribution distribution,
			 EStorage_Ordering ordering)
      {
	typedef typename M::global_size_t mat_gs_t;
	typename ArrayView<GS>::size_type i, size = pointers.size();
	Teuchos::Array<mat_gs_t> pointers_tmp(size);
	mat_gs_t nnz_tmp;

	Op::apply(mat, nzvals, indices, pointers_tmp, nnz_tmp, distribution, ordering);

	for (i = 0; i < size; ++i){
	  pointers[i] = Teuchos::as<GS>(pointers_tmp[i]);
	}
	nnz = Teuchos::as<GS>(nnz_tmp);
      }
    };

    template <class M, typename S, typename GO, typename GS, class Op>
    struct same_go_helper
    {
      static void do_get(const Teuchos::Ptr<M> mat,
			 const ArrayView<typename M::scalar_t> nzvals,
			 const ArrayView<GO> indices,
			 const ArrayView<GS> pointers,
			 GS& nnz, EDistribution distribution,
			 EStorage_Ordering ordering)
      {
	typedef typename M::global_size_t mat_gs_t;
	if_then_else<is_same<GS,mat_gs_t>::value,
	  same_gs_helper<M,S,GO,GS,Op>,
	  diff_gs_helper<M,S,GO,GS,Op> >::type::do_get(mat, nzvals, indices,
						       pointers, nnz, distribution,
						       ordering);
      }
    };

    template <class M, typename S, typename GO, typename GS, class Op>
    struct diff_go_helper
    {
      static void do_get(const Teuchos::Ptr<M> mat,
			 const ArrayView<typename M::scalar_t> nzvals,
			 const ArrayView<GO> indices,
			 const ArrayView<GS> pointers,
			 GS& nnz, EDistribution distribution,
			 EStorage_Ordering ordering)
      {
	typedef typename M::global_ordinal_t mat_go_t;
	typedef typename M::global_size_t mat_gs_t;
	typename ArrayView<GO>::size_type i, size = indices.size();
	Teuchos::Array<mat_go_t> indices_tmp(size);

	if_then_else<is_same<GS,mat_gs_t>::value,
	  same_gs_helper<M,S,GO,GS,Op>,
	  diff_gs_helper<M,S,GO,GS,Op> >::type::do_get(mat, nzvals, indices_tmp,
						       pointers, nnz, distribution,
						       ordering);

	for (i = 0; i < size; ++i){
	  indices[i] = Teuchos::as<GO>(indices_tmp[i]);
	}
      }
    };

    template <class M, typename S, typename GO, typename GS, class Op>
    struct same_scalar_helper
    {
      static void do_get(const Teuchos::Ptr<M> mat,
			 const ArrayView<S> nzvals,
			 const ArrayView<GO> indices,
			 const ArrayView<GS> pointers,
			 GS& nnz, EDistribution distribution,
			 EStorage_Ordering ordering)
      {
	typedef typename M::global_ordinal_t mat_go_t;
	if_then_else<is_same<GO,mat_go_t>::value,
	  same_go_helper<M,S,GO,GS,Op>,
	  diff_go_helper<M,S,GO,GS,Op> >::type::do_get(mat, nzvals, indices,
						       pointers, nnz, distribution,
						       ordering);
      }
    };

    template <class M, typename S, typename GO, typename GS, class Op>
    struct diff_scalar_helper
    {
      static void do_get(const Teuchos::Ptr<M> mat,
			 const ArrayView<S> nzvals,
			 const ArrayView<GO> indices,
			 const ArrayView<GS> pointers,
			 GS& nnz, EDistribution distribution,
			 EStorage_Ordering ordering)
      {
	typedef typename M::scalar_t mat_scalar_t;
	typedef typename M::global_ordinal_t mat_go_t;
	typename ArrayView<S>::size_type i, size = nzvals.size();
	Teuchos::Array<mat_scalar_t> nzvals_tmp(size);

	if_then_else<is_same<GO,mat_go_t>::value,
	  same_go_helper<M,S,GO,GS,Op>,
	  diff_go_helper<M,S,GO,GS,Op> >::type::do_get(mat, nzvals_tmp, indices,
						       pointers, nnz, distribution,
						       ordering);

	for (i = 0; i < size; ++i){
	  nzvals[i] = Teuchos::as<S>(nzvals_tmp[i]);
	}
      }
    };
#endif	// DOXYGEN_SHOULD_SKIP_THIS

    /* This is our generic base class for the CRS and CCS helpers.
     *
     * S, GO, and GS are the desired types.  They are also the types of
     * the respective input parameters.
     *
     * The \c Op template parameter is a function-like class that
     * provides a static \c apply() function.
     */
    template<class Matrix, typename S, typename GO, typename GS, class Op>
    struct get_cxs_helper
    {
      static void do_get(const Teuchos::Ptr<Matrix> mat,
			 const ArrayView<S> nzvals,
			 const ArrayView<GO> indices,
			 const ArrayView<GS> pointers,
			 GS& nnz, EDistribution distribution,
			 EStorage_Ordering ordering=Arbitrary)
      {
	typedef typename Matrix::scalar_t mat_scalar;
	if_then_else<is_same<mat_scalar,S>::value,
	  same_scalar_helper<Matrix,S,GO,GS,Op>,
	  diff_scalar_helper<Matrix,S,GO,GS,Op> >::type::do_get(mat, nzvals,
								indices,
								pointers, nnz,
								distribution,
								ordering);
      }
    };

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    /*
     * These two function-like classes are meant to be used as the \c
     * Op template parameter for the \c get_cxs_helper template class.
     */
    template<class Matrix>
    struct get_ccs_func
    {
      static void apply(const Teuchos::Ptr<Matrix> mat,
			const ArrayView<typename Matrix::scalar_t> nzvals,
			const ArrayView<typename Matrix::global_ordinal_t> rowind,
			const ArrayView<typename Matrix::global_size_t> colptr,
			typename Matrix::global_size_t& nnz,
			EDistribution distribution,
			EStorage_Ordering ordering)
      {
	mat->getCcs(nzvals, rowind, colptr, nnz, distribution, ordering);
      }
    };

    template<class Matrix>
    struct get_crs_func
    {
      static void apply(const Teuchos::Ptr<Matrix> mat,
			const ArrayView<typename Matrix::scalar_t> nzvals,
			const ArrayView<typename Matrix::global_ordinal_t> colind,
			const ArrayView<typename Matrix::global_size_t> rowptr,
			typename Matrix::global_size_t& nnz,
			EDistribution distribution,
			EStorage_Ordering ordering)
      {
	mat->getCrs(nzvals, colind, rowptr, nnz, distribution, ordering);
      }
    };
#endif	// DOXYGEN_SHOULD_SKIP_THIS

    /*
     * These derivations of get_cxs_helper should be the entry points.
     * They inherit the \c do_get() function and provide the
     * appropriate concrete operator.
     *
     * They can be used like so::
     *
     *   typedef Tpetra::CrsMatrix<double,int,int> mat_t;
     *   mat_t m;
     *   < initialize m >
     *   Array<float> nzvals;
     *   Array<int> indices;
     *   Array<long int> pointers;
     *   long int nnz;
     *   get_ccs_helper<mat_t,float,int,long int>::do_get(
     *     m, nzvals, indices, pointers, nnz, Rooted, Arbitrary);
     */
    /**
     * A generic helper class for getting a CCS representation of a Matrix.
     *
     * The template types \c S , \c GO , and \c GS (scalar, global
     * ordinal, and global size type, respectively) are the types that
     * you would like to get from the Matrix, regardless of what types
     * are actually housed in the matrix.  Type conversions will be
     * performed when necessary.
     *
     * \subsection get_ccs_helper_example Example:
     *
     * Say for example that you have a matrix that has \c
     * complex<double> scalar values, \c int global ordinals, and
     * unsigned long size type, but your solver has a special complex
     * data type that it defines and has size type \c int.  As long as
     * the Teuchos::ValueTypeConversionTraits class is specialized for
     * conversion between the \c complex<double> and the solver's
     * complex double type, then you can use this helper to help with
     * this conversion.  We assume that we want the global matrix
     * representation at the root processor (\c Rooted), and the row
     * indices can be in an arbitrary order (\c Arbitrary):
     *
     * \code
     * // with unsigned long size_t
     * typedef Tpetra::CrsMatrix<std::complex<double>, int, int> mat_t;
     * mat_t my_mat;
     * // initialize mt_mat
     * Array<solver_complex> nzvals(nnz);
     * Array<int> rowind(nnz);
     * Array<int> rowptr(numcols+1);
     * get_ccs_helper<mat_t,solver_complex,int,int>::do_get(mat,nzvals,rowind,rowptr,nnz,Rooted,Arbitrary);
     * \endcode
     * 
     * \sa get_crs_helper
     */
    template<class Matrix, typename S, typename GO, typename GS>
    struct get_ccs_helper : get_cxs_helper<Matrix,S,GO,GS,get_ccs_func<Matrix> >
    {};

    template<class Matrix, typename S, typename GO, typename GS>
    struct get_crs_helper : get_cxs_helper<Matrix,S,GO,GS,get_crs_func<Matrix> >
    {};


  } // end namespace Util

} // end namespace Amesos

#endif	// #ifndef AMESOS2_UTIL_DECL_HPP
