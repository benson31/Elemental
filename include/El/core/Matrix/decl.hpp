/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#ifndef EL_MATRIX_DECL_HPP
#define EL_MATRIX_DECL_HPP

#include <El/core/Device.hpp>
#include <El/core/Grid.hpp>
#include <El/core/Memory.hpp>

namespace El
{

// Matrix base for arbitrary rings
template<typename Ring, Device Dev>
class Matrix;

// Specialization for CPU
template <typename Ring>
class Matrix<Ring, Device::CPU> : public AbstractMatrix<Ring>
{
public:
    // Constructors and destructors
    // ============================

    // Create a 0x0 matrix
    Matrix();

    // Create a matrix with the specified dimensions
    Matrix(Int height, Int width);

    // Create a matrix with the specified dimensions and leading dimension
    Matrix(Int height, Int width, Int leadingDimension);

    // Construct a matrix around an existing (possibly immutable) buffer
    Matrix(Int height, Int width, const Ring* buffer, Int leadingDimension);
    Matrix(Int height, Int width, Ring* buffer, Int leadingDimension);

    // Create a copy of a matrix
    Matrix(Matrix<Ring, Device::CPU> const& A);

    // Move the metadata from a given matrix
    Matrix(Matrix<Ring, Device::CPU>&& A) EL_NO_EXCEPT;

    // Destructor
    ~Matrix();

    // Copy assignment
    Matrix<Ring, Device::CPU> & operator=(
        Matrix<Ring, Device::CPU> const& A);

#ifdef HYDROGEN_HAVE_CUDA
    // Create a copy of a matrix from a GPU matrix
    Matrix(Matrix<Ring, Device::GPU> const& A);

    // Assign by copying data from a GPU
    Matrix<Ring, Device::CPU> & operator=(
        Matrix<Ring, Device::GPU> const& A);
#endif // HYDROGEN_HAVE_CUDA

    // Move assignment
    Matrix<Ring, Device::CPU>& operator=(Matrix<Ring, Device::CPU>&& A);

    //
    // Assignment and reconfiguration
    //

    // Reconfigure around the given buffer, but do not assume ownership
    void Attach
    (Int height, Int width, Ring* buffer, Int leadingDimension) override;
    void LockedAttach
    (Int height, Int width, const Ring* buffer, Int leadingDimension) override;

    // Reconfigure around the given buffer and assume ownership
    void Control
    (Int height, Int width, Ring* buffer, Int leadingDimension);

    //
    // Operator overloading
    //

    // Return a view
    Matrix<Ring, Device::CPU> operator()(Range<Int> I, Range<Int> J);
    const Matrix<Ring, Device::CPU> operator()(
        Range<Int> I, Range<Int> J) const;

    // Return a copy of (potentially non-contiguous) subset of indices
    Matrix<Ring, Device::CPU> operator()(
        Range<Int> I, vector<Int> const& J) const;
    Matrix<Ring, Device::CPU> operator()(
        vector<Int> const& I, Range<Int> J) const;
    Matrix<Ring, Device::CPU> operator()(
        vector<Int> const& I, vector<Int> const& J) const;

    //
    // Basic queries
    //

    Ring* Buffer() EL_NO_RELEASE_EXCEPT override;
    Ring* Buffer(Int i, Int j) EL_NO_RELEASE_EXCEPT override;
    const Ring* LockedBuffer() const EL_NO_EXCEPT override;
    const Ring* LockedBuffer(Int i, Int j) const EL_NO_EXCEPT override;

    //
    // Advanced functions
    //
    void SetMemoryMode(unsigned int mode) override;
    unsigned int MemoryMode() const EL_NO_EXCEPT override;

    // Single-entry manipulation
    // =========================
    Ring Get(Int i, Int j=0) const EL_NO_RELEASE_EXCEPT;
    Base<Ring> GetRealPart(Int i, Int j=0) const EL_NO_RELEASE_EXCEPT;
    Base<Ring> GetImagPart(Int i, Int j=0) const EL_NO_RELEASE_EXCEPT;

    void Set(Int i, Int j, Ring const& alpha) EL_NO_RELEASE_EXCEPT;
    void Set(Entry<Ring> const& entry) EL_NO_RELEASE_EXCEPT;

    void SetRealPart
    (Int i, Int j, Base<Ring> const& alpha) EL_NO_RELEASE_EXCEPT;
    void SetImagPart
    (Int i, Int j, Base<Ring> const& alpha) EL_NO_RELEASE_EXCEPT;

    void SetRealPart
    (Entry<Base<Ring>> const& entry) EL_NO_RELEASE_EXCEPT;
    void SetImagPart
    (Entry<Base<Ring>> const& entry) EL_NO_RELEASE_EXCEPT;

    void Update(Int i, Int j, Ring const& alpha) EL_NO_RELEASE_EXCEPT;
    void Update(Entry<Ring> const& entry) EL_NO_RELEASE_EXCEPT;

    void UpdateRealPart
    (Int i, Int j, Base<Ring> const& alpha) EL_NO_RELEASE_EXCEPT;
    void UpdateImagPart
    (Int i, Int j, Base<Ring> const& alpha) EL_NO_RELEASE_EXCEPT;

    void UpdateRealPart
    (Entry<Base<Ring>> const& entry) EL_NO_RELEASE_EXCEPT;
    void UpdateImagPart
    (Entry<Base<Ring>> const& entry) EL_NO_RELEASE_EXCEPT;

    void MakeReal(Int i, Int j) EL_NO_RELEASE_EXCEPT;
    void Conjugate(Int i, Int j) EL_NO_RELEASE_EXCEPT;

    // Return a reference to a single entry without error-checking
    // -----------------------------------------------------------
    inline Ring const& CRef(Int i, Int j=0) const EL_NO_RELEASE_EXCEPT override;
    inline Ring const& operator()(Int i, Int j=0) const EL_NO_RELEASE_EXCEPT override;

    inline Ring& Ref(Int i, Int j=0) EL_NO_RELEASE_EXCEPT override;
    inline Ring& operator()(Int i, Int j=0) EL_NO_RELEASE_EXCEPT override;

private:
    // Member variables
    // ================
    Memory<Ring,Device::CPU> memory_;
    // Const-correctness is internally managed to avoid the need for storing
    // two separate pointers with different 'const' attributes
    Ring* data_=nullptr;

    Int do_get_memory_size_() const EL_NO_EXCEPT override;
    Device do_get_device_() const EL_NO_EXCEPT override;

    // Exchange metadata with another matrix
    // =====================================
    void ShallowSwap(Matrix<Ring, Device::CPU>& A);

    // Reconfigure without error-checking
    // ==================================
    void do_empty_(bool freeMemory) override;
    void do_resize_() override;

    void Control_
    (Int height, Int width, Ring* buffer, Int leadingDimension);
    void Attach_
    (Int height, Int width, Ring* buffer, Int leadingDimension) override;
    void LockedAttach_
    (Int height, Int width, const Ring* buffer, Int leadingDimension) override;

    // Friend declarations
    // ===================
    template<typename S, Device D> friend class Matrix;
    template<typename S> friend class AbstractDistMatrix;
    template<typename S> friend class ElementalMatrix;
    template<typename S> friend class BlockMatrix;

    // For supporting duck typing
    // ==========================
    // The following are provided in order to aid duck-typing over
    // {Matrix, DistMatrix, etc.}.

    // This is equivalent to the trivial constructor in functionality
    // (though an error is thrown if 'grid' is not equal to 'Grid::Trivial()').
    explicit Matrix(El::Grid const& grid);

    // This is a no-op
    // (though an error is thrown if 'grid' is not equal to 'Grid::Trivial()').
    void SetGrid(El::Grid const& grid);

    // This always returns 'Grid::Trivial()'.
    El::Grid const& Grid() const;

    // This is a no-op
    // (though an error is thrown if 'colAlign' or 'rowAlign' is not zero).
    void Align(Int colAlign, Int rowAlign, bool constrain=true);

    // These always return 0.
    int ColAlign() const EL_NO_EXCEPT;
    int RowAlign() const EL_NO_EXCEPT;
};

template <typename T, Device D>
void SetSyncInfo(Matrix<T,D>&, SyncInfo<D> const&)
{}

template <typename T>
SyncInfo<Device::CPU> SyncInfoFromMatrix(Matrix<T,Device::CPU> const& mat)
{
    return SyncInfo<Device::CPU>{};
}

#ifdef HYDROGEN_HAVE_CUDA
// GPU version
template <typename Ring>
class Matrix<Ring, Device::GPU> : public AbstractMatrix<Ring>
{
public:
    // Constructors and destructors
    // ============================

    // Create a 0x0 matrix
    Matrix();

    // Create a matrix with the specified dimensions
    Matrix(Int height, Int width);

    // Create a matrix with the specified dimensions and leading dimension
    Matrix(Int height, Int width, Int leadingDimension);

    // Construct a matrix around an existing (possibly immutable) buffer
    Matrix(Int height, Int width, const Ring* buffer, Int leadingDimension);
    Matrix(Int height, Int width, Ring* buffer, Int leadingDimension);

    // Create a copy of a matrix
    Matrix(Matrix<Ring, Device::GPU> const& A);

    // Create a copy of a matrix from a CPU matrix
    Matrix(Matrix<Ring, Device::CPU> const& A);

    // Move the metadata from a given matrix
    Matrix(Matrix<Ring, Device::GPU>&& A) EL_NO_EXCEPT;

    // Destructor
    ~Matrix();

    // Copy assignment
    Matrix<Ring, Device::GPU>& operator=(
        Matrix<Ring, Device::GPU> const& A);

    // Assign by copying data from a CPU matrix
    Matrix<Ring, Device::GPU>& operator=(
        Matrix<Ring, Device::CPU> const& A);

    // Move assignment
    Matrix<Ring, Device::GPU>& operator=(Matrix<Ring, Device::GPU>&& A);

    DevicePtr<const Ring> Data() { return data_; }

    DevicePtr<Ring> Buffer() EL_NO_RELEASE_EXCEPT override;
    DevicePtr<Ring> Buffer(Int i, Int j) EL_NO_RELEASE_EXCEPT override;
    DevicePtr<const Ring> LockedBuffer() const EL_NO_EXCEPT override;
    DevicePtr<const Ring>
    LockedBuffer(Int i, Int j) const EL_NO_EXCEPT override;


    // Reconfigure around the given buffer, but do not assume ownership
    void Attach(Int height, Int width, Ring* buffer, Int leadingDimension) override;
    void LockedAttach(
        Int height, Int width, const Ring* buffer, Int leadingDimension) override;

    // Return a view
    Matrix<Ring, Device::GPU> operator()(Range<Int> I, Range<Int> J);

    // Return a locked view
    const Matrix<Ring, Device::GPU>
    operator()(Range<Int> I, Range<Int> J) const;

    // Advanced functions
    void SetMemoryMode(unsigned int mode) override;
    unsigned int MemoryMode() const EL_NO_EXCEPT override;

    // Single-entry manipulation
    // =========================

    // FIXME (trb 03/07/18): This is a phenomenally bad idea. This
    // access should be granted for kernels only, if we were to
    // offload these objects directly to device (also probably a bad
    // idea). As is, if the impls didn't just throw, this would
    // require a device sync after every call. No. Just no.
    Ring Get(Int i, Int j=0) const;
    Base<Ring> GetRealPart(Int i, Int j=0) const;
    Base<Ring> GetImagPart(Int i, Int j=0) const;

    void Set(Int i, Int j, Ring const& alpha);
    void Set(Entry<Ring> const& entry);

    void SetRealPart
    (Int i, Int j, Base<Ring> const& alpha);
    void SetImagPart
    (Int i, Int j, Base<Ring> const& alpha);

    void SetRealPart
    (Entry<Base<Ring>> const& entry);
    void SetImagPart
    (Entry<Base<Ring>> const& entry);

    void Update(Int i, Int j, Ring const& alpha);
    void Update(Entry<Ring> const& entry);

    void UpdateRealPart
    (Int i, Int j, Base<Ring> const& alpha);
    void UpdateImagPart
    (Int i, Int j, Base<Ring> const& alpha);

    void UpdateRealPart
    (Entry<Base<Ring>> const& entry);
    void UpdateImagPart
    (Entry<Base<Ring>> const& entry);

    void MakeReal(Int i, Int j);
    void Conjugate(Int i, Int j);

    // Return a reference to a single entry without error-checking
    // -----------------------------------------------------------
    inline Ring const& CRef(Int i, Int j=0) const override;
    inline Ring const& operator()(Int i, Int j=0) const override;

    inline Ring& Ref(Int i, Int j=0) override;
    inline Ring& operator()(Int i, Int j=0) override;

    cudaStream_t Stream() const;
    cudaEvent_t Event() const;

    void SetStream(cudaStream_t stream) EL_NO_EXCEPT;
    void SetEvent(cudaEvent_t event) EL_NO_EXCEPT;

private:

    Int do_get_memory_size_() const EL_NO_EXCEPT override;
    Device do_get_device_() const EL_NO_EXCEPT override;
    void do_empty_(bool freeMemory) override;
    void do_resize_() override;

    void Attach_(
        Int height, Int width, Ring* buffer, Int leadingDimension) override;
    void LockedAttach_(
        Int height, Int width,
        const Ring* buffer, Int leadingDimension) override;

    // Exchange metadata with another matrix
    // =====================================
    void ShallowSwap(Matrix<Ring, Device::GPU>& A);

    template<typename S, Device D> friend class Matrix;
    template<typename S> friend class AbstractDistMatrix;
    template<typename S> friend class ElementalMatrix;
    template<typename S> friend class BlockMatrix;

private:

    Memory<Ring,Device::GPU> memory_;

    DevicePtr<Ring> data_=nullptr;

    cudaStream_t stream_ = GPUManager::Stream();
    cudaEvent_t event_ = GPUManager::Event();

};// class Matrix<Ring,Device::GPU>

template <typename T>
SyncInfo<Device::GPU> SyncInfoFromMatrix(Matrix<T,Device::GPU> const& mat)
{
    return SyncInfo<Device::GPU>{mat.Stream(), mat.Event()};
}

template <typename T>
void SetSyncInfo(
    Matrix<T,Device::GPU>& mat, SyncInfo<Device::GPU> const& syncInfo)
{
    if (syncInfo.Stream() != nullptr)
        mat.SetStream(syncInfo.Stream());
    if (syncInfo.Event() != nullptr)
        mat.SetEvent(syncInfo.Event());
}
#endif // HYDROGEN_HAVE_CUDA

} // namespace El

#endif // ifndef EL_MATRIX_DECL_HPP
