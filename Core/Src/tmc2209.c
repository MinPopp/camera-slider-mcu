#include "tmc2209.h"
#include <string.h>

static UART_HandleTypeDef* tmc_uart = NULL;

#define TMC2209_SYNC_BYTE       0x05
#define TMC2209_WRITE_BIT       0x80
#define TMC2209_TIMEOUT_MS      10
#define TMC2209_REPLY_DELAY_MS  2

static uint8_t TMC2209_CalcCRC(uint8_t* data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if ((crc >> 7) ^ (byte & 0x01))
            {
                crc = (crc << 1) ^ 0x07;
            }
            else
            {
                crc = crc << 1;
            }
            byte >>= 1;
        }
    }
    return crc;
}

void TMC2209_Init(UART_HandleTypeDef* huart)
{
    tmc_uart = huart;
}

TMC2209_Result TMC2209_WriteRegister(uint8_t reg, uint32_t value)
{
    if (tmc_uart == NULL) return TMC2209_ERR_COMM;

    uint8_t tx_buf[8];
    tx_buf[0] = TMC2209_SYNC_BYTE;
    tx_buf[1] = TMC2209_SLAVE_ADDR;
    tx_buf[2] = reg | TMC2209_WRITE_BIT;
    tx_buf[3] = (value >> 24) & 0xFF;
    tx_buf[4] = (value >> 16) & 0xFF;
    tx_buf[5] = (value >> 8) & 0xFF;
    tx_buf[6] = value & 0xFF;
    tx_buf[7] = TMC2209_CalcCRC(tx_buf, 7);

    HAL_StatusTypeDef status = HAL_UART_Transmit(tmc_uart, tx_buf, 8, TMC2209_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        return TMC2209_ERR_COMM;
    }

    HAL_Delay(2);

    return TMC2209_OK;
}

TMC2209_Result TMC2209_ReadRegister(uint8_t reg, uint32_t* value)
{
    if (tmc_uart == NULL || value == NULL) return TMC2209_ERR_COMM;

    uint8_t tx_buf[4];
    tx_buf[0] = TMC2209_SYNC_BYTE;
    tx_buf[1] = TMC2209_SLAVE_ADDR;
    tx_buf[2] = reg & 0x7F;
    tx_buf[3] = TMC2209_CalcCRC(tx_buf, 3);

    HAL_StatusTypeDef status = HAL_UART_Transmit(tmc_uart, tx_buf, 4, TMC2209_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        return TMC2209_ERR_COMM;
    }

    __HAL_UART_CLEAR_OREFLAG(tmc_uart);
    __HAL_UART_FLUSH_DRREGISTER(tmc_uart);

    HAL_HalfDuplex_EnableReceiver(tmc_uart);

    uint8_t rx_buf[8] = {0};
    status = HAL_UART_Receive(tmc_uart, rx_buf, 8, 50);

    HAL_HalfDuplex_EnableTransmitter(tmc_uart);

    if (status != HAL_OK)
    {
        return TMC2209_ERR_TIMEOUT;
    }

    uint8_t crc = TMC2209_CalcCRC(rx_buf, 7);
    if (crc != rx_buf[7])
    {
        return TMC2209_ERR_CRC;
    }

    if (rx_buf[1] != 0xFF || rx_buf[2] != reg)
    {
        return TMC2209_ERR_DATA;
    }

    *value = ((uint32_t)rx_buf[3] << 24) |
             ((uint32_t)rx_buf[4] << 16) |
             ((uint32_t)rx_buf[5] << 8) |
             (uint32_t)rx_buf[6];

    return TMC2209_OK;
}

static uint8_t MicrostepsToMRES(uint16_t microsteps)
{
    switch (microsteps)
    {
        case 1:   return 8;
        case 2:   return 7;
        case 4:   return 6;
        case 8:   return 5;
        case 16:  return 4;
        case 32:  return 3;
        case 64:  return 2;
        case 128: return 1;
        case 256: return 0;
        default:  return 4;
    }
}

TMC2209_Result TMC2209_Configure(const TMC2209_Config* config)
{
    TMC2209_Result result;

    uint32_t gconf = TMC2209_GCONF_PDN_DISABLE | TMC2209_GCONF_MSTEP_REG_SELECT;
    if (config->spreadcycle)
    {
        gconf |= TMC2209_GCONF_EN_SPREADCYCLE;
    }
    result = TMC2209_WriteRegister(TMC2209_REG_GCONF, gconf);
    if (result != TMC2209_OK) return result;

    uint32_t ihold_irun = 0;
    ihold_irun |= (config->current.ihold & 0x1F);
    ihold_irun |= ((config->current.irun & 0x1F) << 8);
    ihold_irun |= ((config->current.iholddelay & 0x0F) << 16);
    result = TMC2209_WriteRegister(TMC2209_REG_IHOLD_IRUN, ihold_irun);
    if (result != TMC2209_OK) return result;

    result = TMC2209_WriteRegister(TMC2209_REG_TPOWERDOWN, config->tpowerdown);
    if (result != TMC2209_OK) return result;

    result = TMC2209_WriteRegister(TMC2209_REG_TPWMTHRS, config->tpwmthrs);
    if (result != TMC2209_OK) return result;

    uint8_t mres = MicrostepsToMRES(config->microsteps);
    uint32_t chopconf = 0x10000053;
    chopconf &= ~(0x0F << 24);
    chopconf |= ((uint32_t)mres << 24);
    result = TMC2209_WriteRegister(TMC2209_REG_CHOPCONF, chopconf);
    if (result != TMC2209_OK) return result;

    uint32_t pwmconf = 0xC10D0024;
    result = TMC2209_WriteRegister(TMC2209_REG_PWMCONF, pwmconf);
    if (result != TMC2209_OK) return result;

    return TMC2209_OK;
}

TMC2209_Result TMC2209_ConfigureDefaults(void)
{
    TMC2209_Config config = {
        .microsteps = 8,
        .spreadcycle = false,
        .current = {
            .ihold = 8,
            .irun = 20,
            .iholddelay = 6
        },
        .tpowerdown = 20,
        .tpwmthrs = 0
    };

    return TMC2209_Configure(&config);
}

TMC2209_Result TMC2209_SetCurrent(uint8_t irun, uint8_t ihold, uint8_t iholddelay)
{
    uint32_t value = 0;
    value |= (ihold & 0x1F);
    value |= ((irun & 0x1F) << 8);
    value |= ((iholddelay & 0x0F) << 16);
    return TMC2209_WriteRegister(TMC2209_REG_IHOLD_IRUN, value);
}

TMC2209_Result TMC2209_SetMicrosteps(uint8_t microsteps)
{
    uint32_t chopconf;
    TMC2209_Result result = TMC2209_ReadRegister(TMC2209_REG_CHOPCONF, &chopconf);
    if (result != TMC2209_OK) return result;

    uint8_t mres = MicrostepsToMRES(microsteps);
    chopconf &= ~(0x0F << 24);
    chopconf |= ((uint32_t)mres << 24);

    return TMC2209_WriteRegister(TMC2209_REG_CHOPCONF, chopconf);
}

TMC2209_Result TMC2209_SetSpreadCycle(bool enable)
{
    uint32_t gconf;
    TMC2209_Result result = TMC2209_ReadRegister(TMC2209_REG_GCONF, &gconf);
    if (result != TMC2209_OK) return result;

    if (enable)
    {
        gconf |= TMC2209_GCONF_EN_SPREADCYCLE;
    }
    else
    {
        gconf &= ~TMC2209_GCONF_EN_SPREADCYCLE;
    }

    return TMC2209_WriteRegister(TMC2209_REG_GCONF, gconf);
}

TMC2209_Result TMC2209_ReadDriverStatus(uint32_t* status)
{
    return TMC2209_ReadRegister(TMC2209_REG_DRVSTATUS, status);
}

bool TMC2209_IsConnected(void)
{
    uint32_t ifcnt1, ifcnt2;

    if (TMC2209_ReadRegister(TMC2209_REG_IFCNT, &ifcnt1) != TMC2209_OK)
    {
        return false;
    }

    if (TMC2209_WriteRegister(TMC2209_REG_GSTAT, 0x07) != TMC2209_OK)
    {
        return false;
    }

    if (TMC2209_ReadRegister(TMC2209_REG_IFCNT, &ifcnt2) != TMC2209_OK)
    {
        return false;
    }

    return (ifcnt2 != ifcnt1);
}

TMC2209_Result TMC2209_ConfigureForSound(void)
{
    TMC2209_Result result;

    // SpreadCycle mode for audible chopper noise
    uint32_t gconf = TMC2209_GCONF_PDN_DISABLE |
                     TMC2209_GCONF_MSTEP_REG_SELECT |
                     TMC2209_GCONF_EN_SPREADCYCLE;
    result = TMC2209_WriteRegister(TMC2209_REG_GCONF, gconf);
    if (result != TMC2209_OK) return result;
    HAL_Delay(5);

    // Current settings (Rsense = 0.11 Ohm, vsense = 1 -> Vfs = 0.18V)
    // Irms = (CS+1)/32 * Vfs / (Rsense + 0.02) * 1/sqrt(2)
    // IRUN=10:  Irms = 11/32 * 0.18 / 0.13 * 0.707 = ~337 mA
    // IHOLD=5:  Irms =  6/32 * 0.18 / 0.13 * 0.707 = ~184 mA
    uint8_t ihold = 5;
    uint8_t irun = 10;
    uint8_t iholddelay = 6;
    uint32_t ihold_irun = (ihold & 0x1F)
                        | ((irun & 0x1F) << 8)
                        | ((iholddelay & 0x0F) << 16);
    result = TMC2209_WriteRegister(TMC2209_REG_IHOLD_IRUN, ihold_irun);
    if (result != TMC2209_OK) return result;
    HAL_Delay(5);

    // Chopper: TOFF=3, HSTRT=5, HEND=0, vsense=1 (low range), fullstep, no interpolation
    uint32_t chopconf = 0;
    chopconf |= (3 << 0);              // TOFF: off-time = 3
    chopconf |= (5 << 4);              // HSTRT: hysteresis start = 5
    chopconf |= (0 << 7);              // HEND: hysteresis end = 0
    chopconf |= (1 << 17);             // vsense: 1 = low-range (Vfs = 0.18V)
    chopconf |= TMC2209_CHOPCONF_MRES_1;  // fullstep for maximum audible effect
    result = TMC2209_WriteRegister(TMC2209_REG_CHOPCONF, chopconf);
    if (result != TMC2209_OK) return result;

    return TMC2209_OK;
}

TMC2209_Result TMC2209_ConfigureForMotion(void)
{
    TMC2209_Result result;

    // StealthChop mode for quiet motion (en_SpreadCycle = 0)
    uint32_t gconf = TMC2209_GCONF_PDN_DISABLE | TMC2209_GCONF_MSTEP_REG_SELECT;
    result = TMC2209_WriteRegister(TMC2209_REG_GCONF, gconf);
    if (result != TMC2209_OK) return result;
    HAL_Delay(5);

    // Current settings (Rsense = 0.11 Ohm, vsense = 0 -> Vfs = 0.32V)
    // Irms = (CS+1)/32 * Vfs / (Rsense + 0.02) * 1/sqrt(2)
    // IRUN=13:  Irms = 14/32 * 0.32 / 0.13 * 0.707 = ~761 mA
    // IHOLD=8:  Irms =  9/32 * 0.32 / 0.13 * 0.707 = ~490 mA
    uint8_t ihold = 8;
    uint8_t irun = 13;
    uint8_t iholddelay = 6;
    uint32_t ihold_irun = (ihold & 0x1F)
                        | ((irun & 0x1F) << 8)
                        | ((iholddelay & 0x0F) << 16);
    result = TMC2209_WriteRegister(TMC2209_REG_IHOLD_IRUN, ihold_irun);
    if (result != TMC2209_OK) return result;
    HAL_Delay(5);

    // Chopper: TOFF=3, HSTRT=5, HEND=0, vsense=0 (high range), 8 microsteps + interpolation
    uint32_t chopconf = 0;
    chopconf |= (3 << 0);              // TOFF: off-time = 3
    chopconf |= (5 << 4);              // HSTRT: hysteresis start = 5
    chopconf |= (0 << 7);              // HEND: hysteresis end = 0
    chopconf |= TMC2209_CHOPCONF_MRES_8;  // 8 microsteps
    chopconf |= TMC2209_CHOPCONF_INTPOL;  // interpolation to 256 microsteps
    result = TMC2209_WriteRegister(TMC2209_REG_CHOPCONF, chopconf);
    if (result != TMC2209_OK) return result;
    HAL_Delay(5);

    // StealthChop PWM config
    uint32_t pwmconf = 0;
    pwmconf |= (36 << 0);              // PWM_OFS: amplitude offset = 36
    pwmconf |= (0 << 8);               // PWM_GRAD: velocity gradient = 0
    pwmconf |= (1 << 16);              // pwm_freq: 1 = 2/683 fCLK
    pwmconf |= (1 << 18);              // pwm_autoscale: auto-tune amplitude
    pwmconf |= (1 << 19);              // pwm_autograd: auto-tune gradient
    pwmconf |= (1 << 24);              // pwm_reg: regulation bandwidth = 1
    pwmconf |= ((uint32_t)12 << 28);   // pwm_lim: limit for PWM_GRAD auto-tuning = 12
    result = TMC2209_WriteRegister(TMC2209_REG_PWMCONF, pwmconf);
    if (result != TMC2209_OK) return result;

    return TMC2209_OK;
}

TMC2209_Result TMC2209_ReadAllRegisters(TMC2209_RegisterDump* dump)
{
    if (dump == NULL) return TMC2209_ERR_COMM;

    memset(dump, 0, sizeof(TMC2209_RegisterDump));
    TMC2209_Result result;

    result = TMC2209_ReadRegister(TMC2209_REG_GCONF, &dump->gconf);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_GSTAT, &dump->gstat);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_IFCNT, &dump->ifcnt);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_IOIN, &dump->ioin);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_TSTEP, &dump->tstep);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_SG_RESULT, &dump->sg_result);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_MSCNT, &dump->mscnt);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_MSCURACT, &dump->mscuract);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_CHOPCONF, &dump->chopconf);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_DRVSTATUS, &dump->drvstatus);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }
    HAL_Delay(5);

    result = TMC2209_ReadRegister(TMC2209_REG_PWMCONF, &dump->pwmconf);
    if (result != TMC2209_OK) { dump->last_error = result; return result; }

    return TMC2209_OK;
}

bool TMC2209_UartProbe(bool testMode)
{
    bool success = true;
    uint32_t value;
    TMC2209_RegisterDump dump;
    TMC2209_ReadAllRegisters(&dump);
    uint32_t initialIfcnt = dump.ifcnt;



    success = (TMC2209_WriteRegister(TMC2209_REG_GSTAT, 0x07) == TMC2209_OK) && success;
    HAL_Delay(5);
    success = (TMC2209_ReadRegister(TMC2209_REG_IOIN, &value) == TMC2209_OK) && success;
    HAL_Delay(5);
    success = (TMC2209_ReadRegister(TMC2209_REG_IFCNT, &value) == TMC2209_OK) && success;
    HAL_Delay(5);
    success = (TMC2209_ReadAllRegisters(&dump) == TMC2209_OK) && success;
    success = success && (dump.ifcnt > initialIfcnt);

    if (testMode)
    {
        while (1)
        {
            TMC2209_WriteRegister(TMC2209_REG_GSTAT, 0x07);
            HAL_Delay(500);

            TMC2209_ReadRegister(TMC2209_REG_IOIN, &value);
            HAL_Delay(500);

            TMC2209_ReadRegister(TMC2209_REG_IFCNT, &value);
            HAL_Delay(500);

            TMC2209_ReadAllRegisters(&dump);
            HAL_Delay(1000);
        }
    }

    return success;
}
