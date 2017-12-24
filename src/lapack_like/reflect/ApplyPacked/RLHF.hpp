/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_APPLYPACKEDREFLECTORS_RLHF_HPP
#define EL_APPLYPACKEDREFLECTORS_RLHF_HPP

namespace El {
namespace apply_packed_reflectors {

//
// Since applying Householder transforms from vectors stored top-to-bottom
// implies that we will be forming a generalization of
//
//  (I - tau_0 v_0^T conj(v_0)) (I - tau_1 v_1^T conj(v_1)) =
//  I - [ v_0^T, v_1^T ] [ tau_0, -tau_0 tau_1 conj(v_0) v_1^T ] [ conj(v_0) ]
//                       [ 0,      tau_1                       ] [ conj(v_1) ],
//
// which has an upper-triangular center matrix, say S, we will form S as
// the inverse of a matrix T, which can easily be formed as
//
//   triu(T,1) = triu( conj(V V^H) ),
//   diag(T) = 1/householderScalars or 1/conj(householderScalars),
//
// where V is the matrix of Householder vectors and householderScalars is the
// vector of Householder reflection coefficients.
//

template<typename F>
void
RLHFUnblocked
( Conjugation conjugation,
  Int offset,
  const Matrix<F>& H,
  const Matrix<F>& householderScalars,
        Matrix<F>& A )
{
    EL_DEBUG_CSE
    EL_DEBUG_ONLY(
      if( A.Width() != H.Width() )
          LogicError("A and H must have the same width");
    )
    const Int diagLength = H.DiagonalLength(offset);
    EL_DEBUG_ONLY(
      if( householderScalars.Height() != diagLength )
          LogicError
          ("householderScalars must be the same length as H's offset diag");
    )
    Matrix<F> hPanCopy, z;

    const Int iOff = ( offset>=0 ? 0      : -offset );
    const Int jOff = ( offset>=0 ? offset : 0       );

    for( Int k=0; k<diagLength; ++k )
    {
        const Int ki = k+iOff;
        const Int kj = k+jOff;

        auto hPan = H( IR(ki), IR(0,kj+1) );
        auto ALeft = A( ALL, IR(0,kj+1) );
        const F tau = householderScalars(k);
        const F gamma = ( conjugation == Conjugation::CONJUGATED ? Conj(tau) : tau );

        // Convert to an explicit (scaled) Householder vector
        hPanCopy = hPan;
        hPanCopy(0,kj) = 1;

        // z := ALeft hPan^T
        Gemv( Orientation::NORMAL, F(1), ALeft, hPanCopy, z );
        // ALeft := ALeft (I - gamma hPan^T conj(hPan))
        Ger( -gamma, z, hPanCopy, ALeft );
    }
}

template<typename F>
void
RLHFBlocked
( Conjugation conjugation,
  Int offset,
  const Matrix<F>& H,
  const Matrix<F>& householderScalars,
        Matrix<F>& A )
{
    EL_DEBUG_CSE
    EL_DEBUG_ONLY(
      if( A.Width() != H.Width() )
          LogicError("A and H must have the same width");
    )
    const Int diagLength = H.DiagonalLength(offset);
    EL_DEBUG_ONLY(
      if( householderScalars.Height() != diagLength )
          LogicError
          ("householderScalars must be the same length as H's offset diag");
    )
    Matrix<F> HPanConj, SInv, Z;

    const Int iOff = ( offset>=0 ? 0      : -offset );
    const Int jOff = ( offset>=0 ? offset : 0       );

    const Int bsize = Blocksize();
    for( Int k=0; k<diagLength; k+=bsize )
    {
        const Int nb = Min(bsize,diagLength-k);
        const Int ki = k+iOff;
        const Int kj = k+jOff;

        auto HPan  = H( IR(ki,ki+nb), IR(0,kj+nb) );
        auto ALeft = A( ALL,          IR(0,kj+nb) );
        auto householderScalars1 = householderScalars( IR(k,k+nb), ALL );

        // Convert to an explicit matrix of (scaled) Householder vectors
        Conjugate( HPan, HPanConj );
        MakeTrapezoidal( UpperOrLower::LOWER, HPanConj, HPanConj.Width()-HPanConj.Height() );
        FillDiagonal( HPanConj, F(1), HPanConj.Width()-HPanConj.Height() );

        // Form the small triangular matrix needed for the UT transform
        Herk( UpperOrLower::UPPER, Orientation::NORMAL, Base<F>(1), HPanConj, SInv );
        FixDiagonal( conjugation, householderScalars1, SInv );

        // Z := ALeft HPan^T
        Gemm( Orientation::NORMAL, Orientation::ADJOINT, F(1), ALeft, HPanConj, Z );
        // Z := ALeft HPan^T inv(SInv)
        Trsm( LeftOrRight::RIGHT, UpperOrLower::UPPER, Orientation::NORMAL, UnitOrNonUnit::NON_UNIT, F(1), SInv, Z );
        // ALeft := ALeft (I - HPan^T inv(SInv) conj(HPan))
        Gemm( Orientation::NORMAL, Orientation::NORMAL, F(-1), Z, HPanConj, F(1), ALeft );
    }
}

template<typename F>
void
RLHF
( Conjugation conjugation,
  Int offset,
  const Matrix<F>& H,
  const Matrix<F>& householderScalars,
        Matrix<F>& A )
{
    EL_DEBUG_CSE
    const Int numLHS = A.Height();
    const Int blocksize = Blocksize();
    if( numLHS < blocksize )
    {
        RLHFUnblocked( conjugation, offset, H, householderScalars, A );
    }
    else
    {
        RLHFBlocked( conjugation, offset, H, householderScalars, A );
    }
}

template<typename F>
void
RLHFUnblocked
( Conjugation conjugation,
  Int offset,
  const AbstractDistMatrix<F>& H,
  const AbstractDistMatrix<F>& householderScalarsPre,
        AbstractDistMatrix<F>& APre )
{
    EL_DEBUG_CSE
    EL_DEBUG_ONLY(
      if( APre.Width() != H.Width() )
          LogicError("A and H must have the same width");
      AssertSameGrids( H, householderScalarsPre, APre );
    )

    // We gather the entire set of Householder scalars at the start rather than
    // continually paying the latency cost of the broadcasts in a 'Get' call
    DistMatrixReadProxy<F,F,Dist::STAR,Dist::STAR>
      householderScalarsProx( householderScalarsPre );
    auto& householderScalars = householderScalarsProx.GetLocked();

    DistMatrixReadWriteProxy<F,F,Dist::MC,Dist::MR> AProx( APre );
    auto& A = AProx.Get();

    const Int diagLength = H.DiagonalLength(offset);
    EL_DEBUG_ONLY(
      if( householderScalars.Height() != diagLength )
          LogicError
          ("householderScalars must be the same length as H's offset diag");
    )
    const Grid& g = H.Grid();
    auto hPan = std::unique_ptr<AbstractDistMatrix<F>>( H.Construct(g,H.Root()) );
    DistMatrix<F,Dist::STAR,Dist::MR> hPan_STAR_MR(g);
    DistMatrix<F,Dist::MC,Dist::STAR> z_MC_STAR(g);

    const Int iOff = ( offset>=0 ? 0      : -offset );
    const Int jOff = ( offset>=0 ? offset : 0       );

    for( Int k=0; k<diagLength; ++k )
    {
        const Int ki = k+iOff;
        const Int kj = k+jOff;

        auto ALeft = A( ALL, IR(0,kj+1) );
        const F tau = householderScalars.GetLocal( k, 0 );
        const F gamma = ( conjugation == Conjugation::CONJUGATED ? Conj(tau) : tau );

        // Convert to an explicit (scaled) Householder vector
        LockedView( *hPan, H, IR(ki), IR(0,kj+1) );
        hPan_STAR_MR.AlignWith( ALeft );
        Copy( *hPan, hPan_STAR_MR );
        hPan_STAR_MR.Set( 0, kj, F(1) );

        // z := ALeft hPan^T
        z_MC_STAR.AlignWith( ALeft );
        Zeros( z_MC_STAR, ALeft.Height(), 1 );
        LocalGemv( Orientation::NORMAL, F(1), ALeft, hPan_STAR_MR, F(0), z_MC_STAR );
        El::AllReduce( z_MC_STAR, ALeft.RowComm() );

        // ALeft := ALeft (I - gamma hPan^T conj(hPan))
        LocalGer( -gamma, z_MC_STAR, hPan_STAR_MR, ALeft );
    }
}

template<typename F>
void
RLHFBlocked
( Conjugation conjugation,
  Int offset,
  const AbstractDistMatrix<F>& H,
  const AbstractDistMatrix<F>& householderScalarsPre,
        AbstractDistMatrix<F>& APre )
{
    EL_DEBUG_CSE
    EL_DEBUG_ONLY(
      if( APre.Width() != H.Width() )
          LogicError("A and H must have the same width");
      AssertSameGrids( H, householderScalarsPre, APre );
    )

    DistMatrixReadProxy<F,F,Dist::MC,Dist::STAR>
      householderScalarsProx( householderScalarsPre );
    auto& householderScalars = householderScalarsProx.GetLocked();

    DistMatrixReadWriteProxy<F,F,Dist::MC,Dist::MR> AProx( APre );
    auto& A = AProx.Get();

    const Int diagLength = H.DiagonalLength(offset);
    EL_DEBUG_ONLY(
      if( householderScalars.Height() != diagLength )
          LogicError
          ("householderScalars must be the same length as H's offset diag");
    )
    const Grid& g = H.Grid();
    auto HPan = std::unique_ptr<AbstractDistMatrix<F>>( H.Construct(g,H.Root()) );
    DistMatrix<F> HPanConj(g);
    DistMatrix<F,Dist::STAR,Dist::VR  > HPan_STAR_VR(g);
    DistMatrix<F,Dist::STAR,Dist::MR  > HPan_STAR_MR(g);
    DistMatrix<F,Dist::STAR,Dist::STAR> householderScalars1_STAR_STAR(g);
    DistMatrix<F,Dist::STAR,Dist::STAR> SInv_STAR_STAR(g);
    DistMatrix<F,Dist::STAR,Dist::MC  > ZAdj_STAR_MC(g);
    DistMatrix<F,Dist::STAR,Dist::VC  > ZAdj_STAR_VC(g);

    const Int iOff = ( offset>=0 ? 0      : -offset );
    const Int jOff = ( offset>=0 ? offset : 0       );

    const Int bsize = Blocksize();
    for( Int k=0; k<diagLength; k+=bsize )
    {
        const Int nb = Min(bsize,diagLength-k);
        const Int ki = k+iOff;
        const Int kj = k+jOff;

        auto ALeft = A( ALL, IR(0,kj+nb) );
        auto householderScalars1 = householderScalars( IR(k,k+nb), ALL );

        // Convert to an explicit matrix of (scaled) Householder vectors
        LockedView( *HPan, H, IR(ki,ki+nb), IR(0,kj+nb) );
        Conjugate( *HPan, HPanConj );
        MakeTrapezoidal( UpperOrLower::LOWER, HPanConj, HPanConj.Width()-HPanConj.Height() );
        FillDiagonal( HPanConj, F(1), HPanConj.Width()-HPanConj.Height() );

        // Form the small triangular matrix needed for the UT transform
        HPan_STAR_VR = HPanConj;
        Zeros( SInv_STAR_STAR, nb, nb );
        Herk
        ( UpperOrLower::UPPER, Orientation::NORMAL,
          Base<F>(1), HPan_STAR_VR.LockedMatrix(),
          Base<F>(0), SInv_STAR_STAR.Matrix() );
        El::AllReduce( SInv_STAR_STAR, HPan_STAR_VR.RowComm() );
        householderScalars1_STAR_STAR = householderScalars1;
        FixDiagonal
        ( conjugation, householderScalars1_STAR_STAR, SInv_STAR_STAR );

        // Z := ALeft HPan^T
        HPan_STAR_MR.AlignWith( ALeft );
        HPan_STAR_MR = HPan_STAR_VR;
        ZAdj_STAR_MC.AlignWith( ALeft );
        LocalGemm( Orientation::NORMAL, Orientation::ADJOINT, F(1), HPan_STAR_MR, ALeft, ZAdj_STAR_MC );
        ZAdj_STAR_VC.AlignWith( ALeft );
        Contract( ZAdj_STAR_MC, ZAdj_STAR_VC );

        // Z := ALeft HPan^T inv(SInv)
        LocalTrsm
        ( LeftOrRight::LEFT, UpperOrLower::UPPER, Orientation::ADJOINT, UnitOrNonUnit::NON_UNIT,
          F(1), SInv_STAR_STAR, ZAdj_STAR_VC );

        // ALeft := ALeft (I - HPan^T inv(SInv) conj(HPan))
        ZAdj_STAR_MC = ZAdj_STAR_VC;
        LocalGemm
        ( Orientation::ADJOINT, Orientation::NORMAL, F(-1), ZAdj_STAR_MC, HPan_STAR_MR, F(1), ALeft );
    }
}

template<typename F>
void
RLHF
( Conjugation conjugation,
  Int offset,
  const AbstractDistMatrix<F>& H,
  const AbstractDistMatrix<F>& householderScalars,
        AbstractDistMatrix<F>& A )
{
    EL_DEBUG_CSE
    const Int numLHS = A.Height();
    const Int blocksize = Blocksize();
    if( numLHS < blocksize )
    {
        RLHFUnblocked( conjugation, offset, H, householderScalars, A );
    }
    else
    {
        RLHFBlocked( conjugation, offset, H, householderScalars, A );
    }
}

} // namespace apply_packed_reflectors
} // namespace El

#endif // ifndef EL_APPLYPACKEDREFLECTORS_RLHF_HPP
