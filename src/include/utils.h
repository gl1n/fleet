#pragma once

#include <error.h>
#include <cstdint>
#include <string>

namespace fleet {
std::string get_error_string();

/**
 * @brief get thread identification
 */
pid_t get_thread_id();

std::string get_thread_name();

/**
 * @brief 获取caller的函数调用栈
 * @param size 获取的最大函数栈层数
 * @param skip 跳过栈顶skip个函数不打印
 */
std::string backtrace_to_string(int size = 64, int skip = 2, const std::string &prefix = "");

uint64_t get_elapsed_ms();
}  // namespace fleet