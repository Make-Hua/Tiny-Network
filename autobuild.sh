# !/bin/bash

set -e

# 如果没有则 mkdir /build 目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

# 清空 build 目录
rm -rf `pwd`/build/*

# 切换到 /build 目录下生成 makefile 文件并且执行
cd `pwd`/build && cmake .. && make



