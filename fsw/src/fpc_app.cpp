/****************************************************************************
 *
 *   Copyright (c) 2017 Windhover Labs, L.L.C. All rights reserved.
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

#include "fpc_app.h"
#include "fpc_msg.h"
#include "fpc_version.h"

#include "math/Limits.hpp"
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
FPC oFPC{};
/************************************************************************
** Local Function Definitions
*************************************************************************/

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Initialize event tables                                         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

FPC::FPC(){}
FPC::~FPC(){}

int32 FPC::InitEvent()
{
    int32  iStatus=CFE_SUCCESS;
    int32  ind = 0;

    /* Initialize the event filter table.
     * Note: 0 is the CFE_EVS_NO_FILTER mask and event 0 is reserved (not used) */
    memset((void*)EventTbl, 0x00, sizeof(EventTbl));

    /* TODO: Choose the events you want to filter.  CFE_EVS_MAX_EVENT_FILTERS
     * limits the number of filters per app.  An explicit CFE_EVS_NO_FILTER 
     * (the default) has been provided as an example. */
    EventTbl[  ind].EventID = FPC_RESERVED_EID;
    EventTbl[ind++].Mask    = CFE_EVS_NO_FILTER;

    EventTbl[  ind].EventID = FPC_INF_EID;
    EventTbl[ind++].Mask    = CFE_EVS_NO_FILTER;

    EventTbl[  ind].EventID = FPC_CONFIG_TABLE_ERR_EID;
    EventTbl[ind++].Mask    = CFE_EVS_NO_FILTER;

    EventTbl[  ind].EventID = FPC_CDS_ERR_EID;
    EventTbl[ind++].Mask    = CFE_EVS_NO_FILTER;

    EventTbl[  ind].EventID = FPC_PIPE_ERR_EID;
    EventTbl[ind++].Mask    = CFE_EVS_NO_FILTER;

    EventTbl[  ind].EventID = FPC_MSGID_ERR_EID;
    EventTbl[ind++].Mask    = CFE_EVS_NO_FILTER;

    EventTbl[  ind].EventID = FPC_MSGLEN_ERR_EID;
    EventTbl[ind++].Mask    = CFE_EVS_NO_FILTER;

    /* Register the table with CFE */
    iStatus = CFE_EVS_Register(EventTbl,
                               FPC_EVT_CNT, CFE_EVS_BINARY_FILTER);
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_ES_WriteToSysLog("FPC - Failed to register with EVS (0x%08X)\n", (unsigned int)iStatus);
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
    int32  iStatus=CFE_SUCCESS;

    /* Init schedule pipe and subscribe to wakeup messages */
    iStatus = CFE_SB_CreatePipe(&SchPipeId,
                                 FPC_SCH_PIPE_DEPTH,
                                 FPC_SCH_PIPE_NAME);
    if (iStatus == CFE_SUCCESS)
    {
        iStatus = CFE_SB_SubscribeEx(FPC_WAKEUP_MID, SchPipeId, CFE_SB_Default_Qos, FPC_SCH_PIPE_WAKEUP_RESERVED);

        if (iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                     "Sch Pipe failed to subscribe to FPC_WAKEUP_MID. (0x%08X)",
                                     (unsigned int)iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

        iStatus = CFE_SB_SubscribeEx(FPC_SEND_HK_MID, SchPipeId, CFE_SB_Default_Qos, FPC_SCH_PIPE_SEND_HK_RESERVED);

        if (iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                     "CMD Pipe failed to subscribe to FPC_SEND_HK_MID. (0x%08X)",
                                     (unsigned int)iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }

    }
    else
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                 "Failed to create SCH pipe (0x%08X)",
                                 (unsigned int)iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    /* Init command pipe and subscribe to command messages */
    iStatus = CFE_SB_CreatePipe(&CmdPipeId,
                                 FPC_CMD_PIPE_DEPTH,
                                 FPC_CMD_PIPE_NAME);
    if (iStatus == CFE_SUCCESS)
    {
        /* Subscribe to command messages */
        iStatus = CFE_SB_Subscribe(FPC_CMD_MID, CmdPipeId);

        if (iStatus != CFE_SUCCESS)
        {
            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                     "CMD Pipe failed to subscribe to FPC_CMD_MID. (0x%08X)",
                                     (unsigned int)iStatus);
            goto FPC_InitPipe_Exit_Tag;
        }
    }
    else
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                 "Failed to create CMD pipe (0x%08X)",
                                 (unsigned int)iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    /* Init data pipe and subscribe to messages on the data pipe */
    iStatus = CFE_SB_CreatePipe(&DataPipeId,
                                 FPC_DATA_PIPE_DEPTH,
                                 FPC_DATA_PIPE_NAME);
    if (iStatus == CFE_SUCCESS)
    {
        //TODO:Add when air speed message exists
//        iStatus = CFE_SB_Subscribe(ASPD4525_HK_TLM_MID, DataPipeId);
//        if (iStatus != CFE_SUCCESS)
//        {
//            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
//                                     "CMD Pipe failed to subscribe to FPC_CMD_MID. (0x%08X)",
//                                     (unsigned int)iStatus);
//            goto FPC_InitPipe_Exit_Tag;
//        }
        /* TODO:  Add CFE_SB_Subscribe() calls for other apps' output data here.
        **
        ** Examples:
        **     CFE_SB_Subscribe(GNCEXEC_OUT_DATA_MID, DataPipeId);
        */
    }
    else
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                 "Failed to create Data pipe (0x%08X)",
                                 (unsigned int)iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_CONTROL_MODE_MID, DataPipeId, CFE_SB_Default_Qos, 1);
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                 "DATA Pipe failed to subscribe to PX4_VEHICLE_CONTROL_MODE_MID. (0x%08lX)",
                 iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_GLOBAL_POSITION_MID, DataPipeId, CFE_SB_Default_Qos, 1);
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                 "DATA Pipe failed to subscribe to PX4_VEHICLE_GLOBAL_POSITION_MID. (0x%08lX)",
                 iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    iStatus = CFE_SB_SubscribeEx(PX4_VEHICLE_LOCAL_POSITION_MID, DataPipeId, CFE_SB_Default_Qos, 1);
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                 "DATA Pipe failed to subscribe to PX4_VEHICLE_LOCAL_POSITION_MID. (0x%08lX)",
                 iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

    iStatus = CFE_SB_SubscribeEx(PX4_POSITION_SETPOINT_TRIPLET_MID, DataPipeId, CFE_SB_Default_Qos, 1);
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_SUBSCRIBE_ERR_EID, CFE_EVS_ERROR,
                 "DATA Pipe failed to subscribe to PX4_VEHICLE_LOCAL_POSITION_MID. (0x%08lX)",
                 iStatus);
        goto FPC_InitPipe_Exit_Tag;
    }

FPC_InitPipe_Exit_Tag:
    return (iStatus);
}
    

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Initialize Global Variables                                     */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int32 FPC::InitData()
{
    int32  iStatus=CFE_SUCCESS;

    /* Init input data */
    memset((void*)&InData, 0x00, sizeof(InData));


    /* Init output data */


    /* Init output messages */
    memset((void*)&m_PositionControlStatusMsg, 0x00, sizeof(m_PositionControlStatusMsg));
    CFE_SB_InitMsg(&m_PositionControlStatusMsg,
        FPC_POSITION_CONTROL_STATUS_MID, sizeof(PX4_Position_Control_Status_t), TRUE);

    /* Init housekeeping packet */
    memset((void*)&HkTlm, 0x00, sizeof(HkTlm));
    CFE_SB_InitMsg(&HkTlm,
                   FPC_HK_TLM_MID, sizeof(HkTlm), TRUE);

    /* Clear input messages */
    CFE_PSP_MemSet(&m_ControlStateMsg, 0, sizeof(m_ControlStateMsg));
    CFE_PSP_MemSet(&m_ManualControlSetpointMsg, 0, sizeof(m_ManualControlSetpointMsg));
    CFE_PSP_MemSet(&m_HomePositionMsg, 0, sizeof(m_HomePositionMsg));
    CFE_PSP_MemSet(&m_VehicleControlModeMsg, 0, sizeof(m_VehicleControlModeMsg));
    CFE_PSP_MemSet(&m_PositionSetpointTripletMsg, 0, sizeof(m_PositionSetpointTripletMsg));
    CFE_PSP_MemSet(&m_VehicleStatusMsg, 0, sizeof(m_VehicleStatusMsg));
    CFE_PSP_MemSet(&m_VehicleLandDetectedMsg, 0, sizeof(m_VehicleLandDetectedMsg));
    CFE_PSP_MemSet(&m_VehicleLocalPositionMsg, 0, sizeof(m_VehicleLocalPositionMsg));

    return (iStatus);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* FPC initialization                                              */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int32 FPC::InitApp()
{
    int32  iStatus   = CFE_SUCCESS;
    int8   hasEvents = 0;

    iStatus = InitEvent();
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_ES_WriteToSysLog("FPC - Failed to init events (0x%08X)\n", (unsigned int)iStatus);
        goto FPC_InitApp_Exit_Tag;
    }
    else
    {
        hasEvents = 1;
    }

    iStatus = InitPipe();
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                 "Failed to init pipes (0x%08X)",
                                 (unsigned int)iStatus);
        goto FPC_InitApp_Exit_Tag;
    }

    iStatus = InitData();
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                 "Failed to init data (0x%08X)",
                                 (unsigned int)iStatus);
        goto FPC_InitApp_Exit_Tag;
    }

    iStatus = InitConfigTbl();
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                 "Failed to init config tables (0x%08X)",
                                 (unsigned int)iStatus);
        goto FPC_InitApp_Exit_Tag;
    }

    iStatus = InitCdsTbl();
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR,
                                 "Failed to init CDS table (0x%08X)",
                                 (unsigned int)iStatus);
        goto FPC_InitApp_Exit_Tag;
    }

FPC_InitApp_Exit_Tag:
    if (iStatus == CFE_SUCCESS)
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
        if (hasEvents == 1)
        {
            (void) CFE_EVS_SendEvent(FPC_INIT_ERR_EID, CFE_EVS_ERROR, "Application failed to initialize");
        }
        else
        {
            (void) CFE_ES_WriteToSysLog("FPC - Application failed to initialize\n");
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
    int32           iStatus=CFE_SUCCESS;
    CFE_SB_Msg_t*   MsgPtr=NULL;
    CFE_SB_MsgId_t  MsgId;

    /* Stop Performance Log entry */
    CFE_ES_PerfLogExit(FPC_MAIN_TASK_PERF_ID);

    /* Wait for WakeUp messages from scheduler */
    iStatus = CFE_SB_RcvMsg(&MsgPtr, SchPipeId, iBlocking);

    /* Start Performance Log entry */
    CFE_ES_PerfLogEntry(FPC_MAIN_TASK_PERF_ID);

    if (iStatus == CFE_SUCCESS)
    {
        MsgId = CFE_SB_GetMsgId(MsgPtr);
        switch (MsgId)
	{
            case FPC_WAKEUP_MID:
                ProcessNewCmds();
                ProcessNewData();
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
                                  "Recvd invalid SCH msgId (0x%04X)", (unsigned short)MsgId);
        }
    }
    else if (iStatus == CFE_SB_NO_MESSAGE)
    {
        /* TODO: If there's no incoming message, you can do something here, or 
         * nothing.  Note, this section is dead code only if the iBlocking arg
         * is CFE_SB_PEND_FOREVER. */
        iStatus = CFE_SUCCESS;
    }
    else if (iStatus == CFE_SB_TIME_OUT)
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
			  "SB pipe read error (0x%08X), app will exit", (unsigned int)iStatus);
        uiRunStatus= CFE_ES_APP_ERROR;
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
    int iStatus = CFE_SUCCESS;
    CFE_SB_Msg_t*   DataMsgPtr=NULL;
    CFE_SB_MsgId_t  DataMsgId;

    /* Process telemetry messages till the pipe is empty */
    while (1)
    {
        iStatus = CFE_SB_RcvMsg(&DataMsgPtr, DataPipeId, CFE_SB_POLL);
        if (iStatus == CFE_SUCCESS)
        {
            DataMsgId = CFE_SB_GetMsgId(DataMsgPtr);
            switch (DataMsgId)
            {

//                case
                /* TODO:  Add code to process all subscribed data here
                **
                ** Example:
                **     case NAV_OUT_DATA_MID:
                **         FPC_ProcessNavData(DataMsgPtr);
                **         break;
                */
                case PX4_CONTROL_STATE_MID:
                {
//                    ProcessControlStateMsg();
//                    CFE_PSP_MemCpy(&m_ControlStateMsg, MsgPtr, sizeof(m_ControlStateMsg));
                    break;
                }

                case PX4_MANUAL_CONTROL_SETPOINT_MID:
                {
//                    CFE_PSP_MemCpy(&m_ManualControlSetpointMsg, MsgPtr, sizeof(m_ManualControlSetpointMsg));
                    break;
                }

                case PX4_HOME_POSITION_MID:
                {
//                    CFE_PSP_MemCpy(&m_HomePositionMsg, MsgPtr, sizeof(m_HomePositionMsg));
                    break;
                }

                case PX4_VEHICLE_CONTROL_MODE_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleControlModeMsg, DataMsgPtr, sizeof(m_VehicleControlModeMsg));
                    break;
                }

                case PX4_POSITION_SETPOINT_TRIPLET_MID:
                {
                    CFE_PSP_MemCpy(&m_PositionSetpointTripletMsg, DataMsgPtr, sizeof(m_PositionSetpointTripletMsg));
//                    ProcessPositionSetpointTripletMsg();
                    break;
                }

                case PX4_VEHICLE_STATUS_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleStatusMsg, DataMsgPtr, sizeof(m_VehicleStatusMsg));
                    break;
                }

                case PX4_VEHICLE_LAND_DETECTED_MID:
                {
                    CFE_PSP_MemCpy(&m_VehicleLandDetectedMsg, DataMsgPtr, sizeof(m_VehicleLandDetectedMsg));
                    break;
                }

                case PX4_VEHICLE_LOCAL_POSITION_MID:
                {
//                    CFE_PSP_MemCpy(&m_VehicleLocalPositionMsg, MsgPtr, sizeof(m_VehicleLocalPositionMsg));
//                    ProcessVehicleLocalPositionMsg();
                    break;
                }

                case PX4_AIRSPEED_MID:
                {
    //                    CFE_PSP_MemCpy(&m_VehicleLocalPositionMsg, MsgPtr, sizeof(m_VehicleLocalPositionMsg));
    //                    ProcessVehicleLocalPositionMsg();
                    break;
                }

                default:
                    (void) CFE_EVS_SendEvent(FPC_MSGID_ERR_EID, CFE_EVS_ERROR,
                                      "Recvd invalid data msgId (0x%04X)", (unsigned short)DataMsgId);
                    break;
            }
        }
        else if (iStatus == CFE_SB_NO_MESSAGE)
        {
            break;
        }
        else
        {
            (void) CFE_EVS_SendEvent(FPC_PIPE_ERR_EID, CFE_EVS_ERROR,
                  "Data pipe read error (0x%08X)", (unsigned int)iStatus);
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
    CFE_SB_Msg_t*   CmdMsgPtr=NULL;
    CFE_SB_MsgId_t  CmdMsgId;

    /* Process command messages till the pipe is empty */
    while (1)
    {
        iStatus = CFE_SB_RcvMsg(&CmdMsgPtr, CmdPipeId, CFE_SB_POLL);
        if(iStatus == CFE_SUCCESS)
        {
            CmdMsgId = CFE_SB_GetMsgId(CmdMsgPtr);
            switch (CmdMsgId)
            {
                case FPC_CMD_MID:
                    ProcessNewAppCmds(CmdMsgPtr);
                    break;

                /* TODO:  Add code to process other subscribed commands here
                **
                ** Example:
                **     case CFE_TIME_DATA_CMD_MID:
                **         FPC_ProcessTimeDataCmd(CmdMsgPtr);
                **         break;
                */

                default:
                    /* Bump the command error counter for an unknown command.
                     * (This should only occur if it was subscribed to with this
                     *  pipe, but not handled in this switch-case.) */
                    HkTlm.usCmdErrCnt++;
                    (void) CFE_EVS_SendEvent(FPC_MSGID_ERR_EID, CFE_EVS_ERROR,
                                      "Recvd invalid CMD msgId (0x%04X)", (unsigned short)CmdMsgId);
                    break;
            }
        }
        else if (iStatus == CFE_SB_NO_MESSAGE)
        {
            break;
        }
        else
        {
            (void) CFE_EVS_SendEvent(FPC_PIPE_ERR_EID, CFE_EVS_ERROR,
                  "CMD pipe read error (0x%08X)", (unsigned int)iStatus);
            uiRunStatus = CFE_ES_APP_ERROR;
            break;
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
        /* Update the landing slope */
        m_LandingSlope.update(math::radians(ConfigTblPtr->LND_ANG), ConfigTblPtr->LND_FLALT,
                     ConfigTblPtr->LND_TLALT, ConfigTblPtr->LND_HVIRT);

        /* Update and publish the navigation capabilities */
        m_PositionControlStatusMsg.LANDING_SLOPE_ANGLE_RAD = m_LandingSlope.landing_slope_angle_rad();
        m_PositionControlStatusMsg.LANDING_HORIZONTAL_SLOPE_DISPLACEMENT = m_LandingSlope.horizontal_slope_displacement();
        m_PositionControlStatusMsg.LANDING_FLARE_LENGTH = m_LandingSlope.flare_length();

        m_PositionControlStatusMsg.Timestamp = PX4LIB_GetPX4TimeUs();


        /* Update Launch Detector Parameters */
//        _launchDetector.updateParams();
//        _runway_takeoff.updateParams();

    }
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Process FPC Commands                                            */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FPC::ProcessNewAppCmds(CFE_SB_Msg_t* MsgPtr)
{
    uint32  uiCmdCode=0;

    if (MsgPtr != NULL)
    {
        uiCmdCode = CFE_SB_GetCmdCode(MsgPtr);
        switch (uiCmdCode)
        {
            case FPC_NOOP_CC:
                {
                    HkTlm.usCmdCnt++;
                    (void) CFE_EVS_SendEvent(FPC_CMD_INF_EID, CFE_EVS_INFORMATION,
                                      "Recvd NOOP cmd (%u), Version %d.%d.%d.%d",
                                      (unsigned int)uiCmdCode,
                                      FPC_MAJOR_VERSION,
                                      FPC_MINOR_VERSION,
                                      FPC_REVISION,
                                      FPC_MISSION_REV);
                    break;
                 }

            case FPC_RESET_CC:
                {
                    HkTlm.usCmdCnt = 0;
                    HkTlm.usCmdErrCnt = 0;
                    (void) CFE_EVS_SendEvent(FPC_CMD_INF_EID, CFE_EVS_INFORMATION,
                                      "Recvd RESET cmd (%u)", (unsigned int)uiCmdCode);
                    break;

                }
            case FPC_DO_GO_AROUND_CC:
                {
                    HkTlm.usCmdCnt++;
                    (void) CFE_EVS_SendEvent(FPC_CMD_INF_EID, CFE_EVS_INFORMATION,
                                      "Recvd FPC_DO_GO_AROUND cmd (%u)", (unsigned int)uiCmdCode);

                    if (m_VehicleControlModeMsg.ControlAutoEnabled &&
                        m_PositionSetpointTripletMsg.Current.Valid &&
                        m_PositionSetpointTripletMsg.Current.Type == PX4_SETPOINT_TYPE_LAND)
                    {

                        m_PositionControlStatusMsg.ABORT_LANDING = TRUE;
                        (void) CFE_EVS_SendEvent(FPC_POS_CRIT_EID, CFE_EVS_CRITICAL,
                                       "Landing aborted");
                    }
                    break;

                }
            /* TODO:  Add code to process the rest of the FPC commands here */

            default:
                {
                    HkTlm.usCmdErrCnt++;
                    (void) CFE_EVS_SendEvent(FPC_MSGID_ERR_EID, CFE_EVS_ERROR,
                                      "Recvd invalid cmdId (%u)", (unsigned int)uiCmdCode);
                    break;
                }
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Send FPC Housekeeping                                           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void FPC::ReportHousekeeping()
{
    /* TODO:  Add code to update housekeeping data, if needed, here.  */

    CFE_SB_TimeStampMsg((CFE_SB_Msg_t*)&HkTlm);
    int32 iStatus = CFE_SB_SendMsg((CFE_SB_Msg_t*)&HkTlm);
    if (iStatus != CFE_SUCCESS)
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
    /* TODO:  Add code to update output data, if needed, here.  */

    UpdateParamsFromTable();

    CFE_SB_TimeStampMsg((CFE_SB_Msg_t*)&OutData);
    int32 iStatus = CFE_SB_SendMsg((CFE_SB_Msg_t*)&OutData);
    if (iStatus != CFE_SUCCESS)
    {
        /* TODO: Decide what to do if the send message fails. */
    }

    SendPositionControlStatusMsg();
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Publish VehicleAttitudeSetpoint message                         */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
void FPC::SendPositionControlStatusMsg()
{
    CFE_SB_TimeStampMsg((CFE_SB_Msg_t*)&m_PositionControlStatusMsg);
    CFE_SB_SendMsg((CFE_SB_Msg_t*)&m_PositionControlStatusMsg);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Verify Command Length                                           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

boolean FPC::VerifyCmdLength(CFE_SB_Msg_t* MsgPtr,
                           uint16 usExpectedLen)
{
    boolean bResult  = TRUE;
    uint16  usMsgLen = 0;

    if (MsgPtr != NULL)
    {
        usMsgLen = CFE_SB_GetTotalMsgLength(MsgPtr);

        if (usExpectedLen != usMsgLen)
        {
            bResult = FALSE;
            CFE_SB_MsgId_t MsgId = CFE_SB_GetMsgId(MsgPtr);
            uint16 usCmdCode = CFE_SB_GetCmdCode(MsgPtr);

            (void) CFE_EVS_SendEvent(FPC_MSGLEN_ERR_EID, CFE_EVS_ERROR,
                              "Rcvd invalid msgLen: msgId=0x%08X, cmdCode=%d, "
                              "msgLen=%d, expectedLen=%d",
                              MsgId, usCmdCode, usMsgLen, usExpectedLen);
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
    if (iStatus != CFE_SUCCESS)
    {
        (void) CFE_ES_WriteToSysLog("FPC - Failed to register the app (0x%08X)\n", (unsigned int)iStatus);
    }

    /* Start Performance Log entry */
    CFE_ES_PerfLogEntry(FPC_MAIN_TASK_PERF_ID);

    /* Perform application initializations */
    if (iStatus == CFE_SUCCESS)
    {
        iStatus = InitApp();
    }

    if (iStatus == CFE_SUCCESS)
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
    while (CFE_ES_RunLoop(&uiRunStatus) == TRUE)
    {
        int32 iStatus = RcvMsg(FPC_SCH_PIPE_PEND_TIME);
        if (iStatus != CFE_SUCCESS)
        {
            /* TODO: Decide what to do for other return values in FPC_RcvMsg(). */
        }
        /* TODO: This is only a suggestion for when to update and save CDS table.
        ** Depends on the nature of the application, the frequency of update
        ** and save can be more or less independently.
        */
        UpdateCdsTbl();
        SaveCdsTbl();

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
extern "C" void FPC_AppMain()
{
    oFPC.AppMain();
}

void FPC::Execute(void)
{

}

/************************/
/*  End of File Comment */
/************************/
