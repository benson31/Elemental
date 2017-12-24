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
#include "El/lapack_like/props.hpp"
#include "El/Types/Enums.hpp"

namespace El
{

template<typename Field>
Base<Field> Condition( const Matrix<Field>& A, NormType type )
{
    EL_DEBUG_CSE
    Base<Field> norm = 0;
    switch( type )
    {
    case NormType::FROBENIUS_NORM:
        norm = FrobeniusCondition( A );
        break;
    case NormType::INFINITY_NORM:
        norm = InfinityCondition( A );
        break;
    case NormType::MAX_NORM:
        norm = MaxCondition( A );
        break;
    case NormType::ONE_NORM:
        norm = OneCondition( A );
        break;
    case NormType::TWO_NORM:
        norm = TwoCondition( A );
        break;
    default:
        LogicError("Invalid norm type for condition number");
    }
    return norm;
}

template<typename Field>
Base<Field> Condition( const AbstractDistMatrix<Field>& A, NormType type )
{
    EL_DEBUG_CSE
    Base<Field> norm = 0;
    switch( type )
    {
    case NormType::FROBENIUS_NORM:
        norm = FrobeniusCondition( A );
        break;
    case NormType::INFINITY_NORM:
        norm = InfinityCondition( A );
        break;
    case NormType::MAX_NORM:
        norm = MaxCondition( A );
        break;
    case NormType::ONE_NORM:
        norm = OneCondition( A );
        break;
    case NormType::TWO_NORM:
        norm = TwoCondition( A );
        break;
    default:
        LogicError("Invalid norm type for condition number");
    }
    return norm;
}

#define PROTO(Field) \
  template Base<Field> Condition( const Matrix<Field>& A, NormType type ); \
  template Base<Field> \
  Condition( const AbstractDistMatrix<Field>& A, NormType type );

#define EL_NO_INT_PROTO
#define EL_ENABLE_DOUBLEDOUBLE
#define EL_ENABLE_QUADDOUBLE
#define EL_ENABLE_QUAD
#define EL_ENABLE_BIGFLOAT
#include "El/macros/Instantiate.h"

} // namespace El
