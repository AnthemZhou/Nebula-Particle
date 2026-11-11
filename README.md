# Particle Atelier

![After Effects](https://img.shields.io/badge/After%20Effects-Native%20Effect-9999ff)
![Version](https://img.shields.io/badge/version-0.2.0--dev-blue)
![Platform](https://img.shields.io/badge/platform-macOS%20arm64-lightgrey)
![License](https://img.shields.io/badge/license-GPL--3.0--only-green)

给文字、Logo、背景和运动素材添加细致的粒子装饰。选择预设，再调整疏密、大小和存活时间，让元素持续闪耀、散发或留下彩色拖尾。

作者：Anthem / [AnthemZhou](https://github.com/AnthemZhou)

当前阶段：**v0.2 开发预览**

[中文](README.md) · [English](README_EN.md) · [Releases](https://github.com/AnthemZhou/Particle_Atelier/releases) · [更新记录](CHANGELOG.md) · [Issues](https://github.com/AnthemZhou/Particle_Atelier/issues)

## 功能

- **持续产生粒子**：通过素材 Alpha、亮度、外部亮度通道或整个画面控制发射区域，原始元素可以保留显示。
- **序列帧拖尾**：读取画面中主体经过的位置，不要求 AE 图层具有位置关键帧。
- **贴图上色**：粒子出生时从来源或独立颜色图取色，之后携带颜色继续运动；也可使用统一染色。
- **清晰颗粒**：细尘、亮点和星芒，配合 Normal / Add / Screen 内部混合；默认使用克制的光晕。
- **独立动画**：分别调整淡入、淡出、闪烁强度与频率。光晕关闭时，粒子本身仍然可见。
- **可控运动**：初速、方向、散开和随时间衰减的阻尼。阻尼放在高级设置里。

v0.2 提供更简洁的参数分组与独立预设面板：包含 6 个内置起点，以及命名保存、浏览和应用“我的预设”的工作流。新面板与分组的宿主验收状态见[使用说明](docs/验证版使用说明.md)，不以开发分支代替稳定发布版。

## 开始使用

当前开发版需要从源码构建。Adobe After Effects C++ SDK 由使用者单独获取，不随本仓库提供。

```sh
git clone https://github.com/AnthemZhou/Particle_Atelier.git
cd Particle_Atelier

# 将路径换成你从 Adobe 获取并解压的 SDK 目录。
AE_SDK_ROOT="/path/to/AfterEffectsSDK" bash scripts/build.sh
bash scripts/install-dev.sh
AE_VERSION=23.6 bash scripts/install-panel.sh
```

详细环境要求见 [SDK 与构建说明](docs/SDK准备.md)。`AE_VERSION` 应与需要安装面板的 AE 偏好版本目录一致。

原生插件更新前先保存工程并退出 AE。安装脚本不会自动关闭应用；重新启动后，在“效果和预设”中搜索 **Particle Atelier**。预设面板从 AE 的 **窗口**菜单打开；开发时也可通过“文件 → 脚本 → 运行脚本文件”运行 `scripts/Particle Atelier Presets.jsx`。

发布安装包后将通过 [Releases](https://github.com/AnthemZhou/Particle_Atelier/releases) 提供。GitHub 自动生成的 Source code ZIP 是源码，不是编译好的 `.plugin`。

## 一个常用工作流

1. 准备同尺寸的素材或预合成，最好带透明背景；运动可以已经烘焙在序列帧里。
2. 在合成尺寸的承载图层上添加 **Particle Atelier**，展开 **Sources** 连接来源。
3. 在预设面板中选择内置效果，再调 **Density、Size、Lifetime**。
4. 在 **Appearance** 调整颗粒、颜色和光晕，在 **Animation** 调整淡入淡出与闪烁。
5. 需要减速时展开 **Advanced → Motion → Damping**。
6. 将满意的设置命名保存到“我的预设”，以后在其他素材上复用。

外部亮度通道只负责粒子出现的位置；颜色图可以连接另一张彩色素材。当前版本建议把发射素材、外部通道和颜色图预合成为同尺寸、同帧率、同起点。

## 参数布局

v0.2 的顶层只保留四个常用入口：

| 参数 | 用途 |
| --- | --- |
| Preset | 选择内置效果起点 |
| Density | 每秒出生数量，控制疏密 |
| Size | 颗粒基础大小 |
| Lifetime | 存活时间，同时影响拖尾持续长度 |

其他参数放进默认折叠的 **Sources、Appearance、Animation、Advanced**。Advanced 再分 **Motion、Timing、Development**，让初速和阻尼、发射时序、开发用重读按钮各归其位。

6 个内置起点包括元素闪耀、细尘拖尾、星芒拖尾、轻柔漂移、向外火花和背景浮尘。它们用于快速建立效果；丰富的成品预设库仍在迭代中。

## 兼容与当前范围

- 当前构建目标为 **macOS arm64**，开发宿主为 After Effects 23.6。
- 渲染核心目前是 CPU 实现，宿主路径支持 8 / 16 bpc。
- v0.2 已完成渲染、旧工程兼容和预设数据操作的宿主回归；完整面板视觉交互仍在验收中。
- GPU、光流惯性、32 bpc / HDR、纵深与景深、Z 深度图、克隆及正式烘焙工作流尚未接入。

更详细的限制、图层采样规则和验证边界收录在[使用说明](docs/验证版使用说明.md)。没有 Particular 性能对比结论，也不以少量测试帧代表生产规模性能。

## 开发与反馈

```sh
bash scripts/test-core.sh --sanitize
python3 scripts/make-fixtures.py
python3 scripts/test-ae.py
```

AE 集成测试需要已安装的插件和对应版本的宿主。测试素材与输出在本地生成，不进入源码仓库。资源重读、原生更新与调试流程见 [SDK 与构建说明](docs/SDK准备.md)。

- [开发需求与版本计划](docs/开发需求与版本计划.md)
- [验证版使用说明](docs/验证版使用说明.md)
- [参与贡献](CONTRIBUTING.md)
- [提交问题或功能建议](https://github.com/AnthemZhou/Particle_Atelier/issues)

介绍与文档组织参考作者的 [Node Console](https://github.com/AnthemZhou/Node_Console) 项目；两者分别实现，不共享产品功能。

## 许可证

Copyright © 2026 AnthemZhou and contributors.

本项目采用 **GNU General Public License v3.0 only**，SPDX 标识为 `GPL-3.0-only`。完整文本见 [LICENSE](LICENSE)，不是“GPL v3 或任意后续版本”。

Adobe After Effects 与 Adobe SDK 为各自权利人的产品，按其自身条款提供。仓库不包含 Adobe SDK、宿主程序或私有测试素材。
