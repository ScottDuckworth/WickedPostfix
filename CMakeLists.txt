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
  add_executable(WickedPostfix_Slim_test
    cpp/tests/WickedPostfix_Slim_test.cc
    cpp/src/WickedPostfix.cc
    cpp/src/WickedEvalStatus.cc
  )
  target_compile_definitions(WickedPostfix_Slim_test PRIVATE WICKED_POSTFIX_SLIM)
  target_link_libraries(WickedPostfix_Slim_test
    GTest::gmock
    GTest::gtest_main
  )
  gtest_discover_tests(WickedPostfix_Slim_test)
endif()
