#include "stm32f7xx.h"
#include "SEGGER_RTT.h"

#define TSL2561_ADDR   0x39        // 7-bit address
#define TSL2561_ADDR_W (0x39 << 1) // 0x72 — write
#define TSL2561_ADDR_R (0x39 << 1 | 1) // 0x73 — read

void delay(volatile uint32_t count) {
    while (count--);
}

void I2C1_Init(void) {
    RCC->AHB1ENR  |= (1U << 1);
    RCC->APB1ENR  |= (1U << 21);

    // PB6=SCL, PB9=SDA alternate function
    GPIOB->MODER &= ~((3U << 12) | (3U << 18));
    GPIOB->MODER |=  ((2U << 12) | (2U << 18));
    GPIOB->OTYPER |= (1U << 6) | (1U << 9);
    GPIOB->PUPDR &= ~((3U << 12) | (3U << 18));
    GPIOB->PUPDR |=  ((1U << 12) | (1U << 18));
    GPIOB->OSPEEDR |= (3U << 12) | (3U << 18);
    GPIOB->AFR[0] &= ~(0xFU << 24); GPIOB->AFR[0] |= (4U << 24); // PB6 AF4
    GPIOB->AFR[1] &= ~(0xFU <<  4); GPIOB->AFR[1] |= (4U <<  4); // PB9 AF4

    // Reset I2C1
    I2C1->CR1 = 0;

    // TIMINGR for 100kHz, APB1=16MHz (HSI)
    // From STM32CubeMX for 16MHz: 0x00303D5B
    I2C1->TIMINGR = 0x00303D5B;

    I2C1->CR1 |= I2C_CR1_PE;
}
uint8_t I2C_ReadByte(uint8_t cmd, uint8_t *out) {

	// Clear any stale flags first
	I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF; // ICR - Interrupt Clear Register, Clearing stale stop and nack flags

    // ── Write phase: send command byte, no AUTOEND, no RELOAD
	// Just wait for TC then do repeated START
	I2C1->CR2 = 0;
    I2C1->CR2 = (TSL2561_ADDR_W)        // slave address (8-bit)
              | (1U << 16)  ;            // 1 byte to send


    I2C1->CR2 |= I2C_CR2_START;

    // Wait for TXIS — ready to send data byte
    uint32_t timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "ReadByte: TXIS timeout\n"); return 1; }
    I2C1->TXDR = cmd;

    // Wait for transfer complete (TCR — because RELOAD was set)
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TC) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "ReadByte: TC timeout\n"); return 1; }


    // ── Read phase: repeated start, 1 byte read ──────────────────────────────
    I2C1->CR2 = 0;
    I2C1->CR2 = (TSL2561_ADDR_R)        // read address (bit0=1)
              | (1U << 16)              // 1 byte to receive
              | I2C_CR2_AUTOEND         // generate STOP after last byte
              | I2C_CR2_RD_WRN;        // read direction

    I2C1->CR2 |= I2C_CR2_START;        // repeated START

    // Wait for RXNE — data ready
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_RXNE) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "ReadByte: RXNE timeout\n"); return 1; }
    *out = I2C1->RXDR;

    // Wait for STOP
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF) && timeout--);
    I2C1->ICR = I2C_ICR_STOPCF;

    return 0;
}

uint8_t I2C_WriteReg(uint8_t cmd, uint8_t data) {

	// Clear CR2 first
	I2C1->CR2 = 0;

    I2C1->CR2 = (TSL2561_ADDR_W)
              | (2U << 16)             // 2 bytes: cmd + data
              | I2C_CR2_AUTOEND;

    I2C1->CR2 |= I2C_CR2_START;

    uint32_t timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "WriteReg: TXIS timeout\n"); return 1; }
    I2C1->TXDR = cmd;

    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "WriteReg: TXIS2 timeout\n"); return 1; }
    I2C1->TXDR = data;

    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "WriteReg: STOPF timeout\n"); return 1; }
    I2C1->ICR = I2C_ICR_STOPCF;

    return 0;
}

uint8_t I2C_ReadWord(uint8_t cmd, uint16_t *out) {
    // Clear stale flags
    I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;

    // Write phase — send command byte
    I2C1->CR2 = 0;
    I2C1->CR2 = (TSL2561_ADDR_W)
              | (1U << 16);           // 1 byte write, no AUTOEND

    I2C1->CR2 |= I2C_CR2_START;

    uint32_t timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "ReadWord: TXIS timeout\n"); return 1; }
    I2C1->TXDR = cmd;

    // Wait for TC
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TC) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "ReadWord: TC timeout\n"); return 1; }

    // Read phase — 2 bytes, repeated START
    I2C1->CR2 = 0;
    I2C1->CR2 = (TSL2561_ADDR_W)
              | (2U << 16)            // 2 bytes
              | I2C_CR2_AUTOEND
              | I2C_CR2_RD_WRN;

    I2C1->CR2 |= I2C_CR2_START;

    // Read low byte
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_RXNE) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "ReadWord: RXNE1 timeout\n"); return 1; }
    uint8_t lo = I2C1->RXDR;

    // Read high byte
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_RXNE) && timeout--);
    if (!timeout) { SEGGER_RTT_WriteString(0, "ReadWord: RXNE2 timeout\n"); return 1; }
    uint8_t hi = I2C1->RXDR;

    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF) && timeout--);
    I2C1->ICR = I2C_ICR_STOPCF;

    *out = ((uint16_t)hi << 8) | lo;
    return 0;
}

uint32_t Calculate_Lux(uint16_t ch0, uint16_t ch1) {
    // Using 101ms integration time, 1x gain (what we configured)
    // Scale factors from datasheet
    uint32_t chScale = 0x0FE7;  // CHSCALE_TINT1 for 101ms

    // 1x gain — scale up by 16
    chScale <<= 4;

    uint32_t channel0 = ((uint32_t)ch0 * chScale) >> 10;
    uint32_t channel1 = ((uint32_t)ch1 * chScale) >> 10;

    // CH1/CH0 ratio
    uint32_t ratio = 0;
    if (channel0 != 0) {
        uint32_t ratio1 = (channel1 << 10) / channel0;
        ratio = (ratio1 + 1) >> 1;
    }

    // T package piecewise coefficients (datasheet p.24)
    uint32_t b = 0, m = 0;
    if      (ratio <= 0x0040) { b = 0x01F2; m = 0x01BE; }
    else if (ratio <= 0x0080) { b = 0x0214; m = 0x02D1; }
    else if (ratio <= 0x00C0) { b = 0x023F; m = 0x037B; }
    else if (ratio <= 0x0100) { b = 0x0270; m = 0x03FE; }
    else if (ratio <= 0x0138) { b = 0x016F; m = 0x01FC; }
    else if (ratio <= 0x019A) { b = 0x00D2; m = 0x00FB; }
    else if (ratio <= 0x029A) { b = 0x0018; m = 0x0012; }
    else                      { b = 0x0000; m = 0x0000; }

    uint32_t temp = 0;
    uint32_t term0 = channel0 * b;
    uint32_t term1 = channel1 * m;
    if (term0 > term1)
        temp = term0 - term1;

    temp += (1U << 13);   // round
    return temp >> 14;    // strip fractional bits
}

void I2C_Scan(void) {
    SEGGER_RTT_WriteString(0, "Scanning I2C bus...\n");
    uint8_t found = 0;

    for (uint8_t addr7 = 0x08; addr7 < 0x78; addr7++) {
    	// Clear any previous flags
    	I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;

        // Configure transfer: 7-bit addr, 0 bytes, write, autoend
        I2C1->CR2 = ((uint32_t)addr7 << 1) | (0 << 16) | I2C_CR2_AUTOEND;

        // Generate START
        I2C1->CR2 |= I2C_CR2_START;

        // Wait for STOP or NACK
        uint32_t timeout = 100000;
        while (!(I2C1->ISR & (I2C_ISR_STOPF | I2C_ISR_NACKF)) && timeout--);

        // NACKF means no device — STOPF alone means ACK received
        if ((I2C1->ISR & I2C_ISR_STOPF) && !(I2C1->ISR & I2C_ISR_NACKF)) {
              SEGGER_RTT_printf(0, "Found: 7-bit=0x%02X  8-bit=0x%02X\n",
              addr7, addr7 << 1);
              found++;
        }

        // Clear flags
        I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF;
        delay(5000);
    }

    if (found == 0)
        SEGGER_RTT_WriteString(0, "No devices found — check wiring!\n");
    else
        SEGGER_RTT_printf(0, "Scan done. %d device(s) found.\n", found);
}

int main(void) {
	uint8_t led_state = 0;
    // Enable GPIOB clock on AHB1 bus
    RCC->AHB1ENR |= (1U << 1);
    // Enable GPIOC clock on AHB1 bus
    RCC->AHB1ENR |= (1U<<2);

    // Configure PB0 as output (LD1 green LED)
    GPIOB->MODER &= ~(3U << 0);   // clear bits 1:0
    GPIOB->MODER |=  (1U << 0);   // set to output mode

    GPIOB->MODER &= ~(3U << 14);  // clear bits 15:14
    GPIOB->MODER |=  (1U << 14);  // set to 01 = output

    GPIOB->MODER &= ~(3U << 28);  // clear bits 15:14
    GPIOB->MODER |=  (1U << 28);  // set to 01 = output

    // Configure PC13 as input (default after reset is already input)
    // but let's be explicit — set bits [27:26] to 00
    GPIOC->MODER &= ~(3U << 26);

    // Initialize RTT
    SEGGER_RTT_Init();
    SEGGER_RTT_WriteString(0, "System started!\n");


    I2C1_Init();
    I2C_Scan();

    // Power up
    uint8_t write_result = I2C_WriteReg(0x80, 0x03);
    SEGGER_RTT_printf(0, "PowerUp write result: %d\n", write_result);
    delay(1000000);


    // Check ISR state before read
    SEGGER_RTT_printf(0, "ISR before read: 0x%08X\n", (unsigned int)I2C1->ISR);

    // Read ID register
    uint8_t id = 0;
    uint8_t read_result = I2C_ReadByte(0x8A, &id);
    SEGGER_RTT_printf(0, "Read result: %d  ID: 0x%02X\n", read_result, id);

    while (1) {

    	uint16_t ch0 = 0, ch1 = 0;

    	    // Command 0xAC = CMD(0x80) | WORD(0x20) | reg 0x0C
    	    I2C_ReadWord(0xAC, &ch0);

    	    // Command 0xAE = CMD(0x80) | WORD(0x20) | reg 0x0E
    	    I2C_ReadWord(0xAE, &ch1);

    	    uint32_t lux = Calculate_Lux(ch0, ch1);

    	    SEGGER_RTT_printf(0, "CH0=%d  CH1=%d  Lux=%d\n", ch0, ch1, lux);



    	if (GPIOC->IDR & (1U <<13)){

    		while(GPIOC->IDR & (1U <<13));
    		delay(50000);

    		GPIOB->ODR &= ~(1U << 0);
    		GPIOB->ODR &= ~(1U << 7);
    		GPIOB->ODR &= ~(1U << 14);

    		if( led_state == 0) {
    			SEGGER_RTT_WriteString(0, "Green ON\n");
    			GPIOB->ODR |= (1U << 0);
    		}
    		else if ( led_state == 1) {
    			SEGGER_RTT_WriteString(0, "Blue ON\n");
    			GPIOB->ODR |= (1U << 7);
    		}
    		else {
    			SEGGER_RTT_WriteString(0, "Red ON\n");
    			GPIOB->ODR |= (1U << 14);
    		}

    		led_state = (led_state + 1) % 3;
    	}


    }
}
