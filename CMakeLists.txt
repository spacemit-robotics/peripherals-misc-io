cmake_minimum_required(VERSION 3.10)
project(misc_io C)

# Standalone default (and compatibility with exported targets).
if(NOT DEFINED SROBOTIS_EXPORT_SET)
    set(SROBOTIS_EXPORT_SET "ComponentsTargets" CACHE STRING "Export set name used by exported targets")
endif()

add_library(misc_io SHARED src/misc_io.c)

target_include_directories(misc_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Find libgpiod
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPIOD libgpiod)

if(NOT LIBGPIOD_FOUND)
    message(WARNING "libgpiod not found. Please install it: sudo apt-get install libgpiod-dev")
endif()

# 获取版本号并传递给编译器
if(LIBGPIOD_VERSION VERSION_LESS "2.0.0")
    add_definitions(-DLIBGPIOD_V1)
else()
    add_definitions(-DLIBGPIOD_V2)
endif()

target_include_directories(misc_io PRIVATE ${LIBGPIOD_INCLUDE_DIRS})
target_link_libraries(misc_io PRIVATE ${LIBGPIOD_LIBRARIES} pthread)
target_compile_options(misc_io PRIVATE ${LIBGPIOD_CFLAGS_OTHER})

# Install rules (so ROS2 nodes can link via PREFIX)
install(TARGETS misc_io
    EXPORT ${SROBOTIS_EXPORT_SET}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(FILES include/misc_io.h DESTINATION include)

# ---- tests ----
option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
    add_executable(test_misc_io
        test/test_misc_io.c
    )
    target_link_libraries(test_misc_io PRIVATE misc_io)
    set_target_properties(test_misc_io PROPERTIES BUILD_RPATH "$ORIGIN/../lib")
    install(TARGETS test_misc_io RUNTIME DESTINATION bin)
endif()

