#include "menu.h"
#include "oled.h"
#include "si2c.h"
#include "delay.h"

static SI2C_TypeDef si2c;
static OLED_TypeDef oled;
static uint32_t last_second = 0xffffffff; // 上一次成功显示的秒数

// @简介：通过软件I2C给OLED发送数据
static int OLED_I2C_Write(uint8_t Addr, const uint8_t *pData, uint16_t Size)
{
	return My_SI2C_SendBytes(&si2c, Addr, pData, Size);
}

// @简介：设置RTC时间，使用24小时制
// @注意：需要先完成RTC初始化
void Menu_SetTime(uint8_t Hour, uint8_t Minute, uint8_t Second)
{
	uint32_t seconds;

	// 把时、分、秒换算成总秒数
	seconds = (uint32_t)Hour * 3600 + (uint32_t)Minute * 60 + Second;

	RTC_WaitForLastTask();
	RTC_SetCounter(seconds);
	RTC_WaitForLastTask();

	last_second = 0xffffffff; // 校时后重新显示
}

// @简介：初始化RTC，使用外部32.768kHz晶振
// @返回值：0 - 成功，-2 - 晶振未起振
static int Clock_Init(void)
{
	const char *build_time = __TIME__;
	uint32_t start_time;
	uint8_t first_init;
	uint8_t hour, minute, second;

	// #1. 使能电源、备份区域时钟，允许修改RTC和备份寄存器
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);

	// #2. 检查初始化标记、RTC时钟源和使能状态
	// 0x5415是自己选的标记，用来记录RTC已经初始化过
	first_init = 0;
	if(BKP_ReadBackupRegister(BKP_DR1) != 0x5415
		|| (RCC->BDCR & RCC_BDCR_RTCSEL) != RCC_RTCCLKSource_LSE
		|| (RCC->BDCR & RCC_BDCR_RTCEN) == 0)
	{
		first_init = 1;
		BKP_DeInit();
	}

	// #3. 启动外部低速晶振，最多等待5秒
	RCC_LSEConfig(RCC_LSE_ON);
	start_time = GetTick();
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
	{
		if((uint32_t)(GetTick() - start_time) >= 5000)
		{
			return -2;
		}
	}

	// #4. 选择RTC时钟源，开启RTC并等待寄存器同步
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
	RCC_RTCCLKCmd(ENABLE);
	RTC_WaitForSynchro();
	RTC_WaitForLastTask();

	// #5. 首次运行时设置分频和时间，复位时保留已有时间
	if(first_init == 1)
	{
		RTC_SetPrescaler(32768 - 1); // 分频后，RTC每秒加1
		RTC_WaitForLastTask();

		// __TIME__的格式是"时:分:秒"，例如"14:30:05"
		// 字符减去'0'得到数字，十位乘10后再加个位
		hour = (build_time[0] - '0') * 10 + (build_time[1] - '0');
		minute = (build_time[3] - '0') * 10 + (build_time[4] - '0');
		second = (build_time[6] - '0') * 10 + (build_time[7] - '0');

		Menu_SetTime(hour, minute, second); // 下载耗时会带来一定偏差
		BKP_WriteBackupRegister(BKP_DR1, 0x5415);
	}

	return 0;
}

// @简介：初始化RTC和OLED
// @返回值：0 - 成功，-1 - OLED初始化失败，-2 - RTC晶振未起振
int Menu_Init(void)
{
	OLED_InitTypeDef OLED_InitStruct;
	int result;

	// #1. 初始化RTC
	result = Clock_Init();
	if(result != 0)
	{
		return result;
	}

	// #2. 配置软件I2C引脚：PB8接SCL，PB9接SDA
	si2c.SCL_GPIOx = GPIOB;
	si2c.SCL_GPIO_Pin = GPIO_Pin_8;
	si2c.SDA_GPIOx = GPIOB;
	si2c.SDA_GPIO_Pin = GPIO_Pin_9;

	My_SI2C_Init(&si2c);
	Delay(100); // 等待OLED上电稳定

	// #3. 把I2C发送函数交给OLED驱动，再初始化屏幕
	OLED_InitStruct.i2c_write_cb = OLED_I2C_Write;
	result = OLED_Init(&oled, &OLED_InitStruct);
	if(result != 0)
	{
		return -1;
	}

	last_second = 0xffffffff;
	return 0;
}

// @简介：读取RTC时间并显示到OLED
// @注意：在main函数的while循环中调用，秒数变化时才刷新
void Menu_Update(void)
{
	uint32_t seconds;
	uint8_t hour, minute, second;

	// #1. 读取RTC计数，同一秒内不用重复刷新
	seconds = RTC_GetCounter();
	if(seconds == last_second)
	{
		return;
	}

	// #2. 把总秒数转换成时、分、秒
	hour = seconds / 3600 % 24;
	minute = seconds / 60 % 60;
	second = seconds % 60;

	// #3. 清空画面，设置文字位置，写入时间
	OLED_Clear(&oled);
	OLED_SetCursor(&oled, 0, 8); // 默认字体的文字基线在y=8
	OLED_Printf(&oled, "%02d:%02d:%02d", hour, minute, second);

	// #4. 把画面发送到屏幕，发送成功后记录本次时间
	if(OLED_SendBuffer(&oled) == 0)
	{
		last_second = seconds;
	}
}
