# Misc IO 通用 IO 组件

## 项目简介
提供基于 Linux GPIO 的通用输入/输出封装，支持按电平变化触发回调或主动读写 GPIO，解决零散 IO 设备（蜂鸣器、继电器、开关、传感器等）接入时的重复适配问题。

## 功能特性
支持：
- 基于 libgpiod 的 GPIO 输入/输出
- 输入去抖与电平变化回调
- 主动读写 GPIO 电平
- 适配多类简单外设（如蜂鸣器/继电器/开关）

不支持：
- PWM、编码器等复杂外设
- GPIO 中断/事件驱动（当前为轮询触发）
- 非 Linux/非 libgpiod 环境

## 快速开始
最短路径跑起来：编译执行 `./test_misc_io` 并触发输入回调打印。

### 环境准备
- Linux 系统，支持 libgpiod（v1/v2 均可）
- `gcc`/`cmake`/`make` 构建工具
- 可访问 `/dev/gpiochip*` 的权限（必要时使用 `sudo`）

### 构建编译
以下为脱离 SDK 的独立构建方式：
```bash
mkdir -p build
cd build
cmake ..
make -j
```

### 运行示例
```bash
sudo ./build/test_misc_io
```
说明：
- 示例默认监听 `gpiochip0` 的 `line_offset=9`，请根据硬件修改 `example/test_misc_io.c`。
- k3的gpio参数配置采用chip+line_offset，比如GPIO83, 对应的输入为chip_name = gpiochip2;line_offset=19。（每个chip有32线）
- 而对于k1，则只有gpiochip0，比如GPIO113, 对应的输入为gpiochip0， 113。
- 如需测试输出或轮询读取，可切换 `main()` 中的 `test_set_io()` 或 `test_get_io()`。

## 详细使用
保留，引用到后续的官方文档。

## 常见问题
- 无回调触发：确认 `line_offset` 是否正确、`active` 逻辑配置是否与硬件一致。
- 无法打开 GPIO：检查 `/dev/gpiochip*` 权限，必要时使用 `sudo`。
- 电平读取异常：确认输入方向配置为 `MISC_DIR_INPUT`。

## 版本与发布

版本以本目录 `package.xml` 中的 `<version>` 为准。

| 版本   | 日期       | 说明 |
| ------ | ---------- | ---- |
| 0.1.0  | 2026-02-28 | 初始版本，提供通用 GPIO 输入/输出与回调接口。 |

## 贡献方式

欢迎参与贡献：提交 Issue 反馈问题，或通过 Pull Request 提交代码。

- **编码规范**：本组件 C 代码遵循 [Google C++ 风格指南](https://google.github.io/styleguide/cppguide.html)（C 相关部分），请按该规范编写与修改代码。
- **提交前检查**：请在提交前运行本仓库的 lint 脚本，确保通过风格检查：
  ```bash
  # 在仓库根目录执行（检查全仓库）
  bash scripts/lint/lint_cpp.sh

  # 仅检查本组件
  bash scripts/lint/lint_cpp.sh components/peripherals/misc_io
  ```
  脚本路径：`scripts/lint/lint_cpp.sh`。若未安装 `cpplint`，可先执行：`pip install cpplint` 或 `pipx install cpplint`。
- **提交说明**：提交 Issue 或 PR 前请描述外设类型、GPIO 芯片/线号与复现步骤。

## License
本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
