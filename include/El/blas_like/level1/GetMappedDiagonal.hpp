/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_BLAS_GETMAPPEDDIAGONAL_HPP
#define EL_BLAS_GETMAPPEDDIAGONAL_HPP

namespace El {

template<typename T,typename S>
void GetMappedDiagonal
( const Matrix<T>& A,
        Matrix<S>& d,
        std::function<S(const T&)> func,
        Int offset )
{
    EL_DEBUG_CSE
    const Int diagLength = A.DiagonalLength(offset);
    d.Resize( diagLength, 1 );

    const Int iStart = Max(-offset,0);
    const Int jStart = Max( offset,0);
    S* dBuf = d.Buffer();
    const T* ABuf = A.LockedBuffer();
    const Int ldim = A.LDim();
    EL_PARALLEL_FOR
    for( Int k=0; k<diagLength; ++k )
    {
        const Int i = iStart + k;
        const Int j = jStart + k;
        dBuf[k] = func(ABuf[i+j*ldim]);
    }
}

template<typename T,typename S,Dist U,Dist V>
void GetMappedDiagonal
( const DistMatrix<T,U,V>& A,
        AbstractDistMatrix<S>& dPre,
        std::function<S(const T&)> func,
        Int offset )
{
    EL_DEBUG_CSE
    EL_DEBUG_ONLY(AssertSameGrids( A, dPre ))
    ElementalProxyCtrl ctrl;
    ctrl.colConstrain = true;
    ctrl.colAlign = A.DiagonalAlign(offset);
    ctrl.rootConstrain = true;
    ctrl.root = A.DiagonalRoot(offset);

    DistMatrixWriteProxy<S,S,DiagCol<U,V>(),DiagRow<U,V>()> dProx( dPre, ctrl );
    auto& d = dProx.Get();

    d.Resize( A.DiagonalLength(offset), 1 );
    if( d.Participating() )
    {
        const Int diagShift = d.ColShift();
        const Int iStart = diagShift + Max(-offset,0);
        const Int jStart = diagShift + Max( offset,0);

        const Int colStride = A.ColStride();
        const Int rowStride = A.RowStride();
        const Int iLocStart = (iStart-A.ColShift()) / colStride;
        const Int jLocStart = (jStart-A.RowShift()) / rowStride;
        const Int iLocStride = d.ColStride() / colStride;
        const Int jLocStride = d.ColStride() / rowStride;

        const Int localDiagLength = d.LocalHeight();
        S* dBuf = d.Buffer();
        const T* ABuf = A.LockedBuffer();
        const Int ldim = A.LDim();
        EL_PARALLEL_FOR
        for( Int k=0; k<localDiagLength; ++k )
        {
            const Int iLoc = iLocStart + k*iLocStride;
            const Int jLoc = jLocStart + k*jLocStride;
            dBuf[k] = func(ABuf[iLoc+jLoc*ldim]);
        }
    }
}

template<typename T,typename S,Dist U,Dist V>
void GetMappedDiagonal
( const DistMatrix<T,U,V,DistWrap::BLOCK>& A,
        AbstractDistMatrix<S>& d,
        std::function<S(const T&)> func,
        Int offset )
{
    EL_DEBUG_CSE
    EL_DEBUG_ONLY(AssertSameGrids( A, d ))

    // TODO(poulson): Make this more efficient
    const Int diagLength = A.DiagonalLength(offset);
    d.Resize( diagLength, 1 );
    Zero( d );
    if( d.Participating() && A.RedundantRank() == 0 )
    {
        const Int iStart = Max(-offset,0);
        const Int jStart = Max( offset,0);
        for( Int k=0; k<diagLength; ++k )
        {
            if( A.IsLocal(iStart+k,jStart+k) )
            {
                const Int iLoc = A.LocalRow(iStart+k);
                const Int jLoc = A.LocalCol(jStart+k);
                d.QueueUpdate(k,0,func(A.GetLocal(iLoc,jLoc)));
            }
        }
    }
    d.ProcessQueues();
}


#ifdef EL_INSTANTIATE_BLAS_LEVEL1
# define EL_EXTERN
#else
# define EL_EXTERN extern
#endif

#define PROTO(T) \
  EL_EXTERN template void GetMappedDiagonal \
  ( const Matrix<T>& A, \
          Matrix<T>& d, \
          std::function<T(const T&)> func, \
          Int offset );

#define EL_ENABLE_DOUBLEDOUBLE
#define EL_ENABLE_QUADDOUBLE
#define EL_ENABLE_QUAD
#define EL_ENABLE_BIGINT
#define EL_ENABLE_BIGFLOAT
#include "El/macros/Instantiate.h"

#undef EL_EXTERN

} // namespace El

#endif // ifndef EL_BLAS_GETMAPPEDDIAGONAL_HPP
