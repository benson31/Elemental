/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#include <El/blas_like.hpp>

#define COLDIST Dist::STAR
#define ROWDIST Dist::MD

#include "./setup.hpp"

namespace El {

// Public section
// ##############

// Assignment and reconfiguration
// ==============================

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::MC,Dist::MR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::MC,Dist::STAR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::STAR,Dist::MR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::MD,Dist::STAR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const BDM& A )
{
    EL_DEBUG_CSE
    copy::Translate( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::MR,Dist::MC,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::MR,Dist::STAR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::STAR,Dist::MC,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::VC,Dist::STAR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::STAR,Dist::VC,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::VR,Dist::STAR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::STAR,Dist::VR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::STAR,Dist::STAR,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    copy::RowFilter( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const DistMatrix<T,Dist::CIRC,Dist::CIRC,DistWrap::BLOCK>& A )
{
    EL_DEBUG_CSE
    // TODO: More efficient implementation
    copy::GeneralPurpose( A, *this );
    return *this;
}

template<typename T>
BDM& BDM::operator=( const BlockMatrix<T>& A )
{
    EL_DEBUG_CSE
    #define GUARD(CDIST,RDIST,WRAP) \
      A.DistData().colDist == CDIST && A.DistData().rowDist == RDIST && \
      DistWrap::BLOCK == WRAP
    #define PAYLOAD(CDIST,RDIST,WRAP) \
      auto& ACast = \
        static_cast<const DistMatrix<T,CDIST,RDIST,DistWrap::BLOCK>&>(A); \
      *this = ACast;
    #include "El/macros/GuardAndPayload.h"
    return *this;
}

// Basic queries
// =============
template<typename T>
mpi::Comm BDM::DistComm() const EL_NO_EXCEPT
{ return this->grid_->MDComm(); }
template<typename T>
mpi::Comm BDM::CrossComm() const EL_NO_EXCEPT
{ return this->grid_->MDPerpComm(); }
template<typename T>
mpi::Comm BDM::RedundantComm() const EL_NO_EXCEPT
{ return ( this->Grid().InGrid() ? mpi::COMM_SELF : mpi::COMM_NULL ); }

template<typename T>
mpi::Comm BDM::ColComm() const EL_NO_EXCEPT
{ return ( this->Grid().InGrid() ? mpi::COMM_SELF : mpi::COMM_NULL ); }
template<typename T>
mpi::Comm BDM::RowComm() const EL_NO_EXCEPT
{ return this->grid_->MDComm(); }

template<typename T>
mpi::Comm BDM::PartialColComm() const EL_NO_EXCEPT
{ return this->ColComm(); }
template<typename T>
mpi::Comm BDM::PartialRowComm() const EL_NO_EXCEPT
{ return this->RowComm(); }

template<typename T>
mpi::Comm BDM::PartialUnionColComm() const EL_NO_EXCEPT
{ return ( this->Grid().InGrid() ? mpi::COMM_SELF : mpi::COMM_NULL ); }
template<typename T>
mpi::Comm BDM::PartialUnionRowComm() const EL_NO_EXCEPT
{ return ( this->Grid().InGrid() ? mpi::COMM_SELF : mpi::COMM_NULL ); }

template<typename T>
int BDM::ColStride() const EL_NO_EXCEPT { return 1; }
template<typename T>
int BDM::RowStride() const EL_NO_EXCEPT { return this->grid_->LCM(); }
template<typename T>
int BDM::DistSize() const EL_NO_EXCEPT { return this->grid_->LCM(); }
template<typename T>
int BDM::CrossSize() const EL_NO_EXCEPT { return this->grid_->GCD(); }
template<typename T>
int BDM::RedundantSize() const EL_NO_EXCEPT { return 1; }
template<typename T>
int BDM::PartialColStride() const EL_NO_EXCEPT { return this->ColStride(); }
template<typename T>
int BDM::PartialRowStride() const EL_NO_EXCEPT { return this->RowStride(); }
template<typename T>
int BDM::PartialUnionColStride() const EL_NO_EXCEPT { return 1; }
template<typename T>
int BDM::PartialUnionRowStride() const EL_NO_EXCEPT { return 1; }

template<typename T>
int BDM::DistRank() const EL_NO_EXCEPT
{ return this->grid_->MDRank(); }
template<typename T>
int BDM::CrossRank() const EL_NO_EXCEPT
{ return this->grid_->MDPerpRank(); }
template<typename T>
int BDM::RedundantRank() const EL_NO_EXCEPT
{ return ( this->Grid().InGrid() ? 0 : mpi::UNDEFINED ); }

template<typename T>
int BDM::ColRank() const EL_NO_EXCEPT
{ return ( this->Grid().InGrid() ? 0 : mpi::UNDEFINED ); }
template<typename T>
int BDM::RowRank() const EL_NO_EXCEPT
{ return this->grid_->MDRank(); }

template<typename T>
int BDM::PartialColRank() const EL_NO_EXCEPT
{ return this->ColRank(); }
template<typename T>
int BDM::PartialRowRank() const EL_NO_EXCEPT
{ return this->RowRank(); }

template<typename T>
int BDM::PartialUnionColRank() const EL_NO_EXCEPT
{ return ( this->Grid().InGrid() ? 0 : mpi::UNDEFINED ); }
template<typename T>
int BDM::PartialUnionRowRank() const EL_NO_EXCEPT
{ return ( this->Grid().InGrid() ? 0 : mpi::UNDEFINED ); }

// Instantiate {Int,Real,Complex<Real>} for each Real in {float,double}
// ####################################################################

#define SELF(T,U,V) \
  template DistMatrix<T,COLDIST,ROWDIST,DistWrap::BLOCK>::DistMatrix \
  ( const DistMatrix<T,U,V,DistWrap::BLOCK>& A );
#define OTHER(T,U,V) \
  template DistMatrix<T,COLDIST,ROWDIST,DistWrap::BLOCK>::DistMatrix \
  ( const DistMatrix<T,U,V>& A ); \
  template DistMatrix<T,COLDIST,ROWDIST,DistWrap::BLOCK>& \
           DistMatrix<T,COLDIST,ROWDIST,DistWrap::BLOCK>::operator= \
           ( const DistMatrix<T,U,V>& A )
#define BOTH(T,U,V) \
  SELF(T,U,V) \
  OTHER(T,U,V)
#define PROTO(T) \
  template class DistMatrix<T,COLDIST,ROWDIST,DistWrap::BLOCK>; \
  BOTH( T,Dist::CIRC,Dist::CIRC); \
  BOTH( T,Dist::MC,  Dist::MR  ); \
  BOTH( T,Dist::MC,  Dist::STAR); \
  BOTH( T,Dist::MD,  Dist::STAR); \
  BOTH( T,Dist::MR,  Dist::MC  ); \
  BOTH( T,Dist::MR,  Dist::STAR); \
  BOTH( T,Dist::STAR,Dist::MC  ); \
  OTHER(T,Dist::STAR,Dist::MD  ); \
  BOTH( T,Dist::STAR,Dist::MR  ); \
  BOTH( T,Dist::STAR,Dist::STAR); \
  BOTH( T,Dist::STAR,Dist::VC  ); \
  BOTH( T,Dist::STAR,Dist::VR  ); \
  BOTH( T,Dist::VC,  Dist::STAR); \
  BOTH( T,Dist::VR,  Dist::STAR);

#define EL_ENABLE_DOUBLEDOUBLE
#define EL_ENABLE_QUADDOUBLE
#define EL_ENABLE_QUAD
#define EL_ENABLE_BIGINT
#define EL_ENABLE_BIGFLOAT
#include <El/macros/Instantiate.h>

} // namespace El
