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

    HAL_Delay(TMC2209_REPLY_DELAY_MS);

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

    __HAL_UART_FLUSH_DRREGISTER(tmc_uart);

    HAL_StatusTypeDef status = HAL_UART_Transmit(tmc_uart, tx_buf, 4, TMC2209_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        return TMC2209_ERR_COMM;
    }

    HAL_Delay(TMC2209_REPLY_DELAY_MS);

    uint8_t rx_buf[8];
    status = HAL_UART_Receive(tmc_uart, rx_buf, 8, TMC2209_TIMEOUT_MS * 2);
    if (status != HAL_OK)
    {
        return TMC2209_ERR_TIMEOUT;
    }

    uint8_t crc = TMC2209_CalcCRC(rx_buf, 7);
    if (crc != rx_buf[7])
    {
        return TMC2209_ERR_CRC;
    }

    *value = ((uint32_t)rx_buf[3] << 24) |
             ((uint32_t)rx_buf[4] << 16) |
             ((uint32_t)rx_buf[5] << 8) |
             (uint32_t)rx_buf[6];

    return TMC2209_OK;
}

static uint8_t MicrostepsToMRES(uint8_t microsteps)
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
        .microsteps = 16,
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
