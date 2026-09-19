# ros_param_sync

轻量级、Header-only 的 ROS 1 参数动态同步与持久化库（C++17）。

支持在节点运行期间通过 `rosparam set` 动态修改参数，自动同步更新 C++ 内部变量，并原子性持久化回写至 YAML 配置文件。

---

## 特性

- **Header-only**：无需编译，引入头文件即可使用。
- **双向热同步**：启动时从 YAML 文件加载并推入 ROS 参数服务器；运行中检测到参数变更自动更新内存变量并回写 YAML。
- **掉电安全**：基于临时文件与 POSIX `rename` 进行原子落盘，防止写入中断导致配置文件损坏。
- **零脏默认值**：支持无默认值绑定（`bind`），配置缺失时报错跳过，杜绝代码硬编码污染配置文件。
- **变更回调**：支持注册参数变动回调函数，在参数热更新时触发关联逻辑。
- **易于复用**：同时支持 CMake `FetchContent` 跨项目引入与标准 Catkin 依赖。

---

## 引入方式

### 1. CMake FetchContent（推荐）

在任意功能包的 `CMakeLists.txt` 中引入：

```cmake
include(FetchContent)
FetchContent_Declare(
  ros_param_sync
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../ros_param_sync # 本地路径或 Git 仓库
)
FetchContent_MakeAvailable(ros_param_sync)

target_link_libraries(your_target
  ${catkin_LIBRARIES}
  ros_param_sync::ros_param_sync
)
```

### 2. Catkin 依赖

若置于同一 `catkin_ws` 工作空间：

- `package.xml`：
  ```xml
  <depend>ros_param_sync</depend>
  ```
- `CMakeLists.txt`：
  ```cmake
  find_package(catkin REQUIRED COMPONENTS roscpp ros_param_sync)
  target_link_libraries(your_target ${catkin_LIBRARIES})
  ```

---

## 使用方法

### 1. C++ 节点

```cpp
#include <ros/ros.h>
#include <ros_param_sync/param_sync.hpp>

class MyNode {
public:
  MyNode(ros::NodeHandle& pnh) {
    std::string config_path;
    pnh.param<std::string>("param_config_path", config_path, "");

    // 1. 初始化（传入私有 NodeHandle 与 YAML 配置文件绝对路径）
    sync_.init(pnh, config_path);

    // 2. 绑定参数（类型自动推导，无默认值，未配置时报错跳过）
    sync_.bind("max_speed", max_speed_);
    sync_.bind("enable_flag", enable_flag_);

    // 3. 绑定参数并注册变更回调
    sync_.bind("kp", kp_, [this](const double& val) {
      ROS_INFO("kp updated to %.2f, resetting state...", val);
    });

    // 4. (可选) 绑定带显式默认值的参数
    sync_.bind_with_default("timeout", timeout_, 3.0);

    // 5. 启动后台同步轮询（单位：秒，默认 1.0s）
    sync_.start(1.0);
  }

private:
  ros_param_sync::ParamSync sync_;

  double max_speed_ = 0.0;
  bool enable_flag_ = false;
  double kp_ = 0.0;
  double timeout_ = 0.0;
};
```

### 2. YAML 配置文件 (`config.yaml`)

```yaml
max_speed: 2.5
enable_flag: true
kp: 1.2
timeout: 5.0
```

### 3. Launch 文件

```xml
<launch>
    <node pkg="my_pkg" type="my_node" name="my_node" output="screen">
        <param name="param_config_path" value="$(find my_pkg)/config/config.yaml" />
    </node>
</launch>
```

### 4. 运行时动态调参

节点运行期间直接在终端执行：

```bash
rosparam set /my_node/max_speed 3.5
```

后台定时器将自动检测到变更，更新 `max_speed_` 内存变量，并将新值原子性写回 `config.yaml`。

---

## API 参考

| 方法 | 说明 |
| :--- | :--- |
| `void init(ros::NodeHandle pnh, const std::string& config_path)` | 初始化同步器，指定私有句柄与目标 YAML 文件路径。 |
| `template<typename T> void bind(name, ref, [on_change])` | **推荐**。绑定变量（无默认值），未配置报错跳过；可选传入变更回调 `std::function<void(const T&)>`。 |
| `template<typename T> void bind_with_default(name, ref, default_val, [on_change])` | 绑定变量，若 YAML 和参数服务器均未配置，则回退到 `default_val`。 |
| `bool start(double interval_sec = 1.0)` | 启动参数初始加载与后台周期同步轮询定时器。 |
| `void stop()` | 停止后台轮询定时器。 |
| `void check_and_sync()` | 手动触发一次检查与同步（适用于无定时器或单步测试场景）。 |

---

## License

MIT
