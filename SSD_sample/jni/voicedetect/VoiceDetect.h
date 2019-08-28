#ifndef _ST_VOICE_H_
#define _ST_VOICE_H_

#ifdef __cplusplus
extern "C"{
#endif	// __cplusplus

#include "mi_common_datatype.h"

#define ExecFunc(_func_, _ret_) \
	do{ \
		MI_S32 s32Ret = MI_SUCCESS; \
		s32Ret = _func_; \
		if (s32Ret != _ret_) \
		{ \
			printf("[%s %d]exec function failed, error:%x\n", __func__, __LINE__, s32Ret); \
			return s32Ret; \
		} \
		else \
		{ \
			printf("[%s %d]exec function pass\n", __func__, __LINE__); \
		} \
} while(0)

typedef struct
{
    char szWord[64];
    int index;
} WordInfo_t;
typedef void* (*VoiceAnalyzeCallback)(char*, int);

MI_S32 SSTAR_VoiceDetectInit();
MI_S32 SSTAR_VoiceDetectDeinit();
MI_S32 SSTAR_VoiceDetectGetWordList(WordInfo_t *pWordList, int cnt);
MI_S32 SSTAR_VoiceDetectStart(VoiceAnalyzeCallback pfnCallback);
void SSTAR_VoiceDetectStop();


#ifdef __cplusplus
}
#endif	// __cplusplus

#endif //_ST_VOICE_H_
