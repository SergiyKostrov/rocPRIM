# MIT License
#
# Copyright (c) 2017-2022 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

option(BENCHMARK_CONFIG_TUNING "Benchmark device-level functions using various configs" OFF)
include(../cmake/ConfigAutotune.cmake)
include(ConfigAutotuneSettings.cmake)

if(BENCHMARK_CONFIG_TUNING)
  add_custom_target("benchmark_config_tuning")
endif()

function(add_rocprim_benchmark BENCHMARK_SOURCE)
  get_filename_component(BENCHMARK_TARGET ${BENCHMARK_SOURCE} NAME_WE)

  if(BENCHMARK_CONFIG_TUNING)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${BENCHMARK_TARGET}.parallel.cpp.in")
      message(STATUS "found ${BENCHMARK_TARGET}.parallel.cpp.in file, compiling in parallel.")
      read_config_autotune_settings(${BENCHMARK_TARGET} list_across_names list_across output_pattern_suffix)
      #make sure that variables are not empty, i.e. there actually is an entry for that benchmark in benchmark/ConfigAutotuneSettings.cmake
      if(list_across_names)
        add_executable(${BENCHMARK_TARGET} ${BENCHMARK_SOURCE})
        target_compile_definitions(${BENCHMARK_TARGET} PRIVATE BENCHMARK_CONFIG_TUNING)
        add_matrix(TARGET ${BENCHMARK_TARGET}
                SHARDS 1
                CURRENT_SHARD 0
                INPUT "${BENCHMARK_TARGET}.parallel.cpp.in"
                OUTPUT_PATTERN "${BENCHMARK_TARGET}_${output_pattern_suffix}"
                NAMES ${list_across_names}
                LISTS ${list_across})
        add_dependencies(benchmark_config_tuning ${BENCHMARK_TARGET})
      else()
        message(WARNING "No config-tuning entry in benchmark/ConfigAutotuneSettings.cmake for ${BENCHMARK_TARGET}!")
        return()
      endif()
    else()
      #do nothing if BENCHMARK_CONFIG_TUNING is ON but no ${BENCHMARK_TARGET}.parallel.cpp.in exists
      return()
    endif()
  else()
    add_executable(${BENCHMARK_TARGET} ${BENCHMARK_SOURCE})
  endif()

  target_link_libraries(${BENCHMARK_TARGET}
    PRIVATE
      rocprim
      benchmark::benchmark
  )
  if(NOT USE_HIP_CPU)
    target_link_libraries(${BENCHMARK_TARGET}
      PRIVATE
        rocprim_hip
    )
  else()
    target_link_libraries(${BENCHMARK_TARGET}
      PRIVATE
        Threads::Threads
        hip_cpu_rt::hip_cpu_rt
    )
    if(STL_DEPENDS_ON_TBB)
      target_link_libraries(${BENCHMARK_TARGET}
        PRIVATE
          TBB::tbb
      )
    endif()
  endif()

  target_compile_options(${BENCHMARK_TARGET}
    PRIVATE
      $<$<CXX_COMPILER_ID:MSVC>:
        /bigobj # number of sections exceeded object file format limit: compile with /bigobj
      >
  )

  set_target_properties(${BENCHMARK_TARGET}
    PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmark"
  )

  rocm_install(TARGETS ${BENCHMARK_TARGET} COMPONENT benchmarks)
  if (WIN32 AND NOT DEFINED DLLS_COPIED)
    set(DLLS_COPIED "YES")
    set(DLLS_COPIED ${DLLS_COPIED} PARENT_SCOPE)
    # for now adding in all .dll as dependency chain is not cmake based on win32
    file( GLOB third_party_dlls
    LIST_DIRECTORIES ON
    CONFIGURE_DEPENDS
    ${HIP_DIR}/bin/*.dll
    ${CMAKE_SOURCE_DIR}/rtest.*
    )
    foreach( file_i ${third_party_dlls})
      add_custom_command( TARGET ${BENCHMARK_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${file_i} ${PROJECT_BINARY_DIR}/benchmark )
    endforeach( file_i )
  endif()
endfunction()

# ****************************************************************************
# Benchmarks
# ****************************************************************************

add_rocprim_benchmark(benchmark_block_adjacent_difference.cpp)
add_rocprim_benchmark(benchmark_block_discontinuity.cpp)
add_rocprim_benchmark(benchmark_block_exchange.cpp)
add_rocprim_benchmark(benchmark_block_histogram.cpp)
add_rocprim_benchmark(benchmark_block_radix_sort.cpp)
add_rocprim_benchmark(benchmark_block_radix_rank.cpp)
add_rocprim_benchmark(benchmark_block_reduce.cpp)
add_rocprim_benchmark(benchmark_block_scan.cpp)
add_rocprim_benchmark(benchmark_block_sort.cpp)
add_rocprim_benchmark(benchmark_config_dispatch.cpp)
add_rocprim_benchmark(benchmark_device_adjacent_difference.cpp)
add_rocprim_benchmark(benchmark_device_binary_search.cpp)
add_rocprim_benchmark(benchmark_device_histogram.cpp)
add_rocprim_benchmark(benchmark_device_merge.cpp)
add_rocprim_benchmark(benchmark_device_merge_sort.cpp)
add_rocprim_benchmark(benchmark_device_merge_sort_block_sort.cpp)
add_rocprim_benchmark(benchmark_device_merge_sort_block_merge.cpp)
add_rocprim_benchmark(benchmark_device_partition.cpp)
add_rocprim_benchmark(benchmark_device_radix_sort.cpp)
add_rocprim_benchmark(benchmark_device_radix_sort_block_sort.cpp)
add_rocprim_benchmark(benchmark_device_radix_sort_onesweep.cpp)
add_rocprim_benchmark(benchmark_device_reduce_by_key.cpp)
add_rocprim_benchmark(benchmark_device_reduce.cpp)
add_rocprim_benchmark(benchmark_device_run_length_encode.cpp)
add_rocprim_benchmark(benchmark_device_scan.cpp)
add_rocprim_benchmark(benchmark_device_select.cpp)
add_rocprim_benchmark(benchmark_device_segmented_radix_sort.cpp)
add_rocprim_benchmark(benchmark_device_segmented_reduce.cpp)
add_rocprim_benchmark(benchmark_device_transform.cpp)
add_rocprim_benchmark(benchmark_warp_exchange.cpp)
add_rocprim_benchmark(benchmark_warp_reduce.cpp)
add_rocprim_benchmark(benchmark_warp_scan.cpp)
add_rocprim_benchmark(benchmark_warp_sort.cpp)
add_rocprim_benchmark(benchmark_device_memory.cpp)
