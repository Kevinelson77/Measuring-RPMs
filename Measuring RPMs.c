// Kevin Nelson
// Embedded Systems - 10k potentiometer ADC to DAC controlled motor w/ display, reading RPM's based on motor control

// Spring 2021

#include "stm32f4xx.h"
#include <stdio.h>

#define RS 0x200     							// PA9 mask for reg select
#define RW 0x400     							// PA10 mask for read/write
#define EN 0x800     							// PA11 mask for enable

void delayMs(int n);
void LCD_command(unsigned char command);
void LCD_data(char datawrite);
void LCD_init(void);
void Port_init(void);
void Send_A_String(char *StringOfCharacters);
void Voltage_Display(void);
void RPM_Display(void);
void Grab_Conversion_Data(void); 
void Pulse_Width_Modulator(float pulse_width);
static int i;
static char Voltage_Buffer[18], RPM_Buffer[18];
static float voltage, RPM, result, Period, Current, Frequency, Last = 0.0;

int main (void) {
		Port_init();
		LCD_init();

    while (1) {
				Grab_Conversion_Data();						// Starts ADC & DAC, then reads data holding register
				Voltage_Display();								// Displays voltage result to LCD from DAC
				LCD_command(0xC0);								// Moves LCD cursor to 2nd line
				Pulse_Width_Modulator(result); 		// Output PWM value to motor
				RPM_Display();										// Displays RPM result to LCD from TIM1
				//delayMs(250);											// Delay to maintain readability
				LCD_command(0x02);								// Clear LCD & move cursor back to line 1
		}
}
	
// Port Initializations for LCD. PA9-R/S, PA10-R/W, PA11-EN, PB0-PB7 for D0-D7, respectively.
void Port_init(void) {
		// Enable Clocks
		RCC->AHB1ENR = 3;	            	// enable GPIOA/B clocks
		RCC->APB1ENR |= 0x200000003;    // enable TIM2 clock & DAC
		RCC->APB2ENR |= 0x00000101;     // enable TIM 1 and ADC1 clock

		// Enable Modes
		GPIOA->MODER = 0;    						// clear pin mode
		GPIOA->MODER = 0x55566B5C;    	// set pins output mode, PA1 to analog(capture pot value), PA5(output PWM) to alternate function, PA8(input capture) alternate function
    GPIOA->BSRR  = 0x0C000000;      // turn off EN and R/W
    GPIOB->MODER = 0;    						// clear pin mode
    GPIOB->MODER = 0x55555555;    	// set pins output mode
		GPIOA->AFR[0] = 0;   						// clear alt mode
    GPIOA->AFR[0] = 0x02100000;    	// set pin to AF1 for TIM2 CH1, configure PA6 as input of TIM3 CH1
		GPIOA->AFR[1] = 0;   						// clear alt mode
    GPIOA->AFR[1] = 0x00000001;   	// set alt mode TIM1_CH1
	
		// Setup ADC1 
    ADC1->CR2 = 0;                  // SW trigger
    ADC1->SQR3 = 1;                 // conversion sequence starts at ch 1
    ADC1->SQR1 = 0;                 // conversion sequence length 1
    ADC1->CR2 |= 1;                 // enable ADC1
	
		// Setup TIM1 for input capture - PA8
		TIM1->PSC = 16000;          		// divided by 2999
    TIM1->CCMR1 = 0x01;           	// Input capture mode, validates transition on TI1 with 1 consecutive sample, input prescaler disabled(IC1PS bits to 0)
    TIM1->CCER = 1;                 // CC1S bits are writable only when the channel is OFF(CC1E = '0' in TIMx_CCER), rising edge selected by keeping CC1P & CC1NP bits to 0
    TIM1->CR1 = 1;                  // enable timer
		
		// Setup TIM2 for output PWM - PA5
    TIM2->PSC = 1000;          			// divided by 1600
    TIM2->ARR = 750;           		// divided by 6000
		TIM2->CNT = 0;                  // clear counter
		TIM2->CCMR1 = 0x0068;           // set output to PWM
   	TIM2->CCR1 = 0;               	// set match value
  	TIM2->CCER = 1;                	// enable ch 1 compare mode 
 		TIM2->CR1 = 1;                  // enable TIM2
}

// Initialize port pins then initialize LCD controller
void LCD_init(void) {
    delayMs(30);            // initialization sequence
    LCD_command(0x30);
    delayMs(10);
    LCD_command(0x30);
    delayMs(1);
    LCD_command(0x30);

    LCD_command(0x38);      // set 8-bit data, 2-line, 5x7 font
    LCD_command(0x06);      // move cursor right after each char
    LCD_command(0x01);      // clear screen, move cursor to home
    LCD_command(0x0F);      // turn on display, cursor blinking
}

// Send command to LCD
void LCD_command(unsigned char command) {
    GPIOA->BSRR = (RS | RW) << 16;  // RS = 0, R/W = 0
    GPIOB->ODR = command;           // put command on data bus
    GPIOA->BSRR = EN;               // pulse E high
    delayMs(0);
    GPIOA->BSRR = EN << 16;         // clear E
    if (command < 4)
        delayMs(2);         				// command 1 and 2 needs up to 1.64ms
    else
        delayMs(1);         				
}

// Write data to the LCD
void LCD_data(char datawrite) {
    GPIOA->BSRR = RS;               // RS = 1
    GPIOA->BSRR = RW << 16;         // R/W = 0
    GPIOB->ODR = datawrite;         // put data on data bus
    GPIOA->BSRR = EN;               // pulse E high
    delayMs(0);
    GPIOA->BSRR = EN << 16;         // clear E
    delayMs(1);
}

// Start & Grab ADC conversion Data
void Grab_Conversion_Data(void){
		ADC1->CR2 |= 0x40000000;        															// start a conversion
		while(!(ADC1->SR & 2)) {}       															// wait for conv complete
		result = ADC1->DR;              															// read conversion result	
}

// Display Voltage Function
void Voltage_Display(void){
		voltage = result *(3.3f/4095.0f);  														// convert ADC output to voltage
		sprintf(Voltage_Buffer, "Voltage: %.2f V", voltage);
		Send_A_String(Voltage_Buffer);																// Display Voltage
}

// Pulse Width Modulation
void Pulse_Width_Modulator(float pulse_width){
	TIM2->CCR1 = pulse_width;
}

// Display RPM Function
void RPM_Display(void){
		if (!(TIM1->SR & 2)) {}  								// wait until input edge is captured
		else if (TIM1->SR & 2){	
			Current = TIM1->CCR1;									// Read CCR1 input capture into Current
			Period = Current - Last;    					// calculate the period
			Last = Current;
			Frequency = 1000.0f / Period;
			RPM = Frequency * 60.0f; }
		
		// Display Data
		sprintf(RPM_Buffer, "RPM: %.1f", RPM);
		Send_A_String(RPM_Buffer);							// Display RPM
}

// Sending a String of Characters
void Send_A_String(char *StringOfCharacters){
	while(*StringOfCharacters){
		LCD_data(*StringOfCharacters++);}
}

// Delay timer
void delayMs(int n) {
    SysTick->LOAD = 16000;  // reload with number of clocks per millisecond
    SysTick->VAL = 0;       // clear current value register
    SysTick->CTRL = 0x5;    // Enable the timer

    for(i = 0; i < n; i++){
			while((SysTick->CTRL & 0x10000) == 0){} // wait until the COUNTFLAG is set{}
    }
    SysTick->CTRL = 0;      // Stop the timer (Enable = 0)
}
