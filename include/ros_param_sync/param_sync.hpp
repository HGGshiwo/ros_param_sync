#pragma once

#include "detail/param_entry.hpp"
#include "detail/atomic_file.hpp"

#include <ros/ros.h>
#include <yaml-cpp/yaml.h>

#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <mutex>
#include <sys/stat.h>

namespace ros_param_sync {

/**
 * @brief Manages bidirectional synchronization between ROS Parameter Server,
 * C++ in-memory variables, and a persistent YAML configuration file.
 */
class ParamSync {
public:
  ParamSync() = default;
  ~ParamSync() {
    stop();
  }

  // Non-copyable
  ParamSync(const ParamSync &) = delete;
  ParamSync &operator=(const ParamSync &) = delete;

  /**
   * @brief Initialize with a private NodeHandle and the target YAML configuration path.
   */
  void init(ros::NodeHandle pnh, const std::string &config_path) {
    pnh_ = pnh;
    config_path_ = config_path;
  }

  /**
   * @brief Bind an in-memory variable WITHOUT a hardcoded default value.
   * If missing from both YAML and ROS parameter server, an error is reported and it is skipped.
   * 
   * @tparam T Variable type
   * @param name Parameter key name
   * @param ref Reference to the internal variable
   * @param on_change Optional callback invoked when parameter changes at runtime
   */
  template <typename T>
  void bind(const std::string &name, T &ref,
            std::function<void(const T &)> on_change = nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(std::make_unique<detail::TypedParamEntry<T>>(
        name, ref, on_change));
  }

  /**
   * @brief Bind an in-memory variable WITH an explicit default value.
   */
  template <typename T>
  void bind_with_default(const std::string &name, T &ref, const T &default_val,
                         std::function<void(const T &)> on_change = nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(std::make_unique<detail::TypedParamEntry<T>>(
        name, ref, default_val, on_change));
  }

  /**
   * @brief Start initial synchronization and launch the recurring polling timer.
   * 
   * @param interval_sec Polling interval in seconds (default: 1.0s)
   */
  bool start(double interval_sec = 1.0) {
    if (config_path_.empty()) {
      ROS_WARN("[ParamSync] No config path provided, skip starting.");
      return false;
    }

    // 1. Initial load from YAML and sync with ROS parameter server
    load_and_sync_initial();

    // 2. Start polling timer
    ros::NodeHandle nh;
    timer_ = nh.createTimer(ros::Duration(interval_sec),
                            &ParamSync::timerCallback, this);

    ROS_INFO("[ParamSync] Started parameter sync monitoring for '%s' (polling every %.2fs).",
             config_path_.c_str(), interval_sec);
    return true;
  }

  /**
   * @brief Stop the background polling timer.
   */
  void stop() {
    if (timer_.isValid()) {
      timer_.stop();
    }
  }

  /**
   * @brief Manually poll the ROS parameter server for any updates,
   * updating internal variables and persisting changes to YAML if dirty.
   */
  void check_and_sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    bool is_dirty = false;
    for (auto &entry : entries_) {
      if (entry->pull_from_ros(pnh_)) {
        is_dirty = true;
      }
    }

    if (is_dirty) {
      save_to_yaml_locked();
    }
  }

private:
  void load_and_sync_initial() {
    std::lock_guard<std::mutex> lock(mutex_);
    YAML::Node root;
    try {
      root = YAML::LoadFile(config_path_);
    } catch (const std::exception &e) {
      ROS_WARN("[ParamSync] Cannot load config file '%s' (%s). A new file will be created upon first save.",
               config_path_.c_str(), e.what());
      root = YAML::Node(YAML::NodeType::Map);
    }

    for (auto &entry : entries_) {
      entry->load_from_yaml(root);

      if (pnh_.hasParam(entry->name())) {
        entry->pull_from_ros(pnh_);
      } else if (entry->is_valid()) {
        entry->push_to_ros(pnh_);
      } else {
        ROS_ERROR_STREAM("[ParamSync] Parameter '" << entry->name()
                         << "' is not provided in YAML config and not in ROS parameter server! Skipping.");
      }
    }
  }

  void timerCallback(const ros::TimerEvent &) {
    check_and_sync();
  }

  bool save_to_yaml_locked() {
    YAML::Node root;
    try {
      root = YAML::LoadFile(config_path_);
    } catch (...) {
      root = YAML::Node(YAML::NodeType::Map);
    }

    for (const auto &entry : entries_) {
      if (entry->is_valid()) {
        entry->save_to_yaml(root);
      }
    }

    // Ensure parent directory exists
    size_t last_slash = config_path_.find_last_of('/');
    if (last_slash != std::string::npos) {
      std::string dir = config_path_.substr(0, last_slash);
      mkdir(dir.c_str(), 0755);
    }

    std::stringstream ss;
    ss << root << "\n";
    if (detail::atomic_write_file(config_path_, ss.str())) {
      ROS_INFO("[ParamSync] Successfully updated and persisted parameters to: %s",
               config_path_.c_str());
      return true;
    } else {
      ROS_ERROR("[ParamSync] Failed to atomically write config to: %s",
                config_path_.c_str());
      return false;
    }
  }

  ros::NodeHandle pnh_;
  std::string config_path_;
  std::vector<std::unique_ptr<detail::IParamEntry>> entries_;
  ros::Timer timer_;
  std::mutex mutex_;
};

} // namespace ros_param_sync
