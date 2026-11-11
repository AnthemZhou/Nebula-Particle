#ifndef NEBULA_PARTICLE_PLUGIN_CONFIG_H
#define NEBULA_PARTICLE_PLUGIN_CONFIG_H

/* Shared by C++ and Rez. Keep this header free of C/C++ declarations. */
#define PA_NAME "Nebula Particle"
/* Keep the original match name so existing AEP effects continue to resolve. */
#define PA_MATCH_NAME "ParticleAtelier.Particles"
#define PA_CATEGORY "Nebula Particle"
#define PA_ENTRY_POINT "EffectMain"
#define PA_SUPPORT_URL ""

/* PF_VERSION(0, 5, 0, PF_Stage_RELEASE, 0), checked against May 2023 SDK. */
#define PA_VERSION 165376
#define PA_VERSION_STRING "0.5.0"

/* DEEP_COLOR_AWARE | WIDE_TIME_INPUT | NON_PARAM_VARY. */
#define PA_OUT_FLAGS 0x02000006
/* PARAM_GROUP_START_COLLAPSED_FLAG; no MFR/SmartFX/GPU claims yet. */
#define PA_OUT_FLAGS2 0x00000008

#endif
