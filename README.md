# Nebula Particle

![After Effects](https://img.shields.io/badge/After%20Effects-Native%20Effect-9999ff)
![Version](https://img.shields.io/badge/version-0.5.0-blue)
![Platform](https://img.shields.io/badge/platform-macOS%20arm64-lightgrey)
![License](https://img.shields.io/badge/license-GPL--3.0--only-green)

给文字、Logo、背景和运动素材添加细致的粒子装饰。选择预设，再调整疏密、大小和存活时间，让元素持续闪耀、散发或留下彩色拖尾。

作者：Anthem / [AnthemZhou](https://github.com/AnthemZhou)

当前版本：**0.5.0**

[中文](README.md) · [English](README_EN.md) · [下载 v0.5.0](https://github.com/AnthemZhou/Nebula-Particle/releases/download/v0.5.0/Nebula-Particle-0.5.0-macos-arm64.zip) · [Releases](https://github.com/AnthemZhou/Nebula-Particle/releases) · [更新记录](CHANGELOG.md) · [Issues](https://github.com/AnthemZhou/Nebula-Particle/issues)

## 功能

- **持续产生粒子**：通过素材 Alpha、亮度、外部亮度通道或整个画面控制发射区域，原始元素可以保留显示。
- **序列帧拖尾**：读取画面中主体经过的位置，不要求 AE 图层具有位置关键帧。
- **贴图上色**：粒子出生时从来源或独立颜色图取色，之后携带颜色继续运动；也可使用统一染色。
- **清晰颗粒**：细尘、亮点和星芒，配合 Normal / Add / Screen 内部混合；默认使用克制的光晕。
- **独立动画**：分别调整淡入、淡出、闪烁强度与频率。光晕关闭时，粒子本身仍然可见。
- **可控运动**：初速、方向、散开和随时间衰减的阻尼。阻尼放在高级设置里。

独立预设面板包含 6 个内置起点，以及命名保存、浏览和应用“我的预设”的工作流。0.5.0 发布当前二维 CPU 功能集，功能范围和验证边界见[使用说明](docs/验证版使用说明.md)；V1.0 规划中的 GPU、光流和纵深功能尚未完成。

## 开始使用

1. 下载 [Nebula-Particle-0.5.0-macos-arm64.zip](https://github.com/AnthemZhou/Nebula-Particle/releases/download/v0.5.0/Nebula-Particle-0.5.0-macos-arm64.zip) 并解压。
2. 保存工程并退出 After Effects。
3. 运行解压目录中的 `install-macos.command`，安装用户级插件与预设面板。**不需要 SDK，也不需要自行编译。**
4. 重新打开 AE，在“效果和预设”中搜索 **Nebula Particle**，从“窗口”菜单打开 **Nebula Particle Presets.jsx**。

当前安装包为 macOS arm64，使用 ad-hoc 签名，尚未经过 Apple 公证。安装位置、系统提示和旧版本兼容说明见[安装说明](docs/安装说明.md)。安装器不会自动关闭 AE。

GitHub 自动生成的 Source code ZIP 是源码。需要修改或自行构建插件时，再阅读 [SDK 与构建说明](docs/SDK准备.md)。

## 一个常用工作流

1. 准备同尺寸的素材或预合成，最好带透明背景；运动可以已经烘焙在序列帧里。
2. 在合成尺寸的承载图层上添加 **Nebula Particle**，展开 **Sources** 连接来源。
3. 在预设面板中选择内置效果，再调 **Density、Size、Lifetime**。
4. 在 **Appearance** 调整颗粒、颜色和光晕，在 **Animation** 调整淡入淡出与闪烁。
5. 需要减速时展开 **Advanced → Motion → Damping**。
6. 将满意的设置命名保存到“我的预设”，以后在其他素材上复用。

外部亮度通道只负责粒子出现的位置；颜色图可以连接另一张彩色素材。当前版本建议把发射素材、外部通道和颜色图预合成为同尺寸、同帧率、同起点。

## 参数布局

顶层只保留四个常用入口：

| 参数 | 用途 |
| --- | --- |
| Preset | 选择内置效果起点 |
| Density | 每秒出生数量，控制疏密 |
| Size | 颗粒基础大小 |
| Lifetime | 存活时间，同时影响拖尾持续长度 |

其他参数放进默认折叠的 **Sources、Appearance、Animation、Advanced**。Advanced 再分 **Motion、Timing、Development tools**，让初速和阻尼、发射时序、开发用重读按钮各归其位。

6 个内置起点包括元素闪耀、细尘拖尾、星芒拖尾、轻柔漂移、向外火花和背景浮尘。它们用于快速建立效果；丰富的成品预设库仍在迭代中。

## 兼容与当前范围

- 当前构建目标为 **macOS arm64**，开发宿主为 After Effects 23.6。
- 渲染核心目前是 CPU 实现，宿主路径支持 8 / 16 bpc。
- 0.5.0 已在 AE 23.6 完成旧名升级、新名称识别、8/16 bpc 渲染、旧工程兼容和预设工作流回归；验证边界见使用说明。
- GPU、光流惯性、32 bpc / HDR、纵深与景深、Z 深度图、克隆及正式烘焙工作流尚未接入。

更详细的限制、图层采样规则和验证边界收录在[使用说明](docs/验证版使用说明.md)。没有 Particular 性能对比结论，也不以少量测试帧代表生产规模性能。

## 开发与反馈

```sh
git clone https://github.com/AnthemZhou/Nebula-Particle.git
cd Nebula-Particle
bash scripts/test-core.sh --sanitize
AE_SDK_ROOT="/path/to/AfterEffectsSDK" bash scripts/build.sh
python3 scripts/make-fixtures.py
python3 scripts/test-ae.py
```

AE 集成测试需要已安装的插件和对应版本的宿主。测试素材与输出在本地生成，不进入源码仓库。资源重读、原生更新与调试流程见 [SDK 与构建说明](docs/SDK准备.md)。

- [安装说明](docs/安装说明.md)
- [开发需求与版本计划](docs/开发需求与版本计划.md)
- [验证版使用说明](docs/验证版使用说明.md)
- [参与贡献](CONTRIBUTING.md)
- [提交问题或功能建议](https://github.com/AnthemZhou/Nebula-Particle/issues)

介绍与文档组织参考作者的 [Node Console](https://github.com/AnthemZhou/Node_Console) 项目；两者分别实现，不共享产品功能。

## 许可证

Copyright © 2026 AnthemZhou and contributors.

本项目采用 **GNU General Public License v3.0 only**，SPDX 标识为 `GPL-3.0-only`。完整文本见 [LICENSE](LICENSE)，不是“GPL v3 或任意后续版本”。

Adobe After Effects 与 Adobe SDK 为各自权利人的产品，按其自身条款提供。仓库不包含 Adobe SDK、宿主程序或私有测试素材。
