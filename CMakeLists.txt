cmake_minimum_required(VERSION 3.16)

project(benchmark
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_CXX_FLAGS "-march=native -mtune=native")

# ------------------------------------------------------------------------------

add_executable(benchmark
    main.cpp
)

# ------------------------------------------------------------------------------
# nanobench header

target_include_directories(benchmark
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nanobench/include/
)

# ------------------------------------------------------------------------------

include(GNUInstallDirs)
install(TARGETS benchmark
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
