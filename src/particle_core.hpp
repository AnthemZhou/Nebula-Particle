#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// CPU reference renderer for the first AE-loadable prototype. This is not the
// eventual GPU renderer, optical-flow implementation, or 3D renderer.
namespace particlelab {

struct Pixel {
    float r = 0, g = 0, b = 0, a = 0;
};

struct ImageView {
    const Pixel* pixels = nullptr;
    int width = 0, height = 0;
    std::size_t rowStridePixels = 0; // Zero means width. Never measured in bytes.
    bool premultiplied = false;
};

struct MutableImageView {
    Pixel* pixels = nullptr;
    int width = 0, height = 0;
    std::size_t rowStridePixels = 0;
};

struct SourceFrame {
    ImageView source;
    ImageView emissionMask; // Empty falls back to source.
    ImageView colorMap;     // Empty falls back to source.
};

// The returned image storage must remain alive until the next provider call.
// Calls are for historical, composition-frame-aligned birth samples, in order
// within one evaluation; separate evaluations may occur in any order/thread.
using FrameProvider = std::function<bool(double birthSampleTime, SourceFrame&)>;

enum class Emission { Alpha, Luminance, ExternalLuminance, FullFrame };
enum class Shape { Dust, Point, Star };
enum class Blend { Normal, Add, Screen };

struct Settings {
    double fps = 30.0;
    double startTime = 0.0;
    double emissionEndTime = -1.0; // Negative means no end. End is exclusive.
    double rate = 500.0;          // Requested births/sec across the whole mask.
    double life = 2.4;            // Seconds, fixed for this reference renderer.
    double size = 1.6;            // Diameter in output pixels before variation.
    double speed = 0.0;           // Output pixels/sec.
    double directionDegrees = 0; // 0 = right; 90 = down.
    double spreadDegrees = 360;  // Full angular spread centered on direction.
    double damping = 0.0;        // Per-second exponential decay constant.
    double fadeIn = 0.10;        // Fraction of particle lifetime, independently.
    double fadeOut = 0.40;       // Fraction of particle lifetime, independently.
    double twinkleStrength = 0.30; // 0 disables twinkle, preserving life envelope.
    double twinkleHz = 1.4;
    double glow = 0.12;          // 0 leaves the crisp core/star fully visible.
    double opacity = 0.85;
    std::uint32_t seed = 1;
    Emission emission = Emission::Alpha;
    Shape shape = Shape::Point;
    Blend blend = Blend::Add;
    bool sourceColor = true;
    Pixel baseColor{0.55f, 0.72f, 1.0f, 1.0f};
    int maskGridLimit = 192;     // Largest edge of approximate emission CDF.
    std::size_t maxParticles = 100000;
};

struct Particle {
    double x = 0, y = 0;
    double velocityX = 0, velocityY = 0;
    double size = 0, opacity = 0;
    double age = 0, life = 0, birthTime = 0;
    Pixel color; // Straight RGB sampled at birth; color.a is not particle opacity.
    bool star = false;
    std::uint64_t id = 0;
};

struct RenderStats {
    std::size_t requestedParticles = 0;
    std::size_t generatedParticles = 0;
    std::size_t sourceFrames = 0;
    bool truncated = false;
    bool invalidInput = false;
};

// Integral of exp(-damping*t) from 0 to age, used for displacement.
double displacementFactor(double damping, double age);
double lifeEnvelope(double age, double life, double fadeIn, double fadeOut);

// Samples a normalized point with bilinear filtering, unpremultiplying when
// necessary. Different source/mask/map dimensions use the same normalized space.
Pixel sampleStraight(const ImageView& image, double u, double v);

std::vector<Particle> sampleParticles(const Settings& settings, double time,
    int outputWidth, int outputHeight, const FrameProvider& provider,
    RenderStats* stats = nullptr);

// Clears only the visible output pixels, then produces premultiplied RGBA.
// Alpha is always in [0,1]; Add and Normal preserve HDR RGB above alpha/1.
// Screen uses display-range source RGB. Source compositing is the host's job.
RenderStats render(const Settings& settings, double time,
    const MutableImageView& output, const FrameProvider& provider);

constexpr int presetCount = 6;
Settings preset(int index);
const char* presetName(int index);

} // namespace particlelab
