/****************************************************************************
 *
 *   Copyright (c) 2022 Windhover Labs, L.L.C. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name Windhover Labs nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/

/************************************************************************
 ** Pragmas
 *************************************************************************/

/************************************************************************
 ** Includes
 *************************************************************************/
#include <string.h>

#include "cfe.h"
#include <math.h>
#include <float.h>

#include "fpc_app.h"
#include "fpc_msg.h"
#include "px4lib_msgids.h"
#include "fpc_version.h"
#include "fpc_msgids.h"

#include "math/Limits.hpp"
#include "math/Vector2F.hpp"
#include "math/Euler.hpp"
#include "math/Quaternion.hpp"
#include "px4lib.h"

/************************************************************************
 ** Local Defines
 *************************************************************************/

/************************************************************************
 ** Local Structure Declarations
 *************************************************************************/

/************************************************************************
 ** External Global Variables
 *************************************************************************/

/************************************************************************
 ** Global Variables
 *************************************************************************/

/************************************************************************
 ** Local Variables
 *************************************************************************/
/************************************************************************
 ** Local Function Definitions
 *************************************************************************/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Initialize event tables                                         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

FPC::FPC()
{
}
FPC::~FPC()
{
}

int32 FPC::InitEvent()
{
    int32 iStatus = CFE_SUCCESS;
    int32 ind = 0;

    /* Initialize the event filter table.
     * Note: 0 is the CFE_EVS_NO_FILTER mask and event 0 is reserved (not used) */
    memset((void*) EventTbl, 0x00, sizeof(EventTbl));

    /* TODO: Choose the events you want to filter.  CFE_EVS_MAX_EVENT_FILTERS
     * limits the number of filters per app.  An explicit CFE_EVS_NO_FILTER
     * (the default) has been provided as an example. */
    EventTbl[ind].EventID = FPC_RESERVED_EID;
    EventTbl[ind++].Mask = CFE_EVS_FIRST_32_STOP;

    EventTbl[ind].EventID = FPC_INF_EID;
    EventTbl[ind++].Mask = CFE_EVS_NO_FILTER;

    EventTbl[ind].EventID = FPC_CONFIG_TABLE_ERR_EID;
    EventTbl[ind++].Mask = CFE_EVS_NO_FILTER;

    EventTbl[ind].EventID = FPC_CDS_ERR_EID;
    EventTbl[ind++].Mask = CFE_EVS_NO_FILTER;

    EventTbl[ind].EventID = FPC_PIPE_ERR_EID;
    EventTbl[ind++].Mask = CFE_EVS_NO_FILTER;

    EventTbl[ind].EventID = FPC_MSGID_ERR_EID;
    EventTbl[ind++].Mask = CFE_EVS_NO_FILTER;

    EventTbl[ind].EventID = FPC_MSGLEN_ERR_EID;
    EventTbl[ind++].Mask = CFE_EVS_NO_FILTER;

    /* Register the table with CFE */
    iStatus = CFE_EVS_Register(EventTbl, FPC_EVT_CNT, CFE_EVS_BINARY_FILTER);
    if(iStatus != CFE_SUCCESS)
    {
        (void) CFE_ES_WriteToSysLog(
                "FPC - Failed to register with EVS (0x%08X)\n",
                (unsigned int) iStatus);
    }

    return (iStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Initialize Message Pipes                                        */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int32 FPC::InitPipe()
{
    int32 iStatus = CFE_SUCCESS;

    /* Init schedule pipe and subscribe to wakeup messages */
    iStatus = CFE_SB_CreatePipe(&SchPipeId,
    FPC_SCH_PIPE_DEPTH,
    FPC_SCH_PIPE_NAME);
    if(iStatus == CFE_SUCCESS)
    {
        iStatus = CFE_SB_SubscribeEx(FPC_WAKEUP_MID, SchPipeId,
                CFE_SB_Default_Qos, FPC_SCH_PIPE_WAKEUP_RESERVED);

        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                    "Sch Pipe failed to subscribe to FPC_WAKEUP_MID. (0x%08X)",
                    (unsigned int) iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(FPC_SEND_HK_MID, SchPipeId,
                CFE_SB_Default_Qos, FPC_SCH_PIPE_SEND_HK_RESERVED);

        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                    "CMD Pipe failed to subscribe to FPC_SEND_HK_MID. (0x%08X)",
                    (unsigned int) iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

    }
    else
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                "Failed to create SCH pipe (0x%08X)", (unsigned int) iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    /* Init command pipe and subscribe to command messages */
    iStatus = CFE_SB_CreatePipe(&CmdPipeId,
    FPC_CMD_PIPE_DEPTH,
    FPC_CMD_PIPE_NAME);
    if(iStatus == CFE_SUCCESS)
    {
        /* Subscribe to command messages */
        iStatus = CFE_SB_SubscribeEx(FPC_CMD_MID, CmdPipeId, CFE_SB_Default_Qos,
                FPC_CMD_MID_RESERVED);

        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                    "CMD Pipe failed to subscribe to FPC_CMD_MID. (0x%08X)",
                    (unsigned int) iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }
    }
    else
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                "Failed to create CMD pipe (0x%08X)", (unsigned int) iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    /* Init data pipe and subscribe to messages on the data pipe */
    iStatus = CFE_SB_CreatePipe(&DataPipeId,
    FPC_DATA_PIPE_DEPTH,
    FPC_DATA_PIPE_NAME);
    if(iStatus == CFE_SUCCESS)
    {
        iStatus = CFE_SB_SubscribeEx(PX4_MANUAL_CONTROL_SETPOINT_MID,
                DataPipeId, CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_MANUAL_CONTROL_SETPOINT_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_CONTROL_MODE_MID, DataPipeId,
                CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_VEHICLE_CONTROL_MODE_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_GLOBAL_POSITION_MID,
                DataPipeId, CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_VEHICLE_GLOBAL_POSITION_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_LOCAL_POSITION_MID, DataPipeId,
                CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_VEHICLE_LOCAL_POSITION_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_POSITION_SETPOINT_TRIPLET_MID,
                DataPipeId, CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_POSITION_SETPOINT_TRIPLET_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_STATUS_MID, DataPipeId,
                CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_VEHICLE_STATUS_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_LAND_DETECTED_MID, DataPipeId,
                CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_VEHICLE_LAND_DETECTED_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_AIRSPEED_MID, DataPipeId,
                CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_AIRSPEED_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_ATTITUDE_MID, DataPipeId,
                CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_VEHICLE_ATTITUDE_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_SENSOR_COMBINED_MID, DataPipeId,
                CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_SENSOR_COMBINED_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(PX4_SENSOR_BARO_MID, DataPipeId,
                CFE_SB_Default_Qos, 1);
        if(iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                    "DATA Pipe failed to subscribe to PX4_SENSOR_BARO_MID. (0x%08lX)",
                    iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }
    }
    else
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                "Failed to create Data pipe (0x%08X)", (unsigned int) iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    FPC_InitPipe_Exit_Tag: return (iStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Initialize Global Variables                                     */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int32 FPC::InitData()
{
    int32 iStatus = CFE_SUCCESS;
    /* Init input data */
    memset((void*) &m_LandingSlope, 0x00, sizeof(m_LandingSlope));

    /* Init output messages */
    CFE_SB_InitMsg(&m_PositionControlStatusMsg, PX4_POSITION_CONTROL_STATUS_MID,
            sizeof(PX4_Position_Control_Status_t), TRUE);
    CFE_SB_InitMsg(&m_VehicleAttitudeSetpointMsg,
    PX4_VEHICLE_ATTITUDE_SETPOINT_MID, sizeof(m_VehicleAttitudeSetpointMsg),
    TRUE);
    CFE_SB_InitMsg(&m_PX4_TecsStatusMsg, PX4_TECS_STATUS_MID,
            sizeof(m_PX4_TecsStatusMsg), TRUE);

    /* Init housekeeping packet */
    CFE_SB_InitMsg(&HkTlm, FPC_HK_TLM_MID, sizeof(HkTlm), TRUE);

    /* Clear input messages */
    CFE_PSP_MemSet(&m_ManualControlSetpointMsg, 0,
            sizeof(m_ManualControlSetpointMsg));
    CFE_PSP_MemSet(&m_VehicleControlModeMsg, 0,
            sizeof(m_VehicleControlModeMsg));
    CFE_PSP_MemSet(&m_PositionSetpointTripletMsg, 0,
            sizeof(m_PositionSetpointTripletMsg));
    CFE_PSP_MemSet(&m_VehicleStatusMsg, 0, sizeof(m_VehicleStatusMsg));
    CFE_PSP_MemSet(&m_VehicleLandDetectedMsg, 0,
            sizeof(m_VehicleLandDetectedMsg));
    CFE_PSP_MemSet(&m_VehicleLocalPositionMsg, 0,
            sizeof(m_VehicleLocalPositionMsg));
    CFE_PSP_MemSet(&m_VehicleGlobalPositionMsg, 0,
            sizeof(m_VehicleGlobalPositionMsg));
    CFE_PSP_MemSet(&m_AirspeedMsg, 0, sizeof(m_AirspeedMsg));
    CFE_PSP_MemSet(&m_VehicleAttitudeMsg, 0, sizeof(m_VehicleAttitudeMsg));
    CFE_PSP_MemSet(&m_SensorCombinedMsg, 0, sizeof(m_SensorCombinedMsg));
    CFE_PSP_MemSet(&m_SensorBaroMsg, 0, sizeof(m_SensorBaroMsg));

    return (iStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FPC initialization                                              */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int32 FPC::InitApp()
{
    int32 iStatus = CFE_SUCCESS;
    int8 hasEvents = 0;

    iStatus = InitEvent();
    if(iStatus != CFE_SUCCESS)
    {
        (void) CFE_ES_WriteToSysLog("FPC - Failed to init events (0x%08X)\n",
                (unsigned int) iStatus);
        goto FPC_InitApp_Exit_Tag;
    }
    else
    {
        hasEvents = 1;
    }

    iStatus = InitPipe();
    if(iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                "Failed to init pipes (0x%08X)", (unsigned int) iStatus);
        goto FPC_InitApp_Exit_Tag;
    }

    iStatus = InitData();
    if(iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                "Failed to init data (0x%08X)", (unsigned int) iStatus);
        goto FPC_InitApp_Exit_Tag;
    }

    iStatus = InitConfigTbl();
    if(iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                "Failed to init config tables (0x%08X)",
                (unsigned int) iStatus);
        goto FPC_InitApp_Exit_Tag;
    }

    FPC_InitApp_Exit_Tag: if(iStatus == CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_INF_EID, CFE_EVS_INFORMATION,
                "Initialized.  Version %d.%d.%d.%d",
                FPC_MAJOR_VERSION,
                FPC_MINOR_VERSION,
                FPC_REVISION,
                FPC_MISSION_REV);
    }
    else
    {
        if(hasEvents == 1)
        {
            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                    "Application failed to initialize");
        }
        else
        {
            (void) CFE_ES_WriteToSysLog(
                    "FPC - Application failed to initialize\n");
        }
    }

    return (iStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Receive and Process Messages                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int32 FPC::RcvMsg(int32 iBlocking)
{
    int32 iStatus = CFE_SUCCESS;
    CFE_SB_Msg_t *MsgPtr = NULL;
    CFE_SB_MsgId_t MsgId;

    /* Stop Performance Log entry */
    CFE_ES_PerfLogExit(FPC_MAIN_TASK_PERF_ID);

    /* Wait for WakeUp messages from scheduler */
    iStatus = CFE_SB_RcvMsg(&MsgPtr, SchPipeId, iBlocking);

    /* Start Performance Log entry */
    CFE_ES_PerfLogEntry(FPC_MAIN_TASK_PERF_ID);

    if(iStatus == CFE_SUCCESS)
    {
        MsgId = CFE_SB_GetMsgId(MsgPtr);
        switch(MsgId)
        {
            case FPC_WAKEUP_MID:
                ProcessNewCmds();
                ProcessNewData();
                UpdateParamsFromTable();
                Execute();

                /* TODO:  Add more code here to handle other things when app wakes up */

                /* The last thing to do at the end of this Wakeup cycle should be to
                 * automatically publish new output. */
                SendOutData();
                break;

            case FPC_SEND_HK_MID:
                ReportHousekeeping();
                break;

            default:
                (void) CFE_EVS_SendEvent(FPC_MSGID_ERR_EID, CFE_EVS_ERROR,
                        "Recvd invalid SCH msgId (0x%04X)",
                        (unsigned short) MsgId);
        }
    }
    else if(iStatus == CFE_SB_NO_MESSAGE)
    {
        /* TODO: If there's no incoming message, you can do something here, or
         * nothing.  Note, this section is dead code only if the iBlocking arg
         * is CFE_SB_PEND_FOREVER. */
        iStatus = CFE_SUCCESS;
    }
    else if(iStatus == CFE_SB_TIME_OUT)
    {
        /* TODO: If there's no incoming message within a specified time (via the
         * iBlocking arg, you can do something here, or nothing.
         * Note, this section is dead code only if the iBlocking arg
         * is CFE_SB_PEND_FOREVER. */
        iStatus = CFE_SUCCESS;
    }
    else
    {
        /* TODO: This is an example of exiting on an error (either CFE_SB_BAD_ARGUMENT, or
         * CFE_SB_PIPE_RD_ERROR).
         */
        (void) CFE_EVS_SendEvent(FPC_PIPE_ERR_EID, CFE_EVS_ERROR,
                "SB pipe read error (0x%08X), app will exit",
                (unsigned int) iStatus);
        uiRunStatus = CFE_ES_APP_ERROR;
    }

    return (iStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Process Incoming Data                                           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FPC::ProcessNewData()
{
    int32 iStatus = CFE_SUCCESS;
    CFE_SB_Msg_t *dataMsgPtr = NULL;
    CFE_SB_MsgId_t DataMsgId;

    /* Process telemetry messages till the pipe is empty */
    for(int32 i = 0; i < FPC_NUM_MSG; ++i)
    {
        iStatus = CFE_SB_RcvMsg(&dataMsgPtr, DataPipeId, CFE_SB_POLL);
        if(iStatus == CFE_SUCCESS)
        {
            DataMsgId = CFE_SB_GetMsgId(dataMsgPtr);
            switch(DataMsgId)
            {

                /* TODO:  Add code to process all subscribed data here
                 **
                 ** Example:
                 **     case NAV_OUT_DATA_MID:
                 **         FPC_ProcessNavData(DataMsgPtr);
                 **         break;
                 */

                case PX4_MANUAL_CONTROL_SETPOINT_MID:
                {
                    CFE_PSP_MemCpy(&m_ManualControlSetpointMsg, dataMsgPtr,
                            sizeof(m_ManualControlSetpointMsg));
                    break;
                }

                case PX4_VEHICLE_CONTROL_MODE_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleControlModeMsg, dataMsgPtr,
                            sizeof(m_VehicleControlModeMsg));
                    break;
                }

                case PX4_POSITION_SETPOINT_TRIPLET_MID:
                {
                    // printf("PX4_POSITION_SETPOINT_TRIPLET_MID: %d\n", PX4_POSITION_SETPOINT_TRIPLET_MID);
                    CFE_PSP_MemCpy(&m_PositionSetpointTripletMsg, dataMsgPtr,
                            sizeof(m_PositionSetpointTripletMsg));
                    HkTlm.m_PositionSetpointTripletMsgCount++;
                    break;
                }

                case PX4_VEHICLE_STATUS_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleStatusMsg, dataMsgPtr,
                            sizeof(m_VehicleStatusMsg));
                    break;
                }

                case PX4_VEHICLE_LAND_DETECTED_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleLandDetectedMsg, dataMsgPtr,
                            sizeof(m_VehicleLandDetectedMsg));
                    break;
                }

                case PX4_VEHICLE_LOCAL_POSITION_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleLocalPositionMsg, dataMsgPtr,
                            sizeof(m_VehicleLocalPositionMsg));
                    break;
                }

                case PX4_VEHICLE_GLOBAL_POSITION_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleGlobalPositionMsg, dataMsgPtr,
                            sizeof(m_VehicleGlobalPositionMsg));
                    break;
                }

                case PX4_AIRSPEED_MID:
                {
                    CFE_PSP_MemCpy(&m_AirspeedMsg, dataMsgPtr,
                            sizeof(m_AirspeedMsg));
                    break;
                }

                case PX4_VEHICLE_ATTITUDE_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleAttitudeMsg, dataMsgPtr,
                            sizeof(m_VehicleAttitudeMsg));
                    break;
                }

                case PX4_SENSOR_COMBINED_MID:
                {
                    CFE_PSP_MemCpy(&m_SensorCombinedMsg, dataMsgPtr,
                            sizeof(m_SensorCombinedMsg));
                    break;
                }

                case PX4_SENSOR_BARO_MID:
                {
                    CFE_PSP_MemCpy(&m_SensorBaroMsg, dataMsgPtr,
                            sizeof(m_SensorBaroMsg));
                    break;
                }

                default:
                    (void) CFE_EVS_SendEvent(FPC_MSGID_ERR_EID, CFE_EVS_ERROR,
                            "Recvd invalid data msgId (0x%04X)",
                            (unsigned short) DataMsgId);
                    break;
            }
        }
        else if(iStatus == CFE_SB_NO_MESSAGE)
        {
            break;
        }
        else
        {
            (void) CFE_EVS_SendEvent(FPC_PIPE_ERR_EID, CFE_EVS_ERROR,
                    "Data pipe read error (0x%08X)", (unsigned int) iStatus);
            uiRunStatus = CFE_ES_APP_ERROR;
            break;
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Process Incoming Commands                                       */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FPC::ProcessNewCmds()
{
    int iStatus = CFE_SUCCESS;
    CFE_SB_Msg_t *CmdMsgPtr = NULL;
    CFE_SB_MsgId_t CmdMsgId;

    /* Process command messages till the pipe is empty */
    //Convert this to a for-loop
    while(1)
    {
        iStatus = CFE_SB_RcvMsg(&CmdMsgPtr, CmdPipeId, CFE_SB_POLL);
        if(iStatus == CFE_SUCCESS)
        {
            CmdMsgId = CFE_SB_GetMsgId(CmdMsgPtr);
            switch(CmdMsgId)
            {
                case FPC_CMD_MID:
                    ProcessNewAppCmds(CmdMsgPtr);
                    break;

                default:
                    /* Bump the command error counter for an unknown command.
                     * (This should only occur if it was subscribed to with this
                     *  pipe, but not handled in this switch-case.) */
                    HkTlm.usCmdErrCnt++;
                    (void) CFE_EVS_SendEvent(FPC_MSGID_ERR_EID, CFE_EVS_ERROR,
                            "Recvd invalid CMD msgId (0x%04X)",
                            (unsigned short) CmdMsgId);
                    break;
            }
        }
        else if(iStatus == CFE_SB_NO_MESSAGE)
        {
            break;
        }
        else
        {
            (void) CFE_EVS_SendEvent(FPC_PIPE_ERR_EID, CFE_EVS_ERROR,
                    "CMD pipe read error (0x%08X)", (unsigned int) iStatus);
            uiRunStatus = CFE_ES_APP_ERROR;
            break;
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Process FPC Commands                                            */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FPC::ProcessNewAppCmds(CFE_SB_Msg_t *MsgPtr)
{
    uint32 uiCmdCode = 0;
    uint32 returnCode = 0;

    if(MsgPtr != NULL)
    {
        uiCmdCode = CFE_SB_GetCmdCode(MsgPtr);
        switch(uiCmdCode)
        {
            case FPC_NOOP_CC:
            {
                if(VerifyCmdLength(MsgPtr, sizeof(FPC_NoArgCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    (void) CFE_EVS_SendEvent(FPC_CMD_INF_EID,
                    CFE_EVS_INFORMATION,
                            "Recvd NOOP cmd (%u), Version %d.%d.%d.%d",
                            (unsigned int) uiCmdCode,
                            FPC_MAJOR_VERSION,
                            FPC_MINOR_VERSION,
                            FPC_REVISION,
                            FPC_MISSION_REV);
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

            case FPC_RESET_CC:
            {
                if(VerifyCmdLength(MsgPtr, sizeof(FPC_NoArgCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt = 0;
                    HkTlm.usCmdErrCnt = 0;
                    (void) CFE_EVS_SendEvent(FPC_CMD_INF_EID,
                    CFE_EVS_INFORMATION, "Recvd RESET cmd (%u)",
                            (unsigned int) uiCmdCode);
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;

            }
            case FPC_DO_GO_AROUND_CC:
            {
                if(VerifyCmdLength(MsgPtr, sizeof(FPC_NoArgCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    (void) CFE_EVS_SendEvent(FPC_CMD_INF_EID,
                    CFE_EVS_INFORMATION, "Recvd FPC_DO_GO_AROUND cmd (%u)",
                            (unsigned int) uiCmdCode);

                    if(m_VehicleControlModeMsg.ControlAutoEnabled
                            && m_PositionSetpointTripletMsg.Current.Valid
                            && m_PositionSetpointTripletMsg.Current.Type
                                    == PX4_SETPOINT_TYPE_LAND)
                    {

                        m_PositionControlStatusMsg.ABORT_LANDING = TRUE;
                        (void) CFE_EVS_SendEvent(FPC_POS_CRIT_EID,
                        CFE_EVS_CRITICAL, "Landing aborted");
                    }
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;

            }

            case FPC_UPDATE_L1_PERIOD_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->L1_PERIOD = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "L1_PERIOD Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

            case FPC_UPDATE_L1_DAMPING_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {

                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->L1_DAMPING = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "L1_DAMPING Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_TIME_CONST_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_TIME_CONST = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_TIME_CONST Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_THRO_CONST_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_THRO_CONST = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_THRO_CONST Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_SINK_MIN_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_SINK_MIN = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_SINK_MIN Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_SINK_MAX_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_SINK_MAX = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_SINK_MAX Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_CLMB_MAX_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_CLMB_MAX = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_CLMB_MAX Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_CLMBOUT_DIFF_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->CLMBOUT_DIFF = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "CLMBOUT_DIFF Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_HRATE_P_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_HRATE_P = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_HRATE_P Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_HRATE_FF_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_HRATE_FF = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_HRATE_FF Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_HGT_NOISE_FILTER_COEFF_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_HGT_NOISE_FILTER_COEFF = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_HGT_NOISE_FILTER_COEFF Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_STE_RATE_ERROR_FILTER_COEFF_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_STE_RATE_ERROR_FILTER_COEFF = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_STE_RATE_ERROR_FILTER_COEFF Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

            case FPC_UPDATE_T_SRATE_P_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_SRATE_P = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_SRATE_P Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_THR_DAMP_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_THR_DAMP = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_THR_DAMP Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_INTEG_GAIN_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_INTEG_GAIN = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_INTEG_GAIN Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_VERT_ACC_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_VERT_ACC = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_VERT_ACC Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_HGT_OMEGA_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_HGT_OMEGA = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_HGT_OMEGA Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_SPD_OMEGA_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_SPD_OMEGA = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_SPD_OMEGA Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_RLL2THR_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_RLL2THR = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_RLL2THR Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_SPDWEIGHT_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_SPDWEIGHT = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_SPDWEIGHT Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_T_PTCH_DAMP_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->T_PTCH_DAMP = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "T_PTCH_DAMP Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_AIRSPD_MIN_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->AIRSPD_MIN = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "AIRSPD_MIN Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_AIRSPD_TRIM_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->AIRSPD_TRIM = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "AIRSPD_TRIM Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_AIRSPD_MAX_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->AIRSPD_MAX = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "AIRSPD_MAX Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_ARSP_MODE_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamBooleanCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamBooleanCmd_t *cmd =
                            (FPC_UpdateParamBooleanCmd_t*) MsgPtr;
                    ConfigTblPtr->ARSP_MODE = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "ARSP_MODE Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_P_LIM_MIN_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->P_LIM_MIN_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "P_LIM_MIN_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_P_LIM_MAX_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->P_LIM_MAX_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "P_LIM_MAX_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_R_LIM_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->R_LIM_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "R_LIM_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_THR_MIN_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->THR_MIN = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "THR_MIN Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_THR_MAX_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->THR_MAX = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "THR_MAX Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_THR_IDLE_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->THR_IDLE = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "THR_IDLE Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_THR_CRUISE_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->THR_CRUISE = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "THR_CRUISE Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_THR_SLEW_MAX_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->THR_SLEW_MAX = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "THR_SLEW_MAX Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_THR_ALT_SCL_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->THR_ALT_SCL = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "THR_ALT_SCL Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_MAN_R_MAX_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->MAN_R_MAX_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "MAN_R_MAX_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_MAN_P_MAX_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->MAN_P_MAX_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "MAN_P_MAX_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_RSP_OFF_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->RSP_OFF_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "RSP_OFF_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_PSP_OFF_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->PSP_OFF_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "PSP_OFF_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_THR_LND_MAX_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->THR_LND_MAX = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "THR_LND_MAX Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_ANG_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->LND_ANG_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_ANG_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_HVIRT_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->LND_HVIRT = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_HVIRT Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_FLALT_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->LND_FLALT = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_FLALT Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_TLALT_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->LND_TLALT = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_TLALT Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_HHDIST_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->LND_HHDIST = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_HHDIST Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_FL_PMIN_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->LND_FL_PMIN_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_FL_PMIN_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_FL_PMAX_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->LND_FL_PMAX_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_FL_PMAX_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_USETER_CC:
            {
                if(VerifyCmdLength(MsgPtr, sizeof(FPC_UpdateParamInt32Cmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamInt32Cmd_t *cmd = (FPC_UpdateParamInt32Cmd_t*) MsgPtr;
                    ConfigTblPtr->LND_USETER = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_USETER Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LND_AIRSPD_SC_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->LND_AIRSPD_SC = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LND_AIRSPD_SC Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

                // FPC_Launch_Detection_t Params
            case FPC_UPDATE_LAUN_ALL_ON_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamBooleanCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamBooleanCmd_t *cmd =
                            (FPC_UpdateParamBooleanCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Launch_Detection.LAUN_ALL_ON = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LAUN_ALL_ON Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LAUN_CAT_A_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Launch_Detection.LAUN_CAT_A = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LAUN_CAT_A Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LAUN_CAT_T_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Launch_Detection.LAUN_CAT_T = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LAUN_CAT_T Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LAUN_CAT_MDEL_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Launch_Detection.LAUN_CAT_MDEL =
                            cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LAUN_CAT_MDEL Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_LAUN_CAT_PMAX_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Launch_Detection.LAUN_CAT_PMAX_RADIANS =
                            cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "LAUN_CAT_PMAX_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

                // FPC_Runway_Takeoff Params
            case FPC_UPDATE_RWTO_TKOFF_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamBooleanCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamBooleanCmd_t *cmd =
                            (FPC_UpdateParamBooleanCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.RWTO_TKOFF = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "RWTO_TKOFF Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_RWTO_HDG_CC:
            {
                if(VerifyCmdLength(MsgPtr, sizeof(FPC_UpdateParamInt32Cmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamInt32Cmd_t *cmd = (FPC_UpdateParamInt32Cmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.RWTO_HDG = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "RWTO_HDG Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_NAV_ALT_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.NAV_ALT = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "NAV_ALT Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_MAX_THR_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.MAX_THR = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "MAX_THR Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_PSP_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.PSP_RADIANS = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "PSP_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_MAX_PITCH_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.MAX_PITCH_RADIANS =
                            cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "MAX_PITCH_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_MAX_ROLL_RADIANS_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.MAX_ROLL_RADIANS =
                            cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "MAX_ROLL_RADIANS Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }
            case FPC_UPDATE_AIRSPD_SCL_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.AIRSPD_SCL = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "AIRSPD_SCL Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

            case FPC_UPDATE_RUNWAY_AIRSPD_MIN_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.RWTO_AIRSPD_MIN = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "RWTO_AIRSPD_MIN Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

            case FPC_UPDATE_RUNWAY_CLMBOUT_DIFF_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    ConfigTblPtr->FPC_Runway_Takeoff.RWTO_CLMBOUT_DIFF = cmd->param;
                    returnCode = CFE_TBL_Modified(ConfigTblHdl);
                    if(returnCode != CFE_SUCCESS)
                    {
                        (void) CFE_EVS_SendEvent(FPC_TBL_ERR_EID,
                        CFE_EVS_ERROR, "CFE_TBL_Modified error (%d)",
                                (unsigned int) returnCode);
                    }
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "RWTO_CLMBOUT_DIFF Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

            case FPC_OVERRIDE_ALTITUDE_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    _hold_alt = cmd->param;
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "_hold_alt Modified.");
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;
            }

            case FPC_OVERRIDE_HEADING_CC:
            {
                if(VerifyCmdLength(MsgPtr,
                        sizeof(FPC_UpdateParamFloatCmd_t)) == TRUE)
                {
                    HkTlm.usCmdCnt++;
                    FPC_UpdateParamFloatCmd_t *cmd =
                            (FPC_UpdateParamFloatCmd_t*) MsgPtr;
                    _hdg_hold_yaw = DEG_TO_RADIANS(cmd->param);
                    (void) CFE_EVS_SendEvent(FPC_TBL_INF_EID,
                    CFE_EVS_INFORMATION, "_hdg_hold_yaw Modified.");
                    _hdg_hold_enabled = FALSE;
                    commandable_heading = TRUE;
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }

                break;
            }

            case FPC_ENABLE_VZ_BYPASS_CC:
            {
                if(VerifyCmdLength(MsgPtr, sizeof(FPC_NoArgCmd_t)) == TRUE)
                {
                    HkTlm.vz_bypass_enabled = true;
                    (void) CFE_EVS_SendEvent(FPC_CMD_INF_EID,
                    CFE_EVS_INFORMATION, "VZBypass Enabled cmd (%u)",
                            (unsigned int) uiCmdCode);
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;

            }

            case FPC_DISABLE_VZ_BYPASS_CC:
            {
                if(VerifyCmdLength(MsgPtr, sizeof(FPC_NoArgCmd_t)) == TRUE)
                {
                    HkTlm.vz_bypass_enabled = false;
                    (void) CFE_EVS_SendEvent(FPC_CMD_INF_EID,
                    CFE_EVS_INFORMATION, "VZBypass Disabled cmd (%u)",
                            (unsigned int) uiCmdCode);
                }
                else
                {
                    HkTlm.usCmdErrCnt++;
                }
                break;

            }
            default:
            {
                HkTlm.usCmdErrCnt++;
                (void) CFE_EVS_SendEvent(FPC_MSGID_ERR_EID, CFE_EVS_ERROR,
                        "Recvd invalid cmdId (%u)", (unsigned int) uiCmdCode);
                break;
            }
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* UpdateParamsFromTable                                           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void FPC::UpdateParamsFromTable(void)
{
    if(ConfigTblPtr != 0)
    {
        /* check if negative value for 2/3 of flare altitude is set for throttle cut */
        if(ConfigTblPtr->LND_TLALT < 0.0f)
        {
            ConfigTblPtr->LND_TLALT = 0.66f * ConfigTblPtr->LND_FLALT;
        }
        /* Update the landing slope */
        m_LandingSlope.update(ConfigTblPtr->LND_ANG_RADIANS,
                ConfigTblPtr->LND_FLALT, ConfigTblPtr->LND_TLALT,
                ConfigTblPtr->LND_HVIRT);

        /* Update and publish the navigation capabilities */
        m_PositionControlStatusMsg.LANDING_SLOPE_ANGLE_RAD =
                m_LandingSlope.landing_slope_angle_rad();
        m_PositionControlStatusMsg.LANDING_HORIZONTAL_SLOPE_DISPLACEMENT =
                m_LandingSlope.horizontal_slope_displacement();
        m_PositionControlStatusMsg.LANDING_FLARE_LENGTH =
                m_LandingSlope.flare_length();

        m_PositionControlStatusMsg.Timestamp = PX4LIB_GetPX4TimeUs();

        /* Update Launch Detector Parameters */
//        _launchDetector.updateParams();
//        _runway_takeoff.updateParams();
        /* L1 control parameters */

        ConfigTblPtr->PSP_OFF_RADIANS = ConfigTblPtr->PSP_OFF_RADIANS;

        _l1_control.set_l1_damping(ConfigTblPtr->L1_DAMPING);
        _l1_control.set_l1_period(ConfigTblPtr->L1_PERIOD);
        _l1_control.set_l1_roll_limit(ConfigTblPtr->R_LIM_RADIANS);

        _tecs.set_time_const(ConfigTblPtr->T_TIME_CONST);
        _tecs.set_time_const_throt(ConfigTblPtr->T_THRO_CONST);
        _tecs.set_min_sink_rate(ConfigTblPtr->T_SINK_MIN);
        _tecs.set_max_sink_rate(ConfigTblPtr->T_SINK_MAX);
        _tecs.set_throttle_damp(ConfigTblPtr->T_THR_DAMP);
        _tecs.set_throttle_slewrate(ConfigTblPtr->THR_SLEW_MAX);
        _tecs.set_integrator_gain(ConfigTblPtr->T_INTEG_GAIN);
        _tecs.set_vertical_accel_limit(ConfigTblPtr->T_VERT_ACC);
        _tecs.set_height_comp_filter_omega(ConfigTblPtr->T_HGT_OMEGA);
        _tecs.set_speed_comp_filter_omega(ConfigTblPtr->T_SPD_OMEGA);
        _tecs.set_roll_throttle_compensation(ConfigTblPtr->T_RLL2THR);
        _tecs.set_speed_weight(ConfigTblPtr->T_SPDWEIGHT);
        _tecs.set_pitch_damping(ConfigTblPtr->T_PTCH_DAMP);
        _tecs.set_indicated_airspeed_min(ConfigTblPtr->AIRSPD_MIN);
        _tecs.set_indicated_airspeed_max(ConfigTblPtr->AIRSPD_MAX);
        _tecs.set_max_climb_rate(ConfigTblPtr->T_CLMB_MAX);
        _tecs.set_heightrate_p(ConfigTblPtr->T_HRATE_P);
        _tecs.set_heightrate_ff(ConfigTblPtr->T_HRATE_FF);
        _tecs.set_height_noise_filter_coeff(ConfigTblPtr->T_HGT_NOISE_FILTER_COEFF);
        _tecs.set_speedrate_p(ConfigTblPtr->T_SRATE_P);
        _tecs.set_ste_rate_error_filter_coeff(ConfigTblPtr->T_STE_RATE_ERROR_FILTER_COEFF);

    }

    _launchDetector.UpdateParamsFromTable(
            ConfigTblPtr->FPC_Launch_Detection.LAUN_CAT_A,
            ConfigTblPtr->FPC_Launch_Detection.LAUN_CAT_T,
            ConfigTblPtr->FPC_Launch_Detection.LAUN_CAT_MDEL,
            ConfigTblPtr->FPC_Launch_Detection.LAUN_CAT_PMAX_RADIANS,
            ConfigTblPtr->FPC_Launch_Detection.LAUN_ALL_ON);

    _runway_takeoff.UpdateParamsFromTable(
            ConfigTblPtr->FPC_Runway_Takeoff.RWTO_TKOFF,
            ConfigTblPtr->FPC_Runway_Takeoff.RWTO_HDG,
            ConfigTblPtr->FPC_Runway_Takeoff.NAV_ALT,
            ConfigTblPtr->FPC_Runway_Takeoff.MAX_THR,
            ConfigTblPtr->FPC_Runway_Takeoff.PSP_RADIANS,
            ConfigTblPtr->FPC_Runway_Takeoff.MAX_PITCH_RADIANS,
            ConfigTblPtr->FPC_Runway_Takeoff.MAX_ROLL_RADIANS,
            ConfigTblPtr->FPC_Runway_Takeoff.AIRSPD_SCL,
            ConfigTblPtr->FPC_Runway_Takeoff.RWTO_AIRSPD_MIN,
            ConfigTblPtr->FPC_Runway_Takeoff.RWTO_CLMBOUT_DIFF);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Send FPC Housekeeping                                           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FPC::ReportHousekeeping()
{
    /* TODO:  Add code to update housekeeping data, if needed, here.  */

    HkTlm._runway_takeoff._state =
            static_cast<RunwayTakeoffState>((int) _runway_takeoff.getState());
    HkTlm._runway_takeoff._initialized = _runway_takeoff.getInitialized();
    HkTlm._runway_takeoff._airspeed_min = _runway_takeoff.getAirspeed_min();
    HkTlm._runway_takeoff._climbout = _runway_takeoff.getClimbout_diff();
    HkTlm._runway_takeoff._throttle_ramp_time =
            _runway_takeoff.getThrottle_ramp_time();
    HkTlm._runway_takeoff._runway_takeoff_enabled =
            _runway_takeoff.get_runway_takeoff_enabled();
    HkTlm._runway_takeoff._heading_mode = _runway_takeoff.getHeading_mode();
    HkTlm._runway_takeoff._nav_alt = _runway_takeoff.getNav_alt();
    HkTlm._runway_takeoff._takeoff_throttle =
            _runway_takeoff.getTakeoff_throttle();
    HkTlm._runway_takeoff._runway_pitch_sp =
            _runway_takeoff.getRunway_pitch_sp();
    HkTlm._runway_takeoff._max_takeoff_pitch =
            _runway_takeoff.getMax_takeoff_pitch();
    HkTlm._runway_takeoff._max_takeoff_roll =
            _runway_takeoff.getMax_takeoff_roll();
    HkTlm._runway_takeoff._min_airspeed_scaling =
            _runway_takeoff.getMinAirspeedScaling();
    HkTlm._runway_takeoff._airspeed_min = _runway_takeoff.getAirspeed_min();
    HkTlm._runway_takeoff._climbout_diff = _runway_takeoff.getClimbout_diff();
    HkTlm._runway_takeoff._runway_takeoff_enabled =
            _runway_takeoff.get_runway_takeoff_enabled();
    HkTlm.ControlModeCurrent =
            static_cast<HK_FW_POSCTRL_MODE>((int) ControlModeCurrent);
    HkTlm.tecsMode = static_cast<ECL_TECS_MODE>((int) _tecs.tecs_mode());
    HkTlm._hold_alt = _hold_alt;
    HkTlm.use_tecs_pitch = use_tecs_pitch;
    HkTlm._airspeed_valid = _airspeed_valid;
    HkTlm._airspeed_last_received = _airspeed_last_received;
    HkTlm._airspeed = _airspeed;
    HkTlm._eas2tas = _eas2tas;
    HkTlm.m_Hdg_Hold_Enabled = _hdg_hold_enabled;
    HkTlm._yaw_lock_engaged = _yaw_lock_engaged;
    HkTlm.inControl = inControl;
    HkTlm._runway_takeoff_initialized = _runway_takeoff.isInitialized();
    HkTlm._hdg_hold_yaw = _hdg_hold_yaw;
    CFE_PSP_MemCpy(&HkTlm.Params, ConfigTblPtr, sizeof(HkTlm.Params));

    CFE_SB_TimeStampMsg((CFE_SB_Msg_t*) &HkTlm);

    int32 iStatus = CFE_SB_SendMsg((CFE_SB_Msg_t*) &HkTlm);
    if(iStatus != CFE_SUCCESS)
    {
        /* TODO: Decide what to do if the send message fails. */
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Publish Output Data                                             */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FPC::SendOutData()
{
    SendPositionControlStatusMsg();
    SendTecsStatusMsg();
    SendVehicleAttitudeSetpointMsg();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Publish VehicleAttitudeSetpoint message                         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void FPC::SendPositionControlStatusMsg()
{
    CFE_SB_TimeStampMsg((CFE_SB_Msg_t*) &m_PositionControlStatusMsg);
    CFE_SB_SendMsg((CFE_SB_Msg_t*) &m_PositionControlStatusMsg);
}

/* Publish VehicleAttitudeSetpoint message                         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void FPC::SendTecsStatusMsg()
{
    CFE_SB_TimeStampMsg((CFE_SB_Msg_t*) &m_PX4_TecsStatusMsg);
    CFE_SB_SendMsg((CFE_SB_Msg_t*) &m_PX4_TecsStatusMsg);
}

/* Publish VehicleAttitudeSetpoint message                         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void FPC::SendVehicleAttitudeSetpointMsg()
{
    CFE_SB_TimeStampMsg((CFE_SB_Msg_t*) &m_VehicleAttitudeSetpointMsg);
    CFE_SB_SendMsg((CFE_SB_Msg_t*) &m_VehicleAttitudeSetpointMsg);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Verify Command Length                                           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

boolean FPC::VerifyCmdLength(CFE_SB_Msg_t *MsgPtr, uint16 usExpectedLen)
{
    boolean bResult = TRUE;
    uint16 usMsgLen = 0;

    if(MsgPtr != NULL)
    {
        usMsgLen = CFE_SB_GetTotalMsgLength(MsgPtr);

        if(usExpectedLen != usMsgLen)
        {
            bResult = FALSE;
            CFE_SB_MsgId_t MsgId = CFE_SB_GetMsgId(MsgPtr);
            uint16 usCmdCode = CFE_SB_GetCmdCode(MsgPtr);

            (void) CFE_EVS_SendEvent(FPC_MSGLEN_ERR_EID, CFE_EVS_ERROR,
                    "Rcvd invalid msgLen: msgId=0x%08X, cmdCode=%d, "
                            "msgLen=%d, expectedLen=%d", MsgId, usCmdCode,
                    usMsgLen, usExpectedLen);
            HkTlm.usCmdErrCnt++;
        }
    }

    return (bResult);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FPC application entry point and main process loop               */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FPC::AppMain()
{
    /* Register the application with Executive Services */
    uiRunStatus = CFE_ES_APP_RUN;

    int32 iStatus = CFE_ES_RegisterApp();
    if(iStatus != CFE_SUCCESS)
    {
        (void) CFE_ES_WriteToSysLog(
                "FPC - Failed to register the app (0x%08X)\n",
                (unsigned int) iStatus);
    }

    /* Start Performance Log entry */
    CFE_ES_PerfLogEntry(FPC_MAIN_TASK_PERF_ID);

    /* Perform application initializations */
    if(iStatus == CFE_SUCCESS)
    {
        iStatus = InitApp();
    }

    if(iStatus == CFE_SUCCESS)
    {
        /* Do not perform performance monitoring on startup sync */
        CFE_ES_PerfLogExit(FPC_MAIN_TASK_PERF_ID);
        CFE_ES_WaitForStartupSync(FPC_STARTUP_TIMEOUT_MSEC);
        CFE_ES_PerfLogEntry(FPC_MAIN_TASK_PERF_ID);
    }
    else
    {
        uiRunStatus = CFE_ES_APP_ERROR;
    }

    /* Application main loop */
    while(CFE_ES_RunLoop(&uiRunStatus) == TRUE)
    {
        int32 iStatus = RcvMsg(FPC_SCH_PIPE_PEND_TIME);
        if(iStatus != CFE_SUCCESS)
        {
            /* TODO: Decide what to do for other return values in FPC_RcvMsg(). */
        }
        /* TODO: This is only a suggestion for when to update and save CDS table.
         ** Depends on the nature of the application, the frequency of update
         ** and save can be more or less independently.
         */

        iStatus = AcquireConfigPointers();
        if(iStatus != CFE_SUCCESS)
        {
            /* We apparently tried to load a new table but failed.  Terminate the application. */
            uiRunStatus = CFE_ES_APP_ERROR;
        }
    }

    /* Stop Performance Log entry */
    CFE_ES_PerfLogExit(FPC_MAIN_TASK_PERF_ID);

    /* Exit the application */
    CFE_ES_ExitApp(uiRunStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* MPC Application C style main entry point.                       */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
extern "C" void FPC_AppMain(void)
{
    FPC oFPC {};

    oFPC.AppMain();
}

void FPC::Execute(void)
{
    /* handle estimator reset events. we only adjust setpoins for manual modes*/
    if(TRUE == m_VehicleControlModeMsg.ControlManualEnabled)
    {
        if(m_VehicleControlModeMsg.ControlAltitudeEnabled
                && (m_VehicleGlobalPositionMsg.AltResetCounter
                        != m_Alt_Reset_Counter))
        {
            //Careful for
            _hold_alt += m_VehicleGlobalPositionMsg.DeltaAlt;
            // make TECS accept step in altitude and demanded altitude
            _tecs.handle_alt_step(m_VehicleGlobalPositionMsg.DeltaAlt,
                    m_VehicleGlobalPositionMsg.Alt);
        }

        // adjust navigation waypoints in position control mode
        if(m_VehicleControlModeMsg.ControlAltitudeEnabled
                && m_VehicleControlModeMsg.ControlVelocityEnabled
                && m_VehicleGlobalPositionMsg.LatLonResetCounter
                        != m_Pos_Reset_Counter)
        {

            // reset heading hold flag, which will re-initialise position control
            _hdg_hold_enabled = FALSE;
        }
    }

    // update the reset counters in any case
    m_Alt_Reset_Counter = m_VehicleGlobalPositionMsg.AltResetCounter;
    m_Pos_Reset_Counter = m_VehicleGlobalPositionMsg.LatLonResetCounter;

//    if(LastProcessed != this->m_AirspeedMsg.Timestamp)
//    {
//        //Is this a new message?
    UpdateAirspeed();
//    }
    UpdateVehicleAttitude();

    math::Vector2F curr_pos(m_VehicleGlobalPositionMsg.Lat,
            m_VehicleGlobalPositionMsg.Lon);
    math::Vector2F ground_speed(m_VehicleGlobalPositionMsg.VelN,
            m_VehicleGlobalPositionMsg.VelE);

    inControl = ControlPosition(curr_pos, ground_speed,
            m_PositionSetpointTripletMsg.Previous,
            m_PositionSetpointTripletMsg.Current);
    /*
     * Attempt to control position, on success (= sensors present and not in manual mode),
     * publish setpoint.
     */
    if(inControl)
    {
        m_VehicleAttitudeSetpointMsg.Timestamp = PX4LIB_GetPX4TimeMs();

        // add attitude setpoint offsets
        m_VehicleAttitudeSetpointMsg.RollBody += ConfigTblPtr->RSP_OFF_RADIANS;
        m_VehicleAttitudeSetpointMsg.PitchBody += ConfigTblPtr->PSP_OFF_RADIANS;

        if(m_VehicleControlModeMsg.ControlManualEnabled)
        {
            m_VehicleAttitudeSetpointMsg.RollBody = math::constrain(
                    m_VehicleAttitudeSetpointMsg.RollBody,
                    -ConfigTblPtr->MAN_R_MAX_RADIANS,
                    ConfigTblPtr->MAN_R_MAX_RADIANS);
            m_VehicleAttitudeSetpointMsg.PitchBody = math::constrain(
                    m_VehicleAttitudeSetpointMsg.PitchBody,
                    -ConfigTblPtr->MAN_P_MAX_RADIANS,
                    ConfigTblPtr->MAN_P_MAX_RADIANS);
        }

        //PX4Lib Math
        math::Quaternion q(
                math::Euler(m_VehicleAttitudeSetpointMsg.RollBody,
                        m_VehicleAttitudeSetpointMsg.PitchBody,
                        m_VehicleAttitudeSetpointMsg.YawBody));
        q.copyTo(m_VehicleAttitudeSetpointMsg.Q_D);
        m_VehicleAttitudeSetpointMsg.Q_D_Valid = true;

        /* XXX check if radius makes sense here */
        float turn_distance = _l1_control.switch_distance(500.0f);

        //Might need to be added flight software
        if((PX4LIB_GetPX4ElapsedTimeUs(m_PositionControlStatusMsg.Timestamp)
                > 1000000)
                || (fabsf(
                        turn_distance
                                - m_PositionControlStatusMsg.TURN_DISTANCE)
                        > FLT_EPSILON && turn_distance > 0))
        {

            /* set new turn distance */
            m_PositionControlStatusMsg.TURN_DISTANCE = turn_distance;

            m_PositionControlStatusMsg.NAV_ROLL = _l1_control.nav_roll();
            m_PositionControlStatusMsg.NAC_PITCH = GetTecsPitch();
            m_PositionControlStatusMsg.NAV_BEARING = _l1_control.nav_bearing();

            m_PositionControlStatusMsg.TARGET_BEARING =
                    _l1_control.target_bearing();
            m_PositionControlStatusMsg.XTRACK_ERRORP =
                    _l1_control.crosstrack_error();

            math::Vector2F curr_wp(
                    (float) m_PositionSetpointTripletMsg.Current.Lat,
                    (float) m_PositionSetpointTripletMsg.Current.Lon);

            m_PositionControlStatusMsg.WP_DIST = get_distance_to_next_waypoint(
                    curr_pos[0], curr_pos[1], curr_wp[0], curr_wp[1]);

        }

    }

}

void FPC::UpdateAirspeed(void)
{
    if(ConfigTblPtr != 0)
    {
        if(!ConfigTblPtr->ARSP_MODE)
        {
            _airspeed_valid = isfinite(m_AirspeedMsg.IndicatedAirspeed)
                    && isfinite(m_AirspeedMsg.TrueAirspeed);
            _airspeed_last_received = m_AirspeedMsg.Timestamp;
            _airspeed = m_AirspeedMsg.IndicatedAirspeed;

            if((m_AirspeedMsg.IndicatedAirspeed > 0.0f)
                    && (m_AirspeedMsg.TrueAirspeed
                            > m_AirspeedMsg.IndicatedAirspeed))
            {
                _eas2tas = math::max(
                        m_AirspeedMsg.TrueAirspeed
                                / m_AirspeedMsg.IndicatedAirspeed, 1.0f);

            }
            else
            {
                _eas2tas = 1.0f;
            }

        }
        else
        {
            /* no airspeed updates for one second */
            if(_airspeed_valid
                    && ((PX4LIB_GetPX4TimeUs() - _airspeed_last_received)
                            > FPC_1SECOND_MICRO))
            {
                _airspeed_valid = FALSE;
            }
        }

        //    /* update TECS state */
        _tecs.enable_airspeed(_airspeed_valid);
    }
}

void FPC::UpdateVehicleAttitude(void)
{
    /* set rotation matrix and euler angles */
    math::Quaternion q_att(m_VehicleAttitudeMsg.Q[0], m_VehicleAttitudeMsg.Q[1],
            m_VehicleAttitudeMsg.Q[2], m_VehicleAttitudeMsg.Q[3]);
    _R_nb = q_att.RotationMatrix();

    math::Vector3F euler_angles;
    euler_angles = _R_nb.ToEuler();
    _roll = euler_angles[0];
    _pitch = euler_angles[1];
    _yaw = euler_angles[2];
}

float FPC::CalculateTargetAirspeed(float airspeed_demand)
{
    /*
     * Calculate accelerated stall airspeed factor from commanded bank angle and use it to increase minimum airspeed.
     *
     *  We don't know the stall speed of the aircraft, but assuming user defined
     *  minimum airspeed (FW_AIRSPD_MIN) is slightly larger than stall speed
     *  this is close enough.
     *
     * increase lift vector to balance additional weight in bank
     *  cos(bank angle) = W/L = 1/n
     *   n is the load factor
     *
     * lift is proportional to airspeed^2 so the increase in stall speed is
     *  Vsacc = Vs * sqrt(n)
     *
     */
    float adjusted_min_airspeed = ConfigTblPtr->AIRSPD_MIN;

    if(_airspeed_valid && isfinite(m_VehicleAttitudeSetpointMsg.RollBody))
    {

        adjusted_min_airspeed = math::constrain(
                ConfigTblPtr->AIRSPD_MIN
                        / sqrtf(cosf(m_VehicleAttitudeSetpointMsg.RollBody)),
                ConfigTblPtr->AIRSPD_MIN, ConfigTblPtr->AIRSPD_MAX);
    }

    // add minimum ground speed undershoot (only non-zero in presence of sufficient wind)
    // sanity check: limit to range
    return math::constrain(airspeed_demand + _groundspeed_undershoot,
            adjusted_min_airspeed, ConfigTblPtr->AIRSPD_MAX);
}

boolean FPC::ControlPosition(const math::Vector2F &curr_pos,
        const math::Vector2F &ground_speed,
        const PX4_PositionSetpoint_t &pos_sp_prev,
        const PX4_PositionSetpoint_t &pos_sp_curr)
{
    float dt = 0.01f;

    if(_control_position_last_called > 0)
    {
        dt = PX4LIB_GetPX4ElapsedTimeUs(_control_position_last_called) * 1e-6f;
    }

    _control_position_last_called = PX4LIB_GetPX4TimeUs();

    /* only run position controller in fixed-wing mode and during transitions for VTOL */
    if(m_VehicleStatusMsg.IsRotaryWing && !m_VehicleStatusMsg.InTransitionMode)
    {
        ControlModeCurrent = FW_POSCTRL_MODE_OTHER;
        return FALSE;
    }

    boolean setpoint = TRUE;

    m_VehicleAttitudeSetpointMsg.FwControlYaw = FALSE; // by default we don't want yaw to be contoller directly with rudder
    m_VehicleAttitudeSetpointMsg.ApplyFlaps = FALSE; // by default we don't use flaps

    CalculateGndSpeedUndershoot(curr_pos, ground_speed, pos_sp_prev,
            pos_sp_curr);

    // l1 navigation logic breaks down when wind speed exceeds max airspeed
    // compute 2D groundspeed from airspeed-heading projection
    math::Vector2F air_speed_2d
    { _airspeed * cosf(_yaw), _airspeed * sinf(_yaw) };
    math::Vector2F nav_speed_2d
    { 0.0f, 0.0f };

    // angle between air_speed_2d and ground_speed
    float air_gnd_angle = acosf(
            (air_speed_2d * ground_speed)
                    / (air_speed_2d.Length() * ground_speed.Length()));

    // if angle > 90 degrees or groundspeed is less than threshold, replace groundspeed with airspeed projection
    if((fabsf(air_gnd_angle) > M_PI_F) || (ground_speed.Length() < 3.0f))
    {
        nav_speed_2d = air_speed_2d;

    }
    else
    {
        nav_speed_2d = ground_speed;
    }

    /* no throttle limit as default */
    float throttle_max = 1.0f;

    /* save time when airplane is in air */
    if(!_was_in_air && !m_VehicleLandDetectedMsg.Landed)
    {
        _was_in_air = TRUE;
        _time_went_in_air = PX4LIB_GetPX4TimeUs();
        _takeoff_ground_alt = m_VehicleGlobalPositionMsg.Alt;
    }

    /* reset flag when airplane landed */
    if(m_VehicleLandDetectedMsg.Landed)
    {
        _was_in_air = FALSE;
    }

    /* Reset integrators if switching to this mode from a other mode in which posctl was not active */
    if(ControlModeCurrent == FW_POSCTRL_MODE_OTHER)
    {
        /* reset integrators */
        _tecs.reset_state();
    }

    // printf("pos_sp_curr.Valid - > %d\n", (pos_sp_curr.Valid==true)?1:0);
    // printf("m_VehicleControlModeMsg.ControlAutoEnabled - > %d\n", m_VehicleControlModeMsg.ControlAutoEnabled?1:0);
    // printf("m_VehicleControlModeMsg.ControlVelocityEnabled - > %d\n", m_VehicleControlModeMsg.ControlVelocityEnabled?1:0);
    // printf("m_VehicleControlModeMsg.ControlAltitudeEnabled - > %d\n", m_VehicleControlModeMsg.ControlAltitudeEnabled?1:0);
    if(m_VehicleControlModeMsg.ControlAutoEnabled && pos_sp_curr.Valid)
    {
        if (!m_VehicleControlModeMsg.ControlAutoMissionEnabled) {
            /* reset hold altitude */
            _hold_alt = m_VehicleGlobalPositionMsg.Alt;

            /* reset hold yaw */
            _hdg_hold_yaw = _yaw;
            /* AUTONOMOUS FLIGHT */

            // printf("Came Here - > %f, %f\n", pos_sp_curr.Alt, _hold_alt);
        }

        ControlModeCurrent = FW_POSCTRL_MODE_AUTO;

        /* get circle mode */
        bool was_circle_mode = _l1_control.circle_mode();

        /* restore speed weight, in case changed intermittently (e.g. in landing handling) */
        _tecs.set_speed_weight(ConfigTblPtr->T_SPDWEIGHT);

        /* current waypoint (the one currently heading for) */
        math::Vector2F curr_wp((float) pos_sp_curr.Lat,
                (float) pos_sp_curr.Lon);

        /* Initialize attitude controller integrator reset flags to 0 */
        m_VehicleAttitudeSetpointMsg.RollResetIntegral = FALSE;
        m_VehicleAttitudeSetpointMsg.PitchResetIntegral = FALSE;
        m_VehicleAttitudeSetpointMsg.YawResetIntegral = FALSE;

        /* previous waypoint */
        math::Vector2F prev_wp
        { 0.0f, 0.0f };

        if(pos_sp_prev.Valid)
        {
            prev_wp[0] = (float) pos_sp_prev.Lat;
            prev_wp[1] = (float) pos_sp_prev.Lon;

        }
        else
        {
            /*
             * No valid previous waypoint, go for the current wp.
             * This is automatically handled by the L1 library.
             */
            prev_wp[0] = (float) pos_sp_curr.Lat;
            prev_wp[1] = (float) pos_sp_curr.Lon;
        }

        float mission_airspeed = ConfigTblPtr->AIRSPD_TRIM;

        if(isfinite(pos_sp_curr.CruisingSpeed)
                && pos_sp_curr.CruisingSpeed > 0.1f)
        {

            mission_airspeed = pos_sp_curr.CruisingSpeed;
        }

        float mission_throttle = ConfigTblPtr->THR_CRUISE;

        if(isfinite(pos_sp_curr.CruisingThrottle)
                && pos_sp_curr.CruisingThrottle > 0.01f)
        {

            mission_throttle = pos_sp_curr.CruisingThrottle;
        }

        //Implement this as a switch, please.
        if(pos_sp_curr.Type == PX4_SETPOINT_TYPE_IDLE)
        {
            m_VehicleAttitudeSetpointMsg.Thrust = 0.0f;
            m_VehicleAttitudeSetpointMsg.RollBody = 0.0f;
            m_VehicleAttitudeSetpointMsg.PitchBody = 0.0f;

        }
        else if(pos_sp_curr.Type == PX4_SETPOINT_TYPE_POSITION)
        {
            /* waypoint is a plain navigation waypoint */
            _l1_control.navigate_waypoints(prev_wp, curr_wp, curr_pos,
                    nav_speed_2d);
            m_VehicleAttitudeSetpointMsg.RollBody = _l1_control.nav_roll();
            m_VehicleAttitudeSetpointMsg.YawBody = _l1_control.nav_bearing();

            TecsUpdatePitchThrottle(pos_sp_curr.Alt,
                    CalculateTargetAirspeed(mission_airspeed),
                    ConfigTblPtr->P_LIM_MIN_RADIANS
                            - ConfigTblPtr->PSP_OFF_RADIANS,
                    ConfigTblPtr->P_LIM_MAX_RADIANS
                            - ConfigTblPtr->PSP_OFF_RADIANS,
                    ConfigTblPtr->THR_MIN, ConfigTblPtr->THR_MAX,
                    mission_throttle,
                    FALSE, ConfigTblPtr->P_LIM_MIN_RADIANS);

        }
        else if(pos_sp_curr.Type == PX4_SETPOINT_TYPE_LOITER)
        {

            /* waypoint is a loiter waypoint */
            _l1_control.navigate_loiter(curr_wp, curr_pos,
                    pos_sp_curr.LoiterRadius, pos_sp_curr.LoiterDirection,
                    nav_speed_2d);
            m_VehicleAttitudeSetpointMsg.RollBody = _l1_control.nav_roll();
            m_VehicleAttitudeSetpointMsg.YawBody = _l1_control.nav_bearing();

            float alt_sp = pos_sp_curr.Alt;

            if(InTakeoffSituation())
            {
                alt_sp = math::max(alt_sp,
                        _takeoff_ground_alt + ConfigTblPtr->CLMBOUT_DIFF);
                m_VehicleAttitudeSetpointMsg.RollBody = math::constrain(
                        m_VehicleAttitudeSetpointMsg.RollBody,
                        math::radians(-5.0f), math::radians(5.0f));
            }

            if(m_PositionControlStatusMsg.ABORT_LANDING)
            {
                if(pos_sp_curr.Alt - m_VehicleGlobalPositionMsg.Alt
                        < ConfigTblPtr->CLMBOUT_DIFF)
                {
                    // aborted landing complete, normal loiter over landing point
                    m_PositionControlStatusMsg.ABORT_LANDING = FALSE;

                }
                else
                {
                    // continue straight until vehicle has sufficient altitude
                    m_VehicleAttitudeSetpointMsg.RollBody = 0.0f;
                }
            }

            TecsUpdatePitchThrottle(alt_sp,
                    CalculateTargetAirspeed(mission_airspeed),
                    ConfigTblPtr->P_LIM_MIN_RADIANS
                            - ConfigTblPtr->PSP_OFF_RADIANS,
                    ConfigTblPtr->P_LIM_MAX_RADIANS
                            - ConfigTblPtr->PSP_OFF_RADIANS,
                    ConfigTblPtr->THR_MIN, ConfigTblPtr->THR_MAX,
                    ConfigTblPtr->THR_CRUISE,
                    FALSE, ConfigTblPtr->P_LIM_MIN_RADIANS);

        }
        else if(pos_sp_curr.Type == PX4_SETPOINT_TYPE_LAND)
        {

            // apply full flaps for landings. this flag will also trigger the use of flaperons
            // if they have been enabled using the corresponding parameter
            m_VehicleAttitudeSetpointMsg.ApplyFlaps = TRUE;

            // save time at which we started landing and reset abort_landing
            if(_time_started_landing == 0)
            {
                _time_started_landing = PX4LIB_GetPX4TimeUs();

                m_PositionControlStatusMsg.ABORT_LANDING = FALSE;
            }

            float bearing_lastwp_currwp = get_bearing_to_next_waypoint(
                    prev_wp[0], prev_wp[1], curr_wp[0], curr_wp[1]);
            float bearing_airplane_currwp = get_bearing_to_next_waypoint(
                    curr_pos[0], curr_pos[1], curr_wp[0], curr_wp[1]);

            /* Horizontal landing control */
            /* switch to heading hold for the last meters, continue heading hold after */
            float wp_distance = get_distance_to_next_waypoint(curr_pos[0],
                    curr_pos[1], curr_wp[0], curr_wp[1]);

            /* calculate a waypoint distance value which is 0 when the aircraft is behind the waypoint */
            float wp_distance_save = wp_distance;

            if(fabsf(bearing_airplane_currwp - bearing_lastwp_currwp)
                    >= math::radians(90.0f))
            {
                wp_distance_save = 0.0f;
            }

            // we want the plane to keep tracking the desired flight path until we start flaring
            // if we go into heading hold mode earlier then we risk to be pushed away from the runway by cross winds
            if((ConfigTblPtr->LND_HHDIST > 0.0f) && !_land_noreturn_horizontal
                    && ((wp_distance < ConfigTblPtr->LND_HHDIST)
                            || _land_noreturn_vertical))
            {

                if(pos_sp_prev.Valid)
                {
                    /* heading hold, along the line connecting this and the last waypoint */
                    _target_bearing = bearing_lastwp_currwp;

                }
                else
                {
                    _target_bearing = _yaw;
                }

                _land_noreturn_horizontal = TRUE;
//                mavlink_log_info(&_mavlink_log_pub, "Landing, heading hold");
                (void) CFE_EVS_SendEvent(FPC_INF_EID, CFE_EVS_INFORMATION,
                        "Landing, heading hold");
            }

            if(_land_noreturn_horizontal)
            {
                // heading hold
                _l1_control.navigate_heading(_target_bearing, _yaw,
                        nav_speed_2d);

            }
            else
            {
                // normal navigation
                _l1_control.navigate_waypoints(prev_wp, curr_wp, curr_pos,
                        nav_speed_2d);
            }

            m_VehicleAttitudeSetpointMsg.RollBody = _l1_control.nav_roll();
            m_VehicleAttitudeSetpointMsg.YawBody = _l1_control.nav_bearing();

            if(_land_noreturn_horizontal)
            {
                /* limit roll motion to prevent wings from touching the ground first */
                m_VehicleAttitudeSetpointMsg.RollBody = math::constrain(
                        m_VehicleAttitudeSetpointMsg.RollBody,
                        math::radians(-10.0f), math::radians(10.0f));
            }

            /* Vertical landing control */
            /* apply minimum pitch (flare) and limit roll if close to touch down, altitude error is negative (going down) */
            float throttle_land = ConfigTblPtr->THR_MIN
                    + (ConfigTblPtr->THR_MAX - ConfigTblPtr->THR_MIN) * 0.1f;
            float airspeed_land = ConfigTblPtr->LND_AIRSPD_SC
                    * ConfigTblPtr->AIRSPD_MIN;
            float airspeed_approach = ConfigTblPtr->LND_AIRSPD_SC
                    * ConfigTblPtr->AIRSPD_MIN;

            // default to no terrain estimation, just use landing waypoint altitude
            float terrain_alt = pos_sp_curr.Alt;

            if(ConfigTblPtr->LND_USETER == 1)
            {
                if(m_VehicleGlobalPositionMsg.TerrainAltValid)
                {
                    // all good, have valid terrain altitude
                    terrain_alt = m_VehicleGlobalPositionMsg.TerrainAlt;
                    _t_alt_prev_valid = terrain_alt;
                    _time_last_t_alt = PX4LIB_GetPX4TimeUs();

                }
                else if(_time_last_t_alt == 0)
                {
                    // we have started landing phase but don't have valid terrain
                    // wait for some time, maybe we will soon get a valid estimate
                    // until then just use the altitude of the landing waypoint
                    if(PX4LIB_GetPX4ElapsedTimeUs(_time_started_landing)
                            < 10 * 1000 * 1000)
                    {
                        terrain_alt = pos_sp_curr.Alt;

                    }
                    else
                    {
                        // still no valid terrain, abort landing
                        terrain_alt = pos_sp_curr.Alt;
                        m_PositionControlStatusMsg.ABORT_LANDING = TRUE;
                    }

                }
                else if((!m_VehicleGlobalPositionMsg.TerrainAltValid
                        && PX4LIB_GetPX4ElapsedTimeUs(_time_last_t_alt)
                                < T_ALT_TIMEOUT * 1000 * 1000)
                        || _land_noreturn_vertical)
                {
                    // use previous terrain estimate for some time and hope to recover
                    // if we are already flaring (land_noreturn_vertical) then just
                    //  go with the old estimate
                    terrain_alt = _t_alt_prev_valid;

                }
                else
                {
                    // terrain alt was not valid for long time, abort landing
                    terrain_alt = _t_alt_prev_valid;
                    m_PositionControlStatusMsg.ABORT_LANDING = TRUE;
                }
            }

            /* Calculate distance (to landing waypoint) and altitude of last ordinary waypoint L */
            float L_altitude_rel = 0.0f;

            if(pos_sp_prev.Valid)
            {
                L_altitude_rel = pos_sp_prev.Alt - terrain_alt;
            }

            float landing_slope_alt_rel_desired =
                    m_LandingSlope.getLandingSlopeRelativeAltitudeSave(
                            wp_distance, bearing_lastwp_currwp,
                            bearing_airplane_currwp);

            /* Check if we should start flaring with a vertical and a
             * horizontal limit (with some tolerance)
             * The horizontal limit is only applied when we are in front of the wp
             */
            if(((m_VehicleGlobalPositionMsg.Alt
                    < terrain_alt + m_LandingSlope.flare_relative_alt())
                    && (wp_distance_save < m_LandingSlope.flare_length() + 5.0f))
                    || _land_noreturn_vertical)
            {  //checking for land_noreturn to avoid unwanted climb out

                /* land with minimal speed */

                /* force TECS to only control speed with pitch, altitude is only implicitly controlled now */
                // _tecs.set_speed_weight(2.0f);
                /* kill the throttle if param requests it */
                throttle_max = ConfigTblPtr->THR_MAX;

                /* enable direct yaw control using rudder/wheel */
                if(_land_noreturn_horizontal)
                {
                    m_VehicleAttitudeSetpointMsg.YawBody = _target_bearing;
                    m_VehicleAttitudeSetpointMsg.FwControlYaw = TRUE;
                }

                if(m_VehicleGlobalPositionMsg.Alt
                        < terrain_alt + m_LandingSlope.motor_lim_relative_alt()
                        || _land_motor_lim)
                {
                    throttle_max = math::min(throttle_max,
                            ConfigTblPtr->THR_LND_MAX);

                    if(!_land_motor_lim)
                    {
                        _land_motor_lim = TRUE;
                        (void) CFE_EVS_SendEvent(FPC_INF_EID,
                        CFE_EVS_INFORMATION, "Landing, limiting throttle");
                    }
                }

                float flare_curve_alt_rel =
                        m_LandingSlope.getFlareCurveRelativeAltitudeSave(
                                wp_distance, bearing_lastwp_currwp,
                                bearing_airplane_currwp);

                /* avoid climbout */
                if((_flare_curve_alt_rel_last < flare_curve_alt_rel
                        && _land_noreturn_vertical) || _land_stayonground)
                {
                    flare_curve_alt_rel = 0.0f; // stay on ground
                    _land_stayonground = TRUE;
                }

                TecsUpdatePitchThrottle(terrain_alt + flare_curve_alt_rel,
                        CalculateTargetAirspeed(airspeed_land),
                        ConfigTblPtr->LND_FL_PMIN_RADIANS,
                        ConfigTblPtr->LND_FL_PMAX_RADIANS, 0.0f, throttle_max,
                        throttle_land,
                        FALSE,
                        _land_motor_lim ?
                                ConfigTblPtr->LND_FL_PMIN_RADIANS :
                                ConfigTblPtr->P_LIM_MIN_RADIANS,
                        _land_motor_lim ?
                                PX4_TECS_MODE_LAND_THROTTLELIM :
                                PX4_TECS_MODE_LAND);

                if(!_land_noreturn_vertical)
                {
                    // just started with the flaring phase
                    m_VehicleAttitudeSetpointMsg.PitchBody = 0.0f;
                    _flare_height = m_VehicleGlobalPositionMsg.Alt
                            - terrain_alt;
                    (void) CFE_EVS_SendEvent(FPC_INF_EID, CFE_EVS_INFORMATION,
                            "Landing, flaring");
                    _land_noreturn_vertical = TRUE;

                }
                else
                {
                    if(m_VehicleGlobalPositionMsg.VelD > 0.1f)
                    {
                        m_VehicleAttitudeSetpointMsg.PitchBody =
                                ConfigTblPtr->LND_FL_PMIN_RADIANS
                                        * math::constrain(
                                                (_flare_height
                                                        - (m_VehicleGlobalPositionMsg.Alt
                                                                - terrain_alt))
                                                        / _flare_height, 0.0f,
                                                1.0f);
                    }

                    // otherwise continue using previous m_VehicleAttitudeSetpointMsg.pitch_body
                }

                _flare_curve_alt_rel_last = flare_curve_alt_rel;

            }
            else
            {

                /* intersect glide slope:
                 * minimize speed to approach speed
                 * if current position is higher than the slope follow the glide slope (sink to the
                 * glide slope)
                 * also if the system captures the slope it should stay
                 * on the slope (bool land_onslope)
                 * if current position is below the slope continue at previous wp altitude
                 * until the intersection with slope
                 * */
                float altitude_desired_rel
                { 0.0f };

                if(m_VehicleGlobalPositionMsg.Alt
                        > terrain_alt + landing_slope_alt_rel_desired
                        || _land_onslope)
                {
                    /* stay on slope */
                    altitude_desired_rel = landing_slope_alt_rel_desired;

                    if(!_land_onslope)
                    {
                        (void) CFE_EVS_SendEvent(FPC_INF_EID,
                        CFE_EVS_INFORMATION, "Landing, on slope");
                        _land_onslope = TRUE;
                    }

                }
                else
                {
                    /* continue horizontally */
                    if(pos_sp_prev.Valid)
                    {
                        altitude_desired_rel = L_altitude_rel;

                    }
                    else
                    {
                        altitude_desired_rel = m_VehicleGlobalPositionMsg.Alt
                                - terrain_alt;
                    }
                }

                TecsUpdatePitchThrottle(terrain_alt + altitude_desired_rel,
                        CalculateTargetAirspeed(airspeed_approach),
                        ConfigTblPtr->P_LIM_MIN_RADIANS,
                        ConfigTblPtr->P_LIM_MAX_RADIANS, ConfigTblPtr->THR_MIN,
                        ConfigTblPtr->THR_MAX, ConfigTblPtr->THR_CRUISE,
                        FALSE, ConfigTblPtr->P_LIM_MIN_RADIANS);
            }

        }
        else if(pos_sp_curr.Type == PX4_SETPOINT_TYPE_TAKEOFF)
        {

            // continuously reset launch detection and runway takeoff until armed
            if(!m_VehicleControlModeMsg.Armed)
            {
                _launchDetector.reset();
                _launch_detection_state =
                        launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_NONE;
                _launch_detection_notify = 0;
            }

            if(_runway_takeoff.runwayTakeoffEnabled())
            {
                if(!_runway_takeoff.isInitialized())
                {
                    math::Euler euler(
                            math::Quaternion(m_VehicleAttitudeMsg.Q[0],
                                    m_VehicleAttitudeMsg.Q[1],
                                    m_VehicleAttitudeMsg.Q[2],
                                    m_VehicleAttitudeMsg.Q[3]));
                    _runway_takeoff.init(euler.Psi(),
                            m_VehicleGlobalPositionMsg.Lat,
                            m_VehicleGlobalPositionMsg.Lon);

                    /* need this already before takeoff is detected
                     * doesn't matter if it gets reset when takeoff is detected eventually */
                    _takeoff_ground_alt = m_VehicleGlobalPositionMsg.Alt;

                    (void) CFE_EVS_SendEvent(FPC_INF_EID, CFE_EVS_INFORMATION,
                            "Takeoff on runway");
                }

                float terrain_alt = GetTerrainAltitudeTakeoff(
                        _takeoff_ground_alt, m_VehicleGlobalPositionMsg);

                // update runway takeoff helper
                _runway_takeoff.update(_airspeed,
                        m_VehicleGlobalPositionMsg.Alt - terrain_alt,
                        m_VehicleGlobalPositionMsg.Lat,
                        m_VehicleGlobalPositionMsg.Lon);

                /*
                 * Update navigation: _runway_takeoff returns the start WP according to mode and phase.
                 * If we use the navigator heading or not is decided later.
                 */
                _l1_control.navigate_waypoints(_runway_takeoff.getStartWP(),
                        curr_wp, curr_pos, nav_speed_2d);

                // update tecs
                float takeoff_pitch_max_deg =
                        _runway_takeoff.getMaxPitch(
                                math::degrees(
                                        ConfigTblPtr->FPC_Runway_Takeoff.MAX_PITCH_RADIANS));
                float takeoff_pitch_max_rad = math::radians(
                        takeoff_pitch_max_deg);

                TecsUpdatePitchThrottle(pos_sp_curr.Alt,
                        CalculateTargetAirspeed(
                                _runway_takeoff.getMinAirspeedScaling()
                                        * ConfigTblPtr->AIRSPD_MIN),
                        ConfigTblPtr->P_LIM_MIN_RADIANS, takeoff_pitch_max_rad,
                        ConfigTblPtr->THR_MIN,
                        ConfigTblPtr->THR_MAX, // XXX should we also set runway_takeoff_throttle here?
                        ConfigTblPtr->THR_CRUISE, _runway_takeoff.climbout(),
                        _runway_takeoff.getMinPitch(pos_sp_curr.PitchMin,
                                math::radians(10.0f),
                                ConfigTblPtr->P_LIM_MIN_RADIANS),
                        PX4_TECS_MODE_TAKEOFF);

                // assign values
                m_VehicleAttitudeSetpointMsg.RollBody = _runway_takeoff.getRoll(
                        _l1_control.nav_roll());
                m_VehicleAttitudeSetpointMsg.YawBody = _runway_takeoff.getYaw(
                        _l1_control.nav_bearing());
                m_VehicleAttitudeSetpointMsg.FwControlYaw =
                        _runway_takeoff.controlYaw();
                m_VehicleAttitudeSetpointMsg.PitchBody =
                        _runway_takeoff.getPitch(GetTecsPitch());

                // reset integrals except yaw (which also counts for the wheel controller)
                m_VehicleAttitudeSetpointMsg.RollResetIntegral =
                        _runway_takeoff.resetIntegrators();
                m_VehicleAttitudeSetpointMsg.PitchResetIntegral =
                        _runway_takeoff.resetIntegrators();

            }
            else
            {
                /* Perform launch detection */
                if(_launchDetector.launchDetectionEnabled()
                        && _launch_detection_state
                                != launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_DETECTED_ENABLEMOTORS)
                {

                    if(m_VehicleControlModeMsg.Armed)
                    {
                        /* Perform launch detection */

                        /* Inform user that launchdetection is running every 4s */
                        if(PX4LIB_GetPX4TimeUs() - _launch_detection_notify
                                > 4e6)
                        {
                            (void) CFE_EVS_SendEvent(FPC_POS_CRIT_EID,
                            CFE_EVS_CRITICAL, "Launch detection running");
                            _launch_detection_notify = PX4LIB_GetPX4TimeUs();
                        }

                        /* Detect launch using body X (forward) acceleration */
                        _launchDetector.update(m_SensorCombinedMsg.Acc[0]);

                        /* update our copy of the launch detection state */
                        _launch_detection_state =
                                _launchDetector.getLaunchDetected();
                    }

                }
                else
                {
                    /* no takeoff detection --> fly */
                    _launch_detection_state =
                            launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_DETECTED_ENABLEMOTORS;
                }

                /* Set control values depending on the detection state */
                if(_launch_detection_state
                        != launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_NONE)
                {
                    /* Launch has been detected, hence we have to control the plane. */

                    _l1_control.navigate_waypoints(prev_wp, curr_wp, curr_pos,
                            nav_speed_2d);
                    m_VehicleAttitudeSetpointMsg.RollBody =
                            _l1_control.nav_roll();
                    m_VehicleAttitudeSetpointMsg.YawBody =
                            _l1_control.nav_bearing();

                    /* Select throttle: only in LAUNCHDETECTION_RES_DETECTED_ENABLEMOTORS we want to use
                     * full throttle, otherwise we use idle throttle */
                    float takeoff_throttle = ConfigTblPtr->THR_MAX;

                    if(_launch_detection_state
                            != launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_DETECTED_ENABLEMOTORS)
                    {
                        takeoff_throttle = ConfigTblPtr->THR_IDLE;
                    }

                    /* select maximum pitch: the launchdetector may impose another limit for the pitch
                     * depending on the state of the launch */
                    float takeoff_pitch_max_rad = _launchDetector.getPitchMax(
                            ConfigTblPtr->P_LIM_MAX_RADIANS);

                    float altitude_error = pos_sp_curr.Alt
                            - m_VehicleGlobalPositionMsg.Alt;

                    /* apply minimum pitch and limit roll if target altitude is not within climbout_diff meters */
                    if(ConfigTblPtr->CLMBOUT_DIFF > 0.0f
                            && altitude_error > ConfigTblPtr->CLMBOUT_DIFF)
                    {
                        /* enforce a minimum of 10 degrees pitch up on takeoff, or take parameter */
                        TecsUpdatePitchThrottle(pos_sp_curr.Alt,
                                ConfigTblPtr->AIRSPD_TRIM,
                                ConfigTblPtr->P_LIM_MIN_RADIANS,
                                takeoff_pitch_max_rad, ConfigTblPtr->THR_MIN,
                                takeoff_throttle, ConfigTblPtr->THR_CRUISE,
                                TRUE,
                                math::max(pos_sp_curr.PitchMin,
                                        math::radians(10.0f)),
                                PX4_TECS_MODE_TAKEOFF);

                        /* limit roll motion to ensure enough lift */
                        m_VehicleAttitudeSetpointMsg.RollBody = math::constrain(
                                m_VehicleAttitudeSetpointMsg.RollBody,
                                math::radians(-15.0f), math::radians(15.0f));

                    }
                    else
                    {
                        TecsUpdatePitchThrottle(pos_sp_curr.Alt,
                                CalculateTargetAirspeed(mission_airspeed),
                                ConfigTblPtr->P_LIM_MIN_RADIANS,
                                ConfigTblPtr->P_LIM_MAX_RADIANS,
                                ConfigTblPtr->THR_MIN, takeoff_throttle,
                                ConfigTblPtr->THR_CRUISE,
                                FALSE, ConfigTblPtr->P_LIM_MIN_RADIANS);
                    }

                }
                else
                {
                    /* Tell the attitude controller to stop integrating while we are waiting
                     * for the launch */
                    m_VehicleAttitudeSetpointMsg.RollResetIntegral = TRUE;
                    m_VehicleAttitudeSetpointMsg.PitchResetIntegral = TRUE;
                    m_VehicleAttitudeSetpointMsg.YawResetIntegral = TRUE;

                    /* Set default roll and pitch setpoints during detection phase */
                    m_VehicleAttitudeSetpointMsg.RollBody = 0.0f;
                    m_VehicleAttitudeSetpointMsg.PitchBody = math::max(
                            pos_sp_curr.PitchMin, math::radians(10.0f));
                }
            }
        }

        /* reset landing state */
        if(pos_sp_curr.Type != PX4_SETPOINT_TYPE_LAND)
        {
            ResetLandingState();
        }

        /* reset takeoff/launch state */
        if(pos_sp_curr.Type != PX4_SETPOINT_TYPE_TAKEOFF)
        {
            ResetTakeoffState();
        }

        if(was_circle_mode && !_l1_control.circle_mode())
        {
            /* just kicked out of loiter, reset roll integrals */
            m_VehicleAttitudeSetpointMsg.RollResetIntegral = TRUE;
        }

    }
    else if(m_VehicleControlModeMsg.ControlVelocityEnabled
            && m_VehicleControlModeMsg.ControlAltitudeEnabled)
    {

        /* POSITION CONTROL: pitch stick moves altitude setpoint, throttle stick sets airspeed,
         heading is set to a distant waypoint */

        if(ControlModeCurrent != FW_POSCTRL_MODE_POSITION)
        {
            /* Need to init because last loop iteration was in a different mode */
            _hold_alt = m_VehicleGlobalPositionMsg.Alt;
            _hdg_hold_yaw = _yaw;
            _hdg_hold_enabled = FALSE; // this makes sure the waypoints are reset below
            _yaw_lock_engaged = FALSE;

            /* reset setpoints from other modes (auto) otherwise we won't
             * level out without new manual input */
            m_VehicleAttitudeSetpointMsg.RollBody = m_ManualControlSetpointMsg.Y
                    * ConfigTblPtr->MAN_R_MAX_RADIANS;
            m_VehicleAttitudeSetpointMsg.YawBody = 0;
        }

        ControlModeCurrent = FW_POSCTRL_MODE_POSITION;

        float altctrl_airspeed = GetDemandedAirspeed();

        /* update desired altitude based on user pitch stick input */
        bool climbout_requested = UpdateDesiredAltitude(dt);

        /* if we assume that user is taking off then help by demanding altitude setpoint well above ground
         * and set limit to pitch angle to prevent stearing into ground
         */
        float pitch_limit_min
        { 0.0f };

        DoTakeoffHelp(&_hold_alt, &pitch_limit_min);

        /* throttle limiting */
        throttle_max = ConfigTblPtr->THR_MAX;

        if(m_VehicleLandDetectedMsg.Landed
                && (fabsf(m_ManualControlSetpointMsg.Z) < THROTTLE_THRESH))
        {
            throttle_max = 0.0f;
        }

        TecsUpdatePitchThrottle(_hold_alt, altctrl_airspeed,
                ConfigTblPtr->P_LIM_MIN_RADIANS,
                ConfigTblPtr->P_LIM_MAX_RADIANS, ConfigTblPtr->THR_MIN,
                throttle_max, ConfigTblPtr->THR_CRUISE, climbout_requested,
                climbout_requested ? math::radians(10.0f) : pitch_limit_min,
                PX4_TECS_MODE_NORMAL);

        /* heading control */
        if(fabsf(m_ManualControlSetpointMsg.R) < HDG_HOLD_MAN_INPUT_THRESH
                && fabsf(m_ManualControlSetpointMsg.Y)
                        < HDG_HOLD_MAN_INPUT_THRESH)
        {
            /* heading / roll is zero, lock onto current heading */
            if(fabsf(m_VehicleAttitudeMsg.YawSpeed) < HDG_HOLD_YAWRATE_THRESH
                    && !_yaw_lock_engaged)
            {
                // little yaw movement, lock to current heading
                _yaw_lock_engaged = TRUE;

            }

            /* user tries to do a takeoff in heading hold mode, reset the yaw setpoint on every iteration
             to make sure the plane does not start rolling
             */
            if(InTakeoffSituation())
            {
                _hdg_hold_enabled = FALSE;
                _yaw_lock_engaged = TRUE;
            }

            if(_yaw_lock_engaged)
            {

                /* just switched back from non heading-hold to heading hold */
                if(!_hdg_hold_enabled)
                {
                    _hdg_hold_enabled = TRUE;
                    if(!commandable_heading)
                    {
                        _hdg_hold_yaw = _yaw;
                    }
                    commandable_heading = FALSE;
                    GetWaypointHeadingDistance(_hdg_hold_yaw,
                            HkTlm._hdg_hold_prev_wp, HkTlm._hdg_hold_curr_wp,
                            TRUE);
                }

                /* we have a valid heading hold position, are we too close? */
                HkTlm.dist_to_next_waypoint = get_distance_to_next_waypoint(
                        m_VehicleGlobalPositionMsg.Lat,
                        m_VehicleGlobalPositionMsg.Lon,
                        HkTlm._hdg_hold_curr_wp.Lat,
                        HkTlm._hdg_hold_curr_wp.Lon);

                if(HkTlm.dist_to_next_waypoint < HDG_HOLD_REACHED_DIST)
                {
                    GetWaypointHeadingDistance(_hdg_hold_yaw,
                            HkTlm._hdg_hold_prev_wp, HkTlm._hdg_hold_curr_wp,
                            FALSE);
                }

                math::Vector2F prev_wp
                { (float) HkTlm._hdg_hold_prev_wp.Lat,
                        (float) HkTlm._hdg_hold_prev_wp.Lon };
                math::Vector2F curr_wp
                { (float) HkTlm._hdg_hold_curr_wp.Lat,
                        (float) HkTlm._hdg_hold_curr_wp.Lon };

                /* populate l1 control setpoint */
                _l1_control.navigate_waypoints(prev_wp, curr_wp, curr_pos,
                        ground_speed);

                m_VehicleAttitudeSetpointMsg.RollBody = _l1_control.nav_roll();
                m_VehicleAttitudeSetpointMsg.YawBody =
                        _l1_control.nav_bearing();

                if(InTakeoffSituation())
                {
                    /* limit roll motion to ensure enough lift */
                    m_VehicleAttitudeSetpointMsg.RollBody = math::constrain(
                            m_VehicleAttitudeSetpointMsg.RollBody,
                            math::radians(-15.0f), math::radians(15.0f));
                }
            }
        }

        if(!_yaw_lock_engaged
                || fabsf(m_ManualControlSetpointMsg.Y)
                        >= HDG_HOLD_MAN_INPUT_THRESH
                || fabsf(m_ManualControlSetpointMsg.R)
                        >= HDG_HOLD_MAN_INPUT_THRESH)
        {

            _hdg_hold_enabled = FALSE;
            _yaw_lock_engaged = FALSE;
            m_VehicleAttitudeSetpointMsg.RollBody = m_ManualControlSetpointMsg.Y
                    * ConfigTblPtr->MAN_R_MAX_RADIANS;
            m_VehicleAttitudeSetpointMsg.YawBody = 0;
        }
    }

    else if(m_VehicleControlModeMsg.ControlAltitudeEnabled)
    {
        /* ALTITUDE CONTROL: pitch stick moves altitude setpoint, throttle stick sets airspeed */

        if(ControlModeCurrent != FW_POSCTRL_MODE_POSITION
                && ControlModeCurrent != FW_POSCTRL_MODE_ALTITUDE)
        {
            /* Need to init because last loop iteration was in a different mode */
            _hold_alt = m_VehicleGlobalPositionMsg.Alt;
        }

        ControlModeCurrent = FW_POSCTRL_MODE_ALTITUDE;

        /* Get demanded airspeed */
        float altctrl_airspeed = GetDemandedAirspeed();

        /* update desired altitude based on user pitch stick input */
        bool climbout_requested = UpdateDesiredAltitude(dt);

        /* if we assume that user is taking off then help by demanding altitude setpoint well above ground
         * and set limit to pitch angle to prevent stearing into ground
         */
        float pitch_limit_min
        { 0.0f };
        DoTakeoffHelp(&_hold_alt, &pitch_limit_min);

        /* throttle limiting */
        throttle_max = ConfigTblPtr->THR_MAX;

        if(m_VehicleLandDetectedMsg.Landed
                && (fabsf(m_ManualControlSetpointMsg.Z) < THROTTLE_THRESH))
        {
            throttle_max = 0.0f;
        }

        TecsUpdatePitchThrottle(_hold_alt, altctrl_airspeed,
                ConfigTblPtr->P_LIM_MIN_RADIANS,
                ConfigTblPtr->P_LIM_MAX_RADIANS, ConfigTblPtr->THR_MIN,
                throttle_max, ConfigTblPtr->THR_CRUISE, climbout_requested,
                climbout_requested ? math::radians(10.0f) : pitch_limit_min,
                PX4_TECS_MODE_NORMAL);

        m_VehicleAttitudeSetpointMsg.RollBody = m_ManualControlSetpointMsg.Y
                * ConfigTblPtr->MAN_R_MAX_RADIANS;
        m_VehicleAttitudeSetpointMsg.YawBody = 0;
    }
    else
    {
        ControlModeCurrent = FW_POSCTRL_MODE_OTHER;

        /* do not publish the setpoint */
        setpoint = FALSE;

        // reset hold altitude
        _hold_alt = m_VehicleGlobalPositionMsg.Alt;

        /* reset landing and takeoff state */
        if(!_last_manual)
        {
            ResetLandingState();
            ResetTakeoffState();
        }
    }

    /* Copy thrust output for publication */
    if(ControlModeCurrent == FW_POSCTRL_MODE_AUTO
            && // launchdetector only available in auto
            pos_sp_curr.Type == PX4_SETPOINT_TYPE_TAKEOFF
            && _launch_detection_state
                    != launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_DETECTED_ENABLEMOTORS
            && !_runway_takeoff.runwayTakeoffEnabled())
    {

        /* making sure again that the correct thrust is used,
         * without depending on library calls for safety reasons.
         the pre-takeoff throttle and the idle throttle normally map to the same parameter. */
        m_VehicleAttitudeSetpointMsg.Thrust = ConfigTblPtr->THR_IDLE;

    }
    else if(ControlModeCurrent == FW_POSCTRL_MODE_AUTO
            && pos_sp_curr.Type == PX4_SETPOINT_TYPE_TAKEOFF
            && _runway_takeoff.runwayTakeoffEnabled())
    {

        m_VehicleAttitudeSetpointMsg.Thrust = _runway_takeoff.getThrottle(
                math::min(GetTecsThrust(), throttle_max));

    }
    else if(ControlModeCurrent == FW_POSCTRL_MODE_AUTO
            && pos_sp_curr.Type == PX4_SETPOINT_TYPE_IDLE)
    {

        m_VehicleAttitudeSetpointMsg.Thrust = 0.0f;

    }
    else if(ControlModeCurrent == FW_POSCTRL_MODE_OTHER)
    {
        m_VehicleAttitudeSetpointMsg.Thrust = math::min(
                m_VehicleAttitudeSetpointMsg.Thrust, ConfigTblPtr->THR_MAX);

    }
    else
    {
        /* Copy thrust and pitch values from tecs */
        if(m_VehicleLandDetectedMsg.Landed)
        {
            // when we are landed state we want the motor to spin at idle speed
            m_VehicleAttitudeSetpointMsg.Thrust = math::min(
                    ConfigTblPtr->THR_IDLE, throttle_max);

        }
        else
        {
            m_VehicleAttitudeSetpointMsg.Thrust = math::min(GetTecsThrust(),
                    throttle_max);
        }
    }

    // decide when to use pitch setpoint from TECS because in some cases pitch
    // setpoint is generated by other means
    use_tecs_pitch = TRUE;

    // auto runway takeoff
    use_tecs_pitch &=
            !(ControlModeCurrent == FW_POSCTRL_MODE_AUTO
                    && pos_sp_curr.Type == PX4_SETPOINT_TYPE_TAKEOFF
                    && (_launch_detection_state
                            == launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_NONE
                            || _runway_takeoff.runwayTakeoffEnabled()));

    // flaring during landing
    use_tecs_pitch &= !(pos_sp_curr.Type == PX4_SETPOINT_TYPE_LAND
            && _land_noreturn_vertical);

    // manual attitude control
    use_tecs_pitch &= !(ControlModeCurrent == FW_POSCTRL_MODE_OTHER);

    if(use_tecs_pitch)
    {
        m_VehicleAttitudeSetpointMsg.PitchBody = GetTecsPitch();
        HkTlm.TecsPitch = GetTecsPitch();
    }

    if(m_VehicleControlModeMsg.ControlPositionEnabled)
    {
        _last_manual = FALSE;

    }
    else
    {
        _last_manual = TRUE;
    }

    return setpoint;
}

void FPC::CalculateGndSpeedUndershoot(const math::Vector2F &curr_pos,
        const math::Vector2F &ground_speed,
        const PX4_PositionSetpoint_t &pos_sp_prev,
        const PX4_PositionSetpoint_t &pos_sp_curr)
{
    if(pos_sp_curr.Valid && !_l1_control.circle_mode())
    {
        /* rotate ground speed vector with current attitude */
        math::Vector2F yaw_vector(_R_nb[0][0], _R_nb[1][0]);
        yaw_vector.Normalize();
        float ground_speed_body = yaw_vector * ground_speed;

        /* The minimum desired ground speed is the minimum airspeed projected on to the ground using the altitude and horizontal difference between the waypoints if available*/
        float distance = 0.0f;
        float delta_altitude = 0.0f;

        if(pos_sp_prev.Valid)
        {
            distance = get_distance_to_next_waypoint(pos_sp_prev.Lat,
                    pos_sp_prev.Lon, pos_sp_curr.Lat, pos_sp_curr.Lon);
            delta_altitude = pos_sp_curr.Alt - pos_sp_prev.Alt;

        }
        else
        {
            distance = get_distance_to_next_waypoint(curr_pos[0], curr_pos[1],
                    pos_sp_curr.Lat, pos_sp_curr.Lon);
            delta_altitude = pos_sp_curr.Alt - m_VehicleGlobalPositionMsg.Alt;
        }

        float ground_speed_desired = ConfigTblPtr->AIRSPD_MIN
                * cosf(atan2f(delta_altitude, distance));

        /*
         * Ground speed undershoot is the amount of ground velocity not reached
         * by the plane. Consequently it is zero if airspeed is >= min ground speed
         * and positive if airspeed < min ground speed.
         *
         * This error value ensures that a plane (as long as its throttle capability is
         * not exceeded) travels towards a waypoint (and is not pushed more and more away
         * by wind). Not countering this would lead to a fly-away.
         */
        _groundspeed_undershoot = math::max(
                ground_speed_desired - ground_speed_body, 0.0f);

    }
    else
    {
        _groundspeed_undershoot = 0.0f;
    }
}

void FPC::TecsUpdatePitchThrottle(float alt_sp, float airspeed_sp,
        float pitch_min_rad, float pitch_max_rad, float throttle_min,
        float throttle_max, float throttle_cruise, bool climbout_mode,
        float climbout_pitch_min_rad, PX4_TecsMode_t mode)
{
    float dt = 0.01f; // prevent division with 0

    if(_last_tecs_update > 0)
    {
        dt = PX4LIB_GetPX4ElapsedTimeUs(_last_tecs_update) * 1e-6;
    }

    _last_tecs_update = PX4LIB_GetPX4TimeUs();

    // do not run TECS if we are not in air
    boolean run_tecs = !m_VehicleLandDetectedMsg.Landed;

    // do not run TECS if vehicle is a VTOL and we are in rotary wing mode or in transition
    // (it should also not run during VTOL blending because airspeed is too low still)
    //TODO:We're not concerned with vtol mode for now.
//    if (_vehicle_status.is_vtol) {
//        if (_vehicle_status.is_rotary_wing || _vehicle_status.in_transition_mode) {
//            run_tecs = FALSE;
//        }

//        if (_vehicle_status.in_transition_mode) {
//            // we're in transition
//            _was_in_transition = TRUE;

//            // set this to transition airspeed to init tecs correctly
//            if (ConfigTblPtr->ARSP_MODE) {
//                // some vtols fly without airspeed sensor
//                _asp_after_transition = _parameters.airspeed_trans;

//            } else {
//                _asp_after_transition = _airspeed;
//            }

//            _asp_after_transition = constrain(_asp_after_transition, ConfigTblPtr->AIRSPD_MIN, ConfigTblPtr->AIRSPD_MAX);

//        } else if (_was_in_transition) {
//            // after transition we ramp up desired airspeed from the speed we had coming out of the transition
//            _asp_after_transition += dt * 2; // increase 2m/s

//            if (_asp_after_transition < airspeed_sp && _airspeed < airspeed_sp) {
//                airspeed_sp = max(_asp_after_transition, _airspeed);

//            } else {
//                _was_in_transition = FALSE;
//                _asp_after_transition = 0;
//            }
//        }
//    }

    _is_tecs_running = run_tecs;

    if(!run_tecs)
    {
        // next time we run TECS we should reinitialize states
        _reinitialize_tecs = TRUE;
        return;
    }

    if(_reinitialize_tecs)
    {
        _tecs.reset_state();
        _reinitialize_tecs = FALSE;
    }

    if(m_VehicleStatusMsg.EngineFailure || m_VehicleStatusMsg.EngineFailureCmd)
    {
        /* Force the slow downwards spiral */
        pitch_min_rad = M_DEG_TO_RAD_F * -1.0f;
        pitch_max_rad = M_DEG_TO_RAD_F * 5.0f;
    }

    /* No underspeed protection in landing mode */
    _tecs.set_detect_underspeed_enabled(
            !(mode == PX4_TECS_MODE_LAND
                    || mode == PX4_TECS_MODE_LAND_THROTTLELIM));

    /* Using tecs library */
    float pitch_for_tecs = _pitch - ConfigTblPtr->PSP_OFF_RADIANS;

    // if the vehicle is a tailsitter we have to rotate the attitude by the pitch offset
    // between multirotor and fixed wing flight
    //TODO:Ignore vtol for now
//    if (_parameters.vtol_type == vtol_type::TAILSITTER && _vehicle_status.is_vtol) {
//        math::Matrix3F3 R_offset;
//        R_offset.from_euler(0, M_PI_2_F, 0);
//        math::Matrix3F3 R_fixed_wing = _R_nb * R_offset;
//        math::Vector3F euler = R_fixed_wing.to_euler();
//        pitch_for_tecs = euler[1];
//    }

    /* filter speed and altitude for controller */
    math::Vector3F accel_body(m_SensorCombinedMsg.Acc[0],
            m_SensorCombinedMsg.Acc[1], m_SensorCombinedMsg.Acc[2]);

    // tailsitters use the multicopter frame as reference, in fixed wing
    // we need to use the fixed wing frame
    //TODO:Ignore vtol for now
//    if (_parameters.vtol_type == vtol_type::TAILSITTER && _vehicle_status.is_vtol) {
//        float tmp = accel_body(0);
//        accel_body(0) = -accel_body(2);
//        accel_body(2) = tmp;
//    }

    /* tell TECS to update its state, but let it know when it    cannot actually control the plane */
    bool in_air_alt_control = (!m_VehicleLandDetectedMsg.Landed
            && (m_VehicleControlModeMsg.ControlAutoEnabled
                    || m_VehicleControlModeMsg.ControlVelocityEnabled
                    || m_VehicleControlModeMsg.ControlAltitudeEnabled));

    /* update TECS vehicle state estimates */
    _tecs.update_vehicle_state_estimates(_airspeed, _R_nb, accel_body,
            (m_VehicleGlobalPositionMsg.Timestamp > 0), in_air_alt_control,
            m_VehicleGlobalPositionMsg.Alt, m_VehicleLocalPositionMsg.V_Z_Valid, HkTlm.vz_bypass_enabled,
            m_VehicleLocalPositionMsg.VZ, m_VehicleLocalPositionMsg.AZ);
    
    

    /* scale throttle cruise by baro pressure */
    if(ConfigTblPtr->THR_ALT_SCL > FLT_EPSILON)
    {
        if(isfinite(m_SensorBaroMsg.Pressure)
                && isfinite(ConfigTblPtr->THR_ALT_SCL))
        {
            // scale throttle as a function of sqrt(p0/p) (~ EAS -> TAS at low speeds and altitudes ignoring temperature)
            const float eas2tas = sqrtf(
                    MSL_PRESSURE_MILLIBAR / m_SensorBaroMsg.Pressure);
            const float scale = math::constrain(
                    eas2tas * ConfigTblPtr->THR_ALT_SCL, 0.9f, 2.0f);

            throttle_max = math::constrain(throttle_max * scale, throttle_min,
                    1.0f);
            throttle_cruise = math::constrain(throttle_cruise * scale,
                    throttle_min + 0.01f, throttle_max - 0.01f);
        }

    }

    _tecs.update_pitch_throttle(_R_nb, pitch_for_tecs,
            m_VehicleGlobalPositionMsg.Alt, alt_sp, airspeed_sp, _airspeed,
            _eas2tas, climbout_mode, climbout_pitch_min_rad, throttle_min,
            throttle_max, throttle_cruise, pitch_min_rad, pitch_max_rad);

    m_PX4_TecsStatusMsg.Timestamp = _tecs.timestamp();

    switch(_tecs.tecs_mode())
    {
        case TECS::ECL_TECS_MODE_NORMAL:
            m_PX4_TecsStatusMsg.Mode = PX4_TECS_MODE_NORMAL;
            break;

        case TECS::ECL_TECS_MODE_UNDERSPEED:
            m_PX4_TecsStatusMsg.Mode = PX4_TECS_MODE_UNDERSPEED;
            break;

        case TECS::ECL_TECS_MODE_BAD_DESCENT:
            m_PX4_TecsStatusMsg.Mode = PX4_TECS_MODE_BAD_DESCENT;
            break;

        case TECS::ECL_TECS_MODE_CLIMBOUT:
            m_PX4_TecsStatusMsg.Mode = PX4_TECS_MODE_CLIMBOUT;
            break;
    }

    m_PX4_TecsStatusMsg.AltitudeSp = _tecs.hgt_setpoint_adj();
    m_PX4_TecsStatusMsg.AltitudeFiltered = _tecs.vert_pos_state();
    m_PX4_TecsStatusMsg.AirspeedSp = _tecs.TAS_setpoint_adj();
    m_PX4_TecsStatusMsg.AirspeedFiltered = _tecs.tas_state();

    m_PX4_TecsStatusMsg.FlightPathAngleSp = _tecs.hgt_rate_setpoint();
    m_PX4_TecsStatusMsg.FlightPathAngle = _tecs.vert_vel_state();

    m_PX4_TecsStatusMsg.AirspeedDerivativeSp = _tecs.TAS_rate_setpoint();
    m_PX4_TecsStatusMsg.AirspeedDerivative = _tecs.speed_derivative();

    m_PX4_TecsStatusMsg.TotalEnergyError = _tecs.STE_error();
    m_PX4_TecsStatusMsg.TotalEnergyRateError = _tecs.STE_rate_error();
    m_PX4_TecsStatusMsg.EnergyDistributionError = _tecs.SEB_error();
    m_PX4_TecsStatusMsg.EnergyDistributionRateError = _tecs.SEB_rate_error();

    m_PX4_TecsStatusMsg.ThrottleInteg = _tecs.throttle_integ_state();
    m_PX4_TecsStatusMsg.PitchInteg = _tecs.pitch_integ_state();

    //This message is published by SendTecsStatusMsg

//    if (_tecs_status_pub != nullptr) {
//        orb_publish(ORB_ID(tecs_status), _tecs_status_pub, &t);

//    } else {
//        _tecs_status_pub = orb_advertise(ORB_ID(tecs_status), &t);
//    }
}

boolean FPC::InTakeoffSituation()
{
    // in air for < 10s
    uint64 delta_takeoff = 10000000;

    return (PX4LIB_GetPX4ElapsedTimeUs(_time_went_in_air) < delta_takeoff)
            && (m_VehicleGlobalPositionMsg.Alt
                    <= _takeoff_ground_alt + ConfigTblPtr->CLMBOUT_DIFF);
}

float FPC::GetTerrainAltitudeTakeoff(float takeoff_alt,
        const PX4_VehicleGlobalPositionMsg_t &global_pos)
{
    if(isfinite(global_pos.TerrainAlt) && global_pos.TerrainAltValid)
    {
        return global_pos.TerrainAlt;
    }

    return takeoff_alt;
}

float FPC::GetTecsPitch()
{
    if(_is_tecs_running)
    {
        return _tecs.get_pitch_setpoint();
    }

    // return 0 to prevent stale tecs state when it's not running
    return 0.0f;
}

void FPC::ResetLandingState()
{
    _time_started_landing = 0;

    // reset terrain estimation relevant values
    _time_last_t_alt = 0;

    _land_noreturn_horizontal = false;
    _land_noreturn_vertical = false;
    _land_stayonground = false;
    _land_motor_lim = false;
    _land_onslope = false;

    // reset abort land, unless loitering after an abort
    if(m_PositionControlStatusMsg.ABORT_LANDING
            && m_PositionSetpointTripletMsg.Current.Type
                    != PX4_SETPOINT_TYPE_LOITER)
    {

        m_PositionControlStatusMsg.ABORT_LANDING = false;
    }
}

void FPC::ResetTakeoffState()
{
    // only reset takeoff if !armed or just landed
    if(!m_VehicleControlModeMsg.Armed
            || (_was_in_air && m_VehicleLandDetectedMsg.Landed))
    {

        _runway_takeoff.reset();

        _launchDetector.reset();
        _launch_detection_state =
                launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_NONE;
        _launch_detection_notify = 0;
    }
    else
    {
        _launch_detection_state =
                launchdetection::LaunchDetectionResult::LAUNCHDETECTION_RES_DETECTED_ENABLEMOTORS;
    }
}

float FPC::GetDemandedAirspeed()
{
    float altctrl_airspeed = 0;

    // neutral throttle corresponds to trim airspeed
    if(m_ManualControlSetpointMsg.Z < 0.5f)
    {
        // lower half of throttle is min to trim airspeed
        altctrl_airspeed = ConfigTblPtr->AIRSPD_MIN
                + (ConfigTblPtr->AIRSPD_TRIM - ConfigTblPtr->AIRSPD_MIN)
                        * m_ManualControlSetpointMsg.Z * 2;

    }
    else
    {
        // upper half of throttle is trim to max airspeed
        altctrl_airspeed = ConfigTblPtr->AIRSPD_TRIM
                + (ConfigTblPtr->AIRSPD_MAX - ConfigTblPtr->AIRSPD_TRIM)
                        * (m_ManualControlSetpointMsg.Z * 2 - 1);
    }

    return altctrl_airspeed;
}

bool FPC::UpdateDesiredAltitude(float dt)
{
    /*
     * The complete range is -1..+1, so this is 6%
     * of the up or down range or 3% of the total range.
     */
    const float deadBand = 0.06f;

    /*
     * The correct scaling of the complete range needs
     * to account for the missing part of the slope
     * due to the deadband
     */
    const float factor = 1.0f - deadBand;

    /* Climbout mode sets maximum throttle and pitch up */
    bool climbout_mode = false;

    /*
     * Reset the hold altitude to the current altitude if the uncertainty
     * changes significantly.
     * This is to guard against uncommanded altitude changes
     * when the altitude certainty increases or decreases.
     */

    if(fabsf(_althold_epv - m_VehicleGlobalPositionMsg.EpV)
            > ALTHOLD_EPV_RESET_THRESH)
    {
        _hold_alt = m_VehicleGlobalPositionMsg.Alt;
        _althold_epv = m_VehicleGlobalPositionMsg.EpV;
    }

    /*
     * Manual control has as convention the rotation around
     * an axis. Positive X means to rotate positively around
     * the X axis in NED frame, which is pitching down
     */
    if(m_ManualControlSetpointMsg.X > deadBand)
    {
        /* pitching down */
        float pitch = -(m_ManualControlSetpointMsg.X - deadBand) / factor;
        _hold_alt += (ConfigTblPtr->T_SINK_MAX * dt) * pitch;
        _was_in_deadband = false;

    }
    else if(m_ManualControlSetpointMsg.X < -deadBand)
    {
        /* pitching up */
        float pitch = -(m_ManualControlSetpointMsg.X + deadBand) / factor;
        _hold_alt += (ConfigTblPtr->T_CLMB_MAX * dt) * pitch;

        _was_in_deadband = false;
        climbout_mode = (pitch > MANUAL_THROTTLE_CLIMBOUT_THRESH);

    }
    else if(!_was_in_deadband)
    {
        /* store altitude at which manual.x was inside deadBand
         * The aircraft should immediately try to fly at this altitude
         * as this is what the pilot expects when he moves the stick to the center */
        _hold_alt = m_VehicleGlobalPositionMsg.Alt;

        _althold_epv = m_VehicleGlobalPositionMsg.EpV;
        _was_in_deadband = true;
    }

    if(m_VehicleStatusMsg.IsVtol)
    {
        if(m_VehicleStatusMsg.IsRotaryWing
                || m_VehicleStatusMsg.InTransitionMode)
        {
            _hold_alt = m_VehicleGlobalPositionMsg.Alt;

        }
    }

    return climbout_mode;
}

void FPC::DoTakeoffHelp(float *hold_altitude, float *pitch_limit_min)
{
    /* demand "climbout_diff" m above ground if user switched into this mode during takeoff */
    if(InTakeoffSituation())
    {
        *hold_altitude = _takeoff_ground_alt + ConfigTblPtr->CLMBOUT_DIFF;
        *pitch_limit_min = math::radians(10.0f);

    }
    else
    {
        *pitch_limit_min = ConfigTblPtr->P_LIM_MIN_RADIANS;
    }
}

void FPC::GetWaypointHeadingDistance(float heading,
        PX4_PositionSetpoint_t &waypoint_prev,
        PX4_PositionSetpoint_t &waypoint_next, bool flag_init)
{
    PX4_PositionSetpoint_t temp_prev = waypoint_prev;
    PX4_PositionSetpoint_t temp_next = waypoint_next;

    if(flag_init)
    {
        // previous waypoint: HDG_HOLD_SET_BACK_DIST meters behind us
        waypoint_from_heading_and_distance(m_VehicleGlobalPositionMsg.Lat,
                m_VehicleGlobalPositionMsg.Lon, heading + math::radians(180.0f),
                HDG_HOLD_SET_BACK_DIST, &temp_prev.Lat, &temp_prev.Lon);

        // next waypoint: HDG_HOLD_DIST_NEXT meters in front of us
        waypoint_from_heading_and_distance(m_VehicleGlobalPositionMsg.Lat,
                m_VehicleGlobalPositionMsg.Lon, heading, HDG_HOLD_DIST_NEXT,
                &temp_next.Lat, &temp_next.Lon);

    }
    else
    {
        // use the existing flight path from prev to next

        // previous waypoint: shifted HDG_HOLD_REACHED_DIST + HDG_HOLD_SET_BACK_DIST
        create_waypoint_from_line_and_dist(waypoint_next.Lat, waypoint_next.Lon,
                waypoint_prev.Lat, waypoint_prev.Lon,
                HDG_HOLD_REACHED_DIST + HDG_HOLD_SET_BACK_DIST, &temp_prev.Lat,
                &temp_prev.Lon);

        // next waypoint: shifted -(HDG_HOLD_DIST_NEXT + HDG_HOLD_REACHED_DIST)
        create_waypoint_from_line_and_dist(waypoint_next.Lat, waypoint_next.Lon,
                waypoint_prev.Lat, waypoint_prev.Lon,
                -(HDG_HOLD_REACHED_DIST + HDG_HOLD_DIST_NEXT), &temp_next.Lat,
                &temp_next.Lon);
    }

    waypoint_prev = temp_prev;
    waypoint_prev.Alt = _hold_alt;
    waypoint_prev.Valid = true;

    waypoint_next = temp_next;
    waypoint_next.Alt = _hold_alt;
    waypoint_next.Valid = true;
}

float FPC::GetTecsThrust()
{
    if(_is_tecs_running)
    {
        return _tecs.get_throttle_setpoint();
    }

    // return 0 to prevent stale tecs state when it's not running
    return 0.0f;
}

/************************/
/*  End of File Comment */
/************************/
