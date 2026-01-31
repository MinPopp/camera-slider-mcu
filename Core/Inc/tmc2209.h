#ifndef TMC2209_H
#define TMC2209_H

#include "stm32l4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define TMC2209_SLAVE_ADDR      0x00

#define TMC2209_REG_GCONF       0x00
#define TMC2209_REG_GSTAT       0x01
#define TMC2209_REG_IFCNT       0x02
#define TMC2209_REG_IOIN        0x06
#define TMC2209_REG_IHOLD_IRUN  0x10
#define TMC2209_REG_TPOWERDOWN  0x11
#define TMC2209_REG_TSTEP       0x12
#define TMC2209_REG_TPWMTHRS    0x13
#define TMC2209_REG_TCOOLTHRS   0x14
#define TMC2209_REG_VACTUAL     0x22
#define TMC2209_REG_SGTHRS      0x40
#define TMC2209_REG_SG_RESULT   0x41
#define TMC2209_REG_COOLCONF    0x42
#define TMC2209_REG_MSCNT       0x6A
#define TMC2209_REG_MSCURACT    0x6B
#define TMC2209_REG_CHOPCONF    0x6C
#define TMC2209_REG_DRVSTATUS   0x6F
#define TMC2209_REG_PWMCONF     0x70

#define TMC2209_GCONF_EN_SPREADCYCLE    (1 << 2)
#define TMC2209_GCONF_PDN_DISABLE       (1 << 6)
#define TMC2209_GCONF_MSTEP_REG_SELECT  (1 << 7)
#define TMC2209_GCONF_MULTISTEP_FILT    (1 << 8)

#define TMC2209_CHOPCONF_MRES_SHIFT     24
#define TMC2209_CHOPCONF_MRES_256       (0 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_MRES_128       (1 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_MRES_64        (2 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_MRES_32        (3 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_MRES_16        (4 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_MRES_8         (5 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_MRES_4         (6 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_MRES_2         (7 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_MRES_1         (8 << TMC2209_CHOPCONF_MRES_SHIFT)
#define TMC2209_CHOPCONF_INTPOL         (1 << 28)

typedef enum {
    TMC2209_OK = 0,
    TMC2209_ERR_TIMEOUT,
    TMC2209_ERR_CRC,
    TMC2209_ERR_COMM
} TMC2209_Result;

typedef struct {
    uint8_t ihold;
    uint8_t irun;
    uint8_t iholddelay;
} TMC2209_CurrentConfig;

typedef struct {
    uint16_t microsteps;
    bool spreadcycle;
    TMC2209_CurrentConfig current;
    uint8_t tpowerdown;
    uint32_t tpwmthrs;
} TMC2209_Config;

void TMC2209_Init(UART_HandleTypeDef* huart);

TMC2209_Result TMC2209_WriteRegister(uint8_t reg, uint32_t value);
TMC2209_Result TMC2209_ReadRegister(uint8_t reg, uint32_t* value);

TMC2209_Result TMC2209_Configure(const TMC2209_Config* config);
TMC2209_Result TMC2209_ConfigureDefaults(void);

TMC2209_Result TMC2209_SetCurrent(uint8_t irun, uint8_t ihold, uint8_t iholddelay);
TMC2209_Result TMC2209_SetMicrosteps(uint8_t microsteps);
TMC2209_Result TMC2209_SetSpreadCycle(bool enable);

TMC2209_Result TMC2209_ReadDriverStatus(uint32_t* status);
bool TMC2209_IsConnected(void);

TMC2209_Result TMC2209_ConfigureForSound(void);
TMC2209_Result TMC2209_ConfigureForMotion(void);

#endif
