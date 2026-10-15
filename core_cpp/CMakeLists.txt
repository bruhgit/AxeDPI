cmake_minimum_required(VERSION 3.16)
project(AxeDpiService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_library(AxeDPI.Service.DllComponents SHARED
    src/dll_main.cpp
    src/dpi_engine.cpp
    src/sni_parser.cpp
    src/doh_resolver.cpp
    src/sysproxy.cpp
)

target_compile_definitions(AxeDPI.Service.DllComponents PRIVATE AXEDPI_EXPORTS)

target_link_libraries(AxeDPI.Service.DllComponents PRIVATE
    ws2_32
    wininet
    winhttp
    iphlpapi
)

set_target_properties(AxeDPI.Service.DllComponents PROPERTIES
    PREFIX ""
    OUTPUT_NAME "AxeDPI.Service.DllComponents"
)
