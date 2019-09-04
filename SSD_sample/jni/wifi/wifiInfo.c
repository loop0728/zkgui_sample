/*
 * wifiInfo.c
 *
 *  Created on: 2019年8月15日
 *      Author: koda.xu
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SUPPORT_WLAN_MODULE
#include "wifiInfo.h"
#include "cJSON.h"


#define WIFI_SETTING_CFG	"/appconfigs/wifisetting.json"

static bool isWifiSupport = true;
static bool isWifiEnable = true;
static bool isConnected = false;
static WLAN_HANDLE wlanHdl = -1;
static MI_WLAN_ConnectParam_t stConnectParam;	// abandon
static cJSON *pRoot = NULL;
static cJSON *pWifi = NULL;
static cJSON *pSavedSsid = NULL;
static cJSON *pConnect = NULL;

static void *malloc_fn(size_t sz)
{
	return malloc(sz);
}

static void free_fn(void *ptr)
{
	free(ptr);
}

bool getWifiSupportStatus()
{
	return isWifiSupport;
}

void setWifiSupportStatus(bool enable)
{
	isWifiSupport = enable;
}

bool getWifiEnableStatus()
{
	return isWifiEnable;
}

void setWifiEnableStatus(bool enable)
{
	isWifiEnable = enable;
}

bool getConnectionStatus()
{
	return isConnected;
}

void setConnectionStatus(bool enable)
{
	isConnected = enable;
}

WLAN_HANDLE getWlanHandle()
{
	return wlanHdl;
}

void setWlanHandle(WLAN_HANDLE handle)
{
	wlanHdl = handle;
}

MI_WLAN_ConnectParam_t * getConnectParam()
{
	return &stConnectParam;
}

void saveConnectParam(MI_WLAN_ConnectParam_t *pConnParam)
{
	memset(&stConnectParam, 0, sizeof(MI_WLAN_ConnectParam_t));
	memcpy(&stConnectParam, pConnParam, sizeof(MI_WLAN_ConnectParam_t));
}

int initWifiConfig()
{
	FILE* fp = NULL;
	long long len = 0;
	char * pConfData = NULL;
	cJSON * root;
	cJSON * obj;
	cJSON * param;
	cJSON * item;
	cJSON_Hooks hooks;

	memset(&stConnectParam, 0, sizeof(MI_WLAN_ConnectParam_t));
	stConnectParam.eSecurity = E_MI_WLAN_SECURITY_WPA;
	stConnectParam.OverTimeMs = 5000;

	fp = fopen(WIFI_SETTING_CFG,"r+");
	if (!fp)
	{
		printf("should open json file first\n");
		return -1;
	}

	printf("open %s success\n", WIFI_SETTING_CFG);

	hooks.free_fn = free_fn;
	hooks.malloc_fn = malloc_fn;
	cJSON_InitHooks(&hooks);

	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	pConfData = (char *)malloc(len + 1);
	fseek(fp, 0, SEEK_SET);
	fread(pConfData, len, 1, fp);
	fclose(fp);
	fp = NULL;

	// read config
	cJSON_Minify(pConfData);
	root = cJSON_Parse(pConfData);
	if (!root)
		return -1;

	obj = cJSON_GetObjectItem(root, "wifi");
	if (!obj)
		return -1;

	printf("parse json success\n");
	item = cJSON_GetObjectItem(obj, "isSupport");
	if (item)
		isWifiSupport = cJSON_IsTrue(item);
	printf("isSupport: %d\n", isWifiSupport);

	item = cJSON_GetObjectItem(obj, "isEnable");
	if (item)
		isWifiEnable = cJSON_IsTrue(item);
	printf("isSupport: %d\n", isWifiEnable);

	param = cJSON_GetObjectItem(obj, "param");
	if (param)
	{
		item = cJSON_GetObjectItem(param, "id");
		if(item)
		{
			wlanHdl = (WLAN_HANDLE)atoi(cJSON_GetStringValue(item));
			item = cJSON_GetObjectItem(param, "ssid");
			strcpy((char*)stConnectParam.au8SSId, cJSON_GetStringValue(item));
			item = cJSON_GetObjectItem(param, "passwd");
			strcpy((char*)stConnectParam.au8Password, cJSON_GetStringValue(item));
		}
	}

	printf("isSupport:%d isEnable:%d id:%d ssid:%s passwd:%s\n", isWifiSupport, isWifiEnable, wlanHdl, (char*)stConnectParam.au8SSId, (char*)stConnectParam.au8Password);

	free(pConfData);
	return 0;
}

int saveWifiConfig()
{
	FILE* fp = NULL;
	cJSON * root;
	cJSON * param;
	cJSON * obj;
	cJSON * item;
	char id[8];

	fp = fopen(WIFI_SETTING_CFG,"w+");
	if (!fp)
	{
		printf("should open json file first\n");
		return -1;
	}

	printf("open %s success\n", WIFI_SETTING_CFG);
	root = cJSON_CreateObject();
	obj = cJSON_AddObjectToObject(root, "wifi");
	item = cJSON_AddBoolToObject(obj, "isSupport", isWifiSupport);
	item = cJSON_AddBoolToObject(obj, "isEnable", isWifiEnable);
	param = cJSON_AddObjectToObject(obj, "param");
	memset(id, 0, sizeof(id));
	sprintf(id, "%d", wlanHdl);
	item = cJSON_AddStringToObject(param, "id", id);
	item = cJSON_AddStringToObject(param, "ssid", (char*)stConnectParam.au8SSId);
	item = cJSON_AddStringToObject(param, "passwd", (char*)stConnectParam.au8Password);
	printf("%s %d %s \n",__FUNCTION__,__LINE__,cJSON_Print(root));

	fseek(fp, 0, SEEK_SET);
	fwrite(cJSON_Print(root),strlen(cJSON_Print(root)),1,fp);
	fclose(fp);
	fp = NULL;

	return 0;
}

#endif
