#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include "mi_sys.h"
#include "mi_ai.h"
#include "mi_ao.h"

#include "base_types.h"
#include "CSpotterSDKApi.h"
#include "CSpotterSDKApi_Const.h"

#include "st_common.h"
#include "VoiceDetect.h"
#include "list.h"
#include "cmdqueue.h"
#include "char_conversion.h"


#define AD_LOG(fmt, args...) {printf("\033[1;34m");printf("%s[%d]:", __FUNCTION__, __LINE__);printf(fmt, ##args);printf("\033[0m");}

typedef struct
{
    unsigned char*      pCmd;
    int                 cmdLen;

    int                 aliveTime;  // millisecond
    struct timeval 	    timestamp;

    struct list_head    stList;
} ST_Voice_Cmd_S;

typedef struct
{
    struct list_head stListHead;
    pthread_mutex_t stMutex;

    HANDLE hCSpotter;

    pthread_t pt;
    MI_BOOL bRunFlag;

    MI_U32 u32TotalFrames;
    MI_BOOL bInit;
} ST_Voice_Mng_S;

typedef struct
{
    unsigned char*      pFrameData;
    int                 frameLen;

    struct list_head    stList;
} ST_Voice_Frame_S;

typedef struct
{
    struct list_head stListHead;
    pthread_mutex_t stMutex;
} ST_Cmd_Mng_S;

typedef struct
{
    pthread_t pt;
    MI_BOOL bRunFlag;
} ST_Audio_In_S;

// trainning word list opt
#define SAVE_TRAINING_WORD(idx, word, listHead)    \
{   \
    TrainingWordData_t *pTrainingWordData = (TrainingWordData_t*)malloc(sizeof(TrainingWordData_t));    \
    memset(pTrainingWordData, 0, sizeof(TrainingWordData_t)); \
    INIT_LIST_HEAD(&pTrainingWordData->wordList); \
    pTrainingWordData->index = idx;    \
    strcpy(pTrainingWordData->szWord, word);    \
    list_add_tail(&pTrainingWordData->wordList, &listHead);   \
}

#define DEL_TRAINING_WORD(word, listHead)    \
{   \
    TrainingWordData_t *pos = NULL;  \
    TrainingWordData_t *posN = NULL;  \
    list_for_each_entry_safe(pos, posN, &listHead, wordList)   \
    {   \
        if (!strcmp(pos->szWord, word))   \
        {   \
            list_del(&pos->wordList); \
            free(pos);  \
            break;  \
        }   \
    }   \
}

#define FIND_TRAINING_WORD(word, listHead, pIndex)      \
{   \
    TrainingWordData_t *pos = NULL;  \
    list_for_each_entry(pos, &listHead, wordList)   \
    {   \
        if (!strcmp(pos->szWord, word))   \
        {   \
            *pIndex = pos->index; \
            break;  \
        }   \
    }   \
}

#define CLEAR_TRAINING_WORD_LIST(listHead) \
{   \
    TrainingWordData_t *pos = NULL;  \
    TrainingWordData_t *posN = NULL;  \
    list_for_each_entry_safe(pos, posN, &listHead, wordList)   \
    {   \
        list_del(&pos->wordList); \
        free(pos);  \
    }   \
}

typedef struct
{
    list_t wordList;
    int index;
    char szWord[62];
}TrainingWordData_t;


#define AUDIO_PT_NUMBER_FRAME (1024)
#define AUDIO_SAMPLE_RATE (E_MI_AUDIO_SAMPLE_RATE_16000)
#define AUDIO_SOUND_MODE (E_MI_AUDIO_SOUND_MODE_MONO)

#define AUDIO_AO_DEV_ID_LINE_OUT 0
#define AUDIO_AO_DEV_ID_I2S_OUT  1

#define AUDIO_AI_DEV_ID_AMIC_IN   0
#define AUDIO_AI_DEV_ID_DMIC_IN   1
#define AUDIO_AI_DEV_ID_I2S_IN    2

#define AI_DEV_ID (AUDIO_AI_DEV_ID_AMIC_IN)
//#define AI_DEV_ID (AUDIO_AI_DEV_ID_DMIC_IN)
#define AO_DEV_ID (AUDIO_AO_DEV_ID_LINE_OUT)

#define CSPOTTER_PATH_PREFIX        "/customer/res/CSpotter"
#define CSPOTTER_LIB_PATH           "/customer/res/CSpotter/lib"
#define CSPOTTER_DATA_PATH          "/customer/res/CSpotter/data"

#define MAX_FRAME_QUEUE_DEPTH       64

static list_t g_wordListHead;
static TrainingWordData_t g_oldTrainWord = {{NULL, NULL}, -1, ""};
static int start_analyze = 0;
static int analyze_init = 0;
static int start_audioIn = 0;
static int do_analyze = 0;

static ST_Voice_Mng_S g_stVoiceMng;
static ST_Audio_In_S g_stAudioInMng;
static ST_Cmd_Mng_S g_stCmdMng;

void _SSTAR_InitFrameQueue(void)
{
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;

    INIT_LIST_HEAD(&pstVoiceMng->stListHead);
    pthread_mutex_init(&pstVoiceMng->stMutex, NULL);
    pstVoiceMng->u32TotalFrames = 0;
}

ST_Voice_Frame_S* _SSTAR_PopFrameFromQueue(void)
{
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;
    ST_Voice_Frame_S *pstVoiceFrame = NULL;
    struct list_head *pListPos = NULL;

    pthread_mutex_lock(&pstVoiceMng->stMutex);
    if (list_empty(&pstVoiceMng->stListHead))
    {
        pthread_mutex_unlock(&pstVoiceMng->stMutex);
        return NULL;
    }

    pListPos = pstVoiceMng->stListHead.next;

    pstVoiceMng->stListHead.next = pListPos->next;
    pListPos->next->prev = pListPos->prev;
    pthread_mutex_unlock(&pstVoiceMng->stMutex);

    pstVoiceFrame = list_entry(pListPos, ST_Voice_Frame_S, stList);

    return pstVoiceFrame;
}

void _SSTAR_PutFrameToQueue(MI_AUDIO_Frame_t *pstAudioFrame)
{
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;
    struct list_head *pListPos = NULL;
	struct list_head *pListPosN = NULL;
    ST_Voice_Frame_S *pstVoiceFrame = NULL;
    int queueDepth = 0;

    if (pstAudioFrame == NULL)
    {
        return;
    }

    // calc depth
    pthread_mutex_lock(&pstVoiceMng->stMutex);
    list_for_each_safe(pListPos, pListPosN, &pstVoiceMng->stListHead)
	{
		queueDepth ++;
	}
    pthread_mutex_unlock(&pstVoiceMng->stMutex);

    // printf("%s %d, u32TotalFrames:%d, queueDepth:%d\n", __func__, __LINE__, pstVoiceMng->u32TotalFrames, queueDepth);

    // max depth check
    if (queueDepth >= MAX_FRAME_QUEUE_DEPTH)
    {
        // pop frame
        pstVoiceFrame = _SSTAR_PopFrameFromQueue();
        if (pstVoiceFrame != NULL)
        {
            if (pstVoiceFrame->pFrameData != NULL)
            {
                free(pstVoiceFrame->pFrameData);
                pstVoiceFrame->pFrameData = NULL;
            }

            free(pstVoiceFrame);
            pstVoiceFrame = NULL;
        }
    }

    pstVoiceFrame = (ST_Voice_Frame_S *)malloc(sizeof(ST_Voice_Frame_S));
    if (pstVoiceFrame == NULL)
    {
        ST_ERR("malloc error, not enough memory\n");
        goto END;
    }
    memset(pstVoiceFrame, 0, sizeof(ST_Voice_Frame_S));

    pstVoiceFrame->pFrameData = (unsigned char *)malloc(pstAudioFrame->u32Len);
    if (pstVoiceFrame->pFrameData == NULL)
    {

        ST_ERR("malloc error, not enough memory\n");
        goto END;
    }
    memset(pstVoiceFrame->pFrameData, 0, pstAudioFrame->u32Len);
    memcpy(pstVoiceFrame->pFrameData, pstAudioFrame->apVirAddr[0], pstAudioFrame->u32Len);
    pstVoiceFrame->frameLen = pstAudioFrame->u32Len;

    // ST_DBG("pFrameData:%p, frameLen:%d\n", pstVoiceFrame->pFrameData, pstVoiceFrame->frameLen);

    pthread_mutex_lock(&pstVoiceMng->stMutex);
    list_add_tail(&pstVoiceFrame->stList, &pstVoiceMng->stListHead);
    pstVoiceMng->u32TotalFrames ++;
    pthread_mutex_unlock(&pstVoiceMng->stMutex);

    return;
END:
    if (pstVoiceFrame->pFrameData != NULL)
    {
        free(pstVoiceFrame->pFrameData);
        pstVoiceFrame->pFrameData = NULL;
    }

    if (pstVoiceFrame)
    {
        free(pstVoiceFrame);
        pstVoiceFrame = NULL;
    }
}

void _SSTAR_ClearFrameQueue(void)
{
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;
    ST_Voice_Frame_S *pstVoiceFrame = NULL;
    struct list_head *pHead = &pstVoiceMng->stListHead;
	struct list_head *pListPos = NULL;
	struct list_head *pListPosN = NULL;

    pthread_mutex_lock(&pstVoiceMng->stMutex);
	list_for_each_safe(pListPos, pListPosN, pHead)
	{
		pstVoiceFrame = list_entry(pListPos, ST_Voice_Frame_S, stList);
		list_del(pListPos);

        if (pstVoiceFrame->pFrameData)
        {
            free(pstVoiceFrame->pFrameData);
        }

		free(pstVoiceFrame);
	}
    pthread_mutex_unlock(&pstVoiceMng->stMutex);
}

void _SSTAR_InitCMDQueue(void)
{
    ST_Cmd_Mng_S *pstCmdMng = &g_stCmdMng;

    INIT_LIST_HEAD(&pstCmdMng->stListHead);
    pthread_mutex_init(&pstCmdMng->stMutex, NULL);
}

void _SSTAR_PutCMDToQueue(unsigned char *pCmd, int cmdLen)
{
    ST_Cmd_Mng_S *pstCmdMng = &g_stCmdMng;
    struct list_head *pListPos = NULL;
	struct list_head *pListPosN = NULL;
    ST_Voice_Cmd_S *pstVoiceCmd = NULL;

    if (pCmd == NULL || cmdLen <= 0)
    {
        return;
    }

    pstVoiceCmd = (ST_Voice_Cmd_S *)malloc(sizeof(ST_Voice_Cmd_S));
    if (pstVoiceCmd == NULL)
    {
        ST_ERR("malloc error, not enough memory\n");
        goto END;
    }
    memset(pstVoiceCmd, 0, sizeof(ST_Voice_Cmd_S));

    pstVoiceCmd->pCmd = (unsigned char *)malloc(cmdLen + 1);
    if (pstVoiceCmd->pCmd == NULL)
    {
        ST_ERR("malloc error, not enough memory\n");
        goto END;
    }

    printf("%s %d, %p, %p\n", __func__, __LINE__, pstVoiceCmd, pstVoiceCmd->pCmd);
    memset(pstVoiceCmd->pCmd, 0, cmdLen + 1);
    memcpy(pstVoiceCmd->pCmd, pCmd, cmdLen);
    pstVoiceCmd->cmdLen = cmdLen + 1;
    pstVoiceCmd->aliveTime = 4000;
    gettimeofday(&pstVoiceCmd->timestamp, NULL);

    pthread_mutex_lock(&pstCmdMng->stMutex);
    list_add_tail(&pstVoiceCmd->stList, &pstCmdMng->stListHead);
    pthread_mutex_unlock(&pstCmdMng->stMutex);

    return;
END:
    if (pstVoiceCmd->pCmd != NULL)
    {
        free(pstVoiceCmd->pCmd);
        pstVoiceCmd->pCmd = NULL;
    }

    if (pstVoiceCmd)
    {
        free(pstVoiceCmd);
        pstVoiceCmd = NULL;
    }
}

void _SSTAR_ClearCMDQueue(void)
{
    ST_Cmd_Mng_S *pstCmdMng = &g_stCmdMng;
    ST_Voice_Cmd_S *pstVoiceCmd = NULL;

    ST_Voice_Frame_S *pstVoiceFrame = NULL;
    struct list_head *pHead = &pstCmdMng->stListHead;
    struct list_head *pListPos = NULL;
    struct list_head *pListPosN = NULL;

    pthread_mutex_lock(&pstCmdMng->stMutex);
    list_for_each_safe(pListPos, pListPosN, pHead)
    {
        pstVoiceCmd = list_entry(pListPos, ST_Voice_Cmd_S, stList);
        list_del(pListPos);

        if (pstVoiceCmd->pCmd)
        {
            free(pstVoiceCmd->pCmd);
        }

        free(pstVoiceCmd);
    }
    pthread_mutex_unlock(&pstCmdMng->stMutex);
}

static void *_SSTAR_AudioInGetDataProc_(void *pdata)
{
    MI_AUDIO_DEV AiDevId = AI_DEV_ID;
    MI_AI_CHN AiChn = 0;
    MI_AUDIO_Frame_t stAudioFrame;
    MI_S32 s32ToTalSize = 0;
    MI_S32 s32Ret = 0;
    FILE *pFile = NULL;
    char szFileName[64] = {0,};
    ST_Audio_In_S *pstAudioInMng = &g_stAudioInMng;

    _SSTAR_InitFrameQueue();
    ST_DBG("pid=%d\n", syscall(SYS_gettid));

    MI_SYS_ChnPort_t stChnPort;
    MI_S32 s32Fd = -1;
    fd_set read_fds;
    struct timeval TimeoutVal;

    memset(&stChnPort, 0, sizeof(MI_SYS_ChnPort_t));
    stChnPort.eModId = E_MI_MODULE_ID_AI;
    stChnPort.u32DevId = AiDevId;
    stChnPort.u32ChnId = AiChn;
    stChnPort.u32PortId = 0;
    s32Ret = MI_SYS_GetFd(&stChnPort, &s32Fd);

    if(MI_SUCCESS != s32Ret)
    {
        ST_ERR("MI_SYS_GetFd err:%x, chn:%d\n", s32Ret, AiChn);
        return NULL;
    }

    while(pstAudioInMng->bRunFlag)
    {
        FD_ZERO(&read_fds);
        FD_SET(s32Fd, &read_fds);

        TimeoutVal.tv_sec  = 1;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(s32Fd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if(s32Ret < 0)
        {
            ST_ERR("select failed!\n");
            //  usleep(10 * 1000);
            continue;
        }
        else if(s32Ret == 0)
        {
            ST_ERR("get audio in frame time out\n");
            //usleep(10 * 1000);
            continue;
        }
        else
        {
            if(FD_ISSET(s32Fd, &read_fds))
            {
                MI_AI_GetFrame(AiDevId, AiChn, &stAudioFrame, NULL, 128);//1024 / 8000 = 128ms
                if (0 == stAudioFrame.u32Len)
                {
                    usleep(10 * 1000);
                    continue;
                }

                _SSTAR_PutFrameToQueue(&stAudioFrame);

                MI_AI_ReleaseFrame(AiDevId,  AiChn, &stAudioFrame, NULL);
            }
        }
    }

    _SSTAR_ClearFrameQueue();

    return NULL;
}

static MI_S32 SSTAR_AudioInStart()
{
    ST_Audio_In_S *pstAudioInMng = &g_stAudioInMng;
    MI_S32 s32Ret = MI_SUCCESS, i;

    //Ai
    MI_AUDIO_DEV AiDevId = AI_DEV_ID;
    MI_AI_CHN AiChn = 0;
    MI_AUDIO_Attr_t stAiSetAttr;
    MI_SYS_ChnPort_t stAiChn0OutputPort0;
    MI_AI_VqeConfig_t stAiVqeConfig;

    //Ao
    MI_AUDIO_DEV AoDevId = AO_DEV_ID;
    MI_AO_CHN AoChn = 0;
    MI_S16 s16CompressionRatioInput[5] = {-70, -60, -30, 0, 0};
    MI_S16 s16CompressionRatioOutput[5] = {-70, -45, -18, 0, 0};


    //set ai attr
    memset(&stAiSetAttr, 0, sizeof(MI_AUDIO_Attr_t));
    stAiSetAttr.eBitwidth = E_MI_AUDIO_BIT_WIDTH_16;
    stAiSetAttr.eSamplerate = AUDIO_SAMPLE_RATE;
    stAiSetAttr.eSoundmode = E_MI_AUDIO_SOUND_MODE_MONO;
    stAiSetAttr.eWorkmode = E_MI_AUDIO_MODE_I2S_MASTER;
    //stAiSetAttr.eWorkmode = E_MI_AUDIO_MODE_I2S_SLAVE;
    stAiSetAttr.u32ChnCnt = 2;
    //stAiSetAttr.u32ChnCnt = 4;
    stAiSetAttr.u32FrmNum = 16;
    stAiSetAttr.u32PtNumPerFrm = AUDIO_PT_NUMBER_FRAME;

    //set ai output port depth
    memset(&stAiChn0OutputPort0, 0, sizeof(MI_SYS_ChnPort_t));
    stAiChn0OutputPort0.eModId = E_MI_MODULE_ID_AI;
    stAiChn0OutputPort0.u32DevId = AiDevId;
    stAiChn0OutputPort0.u32ChnId = AiChn;
    stAiChn0OutputPort0.u32PortId = 0;

    //ai vqe
    memset(&stAiVqeConfig, 0, sizeof(MI_AI_VqeConfig_t));
    stAiVqeConfig.bHpfOpen = FALSE;
    stAiVqeConfig.bAnrOpen = FALSE;
    stAiVqeConfig.bAgcOpen = TRUE;
    stAiVqeConfig.bEqOpen = FALSE;
    stAiVqeConfig.bAecOpen = FALSE;

    stAiVqeConfig.s32FrameSample = 128;
    stAiVqeConfig.s32WorkSampleRate = AUDIO_SAMPLE_RATE;

    //Hpf
    stAiVqeConfig.stHpfCfg.eMode = E_MI_AUDIO_ALGORITHM_MODE_USER;
    stAiVqeConfig.stHpfCfg.eHpfFreq = E_MI_AUDIO_HPF_FREQ_120;

    //Anr
    stAiVqeConfig.stAnrCfg.eMode= E_MI_AUDIO_ALGORITHM_MODE_USER;
    stAiVqeConfig.stAnrCfg.eNrSpeed = E_MI_AUDIO_NR_SPEED_LOW;
    stAiVqeConfig.stAnrCfg.u32NrIntensity = 5;            //[0, 30]
    stAiVqeConfig.stAnrCfg.u32NrSmoothLevel = 10;          //[0, 10]

    //Agc
    stAiVqeConfig.stAgcCfg.eMode = E_MI_AUDIO_ALGORITHM_MODE_USER;
    stAiVqeConfig.stAgcCfg.s32NoiseGateDb = -50;           //[-80, 0], NoiseGateDb disable when value = -80
    stAiVqeConfig.stAgcCfg.s32TargetLevelDb =   0;       //[-80, 0]
    stAiVqeConfig.stAgcCfg.stAgcGainInfo.s32GainInit = 1;  //[-20, 30]
    stAiVqeConfig.stAgcCfg.stAgcGainInfo.s32GainMax =  15; //[0, 30]
    stAiVqeConfig.stAgcCfg.stAgcGainInfo.s32GainMin = -5; //[-20, 30]
    stAiVqeConfig.stAgcCfg.u32AttackTime = 1;              //[1, 20]
    memcpy(stAiVqeConfig.stAgcCfg.s16Compression_ratio_input, s16CompressionRatioInput, sizeof(s16CompressionRatioInput));
    memcpy(stAiVqeConfig.stAgcCfg.s16Compression_ratio_output, s16CompressionRatioOutput, sizeof(s16CompressionRatioOutput));
    stAiVqeConfig.stAgcCfg.u32DropGainMax = 60;            //[0, 60]
    stAiVqeConfig.stAgcCfg.u32NoiseGateAttenuationDb = 10;  //[0, 100]
    stAiVqeConfig.stAgcCfg.u32ReleaseTime = 10;             //[1, 20]
    stAiVqeConfig.u32ChnNum = 1;
    //Eq
    stAiVqeConfig.stEqCfg.eMode = E_MI_AUDIO_ALGORITHM_MODE_USER;
    for (i = 0; i < sizeof(stAiVqeConfig.stEqCfg.s16EqGainDb) / sizeof(stAiVqeConfig.stEqCfg.s16EqGainDb[0]); i++)
    {
       stAiVqeConfig.stEqCfg.s16EqGainDb[i] = 5;
    }

    ExecFunc(MI_AI_SetPubAttr(AiDevId, &stAiSetAttr), MI_SUCCESS);
    ExecFunc(MI_AI_Enable(AiDevId), MI_SUCCESS);
    ExecFunc(MI_AI_EnableChn(AiDevId, AiChn), MI_SUCCESS);

#if 1
    ExecFunc(MI_AI_SetVqeVolume(AiDevId, 0, 9), MI_SUCCESS);
    //ExecFunc(MI_AI_SetVqeVolume(AiDevId, 0, 3), MI_SUCCESS);

    s32Ret = MI_AI_SetVqeAttr(AiDevId, AiChn, AoDevId, AoChn, &stAiVqeConfig);
    if (s32Ret != MI_SUCCESS)
    {
        ST_ERR("%#x\n", s32Ret);
    }
    ExecFunc(MI_AI_EnableVqe(AiDevId, AiChn), MI_SUCCESS);
#endif

    for (i = 0; i < stAiSetAttr.u32ChnCnt; i++)
    {
        stAiChn0OutputPort0.u32ChnId = i;
        ExecFunc(MI_SYS_SetChnOutputPortDepth(&stAiChn0OutputPort0, 4, 8), MI_SUCCESS);
    }

    pstAudioInMng->bRunFlag = TRUE;
    s32Ret = pthread_create(&pstAudioInMng->pt, NULL, _SSTAR_AudioInGetDataProc_, NULL);
    if(0 != s32Ret)
    {
         ST_ERR("create thread failed\n");
         return -1;
    }

    return MI_SUCCESS;
}

static MI_S32 SSTAR_AudioInStop()
{
    MI_AUDIO_DEV AiDevId = AI_DEV_ID;
    MI_AI_CHN AiChn = 0;
    ST_Audio_In_S *pstAudioInMng = &g_stAudioInMng;

    pstAudioInMng->bRunFlag = FALSE;
    pthread_join(pstAudioInMng->pt, NULL);

    ExecFunc(MI_AI_DisableVqe(AiDevId, AiChn), MI_SUCCESS);
    ExecFunc(MI_AI_DisableChn(AiDevId, AiChn), MI_SUCCESS);
    ExecFunc(MI_AI_Disable(AiDevId), MI_SUCCESS);

    return MI_SUCCESS;
}

static unsigned char pbyResult[256];

static void *_SSTAR_VoiceAnalyzeProc_(void *pdata)
{
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;
    ST_Voice_Frame_S *pstVoiceFrame = NULL;
    MI_S32 s32Ret = 0;
    VoiceAnalyzeCallback pfcCallback = (VoiceAnalyzeCallback)pdata;
    //unsigned char pbyResult[256];
    unsigned long msg[4];

    AD_LOG("Enter _SSTAR_VoiceAnalyzeProc_\n");

    if((s32Ret = CSpotter_Reset(pstVoiceMng->hCSpotter)) != CSPOTTER_SUCCESS)
    {
        ST_ERR("CSpotter_Reset:: Fail to start recognition ( %d )!\n", s32Ret);
        return NULL;
    }

    _SSTAR_InitCMDQueue();

    ST_DBG("pid=%d\n", syscall(SYS_gettid));

    while (pstVoiceMng->bRunFlag)
    {
        pstVoiceFrame = _SSTAR_PopFrameFromQueue();

        if (pstVoiceFrame == NULL)
        {
            usleep(1000*10);
            continue;
        }

        // reduce CSpotter_AddSample core dump
        //usleep(1000*10);

        // ST_DBG("pFrameData:%p, frameLen:%d\n", pstVoiceFrame->pFrameData, pstVoiceFrame->frameLen);
        s32Ret = CSpotter_AddSample(pstVoiceMng->hCSpotter, (short*)pstVoiceFrame->pFrameData, pstVoiceFrame->frameLen / 2);
        if(s32Ret == CSPOTTER_SUCCESS)
        {
            printf("Get result!\n");
            s32Ret = CSpotter_GetUTF8Result(pstVoiceMng->hCSpotter, NULL, pbyResult);
            if(s32Ret >= 0)
            {
                int index = -1;
                ST_DBG("Result:%s, s32Ret:%d, %d\n", pbyResult, s32Ret, strlen((const char*)pbyResult));

                // notify callback
                FIND_TRAINING_WORD((char *)pbyResult, g_wordListHead, &index);
                AD_LOG("training word is %s, index is %d\n", (char *)pbyResult, index);
                pfcCallback((char*)pbyResult, strlen((const char*)pbyResult));
                AD_LOG("callback exec success\n");
            }
        }

        if (pstVoiceFrame != NULL)
        {
            if (pstVoiceFrame->pFrameData != NULL)
            {
                free(pstVoiceFrame->pFrameData);
                pstVoiceFrame->pFrameData = NULL;
            }

            free(pstVoiceFrame);
            pstVoiceFrame = NULL;
        }

    }

    _SSTAR_ClearCMDQueue();

    return NULL;
}

static MI_S32 SSTAR_VoiceAnalyzeInit()
{
    unsigned long msg[4];
    HANDLE hCSpotter = NULL;
    int nErr = 0;
    int nCmdNum = 0;
    int i = 0;
    int ret = 0;
    char szEngineLibFile[128] = {0,};
	char szCommandBinFile[128] = {0,};
	char szLicenseBinFile[128] = {0,};
    char szCmdBuf[128] = {0,};
    char szCmdBufTemp[128] = {0,};
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;

    INIT_LIST_HEAD(&g_wordListHead);
    snprintf(szEngineLibFile, sizeof(szEngineLibFile) - 1, "%s/libNINJA.so", CSPOTTER_LIB_PATH);
    snprintf(szCommandBinFile, sizeof(szCommandBinFile) - 1, "%s/RecogCmd.bin", CSPOTTER_DATA_PATH);
    snprintf(szLicenseBinFile, sizeof(szLicenseBinFile) - 1, "%s/CybLicense.bin", CSPOTTER_PATH_PREFIX);
    system("date -s \"2017-12-06 20:49\"");
    hCSpotter = CSpotter_InitWithFiles(szEngineLibFile, szCommandBinFile, szLicenseBinFile, &nErr);
    if(hCSpotter == NULL)
    {
        ST_ERR("CSpotter_InitWithFiles fail, err:%d\n", nErr);
        return -1;
    }

    nCmdNum = CSpotter_GetCommandNumber(hCSpotter);

    AD_LOG("training list size is %d\n", nCmdNum);

    for (i = 0; i < nCmdNum; i ++)
    {
        memset(szCmdBuf, 0, sizeof(szCmdBuf));
        ret = CSpotter_GetUTF8Command(hCSpotter, i, (BYTE *)szCmdBuf);
        if (ret > 0)
        {
            // skip the same cmd
            if(strcmp(szCmdBuf, szCmdBufTemp) != 0)
            {
            	//AD_LOG("%s %d\n", szCmdBuf, nCmdNum);
            	AD_LOG("%s %d\n", szCmdBuf, i);
                memset(szCmdBufTemp, 0, sizeof(szCmdBufTemp));
                strcpy(szCmdBufTemp, szCmdBuf);

                // save to list
                //char cmdGb2312[100] = "";
                //utf8ToGb2312(cmdGb2312, 100, szCmdBuf, strlen(szCmdBuf));
                //printf("%s %d\n", cmdGb2312, strlen(szCmdBuf));
                
                SAVE_TRAINING_WORD(i, szCmdBuf, g_wordListHead);
            }
        }
    }

    pstVoiceMng->hCSpotter = hCSpotter;
    pstVoiceMng->bInit = TRUE;

    return MI_SUCCESS;
}

static MI_S32 SSTAR_VoiceAnalyzeDeInit()
{
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;
    if (TRUE == pstVoiceMng->bInit)
    {
        CSpotter_Release(pstVoiceMng->hCSpotter);
    }

    return MI_SUCCESS;
}

static MI_S32 SSTAR_VoiceAnalyzeStart(VoiceAnalyzeCallback pfnCallback)
{
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;

    pstVoiceMng->bRunFlag = TRUE;
    pthread_create(&pstVoiceMng->pt, NULL, _SSTAR_VoiceAnalyzeProc_, (void*)pfnCallback);

    return MI_SUCCESS;
}

static MI_S32 SSTAR_VoiceAnalyzeStop()
{
    ST_Voice_Mng_S *pstVoiceMng = &g_stVoiceMng;
    if (TRUE == pstVoiceMng->bRunFlag)
    {
        pstVoiceMng->bRunFlag = FALSE;
        pthread_join(pstVoiceMng->pt, NULL);
    }

    return MI_SUCCESS;
}

static MI_S32 SSTAR_VoiceLearnStart()
{
    return MI_SUCCESS;
}

static MI_S32 SSTAR_VoiceLearnStop()
{
    return MI_SUCCESS;
}

ST_Voice_Cmd_S* SSTAR_GetVoiceCMD(void)
{
    ST_Cmd_Mng_S *pstCmdMng = &g_stCmdMng;

    ST_Voice_Cmd_S *pstVoiceCMDNode = NULL;
    ST_Voice_Cmd_S *pstVoiceCMD = NULL;
    struct list_head *pListPos = NULL;
    struct timeval time_now;
    double timestamp = 0;

    pthread_mutex_lock(&pstCmdMng->stMutex);
    if (list_empty(&pstCmdMng->stListHead))
    {
        pthread_mutex_unlock(&pstCmdMng->stMutex);
        return NULL;
    }

    pListPos = pstCmdMng->stListHead.next;
    pstVoiceCMDNode = list_entry(pListPos, ST_Voice_Cmd_S, stList);

    pstVoiceCMD = (ST_Voice_Cmd_S *)malloc(sizeof(ST_Voice_Cmd_S));
    if (pstVoiceCMD == NULL)
    {
        ST_ERR("malloc error, not enough memory\n");
        pthread_mutex_unlock(&pstCmdMng->stMutex);
        return NULL;
    }

    pstVoiceCMD->pCmd = (unsigned char *)malloc(pstVoiceCMDNode->cmdLen);
    if (pstVoiceCMD->pCmd == NULL)
    {
        ST_ERR("malloc error, not enough memory\n");
        free(pstVoiceCMD);
        pthread_mutex_unlock(&pstCmdMng->stMutex);
        return NULL;
    }
    memset(pstVoiceCMD->pCmd, 0, pstVoiceCMDNode->cmdLen);

    memcpy(pstVoiceCMD->pCmd, pstVoiceCMDNode->pCmd, pstVoiceCMDNode->cmdLen);
    pstVoiceCMD->cmdLen = pstVoiceCMDNode->cmdLen;
    pstVoiceCMD->aliveTime = pstVoiceCMDNode->aliveTime;
    pstVoiceCMD->timestamp = pstVoiceCMDNode->timestamp;
    pstVoiceCMD->stList = pstVoiceCMDNode->stList;

    gettimeofday(&time_now, NULL);
    timestamp = (double)(time_now.tv_sec * 1000.0 + time_now.tv_usec / 1000) -
                (double)(pstVoiceCMDNode->timestamp.tv_sec * 1000.0 + pstVoiceCMDNode->timestamp.tv_usec / 1000);
    if (timestamp > pstVoiceCMDNode->aliveTime)
    {
        pstCmdMng->stListHead.next = pListPos->next;
        pListPos->next->prev = pListPos->prev;

        AD_LOG("%s %d del node, %p, %p, cmd:%s\n", __func__, __LINE__,  pstVoiceCMDNode,
            pstVoiceCMDNode->pCmd, pstVoiceCMDNode->pCmd);

        if (pstVoiceCMDNode->pCmd)
            free(pstVoiceCMDNode->pCmd);
        free(pstVoiceCMDNode);
    }
    pthread_mutex_unlock(&pstCmdMng->stMutex);

    return pstVoiceCMD;
}


MI_S32 SSTAR_VoiceDetectInit()
{
    INIT_LIST_HEAD(&g_wordListHead);
    return MI_SUCCESS;
}

MI_S32 SSTAR_VoiceDetectDeinit()
{
    if (start_analyze == 1)
    {
        if (do_analyze)
        {
            SSTAR_VoiceAnalyzeStop();
            do_analyze = 0;
        }

        if (start_audioIn)
        {
            SSTAR_AudioInStop();
            start_audioIn = 0;
            start_analyze = 0;
        }
    }

    if (analyze_init == 1)
    {
        SSTAR_VoiceAnalyzeDeInit();
        analyze_init = 0;
    }

    CLEAR_TRAINING_WORD_LIST(g_wordListHead);

    return MI_SUCCESS;
}

// return actual count.
MI_S32 SSTAR_VoiceDetectGetWordList(WordInfo_t *pWordList, int cnt)
{
    if (!pWordList)
        return 0;

    if (!analyze_init)
    {
        if (MI_SUCCESS == SSTAR_VoiceAnalyzeInit())
        {
            int i = 0;
            TrainingWordData_t *pos;

            analyze_init = 1;
            
            list_for_each_entry(pos, &g_wordListHead, wordList)
            {
                if (i < cnt)
                {
                    strcpy((pWordList+i)->szWord, pos->szWord);
                    pWordList->index = pos->index;
                    printf("Get WordList[%d]: %s\n", i, (pWordList+i)->szWord);
                    i++;
                }
                else
                    return i;
            }
        }
    }

    return 0;
}

MI_S32 SSTAR_VoiceDetectStart(VoiceAnalyzeCallback pfnCallback)
{
    if (!list_empty(&g_wordListHead))
    {
    	AD_LOG("ENTER detectStart >> start_analyze:%d start_audioIn:%d do_analyze:%d\n", start_analyze, start_audioIn, do_analyze);
    
        if (!start_analyze)
        {
            if (!start_audioIn)
            {
                if (MI_SUCCESS == SSTAR_AudioInStart())
                {
                    start_audioIn = 1;
                }
            }

            if (start_audioIn)
            {
                if (MI_SUCCESS == SSTAR_VoiceAnalyzeStart(pfnCallback))
                {
                    do_analyze = 1;
                    start_analyze = 1;
                    AD_LOG("detectStart SUCCESS >> start_analyze:%d start_audioIn:%d do_analyze:%d\n", start_analyze, start_audioIn, do_analyze);
                    return MI_SUCCESS;
                }
            }
        }
    }
    else
    	AD_LOG("please train word first\n");

    AD_LOG("detectStart FAIL >> start_analyze:%d start_audioIn:%d do_analyze:%d\n", start_analyze, start_audioIn, do_analyze);
    return -1;
}

void SSTAR_VoiceDetectStop()
{
	AD_LOG("ENTER detectStart >> start_analyze:%d start_audioIn:%d do_analyze:%d\n", start_analyze, start_audioIn, do_analyze);
    
    if (start_analyze == 1)
    {
        if (do_analyze)
        {
            if (MI_SUCCESS == SSTAR_VoiceAnalyzeStop())
            {
                do_analyze = 0;
            }
        }

        if (!do_analyze)
        {
            if (MI_SUCCESS == SSTAR_AudioInStop())
            {
                start_analyze = 0;
                start_audioIn = 0;
            }
        }

        AD_LOG("LEAVE detectStart >> start_analyze:%d start_audioIn:%d do_analyze:%d\n", start_analyze, start_audioIn, do_analyze);
    }
}

