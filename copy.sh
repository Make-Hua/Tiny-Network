# 设置错误处理
set -e

# 创建目标 include 目录如果不存在
if [ ! -d /usr/include/TinyNetwork ]; then
    mkdir /usr/include/TinyNetwork
fi

# 将头文件拷贝到 /usr/include/TinyNetwork 下
for header in `ls ./include/base/*.h`
do
    cp $header /usr/include/TinyNetwork
done

for header in `ls ./include/logger/*.h`
do
    cp $header /usr/include/TinyNetwork
done

for header in `ls ./include/net/*.h`
do
    cp $header /usr/include/TinyNetwork
done

for header in `ls ./include/net/poller/*.h`
do
    cp $header /usr/include/TinyNetwork
done

for header in `ls ./include/pool/ThreadPool/*.h`
do
    cp $header /usr/include/TinyNetwork
done

# 拷贝动态库到 /usr/lib 下
cp `pwd`/lib/libTinyNetwork.so /usr/lib

# 更新动态库缓存
ldconfig