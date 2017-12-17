/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_BLAS3_HPP
#define EL_BLAS3_HPP

namespace El {

template<typename T> void SetLocalTrrkBlocksize(Int blocksize);
template<typename T> Int LocalTrrkBlocksize();

template<typename T> void SetLocalTrr2kBlocksize(Int blocksize);
template<typename T> Int LocalTrr2kBlocksize();

// Gemm
// ====
namespace GemmAlgorithmNS {
enum GemmAlgorithm {
  GEMM_DEFAULT,
  GEMM_SUMMA_A,
  GEMM_SUMMA_B,
  GEMM_SUMMA_C,
  GEMM_SUMMA_DOT,
  GEMM_CANNON
};
}
using namespace GemmAlgorithmNS;

template<typename T>
void Gemm
(Orientation orientA, Orientation orientB,
  T alpha, const Matrix<T>& A, const Matrix<T>& B, T beta, Matrix<T>& C);

template<typename T>
void Gemm
(Orientation orientA, Orientation orientB,
  T alpha, const Matrix<T>& A, const Matrix<T>& B, Matrix<T>& C);

template<typename T>
void Gemm
(Orientation orientA, Orientation orientB,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
  T beta,        AbstractDistMatrix<T>& C, GemmAlgorithm alg=GEMM_DEFAULT);

template<typename T>
void Gemm
(Orientation orientA, Orientation orientB,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
                 AbstractDistMatrix<T>& C, GemmAlgorithm alg=GEMM_DEFAULT);

template<typename T>
void LocalGemm
(Orientation orientA, Orientation orientB,
  T alpha, const AbstractDistMatrix<T>& A,
           const AbstractDistMatrix<T>& B,
  T beta,        AbstractDistMatrix<T>& C);
template<typename T>
void LocalGemm
(Orientation orientA, Orientation orientB,
  T alpha, const AbstractDistMatrix<T>& A,
           const AbstractDistMatrix<T>& B,
                 AbstractDistMatrix<T>& C);

// Hemm
// ====
template<typename T>
void Hemm
(LeftOrRight side, UpperOrLower uplo,
  T alpha, const Matrix<T>& A, const Matrix<T>& B, T beta, Matrix<T>& C);

template<typename T>
void Hemm
(LeftOrRight side, UpperOrLower uplo,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
  T beta,        AbstractDistMatrix<T>& C);

// Herk
// ====
template<typename T>
void Herk
(UpperOrLower uplo, Orientation orientation,
  Base<T> alpha, const Matrix<T>& A, Base<T> beta, Matrix<T>& C);
template<typename T>
void Herk
(UpperOrLower uplo, Orientation orientation,
  Base<T> alpha, const Matrix<T>& A, Matrix<T>& C);

template<typename T>
void Herk
(UpperOrLower uplo, Orientation orientation,
  Base<T> alpha, const AbstractDistMatrix<T>& A,
  Base<T> beta,        AbstractDistMatrix<T>& C);
template<typename T>
void Herk
(UpperOrLower uplo, Orientation orientation,
  Base<T> alpha, const AbstractDistMatrix<T>& A, AbstractDistMatrix<T>& C);

// Her2k
// =====
template<typename T>
void Her2k
(UpperOrLower uplo, Orientation orientation,
  T alpha,       const Matrix<T>& A, const Matrix<T>& B,
  Base<T> beta,        Matrix<T>& C);

template<typename T>
void Her2k
(UpperOrLower uplo, Orientation orientation,
  T alpha, const Matrix<T>& A, const Matrix<T>& B, Matrix<T>& C);

template<typename T>
void Her2k
(UpperOrLower uplo, Orientation orientation,
  T alpha,      const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
  Base<T> beta,       AbstractDistMatrix<T>& C);

template<typename T>
void Her2k
(UpperOrLower uplo, Orientation orientation,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
                 AbstractDistMatrix<T>& C);

// MultiShiftQuasiTrsm
// ===================
template<typename F>
void MultiShiftQuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, const Matrix<F>& A, const Matrix<F>& shifts, Matrix<F>& B);
template<typename Real>
void MultiShiftQuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  Complex<Real> alpha,
  const Matrix<Real>& A,
  const Matrix<Complex<Real>>& shifts,
        Matrix<Real>& BReal, Matrix<Real>& BImag);
template<typename F>
void MultiShiftQuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, const AbstractDistMatrix<F>& A, const AbstractDistMatrix<F>& shifts,
  AbstractDistMatrix<F>& B);
template<typename Real>
void MultiShiftQuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  Complex<Real> alpha,
  const AbstractDistMatrix<Real>& A,
  const AbstractDistMatrix<Complex<Real>>& shifts,
        AbstractDistMatrix<Real>& BReal, AbstractDistMatrix<Real>& BImag);

template<typename F>
void LocalMultiShiftQuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, const DistMatrix<F,Dist::STAR,Dist::STAR>& A,
           const AbstractDistMatrix<F>& shifts,
                 AbstractDistMatrix<F>& X);
template<typename Real>
void LocalMultiShiftQuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  Complex<Real> alpha,
  const DistMatrix<Real,Dist::STAR,Dist::STAR>& A,
  const AbstractDistMatrix<Complex<Real>>& shifts,
        AbstractDistMatrix<Real>& XReal,
        AbstractDistMatrix<Real>& XImag);

// MultiShiftTrsm
// ==============
template<typename F>
void MultiShiftTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, Matrix<F>& U, const Matrix<F>& shifts, Matrix<F>& X);
template<typename F>
void MultiShiftTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, const AbstractDistMatrix<F>& U, const AbstractDistMatrix<F>& shifts,
  AbstractDistMatrix<F>& X);

// SafeMultiShiftTrsm
// ==================
template<typename F>
void SafeMultiShiftTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, Matrix<F>& A, const Matrix<F>& shifts,
  Matrix<F>& B, Matrix<F>& scales);
template<typename F>
void SafeMultiShiftTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, const AbstractDistMatrix<F>& A, const AbstractDistMatrix<F>& shifts,
  AbstractDistMatrix<F>& B, AbstractDistMatrix<F>& scales);

// QuasiTrsm
// =========
template<typename F>
void QuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, const Matrix<F>& A, Matrix<F>& B,
  bool checkIfSingular=false);
template<typename F>
void QuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, const AbstractDistMatrix<F>& A, AbstractDistMatrix<F>& B,
  bool checkIfSingular=false);

template<typename F>
void LocalQuasiTrsm
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  F alpha, const DistMatrix<F,Dist::STAR,Dist::STAR>& A, AbstractDistMatrix<F>& X,
  bool checkIfSingular=false);

// Symm
// ====
template<typename T>
void Symm
(LeftOrRight side, UpperOrLower uplo,
  T alpha, const Matrix<T>& A, const Matrix<T>& B, T beta, Matrix<T>& C,
  bool conjugate=false);
template<typename T>
void Symm
(LeftOrRight side, UpperOrLower uplo,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
  T beta,        AbstractDistMatrix<T>& C, bool conjugate=false);

namespace symm {

template<typename T>
void LocalAccumulateLL
(Orientation orientation, T alpha,
  const DistMatrix<T>& A,
  const DistMatrix<T,Dist::MC,  Dist::STAR>& B_Dist::MC_STAR,
  const DistMatrix<T,Dist::STAR,Dist::MR  >& BTrans_STAR_MR,
        DistMatrix<T,Dist::MC,  Dist::STAR>& Z_MC_STAR,
        DistMatrix<T,Dist::MR,  Dist::STAR>& Z_MR_STAR);

template<typename T>
void LocalAccumulateLU
(Orientation orientation, T alpha,
  const DistMatrix<T>& A,
  const DistMatrix<T,Dist::MC,  Dist::STAR>& B_MC_STAR,
  const DistMatrix<T,Dist::STAR,Dist::MR  >& BTrans_STAR_MR,
        DistMatrix<T,Dist::MC,  Dist::STAR>& Z_MC_STAR,
        DistMatrix<T,Dist::MR,  Dist::STAR>& Z_MR_STAR);

template<typename T>
void LocalAccumulateRL
(Orientation orientation, T alpha,
  const DistMatrix<T>& A,
  const DistMatrix<T,Dist::STAR,Dist::MC  >& B_STAR_MC,
  const DistMatrix<T,Dist::MR,  Dist::STAR>& BTrans_MR_STAR,
        DistMatrix<T,Dist::MC,  Dist::STAR>& ZTrans_MC_STAR,
        DistMatrix<T,Dist::MR,  Dist::STAR>& ZTrans_MR_STAR);

template<typename T>
void LocalAccumulateRU
(Orientation orientation, T alpha,
  const DistMatrix<T,Dist::MC,  Dist::MR  >& A,
  const DistMatrix<T,STAR,Dist::MC  >& B_STAR_MC,
  const DistMatrix<T,Dist::MR,  Dist::STAR>& BTrans_MR_STAR,
        DistMatrix<T,Dist::MC,  Dist::STAR>& ZTrans_MC_STAR,
        DistMatrix<T,Dist::MR,  Dist::STAR>& ZTrans_MR_STAR);

} // namespace symm

// Syrk
// ====
template<typename T>
void Syrk
(UpperOrLower uplo, Orientation orientation,
  T alpha, const Matrix<T>& A, T beta, Matrix<T>& C,
  bool conjugate=false);
template<typename T>
void Syrk
(UpperOrLower uplo, Orientation orientation,
  T alpha, const Matrix<T>& A, Matrix<T>& C,
  bool conjugate=false);

template<typename T>
void Syrk
(UpperOrLower uplo, Orientation orientation,
  T alpha, const AbstractDistMatrix<T>& A,
  T beta,        AbstractDistMatrix<T>& C, bool conjugate=false);
template<typename T>
void Syrk
(UpperOrLower uplo, Orientation orientation,
  T alpha, const AbstractDistMatrix<T>& A, AbstractDistMatrix<T>& C,
  bool conjugate=false);

// Syr2k
// =====
template<typename T>
void Syr2k
(UpperOrLower uplo, Orientation orientation,
  T alpha, const Matrix<T>& A, const Matrix<T>& B, T beta, Matrix<T>& C,
  bool conjugate=false);

template<typename T>
void Syr2k
(UpperOrLower uplo, Orientation orientation,
  T alpha, const Matrix<T>& A, const Matrix<T>& B, Matrix<T>& C,
  bool conjugate=false);

template<typename T>
void Syr2k
(UpperOrLower uplo, Orientation orientation,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
  T beta,        AbstractDistMatrix<T>& C,
  bool conjugate=false);

template<typename T>
void Syr2k
(UpperOrLower uplo, Orientation orientation,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
                 AbstractDistMatrix<T>& C,
  bool conjugate=false);

// Trdtrmm
// =======
template<typename F>
void Trdtrmm(UpperOrLower uplo, Matrix<F>& A, bool conjugate=false);
template<typename F>
void Trdtrmm
(UpperOrLower uplo, Matrix<F>& A, const Matrix<F>& dOff,
  bool conjugate=false);

template<typename F>
void Trdtrmm
(UpperOrLower uplo, AbstractDistMatrix<F>& A, bool conjugate=false);
template<typename F>
void Trdtrmm
(UpperOrLower uplo,
  AbstractDistMatrix<F>& A, const AbstractDistMatrix<F>& dOff,
  bool conjugate=false);

template<typename F>
void Trdtrmm
(UpperOrLower uplo, DistMatrix<F,Dist::STAR,Dist::STAR>& A, bool conjugate=false);
template<typename F>
void Trdtrmm
(UpperOrLower uplo,
  DistMatrix<F,Dist::STAR,Dist::STAR>& A, const DistMatrix<F,Dist::STAR,Dist::STAR>& dOff,
  bool conjugate=false);

// Trmm
// ====
template<typename T>
void Trmm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  T alpha, const Matrix<T>& A, Matrix<T>& B);
template<typename T>
void Trmm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  T alpha, const AbstractDistMatrix<T>& A, AbstractDistMatrix<T>& X);

template<typename T>
void LocalTrmm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  T alpha, const DistMatrix<T,Dist::STAR,Dist::STAR>& A, AbstractDistMatrix<T>& B);

// Trsm
// ====
namespace TrsmAlgorithmNS {
enum TrsmAlgorithm {
  TRSM_DEFAULT,
  TRSM_LARGE,
  TRSM_MEDIUM,
  TRSM_SMALL
};
}
using namespace TrsmAlgorithmNS;

template<typename F>
void Trsm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  F alpha, const Matrix<F>& A, Matrix<F>& B,
  bool checkIfSingular=false);
template<typename F>
void Trsm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  F alpha,
  const AbstractDistMatrix<F>& A,
        AbstractDistMatrix<F>& B,
  bool checkIfSingular=false, TrsmAlgorithm alg=TRSM_DEFAULT);

template<typename F>
void LocalTrsm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  F alpha,
  const DistMatrix<F,Dist::STAR,Dist::STAR>& A,
        AbstractDistMatrix<F>& X,
  bool checkIfSingular=false);

// Trstrm
// ======
template<typename F>
void Trstrm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  F alpha, const Matrix<F>& A, Matrix<F>& X,
  bool checkIfSingular=true);
template<typename F>
void Trstrm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  F alpha, const AbstractDistMatrix<F>& A, AbstractDistMatrix<F>& X,
  bool checkIfSingular=true);
template<typename F>
void Trstrm
(LeftOrRight side, UpperOrLower uplo,
  Orientation orientation, UnitOrNonUnit diag,
  F alpha, const DistMatrix<F,Dist::STAR,Dist::STAR>& A, DistMatrix<F,Dist::STAR,Dist::STAR>& X,
  bool checkIfSingular=true);

// Trtrmm
// ======
template<typename T>
void Trtrmm(UpperOrLower uplo, Matrix<T>& A, bool conjugate=false);
template<typename T>
void Trtrmm
(UpperOrLower uplo, AbstractDistMatrix<T>& A, bool conjugate=false);
template<typename T>
void Trtrmm
(UpperOrLower uplo, DistMatrix<T,Dist::STAR,Dist::STAR>& A, bool conjugate=false);

// TwoSidedTrmm
// ============
template<typename T>
void TwoSidedTrmm
(UpperOrLower uplo, UnitOrNonUnit diag,
  Matrix<T>& A, const Matrix<T>& B);
template<typename T>
void TwoSidedTrmm
(UpperOrLower uplo, UnitOrNonUnit diag,
  AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B);
template<typename T>
void TwoSidedTrmm
(UpperOrLower uplo, UnitOrNonUnit diag,
  DistMatrix<T,Dist::MC,Dist::MR,DistWrap::BLOCK>& A, const DistMatrix<T,Dist::MC,Dist::MR,DistWrap::BLOCK>& B);
template<typename T>
void LocalTwoSidedTrmm
(UpperOrLower uplo, UnitOrNonUnit diag,
  DistMatrix<T,Dist::STAR,Dist::STAR>& A, const DistMatrix<T,Dist::STAR,Dist::STAR>& B);

// TwoSidedTrsm
// ============
template<typename F>
void TwoSidedTrsm
(UpperOrLower uplo, UnitOrNonUnit diag,
  Matrix<F>& A, const Matrix<F>& B);
template<typename F>
void TwoSidedTrsm
(UpperOrLower uplo, UnitOrNonUnit diag,
  AbstractDistMatrix<F>& A, const AbstractDistMatrix<F>& B);
template<typename F>
void TwoSidedTrsm
(UpperOrLower uplo, UnitOrNonUnit diag,
  DistMatrix<F,Dist::MC,Dist::MR,DistWrap::BLOCK>& A, const DistMatrix<F,Dist::MC,Dist::MR,DistWrap::BLOCK>& B);
template<typename F>
void TwoSidedTrsm
(UpperOrLower uplo, UnitOrNonUnit diag,
  DistMatrix<F,Dist::STAR,Dist::STAR>& A, const DistMatrix<F,Dist::STAR,Dist::STAR>& B);

// Trrk
// ====
template<typename T>
void Trrk
(UpperOrLower uplo,
  Orientation orientA, Orientation orientB,
  T alpha, const Matrix<T>& A, const Matrix<T>& B,
  T beta,        Matrix<T>& C);
template<typename T>
void Trrk
(UpperOrLower uplo,
  Orientation orientA, Orientation orientB,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
  T beta,        AbstractDistMatrix<T>& C);
template<typename T>
void LocalTrrk
(UpperOrLower uplo,
  T alpha, const DistMatrix<T,Dist::MC,  Dist::STAR>& A,
           const DistMatrix<T,Dist::STAR,Dist::MR  >& B,
  T beta,        DistMatrix<T,Dist::MC,  Dist::MR  >& C);
template<typename T>
void LocalTrrk
(UpperOrLower uplo,
  Orientation orientB,
  T alpha, const DistMatrix<T,Dist::MC,Dist::STAR>& A,
           const DistMatrix<T,Dist::MR,Dist::STAR>& B,
  T beta,        DistMatrix<T>& C);
template<typename T>
void LocalTrrk
(UpperOrLower uplo,
  Orientation orientA,
  T alpha, const DistMatrix<T,Dist::STAR,Dist::MC>& A,
           const DistMatrix<T,Dist::STAR,Dist::MR>& B,
  T beta,        DistMatrix<T,Dist::MC,  Dist::MR>& C);
template<typename T>
void LocalTrrk
(UpperOrLower uplo,
  Orientation orientA, Orientation orientB,
  T alpha, const DistMatrix<T,Dist::STAR,Dist::MC  >& A,
           const DistMatrix<T,Dist::MR,  Dist::STAR>& B,
  T beta,        DistMatrix<T,Dist::MC,  Dist::MR  >& C);

// Trr2k
// =====
/*
template<typename T>
void Trr2k
(UpperOrLower uplo,
  Orientation orientA, Orientation orientB,
  Orientation orientC, Orientation orientD,
  T alpha, const Matrix<T>& A, const Matrix<T>& B,
  T beta,  const Matrix<T>& C, const Matrix<T>& D,
  Tgamma,        Matrix<T>& E);
*/
template<typename T>
void Trr2k
(UpperOrLower uplo,
  Orientation orientA, Orientation orientB,
  Orientation orientC, Orientation orientD,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
  T beta,  const AbstractDistMatrix<T>& C, const AbstractDistMatrix<T>& D,
  T gamma,       AbstractDistMatrix<T>& E);

// The distributions of the oriented matrices must match
template<typename T>
void LocalTrr2k
(UpperOrLower uplo,
  Orientation orientA, Orientation orientB,
  Orientation orientC, Orientation orientD,
  T alpha, const AbstractDistMatrix<T>& A, const AbstractDistMatrix<T>& B,
  T beta,  const AbstractDistMatrix<T>& C, const AbstractDistMatrix<T>& D,
  T gamma,       AbstractDistMatrix<T>& E);

// Hermitian from EVD
// ==================
// A := Z diag(w) Z^H, where w is real
template<typename F>
void HermitianFromEVD
(UpperOrLower uplo,
        Matrix<F>& A,
  const Matrix<Base<F>>& w,
  const Matrix<F>& Z);
template<typename F>
void HermitianFromEVD
(UpperOrLower uplo,
        AbstractDistMatrix<F>& A,
  const AbstractDistMatrix<Base<F>>& w,
  const AbstractDistMatrix<F>& Z);

// Normal from EVD
// ===============
// A := Z diag(w) Z^H, where w is complex
template<typename Real>
void NormalFromEVD
(Matrix<Complex<Real>>& A,
 Matrix<Complex<Real>> const& w,
 Matrix<Complex<Real>> const& Z);
template<typename Real>
void NormalFromEVD
(AbstractDistMatrix<Complex<Real>>& A,
 AbstractDistMatrix<Complex<Real>> const& w,
 AbstractDistMatrix<Complex<Real>> const& Z);

} // namespace El

#endif // ifndef EL_BLAS3_HPP
