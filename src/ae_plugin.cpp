#define PF_DEEP_COLOR_AWARE 1
#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectUI.h"
#include "plugin_config.h"
#include "particle_core.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <vector>
#include <dlfcn.h>

extern "C" DllExport PF_Err EffectMain(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*);

namespace {
using namespace particlelab;
// Runtime indices follow the PF_ADD_* call order below. They are NOT disk IDs:
// reordering the UI must never change a saved parameter's serialized identity.
enum Param {
    Input, Preset, Density, Size, Lifetime,
    Sources, Source, Channel, ExternalMask, ShowSource, SourcesEnd,
    Appearance, ShapeParam, BlendParam, ColorMode, ColorMap, Tint, Opacity, Glow, AppearanceEnd,
    Animation, FadeIn, FadeOut, TwinkleAmount, TwinkleRate, AnimationEnd,
    Advanced,
    Motion, Speed, Direction, Spread, Damping, MotionEnd,
    Timing, Prewarm, EndEmission, Seed, TimingEnd,
    DevelopmentTools, ReloadPreset, DevelopmentToolsEnd,
    AdvancedEnd, ParamCount
};

// Serialized IDs shipped in 0.1 are frozen, including the old Advanced topic.
// Only append new IDs. Names and numeric IDs preserve old AEP values/keyframes
// while the runtime index enum above is free to reflect the new UI hierarchy.
enum DiskID {
    IdPreset = 1,
    IdSource = 2,
    IdChannel = 3,
    IdColorMap = 4,
    IdDensity = 5,
    IdSize = 6,
    IdLifetime = 7,
    IdAdvanced = 8,
    IdExternalMask = 9,
    IdShowSource = 10,
    IdShapeParam = 11,
    IdBlendParam = 12,
    IdColorMode = 13,
    IdTint = 14,
    IdSpeed = 15,
    IdDirection = 16,
    IdSpread = 17,
    IdDamping = 18,
    IdFadeIn = 19,
    IdFadeOut = 20,
    IdTwinkleAmount = 21,
    IdTwinkleRate = 22,
    IdGlow = 23,
    IdOpacity = 24,
    IdSeed = 25,
    IdPrewarm = 26,
    IdEndEmission = 27,
    IdReloadPreset = 28,
    IdAdvancedEnd = 29,
    // New layout-only topics in 0.2. No old ID has been reused.
    IdSources = 30,
    IdSourcesEnd = 31,
    IdAppearance = 32,
    IdAppearanceEnd = 33,
    IdAnimation = 34,
    IdAnimationEnd = 35,
    IdMotion = 36,
    IdMotionEnd = 37,
    IdTiming = 38,
    IdTimingEnd = 39,
    IdDevelopmentTools = 40,
    IdDevelopmentToolsEnd = 41
};

static_assert(PA_VERSION == PF_VERSION(0, 5, 0, PF_Stage_RELEASE, 0),
    "C++ and PiPL version constants must match the SDK encoding");

Settings resourcePreset(int index) {
    Settings s = preset(index);
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void*>(&EffectMain), &info) || !info.dli_fname) return s;
    const auto path = std::filesystem::path(info.dli_fname).parent_path().parent_path() / "Resources" / "presets.ini";
    std::ifstream input;
    if (const char* userHome = std::getenv("HOME")) {
        const auto support = std::filesystem::path(userHome) / "Library" / "Application Support";
        input.open(support / "Nebula Particle" / "presets.ini");
        // Legacy editable resources remain usable before the installer migrates
        // them. Prefer the new location and never overwrite either file here.
        if (!input.is_open()) {
            input.clear();
            input.open(support / "Particle Atelier" / "presets.ini");
        }
    }
    if (!input.is_open()) { input.clear(); input.open(path); }
    std::string line;
    int section = -1;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') { std::istringstream parser(line.substr(1)); parser >> section; continue; }
        if (section != index) continue;
        const auto equal = line.find('=');
        if (equal == std::string::npos) continue;
        const auto key = line.substr(0, equal);
        double value = 0;
        std::istringstream parser(line.substr(equal + 1));
        if (!(parser >> value) || !std::isfinite(value)) continue;
        if (key == "rate") s.rate = std::clamp(value, 0.0, 10000.0);
        else if (key == "size") s.size = std::clamp(value, 0.1, 40.0);
        else if (key == "life") s.life = std::clamp(value, 0.04, 10.0);
        else if (key == "speed") s.speed = std::clamp(value, 0.0, 1000.0);
        else if (key == "direction") s.directionDegrees = value;
        else if (key == "spread") s.spreadDegrees = std::clamp(value, 0.0, 360.0);
        else if (key == "damping") s.damping = std::clamp(value, 0.0, 20.0);
        else if (key == "fadeIn") s.fadeIn = std::clamp(value, 0.0, 1.0);
        else if (key == "fadeOut") s.fadeOut = std::clamp(value, 0.0, 1.0);
        else if (key == "twinkle") s.twinkleStrength = std::clamp(value, 0.0, 1.0);
        else if (key == "twinkleHz") s.twinkleHz = std::clamp(value, 0.0, 10.0);
        else if (key == "glow") s.glow = std::clamp(value, 0.0, 1.0);
        else if (key == "opacity") s.opacity = std::clamp(value, 0.0, 1.0);
    }
    return s;
}

PF_Err paramsSetup(PF_InData* in_data, PF_OutData* out_data) {
    PF_ParamDef def{};
    const auto s = preset(0);
    // The four everyday controls remain visible. Everything else is grouped.
    PF_ADD_POPUPX("Preset", 6, 1, "Surface sparkle|Fine dust trail|Star trail|Gentle drift|Outward sparks|Background dust (Full frame)", PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY, IdPreset);
    PF_ADD_FLOAT_SLIDERX("Density (births/sec)", 0, 10000, 0, 2000, s.rate, PF_Precision_INTEGER, 0, PF_ParamFlag_CANNOT_TIME_VARY, IdDensity);
    PF_ADD_FLOAT_SLIDERX("Size (px)", 0.1, 40, 0.1, 6, s.size, PF_Precision_TENTHS, 0, 0, IdSize);
    PF_ADD_FLOAT_SLIDERX("Lifetime (sec)", 0.04, 10, 0.1, 5, s.life, PF_Precision_HUNDREDTHS, 0, PF_ParamFlag_CANNOT_TIME_VARY, IdLifetime);

    PF_ADD_TOPICX("Sources", PF_ParamFlag_START_COLLAPSED, IdSources);
    AEFX_CLR_STRUCT(def); PF_ADD_LAYER("Source (None = this layer)", PF_LayerDefault_NONE, IdSource);
    PF_ADD_POPUPX("Emit from", 4, 1, "Alpha|Luminance|External luminance|Full frame", PF_ParamFlag_CANNOT_TIME_VARY, IdChannel);
    AEFX_CLR_STRUCT(def); PF_ADD_LAYER("External emission mask", PF_LayerDefault_NONE, IdExternalMask);
    PF_ADD_CHECKBOXX("Show source", TRUE, 0, IdShowSource);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(IdSourcesEnd);

    PF_ADD_TOPICX("Appearance", PF_ParamFlag_START_COLLAPSED, IdAppearance);
    PF_ADD_POPUPX("Particle shape", 3, int(s.shape)+1, "Dust|Point|Star", 0, IdShapeParam);
    PF_ADD_POPUPX("Particle blend", 3, int(s.blend)+1, "Normal|Add|Screen", 0, IdBlendParam);
    PF_ADD_POPUPX("Color mode", 2, 1, "Birth color|Tint", 0, IdColorMode);
    AEFX_CLR_STRUCT(def); PF_ADD_LAYER("Color map (None = source)", PF_LayerDefault_NONE, IdColorMap);
    AEFX_CLR_STRUCT(def); PF_ADD_COLOR("Tint color", 140, 184, 255, IdTint);
    PF_ADD_FLOAT_SLIDERX("Opacity (%)", 0, 100, 0, 100, s.opacity*100, PF_Precision_TENTHS, 0, 0, IdOpacity);
    PF_ADD_FLOAT_SLIDERX("Glow (%)", 0, 100, 0, 100, s.glow*100, PF_Precision_TENTHS, 0, 0, IdGlow);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(IdAppearanceEnd);

    PF_ADD_TOPICX("Animation", PF_ParamFlag_START_COLLAPSED, IdAnimation);
    PF_ADD_FLOAT_SLIDERX("Fade in (% lifetime)", 0, 100, 0, 100, s.fadeIn*100, PF_Precision_TENTHS, 0, 0, IdFadeIn);
    PF_ADD_FLOAT_SLIDERX("Fade out (% lifetime)", 0, 100, 0, 100, s.fadeOut*100, PF_Precision_TENTHS, 0, 0, IdFadeOut);
    PF_ADD_FLOAT_SLIDERX("Twinkle strength (%)", 0, 100, 0, 100, s.twinkleStrength*100, PF_Precision_TENTHS, 0, 0, IdTwinkleAmount);
    PF_ADD_FLOAT_SLIDERX("Twinkle frequency (Hz)", 0, 10, 0, 5, s.twinkleHz, PF_Precision_TENTHS, 0, 0, IdTwinkleRate);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(IdAnimationEnd);

    PF_ADD_TOPICX("Advanced", PF_ParamFlag_START_COLLAPSED, IdAdvanced);
    PF_ADD_TOPICX("Motion", PF_ParamFlag_START_COLLAPSED, IdMotion);
    PF_ADD_FLOAT_SLIDERX("Initial speed (px/sec)", 0, 1000, 0, 250, s.speed, PF_Precision_TENTHS, 0, PF_ParamFlag_CANNOT_TIME_VARY, IdSpeed);
    PF_ADD_FLOAT_SLIDERX("Direction (0=right)", -3600, 3600, -180, 180, s.directionDegrees, PF_Precision_TENTHS, 0, PF_ParamFlag_CANNOT_TIME_VARY, IdDirection);
    PF_ADD_FLOAT_SLIDERX("Spread (degrees)", 0, 360, 0, 360, s.spreadDegrees, PF_Precision_TENTHS, 0, PF_ParamFlag_CANNOT_TIME_VARY, IdSpread);
    PF_ADD_FLOAT_SLIDERX("Damping (per sec)", 0, 20, 0, 5, s.damping, PF_Precision_HUNDREDTHS, 0, PF_ParamFlag_CANNOT_TIME_VARY, IdDamping);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(IdMotionEnd);

    PF_ADD_TOPICX("Timing", PF_ParamFlag_START_COLLAPSED, IdTiming);
    PF_ADD_CHECKBOXX("Prewarm at frame zero", TRUE, PF_ParamFlag_CANNOT_TIME_VARY, IdPrewarm);
    PF_ADD_FLOAT_SLIDERX("Stop emission (-1=never)", -1, 86400, -1, 30, -1, PF_Precision_HUNDREDTHS, 0, PF_ParamFlag_CANNOT_TIME_VARY, IdEndEmission);
    PF_ADD_FLOAT_SLIDERX("Random seed", 0, 1000000, 0, 1000, 1, PF_Precision_INTEGER, 0, PF_ParamFlag_CANNOT_TIME_VARY, IdSeed);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(IdTimingEnd);

    PF_ADD_TOPICX("Development tools", PF_ParamFlag_START_COLLAPSED, IdDevelopmentTools);
    PF_ADD_BUTTON("Development", "Reload current preset", 0, PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY, IdReloadPreset);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(IdDevelopmentToolsEnd);
    AEFX_CLR_STRUCT(def); PF_END_TOPIC(IdAdvancedEnd);
    out_data->num_params = ParamCount;
    return PF_Err_NONE;
}

void applyPreset(PF_ParamDef* params[], PF_OutData* out_data) {
    const auto s = resourcePreset(params[Preset]->u.pd.value - 1);
    const auto set = [&](Param p, double v) { params[p]->u.fs_d.value = v; params[p]->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE; };
    const auto pop = [&](Param p, int v) { params[p]->u.pd.value = v; params[p]->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE; };
    set(Density,s.rate); set(Size,s.size); set(Lifetime,s.life);
    set(Speed,s.speed); set(Direction,s.directionDegrees); set(Spread,s.spreadDegrees); set(Damping,s.damping);
    set(FadeIn,s.fadeIn*100); set(FadeOut,s.fadeOut*100); set(TwinkleAmount,s.twinkleStrength*100); set(TwinkleRate,s.twinkleHz);
    set(Glow,s.glow*100); set(Opacity,s.opacity*100); pop(ShapeParam,int(s.shape)+1); pop(BlendParam,int(s.blend)+1);
    pop(ColorMode,s.sourceColor ? 1 : 2);
    // Source, mask, color-map, emission channel, seed and timing remain connected.
    out_data->out_flags |= PF_OutFlag_FORCE_RERENDER | PF_OutFlag_REFRESH_UI;
}

Pixel readPixel(const PF_LayerDef& world, int x, int y) {
    if (!world.data || world.width <= 0 || world.height <= 0) return {};
    x = std::clamp(x, 0, int(world.width)-1); y = std::clamp(y, 0, int(world.height)-1);
    const auto* row = reinterpret_cast<const unsigned char*>(world.data) + y * world.rowbytes;
    if (PF_WORLD_IS_DEEP(&world)) {
        const auto p = reinterpret_cast<const PF_Pixel16*>(row)[x];
        const float k = 1.0f / PF_MAX_CHAN16;
        return {p.red*k,p.green*k,p.blue*k,p.alpha*k};
    }
    const auto p = reinterpret_cast<const PF_Pixel8*>(row)[x];
    const float k = 1.0f / PF_MAX_CHAN8;
    return {p.red*k,p.green*k,p.blue*k,p.alpha*k};
}

ImageView copyAnalysisImage(const PF_LayerDef& world, std::vector<Pixel>& storage) {
    if (!world.data || world.width <= 0 || world.height <= 0) return {};
    const double scale = std::min(1.0, 384.0 / std::max(world.width,world.height));
    const int w = std::max(1,int(std::lround(world.width*scale)));
    const int h = std::max(1,int(std::lround(world.height*scale)));
    storage.resize(std::size_t(w)*h);
    for (int y=0;y<h;++y) for (int x=0;x<w;++x) {
        storage[std::size_t(y)*w+x] = readPixel(world,int((x+.5)*world.width/w),int((y+.5)*world.height/h));
    }
    return {storage.data(),w,h,0,true};
}

struct CheckedLayer {
    PF_InData* in;
    PF_ParamDef parameter{};
    bool acquired=false;
    explicit CheckedLayer(PF_InData* input):in(input) {}
    CheckedLayer(const CheckedLayer&)=delete;
    CheckedLayer& operator=(const CheckedLayer&)=delete;
    PF_Err get(int index) {
        const auto error=PF_CHECKOUT_PARAM(in,index,in->current_time,in->time_step,in->time_scale,&parameter);
        acquired=error==PF_Err_NONE;
        return error;
    }
    bool present() const { return acquired && parameter.u.ld.data && parameter.u.ld.width>0 && parameter.u.ld.height>0; }
    ~CheckedLayer() { if(acquired) PF_CHECKIN_PARAM(in,&parameter); }
};

PF_Err render(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output) {
    if (!output || !output->data || output->width<=0 || output->height<=0 || in_data->time_scale<=0) return PF_Err_BAD_CALLBACK_PARAM;
    Settings s;
    const auto value = [&](Param p) { return double(params[p]->u.fs_d.value); };
    const double previewScale = in_data->downsample_x.den ? double(in_data->downsample_x.num)/in_data->downsample_x.den : 1.0;
    const double step = std::abs(double(in_data->time_step));
    s.fps = step>0 ? double(in_data->time_scale)/step : 30.0;
    s.rate=value(Density); s.life=value(Lifetime); s.size=value(Size)*previewScale;
    s.speed=value(Speed)*previewScale; s.directionDegrees=value(Direction); s.spreadDegrees=value(Spread); s.damping=value(Damping);
    s.fadeIn=value(FadeIn)/100; s.fadeOut=value(FadeOut)/100;
    s.twinkleStrength=value(TwinkleAmount)/100; s.twinkleHz=value(TwinkleRate); s.glow=value(Glow)/100; s.opacity=value(Opacity)/100;
    s.seed=std::uint32_t(value(Seed)); s.emissionEndTime=value(EndEmission);
    s.startTime=params[Prewarm]->u.bd.value ? -s.life : 0;
    s.shape=Shape(std::clamp(int(params[ShapeParam]->u.pd.value)-1,0,2));
    s.blend=Blend(std::clamp(int(params[BlendParam]->u.pd.value)-1,0,2));
    s.emission=Emission(std::clamp(int(params[Channel]->u.pd.value)-1,0,3));
    s.sourceColor=params[ColorMode]->u.pd.value==1;
    const auto tint=params[Tint]->u.cd.value;
    s.baseColor={tint.red/255.f,tint.green/255.f,tint.blue/255.f,1};
    // Basic PF_Cmd_RENDER only pre-populates params[0]'s image. A selected
    // auxiliary layer can still have null data in params[] until checked out.
    CheckedLayer currentSource(in_data),currentMask(in_data),currentColor(in_data);
    PF_Err currentError=currentSource.get(Source);
    if(currentError) return currentError;
    if(s.emission==Emission::ExternalLuminance) { currentError=currentMask.get(ExternalMask); if(currentError) return currentError; }
    if(s.sourceColor) { currentError=currentColor.get(ColorMap); if(currentError) return currentError; }
    const int sourceIndex=currentSource.present() ? Source : Input;
    const bool hasMask=currentMask.present();
    const bool hasColor=currentColor.present();
    if (s.emission==Emission::ExternalLuminance && !hasMask) {
        std::snprintf(out_data->return_msg,sizeof(out_data->return_msg),"Choose Sources > External emission mask, or change Emit from.");
        out_data->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
        return PF_Err_BAD_CALLBACK_PARAM;
    }
    PF_Err checkoutError=PF_Err_NONE;
    std::vector<Pixel> sourcePixels,maskPixels,colorPixels;
    const auto checkout = [&](int index, A_long at, std::vector<Pixel>& pixels, ImageView& view) {
        PF_ParamDef checked{};
        const PF_Err e=PF_CHECKOUT_PARAM(in_data,index,at,in_data->time_step,in_data->time_scale,&checked);
        if (e) { checkoutError=e; return false; }
        // Every checkout is paired even if copying allocates/throws.
        struct Checkin { PF_InData* in; PF_ParamDef* p; ~Checkin(){PF_CHECKIN_PARAM(in,p);} } guard{in_data,&checked};
        view=copyAnalysisImage(checked.u.ld,pixels);
        return true;
    };
    const FrameProvider provider = [&](double birth, SourceFrame& f) {
        if (checkoutError) return false;
        checkoutError=PF_ABORT(in_data);
        if (checkoutError) return false;
        const auto at=A_long(std::llround(std::max(0.0,birth)*in_data->time_scale));
        if (!checkout(sourceIndex,at,sourcePixels,f.source)) return false;
        if (hasMask && s.emission==Emission::ExternalLuminance && !checkout(ExternalMask,at,maskPixels,f.emissionMask)) return false;
        if (hasColor && s.sourceColor && !checkout(ColorMap,at,colorPixels,f.colorMap)) return false;
        return true;
    };
    const int w=output->width,h=output->height;
    std::vector<Pixel> result(std::size_t(w)*h);
    const auto stats=particlelab::render(s,double(in_data->current_time)/in_data->time_scale,{result.data(),w,h,0},provider);
    if (checkoutError) return checkoutError;
    if (stats.invalidInput) return PF_Err_BAD_CALLBACK_PARAM;
    const auto& beauty=currentSource.present() ? currentSource.parameter.u.ld : params[Input]->u.ld;
    const bool show=params[ShowSource]->u.bd.value != 0;
    const auto channel=[](float v,int max) { return std::lround(std::clamp(v,0.f,1.f)*max); };
    for (int y=0;y<h;++y) {
        if ((y&31)==0) { const auto e=PF_ABORT(in_data); if(e) return e; }
        auto* row=reinterpret_cast<unsigned char*>(output->data)+y*output->rowbytes;
        for (int x=0;x<w;++x) {
            auto p=result[std::size_t(y)*w+x];
            if (show) {
                const auto b=readPixel(beauty,int((x+.5)*beauty.width/w),int((y+.5)*beauty.height/h));
                p={p.r+b.r*(1-p.a),p.g+b.g*(1-p.a),p.b+b.b*(1-p.a),p.a+b.a*(1-p.a)};
            }
            if (PF_WORLD_IS_DEEP(output)) {
                auto& dst=reinterpret_cast<PF_Pixel16*>(row)[x];
                dst={A_u_short(channel(p.a,PF_MAX_CHAN16)),A_u_short(channel(p.r,PF_MAX_CHAN16)),A_u_short(channel(p.g,PF_MAX_CHAN16)),A_u_short(channel(p.b,PF_MAX_CHAN16))};
            } else {
                auto& dst=reinterpret_cast<PF_Pixel8*>(row)[x];
                dst={A_u_char(channel(p.a,255)),A_u_char(channel(p.r,255)),A_u_char(channel(p.g,255)),A_u_char(channel(p.b,255))};
            }
        }
    }
    return PF_Err_NONE;
}
} // namespace

extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr, PF_PluginDataCB2 callback, SPBasicSuite*, const char*, const char*) {
    PF_Err result=PF_Err_INVALID_CALLBACK;
    result=PF_REGISTER_EFFECT_EXT2(inPtr,callback,PA_NAME,PA_MATCH_NAME,PA_CATEGORY,AE_RESERVED_INFO,PA_ENTRY_POINT,PA_SUPPORT_URL);
    return result;
}

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra) {
    try {
        switch(cmd) {
            case PF_Cmd_ABOUT:
                std::snprintf(out_data->return_msg,sizeof(out_data->return_msg),PA_NAME " " PA_VERSION_STRING "\rGrouped controls. CPU 2D preview, 8/16 bpc. Optical flow, GPU and depth are not implemented yet.");
                break;
            case PF_Cmd_GLOBAL_SETUP:
                out_data->my_version=PA_VERSION; out_data->out_flags=PA_OUT_FLAGS; out_data->out_flags2=PA_OUT_FLAGS2;
                break;
            case PF_Cmd_PARAMS_SETUP: return paramsSetup(in_data,out_data);
            case PF_Cmd_RENDER: return render(in_data,out_data,params,output);
            case PF_Cmd_USER_CHANGED_PARAM: {
                const auto* changed=static_cast<PF_UserChangedParamExtra*>(extra);
                if(changed && (changed->param_index==Preset || changed->param_index==ReloadPreset)) applyPreset(params,out_data);
                break;
            }
            default: break;
        }
    } catch (const std::bad_alloc&) { return PF_Err_OUT_OF_MEMORY; }
      catch (...) { return PF_Err_INTERNAL_STRUCT_DAMAGED; }
    return PF_Err_NONE;
}
