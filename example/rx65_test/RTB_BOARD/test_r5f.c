/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */
#include <k_api.h>
#include <stdio.h>
#include <stdlib.h>
#include "uart.h"
#include "iodefine.h"
#include <aos/aos.h>
#include "aos\kernel.h"
#include "rx_platform.h"           // Located in the FIT BSP module
#include "r_sci_rx_if.h"        // The SCI module API interface file.
#include "r_byteq_if.h"         // The BYTEQ module API interface file.
#include "r_sci_rx_config.h"    // User configurable options for the SCI module
#include <hal/wifi.h>

#include "r_flash_rx_if.h"
#include "r_flash_rx_config.h"
#include "flash.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
//#include "alink_export.h"
#include "config.h"
#include "json_parser.h"
#include "aos/network.h"
#include "kvmgr.h"
#include <netmgr.h>
//#include <accs.h>

#include "iot_export.h"

#ifdef AOS_ATCMD
#include <atparser.h>
#endif
#ifdef AOS_AT_ADAPTER
#include <at_adapter.h>
#undef CONFIG_YWSS
#endif

#ifdef CONFIG_YWSS
#include <enrollee.h>
#endif

extern int application_start(int argc, char **argv);


void my_sci_callback(void *pArgs);

/* 200 * cpu_stack_t = 400byte */
#define APP_TASK_STACKSIZE	40192
#define DEMO_TASK_STACKSIZE     200
#define DEMO_TASK_PRIORITY1     20
#define DEMO_TASK_PRIORITY2     21

void    BSP_Init (void);
void    BSP_Pre_Init(void);

aos_task_t g_init_task;
static kinit_t kinit;

static ktask_t      demo_task_obj1;
static ktask_t      demo_task_obj2;

static cpu_stack_t  demo_task_buf1[DEMO_TASK_STACKSIZE];
static cpu_stack_t  demo_task_buf2[DEMO_TASK_STACKSIZE];

static uint32_t     tst_cnt;
static uint32_t     tmr_cnt;

 ksem_t       sem_tst;
static ktimer_t     timer_test;
static kbuf_queue_t queue_test;

static char         queue_buf[50];
static char         queue_buf_recv[100];
static size_t       queue_recv_size;

int LightSwitch =0;
double Temperature =24.5;
double Humidity =85;
long illumination = 180;


extern hal_wifi_module_t aos_wifi_module_mk3060;
extern struct hal_ota_module_s rx65n_ota_module;
extern at_parser_t at;
static void timer_cb(void *timer, void *arg)
{
    char queue_buf_recv[10];
    memset(queue_buf_recv, 'a', 10);
    tmr_cnt++;
    krhino_buf_queue_send(&queue_test, queue_buf_recv, 10);
    krhino_timer_stop(&timer_test);
}

void demo_task1(void *arg)
{
	BSP_Init();
    krhino_sem_create(&sem_tst, "sem_tst", 0);

    krhino_buf_queue_create(&queue_test, "queue_test",
                             queue_buf, 500, 500);

    krhino_timer_create(&timer_test, "timer_test", timer_cb,
                            10, 10, 0, 0);

    krhino_timer_start(&timer_test);

    krhino_buf_queue_recv(&queue_test, RHINO_WAIT_FOREVER, queue_buf_recv,
                          &queue_recv_size);

    while (1)
    {
        krhino_sem_give(&sem_tst);
        krhino_task_sleep(RHINO_CONFIG_TICKS_PER_SECOND);
    }
}

void demo_task2(void *arg)
{
	PORTD.PDR.BIT.B7 = 1;  	//LED1 as portD.7
	PORTD.PMR.BIT.B7 = 0;  //portD.7 AS IO
	PORTB.PDR.BIT.B1 = 1;  	//USER LED2 as portB.1
	PORTB.PMR.BIT.B1 = 0;  //portB.1 AS IO
    while (1)
    {
    	if (LightSwitch==0)
    		PORTB.PODR.BIT.B1 = 1; //user LED2 OFF
    	else
    		PORTB.PODR.BIT.B1 = 0; //user LED2 ON
    	PORTD.PODR.BIT.B7 = ~ PORTD.PODR.BIT.B7;
        tst_cnt++;
        krhino_sem_take(&sem_tst, RHINO_WAIT_FOREVER);
    }
}


uart_dev_t console;
uart_dev_t uart_0;
uint8_t testdata[] = {"01234567\n"};
uint32_t   my_sci_err;
sci_hdl_t   g_my_sci_handle;
//#pragma weak fputchar

void init_atparser()
{
	/* ENVISION BOARD
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_MPC); // Use BSP function to unlock the MPC register.
	MPC.PB7PFS.BYTE = 0x0A;
	MPC.PB6PFS.BYTE = 0x0A;
	R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_MPC); // Re-lock the MPC register.
	PORTB.PDR.BIT.B7 = 1;  	//PB7 as TxD9
	PORTB.PODR.BIT.B7 = 1;  //PB7 as TxD9
	PORTB.PDR.BIT.B6 = 0;  	//PB6 as RxD9
	PORTB.PMR.BIT.B6 = 1;
	PORTB.PMR.BIT.B7 = 1;
	 */

	// RTB BOARD
    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_MPC); // Use BSP function to unlock the MPC register.
	MPC.P26PFS.BYTE = 0x0A;
	MPC.P30PFS.BYTE = 0x0A;
	R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_MPC); // Re-lock the MPC register.
	PORT2.PDR.BIT.B6 = 1;  	//P26 as TxD1
	PORT2.PODR.BIT.B6 = 1;  //P26 as TxD1
	PORT3.PDR.BIT.B0 = 0;  	//PB30 as RxD1
	PORT3.PMR.BIT.B0 = 1;
	PORT2.PMR.BIT.B6 = 1;

    hal_wifi_register_module(&aos_wifi_module_mk3060);
    hal_ota_register_module(&rx65n_ota_module);

	}

void init_task(void *arg)
{

    console.port = 5;
    console.config.baud_rate = 115200;
    console.config.data_width = DATA_WIDTH_8BIT;
    console.config.parity = NO_PARITY;
    console.config.stop_bits = STOP_BITS_1;
    console.config.mode = MODE_TX_RX;

    R_BSP_RegisterProtectDisable(BSP_REG_PROTECT_MPC); // Use BSP function to unlock the MPC register.
    MPC.PA3PFS.BYTE = 0x0A;
    MPC.PA4PFS.BYTE = 0x0A;
    R_BSP_RegisterProtectEnable(BSP_REG_PROTECT_MPC); // Re-lock the MPC register.
    PORTA.PDR.BIT.B4 = 1;		//PA4 as TxD5
    PORTA.PODR.BIT.B4 = 1;
    PORTA.PDR.BIT.B3 = 0;		//PA3 as RxD5
    PORTA.PMR.BIT.B4 = 1;
    PORTA.PMR.BIT.B3 = 1;

    PORTE.PDR.BIT.B0 = 1;		//PE0 as TEST PORT
    PORTE.PODR.BIT.B0 = 1;		//PE0 as TEST PORT

    my_sci_err = hal_uart_init(&console);
    uart_0 = console;

    init_atparser();

    kinit.argc = 0;
	kinit.argv = NULL;
	kinit.cli_enable = 1;
	aos_kernel_init(&kinit);
}




int main(void)
{
    flash_err_t err;
    flash_access_window_config_t  accessInfo;

	unsigned char log_example;

	log_example = 3;

	BSP_Pre_Init();
    krhino_init();

	R_FlashCodeCopy();
    /* Open driver */
    err = R_FLASH_Open();
    if (err != FLASH_SUCCESS)
        while(1);

	BSP_Init();



    aos_task_new_ext(&g_init_task, "init", init_task, 0, APP_TASK_STACKSIZE, DEMO_TASK_PRIORITY1);

    krhino_task_create(&demo_task_obj1, "demo_task", 0,DEMO_TASK_PRIORITY1,
        50, demo_task_buf1, DEMO_TASK_STACKSIZE, demo_task1, 1);

   krhino_task_create(&demo_task_obj2, "demo_task2", 0,DEMO_TASK_PRIORITY2,
       50, demo_task_buf2, DEMO_TASK_STACKSIZE, demo_task2, 1);

    IOT_OpenLog(&log_example);
    IOT_SetLogLevel(5);
    krhino_start();
    
    return 0;
}

void my_sci_callback(void *pArgs)
{

}

void localtime_r()
{}



