#include "main.h"

int temperature_errors = 0;
volatile int sched_counter = 0;

float last_temp = 0;
int pwm_value = 0;
int encoder_value = 0;
int counter_fixer = 0;
int last_counter = 0;
int safe_mode = 0;
int safe_mode_try_disable = 0;
int not_changed = 0;
int freq_value = STROBOSCOPE_DEFAULT * STROBOSCOPE_RESOLUTION;
int mode = 0; // 0 - фонарь, 1 - стробоскоп

void main() {
	init_gpio();
	
	usart1_init(115200);
	
	init_printf(NULL, uart_putc);
	pwm_init(1, PWM_PERIOD, 0);
	
	init_encoder(ENCODER_MAX);
	timer_init();
	
	pwm_value = get_last_pwm();
	on_mode_changed();
	
	OW_Init();
	
	int temperature_state = 0, trigger = 0;
	while (1) {
		// Индикация 100% уровня яркости
		if (sched_counter % 10 == 0 && !safe_mode)
			GPIO_WriteBit(GPIOB, GPIO_Pin_8, pwm_value == ENCODER_MAX);
		
		// Проверка температуры
		if (sched_counter > 100) {
			if (not_changed > 1 && pwm_value != get_last_pwm()) {
				GPIO_WriteBit(GPIOB, GPIO_Pin_8, 1);
				FLASH_Unlock();
				FLASH_ErasePage(CONFIG_PWM_ADDR);
				FLASH_ProgramWord(CONFIG_PWM_ADDR, pwm_value);
				FLASH_Lock();
				GPIO_WriteBit(GPIOB, GPIO_Pin_8, 0);
				
				not_changed = 0;
				
				tfp_printf("PWM level saved => %d\r\n", pwm_value);
			}
			++not_changed;
			
			// При перегреве мигаем индикаторным светодиодом
			if (safe_mode) {
				GPIO_WriteBit(GPIOB, GPIO_Pin_8, trigger % 2 == 0);
				++trigger;
			}
			
			float temp = 9999;
			if (!temperature_state) { // Шлём запрос температуры
				if (OW_Send(OW_SEND_RESET, "\xcc\x44", 2, NULL, 0, OW_NO_READ) == OW_NO_DEVICE)
					++temperature_errors;
				else
					temperature_errors = 0;
				OW_EnableTxPin(0);
				temperature_state = 1;
			} else { // Получаем температуру
				uint8_t buf[9];
				
				OW_EnableTxPin(1);
				OW_Send(OW_SEND_RESET, "\xcc\xbe", 2, NULL, 0, OW_NO_READ);
				OW_Send(OW_NO_RESET, "\xff\xff\xff\xff\xff\xff\xff\xff\xff", 9, buf, 9, 0);
				
				if (crc8(buf, 8) == buf[8]) {
					temp = ((float) (buf[1] << 8 | buf[0]) * 0.625) / 10;
					if (temp != last_temp) {
						tfp_printf("%d.%d °C\r\n", (int) temp, (int) (temp * 10) - (int) temp * 10);
						last_temp = temp;
					}
					
					// Перегрев! Включаем безопасный режим
					if (temp > MAX_SAFE_TEMP && temp != 85) {
						if (!safe_mode) {
							safe_mode = 1;
							recalc_pwm_value();
							tfp_printf("safe mode enabled!\r\n");
						}
					}
					// Немного остыли, можно отключать безопасный режим
					else if (safe_mode) {
						++safe_mode_try_disable;
						
						// Включаем обратно только если температура зафиксировалась
						if (safe_mode_try_disable > 5) {
							safe_mode = 0;
							safe_mode_try_disable = 0;
							recalc_pwm_value();
							tfp_printf("safe mode disabled\r\n");
						}
					}
					
					temperature_errors = 0;
				} else {
					tfp_printf("[ds18b20] crc error %02X != %02X\r\n", crc8(buf, 8), buf[8]);
					++temperature_errors;
				}
				
				// Сломался термометр, включаем безопасный режим
				if (temperature_errors > 3 && !safe_mode) {
					safe_mode = 1;
					recalc_pwm_value();
					tfp_printf("[ds18b20] temp error %d.%d °C\r\n", (int) temp, (int) (temp * 10) - (int) temp * 10);
					tfp_printf("safe mode enabled!\r\n");
				}
				
				temperature_state = 0;
			}
			
			sched_counter = 0;
			
			if (mode) {
				double freq = round((double) SystemCoreClock / (double) TIM4->PSC / (double) TIM4->ARR * 10);
				int freq_n1 = freq / 10, 
					freq_n2 = freq - (freq_n1 * 10);
				tfp_printf("[stroboscope] freq=%d.%d Hz\r\n", freq_n1, freq_n2);
			}
		}
	}
}

int get_last_pwm() {
	volatile int pwm = *((volatile int *) CONFIG_PWM_ADDR);
	if (pwm > ENCODER_MAX)
		return ENCODER_MAX;
	if (pwm < 0)
		return 0;
	return pwm;
}

void on_mode_changed() {
	mode = !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
	
	if (!mode) {
		TIM4->PSC = 0;
		TIM4->CCR4 = 0;
		TIM4->ARR = PWM_PERIOD;
		TIM4->EGR = TIM_PSCReloadMode_Immediate;
	}
	
	recalc_pwm_value();
	
	tfp_printf("mode = %s\r\n", mode ? "Stroboscope" : "Lighter");
}

void recalc_pwm_value() {
	// Логарифмическая шкала яркости, генерируется через lg.php
	const static uint16_t linear_to_log[] = {
		0, 2, 4, 7, 9, 11, 13, 16, 18, 20, 
		23, 25, 27, 30, 32, 35, 37, 40, 43, 45, 
		48, 51, 53, 56, 59, 62, 64, 67, 70, 73, 
		76, 79, 83, 86, 89, 92, 95, 99, 102, 106, 
		109, 113, 116, 120, 124, 128, 132, 136, 140, 144, 
		148, 152, 157, 161, 166, 170, 175, 180, 185, 190, 
		195, 201, 206, 212, 218, 224, 230, 236, 242, 249, 
		256, 263, 270, 278, 286, 294, 303, 311, 321, 330, 
		340, 351, 362, 374, 386, 399, 413, 428, 444, 462, 
		480, 501, 524, 549, 578, 612, 651, 700, 762, 850, 
		1000
	};
	static int last_encoder = 0;
	
	if (mode) { // Стробоскоп, регулируем частоту
		freq_value += (encoder_value - last_encoder);
		if (freq_value < STROBOSCOPE_MIN * STROBOSCOPE_RESOLUTION)
			freq_value = STROBOSCOPE_MIN * STROBOSCOPE_RESOLUTION;
		if (freq_value > STROBOSCOPE_MAX * STROBOSCOPE_RESOLUTION)
			freq_value = STROBOSCOPE_MAX * STROBOSCOPE_RESOLUTION;
		
		for (int psc = 10; psc <= 100; psc += 10) {
			int arr = (int) round((double) (SystemCoreClock / psc) / ((double) freq_value / (double) STROBOSCOPE_RESOLUTION));
			if (arr <= 65534) {
				TIM4->ARR = arr;
				TIM4->CCR4 = SystemCoreClock / STROBOSCOPE_LIGHT / psc;
				
				if (psc != TIM4->PSC) {
					TIM4->PSC = psc;
					TIM4->EGR = TIM_PSCReloadMode_Immediate;
				}
				
				break;
			}
		}
	} else { // Фонарь, регулируем яркость
		pwm_value += (encoder_value - last_encoder);
		if (pwm_value < 0)
			pwm_value = 0;
		if (pwm_value > ENCODER_MAX)
			pwm_value = ENCODER_MAX;
		
		// Безопасный режим, в случае перегрева или выхода из строя термометра
		// Максимум 50% яркости
		if (safe_mode && pwm_value > ENCODER_MAX / 2)
			TIM4->CCR4 = linear_to_log[ENCODER_MAX / 2];
		else
			TIM4->CCR4 = linear_to_log[pwm_value];
		
		not_changed = 0;
	}
	
	last_encoder = encoder_value;
}

void EXTI0_IRQHandler() {
	int last_mode = mode;
	mode = !GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);
	EXTI_ClearITPendingBit(EXTI_Line0);
	if (last_mode != mode)
		on_mode_changed();
}

volatile void TIM2_IRQHandler() {
	++sched_counter;
	
	int cnt = TIM_GetCounter(TIM3);
	if (last_counter != cnt) {
		// Фиксим переполнение счётчика энкодера (вправо ENCODER_MAX->0 и влево 0->ENCODER_MAX)
		if (cnt - last_counter > ENCODER_OVERFLOW_MAX) {
			counter_fixer -= ENCODER_MAX;
		} else if (cnt - last_counter < -ENCODER_OVERFLOW_MAX) {
			counter_fixer += ENCODER_MAX;
		}
		last_counter = cnt;
		encoder_value = cnt + counter_fixer;
		recalc_pwm_value();
	}
	
	TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
}

uint8_t crc8(const uint8_t *data, uint8_t len) {
	const static uint8_t crc8_table[] = {
		0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
		157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
		35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
		190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
		70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
		219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
		101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
		248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
		140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
		17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
		175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
		50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
		202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
		87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
		233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
		116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
	};
	
	uint8_t crc = 0;
	while (len--)
		crc = crc8_table[(crc ^ *data++)];
	
	return crc;
}

void init_gpio() {
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	
	// 8 - светодиод для индикации статус
	// 9 - пин для PWM, изначально зануляем его в Push-Pull режиме, что бы при включении не было вспышки
	GPIO_InitTypeDef gpio;
	gpio.GPIO_Pin       = GPIO_Pin_8 | GPIO_Pin_9;
	gpio.GPIO_Speed     = GPIO_Speed_50MHz;
	gpio.GPIO_Mode      = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &gpio);
	
	GPIO_WriteBit(GPIOB, GPIO_Pin_9, 1);
	GPIO_WriteBit(GPIOB, GPIO_Pin_8, 0);
	
	// Кнопка режима стробоскоба
	gpio.GPIO_Pin       = GPIO_Pin_0;
	gpio.GPIO_Speed     = GPIO_Speed_50MHz;
	gpio.GPIO_Mode      = GPIO_Mode_IPU;
	GPIO_Init(GPIOA, &gpio);
	
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
	
	EXTI_InitTypeDef exti;
	EXTI_StructInit(&exti);
	exti.EXTI_Line		= EXTI_Line0;
	exti.EXTI_LineCmd	= ENABLE;
	exti.EXTI_Mode		= EXTI_Mode_Interrupt;
	exti.EXTI_Trigger	= EXTI_Trigger_Rising_Falling;
	EXTI_Init(&exti);
	
	NVIC_EnableIRQ(EXTI0_IRQn);
}

void init_encoder(int max) {
	RCC_APB1PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef gpio;
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOA, &gpio);
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	TIM_TimeBaseInitTypeDef timer;
	TIM_TimeBaseStructInit(&timer);
	timer.TIM_Prescaler = 0;
	timer.TIM_Period = max;
	timer.TIM_CounterMode = TIM_CounterMode_Down | TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &timer);
	
	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
	TIM_SetCounter(TIM3, 0);
	TIM_Cmd(TIM3, ENABLE);
}

void timer_init() {
	TIM_TimeBaseInitTypeDef tm;
	
	// TIM2
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	
	TIM_TimeBaseStructInit(&tm);
	tm.TIM_Prescaler = 720;
	tm.TIM_Period = 1000;
	TIM_TimeBaseInit(TIM2, &tm);
	
	NVIC_EnableIRQ(TIM2_IRQn);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	
	TIM_Cmd(TIM2, ENABLE);
}

void pwm_init(int prescaler, uint16_t period, uint16_t pulse) {
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef gpio;
	gpio.GPIO_Pin = GPIO_Pin_9;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);
	
	TIM_TimeBaseInitTypeDef time_base;
	TIM_TimeBaseStructInit(&time_base);
	time_base.TIM_Period = period - 1;
	time_base.TIM_Prescaler = prescaler - 1;
	time_base.TIM_ClockDivision = TIM_CKD_DIV1;
	time_base.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &time_base);
	
	TIM_OCInitTypeDef time_oc;
	TIM_OCStructInit(&time_oc);
	time_oc.TIM_OCMode = TIM_OCMode_PWM1;
	time_oc.TIM_OutputState = TIM_OutputState_Enable;
	time_oc.TIM_Pulse = pulse;
	time_oc.TIM_OCPolarity = TIM_OCPolarity_Low;
	time_oc.TIM_OCNPolarity = TIM_OCPolarity_Low;
	
	TIM_OC4Init(TIM4, &time_oc);
	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);
	
	TIM_Cmd(TIM4, ENABLE);
}

void usart1_init(uint32_t baudrate) {
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef uart;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	gpio.GPIO_Pin = GPIO_Pin_9;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio);
	
	gpio.GPIO_Pin = GPIO_Pin_10;
	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &gpio);
	
	uart.USART_BaudRate = baudrate;
	uart.USART_WordLength = USART_WordLength_8b;
	uart.USART_StopBits = USART_StopBits_1;
	uart.USART_Parity = USART_Parity_No;
	uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	uart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &uart);
	
	USART_Cmd(USART1, ENABLE);
}

void uart_putc(void *p, char c) {
	USART_SendData(USART1, c);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}
