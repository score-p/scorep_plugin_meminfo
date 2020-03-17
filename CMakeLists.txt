cmake_minimum_required(VERSION 3.15)
project(scorep_plugin_meminfo)

set(NITRO_POSITION_INDEPENDENT_CODE ON CACHE INTERNAL "")

find_package(Threads REQUIRED)

add_subdirectory(lib/scorep_plugin_cxx_wrapper)

add_library(meminfo_plugin MODULE src/plugin.cpp)

target_include_directories(meminfo_plugin PUBLIC include)

target_compile_features(meminfo_plugin PUBLIC cxx_std_17)

target_link_libraries(meminfo_plugin PUBLIC Scorep::scorep-plugin-cxx Threads::Threads)


install(
    TARGETS meminfo_plugin
    LIBRARY DESTINATION lib
)