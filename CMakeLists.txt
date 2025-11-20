cmake_minimum_required(VERSION 3.16)

project(TestInplaceAny LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(TestInplaceAny main.cpp
    InplaceAnyCopy.h)

include(GNUInstallDirs)
install(TARGETS TestInplaceAny
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
