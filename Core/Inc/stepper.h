#ifndef STEPPER_H
#define STEPPER_H

#include "stm32l4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define STEPPER_STEP_PORT   Mot1_step_GPIO_Port
#define STEPPER_STEP_PIN    Mot1_step_Pin
#define STEPPER_DIR_PORT    Mot1_dir_GPIO_Port
#define STEPPER_DIR_PIN     Mot1_dir_Pin

#define STEPPER_TIMER_CLOCK_HZ  32000000U
#define STEPPER_MIN_PULSE_US    2U

#define STEPPER_DEFAULT_ACCEL   600U
#define STEPPER_HOME_SPEED      500
#define STEPPER_MIN_SPEED       50U
#define STEPPER_MAX_SPEED       5000U

typedef void (*StepperCallback)(bool completed, int32_t position);

typedef struct {
    int32_t steps;
    uint32_t max_speed;
    uint32_t acceleration;
    StepperCallback on_complete;
} StepperMoveParams;

void Stepper_Init(void);
bool Stepper_IsRunning(void);
int32_t Stepper_GetPosition(void);
void Stepper_SetPosition(int32_t position);

bool Stepper_StartMove(const StepperMoveParams* params);
void Stepper_Stop(bool faststop);

void Stepper_TimerISR(void);

#endif
