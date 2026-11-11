# Particle Atelier

![After Effects](https://img.shields.io/badge/After%20Effects-Native%20Effect-9999ff)
![Version](https://img.shields.io/badge/version-0.2.0--dev-blue)
![Platform](https://img.shields.io/badge/platform-macOS%20arm64-lightgrey)
![License](https://img.shields.io/badge/license-GPL--3.0--only-green)

Fine particle decoration for text, logos, backgrounds and moving footage in After Effects. Start with a preset, then adjust density, size and lifetime to create continuous sparkle, emission or colored trails.

Author: Anthem / [AnthemZhou](https://github.com/AnthemZhou)

Current stage: **v0.2 development preview**

[中文](README.md) · [English](README_EN.md) · [Releases](https://github.com/AnthemZhou/Particle_Atelier/releases) · [Changelog](CHANGELOG.md) · [Issues](https://github.com/AnthemZhou/Particle_Atelier/issues)

## Features

- **Continuous emission:** use source alpha, luminance, an external luminance pass or the full frame. Keep the original element visible if needed.
- **Trails from image sequences:** emit from the subject's position in the image content, without requiring AE position keyframes.
- **Image-driven color:** sample a source or separate color map at birth and carry that color through the particle's life; a uniform tint is also available.
- **Clear particles:** dust, points and stars with Normal / Add / Screen particle blending and restrained glow.
- **Independent animation:** separate fade-in, fade-out, twinkle strength and frequency. Turning glow off preserves the particles themselves.
- **Motion controls:** initial speed, direction, spread and time-based exponential damping. Damping lives in the advanced settings.

v0.2 provides cleaner parameter groups and a separate preset panel with six built-in starting points, plus named user presets that can be saved and reapplied. See the [preview guide](docs/验证版使用说明.md) for the host verification status of the panel and groups. The development branch is not a stable release.

## Install from source

The current preview is built from source. Obtain the Adobe After Effects C++ SDK separately; it is not included in this repository.

```sh
git clone https://github.com/AnthemZhou/Particle_Atelier.git
cd Particle_Atelier

# Replace this with your extracted SDK directory from Adobe.
AE_SDK_ROOT="/path/to/AfterEffectsSDK" bash scripts/build.sh
bash scripts/install-dev.sh
AE_VERSION=23.6 bash scripts/install-panel.sh
```

See the [SDK and build guide](docs/SDK准备.md) for prerequisites. Set `AE_VERSION` to the preferences-version folder of the host where the panel should be installed.

Save your work and quit AE before replacing an installed native plugin. The installer does not close AE automatically. Start a new session and search **Effects & Presets** for **Particle Atelier**. Open the preset panel from AE's **Window** menu, or run `scripts/Particle Atelier Presets.jsx` through **File → Scripts → Run Script File** during development.

Compiled packages will be listed on [Releases](https://github.com/AnthemZhou/Particle_Atelier/releases) when published. GitHub's automatic Source code ZIP is not a compiled `.plugin` installer.

## A typical workflow

1. Prepare footage or a precomposition, preferably with alpha. Motion may already be rendered into an image sequence.
2. Apply **Particle Atelier** to a composition-sized carrier layer, then connect the source in **Sources**.
3. Choose a built-in preset and adjust **Density, Size and Lifetime**.
4. Use **Appearance** for shape, color and glow, and **Animation** for fades and twinkle.
5. Open **Advanced → Motion → Damping** when particles should slow down after emission.
6. Save the result as a named user preset to reuse with other footage.

An external luminance pass controls where particles are born; another image can provide their colors. For the current preview, precompose emission footage, external masks and color maps to the same dimensions, frame rate and start time.

## Controls

v0.2 keeps four frequently used controls at the top level:

| Control | Purpose |
| --- | --- |
| Preset | Select a built-in starting point |
| Density | Births per second |
| Size | Base particle size |
| Lifetime | Particle duration, which also affects trail length |

Other controls are in the initially collapsed **Sources, Appearance, Animation and Advanced** groups. Advanced contains **Motion, Timing and Development** subgroups.

The six built-in starting points cover surface sparkle, fine dust trails, star trails, gentle drift, outward sparks and background dust. A larger library of finished looks remains in development.

## Compatibility and scope

- The current build target is **macOS arm64**; the development host is After Effects 23.6.
- The renderer currently uses the CPU, with 8 / 16 bpc host paths.
- v0.2 has passed host regression checks for rendering, saved-project compatibility and preset data operations. Complete visual panel interaction remains under verification.
- GPU acceleration, optical-flow inertia, 32 bpc / HDR, depth and depth of field, Z output, cloning and a complete bake workflow are not implemented yet.

See the [preview guide](docs/验证版使用说明.md) for sampling rules and verification limits. No performance advantage over Particular is claimed.

## Development and feedback

```sh
bash scripts/test-core.sh --sanitize
python3 scripts/make-fixtures.py
python3 scripts/test-ae.py
```

AE integration tests require an installed build and the matching host. Fixtures and outputs are generated locally and excluded from source control. The [build guide](docs/SDK准备.md) explains resource reloads and native updates.

- [Requirements and roadmap](docs/开发需求与版本计划.md)
- [Preview guide](docs/验证版使用说明.md)
- [Contributing](CONTRIBUTING.md)
- [Report a problem or suggest a feature](https://github.com/AnthemZhou/Particle_Atelier/issues)

The introduction and documentation structure are inspired by the author's [Node Console](https://github.com/AnthemZhou/Node_Console) project. The two products are implemented separately.

## License

Copyright © 2026 AnthemZhou and contributors.

This project is licensed under the **GNU General Public License v3.0 only**, SPDX identifier `GPL-3.0-only`. See [LICENSE](LICENSE) for the complete text. This is not an “or any later version” grant.

Adobe After Effects and the Adobe SDK are provided by their respective rights holders under their own terms. This repository contains no Adobe SDK, host application or private test media.
