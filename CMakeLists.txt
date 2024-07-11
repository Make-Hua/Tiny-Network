cmake_minimum_required(VERSION 3.0)

project(TinyNetwork)

# 添加头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include/base)
include_directories(${PROJECT_SOURCE_DIR}/include/logger)
include_directories(${PROJECT_SOURCE_DIR}/include/net)
include_directories(${PROJECT_SOURCE_DIR}/include/net/poller)
include_directories(${PROJECT_SOURCE_DIR}/include/net/timer)
include_directories(${PROJECT_SOURCE_DIR}/include/pool/ThreadPool)
include_directories(${PROJECT_SOURCE_DIR}/include/http)

# include_directories(${PROJECT_SOURCE_DIR}/pool/MySqlPool)
# include_directories(${PROJECT_SOURCE_DIR}/pool/MemoryPool)


# 添加源文件搜索路径
aux_source_directory(${PROJECT_SOURCE_DIR}/src/base SRC_BASE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net SRC_NET)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net/poller SRC_POLLER)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net/timer SRC_TIMER)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/http SRC_HTTP)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/logger SRC_LOG)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/pool/thread SRC_THREAD)

# aux_source_directory(${PROJECT_SOURCE_DIR}/src/pool/memory SRC_MEMORY)
# aux_source_directory(${PROJECT_SOURCE_DIR}/src/pool/mysql SRC_MYSQL)


# 由于希望最终把 TinyNetwork 编译为 so 动态库
# 设置动态库路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置调试信息 启动 c++11 语言标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 编译生成动态库
add_library(TinyNetwork SHARED 
            ${SRC_BASE}
            ${SRC_NET}
            ${SRC_POLLER}
            ${SRC_TIMER}
            ${SRC_LOG}
            ${SRC_THREAD}
            ${SRC_HTTP}
        )