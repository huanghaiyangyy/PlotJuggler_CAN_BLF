![PlotJuggler](docs/plotjuggler3_banner.svg)

[![windows](https://github.com/facontidavide/PlotJuggler/actions/workflows/windows.yaml/badge.svg)](https://github.com/facontidavide/PlotJuggler/actions/workflows/windows.yaml)
[![ubuntu](https://github.com/facontidavide/PlotJuggler/actions/workflows/ubuntu.yaml/badge.svg)](https://github.com/facontidavide/PlotJuggler/actions/workflows/ubuntu.yaml)
[![macos](https://github.com/facontidavide/PlotJuggler/actions/workflows/macos.yaml/badge.svg)](https://github.com/facontidavide/PlotJuggler/actions/workflows/macos.yaml)
[![ROS1](https://github.com/facontidavide/PlotJuggler/workflows/ros1/badge.svg)](https://github.com/facontidavide/PlotJuggler/actions?query=workflow%3Aros1)
[![ROS2](https://github.com/facontidavide/PlotJuggler/workflows/ros2/badge.svg)](https://github.com/facontidavide/PlotJuggler/actions?query=workflow%3Aros2)
[![Tweet](https://img.shields.io/twitter/url/http/shields.io.svg?style=social)](https://twitter.com/intent/tweet?text=I%20use%20PlotJuggler%20and%20it%20is%20amazing%0D%0A&url=https://github.com/facontidavide/PlotJuggler&via=facontidavide&hashtags=dataviz,plotjuggler,GoROS,PX4)

## Fork Modifications (huanghaiyangyy)

### Upstream README Reference

- Original PlotJuggler README:
  https://github.com/facontidavide/PlotJuggler/blob/main/README.md

### Release Notes (3.17.3)

#### 新功能

##### DataLoadBLF - CAN/CAN FD 数据加载
- 支持加载 CAN/CAN FD 数据（BLF 格式）
- 可通过 DBC 文件解析信号
- 未解析消息自动回退为原始字节显示

##### 界面/交互优化
- 数据头摘要显示
- 信号树行为调整
- 时间戳显示修复
- 绘图交互增强

##### 内置 Lua 代码片段
单信号处理滤波（车速/IMU加速度信号处理）：
- `01_ax_mv_avg_flt`（轴移动平均滤波）
- `02_delta_v_from_speed`（由车速计算速度差）
- `03_dvx_mv_avg`（Delta_Vx 移动平均）
- `04_dvx_low_pass`（Delta_Vx 低通滤波）

##### 开发/发布流程
- 本地开发/发布工作区迁移及打包流程
- 适配 Ubuntu 20.04 交付

#### 性能优化
- BLF 加载性能优化：通过缓存序列写入、过滤未映射通道实现提速

#### Bug 修复
- 修复 Ubuntu AppImage 中 DBC 加载失败问题（dbcppp 库链接问题）

#### 系统要求
- Windows: 若主窗口无法打开，请安装最新版微软 Visual C++ 运行库：
  https://learn.microsoft.com/zh-cn/cpp/windows/latest-supported-vc-redist?view=msvc-170

#### Patch Notes (3.17.2 -> 3.17.3)
- Fixed Ubuntu AppImage BLF parsing regression where only one frame/message could be parsed for logs containing unknown BLF object types.
- Pinned `vector_blf` to upstream commit `3512fc2ddca43248c95b773905d9c3ba46bc6570` (includes unknown-object skip fix) to align CI artifacts with locally validated behavior.

#### Release Note Style and Compare Links
- This patch release inherits the previous release-note style and keeps a full compare link.
- **Full Changelog**: https://github.com/huanghaiyangyy/PlotJuggler_CAN_BLF/compare/3.17.1...3.17.2
- **Full Changelog**: https://github.com/huanghaiyangyy/PlotJuggler_CAN_BLF/compare/3.17.2...3.17.3

#### What's Changed
- [codex] fix CI BLF/dbcppp packaging and checks by @huanghaiyangyy in https://github.com/huanghaiyangyy/PlotJuggler_CAN_BLF/pull/3

#### New Contributors
- @huanghaiyangyy made their first contribution in https://github.com/huanghaiyangyy/PlotJuggler_CAN_BLF/pull/3

Design and implementation references:

- `docs/superpowers/specs/2026-03-28-canfd-blf-dataloader-design.md`
- `docs/superpowers/specs/2026-03-29-plotjuggler-blf-ui-interactions-design.md`
- `docs/superpowers/specs/2026-03-30-plotjuggler-dev-migration-design.md`

## Gold Sponsor:
[![Greenzie](docs/sponsor_greenzie.png)](https://www.greenzie.com/)
[![Intermodalics](docs/sponsor_intermodalics.png)](https://www.intermodalics.ai/)
[![Greenzie](docs/sponsor_ark.png)](https://arkelectron.com/)

# PlotJuggler 3.15

PlotJuggler is a tool to visualize time series that is **fast**, **powerful** and  **intuitive**.

Noteworthy features:

- Simple Drag & Drop user interface.
- Load __data from file__.
- Connect to live __streaming__ of data.
- Save the visualization layout and configurations to reuse them later.
- Fast **OpenGL** visualization.
- Can handle **thousands** of timeseries and **millions** of data points.
- Transform your data using a simple editor: derivative, moving average, integral, etc…
- PlotJuggler can be easily extended using __plugins__.

![PlotJuggler](docs/plotjuggler3.gif)


## Data sources (file and streaming)

- Load CSV files.
- Load [ULog](https://docs.px4.io/main/en/dev_log/ulog_file_format) (PX4).
- Subscribe to many different streaming sources: MQTT, WebSockets, ZeroMQ, UDP, etc.
- Understand data formats such as JSON, CBOR, BSON, Message Pack, etc.
- Well integrated with [ROS](https://www.ros.org/): open *rosbags* and/or subscribe to ROS *topics* (both ROS1 and ROS2).
- Supports the [Lab Streaming Layer](https://labstreaminglayer.readthedocs.io/info/intro.html), that is used by [many devices](https://labstreaminglayer.readthedocs.io/info/supported_devices.html).
- Easily add your custom data source and/or formats...

![](docs/data_sources.svg)

## Transform and analyze your data
PlotJuggler makes it easy to visualize data but also to analyze it.
You can manipulate your time series using a simple and extendable Transform Editor.

![](docs/function_editor.png)

Alternatively, you may use the Custom Function Editor, which allows you to create Multi-input / Single-output functions
using a scripting language based on [Lua](https://www.tutorialspoint.com/lua/index.htm).

If you are not familiar with Lua, don't be afraid, you won't need more than 5 minutes to learn it ;)

![](docs/custom_editor.png)

## Tutorials

To learn how to use PlotJuggler, check the tutorials here:

| Tutorial 1   |  Tutorial 2 | Tutorial 3 |
:-------------------------:|:-------------------------:|:-------------------------:
| [![](docs/tutorial_1.png)](https://slides.com/davidefaconti/introduction-to-plotjuggler) | [![](docs/tutorial_2.png)](https://slides.com/davidefaconti/plotjuggler-data) | [![](docs/tutorial_3.png)](https://slides.com/davidefaconti/plotjuggler-transforms) |

## Supported plugins

Some plugins can be found in a different repository. The individual README files
*should* include all the information needed to compile and use the plugin.

Please submit specific issues, Pull Requests and questions on the related Github repository:

- [MQTT DataStreamer](https://github.com/PlotJuggler/plotjuggler-mqtt).
- [Lab Streaming Layer DataStreamer](https://github.com/PlotJuggler/plotjuggler-lsl).
- [ROS plugins](https://github.com/PlotJuggler/plotjuggler-ros-plugins).
- [CAN .dbg DataLoader](https://github.com/PlotJuggler/plotjuggler-CAN-dbs).

If you want a simple example to learn how to write your own plugins, have a look at
[PlotJuggler/plotjuggler-sample-plugins](https://github.com/PlotJuggler/plotjuggler-sample-plugins)

# Installation

You can download the latest ready to use binaries from the [Release page](https://github.com/facontidavide/PlotJuggler/releases).

<div align="center">

| 🐧 Linux | 🍎 macOS | 🪟 Windows | 📦 Debian |
|:--------:|:--------:|:----------:|:---------:|
| **AppImage** | **Installer** | **Installer** | **Packages** |
| x86 / arm64 | x86 / arm64 | x64 | bookworm, trixie |

</div>

## Snap (recommended in Ubuntu, to ROS users too)

The snap contains a version of PlotJuggler that works (with some limitations) with ROS2.

![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)

To install it in Ubuntu, run:

```
sudo snap install plotjuggler
```

### Debian packages for ROS User

Install the ROS packages with:

```
sudo apt install ros-$ROS_DISTRO-plotjuggler-ros
```
To launch PlotJuggler, use the command:

```
ros2 run plotjuggler plotjuggler
```

ROS plugins are available in a separate repository: https://github.com/PlotJuggler/plotjuggler-ros-plugins

Please take a look at the instructions in that repository if you want to compile PJ and its ROS plugins from source.


## Compile from source

You can find the detailed instructions here: [COMPILE.md](COMPILE.md).

# Sponsorship and commercial support

PlotJuggler required a lot of work to develop and maintain; my goal is to build the most
intuitive and powerful tool to visualize data and timeseries.

If you find PlotJuggler useful, consider donating [PayPal](https://www.paypal.me/facontidavide) or becoming a
[Github Sponsor](https://github.com/sponsors/facontidavide).

If you need to extend any of the functionalities of PlotJuggler to cover a specific
need or to parse your custom data formats, you can receive commercial
support from the main author, [Davide Faconti](mailto:davide.faconti@gmail.com).

# License

PlotJuggler is released under the [Mozilla Public License Version 2.0](LICENSE.md),
which allows users to develop closed-source plugins.

Please note that some third-party dependencies (including Qt) use the
**GNU Lesser General Public License**.

# Star History

[![Star History Chart](https://api.star-history.com/svg?repos=facontidavide/PlotJuggler&type=Date)](https://star-history.com/#facontidavide/PlotJuggler&Date)

# Contributors

<a href="https://github.com/facontidavide/plotjuggler/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=facontidavide/plotjuggler" />
</a>
