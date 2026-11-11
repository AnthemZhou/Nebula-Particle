#include "particle_core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace particlelab {
namespace {
constexpr double pi = 3.1415926535897932384626433832795;

double finiteOr(double value, double fallback) {
    return std::isfinite(value) ? value : fallback;
}
double unit(double value) { return std::clamp(finiteOr(value, 0.0), 0.0, 1.0); }
double smooth(double value) {
    const double x = unit(value);
    return x * x * (3.0 - 2.0 * x);
}
float nonnegative(float value) {
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}
std::uint64_t hash(std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
double random(std::uint64_t id, std::uint32_t seed, std::uint64_t stream) {
    const auto h = hash(id ^ (std::uint64_t(seed) << 32) ^ hash(stream));
    return double(h >> 11) * (1.0 / 9007199254740992.0);
}
template <class View> std::size_t stride(const View& image) {
    return image.rowStridePixels ? image.rowStridePixels : std::size_t(image.width);
}
template <class View> bool validImage(const View& image) {
    return image.pixels && image.width > 0 && image.height > 0 &&
        stride(image) >= std::size_t(image.width) &&
        stride(image) <= std::numeric_limits<std::size_t>::max() / std::size_t(image.height);
}
Pixel clean(Pixel p) {
    p.r = nonnegative(p.r); p.g = nonnegative(p.g); p.b = nonnegative(p.b);
    p.a = float(unit(p.a));
    return p;
}
Pixel interpolate(const Pixel& a, const Pixel& b, double t) {
    const float k = float(t), j = 1.0f - k;
    return {a.r*j+b.r*k, a.g*j+b.g*k, a.b*j+b.b*k, a.a*j+b.a*k};
}

struct EmissionGrid {
    int width = 0, height = 0;
    std::vector<double> cumulative;
    double total = 0;
};

double emissionWeight(const ImageView& image, Emission mode, double u, double v) {
    const Pixel p = sampleStraight(image, u, v);
    if (mode == Emission::Alpha) return p.a;
    // Luminance is a spatial probability weight, normalized across the mask.
    // Multiplying by alpha prevents invisible RGB from emitting particles.
    return unit((p.r * 0.2126 + p.g * 0.7152 + p.b * 0.0722) * p.a);
}

EmissionGrid makeGrid(const ImageView& mask, Emission mode, int gridLimit) {
    EmissionGrid grid;
    if (!validImage(mask)) return grid;
    const double scale = std::min(1.0,
        double(std::clamp(gridLimit, 8, 512)) / std::max(mask.width, mask.height));
    grid.width = std::max(1, int(std::ceil(mask.width * scale)));
    grid.height = std::max(1, int(std::ceil(mask.height * scale)));
    grid.cumulative.reserve(std::size_t(grid.width) * grid.height);
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            // Four samples reduce dropped thin edges compared with one center.
            double weight = 0;
            for (double oy : {0.25, 0.75})
                for (double ox : {0.25, 0.75})
                    weight += emissionWeight(mask, mode,
                        (x + ox) / grid.width, (y + oy) / grid.height);
            grid.total += weight * 0.25;
            grid.cumulative.push_back(grid.total);
        }
    }
    return grid;
}

bool birthPoint(const EmissionGrid& grid, const ImageView& mask, Emission mode,
    std::uint64_t id, std::uint32_t seed, double& u, double& v) {
    if (mode == Emission::FullFrame) {
        u = random(id, seed, 1); v = random(id, seed, 2); return true;
    }
    if (!(grid.total > 0) || grid.cumulative.empty()) return false;
    const double target = random(id, seed, 3) * grid.total;
    auto selected = std::upper_bound(grid.cumulative.begin(), grid.cumulative.end(), target);
    const auto index = std::min(std::size_t(selected - grid.cumulative.begin()),
        grid.cumulative.size() - 1);
    const int gx = int(index % std::size_t(grid.width));
    const int gy = int(index / std::size_t(grid.width));
    // Avoid emitting outside a thin silhouette merely because its grid cell has
    // nonzero weight. The CDF remains an intentional low-resolution approximation.
    for (std::uint64_t attempt = 0; attempt < 8; ++attempt) {
        u = (gx + random(id, seed, 10 + attempt*2)) / grid.width;
        v = (gy + random(id, seed, 11 + attempt*2)) / grid.height;
        if (emissionWeight(mask, mode, u, v) > 1e-5) return true;
    }
    for (double oy : {0.25, 0.75}) {
        for (double ox : {0.25, 0.75}) {
            u = (gx + ox) / grid.width; v = (gy + oy) / grid.height;
            if (emissionWeight(mask, mode, u, v) > 1e-5) return true;
        }
    }
    return false;
}

void blendPixel(Pixel& destination, const Pixel& color, double coverage, Blend blend) {
    const float a = float(unit(coverage));
    if (a <= 0) return;
    const float inverse = 1 - a;
    const auto channel = [=](float d, float c) {
        c = nonnegative(c);
        double value = 0;
        if (blend == Blend::Add) value = double(d) + double(c) * a;
        if (blend == Blend::Screen) {
            const float s = std::clamp(c, 0.0f, 1.0f) * a;
            value = double(s) + double(d) * (1 - s);
        }
        if (blend == Blend::Normal) value = double(c)*a + double(d)*inverse;
        return float(std::clamp(value, 0.0, double(std::numeric_limits<float>::max())));
    };
    destination.r = channel(destination.r, color.r);
    destination.g = channel(destination.g, color.g);
    destination.b = channel(destination.b, color.b);
    destination.a = std::clamp(a + destination.a*inverse, 0.0f, 1.0f);
}

void drawParticle(const Particle& p, const Settings& settings, const MutableImageView& output) {
    if (!(p.opacity > 0) || !(p.size > 0) || !std::isfinite(p.x) || !std::isfinite(p.y)) return;
    const double radius = std::max(0.2, p.size * 0.5);
    const double glow = unit(settings.glow);
    const bool isStar = settings.shape == Shape::Star && p.star;
    const double extent = std::min(2048.0,
        std::max(radius + 1, radius * (isStar ? 4.0 : 1.0) + 1) + (glow > 0 ? radius * 3 : 0));
    if (p.x + extent < 0 || p.y + extent < 0 ||
        p.x - extent > output.width || p.y - extent > output.height) return;
    const int x0 = int(std::max(0.0, std::floor(p.x - extent)));
    const int y0 = int(std::max(0.0, std::floor(p.y - extent)));
    const int x1 = int(std::min(double(output.width - 1), std::ceil(p.x + extent)));
    const int y1 = int(std::min(double(output.height - 1), std::ceil(p.y + extent)));
    const double haloSigma = std::max(0.7, radius * 1.3);
    for (int y = y0; y <= y1; ++y) {
        Pixel* row = output.pixels + std::size_t(y)*stride(output);
        for (int x = x0; x <= x1; ++x) {
            const double dx = x + 0.5 - p.x, dy = y + 0.5 - p.y;
            const double distanceSquared = dx*dx + dy*dy;
            double core = unit(radius + 0.5 - std::sqrt(distanceSquared));
            if (isStar) {
                const double vertical = unit(1 - std::abs(dx)/(radius*.45+.25)) *
                    std::pow(unit(1 - std::abs(dy)/(radius*4)), 1.8);
                const double horizontal = unit(1 - std::abs(dy)/(radius*.45+.25)) *
                    std::pow(unit(1 - std::abs(dx)/(radius*3)), 1.8);
                core = std::max(core, std::max(vertical, horizontal));
            }
            const double halo = glow > 0 ?
                glow * 0.18 * std::exp(-distanceSquared/(2*haloSigma*haloSigma)) : 0;
            blendPixel(row[x], p.color, p.opacity * std::min(1.0, core + halo), settings.blend);
        }
    }
}
} // namespace

double displacementFactor(double damping, double age) {
    if (!std::isfinite(age) || age <= 0) return 0;
    damping = std::max(0.0, finiteOr(damping, 0.0));
    if (damping < 1e-9) return age;
    return -std::expm1(-damping * age) / damping;
}

double lifeEnvelope(double age, double life, double fadeIn, double fadeOut) {
    if (!std::isfinite(age) || !std::isfinite(life) || life <= 0 || age < 0 || age >= life) return 0;
    const double u = age/life, in = unit(fadeIn), out = unit(fadeOut);
    const double a = in > 0 ? smooth(u/in) : 1;
    const double b = out > 0 ? smooth((1-u)/out) : 1;
    return a*b;
}

Pixel sampleStraight(const ImageView& image, double u, double v) {
    if (!validImage(image) || !std::isfinite(u) || !std::isfinite(v)) return {};
    const double x = unit(u)*std::max(0, image.width-1);
    const double y = unit(v)*std::max(0, image.height-1);
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    const int x1 = std::min(x0+1, image.width-1), y1 = std::min(y0+1, image.height-1);
    const Pixel* row0 = image.pixels + std::size_t(y0)*stride(image);
    const Pixel* row1 = image.pixels + std::size_t(y1)*stride(image);
    Pixel p = clean(interpolate(interpolate(clean(row0[x0]), clean(row0[x1]), x-x0),
        interpolate(clean(row1[x0]), clean(row1[x1]), x-x0), y-y0));
    if (image.premultiplied) {
        if (p.a > 1e-7f) { p.r /= p.a; p.g /= p.a; p.b /= p.a; }
        else { p.r = 0; p.g = 0; p.b = 0; }
    }
    return clean(p);
}

std::vector<Particle> sampleParticles(const Settings& settings, double time,
    int outputWidth, int outputHeight, const FrameProvider& provider, RenderStats* stats) {
    RenderStats result;
    std::vector<Particle> particles;
    const auto finish = [&]() {
        result.generatedParticles = particles.size();
        if (stats) *stats = result;
    };
    const double fps = settings.fps, rate = settings.rate, life = settings.life;
    if (outputWidth <= 0 || outputHeight <= 0 || !std::isfinite(time) ||
        !std::isfinite(fps) || fps <= 0 || !std::isfinite(rate) || rate < 0 ||
        !std::isfinite(life) || life <= 0 || !std::isfinite(settings.startTime)) {
        result.invalidInput = true; finish(); return particles;
    }
    if (rate == 0 || time < settings.startTime || settings.maxParticles == 0) {
        finish(); return particles;
    }
    const double end = settings.emissionEndTime >= 0 && std::isfinite(settings.emissionEndTime) ?
        settings.emissionEndTime : std::numeric_limits<double>::infinity();
    if (end <= settings.startTime) { finish(); return particles; }
    const double latest = std::min(time, end);
    const double earliest = std::max(time-life, settings.startTime);
    if (latest < earliest) { finish(); return particles; }
    // Birth i always occurs at start+(i+0.5)/rate. IDs do not depend on the
    // requested frame, the visitation order, the mask shape, or render size.
    const long double firstValue = std::max(0.0L,
        std::ceil((static_cast<long double>(earliest)-settings.startTime)*rate - .5L));
    const long double lastValue = std::floor((static_cast<long double>(latest)-settings.startTime)*rate - .5L);
    constexpr long double maxExactID = 9007199254740991.0L;
    if (firstValue > maxExactID || lastValue > maxExactID || !std::isfinite(firstValue) ||
        !std::isfinite(lastValue)) {
        result.invalidInput = true; finish(); return particles;
    }
    if (lastValue < firstValue || lastValue < 0) { finish(); return particles; }
    std::uint64_t first = std::uint64_t(firstValue), last = std::uint64_t(lastValue);
    const std::uint64_t count = last-first+1;
    result.requestedParticles = std::size_t(std::min<std::uint64_t>(count,
        std::numeric_limits<std::size_t>::max()));
    if (count > settings.maxParticles) {
        result.truncated = true;
        first = last - std::uint64_t(settings.maxParticles) + 1;
    }
    particles.reserve(std::size_t(last-first+1));
    const double damping = std::max(0.0, finiteOr(settings.damping, 0.0));
    const double speed = finiteOr(settings.speed, 0.0);
    const double direction = finiteOr(settings.directionDegrees, 0.0);
    const double spread = std::clamp(finiteOr(settings.spreadDegrees, 0.0), 0.0, 360.0);
    const double size = std::max(0.0, finiteOr(settings.size, 0.0));
    const double opacity = unit(settings.opacity), twinkle = unit(settings.twinkleStrength);
    const double hz = std::max(0.0, finiteOr(settings.twinkleHz, 0.0));
    SourceFrame sourceFrame;
    ImageView mask, color;
    EmissionGrid grid;
    double previousFrame = std::numeric_limits<double>::quiet_NaN();
    bool frameAvailable = false;
    for (std::uint64_t id = first; id <= last; ++id) {
        const double birth = settings.startTime + (double(id)+.5)/rate;
        const double age = time-birth;
        if (birth >= end || age < 0 || age >= life) continue;
        const double frame = std::floor(birth*fps + 1e-8);
        if (!std::isfinite(frame)) {
            result.invalidInput = true; finish(); return particles;
        }
        if (frame != previousFrame) {
            sourceFrame = {};
            const bool needsSource = settings.emission != Emission::FullFrame || settings.sourceColor;
            frameAvailable = needsSource && provider ? provider(frame/fps, sourceFrame) : false;
            if (needsSource && provider) ++result.sourceFrames;
            previousFrame = frame;
            mask = validImage(sourceFrame.emissionMask) ? sourceFrame.emissionMask : sourceFrame.source;
            color = validImage(sourceFrame.colorMap) ? sourceFrame.colorMap : sourceFrame.source;
            grid = settings.emission == Emission::FullFrame ? EmissionGrid{} :
                makeGrid(mask, settings.emission, settings.maskGridLimit);
        }
        if (!frameAvailable && settings.emission != Emission::FullFrame) continue;
        double u = 0, v = 0;
        if (!birthPoint(grid, mask, settings.emission, id, settings.seed, u, v)) continue;
        const double angle = (direction + (random(id, settings.seed, 31)-.5)*spread)*pi/180;
        const double vx = speed*std::cos(angle), vy = speed*std::sin(angle);
        const double travel = displacementFactor(damping, age);
        const double lifeGain = lifeEnvelope(age, life, settings.fadeIn, settings.fadeOut);
        const double phase = random(id, settings.seed, 32)*2*pi;
        const double wave = .5+.5*std::sin(age*hz*2*pi+phase);
        const double twinkleGain = hz > 0 ? 1-twinkle+twinkle*wave*wave*wave : 1;
        Pixel birthColor = clean(settings.baseColor);
        if (settings.sourceColor && frameAvailable && validImage(color)) birthColor = sampleStraight(color, u, v);
        Particle p;
        p.id = id; p.birthTime = birth; p.age = age; p.life = life;
        p.x = u*outputWidth + vx*travel; p.y = v*outputHeight + vy*travel;
        p.velocityX = vx*std::exp(-damping*age); p.velocityY = vy*std::exp(-damping*age);
        p.size = size*(.6+std::pow(random(id, settings.seed, 33), 3)*1.3);
        p.opacity = opacity*lifeGain*twinkleGain*(.65+.35*random(id, settings.seed, 34));
        p.color = birthColor; p.star = random(id, settings.seed, 35) > .90;
        particles.push_back(p);
    }
    finish();
    return particles;
}

RenderStats render(const Settings& settings, double time,
    const MutableImageView& output, const FrameProvider& provider) {
    RenderStats result;
    if (!validImage(output)) { result.invalidInput = true; return result; }
    for (int y = 0; y < output.height; ++y)
        std::fill_n(output.pixels+std::size_t(y)*stride(output), output.width, Pixel{});
    const auto particles = sampleParticles(settings, time, output.width, output.height, provider, &result);
    for (const auto& particle : particles) drawParticle(particle, settings, output);
    return result;
}

const char* presetName(int index) {
    static constexpr const char* names[presetCount] = {
        "Surface Sparkle", "Fine Dust Trail", "Star Trail",
        "Gentle Drift", "Outward Sparks", "Background Dust"
    };
    return names[std::clamp(index, 0, presetCount-1)];
}

Settings preset(int index) {
    Settings s;
    switch (std::clamp(index, 0, presetCount-1)) {
    case 0:
        s.rate=180; s.life=1.25; s.size=1.8; s.shape=Shape::Star;
        s.twinkleStrength=.55; s.twinkleHz=1.7; s.fadeIn=.18; s.fadeOut=.50;
        break;
    case 1:
        s.rate=650; s.life=2.4; s.size=1.2; s.shape=Shape::Dust;
        s.glow=0; s.twinkleStrength=.15; s.fadeIn=.04; s.fadeOut=.7;
        break;
    case 2:
        s.rate=480; s.life=2.0; s.size=1.7; s.shape=Shape::Star;
        s.glow=.08; s.twinkleStrength=.4; s.fadeIn=.04; s.fadeOut=.65;
        break;
    case 3:
        s.rate=350; s.life=3.0; s.size=1.4; s.speed=24;
        s.directionDegrees=-70; s.spreadDegrees=50; s.damping=.35;
        s.shape=Shape::Point; s.glow=.08;
        break;
    case 4:
        s.rate=550; s.life=1.8; s.size=1.7; s.speed=85;
        s.spreadDegrees=360; s.damping=1.7; s.shape=Shape::Star;
        s.glow=.08; s.fadeIn=.02; s.fadeOut=.7;
        break;
    case 5:
        s.rate=280; s.life=4; s.size=1.2; s.speed=8;
        s.directionDegrees=-90; s.spreadDegrees=80; s.emission=Emission::FullFrame;
        s.sourceColor=false; s.shape=Shape::Dust; s.glow=0;
        s.twinkleStrength=.15; s.fadeIn=.20; s.fadeOut=.40;
        break;
    }
    return s;
}

} // namespace particlelab
