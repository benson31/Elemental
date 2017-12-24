/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/

#include "El/core/DistMatrix/Abstract.hpp"
#include "El/core/DistPermutation.hpp"
#include "El/core/Matrix.hpp"
#include "El/core/Permutation.hpp"
#include "El/lapack_like/factor.hpp"
#include "El/lapack_like/funcs.hpp"
#include "El/lapack_like/props.hpp"
#include "El/Types/InertiaType.hpp"

namespace El
{

template<typename Field>
Base<Field> MaxCondition(const Matrix<Field>& A)
{
    EL_DEBUG_CSE
    typedef Base<Field> Real;
    Matrix<Field> B(A);
    const Real maxNorm = MaxNorm(B);
    try { Inverse(B); }
    catch(SingularMatrixException& e)
    { return limits::Infinity<Real>(); }
    const Real maxNormInv = MaxNorm(B);
    return maxNorm*maxNormInv;
}

template<typename Field>
Base<Field> MaxCondition(const AbstractDistMatrix<Field>& A)
{
    EL_DEBUG_CSE
    typedef Base<Field> Real;
    DistMatrix<Field> B(A);
    const Real maxNorm = MaxNorm(B);
    try { Inverse(B); }
    catch(SingularMatrixException& e)
    { return limits::Infinity<Real>(); }
    const Real maxNormInv = MaxNorm(B);
    return maxNorm*maxNormInv;
}

#define PROTO(Field) \
  template Base<Field> MaxCondition(const Matrix<Field>& A); \
  template Base<Field> MaxCondition(const AbstractDistMatrix<Field>& A);

#define EL_NO_INT_PROTO
#define EL_ENABLE_DOUBLEDOUBLE
#define EL_ENABLE_QUADDOUBLE
#define EL_ENABLE_QUAD
#define EL_ENABLE_BIGFLOAT
#include "El/macros/Instantiate.h"

} // namespace El
