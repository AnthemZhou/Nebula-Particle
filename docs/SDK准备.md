# Adobe SDK 与构建说明

本文面向从源码构建 Particle Atelier 的开发者。当前脚本目标为 macOS arm64，最低构建部署目标为 macOS 12.0；具体宿主兼容性以对应版本的 AE 验证记录为准。

Adobe SDK 不随仓库分发。不要把 SDK 头文件、示例源码、下载映像、宿主程序或授权文件提交到 Git。

## 1. 获取 SDK

1. 打开 [Adobe After Effects 开发入口](https://developer.adobe.com/after-effects/)。
2. 进入 **Get the SDKs**；下载入口可能要求使用自己的 Adobe 账号登录并接受相应条款。
3. 获取 macOS **C/C++ Plug-in SDK**，不是 UXP Developer Tools 或 AE 应用安装包。
4. 解压后保留完整目录，把 `AE_SDK_ROOT` 指向 SDK 根目录。

原生构建基线使用 May 2023 macOS SDK。其他 SDK 版本需要重新构建与宿主验证，不能仅凭头文件存在判断兼容。

构建脚本支持以下布局：

```text
AfterEffectsSDK/
└── Examples/
    ├── Headers/
    ├── Util/
    └── Resources/
```

也可以让 `AE_SDK_ROOT` 直接指向包含 `Headers`、`Util`、`Resources` 的 `Examples` 目录。仅限本地的 `.local/ae-sdk/` 是脚本默认位置，并被 Git 忽略。

## 2. 开发依赖

- macOS 与 Xcode，且 `xcode-select` 指向所需的开发工具。
- Apple Clang C++17 编译器、macOS SDK、`Rez`、`codesign` 与 `ditto`。
- 自行获取的 Adobe After Effects C++ SDK。
- 与目标架构相符的 AE 宿主；当前构建只有 `arm64`，不是 Intel/Apple Silicon 通用二进制。
- Python 3，用于生成测试素材、调用 AE 集成测试和验证输出。

可先检查工具：

```sh
xcode-select -p
xcrun clang++ --version
xcrun --find Rez
```

## 3. 编译与安装

在仓库根目录执行：

```sh
bash scripts/test-core.sh --sanitize
AE_SDK_ROOT="/path/to/AfterEffectsSDK" bash scripts/build.sh
```

路径示例必须换成自己解压 SDK 的目录。构建脚本会检查所需 SDK 文件，编译 C++ 与 PiPL 资源，生成开发期临时签名，并验证包和架构。缺少 SDK 或编译失败时应停止，不生成声称可用的安装包。

输出入口：

- `build/Particle Atelier.plugin`：指向本地签名实体包的符号链接。
- `build/Particle Atelier.zip`：便于复制的构建归档，不自动发布。

实体包存放在用户 Application Support 下的 `Particle Atelier/Builds/`。这样可避免受同步服务管理的工作目录给已签名包追加 Finder 元数据。不要直接修改已签名包；改变资源后应重建或使用外部资源重读。

安装原生效果：

```sh
bash scripts/install-dev.sh
```

脚本安装到用户级 MediaCore：

```text
~/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/
```

安装器只处理 Particle Atelier，核验包标识与签名，并在替换前备份旧版本。更新已有原生插件前需要保存工程并退出 AE；安装器不会自动关闭或启动宿主。首次复制新插件也需要新 AE 会话才能被发现。

开发期临时签名不是公开发行所需签名、公证和分发流程的完成证明。

## 4. 安装预设面板

v0.2 面板包含：

- `scripts/Particle Atelier Presets.jsx`
- `scripts/ParticleAtelierPresetLib.jsxinc`

两份文件必须放在同一目录。安装命令：

```sh
AE_VERSION=23.6 bash scripts/install-panel.sh
```

安装目标为所选 AE 偏好版本目录下的用户级 `Scripts/ScriptUI Panels/`。`AE_VERSION` 应与实际使用的 AE 偏好版本一致，不能只凭软件营销年份推断。

启动新 AE 会话后从 **窗口** 菜单打开面板。开发时可通过 **文件 → 脚本 → 运行脚本文件** 直接运行 JSX，作为浮动面板使用。停靠、菜单发现和预设操作的实际兼容性需要在对应宿主中验收。

## 5. 资源重读与原生更新

编辑仓库的 `resources/presets.ini` 后执行：

```sh
bash scripts/update-presets.sh
```

外部可编辑资源位于：

```text
~/Library/Application Support/Particle Atelier/presets.ini
```

然后在效果控件中使用 **Advanced → Development → Reload current preset**。此操作会重新应用当前内置预设的视觉值，保留来源连接、发射通道、种子和停止时间，并使相关画面缓存失效。

这是资源重读，不是原生 C++ 热更新，也不是已经实现 GPU 热重载。修改 C++、参数结构或 PiPL 后必须重新构建，并通过新宿主会话验证。保存“我的预设”也不等于更换已加载的插件代码。

## 6. 测试

独立核心测试不要求 Adobe SDK：

```sh
bash scripts/test-core.sh --sanitize
```

安装构建后可执行集成测试：

```sh
python3 scripts/make-fixtures.py
python3 scripts/test-ae.py
```

现有集成脚本针对 AE 2023 的标准 macOS 安装位置；其他宿主版本需先检查脚本配置。它生成独立测试工程，调用真实 AE 渲染器，并检查透明输出、阻尼、历史来源拖尾和停止发射。运行前保存当前工作，不把测试脚本当作生产工程操作工具。

输出位于被忽略的 `test-output/`。不要提交原始日志或含本机目录的工程指针；公开验证记录只保留必要的环境类别、场景和结果。转换为 8-bit PNG/PSD 的检查副本不能证明原始 16 bpc 输出完整精度。

## 7. 宿主接入要点

- 参数磁盘 ID 需要保持稳定；面板分组改变不能破坏已保存工程。
- 历史帧 checkout 需要正确的时间尺度和对应 check-in；归还之后不能继续持有像素指针。
- 普通图层 checkout 不应假定自动包含所有前序效果、遮罩与变换；当前输入建议使用烘焙素材或预合成。
- 每张图像分别使用自己的 `rowbytes` 和正确位深，处理预乘 Alpha。
- 时间变化、跨时间输入、位深和渲染能力标志必须与实际实现一致。
- 不因编译成功就声明多帧渲染、GPU、32 bpc 或其他尚未完成的能力。

相关 SDK 文档：[起步说明](https://ae-plugins.docsforadobe.dev/intro/how-to-start-creating-plug-ins/)、[参数](https://ae-plugins.docsforadobe.dev/effect-basics/parameters/)、[图层采样回调](https://ae-plugins.docsforadobe.dev/effect-details/interaction-callback-functions/)、[时间与尺寸](https://ae-plugins.docsforadobe.dev/effect-basics/PF_InData/)、[PiPL](https://ae-plugins.docsforadobe.dev/intro/pipl-resources/)。

## 8. 源码与发布边界

本仓库的原创源码使用 GPL-3.0-only；Adobe SDK 由 Adobe 单独提供。构建依赖与本项目许可分别保留，不把 SDK 重打包为本项目代码。

源码仓库不包含 SDK、编译插件、安装映像、个人预设库和私有测试结果。公开二进制发行需要对应版本、源代码、许可证和可复现的构建说明，并明确实际验证过的平台与宿主版本。
