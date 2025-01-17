/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "iot_cloud.h"
#include "iot_profile.h"
#include "cJSON.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief we make the data to a json string and send it to the iot cloud engine
 *
 */
int IotProfile_Report(int motorStatus, int temperature, int humidity, int moisture)
{
    int ret = -1;
    cJSON *root;
    char *jsonString;
    IotProfileService    service;
    IotProfileKV  kvTemperature;
    IotProfileKV  kvMotorStatus;
    IotProfileKV  kvHumidity;
    IotProfileKV  kvMoisture;

    /* package the data */
    service.eventTime = NULL;
    service.serviceID = "SmartWatering";
    service.propertyLst = &kvMotorStatus;
    service.nxt = NULL;

    kvMotorStatus.key = "MotorStatus";
    kvMotorStatus.value = motorStatus ? "ON" : "OFF";
    kvMotorStatus.type = IOT_PROFILE_KEY_DATATYPE_STRING;
    kvMotorStatus.nxt = &kvTemperature;

    kvTemperature.key = "Temperature";
    kvTemperature.value = &temperature;
    kvTemperature.type = IOT_PROFILE_KEY_DATATYPE_INT;
    kvTemperature.nxt = &kvHumidity;

    kvHumidity.key = "Humidity";
    kvHumidity.value = &humidity;
    kvHumidity.type = IOT_PROFILE_KEY_DATATYPE_INT;
    kvHumidity.nxt = &kvMoisture;

    kvMoisture.key = "Moisture";
    kvMoisture.value = &moisture;
    kvMoisture.type = IOT_PROFILE_KEY_DATATYPE_INT;
    kvMoisture.nxt = NULL;

    jsonString = IoTProfilePackage(&service);
    if ( NULL != jsonString) {
        RaiseLog(LOG_LEVEL_INFO, "jsonString:%s", jsonString);

        ret = CLOUD_ReportMsg(jsonString);
        free(jsonString);
    } else {
        RaiseLog(LOG_LEVEL_ERR, "format the report message error");
    }
    return ret;
}

static int GetCommandParamSetMotor(CommandParamSetControlMotor *setMotorParam, cJSON *objCmd)
{
    cJSON *objPara = NULL;

    if (setMotorParam == NULL || objCmd == NULL) {
        RaiseLog(LOG_LEVEL_ERR, "NULL POINT!\n");
        return -1;
    }

    memset(setMotorParam, 0x00, sizeof(CommandParamSetControlMotor));
    if ((objPara = cJSON_GetObjectItem(objCmd, "MotorStatus")) == NULL) {
        RaiseLog(LOG_LEVEL_ERR, "cJSON_GetObjectItem MotorStatus failed!\n");
        return -1;
    }
    if (0 == strcmp(cJSON_GetStringValue(objPara), "ON")) {
        setMotorParam->status = CN_BOARD_SWITCH_ON;
    }
    else {
        setMotorParam->status = CN_BOARD_SWITCH_OFF;
    }

    if ((objPara = cJSON_GetObjectItem(objCmd, "Capacity")) != NULL) {
        setMotorParam->capacity = cJSON_GetNumberValue(objPara);
    }
    RaiseLog(LOG_LEVEL_INFO, "status:%s, Capacity=%d\n", setMotorParam->status ? "ON" : "OFF", setMotorParam->capacity);
    return 0;
}

/**
 * @brief deal the message received from the queue
 *
 */
static int DealSetMotor(cJSON *objCmd)
{
    int ret = -1;
    cJSON *objParas = NULL;
    CommandParamSetControlMotor setControlMotorParam;

    if ((objParas = cJSON_GetObjectItem(objCmd, "paras")) == NULL) {
        RaiseLog(LOG_LEVEL_ERR, "Could not get paras in json\n");
        return ret;
    }

    if (GetCommandParamSetMotor(&setControlMotorParam, objParas) < 0) {
        return ret;
    }
    
    ret = IotProfile_CommandCallback(CLOUD_COMMAND_CONTROLMOTOR, &setControlMotorParam);
    if (ret != 0) {
        RaiseLog(LOG_LEVEL_ERR, "IotProfile_CommandCallback failed! \n");
    }

    return ret;
}

static int GetScheduleWeekDays(const char *weekdaystr, int *weekday, int length)
{
    int num = 0;
    char *ps, *pe;

    ps = weekdaystr;
    for (int i = 0; i < length; i++) {
        char day[2] = {0};
        pe = strchr(ps, ',');
        if (pe == NULL) {
            num = i + 1;
            weekday[i] = atoi(ps);
            break;
        }

        day[0] = *ps;
        ps = pe + 1;
        weekday[i] = atoi(day);
    }

    return num;
}

static int DealSetShedule(cJSON *objCmd)
{
    int ret = -1;
    cJSON *objParas = NULL;
    cJSON *objPara = NULL;
    CommandParamSetShedule setSheduleParam;
    memset(&setSheduleParam, 0x00, sizeof(CommandParamSetShedule));

    if ((objParas = cJSON_GetObjectItem(objCmd, "paras")) == NULL) {
        RaiseLog(LOG_LEVEL_ERR, "Paras not exist");
        return ret;
    }

    if ((objPara = cJSON_GetObjectItem(objParas, "Day")) != NULL) {
        char *weekdays = cJSON_GetStringValue(objPara);
        setSheduleParam.num = GetScheduleWeekDays(weekdays, setSheduleParam.day, CN_MAX_WEEKDAY);
    } else {
        return ret;
    }

    if ((objPara = cJSON_GetObjectItem(objParas, "StartHour")) != NULL) {
        setSheduleParam.startHour = cJSON_GetNumberValue(objPara);
    } else {
        return ret;
    }

    if ((objPara = cJSON_GetObjectItem(objParas, "StartMinute")) != NULL) {
        setSheduleParam.startMinute = cJSON_GetNumberValue(objPara);
    } else {
        return ret;
    }

    if ((objPara = cJSON_GetObjectItem(objParas, "ScheduleID")) != NULL) {
        strcpy(setSheduleParam.scheduleID, cJSON_GetStringValue(objPara));
    } else {
        return ret;
    }

    if ((objPara = cJSON_GetObjectItem(objParas, "Option")) != NULL) {
        setSheduleParam.option = *(cJSON_GetStringValue(objPara));
    } else {
        return ret;
    }

    if ((objPara = cJSON_GetObjectItem(objParas, "Command")) != NULL) {
        if (GetCommandParamSetMotor(&(setSheduleParam.motorCmd), objPara)) {
            return ret;
        }
        setSheduleParam.duration = setSheduleParam.motorCmd.capacity / 10;      // 1s 出水量为10ml,由此计算出浇水时间
    } else {
        return ret;
    }

    ret = IotProfile_CommandCallback(CLOUD_COMMAND_SETSHEDULE, &setSheduleParam);

    return ret;
}

int CLOUD_CommandCallBack(const char *jsonString)
{
    cJSON *objRoot = NULL;
    cJSON *objCmdName = NULL;
    int ret = -1;
    if (jsonString == NULL) {
        return ret;
    }

    if ((objRoot = cJSON_Parse(jsonString)) == NULL) {
        RaiseLog(LOG_LEVEL_ERR, "could not parse the payload\r\n");
        goto EXIT_JSONPARSE;
    }

    if ((objCmdName = cJSON_GetObjectItem(objRoot, "command_name")) == NULL) {
        RaiseLog(LOG_LEVEL_ERR, "%s:could not get the cmd name from json\r\n");
        goto EXIT_CMDOBJ;
    }

    if (0 == strcmp(cJSON_GetStringValue(objCmdName), "SetMotor")) {
        ret = DealSetMotor(objRoot);
    } else if(0 == strcmp(cJSON_GetStringValue(objCmdName), "SetSchedule")) {
        ret = DealSetShedule(objRoot);
    }else {
        RaiseLog(LOG_LEVEL_ERR, "unresolved command:%d\r\n", cJSON_GetStringValue(objCmdName));
    }

EXIT_CMDOBJ:
EXIT_JSONPARSE:
    if (objRoot != NULL) {
        cJSON_Delete(objRoot);
    }
    return ret;
}




