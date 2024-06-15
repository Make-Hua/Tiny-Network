#pragma once

/*
    从 noncopyable 类继承后的类，派生类对象可以正常使用析构构造，
    但是无法进行拷贝构造和拷贝赋值操作
*/
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;

};