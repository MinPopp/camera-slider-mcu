#include "command_parser.h"
#include "slider.h"
#include "stepper.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart1;


extern osThreadId cmdParseTaskHandle;
extern osMessageQId cmdRxQueueHandle;

// static uint8_t rxByte;
static char cmdBuffer[CMD_BUFFER_SIZE];
static uint8_t cmdIndex = 0;

static char txBuffer[TX_BUFFER_SIZE];
char rxBuffer[RX_BUFFER_SIZE];

static void ProcessCommand(const char* cmd);
static void SendResponse(const char* response);

static void HandlePing(void);
static void HandleStatus(void);
static void HandleHome(void);
static void HandleMove(const char* args);
static void HandleStop(void);
static void HandleGetPos(void);
static void HandleSetParam(const char* args);
static void HandleGetParam(void);
static void SendError(int code, const char* message);

void CommandParser_Init(void)
{
    // HAL_UART_Receive_IT(&huart1, &rxByte, 1);
}


// void CommandParser_RxCallback(UART_HandleTypeDef* huart)
// {
//     if (huart == &huart1)
//     {
//         osMessagePut(cmdRxQueueHandle, rxByte, 0);
//         // HAL_UART_Receive_IT(&huart1, &rxByte, 1);
//     }
// }

void CommandParser_Run()
{
    osEvent evt;

    evt = osMessageGet(cmdRxQueueHandle, osWaitForever);

    if (evt.status == osEventMessage)
    {
        char c = (char)evt.value.v;

        if (c == '\n' || c == '\r')
        {
            if (cmdIndex > 0)
            {
                cmdBuffer[cmdIndex] = '\0';
                ProcessCommand(cmdBuffer);
                cmdIndex = 0;
            }
        }
        else if (cmdIndex < CMD_BUFFER_SIZE - 1)
        {
            cmdBuffer[cmdIndex++] = c;
        }
    }
}

static void ProcessCommand(const char* cmd)
{
    while (*cmd == ' ')
        cmd++;

    if (strncmp(cmd, "PING", 4) == 0 && (cmd[4] == '\0' || cmd[4] == ' '))
    {
        HandlePing();
    }
    else if (strncmp(cmd, "STATUS", 6) == 0 && (cmd[6] == '\0' || cmd[6] == ' '))
    {
        HandleStatus();
    }
    else if (strncmp(cmd, "HOME", 4) == 0 && (cmd[4] == '\0' || cmd[4] == ' '))
    {
        HandleHome();
    }
    else if (strncmp(cmd, "MOVE", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0'))
    {
        HandleMove(cmd + 4);
    }
    else if (strncmp(cmd, "STOP", 4) == 0 && (cmd[4] == '\0' || cmd[4] == ' '))
    {
        HandleStop();
    }
    else if (strncmp(cmd, "GETPOS", 6) == 0 && (cmd[6] == '\0' || cmd[6] == ' '))
    {
        HandleGetPos();
    }
    else if (strncmp(cmd, "SETPARAM", 8) == 0 && (cmd[8] == ' ' || cmd[8] == '\0'))
    {
        HandleSetParam(cmd + 8);
    }
    else if (strncmp(cmd, "GETPARAM", 8) == 0 && (cmd[8] == '\0' || cmd[8] == ' '))
    {
        HandleGetParam();
    }
    else
    {
        SendError(30, "UNKNOWN_COMMAND");
    }
}

static void SendResponse(const char* response)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)response, strlen(response), HAL_MAX_DELAY);
}

static void HandlePing(void)
{
    SendResponse("OK\n");
}

static void HandleStatus(void)
{
    SliderStatus status = Slider_GetStatus();

    const char* stateStr;
    switch (status.state)
    {
    case SLIDER_STATE_IDLE:
        stateStr = "idle";
        break;
    case SLIDER_STATE_MOVING:
        stateStr = "moving";
        break;
    case SLIDER_STATE_HOMING:
        stateStr = "homing";
        break;
    case SLIDER_STATE_ERROR:
        stateStr = "error";
        break;
    default:
        stateStr = "unknown";
        break;
    }

    snprintf(txBuffer, TX_BUFFER_SIZE, "OK STATE=%s POS=%ld HOMED=%d\n",
             stateStr, (long)status.position, status.homed ? 1 : 0);
    SendResponse(txBuffer);
}

static void HandleHome(void)
{
    SliderResult result = Slider_Home();
    if (result == SLIDER_OK)
    {
        SendResponse("OK\n");
    }
    else if (result == SLIDER_ERR_BUSY)
    {
        SendError(21, "BUSY");
    }
    else
    {
        SendError(30, "HOME_FAILED");
    }
}

static void HandleMove(const char* args)
{
    int32_t steps = 0;
    int32_t speed;

    const char* stepsPtr = strstr(args, "STEPS=");
    const char* speedPtr = strstr(args, "SPEED=");

    if (stepsPtr == NULL)
    {
        SendError(30, "MISSING_STEPS");
        return;
    }

    steps = atoi(stepsPtr + 6);

    if (speedPtr != NULL)
    {
        speed = atoi(speedPtr + 6);
    }
    else
    {
        speed = (int32_t)Slider_GetMotionParams().speed;
    }

    if (speed <= 0)
    {
        SendError(30, "INVALID_SPEED");
        return;
    }

    SliderResult result = Slider_Move(steps, (uint32_t)speed);
    if (result == SLIDER_OK)
    {
        SendResponse("OK\n");
    }
    else if (result == SLIDER_ERR_BUSY)
    {
        SendError(21, "BUSY");
    }
    else if (result == SLIDER_ERR_INVALID_PARAM)
    {
        SendError(30, "INVALID_PARAM");
    }
    else
    {
        SendError(30, "MOVE_FAILED");
    }
}

static void HandleStop(void)
{
    Slider_Stop();
    SliderStatus status = Slider_GetStatus();
    snprintf(txBuffer, TX_BUFFER_SIZE, "OK POS=%ld\n", (long)status.position);
    SendResponse(txBuffer);
}

static void HandleGetPos(void)
{
    SliderStatus status = Slider_GetStatus();
    snprintf(txBuffer, TX_BUFFER_SIZE, "OK POS=%ld\n", (long)status.position);
    SendResponse(txBuffer);
}

static void HandleSetParam(const char* args)
{
    const char* speedPtr = strstr(args, "SPEED=");
    const char* accelPtr = strstr(args, "ACCEL=");

    if (speedPtr == NULL && accelPtr == NULL)
    {
        SendError(31, "INVALID_PARAM");
        return;
    }

    uint32_t speed_val;
    uint32_t accel_val;
    uint32_t *speed_p = NULL;
    uint32_t *accel_p = NULL;

    if (speedPtr != NULL)
    {
        int32_t v = atoi(speedPtr + 6);
        if (v <= 0)
        {
            SendError(31, "INVALID_PARAM");
            return;
        }
        speed_val = (uint32_t)v;
        speed_p = &speed_val;
    }

    if (accelPtr != NULL)
    {
        int32_t v = atoi(accelPtr + 6);
        if (v <= 0)
        {
            SendError(31, "INVALID_PARAM");
            return;
        }
        accel_val = (uint32_t)v;
        accel_p = &accel_val;
    }

    SliderResult result = Slider_SetMotionParams(speed_p, accel_p);
    if (result == SLIDER_OK)
    {
        SliderMotionParams params = Slider_GetMotionParams();
        snprintf(txBuffer, TX_BUFFER_SIZE, "OK SPEED=%lu ACCEL=%lu\n",
                 (unsigned long)params.speed, (unsigned long)params.acceleration);
        SendResponse(txBuffer);
    }
    else
    {
        SendError(31, "INVALID_PARAM");
    }
}

static void HandleGetParam(void)
{
    SliderMotionParams params = Slider_GetMotionParams();
    snprintf(txBuffer, TX_BUFFER_SIZE, "OK SPEED=%lu ACCEL=%lu\n",
             (unsigned long)params.speed, (unsigned long)params.acceleration);
    SendResponse(txBuffer);
}

static void SendError(int code, const char* message)
{
    snprintf(txBuffer, TX_BUFFER_SIZE, "ERROR %d %s\n", code, message);
    SendResponse(txBuffer);
}
