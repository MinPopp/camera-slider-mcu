# Camera Slider MCU Firmware

Firmware for an STM32L432-based motorized camera slider controller.

## Hardware

- **MCU**: STM32L432 (ARM Cortex-M4, 80MHz)
- **Stepper Driver**: TMC2209 with UART configuration
- **End Switch**: Limit switch for homing

### Pin Assignments

| Function | Port/Pin |
|----------|----------|
| USART1 TX (Commands) | PB6 |
| USART1 RX (Commands) | PB7 |
| USART2 TX (TMC2209) | PA2 |
| USART2 RX (TMC2209) | PA3 |
| Stepper STEP | Mot1_step |
| Stepper DIR | Mot1_dir |
| End Switch | end_switch |

## Architecture

```
┌─────────────────┐     UART1      ┌──────────────────┐
│  Host/Remote    │◄──────────────►│   Command Parser │
└─────────────────┘                └────────┬─────────┘
                                            │
                                            ▼
                                   ┌────────────────┐
                                   │     Slider     │
                                   │  State Machine │
                                   └───────┬────────┘
                                           │
                          ┌────────────────┼────────────────┐
                          ▼                                 ▼
                 ┌─────────────────┐              ┌─────────────────┐
                 │     Stepper     │              │     TMC2209     │
                 │ (Motion Control)│              │  (Driver Config)│
                 └────────┬────────┘              └────────┬────────┘
                          │                                │
                    Timer IRQ                           UART2
                          │                                │
                          ▼                                ▼
                 ┌─────────────────┐              ┌─────────────────┐
                 │   STEP/DIR      │              │   TMC2209 IC    │
                 │   Signals       │              │                 │
                 └─────────────────┘              └─────────────────┘
```

## Modules

### slider.c / slider.h
High-level state machine managing slider operations:
- **States**: IDLE, MOVING, HOMING, CONFIGURING, ERROR
- Thread-safe API with mutex protection
- Coordinates between motion control and driver configuration

### stepper.c / stepper.h
Low-level stepper motor control with trapezoidal motion profiles:
- Acceleration/deceleration phases
- Timer-interrupt-driven pulse generation
- Position tracking

### tmc2209.c / tmc2209.h
TMC2209 stepper driver UART communication:
- Register read/write with CRC-8 checksums
- Current control (IRUN/IHOLD)
- Microstepping configuration
- StealthChop/SpreadCycle mode selection

### command_parser.c / command_parser.h
UART command interface (DMA-based reception on UART1):
- Parses incoming commands
- Dispatches to slider control functions

## Building

### Prerequisites
- ARM GCC toolchain (`arm-none-eabi-gcc`)
- CMake 3.22+
- Ninja (optional, recommended)

### Build Commands

```bash
# Configure
cmake -B build -G Ninja --preset default

# Build
cmake --build build

# Or for release build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The output binary is `build/slider-mcu.elf`.

## Flashing

Use OpenOCD, ST-Link, or your preferred programmer:

```bash
# Using OpenOCD
openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
  -c "program build/slider-mcu.elf verify reset exit"

# Using ST-Link
st-flash --format ihex write build/slider-mcu.hex
```

## UART Protocol

### Command Interface (UART1 - 115200 baud)
Text-based commands for motion control. See `command_parser.c` for details.

### TMC2209 Interface (UART2 - 115200 baud)
Binary protocol with CRC-8 checksums for driver configuration. The TMC2209 is configured automatically at startup with default parameters:
- 8 microsteps (interpolated to 256 by TMC2209)
- StealthChop mode (quiet operation)
- IRUN: 20 (run current)
- IHOLD: 8 (hold current)

Driver reconfiguration can be triggered via `Slider_ConfigureDriver()`.

## Configuration

Default stepper parameters (in `stepper.h`):
- `STEPPER_DEFAULT_ACCEL`: 600 steps/s²
- `STEPPER_HOME_SPEED`: 500 steps/s
- `STEPPER_MIN_SPEED`: 50 steps/s
- `STEPPER_MAX_SPEED`: 5000 steps/s

TMC2209 defaults (in `tmc2209.c`):
- Microsteps: 8 (interpolated to 256)
- Run current: ~1.25A (IRUN=20)
- Hold current: ~0.5A (IHOLD=8)
- Mode: StealthChop (quiet)

## License

See LICENSE file if present, otherwise contact the author.
