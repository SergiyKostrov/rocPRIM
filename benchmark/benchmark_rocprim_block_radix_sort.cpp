// MIT License
//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <iostream>
#include <chrono>
#include <vector>
#include <locale>
#include <codecvt>
#include <string>

// Google Benchmark
#include "benchmark/benchmark.h"

// CmdParser
#include "cmdparser.hpp"

#include "benchmark_utils.hpp"

// HC API
#include <hcc/hc.hpp>
// HIP API
#include <hip/hip_runtime.h>
#include <hip/hip_hcc.h>

// rocPRIM
#include <block/block_radix_sort.hpp>

#define HIP_CHECK(condition)         \
  {                                  \
    hipError_t error = condition;    \
    if(error != hipSuccess){         \
        std::cout << "HIP error: " << error << " line: " << __LINE__ << std::endl; \
        exit(error); \
    } \
  }

#ifndef DEFAULT_N
const size_t DEFAULT_N = 1024 * 1024 * 128;
#endif

namespace rp = rocprim;

template<
    class T,
    unsigned int BlockSize,
    unsigned int ItemsPerThread
>
__global__
void block_radix_sort_kernel(const T * input, T * output)
{
    const unsigned int i = hipBlockIdx_x * hipBlockDim_x + hipThreadIdx_x;

    T keys[ItemsPerThread];
    for(unsigned int k = 0; k < ItemsPerThread; k++)
    {
        keys[k] = input[i * ItemsPerThread + k];
    }

    rp::block_radix_sort<T, BlockSize, ItemsPerThread> sort;
    sort.sort(keys);

    for(unsigned int k = 0; k < ItemsPerThread; k++)
    {
        output[i * ItemsPerThread + k] = keys[k];
    }
}

template<
    class T,
    unsigned int BlockSize,
    unsigned int ItemsPerThread
>
void benchmark_block_radix_sort(benchmark::State& state, hipStream_t stream, size_t N)
{
    constexpr auto items_per_block = BlockSize * ItemsPerThread;
    const auto size = items_per_block * ((N + items_per_block - 1)/items_per_block);

    std::vector<T> input;
    if(std::is_floating_point<T>::value)
    {
        input = get_random_data<T>(size, (T)-1000, (T)+1000);
    }
    else
    {
        input = get_random_data<T>(
            size,
            std::numeric_limits<T>::min(),
            std::numeric_limits<T>::max()
        );
    }
    T * d_input;
    T * d_output;
    HIP_CHECK(hipMalloc(&d_input, size * sizeof(T)));
    HIP_CHECK(hipMalloc(&d_output, size * sizeof(T)));
    HIP_CHECK(
        hipMemcpy(
            d_input, input.data(),
            size * sizeof(T),
            hipMemcpyHostToDevice
        )
    );
    HIP_CHECK(hipDeviceSynchronize());

    for(auto _ : state)
    {
        auto start = std::chrono::high_resolution_clock::now();

        hipLaunchKernelGGL(
            HIP_KERNEL_NAME(block_radix_sort_kernel<T, BlockSize, ItemsPerThread>),
            dim3(size/items_per_block), dim3(BlockSize), 0, stream,
            d_input, d_output
        );
        HIP_CHECK(hipPeekAtLastError());
        HIP_CHECK(hipDeviceSynchronize());

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
    state.SetBytesProcessed(state.iterations() * size * sizeof(T));
    state.SetItemsProcessed(state.iterations() * size);

    HIP_CHECK(hipFree(d_input));
    HIP_CHECK(hipFree(d_output));
}

int main(int argc, char *argv[])
{
    cli::Parser parser(argc, argv);
    parser.set_optional<size_t>("size", "size", DEFAULT_N, "number of values");
    parser.set_optional<int>("trials", "trials", -1, "number of iterations");
    parser.run_and_exit_if_error();

    // Parse argv
    benchmark::Initialize(&argc, argv);
    const size_t size = parser.get<size_t>("size");
    const int trials = parser.get<int>("trials");

    // HIP
    hipStream_t stream = 0; // default
    hipDeviceProp_t devProp;
    int device_id = 0;
    HIP_CHECK(hipGetDevice(&device_id));
    HIP_CHECK(hipGetDeviceProperties(&devProp, device_id));
    std::cout << "[HIP] Device name: " << devProp.name << std::endl;

    // HC
    hc::accelerator_view* acc_view;
    HIP_CHECK(hipHccGetAcceleratorView(stream, &acc_view));
    auto acc = acc_view->get_accelerator();
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    std::cout << "[HC] Device name: " << conv.to_bytes(acc.get_description()) << std::endl;

    // Add benchmarks
    std::vector<benchmark::internal::Benchmark*> benchmarks =
    {
        benchmark::RegisterBenchmark(
            "block_radix_sort<unsigned int, 256, 1>",
            [=](benchmark::State& state) { benchmark_block_radix_sort<unsigned int, 256, 1>(state, stream, size); }
        ),
        benchmark::RegisterBenchmark(
            "block_radix_sort<double, 256, 1>",
            [=](benchmark::State& state) { benchmark_block_radix_sort<double, 256, 1>(state, stream, size); }
        ),
        benchmark::RegisterBenchmark(
            "block_radix_sort<unsigned int, 512, 4>",
            [=](benchmark::State& state) { benchmark_block_radix_sort<unsigned int, 512, 4>(state, stream, size); }
        ),
        benchmark::RegisterBenchmark(
            "block_radix_sort<double, 256, 8>",
            [=](benchmark::State& state) { benchmark_block_radix_sort<double, 256, 8>(state, stream, size); }
        ),
        benchmark::RegisterBenchmark(
            "block_radix_sort<int, 128, 16>",
            [=](benchmark::State& state) { benchmark_block_radix_sort<int, 128, 16>(state, stream, size); }
        ),
        benchmark::RegisterBenchmark(
            "block_radix_sort<short, 400, 5>",
            [=](benchmark::State& state) { benchmark_block_radix_sort<short, 400, 5>(state, stream, size); }
        ),
        benchmark::RegisterBenchmark(
            "block_radix_sort<long long, 300, 3>",
            [=](benchmark::State& state) { benchmark_block_radix_sort<long long, 300, 3>(state, stream, size); }
        ),
    };

    // Use manual timing
    for(auto& b : benchmarks)
    {
        b->UseManualTime();
        b->Unit(benchmark::kMillisecond);
    }

    // Force number of iterations
    if(trials > 0)
    {
        for(auto& b : benchmarks)
        {
            b->Iterations(trials);
        }
    }

    // Run benchmarks
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
