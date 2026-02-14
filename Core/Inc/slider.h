#ifndef SLIDER_H
#define SLIDER_H

#include "stm32l4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define SLIDER_RAIL_LENGTH_STEPS    48000

typedef enum {
    SLIDER_STATE_IDLE,
    SLIDER_STATE_MOVING,
    SLIDER_STATE_HOMING,
    SLIDER_STATE_CONFIGURING,
    SLIDER_STATE_ERROR
} SliderState;

typedef enum {
    SLIDER_OK,
    SLIDER_ERR_BUSY,
    SLIDER_ERR_NOT_HOMED,
    SLIDER_ERR_INVALID_PARAM,
    SLIDER_ERR_OUT_OF_BOUNDS
} SliderResult;

typedef enum {
    SLIDER_ERROR_NONE = 0,
    SLIDER_ERROR_ENDSTOP_NOT_FOUND = 10,
    SLIDER_ERROR_LIMIT_REACHED = 20,
    SLIDER_ERROR_MOVE_TIMEOUT = 21,
    SLIDER_ERROR_DRIVER_COMM = 30
} SliderErrorCode;

typedef struct {
    SliderState state;
    SliderErrorCode error_code;
    int32_t position;
    bool homed;
} SliderStatus;

void Slider_Init(void);
void Slider_Task(void const* argument);

void Slider_Run(void);
SliderStatus Slider_GetStatus(void);
SliderResult Slider_Home(void);
SliderResult Slider_Move(int32_t steps, uint32_t speed);
SliderResult Slider_Stop(void);

SliderResult Slider_ConfigureDriver(void);
bool Slider_IsDriverConfigured(void);

#endif
