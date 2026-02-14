#include "slider.h"
#include "stepper.h"
#include "tmc2209.h"
#include "cmsis_os.h"
#include "main.h"

static SliderState state = SLIDER_STATE_IDLE;
static SliderErrorCode error_code = SLIDER_ERROR_NONE;
static bool homed = false;
static bool driver_configured = false;

static int32_t pending_steps = 0;
static uint32_t pending_speed = 0;
static volatile bool motion_requested = false;
static volatile bool home_requested = false;
static volatile bool stop_requested = false;
static volatile bool config_requested = false;
static volatile bool motion_complete = false;
static volatile bool motion_success = false;

extern osMutexId sliderMutexHandle;
extern osSemaphoreId motionSemHandle;

static void OnMotionComplete(bool completed, int32_t position);

void Slider_Init(void)
{
    osSemaphoreWait(motionSemHandle, 0);

    Stepper_Init();

    state = SLIDER_STATE_IDLE;
    homed = false;
    driver_configured = false;
}

SliderStatus Slider_GetStatus(void)
{
    SliderStatus status;
    osMutexWait(sliderMutexHandle, osWaitForever);
    status.state = state;
    status.error_code = error_code;
    status.position = Stepper_GetPosition();
    status.homed = homed;
    osMutexRelease(sliderMutexHandle);
    return status;
}

SliderResult Slider_Home(void)
{
    osMutexWait(sliderMutexHandle, osWaitForever);
    if (state != SLIDER_STATE_IDLE)
    {
        osMutexRelease(sliderMutexHandle);
        return SLIDER_ERR_BUSY;
    }
    home_requested = true;
    osMutexRelease(sliderMutexHandle);
    return SLIDER_OK;
}

SliderResult Slider_Move(int32_t steps, uint32_t speed)
{
    if (steps == 0 || speed == 0)
    {
        return SLIDER_ERR_INVALID_PARAM;
    }

    osMutexWait(sliderMutexHandle, osWaitForever);
    if (state != SLIDER_STATE_IDLE)
    {
        osMutexRelease(sliderMutexHandle);
        return SLIDER_ERR_BUSY;
    }

    if (homed)
    {
        int32_t current_pos = Stepper_GetPosition();
        int32_t target_pos = current_pos + steps;
        if (target_pos < 0 || target_pos > SLIDER_RAIL_LENGTH_STEPS)
        {
            osMutexRelease(sliderMutexHandle);
            return SLIDER_ERR_OUT_OF_BOUNDS;
        }
    }

    pending_steps = steps;
    pending_speed = speed;
    motion_requested = true;
    osMutexRelease(sliderMutexHandle);
    return SLIDER_OK;
}

SliderResult Slider_Stop(void)
{
    osMutexWait(sliderMutexHandle, osWaitForever);
    stop_requested = true;
    osMutexRelease(sliderMutexHandle);
    return SLIDER_OK;
}

SliderResult Slider_ConfigureDriver(void)
{
    osMutexWait(sliderMutexHandle, osWaitForever);
    if (state != SLIDER_STATE_IDLE)
    {
        osMutexRelease(sliderMutexHandle);
        return SLIDER_ERR_BUSY;
    }
    config_requested = true;
    osMutexRelease(sliderMutexHandle);
    return SLIDER_OK;
}

bool Slider_IsDriverConfigured(void)
{
    return driver_configured;
}

static void OnMotionComplete(bool completed, int32_t position)
{
    (void)position;
    motion_complete = true;
    motion_success = completed;
    osSemaphoreRelease(motionSemHandle);
}

void Slider_Run()
{
    osMutexWait(sliderMutexHandle, osWaitForever);

    if (stop_requested)
    {
        stop_requested = false;
        if (state == SLIDER_STATE_MOVING || state == SLIDER_STATE_HOMING)
        {
            Stepper_Stop(false);
        }
    }

    switch (state)
    {
    case SLIDER_STATE_IDLE:
        if (home_requested)
        {
            home_requested = false;
            state = SLIDER_STATE_HOMING;
            homed = false;
            error_code = SLIDER_ERROR_NONE;

            StepperMoveParams params = {
                .steps = -100000,
                .max_speed = STEPPER_HOME_SPEED,
                .acceleration = STEPPER_DEFAULT_ACCEL,
                .on_complete = OnMotionComplete,
            };
            motion_complete = false;
            Stepper_StartMove(&params);
        }
        else if (config_requested)
        {
            config_requested = false;
            state = SLIDER_STATE_CONFIGURING;
            error_code = SLIDER_ERROR_NONE;
        }
        else if (motion_requested)
        {
            motion_requested = false;
            state = SLIDER_STATE_MOVING;
            error_code = SLIDER_ERROR_NONE;

            StepperMoveParams params = {
                .steps = pending_steps,
                .max_speed = pending_speed,
                .acceleration = STEPPER_DEFAULT_ACCEL,
                .on_complete = OnMotionComplete
            };
            motion_complete = false;
            Stepper_StartMove(&params);
        }
        break;

    case SLIDER_STATE_CONFIGURING:
        {
            osMutexRelease(sliderMutexHandle);
            // TMC2209_Result result = TMC2209_ConfigureDefaults();
            osMutexWait(sliderMutexHandle, osWaitForever);

            driver_configured = true;
            state = SLIDER_STATE_IDLE;
            // if (result == TMC2209_OK)
            // {
            //     driver_configured = true;
            //     state = SLIDER_STATE_IDLE;
            // }
            // else
            // {
            //     error_code = SLIDER_ERROR_DRIVER_COMM;
            //     state = SLIDER_STATE_ERROR;
            // }
        }
        break;

    case SLIDER_STATE_HOMING:
        if (motion_complete || HAL_GPIO_ReadPin(end_switch_GPIO_Port, end_switch_Pin) == GPIO_PIN_RESET)
        {
            Stepper_Stop(true);
            homed = true;
            motion_complete = true;
            state = SLIDER_STATE_IDLE;
            Stepper_SetPosition(0); //note: this needs to be after setting the new state
        }
        break;

    case SLIDER_STATE_MOVING:
        if (motion_complete)
        {
            motion_complete = false;
            state = SLIDER_STATE_IDLE;
        }
        break;

    case SLIDER_STATE_ERROR:
        break;
    }

    osMutexRelease(sliderMutexHandle);

    if (state == SLIDER_STATE_MOVING || state == SLIDER_STATE_HOMING)
    {
        osSemaphoreWait(motionSemHandle, 100);
    }
    else
    {
        osDelay(10);
    }
}
