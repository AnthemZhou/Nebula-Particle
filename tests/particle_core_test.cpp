#include "../src/particle_core.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace particlelab;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(double a, double b, double tolerance=1e-9) { return std::abs(a-b) <= tolerance; }

struct MovingSource {
    static constexpr int width=80, height=24;
    std::vector<Pixel> source = std::vector<Pixel>(width*height);
    std::vector<Pixel> mask = std::vector<Pixel>(width*height);
    std::vector<Pixel> color = std::vector<Pixel>(width*height);
    bool operator()(double t, SourceFrame& frame) {
        std::fill(source.begin(), source.end(), Pixel{});
        std::fill(mask.begin(), mask.end(), Pixel{});
        const int left = t < .5 ? 8 : 58;
        const Pixel tint = t < .5 ? Pixel{1,0,0,1} : Pixel{0,0,1,1};
        std::fill(color.begin(), color.end(), tint);
        for (int y=8; y<16; ++y) for (int x=left; x<left+9; ++x) {
            source[y*width+x] = {0,1,0,1};
            mask[y*width+x] = {1,1,1,1};
        }
        frame.source = {source.data(),width,height,0,false};
        frame.emissionMask = {mask.data(),width,height,0,false};
        frame.colorMap = {color.data(),width,height,0,false};
        return true;
    }
};

Settings testSettings() {
    Settings s;
    s.fps=20; s.rate=100; s.life=2; s.size=2; s.speed=0;
    s.fadeIn=0; s.fadeOut=0; s.twinkleStrength=0; s.glow=0;
    s.emission=Emission::ExternalLuminance;
    return s;
}

void testDampingAndEnvelope() {
    require(close(displacementFactor(0,2),2), "Zero damping must preserve ballistic travel");
    require(close(displacementFactor(std::log(2.0),1),.5/std::log(2.0)),
        "One-second half-life must integrate to the analytic displacement");
    require(displacementFactor(5,2) < displacementFactor(.5,2), "More damping must shorten travel");
    require(close(displacementFactor(1e-12,2),2,1e-8), "Tiny damping must not lose precision");
    require(lifeEnvelope(0,2,.2,.3)==0, "Fade-in must begin transparent");
    require(lifeEnvelope(0,2,0,.3)==1, "Zero fade-in must appear immediately");
    require(lifeEnvelope(2,2,0,0)==0, "Expired particles must be absent even without fade-out");
    require(lifeEnvelope(-.1,2,0,0)==0, "Unborn particles must be absent");
    require(lifeEnvelope(.5,0,0,0)==0, "Zero lifetime must not divide by zero");
    require(lifeEnvelope(.2,1,.1,.3) > lifeEnvelope(.95,1,.1,.3), "Fade-out must reduce opacity");
}

void testBirthColorAndTrail() {
    MovingSource source;
    Settings s=testSettings();
    RenderStats stats;
    auto particles=sampleParticles(s,.95,80,24,std::ref(source),&stats);
    require(!particles.empty(), "Moving source should emit");
    std::size_t left=0,right=0;
    for(const auto& p:particles) {
        if(p.birthTime<.5) {
            ++left;
            require(p.x<22, "Old particles must stay at the historical left-hand source");
            require(p.color.r>.99f && p.color.b<.01f && p.color.g<.01f,
                "Old particles must carry historical map red, not current blue or source green");
        } else {
            ++right;
            require(p.x>52, "New particles must appear at the source's current right position");
            require(p.color.b>.99f && p.color.r<.01f, "New particles must use blue birth map");
        }
    }
    require(left>20 && right>20, "Both historical and current emission regions must be represented");
    require(stats.sourceFrames <= 20, "Provider must be called by frame, not for each particle");
    s.emissionEndTime=.5;
    particles=sampleParticles(s,.95,80,24,std::ref(source));
    require(std::all_of(particles.begin(),particles.end(),[](const Particle& p){return p.birthTime<.5;}),
        "Stopping emission must leave only already-born particles");
}

void testOutOfOrderAndDampingTrajectory() {
    MovingSource source;
    Settings s=testSettings();
    s.speed=30; s.directionDegrees=0; s.spreadDegrees=0; s.damping=2;
    auto a=sampleParticles(s,.95,80,24,std::ref(source));
    (void)sampleParticles(s,1.7,80,24,std::ref(source));
    (void)sampleParticles(s,.15,80,24,std::ref(source));
    auto b=sampleParticles(s,.95,80,24,std::ref(source));
    require(a.size()==b.size(), "Seeking must not change particle population");
    for(std::size_t i=0;i<a.size();++i) {
        require(a[i].id==b[i].id && a[i].x==b[i].x && a[i].opacity==b[i].opacity &&
            a[i].color.r==b[i].color.r, "Out-of-order renders must be identical");
        require(a[i].velocityX<=30 && a[i].velocityX>0 && close(a[i].velocityY,0),
            "Rightward damped velocities must remain rightward and decrease");
    }
    s.damping=0;
    auto ballistic=sampleParticles(s,.95,80,24,std::ref(source));
    for(std::size_t i=0;i<a.size();++i)
        require(ballistic[i].x>=a[i].x, "Damped particles must not outrun identical ballistic births");
}

void testSamplingAndRendering() {
    Pixel premult[]={{.25f,.125f,0,.5f}};
    const auto color=sampleStraight({premult,1,1,0,true},.5,.5);
    require(close(color.r,.5) && close(color.g,.25) && close(color.a,.5),
        "Premultiplied image input must be unpremultiplied before birth color sampling");
    Settings s=testSettings(); s.emission=Emission::FullFrame; s.sourceColor=false;
    s.baseColor={2,1,.5f,1}; s.blend=Blend::Add; s.rate=1000; s.size=3;
    constexpr int width=24,height=24,stride=28;
    const Pixel sentinel{8,7,6,5};
    std::vector<Pixel> image(stride*height,sentinel);
    const auto stats=render(s,.8,{image.data(),width,height,stride},{});
    require(stats.generatedParticles>0, "Full-frame emission must work without input footage");
    bool visible=false,hdr=false;
    for(int y=0;y<height;++y)for(int x=0;x<stride;++x) {
        const auto& p=image[y*stride+x];
        if(x>=width) {
            require(p.r==sentinel.r && p.a==sentinel.a, "Renderer must not overwrite row padding");
        }else{
            require(std::isfinite(p.r) && p.a>=0 && p.a<=1, "Output must be finite with bounded alpha");
            visible |= p.a>0; hdr |= p.r>p.a;
        }
    }
    require(visible, "Zero glow must retain the crisp particle core");
    require(hdr, "Additive output must preserve HDR premultiplied RGB above alpha");
    s.rate=0;
    render(s,.8,{image.data(),width,height,stride},{});
    require(image[0].a==0 && image[0].r==0, "Zero rate must clear stale output");
}

void testInvalidAndLimits() {
    Settings s=testSettings(); MovingSource source;
    require(sampleParticles(s,-1,80,24,std::ref(source)).empty(), "Before emission start must be empty");
    s.emission=Emission::FullFrame; s.sourceColor=false; s.maxParticles=12;
    RenderStats stats;
    auto p=sampleParticles(s,1,80,24,{},&stats);
    require(p.size()==12 && stats.truncated, "Resource cap must be reported explicitly");
    s.rate=std::numeric_limits<double>::quiet_NaN();
    p=sampleParticles(s,1,80,24,{},&stats);
    require(p.empty() && stats.invalidInput, "NaN rate must be rejected");
    s=testSettings(); s.fps=0;
    p=sampleParticles(s,1,80,24,{},&stats);
    require(p.empty() && stats.invalidInput, "Zero frame rate must be rejected");
}
} // namespace

int main() {
    try {
        testDampingAndEnvelope();
        testBirthColorAndTrail();
        testOutOfOrderAndDampingTrajectory();
        testSamplingAndRendering();
        testInvalidAndLimits();
        std::cout << "particle_core: all tests passed\n";
    } catch(const std::exception& error) {
        std::cerr << "particle_core test failure: " << error.what() << '\n';
        return 1;
    }
}
