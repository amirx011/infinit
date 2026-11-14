cmake_minimum_required(VERSION 3.16)
project(odecore VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ODE_SOURCES
    src/expr.cpp
    src/parse.cpp
    src/algebra.cpp
    src/poly.cpp
    src/calculus.cpp
    src/astdiff.cpp
    src/quadrature.cpp
    src/ode.cpp
    src/numeric.cpp
    src/numeric.c
    src/graph.cpp
    src/api.cpp
    src/calculus_api.cpp
    src/calculus_apps.cpp
    src/calculus_more.cpp
    src/limits.cpp
    src/topics.cpp
    src/series_solutions.cpp
    src/linear_systems.cpp
    src/dynamical_systems.cpp
    src/bvp.cpp
    src/special_functions.cpp
    src/special_series.cpp
    src/pde_series.cpp
    src/calculus_topics.cpp
)

option(ODE_BUILD_TESTS "Build native test executables" ON)
option(ODE_BUILD_STATIC "Build the static library" ON)

if(ODE_BUILD_STATIC)
  add_library(odecore STATIC ${ODE_SOURCES})
  target_include_directories(odecore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

add_library(odecore_shared SHARED ${ODE_SOURCES})
set_target_properties(odecore_shared PROPERTIES OUTPUT_NAME odecore)
target_include_directories(odecore_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(odecore_shared PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

if (UNIX)
    if(ODE_BUILD_STATIC)
        target_link_libraries(odecore PUBLIC m)
    endif()
    target_link_libraries(odecore_shared PUBLIC m)
endif()

if(ANDROID)
    # Android 15+ devices may use 16 KiB memory pages.
    target_link_options(odecore_shared PRIVATE "-Wl,-z,max-page-size=16384")
endif()

if(ODE_BUILD_TESTS)
  add_executable(odetest test/test_main.cpp)
  target_link_libraries(odetest PRIVATE odecore)

  add_executable(odetest_ode test/test_ode.cpp)
  target_link_libraries(odetest_ode PRIVATE odecore)

  add_executable(odetest_regression test/test_regression.cpp)
  target_link_libraries(odetest_regression PRIVATE odecore)

  add_executable(odetest_calculus test/test_calculus.cpp)
  target_link_libraries(odetest_calculus PRIVATE odecore)

  add_executable(odetest_limits test/test_limits.cpp)
  target_link_libraries(odetest_limits PRIVATE odecore)

  add_executable(odetest_calculus_apps test/test_calculus_apps.cpp)
  target_link_libraries(odetest_calculus_apps PRIVATE odecore)

  add_executable(odetest_calculus_more test/test_calculus_more.cpp)
  target_link_libraries(odetest_calculus_more PRIVATE odecore)

  add_executable(odetest_pde test/test_pde.cpp)
  target_link_libraries(odetest_pde PRIVATE odecore)

  add_executable(odetest_series test/test_series.cpp)
  target_link_libraries(odetest_series PRIVATE odecore)

  add_executable(odetest_linear test/test_linear.cpp)
  target_link_libraries(odetest_linear PRIVATE odecore)

  add_executable(odetest_dynamical test/test_dynamical.cpp)
  target_link_libraries(odetest_dynamical PRIVATE odecore)

  add_executable(odetest_bvp test/test_bvp.cpp)
  target_link_libraries(odetest_bvp PRIVATE odecore)

  add_executable(odetest_special test/test_special.cpp)
  target_link_libraries(odetest_special PRIVATE odecore)

  add_executable(odetest_pde_series test/test_pde_series.cpp)
  target_link_libraries(odetest_pde_series PRIVATE odecore)

  add_executable(odetest_calculus_topics test/test_calculus_topics.cpp)
  target_link_libraries(odetest_calculus_topics PRIVATE odecore)

  enable_testing()
  add_test(NAME algebra COMMAND odetest)
  add_test(NAME ode COMMAND odetest_ode)
  add_test(NAME regression COMMAND odetest_regression)
  add_test(NAME calculus COMMAND odetest_calculus)
  add_test(NAME limits COMMAND odetest_limits)
  add_test(NAME calculus_apps COMMAND odetest_calculus_apps)
  add_test(NAME calculus_more COMMAND odetest_calculus_more)
  add_test(NAME pde COMMAND odetest_pde)
  add_test(NAME series COMMAND odetest_series)
  add_test(NAME linear COMMAND odetest_linear)
  add_test(NAME dynamical COMMAND odetest_dynamical)
  add_test(NAME bvp COMMAND odetest_bvp)
  add_test(NAME special COMMAND odetest_special)
  add_test(NAME pde_series COMMAND odetest_pde_series)
  add_test(NAME calculus_topics COMMAND odetest_calculus_topics)
endif()
