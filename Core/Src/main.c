#include "stm32f7xx.h"
#include "SEGGER_RTT.h"

void delay(volatile uint32_t count) {
    while (count--);
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

    while (1) {


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
