/*
 * wifiInfo.h
 *
 *  Created on: 2019年8月15日
 *      Author: koda.xu
 */

#ifndef JNI_WIFIINFO_H_
#define JNI_WIFIINFO_H_

#ifdef SUPPORT_WLAN_MODULE
#include "mi_common_datatype.h"
#include "mi_wlan.h"


bool getWifiSupportStatus();
void setWifiSupportStatus(bool enable);
bool getWifiEnableStatus();
void setWifiEnableStatus(bool enable);
bool getConnectionStatus();
void setConnectionStatus(bool enable);
WLAN_HANDLE getWlanHandle();
void setWlanHandle(WLAN_HANDLE handle);
MI_WLAN_ConnectParam_t * getConnectParam();
void saveConnectParam(MI_WLAN_ConnectParam_t *pConnParam);
int initWifiConfig();
int saveWifiConfig();

#endif

#endif /* JNI_WIFIINFO_H_ */
