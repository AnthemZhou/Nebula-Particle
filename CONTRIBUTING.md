# Contributing / 参与贡献

感谢帮助 Particle Atelier 改进粒子质感、AE 工作流和性能。问题反馈、可复现工程说明、预设建议、文档修正和代码都欢迎。

## 反馈问题

在 [Issues](https://github.com/AnthemZhou/Particle_Atelier/issues) 描述：

- 插件版本、AE 版本、操作系统与运行架构。
- 复现步骤，以及预期结果和实际结果。
- 合成尺寸、帧率、位深、来源类型与关键参数。
- 是否能够在空工程及自行生成的简单素材上复现。

可以附上已经确认可公开的截图或最小测试工程。分享日志前移除用户名、个人目录、账号信息和私有素材路径；不要提交 SDK、Adobe 安装包、授权文件或客户素材。

## 开始开发

1. Fork 仓库并创建工作分支。
2. 按 [SDK 与构建说明](docs/SDK准备.md) 准备本地开发环境。
3. 保持改动聚焦，先完成可验证的行为，再扩展功能。
4. 运行与改动相关的检查，在 PR 中说明结果与尚未验证的部分。

```sh
bash scripts/test-core.sh --sanitize
AE_SDK_ROOT="/path/to/AfterEffectsSDK" bash scripts/build.sh
```

修改宿主包装、参数、位深、历史帧采样或预设应用逻辑时，需要在 AE 中验证。单独核心测试和成功编译不能替代宿主测试。

## 代码与兼容性

- 粒子核心尽量独立于 AE，以便测试确定性、生命周期、运动和合成。
- 参数磁盘 ID 与保存数据需要保持兼容；调整面板分组时不要按视觉顺序重新编号旧参数。
- AE 可以非顺序或并行请求帧，不要依赖单一“上一帧”的全局状态。
- 清楚区分原生 C++ 更新、外部资源重读和将来的 GPU 代码热重载。
- 主面板维持精简；阻尼、详细物理参数与开发工具继续放在折叠组中。
- 新功能、位深和硬件加速只有在实现并测试后才能写入支持列表。

## 测试与提交

- 对逻辑变更运行核心测试；内存、像素或生命周期变更同时运行 sanitizer。
- 对影响 AE 的改动，记录宿主版本、复现素材、实际输出和限制。
- 对纯文档修改检查链接、命令与当前参数名，不需要无关的完整渲染回归。
- 不提交 `.local/`、`build/`、`test-output/`、SDK、插件二进制、个人预设库或私有日志。
- PR 描述写清问题、改动后的行为、验证方式及未完成事项；需要时更新 [CHANGELOG.md](CHANGELOG.md)。

没有性能测量时，请描述算法或体验变化，不写“更快”“实时”等未经验证的结论。

## 许可

贡献到本仓库的原创内容按项目的 **GPL-3.0-only** 许可提供。不要引入无权重新分发的代码或资源。第三方依赖需要注明来源、版本与许可证，并与项目分发方式兼容。

Adobe SDK 需要贡献者自行取得，只作为本地构建依赖；不能复制到源码仓库。

## English summary

Report a reproducible issue with the plugin/AE versions, platform, composition settings and expected behavior. Use public or self-generated media, and remove personal paths from logs.

Keep pull requests focused. Run the relevant core tests and verify AE integration changes in the host. Preserve parameter IDs and saved-project compatibility. Do not commit the SDK, compiled plugins, local test output or private presets. Clearly separate implemented features, tested behavior and future plans.

Original contributions are submitted under **GPL-3.0-only**. See [LICENSE](LICENSE).
