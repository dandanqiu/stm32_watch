#include "stm32f10x.h"
#include "menu.h"
#include "delay.h"

int main(void)
{
	volatile int init_result;

	// #1. 初始化RTC和OLED
	init_result = Menu_Init();
	if(init_result != 0)
	{
		// 调试时查看init_result：-1为OLED错误，-2为RTC晶振错误
		while(1)
		{
		}
	}

	// 手动校时示例：Menu_SetTime(14, 30, 0);
	// 校准后移除这句，避免每次复位都重新设时

	// #2. 循环检查时间，秒数变化时刷新OLED
	while(1)
	{
		Menu_Update();
		Delay(20);
	}
}
