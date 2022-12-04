#pragma once

#include <error.h>
#include <string>

namespace fleet {
std::string get_error_string();

/**
 * @brief get thread identification
 */
pid_t get_thread_id();

std::string get_thread_name();
}  // namespace fleet