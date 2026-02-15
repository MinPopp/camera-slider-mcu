#include "startup_sound.h"
#include "tmc2209.h"
#include "stepper.h"
#include "main.h"
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_A3  220
#define NOTE_AS3 233   // Bb3
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_DS4 311   // Eb4
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_REST 0

typedef struct {
    uint16_t frequency;
    uint16_t duration_ms;
} Note;

static const Note tune[] = {
    { NOTE_G3, 350 },
    { NOTE_REST, 50 },
    { NOTE_G3, 350 },
    { NOTE_REST, 50 },
    { NOTE_G3, 350 },
    { NOTE_REST, 50 },
    { NOTE_DS4, 250 },
    { NOTE_REST, 50 },
    { NOTE_AS3, 120 },
    { NOTE_REST, 50 },
    { NOTE_G3, 350 },
    { NOTE_REST, 50 },
    { NOTE_DS4, 250 },
    { NOTE_REST, 50 },
    { NOTE_AS3, 120 },
    { NOTE_REST, 50 },
    { NOTE_G3, 450 },
    { NOTE_REST, 80 },

    // { NOTE_D4, 350 },
    // { NOTE_REST, 50 },
    // { NOTE_D4, 350 },
    // { NOTE_REST, 50 },
    // { NOTE_D4, 350 },
    // { NOTE_REST, 50 },
    // { NOTE_DS4, 250 },
    // { NOTE_REST, 50 },
    // { NOTE_AS3, 120 },
    // { NOTE_REST, 50 },
    // { NOTE_FS3, 350 },
    // { NOTE_REST, 50 },
    // { NOTE_DS4, 250 },
    // { NOTE_REST, 50 },
    // { NOTE_AS3, 120 },
    // { NOTE_REST, 50 },
    // { NOTE_G3, 450 },
    // { NOTE_REST, 80 },

    // { NOTE_G4, 350 },
    // { NOTE_REST, 50 },
    // { NOTE_G3, 300 },
    // { NOTE_REST, 50 },
    // { NOTE_G3, 120 },
    // { NOTE_REST, 40 },
    // { NOTE_G4, 350 },
    // { NOTE_REST, 50 },
    // { NOTE_FS4, 250 },
    // { NOTE_REST, 50 },
    // { NOTE_F4, 250 },
    // { NOTE_REST, 50 },
    // { NOTE_E4, 250 },
    // { NOTE_REST, 50 },
    // { NOTE_DS4, 250 },
    // { NOTE_REST, 50 },
    // { NOTE_E4, 300 },
    // { NOTE_REST, 80 },

};

static void PlayTone(uint16_t frequency, uint16_t duration_ms)
{
    if (frequency == 0)
    {
        HAL_Delay(duration_ms);
        return;
    }

    uint32_t period_us = 1000000 / frequency;
    uint32_t cycles = (uint32_t)frequency * duration_ms / 1000;

    for (uint32_t i = 0; i < cycles; i++)
    {
        HAL_GPIO_WritePin(STEPPER_STEP_PORT, STEPPER_STEP_PIN, GPIO_PIN_SET);
        for (volatile uint32_t d = 0; d < 20; d++) __NOP();
        HAL_GPIO_WritePin(STEPPER_STEP_PORT, STEPPER_STEP_PIN, GPIO_PIN_RESET);

        uint32_t delay_us = period_us - 1;
        uint32_t delay_ticks = delay_us * (SystemCoreClock / 1000000) / 4;
        for (volatile uint32_t d = 0; d < delay_ticks; d++) __NOP();
    }
}

void StartupSound_Play(void)
{
    TMC2209_ConfigureForSound();

    HAL_GPIO_WritePin(STEPPER_DIR_PORT, STEPPER_DIR_PIN, GPIO_PIN_SET);

    uint32_t num_notes = sizeof(tune) / sizeof(tune[0]);
    for (uint32_t i = 0; i < num_notes; i++)
    {
        PlayTone(tune[i].frequency, tune[i].duration_ms);
    }

    HAL_Delay(50);

    TMC2209_ConfigureForMotion();
}
