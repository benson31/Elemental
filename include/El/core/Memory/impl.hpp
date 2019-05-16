/*
  Copyright (c) 2009-2016, Jack Poulson
  All rights reserved.

  This file is part of Elemental and is under the BSD 2-Clause License,
  which can be found in the LICENSE file in the root directory, or at
  http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_CORE_MEMORY_IMPL_HPP_
#define EL_CORE_MEMORY_IMPL_HPP_

#include <iostream>
#include <sstream>

#ifdef HYDROGEN_HAVE_CUDA
#include <cuda_runtime.h>
#endif // HYDROGEN_HAVE_CUDA

#include "El/hydrogen_config.h"
#include "decl.hpp"

namespace El
{

namespace
{

template <typename G>
G* New(size_t size, unsigned int mode, SyncInfo<Device::CPU> const&)
{
    G* ptr = nullptr;
    switch (mode) {
    case 0: ptr = new G[size]; break;
#ifdef HYDROGEN_HAVE_CUDA
    case 1:
    {
        // Pinned memory
        auto error = cudaMallocHost(&ptr, size * sizeof(G));
        if (error != cudaSuccess)
        {
            RuntimeError("Failed to allocate pinned memory with message: ",
                         "\"", cudaGetErrorString(error), "\"");
        }
    }
    break;
#endif // HYDROGEN_HAVE_CUDA
    default: RuntimeError("Invalid CPU memory allocation mode");
    }
    return ptr;
}

template <typename G>
void Delete( G*& ptr, unsigned int mode, SyncInfo<Device::CPU> const& )
{
    switch (mode) {
    case 0: delete[] ptr; break;
#ifdef HYDROGEN_HAVE_CUDA
    case 1:
    {
        // Pinned memory
        auto error = cudaFreeHost(ptr);
        if (error != cudaSuccess)
        {
            RuntimeError("Failed to free pinned memory with message: ",
                         "\"", cudaGetErrorString(error), "\"");
        }
    }
    break;
#endif // HYDROGEN_HAVE_CUDA
    default: RuntimeError("Invalid CPU memory deallocation mode");
    }
    ptr = nullptr;
}

template <typename G>
void MemZero( G* buffer, size_t numEntries, unsigned int mode,
              SyncInfo<Device::CPU> const& )
{
    MemZero(buffer, numEntries);
}

#ifdef HYDROGEN_HAVE_CUDA

template <typename G>
G* New( size_t size, unsigned int mode, SyncInfo<Device::GPU> const& syncInfo_ )
{
    // Allocate memory
    G* ptr = nullptr;
    cudaError_t status = cudaSuccess;
    switch (mode) {
    case 0: status = cudaMalloc(&ptr, size * sizeof(G)); break;
#ifdef HYDROGEN_HAVE_CUB
    case 1:
        status = cub::MemoryPool().DeviceAllocate(
            reinterpret_cast<void**>(&ptr),
            size * sizeof(G),
            syncInfo_.Stream());
        break;
#endif // HYDROGEN_HAVE_CUB
    default: RuntimeError("Invalid GPU memory allocation mode");
    }

    // Check for errors
    if (status != cudaSuccess)
    {
        size_t freeMemory = 0;
        size_t totalMemory = 0;
        cudaMemGetInfo(&freeMemory, &totalMemory);
        RuntimeError("Failed to allocate GPU memory with message: ",
                     "\"", cudaGetErrorString(status), "\" ",
                     "(",size*sizeof(G)," bytes requested, ",
                     freeMemory," bytes available, ",
                     totalMemory," bytes total)");
    }

    return ptr;
}

template <typename G>
void Delete( G*& ptr, unsigned int mode, SyncInfo<Device::GPU> const& )
{
    switch (mode) {
    case 0: EL_CHECK_CUDA(cudaFree(ptr)); break;
#ifdef HYDROGEN_HAVE_CUB
    case 1:
        EL_CHECK_CUDA(
            cub::MemoryPool().DeviceFree(reinterpret_cast<void*>(ptr)));
        break;
#endif // HYDROGEN_HAVE_CUB
    default: RuntimeError("Invalid GPU memory deallocation mode");
    }
    ptr = nullptr;
}

template <typename G>
void MemZero( G* buffer, size_t numEntries, unsigned int mode,
                     SyncInfo<Device::GPU> const& syncInfo_ )
{
    EL_CHECK_CUDA(
        cudaMemsetAsync(buffer, 0x0, numEntries * sizeof(G),
                        syncInfo_.Stream()));
}

#endif // HYDROGEN_HAVE_CUDA

} // namespace <anonymous>

template<typename G, Device D>
Memory<G,D>::Memory(SyncInfo<D> const& syncInfo)
    : size_{0}, rawBuffer_{nullptr}, buffer_{nullptr}, syncInfo_{syncInfo}
{ }

template<typename G, Device D>
Memory<G,D>::Memory(size_t size, SyncInfo<D> const& syncInfo)
    : size_{0}, rawBuffer_{nullptr}, buffer_{nullptr}, syncInfo_{syncInfo}
{ Require(size); }

template<typename G, Device D>
Memory<G,D>::Memory(size_t size, unsigned int mode, SyncInfo<D> const& syncInfo)
    : size_{0}, rawBuffer_{nullptr}, buffer_{nullptr}, mode_{mode},
      syncInfo_{syncInfo}
{ Require(size); }

template<typename G, Device D>
Memory<G,D>::Memory(Memory<G,D>&& mem)
    : size_{0}, rawBuffer_{nullptr}, buffer_{nullptr}
{ ShallowSwap(mem); }

template<typename G, Device D>
Memory<G,D>& Memory<G,D>::operator=(Memory<G,D>&& mem)
{ ShallowSwap(mem); return *this; }

template<typename G, Device D>
void Memory<G,D>::ShallowSwap(Memory<G,D>& mem) EL_NO_EXCEPT
{
    std::swap(size_, mem.size_);
    std::swap(rawBuffer_, mem.rawBuffer_);
    std::swap(buffer_, mem.buffer_);
    std::swap(mode_, mem.mode_);
    std::swap(syncInfo_, mem.syncInfo_);
}

template<typename G, Device D>
Memory<G,D>::~Memory()
{ Empty(); }

template<typename G, Device D>
G* Memory<G,D>::Buffer() const EL_NO_EXCEPT { return buffer_; }

template<typename G, Device D>
size_t  Memory<G,D>::Size() const EL_NO_EXCEPT { return size_; }

template<typename G, Device D>
G* Memory<G,D>::Require(size_t size)
{
    if(size > size_)
    {
        Empty();
#ifndef EL_RELEASE
        try
        {
#endif
            // TODO: Optionally overallocate to force alignment of buffer_
            rawBuffer_ = New<G>(size, mode_, syncInfo_);
            buffer_ = rawBuffer_;
            size_ = size;
#ifndef EL_RELEASE
        }
        catch(std::bad_alloc& e)
        {
            size_ = 0;
            std::ostringstream os;
            os << "Failed to allocate " << size*sizeof(G)
               << " bytes on process " << mpi::Rank() << std::endl;
            std::cerr << os.str();
            throw e;
        }
#endif
#ifdef EL_ZERO_INIT
        MemZero(buffer_, size_, mode_, syncInfo_);
#elif defined(EL_HAVE_VALGRIND)
        if(EL_RUNNING_ON_VALGRIND)
            MemZero(buffer_, size_, mode_, syncInfo_);
#endif
    }
    return buffer_;
}

template<typename G, Device D>
void Memory<G,D>::Release()
{ this->Empty(); }

template<typename G, Device D>
void Memory<G,D>::Empty()
{
    if(rawBuffer_ != nullptr)
    {
        Delete(rawBuffer_, mode_, syncInfo_);
    }
    buffer_ = nullptr;
    size_ = 0;
}

template <typename G, Device D>
void Memory<G,D>::ResetSyncInfo(SyncInfo<D> const& syncInfo)
{
#ifdef HYDROGEN_HAVE_CUDA
    // FIXME: This treats this case as an error. Alternatively, this
    // could reallocate memory. See SetMode() below.
    if ((size_ > 0) && (D == Device::GPU) && (mode_ == 1))
    {
        LogicError("Cannot assign new SyncInfo object to "
                   "already-allocated CUB memory.");
    }
#endif // HYDROGEN_HAVE_CUDA

    syncInfo_ = syncInfo;
}

template <typename G, Device D>
SyncInfo<D> const& Memory<G,D>::GetSyncInfo() const
{
    return syncInfo_;
}

template<typename G, Device D>
void Memory<G,D>::SetMode(unsigned int mode)
{
    if (size_ > 0 && mode_ != mode)
    {
        Delete(rawBuffer_, mode_, syncInfo_);
        rawBuffer_ = New<G>(size_, mode, syncInfo_);
        buffer_ = rawBuffer_;
    }
    mode_ = mode;
}

template<typename G, Device D>
unsigned int Memory<G,D>::Mode() const
{ return mode_; }

#ifdef EL_INSTANTIATE_CORE
# define EL_EXTERN
#else
# define EL_EXTERN// extern
#endif

#if 0
#define PROTO(T) EL_EXTERN template class Memory<T,Device::CPU>;
#define EL_ENABLE_DOUBLEDOUBLE
#define EL_ENABLE_QUADDOUBLE
#define EL_ENABLE_QUAD
#define EL_ENABLE_BIGINT
#define EL_ENABLE_BIGFLOAT
#include <El/macros/Instantiate.h>
#endif // 0

EL_EXTERN template class Memory<double, Device::CPU>;

// GPU instantiations
#ifdef HYDROGEN_HAVE_CUDA
EL_EXTERN template class Memory<float, Device::GPU>;
EL_EXTERN template class Memory<double, Device::GPU>;
#endif

#undef EL_EXTERN

} // namespace El
#endif // EL_CORE_MEMORY_IMPL_HPP_
