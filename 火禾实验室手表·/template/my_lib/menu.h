#ifndef menu_h
#define menu_h

#include "stm32f10x.h"

// 初始化RTC和OLED：0 - 成功，-1 - OLED错误，-2 - RTC晶振错误
int Menu_Init(void);

// 在主循环中调用，更新OLED上的时间
void Menu_Update(void);

// 初始化成功后调用，设置时、分、秒（24小时制）
void Menu_SetTime(uint8_t Hour, uint8_t Minute, uint8_t Second);

#endif // !menu_h
