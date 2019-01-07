/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/

#include "nvToolsExt.h"
#include "nvToolsExtCuda.h"
#include "nvToolsExtCudaRt.h"
#include "cuda_runtime.h"

namespace El {
extern std::ofstream root_debug_ofs_;

namespace gemm {

// Cannon's algorithm
template<typename T>
void Cannon_NN
(T alpha,
  const AbstractDistMatrix<T>& APre,
  const AbstractDistMatrix<T>& BPre,
        AbstractDistMatrix<T>& CPre)
{
    EL_DEBUG_CSE

    if (APre.GetLocalDevice() != Device::CPU)
        LogicError("Cannon_NN not implemented for device!");

    const Grid& g = APre.Grid();
    if (g.Height() != g.Width())
        LogicError("Process grid must be square for Cannon's");

    // Force A, B, and C to be in [MC,MR] distributions aligned with C
    DistMatrixReadWriteProxy<T,T,MC,MR> CProx(CPre);
    auto& C = CProx.Get();

    ElementalProxyCtrl ctrlA, ctrlB;
    ctrlA.colConstrain = true; ctrlA.colAlign = C.ColAlign();
    ctrlB.rowConstrain = true; ctrlB.rowAlign = C.RowAlign();

    DistMatrixReadProxy<T,T,MC,MR> AProx(APre, ctrlA);
    DistMatrixReadProxy<T,T,MC,MR> BProx(BPre, ctrlB);
    auto& A = AProx.GetLocked();
    auto& B = BProx.GetLocked();

    const Int row = g.Row();
    const Int col = g.Col();
    const Int pSqrt = g.Height();
    mpi::Comm rowComm = g.RowComm();
    mpi::Comm colComm = g.ColComm();
    if (A.Width() % pSqrt != 0)
        LogicError("For now, width(A) must be integer multiple of sqrt(p)");

    // Load the initial A and B packages (may want to transpose B...)
    const Int localHeightA = A.LocalHeight();
    const Int localHeightB = B.LocalHeight();
    const Int localWidthA = A.LocalWidth();
    const Int localWidthB = B.LocalWidth();
    Matrix<T> pkgA(localHeightA,localWidthA,localHeightA),
              pkgB(localHeightB,localWidthB,localHeightB);
    for(Int jLoc=0; jLoc<localWidthA; ++jLoc)
        MemCopy
        (pkgA.Buffer(0,jLoc), A.LockedBuffer(0,jLoc), localHeightA);
    for(Int jLoc=0; jLoc<localWidthB; ++jLoc)
        MemCopy
        (pkgB.Buffer(0,jLoc), B.LockedBuffer(0,jLoc), localHeightB);

    // Perform the initial circular shifts so that our A and B packages align
    const Int rowShiftA = A.RowShift();
    const Int colShiftB = B.ColShift();
    const Int leftInitA  = Mod(col-colShiftB,pSqrt);
    const Int rightInitA = Mod(col+colShiftB,pSqrt);
    const Int aboveInitB = Mod(row-rowShiftA,pSqrt);
    const Int belowInitB = Mod(row+rowShiftA,pSqrt);
    const Int pkgSizeA = localHeightA*localWidthA;
    const Int pkgSizeB = localHeightB*localWidthB;
    mpi::SendRecv(pkgA.Buffer(), pkgSizeA, leftInitA, rightInitA, rowComm,
                  SyncInfo<Device::CPU>{});
    mpi::SendRecv(pkgB.Buffer(), pkgSizeB, aboveInitB, belowInitB, colComm,
                  SyncInfo<Device::CPU>{});

    // Now begin the data flow
    const Int aboveRow = Mod(row-1,pSqrt);
    const Int belowRow = Mod(row+1,pSqrt);
    const Int leftCol  = Mod(col-1,pSqrt);
    const Int rightCol = Mod(col+1,pSqrt);
    for(Int q=0; q<pSqrt; ++q)
    {
        Gemm(NORMAL, NORMAL, alpha, pkgA, pkgB, T(1), C.Matrix());
        if (q != pSqrt-1)
        {
            mpi::SendRecv(
                pkgA.Buffer(), pkgSizeA, leftCol, rightCol, rowComm,
                SyncInfo<Device::CPU>{});
            mpi::SendRecv(
                pkgB.Buffer(), pkgSizeB, aboveRow, belowRow, colComm,
                SyncInfo<Device::CPU>{});
        }
    }
}

// Normal Normal Gemm that avoids communicating the matrix A
template <Device D, typename T, typename=EnableIf<IsDeviceValidType<T,D>>>
void SUMMA_NNA_impl
(T alpha,
 AbstractDistMatrix<T> const& APre,
 AbstractDistMatrix<T> const& BPre,
 AbstractDistMatrix<T>& CPre)
{
    EL_DEBUG_CSE
    AUTO_PROFILE_REGION(
        "SUMMA.NNA",
        SyncInfoFromMatrix(
            static_cast<Matrix<T,D> const&>(CPre.LockedMatrix())));

    const Int n = CPre.Width();
    const Int bsize = Blocksize();
    const Grid& g = APre.Grid();

    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> AProx(APre);
    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> BProx(BPre);
    DistMatrixReadWriteProxy<T,T,MC,MR,ELEMENT,D> CProx(CPre);
    auto& A = AProx.GetLocked();
    auto& B = BProx.GetLocked();
    auto& C = CProx.Get();

    // Temporary distributions
    DistMatrix<T,VR,STAR,ELEMENT,D> B1_VR_STAR(g);
    DistMatrix<T,STAR,MR,ELEMENT,D> B1Trans_STAR_MR(g);
    DistMatrix<T,MC,STAR,ELEMENT,D> D1_MC_STAR(g);

    B1_VR_STAR.AlignWith(A);
    B1Trans_STAR_MR.AlignWith(A);
    D1_MC_STAR.AlignWith(A);

    for(Int k=0; k<n; k+=bsize)
    {
        const Int nb = Min(bsize,n-k);
        auto B1 = B(ALL, IR(k,k+nb));
        auto C1 = C(ALL, IR(k,k+nb));

        // D1[MC,*] := alpha A[MC,MR] B1[MR,*]
        B1_VR_STAR = B1;
        Transpose(B1_VR_STAR, B1Trans_STAR_MR);
        LocalGemm(NORMAL, TRANSPOSE, alpha, A, B1Trans_STAR_MR, D1_MC_STAR);

        // C1[MC,MR] += scattered result of D1[MC,*] summed over grid rows
        AxpyContract(T(1), D1_MC_STAR, C1);
    }
}

template <Device D, typename T,
          typename=DisableIf<IsDeviceValidType<T,D>>, typename=void>
void SUMMA_NNA_impl(T alpha,
                    AbstractDistMatrix<T> const& APre,
                    AbstractDistMatrix<T> const& BPre,
                    AbstractDistMatrix<T>& CPre)
{
    LogicError("SUMMA_NNA_impl type-device combo not supported.");
}

template <typename T>
void SUMMA_NNA
(T alpha,
 AbstractDistMatrix<T> const& APre,
 AbstractDistMatrix<T> const& BPre,
 AbstractDistMatrix<T>& CPre)
{
    EL_DEBUG_CSE

    switch (CPre.GetLocalDevice())
    {
    case Device::CPU:
        SUMMA_NNA_impl<Device::CPU>(alpha, APre, BPre, CPre);
        break;
#ifdef HYDROGEN_HAVE_CUDA
    case Device::GPU:
        SUMMA_NNA_impl<Device::GPU>(alpha, APre, BPre, CPre);
        break;
#endif // HYDROGEN_HAVE_CUDA
    default:
        LogicError("SUMMA_NNA: Bad device.");
    }
}

// Normal Normal Gemm that avoids communicating the matrix B
template <Device D, typename T, typename=EnableIf<IsDeviceValidType<T,D>>>
void SUMMA_NNB_impl
(T alpha,
  const AbstractDistMatrix<T>& APre,
  const AbstractDistMatrix<T>& BPre,
        AbstractDistMatrix<T>& CPre)
{
    EL_DEBUG_CSE
    AUTO_PROFILE_REGION(
        "SUMMA.NNB",
        SyncInfoFromMatrix(
            static_cast<Matrix<T,D> const&>(CPre.LockedMatrix())));

    const Int m = CPre.Height();
    const Int bsize = Blocksize();
    const Grid& g = APre.Grid();

    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> AProx(APre);
    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> BProx(BPre);
    DistMatrixReadWriteProxy<T,T,MC,MR,ELEMENT,D> CProx(CPre);
    auto& A = AProx.GetLocked();
    auto& B = BProx.GetLocked();
    auto& C = CProx.Get();

    // Temporary distributions
    DistMatrix<T,STAR,MC,ELEMENT,D> A1_STAR_MC(g);
    DistMatrix<T,MR,STAR,ELEMENT,D> D1Trans_MR_STAR(g);

    A1_STAR_MC.AlignWith(B);
    D1Trans_MR_STAR.AlignWith(B);

    for(Int k=0; k<m; k+=bsize)
    {
        const Int nb = Min(bsize,m-k);
        auto A1 = A(IR(k,k+nb), ALL);
        auto C1 = C(IR(k,k+nb), ALL);

        // D1^T[MR,* ] := alpha B^T[MR,MC] A1^T[MC,* ]
        A1_STAR_MC = A1;
        LocalGemm(
            TRANSPOSE, TRANSPOSE, alpha, B, A1_STAR_MC, D1Trans_MR_STAR);

        TransposeAxpyContract(T(1), D1Trans_MR_STAR, C1);
    }
}

template <Device D, typename T,
          typename=DisableIf<IsDeviceValidType<T,D>>, typename=void>
void SUMMA_NNB_impl(T alpha,
                    AbstractDistMatrix<T> const& APre,
                    AbstractDistMatrix<T> const& BPre,
                    AbstractDistMatrix<T>& CPre)
{
    LogicError("SUMMA_NNB_impl type-device combo not supported.");
}

template <typename T>
void SUMMA_NNB
(T alpha,
 AbstractDistMatrix<T> const& APre,
 AbstractDistMatrix<T> const& BPre,
 AbstractDistMatrix<T>& CPre)
{
    EL_DEBUG_CSE

    switch (CPre.GetLocalDevice())
    {
    case Device::CPU:
        SUMMA_NNB_impl<Device::CPU>(alpha, APre, BPre, CPre);
        break;
#ifdef HYDROGEN_HAVE_CUDA
    case Device::GPU:
        SUMMA_NNB_impl<Device::GPU>(alpha, APre, BPre, CPre);
        break;
#endif // HYDROGEN_HAVE_CUDA
    default:
        LogicError("SUMMA_NNB: Bad device.");
    }
}

// Hack hack hack
namespace // anon
{

size_t getDefaultSyncPoolSize() noexcept
{
    return 4;
}

std::vector<SyncInfo<Device::GPU>> syncPool;
size_t syncPool_size = getDefaultSyncPoolSize();
bool syncPool_initialized = false;

void ensureSyncPoolSize(size_t size = syncPool_size) noexcept
{
    if (size > syncPool.size())
    {
        syncPool.resize(size, SyncInfo<Device::GPU>{nullptr,nullptr});
        syncPool_size = size;
        syncPool_initialized = false;
    }
}

void ensureSyncPoolInit()
{
    if (!syncPool_initialized)
    {
        for (auto&& si : syncPool)
        {
            if (!si.stream_)
            {
                cudaStreamCreateWithPriority(
                    &si.stream_, cudaStreamNonBlocking, 100);
            }
            if (!si.event_)
            {
                cudaEventCreateWithFlags(&si.event_, cudaEventDisableTiming);
            }
        }
        syncPool_initialized = true;
    }
}

std::vector<SyncInfo<Device::GPU>> const& getSyncPool() noexcept
{
    ensureSyncPoolSize();
    ensureSyncPoolInit();
    return syncPool;
}

}// namespace <anon>

template <typename T>
struct TypeCheck;

template <Device D, typename T, typename=EnableIf<IsDeviceValidType<T,D>>>
void SUMMA_NNC_impl_gpu_multistream(T alpha,
                                    AbstractDistMatrix<T> const& APre,
                                    AbstractDistMatrix<T> const& BPre,
                                    AbstractDistMatrix<T>& CPre)
{
//    nvtxDomainHandle_t domain = nvtxDomainCreateA("SUMMA_NNC_MS");
    if (D != Device::GPU)
        LogicError("GPU only.");

    AUTO_PROFILE_REGION(
        "SUMMA.NNC.Multistream",
        SyncInfoFromMatrix(
            static_cast<Matrix<T,D> const&>(CPre.LockedMatrix())));
    root_debug_ofs_ << "BEGIN SUMMA_NNC_impl_gpu_multistream()" << std::endl;
    EL_DEBUG_CSE
    const Int sumDim = APre.Width();
    const Int bsize = Blocksize();
    const Grid& g = APre.Grid();
    const Int nblks = sumDim / bsize + 1;

    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> AProx(APre);
    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> BProx(BPre);
    DistMatrixReadWriteProxy<T,T,MC,MR,ELEMENT,D> CProx(CPre);
    auto& A = AProx.GetLocked();
    auto& B = BProx.GetLocked();
    auto& C = CProx.Get();

    // Temporary distributions
    auto&& syncpool = getSyncPool();
    auto numstreams = std::min(syncpool.size(), static_cast<size_t>(nblks));

    std::vector<DistMatrix<T,MC,STAR,ELEMENT,D>> A1_MC_STAR;
    std::vector<DistMatrix<T,MR,STAR,ELEMENT,D>> B1Trans_MR_STAR;
    std::vector<DistMatrix<T,MC,MR,ELEMENT,D>> C_Views;

    A1_MC_STAR.reserve(numstreams);
    B1Trans_MR_STAR.reserve(numstreams);
    C_Views.reserve(numstreams);

    root_debug_ofs_ << "Setting up " << numstreams << " temporary matrices."
                    << std::endl;

    for (auto id = 0UL; id < numstreams; ++id)
    {
        root_debug_ofs_ << "Stream " << id << ": {stream:" << get_stream_name(syncPool[id].stream_)
                        << ", event:" << get_event_name(syncPool[id].event_) << "}" << std::endl;

        auto A1 = A1_MC_STAR.emplace(A1_MC_STAR.end(), g);
        auto B1 = B1Trans_MR_STAR.emplace(B1Trans_MR_STAR.end(), g);
        auto C1 = C_Views.emplace(C_Views.end(), g);

        A1->AlignWith(C);
        B1->AlignWith(C);
        View(*C1, C);

        SetSyncInfo(A1->Matrix(), syncPool[id]);
        SetSyncInfo(B1->Matrix(), syncPool[id]);
        SetSyncInfo(C1->Matrix(), syncPool[id]);
    }
    root_debug_ofs_ << "Done setting up temporary matrices.\n"
                    << "Launching block Gemms..." << std::endl;

    Int k = 0;
    size_t nblks_per_stream = nblks / numstreams;
    for (size_t blk = 0; blk < nblks_per_stream; ++blk)
    {
        for (size_t sid = 0; sid < numstreams; ++sid)
        {
            root_debug_ofs_ << "Starting blk " << blk << " on stream " << sid << std::endl;

            std::string id = std::string("Blk.") + std::to_string(blk) + ".SID."

                + std::to_string(sid);
            AUTO_PROFILE_REGION(
                std::move(id),
                SyncInfoFromMatrix(
                    static_cast<Matrix<T,D> const&>(
                        C_Views[sid].LockedMatrix())));

            DistMatrix<T,MC,MR,ELEMENT,D> A1(g), B1(g);
            const Int nb = Min(bsize,sumDim-k);
            {
                root_debug_ofs_ << "-- Setup A1" << std::endl;
                AUTO_NOSYNC_PROFILE_REGION("A1");
                SetSyncInfo(A1.Matrix(), syncPool[sid]);
                A1 = A(ALL,        IR(k,k+nb));
                root_debug_ofs_ << "-- DONE setup A1" << std::endl;
            }
            {
                root_debug_ofs_ << "-- Setup B1" << std::endl;
                AUTO_NOSYNC_PROFILE_REGION("B1");
                SetSyncInfo(B1.Matrix(), syncPool[sid]);
                B1 = B(IR(k,k+nb), ALL       );
                root_debug_ofs_ << "-- DONE setup B1" << std::endl;
            }

            // C[MC,MR] += alpha A1[MC,*] (B1^T[MR,*])^T
            //           = alpha A1[MC,*] B1[*,MR]
            {
                root_debug_ofs_ << "-- Setup A1_MC_STAR" << std::endl;

                AUTO_NOSYNC_PROFILE_REGION("A1_MC_STAR");
                A1_MC_STAR[sid] = A1;
                root_debug_ofs_ << "-- DONE setup A1_MC_STAR" << std::endl;
            }
            {
                root_debug_ofs_ << "-- Setup B1Trans_MR_STAR" << std::endl;
                AUTO_NOSYNC_PROFILE_REGION("B1T_MR_STAR");
                Transpose(B1, B1Trans_MR_STAR[sid]);
                root_debug_ofs_ << "-- DONE setup B1Trans_MR_STAR" << std::endl;
            }
            {
                root_debug_ofs_ << "-- LocalGemm" << std::endl;
                AUTO_NOSYNC_PROFILE_REGION("LocalGemm");
                LocalGemm(NORMAL, TRANSPOSE,
                          alpha, A1_MC_STAR[sid], B1Trans_MR_STAR[sid],
                          T(1), C_Views[sid]);
                root_debug_ofs_ << "-- Done LocalGemm" << std::endl;
            }

            // Update the blocksize
            k += bsize;

            root_debug_ofs_ << "Done with blk " << blk << " on stream " << sid << std::endl;
        }
    }
    root_debug_ofs_ << "Done setting up temporary matrices.\n"
                    << "END SUMMA_NNC_impl_gpu_multistream()" << std::endl;
//    nvtxDomainDestroy(domain);
}


template <Device D, typename T, typename=EnableIf<IsDeviceValidType<T,D>>>
void SUMMA_NNC_impl_gpu_multistream_two(T alpha,
                                        AbstractDistMatrix<T> const& APre,
                                        AbstractDistMatrix<T> const& BPre,
                                        AbstractDistMatrix<T>& CPre)
{
//    nvtxDomainHandle_t domain = nvtxDomainCreateA("SUMMA_NNC_MS");
    if (D != Device::GPU)
        LogicError("GPU only.");

    AUTO_PROFILE_REGION(
        "SUMMA.NNC.Multistream.2",
        SyncInfoFromMatrix(
            static_cast<Matrix<T,D> const&>(CPre.LockedMatrix())));
    root_debug_ofs_ << "BEGIN SUMMA_NNC_impl_gpu_multistream()" << std::endl;
    EL_DEBUG_CSE
    const Int sumDim = APre.Width();
    const Int bsize = Blocksize();
    const Grid& g = APre.Grid();
    const Int nblks = sumDim / bsize + 1;

    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> AProx(APre);
    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> BProx(BPre);
    DistMatrixReadWriteProxy<T,T,MC,MR,ELEMENT,D> CProx(CPre);
    auto& A = AProx.GetLocked();
    auto& B = BProx.GetLocked();
    auto& C = CProx.Get();

    // Temporary distributions
    auto&& syncpool = getSyncPool();
    auto numstreams = std::min(syncpool.size(), static_cast<size_t>(nblks));

    std::vector<DistMatrix<T,MC,STAR,ELEMENT,D>> A1_MC_STAR;
    std::vector<DistMatrix<T,MR,STAR,ELEMENT,D>> B1Trans_MR_STAR;
    std::vector<DistMatrix<T,MC,MR,ELEMENT,D>> C_Views;

    A1_MC_STAR.reserve(numstreams);
    B1Trans_MR_STAR.reserve(numstreams);
    C_Views.reserve(numstreams);

    root_debug_ofs_ << "Setting up " << numstreams << " temporary matrices."
                    << std::endl;

    for (auto id = 0UL; id < numstreams; ++id)
    {
        root_debug_ofs_ << "Stream " << id << ": {stream:" << get_stream_name(syncPool[id].stream_)
                        << ", event:" << get_event_name(syncPool[id].event_) << "}" << std::endl;

        auto A1 = A1_MC_STAR.emplace(A1_MC_STAR.end(), g);
        auto B1 = B1Trans_MR_STAR.emplace(B1Trans_MR_STAR.end(), g);
        auto C1 = C_Views.emplace(C_Views.end(), g);

        A1->AlignWith(C);
        B1->AlignWith(C);
        View(*C1, C);

        SetSyncInfo(A1->Matrix(), syncPool[id]);
        SetSyncInfo(B1->Matrix(), syncPool[id]);
        SetSyncInfo(C1->Matrix(), syncPool[id]);
    }
    root_debug_ofs_ << "Done setting up temporary matrices.\n"
                    << "Launching block Gemms..." << std::endl;

    Int k = 0;
    while (k < sumDim)
    {
        // Launch N communications
        Int k_start = k;
        for (size_t sid = 0; sid < numstreams; ++sid)
        {
            if (k_start >= sumDim)
                continue;

            std::string id = std::string("SID.") + std::to_string(sid);
            AUTO_PROFILE_REGION(
                std::move(id),
                SyncInfoFromMatrix(
                    static_cast<Matrix<T,D> const&>(
                        C_Views[sid].LockedMatrix())));

            DistMatrix<T,MC,MR,ELEMENT,D> A1(g), B1(g);
            const Int nb = Min(bsize,sumDim-k_start);
            {
                root_debug_ofs_ << "-- Setup A1" << std::endl;
                AUTO_NOSYNC_PROFILE_REGION("A1");
                SetSyncInfo(A1.Matrix(), syncPool[sid]);
                A1 = A(ALL,        IR(k_start,k_start+nb));
                root_debug_ofs_ << "-- DONE setup A1" << std::endl;
            }
            {
                root_debug_ofs_ << "-- Setup B1" << std::endl;
                AUTO_NOSYNC_PROFILE_REGION("B1");
                SetSyncInfo(B1.Matrix(), syncPool[sid]);
                B1 = B(IR(k_start,k_start+nb), ALL       );
                root_debug_ofs_ << "-- DONE setup B1" << std::endl;
            }

            // C[MC,MR] += alpha A1[MC,*] (B1^T[MR,*])^T
            //           = alpha A1[MC,*] B1[*,MR]
            {
                root_debug_ofs_ << "-- Setup A1_MC_STAR" << std::endl;

                AUTO_NOSYNC_PROFILE_REGION("A1_MC_STAR");
                A1_MC_STAR[sid] = A1;
                root_debug_ofs_ << "-- DONE setup A1_MC_STAR" << std::endl;
            }
            {
                root_debug_ofs_ << "-- Setup B1Trans_MR_STAR" << std::endl;
                AUTO_NOSYNC_PROFILE_REGION("B1T_MR_STAR");
                Transpose(B1, B1Trans_MR_STAR[sid]);
                root_debug_ofs_ << "-- DONE setup B1Trans_MR_STAR" << std::endl;
            }

            k_start += bsize;
        }

        AddSynchronizationPoint(
            SyncInfo<Device::GPU>{GPUManager::Stream(),
                    GPUManager::Event()});
        for (size_t sid = 0; sid < numstreams; ++sid)
        {
            cudaStreamWaitEvent(syncPool[sid].stream_, GPUManager::Event(), 0);
        }

        // Launch N computations
        k_start = k;
        for (size_t sid = 0; sid < numstreams; ++sid)
        {
            if (k_start >= sumDim)
                continue;

            root_debug_ofs_ << "-- LocalGemm" << std::endl;
            AUTO_NOSYNC_PROFILE_REGION("LocalGemm");
            LocalGemm(NORMAL, TRANSPOSE,
                      alpha, A1_MC_STAR[sid], B1Trans_MR_STAR[sid],
                      T(1), C_Views[sid]);
            root_debug_ofs_ << "-- Done LocalGemm" << std::endl;

            k_start += bsize;
        }

        for (size_t sid = 0; sid < numstreams; ++sid)
        {
            AddSynchronizationPoint(syncPool[sid]);
            cudaStreamWaitEvent(GPUManager::Stream(), syncPool[sid].event_, 0);
        }

        k = k_start;
    }

//    nvtxDomainDestroy(domain);
}



// Normal Normal Gemm that avoids communicating the matrix C
template <Device D, typename T, typename=EnableIf<IsDeviceValidType<T,D>>>
void SUMMA_NNC_impl(T alpha,
                    AbstractDistMatrix<T> const& APre,
                    AbstractDistMatrix<T> const& BPre,
                    AbstractDistMatrix<T>& CPre)
{
    EL_DEBUG_CSE
    AUTO_PROFILE_REGION(
        "SUMMA.NNC",
        SyncInfoFromMatrix(
            static_cast<Matrix<T,D> const&>(CPre.LockedMatrix())));
    const Int sumDim = APre.Width();
    const Int bsize = Blocksize();
    const Grid& g = APre.Grid();

    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> AProx(APre);
    DistMatrixReadProxy<T,T,MC,MR,ELEMENT,D> BProx(BPre);
    DistMatrixReadWriteProxy<T,T,MC,MR,ELEMENT,D> CProx(CPre);
    auto& A = AProx.GetLocked();
    auto& B = BProx.GetLocked();
    auto& C = CProx.Get();

    // Temporary distributions
    DistMatrix<T,MC,STAR,ELEMENT,D> A1_MC_STAR(g);
    DistMatrix<T,MR,STAR,ELEMENT,D> B1Trans_MR_STAR(g);

    A1_MC_STAR.AlignWith(C);
    B1Trans_MR_STAR.AlignWith(C);

    for(Int k=0; k<sumDim; k+=bsize)
    {
        const Int nb = Min(bsize,sumDim-k);
        auto A1 = A(ALL,        IR(k,k+nb));
        auto B1 = B(IR(k,k+nb), ALL       );

        // C[MC,MR] += alpha A1[MC,*] (B1^T[MR,*])^T
        //           = alpha A1[MC,*] B1[*,MR]
        A1_MC_STAR = A1;
        Transpose(B1, B1Trans_MR_STAR);
        LocalGemm
        (NORMAL, TRANSPOSE, alpha, A1_MC_STAR, B1Trans_MR_STAR, T(1), C);
    }
}
template <Device D, typename T,
          typename=DisableIf<IsDeviceValidType<T,D>>,
          typename=void>
void SUMMA_NNC_impl_gpu_multistream(T alpha,
                                    AbstractDistMatrix<T> const& APre,
                                    AbstractDistMatrix<T> const& BPre,
                                    AbstractDistMatrix<T>& CPre)
{
    LogicError("SUMMA_NNC_impl type-device combo not supported.");
}

template <Device D, typename T,
          typename=DisableIf<IsDeviceValidType<T,D>>,
          typename=void>
void SUMMA_NNC_impl_gpu_multistream_two(T alpha,
                                    AbstractDistMatrix<T> const& APre,
                                    AbstractDistMatrix<T> const& BPre,
                                    AbstractDistMatrix<T>& CPre)
{
    LogicError("SUMMA_NNC_impl type-device combo not supported.");
}

template <Device D, typename T,
          typename=DisableIf<IsDeviceValidType<T,D>>, typename=void>
void SUMMA_NNC_impl(T alpha,
               AbstractDistMatrix<T> const& APre,
               AbstractDistMatrix<T> const& BPre,
               AbstractDistMatrix<T>& CPre)
{
    LogicError("SUMMA_NNC_impl type-device combo not supported.");
}

#define USE_MULTISTREAM_GEMM

template<typename T>
void SUMMA_NNC
(T alpha,
  const AbstractDistMatrix<T>& APre,
  const AbstractDistMatrix<T>& BPre,
        AbstractDistMatrix<T>& CPre)
{
    EL_DEBUG_CSE

    switch (CPre.GetLocalDevice())
    {
    case Device::CPU:
        SUMMA_NNC_impl<Device::CPU>(alpha, APre, BPre, CPre);
        break;
#ifdef HYDROGEN_HAVE_CUDA
    case Device::GPU:
#ifdef USE_MULTISTREAM_GEMM
        SUMMA_NNC_impl_gpu_multistream_two<Device::GPU>(alpha, APre, BPre, CPre);
#else
        SUMMA_NNC_impl<Device::GPU>(alpha, APre, BPre, CPre);
#endif // USE_MULTISTREAM_GEMM
        break;
#endif // HYDROGEN_HAVE_CUDA
    default:
        LogicError("SUMMA_NNC: Bad device.");
    }

}

// Normal Normal Gemm for panel-panel dot products
//
// Use summations of local multiplications from a 1D distribution of A and B
// to update blockSize x blockSize submatrices of C
//
template <Device D, typename T, typename=EnableIf<IsDeviceValidType<T,D>>>
void SUMMA_NNDot_impl
(T alpha,
  const AbstractDistMatrix<T>& APre,
  const AbstractDistMatrix<T>& BPre,
        AbstractDistMatrix<T>& CPre,
  Int blockSize)
{
    EL_DEBUG_CSE
    AUTO_PROFILE_REGION(
        "SUMMA.NNDot",
        SyncInfoFromMatrix(
            static_cast<Matrix<T,D> const&>(CPre.LockedMatrix())));

    const Int m = CPre.Height();
    const Int n = CPre.Width();
    const Grid& g = APre.Grid();

    DistMatrixReadProxy<T,T,STAR,VC,ELEMENT,D> AProx(APre);
    auto& A = AProx.GetLocked();

    ElementalProxyCtrl BCtrl;
    BCtrl.colConstrain = true;
    BCtrl.colAlign = A.RowAlign();
    DistMatrixReadProxy<T,T,VC,STAR,ELEMENT,D> BProx(BPre, BCtrl);
    auto& B = BProx.GetLocked();

    DistMatrixReadWriteProxy<T,T,MC,MR,ELEMENT,D> CProx(CPre);
    auto& C = CProx.Get();

    DistMatrix<T,STAR,STAR,ELEMENT,D> C11_STAR_STAR(g);
    for(Int kOuter=0; kOuter<m; kOuter+=blockSize)
    {
        const Int nbOuter = Min(blockSize,m-kOuter);
        const Range<Int> indOuter(kOuter, kOuter+nbOuter);

        auto A1 = A(indOuter, ALL);

        for(Int kInner=0; kInner<n; kInner+=blockSize)
        {
            const Int nbInner = Min(blockSize,n-kInner);
            const Range<Int> indInner(kInner, kInner+nbInner);

            auto B1  = B(ALL,      indInner);
            auto C11 = C(indOuter, indInner);

            LocalGemm(NORMAL, NORMAL, alpha, A1, B1, C11_STAR_STAR);
            AxpyContract(T(1), C11_STAR_STAR, C11);
        }
    }
}

template <Device D, typename T,
          typename=DisableIf<IsDeviceValidType<T,D>>,typename=void>
void SUMMA_NNDot_impl
(T alpha,
  const AbstractDistMatrix<T>& APre,
  const AbstractDistMatrix<T>& BPre,
        AbstractDistMatrix<T>& CPre,
  Int blockSize)
{
    LogicError("SUMMA_NNDot_impl type-device combo not supported.");
}

template <typename T>
void SUMMA_NNDot
(T alpha,
  const AbstractDistMatrix<T>& APre,
  const AbstractDistMatrix<T>& BPre,
        AbstractDistMatrix<T>& CPre,
  Int blockSize=2000)
{
    EL_DEBUG_CSE

    switch (CPre.GetLocalDevice())
    {
    case Device::CPU:
        SUMMA_NNDot_impl<Device::CPU>(alpha, APre, BPre, CPre, blockSize);
        break;
#ifdef HYDROGEN_HAVE_CUDA
    case Device::GPU:
        SUMMA_NNDot_impl<Device::GPU>(alpha, APre, BPre, CPre, blockSize);
        break;
#endif // HYDROGEN_HAVE_CUDA
    default:
        LogicError("SUMMA_NNDot: Bad device.");
    }
}

template<typename T>
void SUMMA_NN
(T alpha,
  const AbstractDistMatrix<T>& A,
  const AbstractDistMatrix<T>& B,
        AbstractDistMatrix<T>& C,
  GemmAlgorithm alg=GEMM_DEFAULT)
{
    EL_DEBUG_CSE
    EL_DEBUG_ONLY(
      AssertSameGrids(A, B, C);
      if (A.Height() != C.Height() ||
          B.Width() != C.Width() ||
          A.Width() != B.Height())
          LogicError
          ("Nonconformal matrices:\n",
           DimsString(A,"A"),"\n",
           DimsString(B,"B"),"\n",
           DimsString(C,"C"));
   )

    const Int m = C.Height();
    const Int n = C.Width();
    const Int sumDim = A.Width();
    const double weightTowardsC = 2.;
    const double weightAwayFromDot = 10.;

    // TODO(poulson): Make this tunable
    const Int blockSizeDot = 2000;

    switch(alg)
    {
    case GEMM_DEFAULT:
        if (weightAwayFromDot*m <= sumDim && weightAwayFromDot*n <= sumDim)
        {
            // FIXME (trb 03/27/18): There's a correctness issue with
            // this method. This exception is for your own safety.
            SUMMA_NNDot(alpha, A, B, C, blockSizeDot);
        }
        else if (m <= n && weightTowardsC*m <= sumDim)
            SUMMA_NNB(alpha, A, B, C);
        else if (n <= m && weightTowardsC*n <= sumDim)
            SUMMA_NNA(alpha, A, B, C);
        else
            SUMMA_NNC(alpha, A, B, C);
        break;
    case GEMM_SUMMA_A:   SUMMA_NNA(alpha, A, B, C); break;
    case GEMM_SUMMA_B:   SUMMA_NNB(alpha, A, B, C); break;
    case GEMM_SUMMA_C:   SUMMA_NNC(alpha, A, B, C); break;
    case GEMM_SUMMA_DOT: SUMMA_NNDot(alpha, A, B, C, blockSizeDot); break;
    default: LogicError("Unsupported Gemm option");
    }
}

} // namespace gemm
} // namespace El
