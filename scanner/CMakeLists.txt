cmake_minimum_required(VERSION 3.16)
project(infinit_scanner VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SCANNER_SOURCES
    src/image.cpp
    src/preprocess.cpp
    src/segment.cpp
    src/glyphs.cpp
    src/classify.cpp
    src/reader.cpp
    src/structure.cpp
    src/scanner_api.cpp
)

add_library(infinit_scanner STATIC ${SCANNER_SOURCES})
target_include_directories(infinit_scanner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

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

# OpenCV is optional: the scanner itself is dependency-free so the same code
# compiles for desktop and Android. When it is present we cross-check our
# hand-written labelling against cv::connectedComponents, and later stages may
# use it for tuning.
find_package(OpenCV QUIET COMPONENTS core imgproc)

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
