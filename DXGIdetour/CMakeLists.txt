cmake_minimum_required(VERSION 3.20)

project(DXGIdetour)

add_library(dxgidetour SHARED main.cpp main.def)

target_link_libraries(dxgidetour PRIVATE detours DXGI)

if (NOT DEFINED DETOURS_PATH)
  message(FATAL_ERROR "Plase provide a path to detours as DETOURS_PATH!")
endif()

target_include_directories(dxgidetour PRIVATE ${DETOURS_PATH}/include)
target_link_directories(dxgidetour PRIVATE ${DETOURS_PATH}/lib.x64)

target_compile_features(dxgidetour PRIVATE cxx_std_17)