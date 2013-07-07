/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef LAPACK_HERMITIANTRIDIAG_PANELU_HPP
#define LAPACK_HERMITIANTRIDIAG_PANELU_HPP

#include "elemental/blas-like/level1/Zero.hpp"
#include "elemental/blas-like/level2/Gemv.hpp"
#include "elemental/blas-like/level2/Symv.hpp"
#include "elemental/lapack-like/Reflector/Col.hpp"

namespace elem {
namespace hermitian_tridiag {

template<typename F>
void PanelU
( DistMatrix<F>& A,
  DistMatrix<F>& W,
  DistMatrix<F,MD,STAR>& t,
  DistMatrix<F,MC,STAR>& APan_MC_STAR, 
  DistMatrix<F,MR,STAR>& APan_MR_STAR,
  DistMatrix<F,MC,STAR>& W_MC_STAR,
  DistMatrix<F,MR,STAR>& W_MR_STAR )
{
    const int panelSize = W.Width();
    const int topSize = W.Height()-panelSize;
#ifndef RELEASE
    CallStackEntry entry("hermitian_tridiag::PanelU");
    if( A.Grid() != W.Grid() || W.Grid() != t.Grid() )
        throw std::logic_error
        ("A, W, and t must be distributed over the same grid.");
    if( A.Height() != A.Width() )
        throw std::logic_error("A must be square.");
    if( A.Height() != W.Height() )
        throw std::logic_error( "A and W must be the same height.");
    if( W.Height() < panelSize )
        throw std::logic_error("W must be a column panel.");
    if( t.Height() != W.Width() || t.Width() != 1 )
        throw std::logic_error
        ("t must be a column vector of the same length as W's width.");
#endif
    typedef BASE(F) R;

    const Grid& g = A.Grid();
    const int r = g.Height();
    const int c = g.Width();
    const int p = g.Size();

    // Create a distributed matrix for storing the superdiagonal
    DistMatrix<R,MD,STAR> e(g);
    DistMatrix<F> expandedABR(g);
    View( expandedABR, A, topSize-1, topSize-1, panelSize+1, panelSize+1 );
    e.AlignWithDiagonal( expandedABR.DistData(), 1 );
    e.ResizeTo( panelSize, 1 );

    // Matrix views 
    DistMatrix<F> 
        ATL(g), ATR(g),  A00(g), a01(g),     A02(g),  ACol(g), a01T(g),
        ABL(g), ABR(g),  a10(g), alpha11(g), a12(g),           alpha01B(g),
                         A20(g), a21(g),     A22(g),  A02T(g), A00Pan(g);
    DistMatrix<F> 
        WTL(g), WTR(g),  W00(g), w01(g),     W02(g),  WCol(g),
        WBL(g), WBR(g),  w10(g), omega11(g), w12(g),
                         W20(g), w21(g),     W22(g),  W02T(g), w01Last(g);
    DistMatrix<R,MD,STAR> eT(g),  e0(g),
                          eB(g),  epsilon1(g),
                                  e2(g);
    DistMatrix<F,MD,STAR>
        tT(g),  t0(g),
        tB(g),  tau1(g),
                t2(g);

    // Temporary distributions
    std::vector<F> w01LastBuffer(A.Height()/r+1);
    DistMatrix<F,MC,STAR> a01_MC_STAR(g);
    DistMatrix<F,MC,STAR> a01T_MC_STAR(g);
    DistMatrix<F,MR,STAR> a01_MR_STAR(g);
    DistMatrix<F,MC,STAR> p01_MC_STAR(g);
    DistMatrix<F,MC,STAR> p01T_MC_STAR(g);
    DistMatrix<F,MR,STAR> q01_MR_STAR(g);
    DistMatrix<F,MR,STAR> x21_MR_STAR(g);
    DistMatrix<F,MR,STAR> y21_MR_STAR(g);
    DistMatrix<F,MC,STAR> a01Last_MC_STAR(g);
    DistMatrix<F,MR,STAR> a01Last_MR_STAR(g);
    DistMatrix<F,MC,STAR> w01Last_MC_STAR(g);
    DistMatrix<F,MR,STAR> w01Last_MR_STAR(g);

    PartitionUpOffsetDiagonal
    ( A.Width()-A.Height(),
      A, ATL, ATR,
         ABL, ABR, 0 );
    PartitionUpOffsetDiagonal
    ( W.Width()-W.Height(),
      W, WTL, WTR,
         WBL, WBR, 0 );
    PartitionUp
    ( e, eT,
         eB, 0 );
    PartitionUp
    ( t, tT,
         tB, 0 );
    bool firstIteration = true;
    F tau = 0;
    F w01LastBottomEntry = 0;
    while( WBR.Width() < panelSize )
    {
        RepartitionUpDiagonal
        ( ATL, /**/ ATR,  A00, a01,     /**/ A02,
               /**/       a10, alpha11, /**/ a12, 
         /*************/ /**********************/
          ABL, /**/ ABR,  A20, a21,     /**/ A22, 1 );
        
        RepartitionUpDiagonal
        ( WTL, /**/ WTR,  W00, w01,     /**/ W02,
               /**/       w10, omega11, /**/ w12,
         /*************/ /**********************/
          WBL, /**/ WBR,  W20, w21,     /**/ W22, 1 );

        RepartitionUp
        ( eT,  e0,
               epsilon1,
         /**/ /********/
          eB,  e2, 1 );

        RepartitionUp
        ( tT,  t0,
               tau1,
         /**/ /****/
          tB,  t2, 1 );

        View2x1
        ( ACol, a01,
                alpha11 );

        View2x1
        ( WCol, w01,
                omega11 );

        // View the portions of A02 and W0T outside of this panel's square
        View( A02T, A02, 0, 0, topSize, A02.Width() );
        View( W02T, W02, 0, 0, topSize, W02.Width() );

        // View the portion of A00 inside the current panel
        View( A00Pan, A00, 0, topSize, A00.Height(), A00.Width()-topSize );

        if( !firstIteration )
        {
            View
            ( a01Last_MC_STAR, APan_MC_STAR, 0, WTL.Width(), ACol.Height(), 1 );
            View
            ( a01Last_MR_STAR, APan_MR_STAR, 0, WTL.Width(), ACol.Height(), 1 );
            View( w01Last, W, 0, WTL.Width(), ACol.Height(), 1 );
        }
            
        PartitionUp
        ( a01, a01T,
               alpha01B, 1 );

        a01_MC_STAR.AlignWith( A00 );
        a01_MR_STAR.AlignWith( A00 );
        p01_MC_STAR.AlignWith( A00 );
        q01_MR_STAR.AlignWith( A00 );
        x21_MR_STAR.AlignWith( A02T );
        y21_MR_STAR.AlignWith( A02T );
        
        a01_MC_STAR.ResizeTo( a01.Height(), 1 );
        a01_MR_STAR.ResizeTo( a01.Height(), 1 );
        p01_MC_STAR.ResizeTo( a01.Height(), 1 );

        // View the portions of a01[MC,* ] and p01[MC,* ] above the current
        // panel's square
        View( a01T_MC_STAR, a01_MC_STAR, 0, 0, topSize, 1 );
        View( p01T_MC_STAR, p01_MC_STAR, 0, 0, topSize, 1 );
        //--------------------------------------------------------------------//
        const bool thisIsMyCol = ( g.Col() == alpha11.RowAlignment() );
        if( thisIsMyCol )
        {
            if( !firstIteration )
            {
                // Finish updating the current column with two axpy's
                const int AColLocalHeight = ACol.LocalHeight();
                F* AColBuffer = ACol.Buffer();
                const F* a01Last_MC_STAR_Buffer = a01Last_MC_STAR.Buffer();
                for( int i=0; i<AColLocalHeight; ++i )
                    AColBuffer[i] -=
                        w01LastBuffer[i] + 
                        a01Last_MC_STAR_Buffer[i]*Conj(w01LastBottomEntry);
            }
            // Compute the Householder reflector
            tau = reflector::Col( alpha01B, a01T );
            if( g.Row() == alpha01B.ColAlignment() )
                tau1.SetLocal(0,0,tau);
        }

        // Store the subdiagonal value and turn a01 into a proper scaled 
        // reflector by explicitly placing the implicit one in its first entry.
        alpha01B.GetRealPartOfDiagonal( epsilon1 );
        alpha01B.Set( 0, 0, F(1) );

        // If this is the first iteration, have each member of the owning 
        // process column broadcast tau and a01 within its process row. 
        // Otherwise, also add w01 into the broadcast.
        if( firstIteration )
        {
            const int a01LocalHeight = a01.LocalHeight();
            std::vector<F> rowBroadcastBuffer(a01LocalHeight+1);
            if( thisIsMyCol )
            {
                // Pack the broadcast buffer with a01 and tau
                MemCopy
                ( &rowBroadcastBuffer[0], a01.Buffer(), a01LocalHeight );
                rowBroadcastBuffer[a01LocalHeight] = tau;
            }
            // Broadcast a01 and tau across the process row
            mpi::Broadcast
            ( &rowBroadcastBuffer[0], 
              a01LocalHeight+1, a01.RowAlignment(), g.RowComm() );
            // Store a01[MC,* ] into its DistMatrix class and also store a copy
            // for the next iteration
            MemCopy
            ( a01_MC_STAR.Buffer(), &rowBroadcastBuffer[0], a01LocalHeight );
            // Store a01[MC,* ] into APan[MC,* ]
            MemCopy
            ( APan_MC_STAR.Buffer(0,W00.Width()), 
              &rowBroadcastBuffer[0], a01LocalHeight );
            // Store tau
            tau = rowBroadcastBuffer[a01LocalHeight];
            
            a01_MR_STAR = a01_MC_STAR;
            // Store a01[MR,* ]
            MemCopy
            ( APan_MR_STAR.Buffer(0,W00.Width()),
              a01_MR_STAR.Buffer(),
              a01_MR_STAR.LocalHeight() );
        }
        else
        {
            const int a01LocalHeight = a01.LocalHeight();
            const int w01LastLocalHeight = ACol.LocalHeight();
            std::vector<F> 
                rowBroadcastBuffer(a01LocalHeight+w01LastLocalHeight+1);
            if( thisIsMyCol ) 
            {
                // Pack the broadcast buffer with a01, w01Last, and tau
                MemCopy( &rowBroadcastBuffer[0], a01.Buffer(), a01LocalHeight );
                MemCopy
                ( &rowBroadcastBuffer[a01LocalHeight], 
                  &w01LastBuffer[0], w01LastLocalHeight );
                rowBroadcastBuffer[a01LocalHeight+w01LastLocalHeight] = tau;
            }
            // Broadcast a01, w01Last, and tau across the process row
            mpi::Broadcast
            ( &rowBroadcastBuffer[0], 
              a01LocalHeight+w01LastLocalHeight+1, 
              a01.RowAlignment(), g.RowComm() );
            // Store a01[MC,* ] into its DistMatrix class 
            MemCopy
            ( a01_MC_STAR.Buffer(), &rowBroadcastBuffer[0], a01LocalHeight );
            // Store a01[MC,* ] into APan[MC,* ]
            MemCopy
            ( APan_MC_STAR.Buffer(0,W00.Width()), 
              &rowBroadcastBuffer[0], a01LocalHeight );
            // Store w01Last[MC,* ] into its DistMatrix class
            w01Last_MC_STAR.AlignWith( A00 );
            w01Last_MC_STAR.ResizeTo( a01.Height()+1, 1 );
            MemCopy
            ( w01Last_MC_STAR.Buffer(), 
              &rowBroadcastBuffer[a01LocalHeight], w01LastLocalHeight );
            // Store the bottom part of w01Last[MC,* ] into WB[MC,* ] and, 
            // if necessary, w01.
            MemCopy
            ( W_MC_STAR.Buffer(0,W00.Width()+1),
              &rowBroadcastBuffer[a01LocalHeight], w01LastLocalHeight );
            if( g.Col() == w01Last.RowAlignment() )
            {
                MemCopy
                ( w01Last.Buffer(),
                  &rowBroadcastBuffer[a01LocalHeight], w01LastLocalHeight );
            }
            // Store tau
            tau = rowBroadcastBuffer[a01LocalHeight+w01LastLocalHeight];

            // Form a01[MR,* ] and w01Last[MR,* ] by combining the 
            // communications needed for taking a vector from 
            // [MC,* ] -> [MR,* ]: 
            //   local copy to [VC,* ], 
            //   Send/Recv to [VR,* ], 
            //   AllGather to [MR,* ]
            // We can combine the two by treating a01 as [a01; 0] 

            const int colAlignSource = A00.ColAlignment();
            const int colAlignDest = A00.RowAlignment();
            const int colShiftSource = A00.ColShift();
            const int colShiftDest = A00.RowShift();

            const int height = a01.Height()+1;
            const int portionSize = 
                std::max(2*MaxLength(height,p),mpi::MIN_COLL_MSG);

            const int colShiftVRDest = Shift(g.VRRank(),colAlignDest,p);
            const int colShiftVCSource = Shift(g.VCRank(),colAlignSource,p);
            const int sendRankRM = 
                (g.VRRank()+(p+colShiftVCSource-colShiftVRDest))%p;
            const int recvRankCM = 
                (g.VCRank()+(p+colShiftVRDest-colShiftVCSource))%p;
            const int recvRankRM = 
                (recvRankCM/r)+c*(recvRankCM%r);

            std::vector<F> transposeBuffer( (r+1)*portionSize );
            F* sendBuf = &transposeBuffer[0];
            F* recvBuf = &transposeBuffer[r*portionSize];

            // (w01Last[VC,* ] <- w01Last[MC,* ]) and
            // ([a01; 0][VC,* ] <- [a01; 0][MC,* ])
            {
                // Pack the necessary portion of w01Last[MC,* ]
                const int shift = Shift(g.VCRank(),colAlignSource,p);
                const int offset = (shift-colShiftSource)/r;
                const int w01VCLocalHeight = Length(height,shift,p);
                const F* w01Buffer = w01Last_MC_STAR.Buffer(offset,0);
                for( int i=0; i<w01VCLocalHeight; ++i )
                    sendBuf[i] = w01Buffer[i*c];
                
                // Pack the necessary portion of a01[MC,* ]
                const int a01VCLocalHeight = Length(height-1,shift,p);
                const F* a01Buffer = a01_MC_STAR.Buffer(offset,0);
                for( int i=0; i<a01VCLocalHeight; ++i )
                    sendBuf[w01VCLocalHeight+i] = a01Buffer[i*c];
            }

            // [VR,* ] <- [VC,* ]
            mpi::SendRecv
            ( sendBuf, portionSize, sendRankRM, 0,
              recvBuf, portionSize, recvRankRM, mpi::ANY_TAG, g.VRComm() );

            // [MR,* ] <- [VR,* ]
            mpi::AllGather
            ( recvBuf, portionSize,
              sendBuf, portionSize, g.ColComm() );

            // Unpack
            w01Last_MR_STAR.AlignWith( A00 );
            w01Last_MR_STAR.ResizeTo( a01.Height()+1, 1 );
            for( int k=0; k<r; ++k )
            {
                // Unpack into w01Last[MR,* ]
                const F* w01Data = &sendBuf[k*portionSize];
                const int shift = Shift(g.Col()+c*k,colAlignDest,p);
                const int offset = (shift-colShiftDest) / c;
                const int w01VCLocalHeight = Length(height,shift,p);
                F* w01Buffer = w01Last_MR_STAR.Buffer(offset,0);
                for( int i=0; i<w01VCLocalHeight; ++i )
                    w01Buffer[i*r] = w01Data[i];

                // Unpack into a01[MR,* ]
                const F* a01Data = &sendBuf[k*portionSize+w01VCLocalHeight];
                const int a01VCLocalHeight = Length(height-1,shift,p);
                F* a01Buffer = a01_MR_STAR.Buffer(offset,0);
                for( int i=0; i<a01VCLocalHeight; ++i )
                    a01Buffer[i*r] = a01Data[i];
            }
            // Store w01Last[MR,* ]
            MemCopy
            ( W_MR_STAR.Buffer(0,W00.Width()+1),
              w01Last_MR_STAR.Buffer(),
              w01Last_MR_STAR.LocalHeight() );
            // Store a01[MR,* ]
            MemCopy
            ( APan_MR_STAR.Buffer(0,W00.Width()),
              a01_MR_STAR.Buffer(),
              a01_MR_STAR.LocalHeight() );

            // Update the portion of A00 that is in our current panel with 
            // w01Last and a01Last using two gers. We do not need their bottom
            // entries. We trash the lower triangle of our panel of A since we 
            // are only doing slightly more work and we can replace it
            // afterwards.
            DistMatrix<F,MC,STAR> a01Last_MC_STAR_Top(g),
                                  w01Last_MC_STAR_Top(g);
            DistMatrix<F,MR,STAR> a01Last_MR_STAR_TopPan(g),
                                  w01Last_MR_STAR_TopPan(g);
            View( a01Last_MC_STAR_Top, a01Last_MC_STAR, 0, 0, a01.Height(), 1 );
            View( w01Last_MC_STAR_Top, w01Last_MC_STAR, 0, 0, a01.Height(), 1 );
            View
            ( a01Last_MR_STAR_TopPan, 
              a01Last_MR_STAR, topSize, 0, a01.Height()-topSize, 1 );
            View
            ( w01Last_MR_STAR_TopPan,
              w01Last_MR_STAR, topSize, 0, a01.Height()-topSize, 1 );
            const F* a01_MC_STAR_Buffer = a01Last_MC_STAR_Top.Buffer();
            const F* w01_MC_STAR_Buffer = w01Last_MC_STAR_Top.Buffer();
            const F* a01_MR_STAR_Buffer = a01Last_MR_STAR_TopPan.Buffer();
            const F* w01_MR_STAR_Buffer = w01Last_MR_STAR_TopPan.Buffer();
            F* A00PanBuffer = A00Pan.Buffer();
            const int localHeight = A00Pan.LocalHeight();
            const int localWidth = A00Pan.LocalWidth();
            const int lDim = A00Pan.LDim();
            for( int jLocal=0; jLocal<localWidth; ++jLocal )
                for( int iLocal=0; iLocal<localHeight; ++iLocal )
                    A00PanBuffer[iLocal+jLocal*lDim] -=
                        w01_MC_STAR_Buffer[iLocal]*
                        Conj(a01_MR_STAR_Buffer[jLocal]) +
                        a01_MC_STAR_Buffer[iLocal]*
                        Conj(w01_MR_STAR_Buffer[jLocal]);
        }

        // Form the local portions of (A00 a01) into p01[MC,* ] and q01[MR,* ]:
        //   p01[MC,* ] := triu(A00)[MC,MR] a01[MR,* ]
        //   q01[MR,* ] := triu(A00,+1)'[MR,MC] a01[MC,* ]
        Zero( p01_MC_STAR );
        Zeros( q01_MR_STAR, a01.Height(), 1 );
        internal::LocalSymvColAccumulateU
        ( F(1), A00, a01_MC_STAR, a01_MR_STAR, p01_MC_STAR, q01_MR_STAR, true );

        Zeros( x21_MR_STAR, A02.Width(), 1 );
        Zeros( y21_MR_STAR, A02.Width(), 1 );
        LocalGemv( ADJOINT, F(1), W02T, a01T_MC_STAR, F(0), x21_MR_STAR );
        LocalGemv( ADJOINT, F(1), A02T, a01T_MC_STAR, F(0), y21_MR_STAR );

        // Combine the AllReduce column summations of x21[MR,* ], y21[MR,* ],
        // and q01[MR,* ]
        {
            const int x21LocalHeight = x21_MR_STAR.LocalHeight();
            const int y21LocalHeight = y21_MR_STAR.LocalHeight();
            const int q01LocalHeight = q01_MR_STAR.LocalHeight();
            const int reduceSize = x21LocalHeight+y21LocalHeight+q01LocalHeight;
            std::vector<F> colSumSendBuffer(reduceSize),
                           colSumRecvBuffer(reduceSize);
            MemCopy
            ( &colSumSendBuffer[0], x21_MR_STAR.Buffer(), x21LocalHeight );
            MemCopy
            ( &colSumSendBuffer[x21LocalHeight],
              y21_MR_STAR.Buffer(), y21LocalHeight );
            MemCopy
            ( &colSumSendBuffer[x21LocalHeight+y21LocalHeight],
              q01_MR_STAR.Buffer(), q01LocalHeight );
            mpi::AllReduce
            ( &colSumSendBuffer[0], 
              &colSumRecvBuffer[0],
              reduceSize, mpi::SUM, g.ColComm() );
            MemCopy
            ( x21_MR_STAR.Buffer(), &colSumRecvBuffer[0], x21LocalHeight );
            MemCopy
            ( y21_MR_STAR.Buffer(), 
              &colSumRecvBuffer[x21LocalHeight], y21LocalHeight );
            MemCopy
            ( q01_MR_STAR.Buffer(), 
              &colSumRecvBuffer[x21LocalHeight+y21LocalHeight], 
              q01LocalHeight );
        }

        LocalGemv( NORMAL, F(-1), A02T, x21_MR_STAR, F(1), p01T_MC_STAR );
        LocalGemv( NORMAL, F(-1), W02T, y21_MR_STAR, F(1), p01T_MC_STAR );

        if( W00.Width() > 0 )
        {
            // This is not the last iteration of the panel factorization, 
            // combine the Reduce to one of p01[MC,* ] with the redistribution 
            // of q01[MR,* ] -> q01[MC,MR] to the next process column.
            const int localHeight = p01_MC_STAR.LocalHeight();
            std::vector<F> reduceToOneSendBuffer(2*localHeight),
                           reduceToOneRecvBuffer(2*localHeight);

            // Pack p01[MC,* ]
            MemCopy
            ( &reduceToOneSendBuffer[0], p01_MC_STAR.Buffer(), localHeight );

            // Fill in contributions to q01[MC,MR] from q01[MR,* ]
            const bool contributing = 
                ( q01_MR_STAR.ColShift() % g.GCD() ==
                  p01_MC_STAR.ColShift() % g.GCD() );
            if( contributing )
            {
                if( r == c )
                {
                    MemCopy
                    ( &reduceToOneSendBuffer[localHeight],
                      q01_MR_STAR.Buffer(), localHeight );
                }
                else
                {
                    // Zero the entire buffer first
                    MemZero( &reduceToOneSendBuffer[localHeight], localHeight );
                    // Fill in the entries that we contribute to.
                    // We seek to find the minimum s in N such that
                    //   s*c = a0-b0 (mod r)
                    // where a0 is the column shift of MC, b0 is the row shift
                    // of MR, and s is our first local entry of MR that will 
                    // contribute to MC. I cannot think of an O(1) method, so
                    // I will instead use the worst-case O(lcm(c,r)/c) method.
                    const int sourcePeriod = g.LCM() / c;
                    const int targetPeriod = g.LCM() / r;
                    const int a0 = p01_MC_STAR.ColShift();
                    const int b0 = q01_MR_STAR.ColShift();

                    int sourceStart = 0;
                    const int f = (r+a0-b0) % r;
                    for( int s=0; s<sourcePeriod; ++s )
                    {
                        if( (s*c) % r == f )
                        {
                            sourceStart = s;
                            break;
                        }
                    }

                    const int globalShift = b0+sourceStart*c;
                    const int targetStart = (globalShift-a0)/r;
                    const int localLength =
                        Length(localHeight,targetStart,targetPeriod);
                    const F* q01_MR_STAR_Buffer = q01_MR_STAR.Buffer();
                    const int offset = localHeight + targetStart;
                    for( int i=0; i<localLength; ++i )                        
                        reduceToOneSendBuffer[offset+i*targetPeriod] = 
                            q01_MR_STAR_Buffer[sourceStart+i*sourcePeriod];
                }
            }
            else
                MemZero( &reduceToOneSendBuffer[localHeight], localHeight );

            const int nextProcessRow = (alpha11.ColAlignment()+r-1) % r;
            const int nextProcessCol = (alpha11.RowAlignment()+c-1) % c;
            mpi::Reduce
            ( &reduceToOneSendBuffer[0], 
              &reduceToOneRecvBuffer[0],
              2*localHeight, mpi::SUM, nextProcessCol, g.RowComm() );
            if( g.Col() == nextProcessCol )
            {
                // Combine the second half into the first half        
                for( int i=0; i<localHeight; ++i )
                    reduceToOneRecvBuffer[i] +=
                        reduceToOneRecvBuffer[i+localHeight];

                // Finish computing w01. During its computation, ensure that 
                // every process has a copy of the last element of the w01.
                // We know a priori that the last element of a01 is one.
                const F* a01_MC_STAR_Buffer = a01_MC_STAR.Buffer();
                F myDotProduct = blas::Dot
                    ( localHeight, &reduceToOneRecvBuffer[0], 1, 
                                   &a01_MC_STAR_Buffer[0],    1 );
                F sendBuffer[2], recvBuffer[2];
                sendBuffer[0] = myDotProduct;
                sendBuffer[1] = ( g.Row()==nextProcessRow ? 
                                  reduceToOneRecvBuffer[localHeight-1] : 0 );
                mpi::AllReduce
                ( sendBuffer, recvBuffer, 2, mpi::SUM, g.ColComm() );
                F dotProduct = recvBuffer[0];

                // Set up for the next iteration by filling in the values for:
                // - w01LastBuffer
                // - w01LastBottomEntry
                F scale = dotProduct*Conj(tau) / F(2);
                for( int i=0; i<localHeight; ++i )
                    w01LastBuffer[i] = tau*
                        ( reduceToOneRecvBuffer[i]-
                          scale*a01_MC_STAR_Buffer[i] );
                w01LastBottomEntry = tau*( recvBuffer[1]-scale );
            }
        }
        else
        {
            // This is the last iteration, our last task is to finish forming
            // w01[MC,* ] and w01[MR,* ] so that we may place them into W[MC,* ]
            // and W[MR,* ]
            const int localHeight = p01_MC_STAR.LocalHeight();
            std::vector<F> allReduceSendBuffer(2*localHeight),
                           allReduceRecvBuffer(2*localHeight);

            // Pack p01[MC,* ]
            MemCopy
            ( &allReduceSendBuffer[0], p01_MC_STAR.Buffer(), localHeight );

            // Fill in contributions to q01[MC,* ] from q01[MR,* ]
            const bool contributing = 
                ( q01_MR_STAR.ColShift() % g.GCD() ==
                  p01_MC_STAR.ColShift() % g.GCD() );
            if( contributing )
            {
                if( r == c )
                {
                    MemCopy
                    ( &allReduceSendBuffer[localHeight],
                      q01_MR_STAR.Buffer(), localHeight );
                }
                else
                {
                    // Zero the entire buffer first
                    MemZero( &allReduceSendBuffer[localHeight], localHeight );
                    // Fill in the entries that we contribute to.
                    // We seek to find the minimum s in N such that
                    //   s*c = a0-b0 (mod r)
                    // where a0 is the column shift of MC, b0 is the row shift
                    // of MR, and s is our first local entry of MR that will 
                    // contribute to MC. I cannot think of an O(1) method, so
                    // I will instead use the worst-case O(lcm(c,r)/c) method.
                    const int sourcePeriod = g.LCM() / c;
                    const int targetPeriod = g.LCM() / r;
                    const int a0 = p01_MC_STAR.ColShift();
                    const int b0 = q01_MR_STAR.ColShift();

                    int sourceStart = 0;
                    const int f = (r+a0-b0) % r;
                    for( int s=0; s<sourcePeriod; ++s )
                    {
                        if( (s*c) % r == f )
                        {
                            sourceStart = s;
                            break;
                        }
                    }

                    const int globalShift = b0+sourceStart*c;
                    const int targetStart = (globalShift-a0)/r;
                    const int localLength = 
                        Length(localHeight,targetStart,targetPeriod);
                    const F* q01_MR_STAR_Buffer = q01_MR_STAR.Buffer();
                    const int offset = localHeight + targetStart;
                    for( int i=0; i<localLength; ++i )
                        allReduceSendBuffer[offset+i*targetPeriod] = 
                            q01_MR_STAR_Buffer[sourceStart+i*sourcePeriod];
                }
            }
            else
                MemZero( &allReduceSendBuffer[localHeight], localHeight );

            mpi::AllReduce
            ( &allReduceSendBuffer[0], 
              &allReduceRecvBuffer[0],
              2*localHeight, mpi::SUM, g.RowComm() );

            // Combine the second half into the first half        
            for( int i=0; i<localHeight; ++i )
                allReduceRecvBuffer[i] += allReduceRecvBuffer[i+localHeight];
 
            // Finish computing w01. During its computation, ensure that 
            // every process has a copy of the last element of the w01.
            // We know a priori that the last element of a01 is one.
            const F* a01_MC_STAR_Buffer = a01_MC_STAR.Buffer();
            F myDotProduct = blas::Dot
                ( localHeight, &allReduceRecvBuffer[0], 1, 
                               a01_MC_STAR_Buffer,      1 );
            F dotProduct;
            mpi::AllReduce
            ( &myDotProduct, &dotProduct, 1, mpi::SUM, g.ColComm() );

            // Grab views into W[MC,* ] and W[MR,* ]
            DistMatrix<F,MC,STAR> w01_MC_STAR(g);
            DistMatrix<F,MR,STAR> w01_MR_STAR(g);
            View( w01_MC_STAR, W_MC_STAR, 0, W00.Width(), w01.Height(), 1 );
            View( w01_MR_STAR, W_MR_STAR, 0, W00.Width(), w01.Height(), 1 );

            // Store w01[MC,* ]
            F scale = dotProduct*Conj(tau) / F(2);
            F* w01_MC_STAR_Buffer = w01_MC_STAR.Buffer();
            for( int i=0; i<localHeight; ++i )
                w01_MC_STAR_Buffer[i] = 
                    tau*( allReduceRecvBuffer[i]-scale*a01_MC_STAR_Buffer[i] );

            // Form w01[MR,* ]
            w01_MR_STAR = w01_MC_STAR;
        }
        //--------------------------------------------------------------------//

        SlidePartitionUp
        ( tT,  t0,
         /**/ /****/
               tau1,
          tB,  t2 );

        SlidePartitionUp
        ( eT,  e0,
         /**/ /********/
               epsilon1,
          eB,  e2 );

        SlidePartitionUpDiagonal
        ( WTL, /**/ WTR,  W00, /**/ w01,     W02,
         /*************/ /**********************/
               /**/       w10, /**/ omega11, w12,
          WBL, /**/ WBR,  W20, /**/ w21,     W22 );

        SlidePartitionUpDiagonal
        ( ATL, /**/ ATR,  A00, /**/ a01,     A02,
         /*************/ /**********************/
               /**/       a10, /**/ alpha11, a12,
          ABL, /**/ ABR,  A20, /**/ a21,     A22 );

        firstIteration = false;
    }

    expandedABR.SetRealPartOfDiagonal( e, 1 );
}

} // namespace hermitian_tridiag
} // namespace elem

#endif // ifndef LAPACK_HERMITIANTRIDIAG_PANELU_HPP
