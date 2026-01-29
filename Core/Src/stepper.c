#include "stepper.h"
#include "main.h"

extern TIM_HandleTypeDef htim2;

typedef enum {
    PHASE_IDLE,
    PHASE_ACCEL,
    PHASE_CRUISE,
    PHASE_DECEL
} StepPhase;

static volatile struct {
    int32_t position;
    int32_t target_steps;
    int32_t steps_done;
    int32_t steps_to_go;

    uint32_t current_speed;
    uint32_t max_speed;
    uint32_t accel;

    int32_t accel_steps;
    int32_t decel_start;

    bool direction;
    StepPhase phase;
    StepperCallback callback;
} state;

static inline void SetDirection(bool forward)
{
    if (forward)
        HAL_GPIO_WritePin(STEPPER_DIR_PORT, STEPPER_DIR_PIN, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(STEPPER_DIR_PORT, STEPPER_DIR_PIN, GPIO_PIN_RESET);
}

static inline void StepPulse(void)
{
    HAL_GPIO_WritePin(STEPPER_STEP_PORT, STEPPER_STEP_PIN, GPIO_PIN_SET);
    volatile uint32_t delay = (STEPPER_TIMER_CLOCK_HZ / 1000000U) * STEPPER_MIN_PULSE_US / 4;
    while (delay--) __NOP();
    HAL_GPIO_WritePin(STEPPER_STEP_PORT, STEPPER_STEP_PIN, GPIO_PIN_RESET);
}

static inline uint32_t SpeedToInterval(uint32_t speed)
{
    if (speed == 0) speed = 1;
    return STEPPER_TIMER_CLOCK_HZ / speed;
}

static inline uint32_t CalcDecelSteps(uint32_t speed, uint32_t accel)
{
    if (accel == 0) return 0;
    return (uint32_t)(((uint64_t)speed * speed) / (2ULL * accel));
}

static void Timer_SetInterval(uint32_t ticks)
{
    __HAL_TIM_SET_AUTORELOAD(&htim2, ticks - 1);
    __HAL_TIM_SET_COUNTER(&htim2, 0);
}

static void Timer_Start(void)
{
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(&htim2);
}

static void Timer_Stop(void)
{
    HAL_TIM_Base_Stop_IT(&htim2);
}

void Stepper_Init(void)
{
    state.position = 0;
    state.phase = PHASE_IDLE;

    HAL_GPIO_WritePin(STEPPER_STEP_PORT, STEPPER_STEP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STEPPER_DIR_PORT, STEPPER_DIR_PIN, GPIO_PIN_RESET);
}

bool Stepper_IsRunning(void)
{
    return state.phase != PHASE_IDLE;
}

int32_t Stepper_GetPosition(void)
{
    return state.position;
}

void Stepper_SetPosition(int32_t position)
{
    if (!Stepper_IsRunning())
    {
        state.position = position;
    }
}

bool Stepper_StartMove(const StepperMoveParams* params)
{
    if (Stepper_IsRunning()) return false;
    if (params->steps == 0) return false;

    state.direction = (params->steps > 0);
    state.target_steps = params->steps > 0 ? params->steps : -params->steps;
    state.steps_done = 0;
    state.steps_to_go = state.target_steps;

    state.max_speed = params->max_speed;
    if (state.max_speed < STEPPER_MIN_SPEED) state.max_speed = STEPPER_MIN_SPEED;
    if (state.max_speed > STEPPER_MAX_SPEED) state.max_speed = STEPPER_MAX_SPEED;

    state.accel = params->acceleration;
    if (state.accel == 0) state.accel = STEPPER_DEFAULT_ACCEL;

    state.current_speed = STEPPER_MIN_SPEED;
    state.accel_steps = 0;

    uint32_t full_accel_steps = CalcDecelSteps(state.max_speed, state.accel);

    if (2 * full_accel_steps >= (uint32_t)state.target_steps)
    {
        state.decel_start = state.target_steps / 2;
    }
    else
    {
        state.decel_start = state.target_steps - full_accel_steps;
    }

    state.callback = params->on_complete;

    SetDirection(state.direction);

    state.phase = PHASE_ACCEL;

    Timer_SetInterval(SpeedToInterval(state.current_speed));
    Timer_Start();

    return true;
}

void Stepper_Stop(void)
{
    if (state.phase != PHASE_IDLE)
    {
        if (state.phase != PHASE_DECEL && state.current_speed > STEPPER_MIN_SPEED)
        {
            uint32_t decel_steps = CalcDecelSteps(state.current_speed, state.accel);
            state.decel_start = state.steps_done;
            state.target_steps = state.steps_done + decel_steps;
            state.steps_to_go = decel_steps;
            state.phase = PHASE_DECEL;
        }
        else
        {
            Timer_Stop();
            state.phase = PHASE_IDLE;
            if (state.callback)
            {
                state.callback(false, state.position);
            }
        }
    }
}

void Stepper_TimerISR(void)
{
    if (__HAL_TIM_GET_FLAG(&htim2, TIM_FLAG_UPDATE) == RESET) return;
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);

    if (state.phase == PHASE_IDLE) return;

    StepPulse();

    if (state.direction)
        state.position++;
    else
        state.position--;

    state.steps_done++;
    state.steps_to_go--;

    if (state.steps_to_go <= 0)
    {
        Timer_Stop();
        StepPhase prev_phase = state.phase;
        state.phase = PHASE_IDLE;
        if (state.callback)
        {
            state.callback(prev_phase != PHASE_IDLE, state.position);
        }
        return;
    }

    switch (state.phase)
    {
    case PHASE_ACCEL:
        state.accel_steps++;
        {
            uint32_t speed_sq = (uint32_t)STEPPER_MIN_SPEED * STEPPER_MIN_SPEED 
                              + 2ULL * state.accel * state.accel_steps;
            uint32_t new_speed = 1;
            for (uint32_t s = state.current_speed; s * s <= speed_sq; s++)
            {
                new_speed = s;
            }
            state.current_speed = new_speed;
        }

        if (state.current_speed >= state.max_speed)
        {
            state.current_speed = state.max_speed;
            state.phase = PHASE_CRUISE;
        }
        else if (state.steps_done >= state.decel_start)
        {
            state.phase = PHASE_DECEL;
        }
        break;

    case PHASE_CRUISE:
        if (state.steps_done >= state.decel_start)
        {
            state.phase = PHASE_DECEL;
        }
        break;

    case PHASE_DECEL:
        {
            int32_t decel_step = state.steps_done - state.decel_start;
            uint32_t speed_sq = (uint32_t)state.max_speed * state.max_speed;
            uint32_t reduction = 2ULL * state.accel * decel_step;
            
            if (reduction >= speed_sq)
            {
                state.current_speed = STEPPER_MIN_SPEED;
            }
            else
            {
                uint32_t target_sq = speed_sq - reduction;
                uint32_t new_speed = state.current_speed;
                while (new_speed > STEPPER_MIN_SPEED && new_speed * new_speed > target_sq)
                {
                    new_speed--;
                }
                state.current_speed = new_speed;
            }
        }

        if (state.current_speed <= STEPPER_MIN_SPEED)
        {
            state.current_speed = STEPPER_MIN_SPEED;
        }
        break;

    case PHASE_IDLE:
        break;
    }

    Timer_SetInterval(SpeedToInterval(state.current_speed));
}
