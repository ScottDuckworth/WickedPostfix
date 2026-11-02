cmake_minimum_required(VERSION 3.22)

project(
  WickedWinchProtocol
  VERSION 0.1
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_library(WickedWinchProtocol
  cpp/include/WickedEvalStatus.h
  cpp/include/WickedPostfix.h
  cpp/include/WickedPath.h
  cpp/src/WickedEvalStatus.cc
  cpp/src/WickedPostfix.cc
  cpp/src/WickedPath.cc
  cpp/src/WickedUtil.hh
)

target_include_directories(WickedWinchProtocol PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include
)

if(NOT WICKEDWINCHPROTOCOL_TESTING_DISABLED)
  include(FetchContent)

  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(_Python3_EXECUTABLE_DEBUG "_Python3_EXECUTABLE_DEBUG-NOTFOUND" CACHE FILEPATH "Path to python debug executable")
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  include(GoogleTest)

  add_executable(WickedPostfix_test
    cpp/tests/WickedPostfix_test.cc
  )
  target_link_libraries(WickedPostfix_test
    GTest::gmock
    GTest::gtest_main
    WickedWinchProtocol
  )
  gtest_discover_tests(WickedPostfix_test)

  add_executable(WickedPath_test
    cpp/tests/WickedPath_test.cc
  )
  target_link_libraries(WickedPath_test
    GTest::gmock
    GTest::gtest_main
    WickedWinchProtocol
  )
  gtest_discover_tests(WickedPath_test)
endif()

# Embedded Flash Memory Footprint Benchmark Target
find_program(PYTHON_EXECUTABLE NAMES python3 python)
if(PYTHON_EXECUTABLE)
  find_program(ARM_NONE_EABI_GXX "arm-none-eabi-g++")
  find_program(ARM_NONE_EABI_SIZE "arm-none-eabi-size")
  find_program(XTENSA_ESP32S3_ELF_GXX "xtensa-esp32s3-elf-g++")
  find_program(XTENSA_ESP32S3_ELF_SIZE "xtensa-esp32s3-elf-size")

  set(BENCHMARK_FLASH_ARGS)
  if(ARM_NONE_EABI_GXX)
    list(APPEND BENCHMARK_FLASH_ARGS "--arm-cxx" "${ARM_NONE_EABI_GXX}")
  endif()
  if(ARM_NONE_EABI_SIZE)
    list(APPEND BENCHMARK_FLASH_ARGS "--arm-size" "${ARM_NONE_EABI_SIZE}")
  endif()
  if(XTENSA_ESP32S3_ELF_GXX)
    list(APPEND BENCHMARK_FLASH_ARGS "--xtensa-cxx" "${XTENSA_ESP32S3_ELF_GXX}")
  endif()
  if(XTENSA_ESP32S3_ELF_SIZE)
    list(APPEND BENCHMARK_FLASH_ARGS "--xtensa-size" "${XTENSA_ESP32S3_ELF_SIZE}")
  endif()
  if(STM32CUBE_FW_PATH)
    list(APPEND BENCHMARK_FLASH_ARGS "--stm32cube-fw" "${STM32CUBE_FW_PATH}")
  endif()

  add_custom_target(flash_benchmark
    COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/benchmark_flash.py"
            "--protocol-root" "${CMAKE_CURRENT_SOURCE_DIR}"
            ${BENCHMARK_FLASH_ARGS}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Running embedded flash memory footprint benchmarks across target profiles..."
    USES_TERMINAL
  )
endif()
