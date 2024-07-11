#!/bin/bash  
  
# 设置错误处理  
set -e  
  
# 定义目标目录  
TARGET_DIR="/usr/include/TinyNetwork"  
  
# 确保目标目录存在  
mkdir -p "$TARGET_DIR"  
  
# 定义要复制的头文件目录  
HEADERS_DIRS=(./include/base 
              ./include/logger 
              ./include/http 
              ./include/net 
              ./include/net/poller 
              ./include/net/timer 
              ./include/pool/ThreadPool)  
  
# 遍历每个目录，并复制其中的.h文件到目标目录  
for dir in "${HEADERS_DIRS[@]}"; do  
    # 使用通配符和nullglob选项来安全地匹配文件  
    shopt -s nullglob  
    for header in "$dir"/*.h; do  
        cp "$header" "$TARGET_DIR"  
    done  
    shopt -u nullglob  
done  
  
# 拷贝动态库到 /usr/lib 下
cp `pwd`/lib/libTinyNetwork.so /usr/lib

# 更新动态库缓存
ldconfig