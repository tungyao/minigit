cmake_minimum_required(VERSION 3.16)

project(MiniGit
        VERSION 1.0.0
        DESCRIPTION "A tiny git-like tool supporting init, add, commit, push, pull, reset"
        LANGUAGES C CXX
)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 查找 clang 和 gcc
find_program(CLANG_C NAMES clang)
find_program(CLANG_CXX NAMES clang++)

find_program(GCC_C NAMES gcc)
find_program(GCC_CXX NAMES g++)

# 优先使用 clang
if(!WIN32)
    if (CLANG_C AND CLANG_CXX)
        message(STATUS "Using Clang: ${CLANG_C}, ${CLANG_CXX}")
        set(CMAKE_C_COMPILER "${CLANG_C}" CACHE STRING "" FORCE)
        set(CMAKE_CXX_COMPILER "${CLANG_CXX}" CACHE STRING "" FORCE)
    elseif (GCC_C AND GCC_CXX)
        message(STATUS "Using GCC: ${GCC_C}, ${GCC_CXX}")
        set(CMAKE_C_COMPILER "${GCC_C}" CACHE STRING "" FORCE)
        set(CMAKE_CXX_COMPILER "${GCC_CXX}" CACHE STRING "" FORCE)
    else ()
        message(FATAL_ERROR "Neither clang nor gcc found!")
    endif ()
endif ()
# 设置默认构建类型为Release（如果未指定）
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
# 找到源文件
file(GLOB_RECURSE ALL_SRC
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.c"
        "${CMAKE_SOURCE_DIR}/include/*.hpp"
        "${CMAKE_SOURCE_DIR}/include/*.h"
)

# 自定义 target: format
add_custom_target(format
        COMMAND clang-format -i ${ALL_SRC}
        COMMENT "Formatting all source files..."
        message(STATUS "Formatting all source files...")
)
# 编译器特定设置
if (MSVC)
    # Windows MSVC编译器设置
    add_compile_options(/W3)
    add_compile_options(/utf-8)  # 支持UTF-8编码
    # 为Release模式添加优化选项
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else ()
    # GCC/Clang编译器设置（Linux/macOS）
    add_compile_options(-Wall -Wextra)
    # 为Release模式添加优化选项
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
endif ()

# 检查并添加必要的系统库
if (WIN32)
    # Windows特定设置
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7及以上
    # Windows网络库
    set(NETWORK_LIBS ws2_32 wsock32)
else ()
    # Linux/Unix特定设置
    find_package(Threads REQUIRED)
    # Linux网络库通常不需要额外链接
    set(NETWORK_LIBS)
endif ()

# 查找OpenSSL库（用于加密）
#find_package(OpenSSL)
#if(OpenSSL_FOUND)
#    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
#    set(CRYPTO_LIBS OpenSSL::SSL OpenSSL::Crypto)
#    add_definitions(-DHAVE_OPENSSL)
#else()
#    message(STATUS "OpenSSL not found, using built-in crypto implementations")
#    set(CRYPTO_LIBS)
#endif()

# 收集源文件
set(SOURCES
        src/aes.c
        src/main.cpp
        src/sha256.cpp
        src/filesystem_utils.cpp
        src/index.cpp
        src/objects.cpp
        src/commit.cpp
        src/commands.cpp
        src/commands_basic.cpp
        src/commands_history.cpp
        src/commands_remote.cpp
        src/server.cpp
        src/crypto.cpp
        src/protocol.cpp
        src/client.cpp
        src/progress.cpp
        src/compression.cpp
        src/mignore.h
        src/mignore.cpp
)

# lz4
set(LZ4
        include/lz4.c
        include/lz4.h
)
# 收集头文件
set(HEADERS
        src/aes.h
        src/aes.hpp
        src/progress.h
        src/common.h
        src/sha256.h
        src/filesystem_utils.h
        src/index.h
        src/objects.h
        src/commit.h
        src/commands.h
        src/commands_basic.h
        src/commands_history.h
        src/commands_remote.h
        src/server.h
        src/crypto.h
        src/protocol.h
        src/client.h
        src/compression.h
)

# 添加可执行文件
add_executable(minigit ${LZ4} ${SOURCES} ${HEADERS})
add_dependencies(minigit format)
# 包含头文件目录
target_include_directories(minigit PRIVATE src include)

# 设置输出目录
set_target_properties(minigit PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 链接必要的库
if (WIN32)
    # Windows下需要的库
    target_link_libraries(minigit ${NETWORK_LIBS})
else ()
    # Linux下需要的库
    target_link_libraries(minigit Threads::Threads ${NETWORK_LIBS})
endif ()

# 安装规则（可选）
install(TARGETS minigit
        RUNTIME DESTINATION bin
)

# 添加一些有用的信息输出
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Source files: ${SOURCES}")
