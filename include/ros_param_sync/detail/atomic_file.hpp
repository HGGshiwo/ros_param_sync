#pragma once

#include <string>
#include <fstream>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

namespace ros_param_sync {
namespace detail {

/**
 * @brief Atomically writes string content to a file using temporary file + rename.
 * Prevents file corruption during unexpected power loss or crashes.
 */
inline bool atomic_write_file(const std::string &filepath, const std::string &content) {
  std::string tmp_path = filepath + ".tmp";
  {
    std::ofstream ofs(tmp_path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
      return false;
    }
    ofs << content;
    ofs.flush();
    if (!ofs.good()) {
      return false;
    }
  }

  // POSIX rename guarantees atomicity on Unix filesystems
  if (std::rename(tmp_path.c_str(), filepath.c_str()) != 0) {
    std::remove(tmp_path.c_str());
    return false;
  }
  return true;
}

} // namespace detail
} // namespace ros_param_sync
