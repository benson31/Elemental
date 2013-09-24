/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef ELEM_BLAS_AXPY_HPP
#define ELEM_BLAS_AXPY_HPP

namespace elem {

template<typename T>
inline void
Axpy( T alpha, const Matrix<T>& X, Matrix<T>& Y )
{
#ifndef RELEASE
    CallStackEntry entry("Axpy");
#endif
    // If X and Y are vectors, we can allow one to be a column and the other
    // to be a row. Otherwise we force X and Y to be the same dimension.
    if( X.Height()==1 || X.Width()==1 )
    {
        const Int XLength = ( X.Width()==1 ? X.Height() : X.Width() );
        const Int XStride = ( X.Width()==1 ? 1          : X.LDim() );
        const Int YStride = ( Y.Width()==1 ? 1          : Y.LDim() );
#ifndef RELEASE
        const Int YLength = ( Y.Width()==1 ? Y.Height() : Y.Width() );
        if( XLength != YLength )
            LogicError("Nonconformal Axpy");
#endif
        blas::Axpy
        ( XLength, alpha, X.LockedBuffer(), XStride, Y.Buffer(), YStride );
    }
    else
    {
#ifndef RELEASE
        if( X.Height() != Y.Height() || X.Width() != Y.Width() )
            LogicError("Nonconformal Axpy");
#endif
        if( X.Width() <= X.Height() )
        {
            for( Int j=0; j<X.Width(); ++j )
            {
                blas::Axpy
                ( X.Height(), alpha, X.LockedBuffer(0,j), 1, Y.Buffer(0,j), 1 );
            }
        }
        else
        {
            for( Int i=0; i<X.Height(); ++i )
            {
                blas::Axpy
                ( X.Width(), alpha, X.LockedBuffer(i,0), X.LDim(),
                                    Y.Buffer(i,0),       Y.LDim() );
            }
        }
    }
}

#ifndef SWIG
template<typename T>
inline void
Axpy( BASE(T) alpha, const Matrix<T>& X, Matrix<T>& Y )
{ Axpy( T(alpha), X, Y ); }
#endif

template<typename T,Distribution U,Distribution V>
inline void
Axpy( T alpha, const DistMatrix<T,U,V>& X, DistMatrix<T,U,V>& Y )
{
#ifndef RELEASE
    CallStackEntry entry("Axpy");
    if( X.Grid() != Y.Grid() )
        LogicError
        ("X and Y must be distributed over the same grid");
#endif
    if( X.ColAlignment() == Y.ColAlignment() &&
        X.RowAlignment() == Y.RowAlignment() )
    {
        Axpy( alpha, X.LockedMatrix(), Y.Matrix() );
    }
    else
    {
        DistMatrix<T,U,V> XCopy( X.Grid() );
        XCopy.AlignWith( Y );
        XCopy = X;
        Axpy( alpha, XCopy.LockedMatrix(), Y.Matrix() );
    }
}

#ifndef SWIG
template<typename T,Distribution U,Distribution V>
inline void
Axpy
( BASE(T) alpha,
  const DistMatrix<T,U,V>& X, DistMatrix<T,U,V>& Y )
{ Axpy( T(alpha), X, Y ); }
#endif

} // namespace elem

#endif // ifndef ELEM_BLAS_AXPY_HPP
