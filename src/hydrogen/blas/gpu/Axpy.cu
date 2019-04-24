#include <hydrogen/blas/gpu/Axpy.hpp>
#include <hydrogen/device/gpu/CUDA.hpp>

namespace
{

template <int TILE_SIZE, int BLK_COLS, typename T, typename SizeT>
__global__ void axpy_2d_tiled_kernel(
    SizeT m, SizeT n, T alpha,
    T const* A, SizeT row_stride_A, SizeT col_stride_A,
    T* B, SizeT row_stride_B, SizeT col_stride_B)
{
    auto row_start = blockIdx.x * TILE_SIZE + threadIdx.x;
    auto col_start = blockIdx.y * TILE_SIZE + threadIdx.y;

    auto idx_in = row_start*row_stride_A + col_start*col_stride_A;
    auto idx_out = row_start*row_stride_B + col_start*col_stride_B;

    if (row_start < m)
    {
        A += idx_in;
        B += idx_out;
        if (col_start + TILE_SIZE <= n)
        {
            #pragma unroll
            for (int ii = 0; ii < TILE_SIZE; ii += BLK_COLS)
                B[ii*col_stride_B] += alpha * A[ii*col_stride_A];
        }
        else
        {
            for (int ii = 0; ii < TILE_SIZE && col_start + ii < n; ii += BLK_COLS)
                B[ii*col_stride_B] += alpha * A[ii*col_stride_A];
        }
    }
}

}// namespace <anon>

namespace hydrogen
{

template <typename T, typename SizeT, typename>
void Axpy_GPU_impl(
    SizeT height, SizeT width,
    T alpha,
    T const* X, SizeT colStrideX, SizeT rowStrideX,
    T* Y, SizeT colStrideY, SizeT rowStrideY,
    cudaStream_t stream)
{
    constexpr int TILE_SIZE = 32;
    constexpr int BLK_COLS = 8;

    // Short-circuit
    if (height <= 0 || width <= 0)
        return;

    dim3 blks((height + TILE_SIZE - 1) / TILE_SIZE,
              (width + TILE_SIZE - 1) / TILE_SIZE, 1);
    dim3 thds(TILE_SIZE, BLK_COLS, 1);
    void* args[] = {&height, &width, &alpha,
                    &X, &colStrideX, &rowStrideX,
                    &Y, &colStrideY, &rowStrideY};

    H_CHECK_CUDA(
        cudaLaunchKernel(
            (void const*)&axpy_2d_tiled_kernel<TILE_SIZE, BLK_COLS, T, SizeT>,
            blks, thds, args, 0, stream));
}

#define ETI(ScalarT, SizeT)                             \
    template void Axpy_GPU_impl(                        \
        SizeT, SizeT, ScalarT,                          \
        ScalarT const*, SizeT, SizeT,                   \
        ScalarT*, SizeT, SizeT, cudaStream_t)

#ifdef HYDROGEN_GPU_USE_FP16
ETI(gpu_half_type, int);
ETI(gpu_half_type, long);
ETI(gpu_half_type, unsigned);
ETI(gpu_half_type, size_t);
#endif

ETI(float, int);
ETI(float, long);
ETI(float, unsigned);
ETI(float, size_t);

ETI(double, int);
ETI(double, long);
ETI(double, unsigned);
ETI(double, size_t);

}// namespace hydrogen