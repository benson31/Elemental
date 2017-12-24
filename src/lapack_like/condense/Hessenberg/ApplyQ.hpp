/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_HESSENBERG_APPLYQ_HPP
#define EL_HESSENBERG_APPLYQ_HPP

#include "El/core/Matrix.hpp"
#include "El/lapack_like/reflect.hpp"
#include "El/Types/Enums.hpp"

namespace El
{
namespace hessenberg
{

template<typename F>
void ApplyQ
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  const Matrix<F>& A,
  const Matrix<F>& householderScalars,
        Matrix<F>& B)
{
    EL_DEBUG_CSE
    const bool normal = (orientation==Orientation::NORMAL);
    const bool onLeft = (side==LeftOrRight::LEFT);
    const ForwardOrBackward direction =
        (normal==onLeft
          ? ForwardOrBackward::BACKWARD : ForwardOrBackward::FORWARD);
    if(uplo == UpperOrLower::LOWER)
    {
        const Conjugation conjugation =
            (normal ? Conjugation::UNCONJUGATED : Conjugation::CONJUGATED);
        ApplyPackedReflectors
        (side, UpperOrLower::UPPER, VerticalOrHorizontal::HORIZONTAL,
          direction, conjugation, 1, A, householderScalars, B);
    }
    else
    {
        const Conjugation conjugation =
            (normal ? Conjugation::CONJUGATED : Conjugation::UNCONJUGATED);
        ApplyPackedReflectors
        (side, UpperOrLower::LOWER, VerticalOrHorizontal::VERTICAL,
          direction, conjugation, -1, A, householderScalars, B);
    }
}

template<typename F>
void ApplyQ
(LeftOrRight side, UpperOrLower uplo, Orientation orientation,
  const AbstractDistMatrix<F>& A,
  const AbstractDistMatrix<F>& householderScalars,
        AbstractDistMatrix<F>& B)
{
    EL_DEBUG_CSE
    const bool normal = (orientation==Orientation::NORMAL);
    const bool onLeft = (side==LeftOrRight::LEFT);
    const ForwardOrBackward direction =
        (normal==onLeft
          ? ForwardOrBackward::BACKWARD : ForwardOrBackward::FORWARD);
    if(uplo == UpperOrLower::LOWER)
    {
        const Conjugation conjugation =
            (normal ? Conjugation::UNCONJUGATED : Conjugation::CONJUGATED);
        ApplyPackedReflectors
        (side, UpperOrLower::UPPER, VerticalOrHorizontal::HORIZONTAL,
          direction, conjugation, 1, A, householderScalars, B);
    }
    else
    {
        const Conjugation conjugation =
            (normal ? Conjugation::CONJUGATED : Conjugation::UNCONJUGATED);
        ApplyPackedReflectors
        (side, UpperOrLower::LOWER, VerticalOrHorizontal::VERTICAL,
          direction, conjugation, -1, A, householderScalars, B);
    }
}

} // namespace hessenberg
} // namespace El

#endif // ifndef EL_HESSENBERG_APPLYQ_HPP
