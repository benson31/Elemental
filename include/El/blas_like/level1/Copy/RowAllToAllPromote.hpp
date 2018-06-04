/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_BLAS_COPY_ROWALLTOALLPROMOTE_HPP
#define EL_BLAS_COPY_ROWALLTOALLPROMOTE_HPP

namespace El {
namespace copy {

// FIXME (trb 03/06/18) -- Need to do the GPU impl
template<typename T,Dist U,Dist V,Device D>
void RowAllToAllPromote
( const DistMatrix<T,U,V,ELEMENT,D>& A,
  DistMatrix<T,PartialUnionCol<U,V>(),Partial<V>(),ELEMENT,D>& B )
{
    EL_DEBUG_CSE
    AssertSameGrids( A, B );

    const Int height = A.Height();
    const Int width = A.Width();
    B.AlignRowsAndResize
    ( Mod(A.RowAlign(),B.RowStride()), height, width, false, false );
    if( !B.Participating() )
        return;

    const Int rowAlign = A.RowAlign();

    const Int rowStride = A.RowStride();
    const Int rowStridePart = A.PartialRowStride();
    const Int rowStrideUnion = A.PartialUnionRowStride();
    const Int rowRankPart = A.PartialRowRank();
    const Int rowDiff = B.RowAlign() - Mod(rowAlign,rowStridePart);

    const Int maxLocalWidth = MaxLength(width,rowStride);
    const Int maxLocalHeight = MaxLength(height,rowStrideUnion);
    const Int portionSize = mpi::Pad( maxLocalHeight*maxLocalWidth );

    if( rowDiff == 0 )
    {
        if( A.PartialUnionRowStride() == 1 )
        {
            Copy( A.LockedMatrix(), B.Matrix() );
        }
        else
        {
            OutputFromRoot(A.Grid().Comm(), "RowAllToAllPromote: AllToAll path");

            simple_buffer<T,D> buffer(2*rowStrideUnion*portionSize);
            T* firstBuf  = buffer.data();
            T* secondBuf = buffer.data() + rowStrideUnion*portionSize;

            // Pack

            // DEBUG
            Timer clock;
            double time_elapsed;

            clock.Start();
            util::ColStridedPack<T,D>
                ( height, A.LocalWidth(),
                  B.ColAlign(), rowStrideUnion,
                  A.LockedBuffer(), A.LDim(),
                  firstBuf,         portionSize );

            time_elapsed = clock.Stop();
            MPI_Reduce(A.Grid().Comm().Rank() == 0 ? MPI_IN_PLACE : &time_elapsed,
                       &time_elapsed, 1, mpi::TypeMap<T>(),
                       MPI_MAX, 0, A.Grid().Comm().comm);
            OutputFromRoot(A.Grid().Comm(),
                           "           ColStridedPack: ", time_elapsed, "s");

            // Simultaneously Gather in rows and Scatter in columns
            clock.Reset();
            clock.Start();
            mpi::AllToAll
            ( firstBuf,  portionSize,
              secondBuf, portionSize, A.PartialUnionRowComm() );

            time_elapsed = clock.Stop();
            MPI_Reduce(A.Grid().Comm().Rank() == 0 ? MPI_IN_PLACE : &time_elapsed,
                       &time_elapsed, 1, mpi::TypeMap<T>(),
                       MPI_MAX, 0, A.Grid().Comm().comm);
            OutputFromRoot(A.Grid().Comm(),
                           "                 AllToAll: ", time_elapsed, "s");
            // Unpack
            clock.Reset();
            clock.Start();
            util::PartialRowStridedUnpack<T,D>
            ( B.LocalHeight(), width,
              rowAlign, rowStride,
              rowStrideUnion, rowStridePart, rowRankPart,
              B.RowShift(),
              secondBuf, portionSize,
              B.Buffer(), B.LDim() );

            time_elapsed = clock.Stop();
            MPI_Reduce(A.Grid().Comm().Rank() == 0 ? MPI_IN_PLACE : &time_elapsed,
                       &time_elapsed, 1, mpi::TypeMap<T>(),
                       MPI_MAX, 0, A.Grid().Comm().comm);
            OutputFromRoot(A.Grid().Comm(),
                           "  PartialRowStridedUnpack: ", time_elapsed, "s");
        }
    }
    else
    {
#ifdef EL_UNALIGNED_WARNINGS
        if( A.Grid().Rank() == 0 )
            cerr << "Unaligned RowAllToAllPromote" << endl;
#endif
        const Int sendRowRankPart = Mod( rowRankPart+rowDiff, rowStridePart );
        const Int recvRowRankPart = Mod( rowRankPart-rowDiff, rowStridePart );

        simple_buffer<T,D> buffer(2*rowStrideUnion*portionSize);
        T* firstBuf  = buffer.data();
        T* secondBuf = buffer.data() + rowStrideUnion*portionSize;

        // Pack
        util::ColStridedPack<T,D>
        ( height, A.LocalWidth(),
          B.ColAlign(), rowStrideUnion,
          A.LockedBuffer(), A.LDim(),
          secondBuf,        portionSize );

        // Realign the input
        mpi::SendRecv
        ( secondBuf, rowStrideUnion*portionSize, sendRowRankPart,
          firstBuf,  rowStrideUnion*portionSize, recvRowRankPart,
          A.PartialRowComm() );

        // Simultaneously Scatter in rows and Gather in columns
        mpi::AllToAll
        ( firstBuf,  portionSize,
          secondBuf, portionSize, A.PartialUnionRowComm() );

        // Unpack
        util::PartialRowStridedUnpack<T,D>
        ( B.LocalHeight(), width,
          rowAlign, rowStride,
          rowStrideUnion, rowStridePart, recvRowRankPart,
          B.RowShift(),
          secondBuf, portionSize,
          B.Buffer(), B.LDim() );
    }
}

template<typename T,Dist U,Dist V>
void RowAllToAllPromote
( const DistMatrix<T,                U,             V   ,BLOCK>& A,
        DistMatrix<T,PartialUnionCol<U,V>(),Partial<V>(),BLOCK>& B )
{
    EL_DEBUG_CSE
    AssertSameGrids( A, B );
    // TODO(poulson): More efficient implementation
    GeneralPurpose( A, B );
}

} // namespace copy
} // namespace El

#endif // ifndef EL_BLAS_COPY_ROWALLTOALLPROMOTE_HPP
