#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "plugin_config.h"
#include "AE_General.r"

resource 'PiPL' (16000) {
    {
        Kind { AEEffect },
        Name { PA_NAME },
        Category { PA_CATEGORY },
        CodeMacARM64 { PA_ENTRY_POINT },
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { PA_VERSION },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags { PA_OUT_FLAGS },
        AE_Effect_Global_OutFlags_2 { PA_OUT_FLAGS2 },
        AE_Effect_Match_Name { PA_MATCH_NAME },
        AE_Reserved_Info { 0 },
        AE_Effect_Support_URL { PA_SUPPORT_URL }
    }
};
