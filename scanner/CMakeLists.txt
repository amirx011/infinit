cmake_minimum_required(VERSION 3.16)
project(infinit_scanner VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SCANNER_SOURCES
    src/image.cpp
    src/ink.cpp
    src/inkproto.cpp
    src/trace.cpp
    src/cloud.cpp
    src/preprocess.cpp
    src/segment.cpp
    src/glyphs.cpp
    src/classify.cpp
    src/reader.cpp
    src/repair.cpp
    src/structure.cpp
    src/scanner_api.cpp
    src/ndk_compat.cpp
)

# The scanner uses OpenCV's core and imgproc only. On Android, point OpenCV_DIR
# at third_party/opencv-mobile/sdk/native/jni (see scripts/fetch_opencv_mobile.sh).
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

add_library(infinit_scanner STATIC ${SCANNER_SOURCES})
target_include_directories(infinit_scanner PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS})
target_link_libraries(infinit_scanner PUBLIC ${OpenCV_LIBS})

# stb_image is third-party and noisy under -Wall; keep the warning budget for
# our own code.
if(NOT MSVC)
  set_source_files_properties(src/image.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()

add_executable(scan_debug tools/scan_debug.cpp)
target_link_libraries(scan_debug PRIVATE infinit_scanner)
target_compile_definitions(scan_debug PRIVATE
    SCANNER_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

option(SCANNER_BUILD_TESTS "Build scanner test executables" ON)

if(SCANNER_BUILD_TESTS)
  enable_testing()

  add_executable(scanner_test tests/test_segment.cpp)
  target_link_libraries(scanner_test PRIVATE infinit_scanner)
  add_test(NAME scanner_segment COMMAND scanner_test)

  add_executable(scanner_structure_test tests/test_structure.cpp)
  target_link_libraries(scanner_structure_test PRIVATE infinit_scanner)
  add_test(NAME scanner_structure COMMAND scanner_structure_test)

  add_executable(scanner_pipeline_test tests/test_pipeline.cpp)
  target_link_libraries(scanner_pipeline_test PRIVATE infinit_scanner)
  target_compile_definitions(scanner_pipeline_test PRIVATE
      SCANNER_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
      SCANNER_SAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples")
  add_test(NAME scanner_pipeline COMMAND scanner_pipeline_test)

  add_executable(scanner_classify_test tests/test_classify.cpp)
  target_link_libraries(scanner_classify_test PRIVATE infinit_scanner)
  target_compile_definitions(scanner_classify_test PRIVATE
      SCANNER_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
      SCANNER_SAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/samples")
  add_test(NAME scanner_classify COMMAND scanner_classify_test)

  add_executable(scanner_repair_test tests/test_repair.cpp)
  target_link_libraries(scanner_repair_test PRIVATE infinit_scanner)
  add_test(NAME scanner_repair COMMAND scanner_repair_test)

  add_executable(scanner_trace_test tests/test_trace.cpp)
  target_link_libraries(scanner_trace_test PRIVATE infinit_scanner)
  add_test(NAME scanner_trace COMMAND scanner_trace_test)

  add_executable(scanner_ink_test tests/test_ink.cpp)
  target_link_libraries(scanner_ink_test PRIVATE infinit_scanner)
  target_compile_definitions(scanner_ink_test PRIVATE
      SCANNER_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
  add_test(NAME scanner_ink COMMAND scanner_ink_test)

  if(OpenCV_FOUND)
    message(STATUS "OpenCV ${OpenCV_VERSION} found: building cross-checks")
    add_executable(scanner_crosscheck tests/test_opencv_crosscheck.cpp)
    target_link_libraries(scanner_crosscheck PRIVATE infinit_scanner ${OpenCV_LIBS})
    target_include_directories(scanner_crosscheck PRIVATE ${OpenCV_INCLUDE_DIRS})
    add_test(NAME scanner_crosscheck COMMAND scanner_crosscheck)
  else()
    message(STATUS "OpenCV not found: cross-checks skipped")
  endif()
endif()
