# STM32F767ZI Bare Metal Embedded Project

A bare metal embedded systems project on the **NUCLEO-F767ZI** development board, built without HAL or CubeMX code generation. Developed as interview preparation for a firmware engineer role, with AI-assisted learning using **Claude (Anthropic)**.

---

## Hardware

| Component | Details |
|---|---|
| Board | NUCLEO-F767ZI |
| MCU | STM32F767ZI — Cortex-M7 @ 216MHz |
| Sensor | TSL2561 ambient light sensor |
| Debugger | J-Link (converted from ST-Link) |
| Debug output | SEGGER RTT via Ozone |

### Wiring — TSL2561 to Nucleo

| TSL2561 Pin | Nucleo Pin | STM32 Pin |
|---|---|---|
| VCC | 3.3V | — |
| GND | GND | — |
| SCL | CN10 pin13 (D26) | PB6 |
| SDA | CN7 pin4 (D14)  | PB9 |

TSL2561 I2C address: **0x39** (7-bit), ADDR pin floating.

---

## Project Structure

```
baremetal-blinky/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       ├── main.c          ← all bare metal code lives here
│       └── SEGGER_RTT_printf.c
├── RTT/
│   ├── SEGGER_RTT.c        ← RTT debug library
│   ├── SEGGER_RTT.h
│   └── SEGGER_RTT_Conf.h
└── Drivers/                ← CMSIS headers only (no HAL used)
```

---

## Features Built

### 1. GPIO Output — LED control
Three user LEDs on GPIOB controlled bare metal:

| LED | Pin | MODER bits |
|---|---|---|
| LD1 Green | PB0 | [1:0] |
| LD2 Blue | PB7 | [15:14] |
| LD3 Red | PB14 | [29:28] |

### 2. GPIO Input — Button state machine
User button on PC13 (active HIGH on F767ZI). Each press cycles through Green → Blue → Red with debounce.

### 3. RTT Debug Output
SEGGER RTT prints through J-Link debug connection — no UART hardware needed. Viewed in Ozone terminal.

### 4. I2C Bare Metal Driver
Full I2C1 driver using F767 register set (completely different from F1/F4):
- Bus scanner to detect device addresses
- `I2C_WriteReg()` — write one byte to a register
- `I2C_ReadByte()` — read one byte from a register
- `I2C_ReadWord()` — read two bytes (for 16-bit ADC values)

### 5. TSL2561 Light Sensor
- Power up via CONTROL register
- ID register readback for communication verification
- CH0 (visible + IR) and CH1 (IR only) ADC channel reads
- Integer-only lux calculation from datasheet piecewise formula

---

## Register Reference

### RCC — Reset and Clock Control

#### RCC->AHB1ENR (AHB1 peripheral clock enable)
```
Bit:  31 ... 8  7  6  5  4  3  2  1  0
      [reserved] [GPIOH] [GPIOG] [GPIOF] [GPIOE] [GPIOD] [GPIOC] [GPIOB] [GPIOA]
```
```c
RCC->AHB1ENR |= (1U << 1);  // enable GPIOB clock
RCC->AHB1ENR |= (1U << 2);  // enable GPIOC clock
```

#### RCC->APB1ENR (APB1 peripheral clock enable)
```
Bit: 31 ... 22  21  20 ...
               [I2C1]
```
```c
RCC->APB1ENR |= (1U << 21);  // enable I2C1 clock
```

---

### GPIO Registers

#### GPIOx->MODER (pin mode — 2 bits per pin)
```
Bit: 31 30 | 29 28 | ... | 3  2 | 1  0
     [PB15] [PB14]        [PB1] [PB0]

Each 2-bit field:
  00 = Input
  01 = Output
  10 = Alternate Function
  11 = Analog
```
```c
// Clear-then-set pattern — never write blindly
GPIOB->MODER &= ~(3U << (pin * 2));  // clear 2 bits
GPIOB->MODER |=  (mode << (pin * 2)); // set mode
```

#### GPIOx->OTYPER (output type — 1 bit per pin)
```
Bit: 15 14 ... 1  0
     [PB15]    [PB1][PB0]

  0 = Push-pull (default, for LEDs)
  1 = Open-drain (required for I2C)
```

#### GPIOx->OSPEEDR (output speed — 2 bits per pin)
```
  00 = Low
  01 = Medium
  10 = High
  11 = Very High
```

#### GPIOx->PUPDR (pull-up/pull-down — 2 bits per pin)
```
  00 = No pull
  01 = Pull-up
  10 = Pull-down
```

#### GPIOx->ODR (output data register — 1 bit per pin)
```
Bit: 15 14 ... 1  0
     [PB15]    [PB1][PB0]
```
```c
GPIOB->ODR |=  (1U << 0);   // set PB0 high
GPIOB->ODR &= ~(1U << 0);   // set PB0 low
GPIOB->ODR ^=  (1U << 0);   // toggle PB0
```

#### GPIOx->IDR (input data register — read only)
```c
if (GPIOC->IDR & (1U << 13))  // read PC13
```

#### GPIOx->AFR[0] and AFR[1] (alternate function)
```
AFR[0] controls pins 0–7  (4 bits per pin)
AFR[1] controls pins 8–15 (4 bits per pin)

Pin n in AFR[0]: bits [(n*4)+3 : n*4]
Pin n in AFR[1]: bits [((n-8)*4)+3 : (n-8)*4]

AF4 = I2C1/2/3/4
```
```c
GPIOB->AFR[0] |= (4U << 24);  // PB6 → AF4 (I2C1_SCL)
GPIOB->AFR[1] |= (4U <<  4);  // PB9 → AF4 (I2C1_SDA)
```

---

### I2C Registers (F767 — different from F1/F4)

> ⚠️ The STM32F7 I2C peripheral is a new design. Register names like SR1, SR2, DR, CCR, TRISE **do not exist** on F7. Do not use F1/F4 examples.

#### I2C1->CR1 (control register 1 — set once at init)
```
Bit: 31 ... 8  7      4      1      0
              [ERRIE] [RXIE] [TXIE] [PE]

PE = Peripheral Enable — set last after all config
```
```c
I2C1->CR1 |= I2C_CR1_PE;
```

#### I2C1->CR2 (control register 2 — rewrite per transaction)
```
Bit: 31 ... 26   25       24      23..16   13      10     9..1    0
              [AUTOEND] [RELOAD] [NBYTES] [START] [RD_WRN] [SADD]

SADD    [9:1]  = 7-bit slave address (place in bits [7:1])
NBYTES [23:16] = number of bytes to transfer
RD_WRN  [10]   = 0:write, 1:read
AUTOEND [25]   = auto generate STOP after last byte
RELOAD  [24]   = reload mode — wait for TCR before continuing
START   [13]   = generate START condition
```
```c
// Write transaction example
I2C1->CR2 = (0x39 << 1)       // SADD = 7-bit address
           | (2U << 16)        // NBYTES = 2
           | I2C_CR2_AUTOEND;  // auto STOP
I2C1->CR2 |= I2C_CR2_START;
```

#### I2C1->ISR (interrupt and status register — read only)
```
Bit: 15    7      6    5      4      2      1      0
    [BUSY] [TCR] [TC] [STOPF] [NACKF] [RXNE] [TXIS] [TXE]

TXE   [0]  = TX buffer empty
TXIS  [1]  = ready to write next byte to TXDR
RXNE  [2]  = received byte ready in RXDR
NACKF [4]  = NACK received from slave
STOPF [5]  = STOP detected
TC    [6]  = transfer complete (all bytes sent, bus held)
TCR   [7]  = transfer complete reload (NBYTES done in RELOAD mode)
BUSY  [15] = bus busy
```

> **Key lesson:** When a device NACKs, both `NACKF` and `STOPF` are set.  
> Always check `NACKF` first — `STOPF` alone means ACK (device found).

#### I2C1->ICR (interrupt clear register — write only)
```c
I2C1->ICR = I2C_ICR_STOPCF;  // clear STOPF
I2C1->ICR = I2C_ICR_NACKCF;  // clear NACKF
```

> **Key lesson:** Always clear stale flags before starting a new transaction,  
> especially STOPF left over from a previous write before doing a read.

#### I2C1->TIMINGR (replaces CCR + TRISE from F1/F4)
```c
I2C1->TIMINGR = 0x00303D5B;  // 100kHz at 16MHz APB1 (HSI default)
                               // value from STM32CubeMX timing calculator
```

#### I2C1->TXDR / RXDR (data registers — separate unlike F1/F4 DR)
```c
I2C1->TXDR = byte_to_send;
uint8_t received = I2C1->RXDR;
```

---

## TSL2561 Command Byte Format

```
Bit:  7    6     5     4    3  2  1  0
     [CMD] [CLR] [WORD] [BLK] [  ADDRESS  ]

CMD  must always be 1
WORD set for 16-bit word reads (reads 2 sequential registers)
ADDRESS selects the target register (Table 2 in datasheet)
```

### Key commands used
```c
0x80  // CMD | reg 0x00 — CONTROL register (power up/down)
0x81  // CMD | reg 0x01 — TIMING register (gain/integration)
0x8A  // CMD | reg 0x0A — ID register
0xAC  // CMD | WORD | reg 0x0C — read CH0 (16-bit)
0xAE  // CMD | WORD | reg 0x0E — read CH1 (16-bit)
```

### Power up sequence
```c
I2C_WriteReg(0x80, 0x03);  // write 0x03 to CONTROL — powers up ADC
delay(1000000);             // wait for first conversion (~402ms default)
```

---

## I2C Read Sequence (F767 repeated start)

```
Master:  START | ADDR+W | CMD_BYTE | REPEATED_START | ADDR+R | NACK | STOP
Slave:                  |   ACK   |                |  ACK  | DATA |
```

The critical detail: after the write phase, wait for `TC` (not `STOPF`) — this holds the bus for the repeated START without releasing it.

---

## Debugging Tools Used

| Tool | Purpose |
|---|---|
| Ozone | J-Link debugger, RTT terminal viewer |
| SEGGER RTT | Printf-style debug output through debug probe |
| I2C bus scanner | Custom bare metal scanner using ISR NACKF/STOPF flags |
| Breakpoints + Local Data view | Verified I2C address during initial debug |

---

## Common Mistakes Encountered

**Wrong LED pin** — NUCLEO-F767ZI LEDs are on GPIOB (PB0, PB7, PB14), not GPIOA like F401.

**F1/F4 I2C registers on F7** — SR1, SR2, DR, CCR, TRISE do not exist on F767. Use ISR, TXDR, RXDR, TIMINGR.

**Stale STOPF flag** — leftover STOPF from a write transaction causes the subsequent read to fail. Always call `I2C1->ICR = I2C_ICR_STOPCF` before starting a read.

**NACKF + STOPF both set on NACK** — checking only STOPF gives false positives in the bus scanner. Must check `STOPF && !NACKF` for a real ACK.

**RELOAD vs TC** — use `TC` (transfer complete) for repeated start. `TCR` is only for RELOAD mode where you continue the same transaction with more bytes.

**ODR bit clear wrong** — `GPIOB->ODR &= (1U << 7)` clears all bits except bit 7. Correct: `GPIOB->ODR &= ~(1U << 7)`.

---

## Build Environment

| Tool | Version |
|---|---|
| STM32CubeIDE | 1.19.0 |
| GCC ARM | 13.3.1 (arm-none-eabi) |
| SEGGER J-Link | Installed, ST-Link converted to J-Link |
| Ozone | SEGGER Ozone debugger |
| SEGGER RTT | Included in J-Link installation |

---

## AI Assistance

This project was developed with the assistance of **Claude (Anthropic)** — claude.ai — used as a learning and debugging partner throughout the development process. Claude helped with:

- Explaining bare metal concepts (clock gating, MODER bit patterns, read-modify-write)
- Identifying the F767 vs F1/F4 I2C register differences
- Debugging the NACKF/STOPF scanner logic
- Interpreting ISR register values during live debugging
- Walking through the TSL2561 datasheet command byte format
- Explaining the TC vs TCR distinction for repeated start

All code was written, tested, and debugged on real hardware by the developer.

---

## Next Steps

- [x] Read CH0 and CH1 and calculate lux using integer piecewise formula
- [ ] Bare metal UART driver (USART3, PD8/PD9) once USB-UART adapter arrives
- [ ] PWM generation from bare metal timer registers
- [ ] FreeRTOS on STM32 — sensor task + display task with queue
