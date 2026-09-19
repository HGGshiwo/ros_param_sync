#pragma once

#include <ros/ros.h>
#include <yaml-cpp/yaml.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ros_param_sync {
namespace detail {

class IParamEntry {
public:
  virtual ~IParamEntry() = default;
  virtual const std::string &name() const = 0;
  virtual bool is_valid() const = 0;
  virtual void load_from_yaml(const YAML::Node &node) = 0;
  virtual void push_to_ros(ros::NodeHandle &pnh) = 0;
  virtual bool pull_from_ros(ros::NodeHandle &pnh) = 0;
  virtual void save_to_yaml(YAML::Node &node) const = 0;
};

template <typename T>
class TypedParamEntry : public IParamEntry {
public:
  // 无默认值版本：若 YAML 和 ROS 参数服务器均未提供，则报错跳过，不篡改变量原值
  TypedParamEntry(const std::string &name, T &ref,
                  std::function<void(const T &)> on_change = nullptr)
      : name_(name), ref_(ref), default_val_(std::nullopt), on_change_(on_change), is_valid_(false) {}

  // 显式指定默认值版本
  TypedParamEntry(const std::string &name, T &ref, const T &default_val,
                  std::function<void(const T &)> on_change = nullptr)
      : name_(name), ref_(ref), default_val_(default_val), on_change_(on_change), is_valid_(false) {}

  const std::string &name() const override { return name_; }
  bool is_valid() const override { return is_valid_; }

  void load_from_yaml(const YAML::Node &node) override {
    if (node && node[name_]) {
      try {
        ref_ = node[name_].as<T>();
        is_valid_ = true;
        return;
      } catch (const std::exception &e) {
        ROS_ERROR_STREAM("[ParamSync] Failed to parse '" << name_ << "' from YAML: " << e.what());
      }
    }
    if (default_val_.has_value()) {
      ref_ = *default_val_;
      is_valid_ = true;
    } else {
      is_valid_ = false;
    }
  }

  void push_to_ros(ros::NodeHandle &pnh) override {
    if (is_valid_) {
      pnh.setParam(name_, ref_);
    }
  }

  bool pull_from_ros(ros::NodeHandle &pnh) override {
    T latest_val;
    if (pnh.getParam(name_, latest_val)) {
      if (!is_valid_ || latest_val != ref_) {
        ref_ = latest_val;
        is_valid_ = true;
        if (on_change_) {
          on_change_(ref_);
        }
        return true;
      }
    }
    return false;
  }

  void save_to_yaml(YAML::Node &node) const override {
    if (is_valid_) {
      node[name_] = ref_;
    }
  }

private:
  std::string name_;
  T &ref_;
  std::optional<T> default_val_;
  std::function<void(const T &)> on_change_;
  bool is_valid_ = false;
};

} // namespace detail
} // namespace ros_param_sync
