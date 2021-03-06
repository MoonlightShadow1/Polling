/*
*********************************************************************************************************
*                                              EXAMPLE CODE
*
*                          (c) Copyright 2003-2013; Micrium, Inc.; Weston, FL
*
*               All rights reserved.  Protected by international copyright laws.
*               Knowledge of the source code may NOT be used to develop a similar product.
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                            EXAMPLE CODE
*
*                                     ST Microelectronics STM32
*                                              on the
*
*                                     Micrium uC-Eval-STM32F107
*                                        Evaluation Board
*
* Filename      : app.c
* Version       : V1.00
* Programmer(s) : EHS
*                 DC
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include <includes.h>
#include <string.h>


/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/
OS_MUTEX List;                         //声明互斥信号量
OS_SEM  SemOfPoll;
OS_MEM  Mem;                    //声明内存管理对象

uint8_t Array [ 3 ] [ 100 ];   //声明内存分区大小
uint8_t Rx_Cnt=0;
uint8_t Rx_Temp[20]={0};
Node *Head;

	uint8_t Device_Exist=0;
	uint8_t Find_Device=0;

/*
*********************************************************************************************************
*                                                 TCB
*********************************************************************************************************
*/

static  OS_TCB   AppTaskStartTCB;    //任务控制块

       OS_TCB AppTaskCheckDeviceTCB;
static OS_TCB AppTaskListTCB;
static OS_TCB AppTaskTCPSERVERTCB;



/*
*********************************************************************************************************
*                                                STACKS
*********************************************************************************************************
*/

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];       //任务堆栈

static  CPU_STK  AppTaskCheckDeviceStk[ APP_TASK_CHECK_DEVICE_SIZE ];
static  CPU_STK  AppTaskListStk[ APP_TASK_LIST_SIZE ];
static  CPU_STK  AppTaskTCPServerStk[ APP_TASK_TCP_SERVERT_SIZE ];



/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart  (void *p_arg);               //任务函数声明

static  void  AppTaskCheckDevice (void *p_arg);
static  void  AppTaskList(void *p_arg);
static  void  AppTaskTCPServer (void *p_arg);

/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*
* Arguments   : none
*
* Returns     : none
*********************************************************************************************************
*/

int  main (void)
{
    OS_ERR  err;

	
    OSInit(&err);                                                           //初始化 uC/OS-III

	  /* 创建起始任务 */
    OSTaskCreate((OS_TCB     *)&AppTaskStartTCB,                            //任务控制块地址
                 (CPU_CHAR   *)"App Task Start",                            //任务名称
                 (OS_TASK_PTR ) AppTaskStart,                               //任务函数
                 (void       *) 0,                                          //传递给任务函数（形参p_arg）的实参
                 (OS_PRIO     ) APP_TASK_START_PRIO,                        //任务的优先级
                 (CPU_STK    *)&AppTaskStartStk[0],                         //任务堆栈的基地址
                 (CPU_STK_SIZE) APP_TASK_START_STK_SIZE / 10,               //任务堆栈空间剩下1/10时限制其增长
                 (CPU_STK_SIZE) APP_TASK_START_STK_SIZE,                    //任务堆栈空间（单位：sizeof(CPU_STK)）
                 (OS_MSG_QTY  ) 5u,                                         //任务可接收的最大消息数
                 (OS_TICK     ) 0u,                                         //任务的时间片节拍数（0表默认值OSCfg_TickRate_Hz/10）
                 (void       *) 0,                                          //任务扩展（0表不扩展）
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), //任务选项
                 (OS_ERR     *)&err);                                       //返回错误类型

    OSStart(&err);                                                          //启动多任务管理（交由uC/OS-III控制）

}


/*
*********************************************************************************************************
*                                          STARTUP TASK
*
* Description : This is an example of a startup task.  As mentioned in the book's text, you MUST
*               initialize the ticker only once multitasking has started.
*
* Arguments   : p_arg   is the argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*
* Returns     : none
*
* Notes       : 1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                  used.  The compiler should not generate any code for this statement.
*********************************************************************************************************
*/

static  void  AppTaskStart (void *p_arg)
{
    CPU_INT32U  cpu_clk_freq;
    CPU_INT32U  cnts;
    OS_ERR      err;


    (void)p_arg;

    BSP_Init();                                                 //板级初始化
    CPU_Init();                                                 //初始化 CPU 组件（时间戳、关中断时间测量和主机名）

    cpu_clk_freq = BSP_CPU_ClkFreq();                           //获取 CPU 内核时钟频率（SysTick 工作时钟）
    cnts = cpu_clk_freq / (CPU_INT32U)OSCfg_TickRate_Hz;        //根据用户设定的时钟节拍频率计算 SysTick 定时器的计数值
    OS_CPU_SysTickInit(cnts);                                   //调用 SysTick 初始化函数，设置定时器计数值和启动定时器

    Mem_Init();                                                 //初始化内存管理组件（堆内存池和内存池表）

#if OS_CFG_STAT_TASK_EN > 0u                                    //如果使能（默认使能）了统计任务
    OSStatTaskCPUUsageInit(&err);                               //计算没有应用任务（只有空闲任务）运行时 CPU 的（最大）
#endif                                                          //容量（决定 OS_Stat_IdleCtrMax 的值，为后面计算 CPU 
                                                                //使用率使用）。
    CPU_IntDisMeasMaxCurReset();                                //复位（清零）当前最大关中断时间

    
    /* 配置时间片轮转调度 */		
    OSSchedRoundRobinCfg((CPU_BOOLEAN   )DEF_ENABLED,          //使能时间片轮转调度
		                     (OS_TICK       )0,                    //把 OSCfg_TickRate_Hz / 10 设为默认时间片值
												 (OS_ERR       *)&err );               //返回错误类型

		/* 创建互斥信号量 mutex */
    OSMutexCreate ((OS_MUTEX  *)&List,           //指向信号量变量的指针
                   (CPU_CHAR  *)"List", //信号量的名字
                   (OS_ERR    *)&err);            //错误类型
									 
									 
		/* 创建内存管理对象 mem */
		OSMemCreate ((OS_MEM      *)&Mem,             //指向内存管理对象
								 (CPU_CHAR    *)"Mem For Test",   //命名内存管理对象
								 (void        *)Array,          //内存分区的首地址
								 (OS_MEM_QTY   )3,               //内存分区中内存块数目
								 (OS_MEM_SIZE  )100,                //内存块的字节数目
								 (OS_ERR      *)&err);            //返回错误类型
	
  						 
     /*创建轮询查询Usart数据是否接收完毕任务*/		
    OSTaskCreate((OS_TCB     *)&AppTaskCheckDeviceTCB,                             //任务控制块地址
                 (CPU_CHAR   *)"App_Task_Check_Device",                             //任务名称
                 (OS_TASK_PTR ) AppTaskCheckDevice,                                //任务函数
                 (void       *) 0,                                          //传递给任务函数（形参p_arg）的实参
                 (OS_PRIO     ) APP_TASK_CHECK_DEVICE_PRIO,                         //任务的优先级
                 (CPU_STK    *)&AppTaskCheckDeviceStk[0],                          //任务堆栈的基地址
                 (CPU_STK_SIZE) APP_TASK_CHECK_DEVICE_SIZE / 10,                //任务堆栈空间剩下1/10时限制其增长
                 (CPU_STK_SIZE) APP_TASK_CHECK_DEVICE_SIZE,                     //任务堆栈空间（单位：sizeof(CPU_STK)）
                 (OS_MSG_QTY  ) 50u,                                        //任务可接收的最大消息数
                 (OS_TICK     ) 0u,                                         //任务的时间片节拍数（0表默认值OSCfg_TickRate_Hz/10）
                 (void       *) 0,                                          //任务扩展（0表不扩展）
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), //任务选项
                 (OS_ERR     *)&err);                                       //返回错误类型


		/* 创建 AppTaskPend 任务 */
    OSTaskCreate((OS_TCB     *)&AppTaskListTCB,                             //任务控制块地址
                 (CPU_CHAR   *)"App Task Pend",                             //任务名称
                 (OS_TASK_PTR ) AppTaskList,                                //任务函数
                 (void       *) 0,                                          //传递给任务函数（形参p_arg）的实参
                 (OS_PRIO     ) APP_TASK_LIST_PRIO,                         //任务的优先级
                 (CPU_STK    *)&AppTaskListStk[0],                          //任务堆栈的基地址
                 (CPU_STK_SIZE) APP_TASK_LIST_SIZE / 10,                //任务堆栈空间剩下1/10时限制其增长
                 (CPU_STK_SIZE) APP_TASK_LIST_SIZE,                     //任务堆栈空间（单位：sizeof(CPU_STK)）
                 (OS_MSG_QTY  ) 50u,                                        //任务可接收的最大消息数
                 (OS_TICK     ) 0u,                                         //任务的时间片节拍数（0表默认值OSCfg_TickRate_Hz/10）
                 (void       *) 0,                                          //任务扩展（0表不扩展）
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), //任务选项
                 (OS_ERR     *)&err);                                       //返回错误类型

     /*创建轮询查询Usart数据是否接收完毕任务*/		
    OSTaskCreate((OS_TCB     *)&AppTaskTCPSERVERTCB,                             //任务控制块地址
                 (CPU_CHAR   *)"App_Task_TCP_Server",                             //任务名称
                 (OS_TASK_PTR ) AppTaskTCPServer,                                //任务函数
                 (void       *) 0,                                          //传递给任务函数（形参p_arg）的实参
                 (OS_PRIO     ) APP_TASK_TCP_SERVER_PRIO,                         //任务的优先级
                 (CPU_STK    *)&AppTaskTCPServerStk[0],                          //任务堆栈的基地址
                 (CPU_STK_SIZE) APP_TASK_TCP_SERVERT_SIZE / 10,                //任务堆栈空间剩下1/10时限制其增长
                 (CPU_STK_SIZE) APP_TASK_TCP_SERVERT_SIZE,                     //任务堆栈空间（单位：sizeof(CPU_STK)）
                 (OS_MSG_QTY  ) 50u,                                        //任务可接收的最大消息数
                 (OS_TICK     ) 0u,                                         //任务的时间片节拍数（0表默认值OSCfg_TickRate_Hz/10）
                 (void       *) 0,                                          //任务扩展（0表不扩展）
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), //任务选项
                 (OS_ERR     *)&err);     


								 
														 
		OSTaskDel ( 0, & err );                     //删除起始任务本身，该任务不再运行
		
		
}

/*
*********************************************************************************************************
*                                 USART CHECK TASK
*********************************************************************************************************
*/
static  void  AppTaskCheckDevice( void * p_arg )
{
	OS_ERR      err;
  OS_MSG_SIZE Msg_size;
	CPU_SR_ALLOC();
	
	uint8_t i=0;
	uint8_t Addr=0x01;
	uint32_t Check_Temp[20]={0};
	uint32_t *Msg=0;
//	uint8_t Device_Exist=0;
//	uint8_t Find_Device=0;
	
	(void)p_arg;

					 
	while (DEF_TRUE) 
		{      
			if(Addr>0x3f)
			{
			 Addr=0;
			}
			OSMutexPend ((OS_MUTEX  *)&List,                  //申请互斥信号量 mutex
									 (OS_TICK    )0,                       //无期限等待
									 (OS_OPT     )OS_OPT_PEND_BLOCKING,    //如果申请不到就堵塞任务
									 (CPU_TS    *)0,                       //不想获得时间戳
									 (OS_ERR    *)&err);                   //返回错误类型	

      Device_Exist=Find_Node(Addr);
			
		  OSMutexPost ((OS_MUTEX  *)&List,                 //释放互斥信号量 mutex
								   (OS_OPT     )OS_OPT_POST_NONE,       //进行任务调度
								   (OS_ERR    *)&err);   	
			
			Check_Temp[0]=0xad;
			Check_Temp[1]=0xda;
			Check_Temp[2]=0x02;
			Check_Temp[3]=Addr;
			Check_Temp[4]=0x20;
			Check_Temp[5]=0x03;
		 	
      USART1_Send_Data(Check_Temp,6);
			
//			for(i=0;i<6;i++)
//			{
//			 TCP_Send_Buffer[i]=(uint8_t)Check_Temp[i];			
//			}			
////			memcpy(TCP_Send_Buffer,(uint8_t *)Check_Temp,6);
//			TCP_Send_Cnt=6;
//      TCP_Send_Flag = 1;
			

			
		/* 阻塞任务，等待任务消息 */
		 Msg = OSTaskQPend ((OS_TICK        )5000,                    //无期限等待
											  (OS_OPT         )OS_OPT_PEND_BLOCKING, //没有消息就阻塞任务
											  (OS_MSG_SIZE   *)&Msg_size,            //返回消息长度
											  (CPU_TS        *)0,                    //返回消息被发布的时间戳
											  (OS_ERR        *)&err);                //返回错误类型
			
		OS_CRITICAL_ENTER();                                       //进入临界段，避免串口打印被打断
		
		Find_Device=(*(Msg+3)==Addr?1:0);
		
		
    if(Device_Exist)
		{ 
			if(Find_Device)        //设备存在于链表并且查找有回复
			{
			//对比设备的状态信息，看看是否有新IO添加、新状态更新
				
			  OSMutexPend ((OS_MUTEX  *)&List,                  //申请互斥信号量 mutex
									   (OS_TICK    )0,                       //无期限等待
									   (OS_OPT     )OS_OPT_PEND_BLOCKING,    //如果申请不到就堵塞任务
									   (CPU_TS    *)0,                       //不想获得时间戳
									   (OS_ERR    *)&err);                   //返回错误类型		
				
        Updata_Node(Msg);

		    OSMutexPost ((OS_MUTEX  *)&List,                 //释放互斥信号量 mutex
								     (OS_OPT     )OS_OPT_POST_NONE,       //进行任务调度
								     (OS_ERR    *)&err); 				
			
			}
			else                 //设备存在于链表，但是查找没有回复
			{
			//对设备进行offline操作，删除节点
				
        OSMutexPend ((OS_MUTEX  *)&List,                  //申请互斥信号量 mutex
									   (OS_TICK    )0,                       //无期限等待
									   (OS_OPT     )OS_OPT_PEND_BLOCKING,    //如果申请不到就堵塞任务
									   (CPU_TS    *)0,                       //不想获得时间戳
									   (OS_ERR    *)&err);                   //返回错误类型		

		    Delete_Node(Addr);		

		    OSMutexPost ((OS_MUTEX  *)&List,                 //释放互斥信号量 mutex
								     (OS_OPT     )OS_OPT_POST_NONE,       //进行任务调度
								     (OS_ERR    *)&err); 	
			}
		  
		}
		else 
		{
			if(Find_Device)    //设备不存在于链表，但是查找有回复
			{
			//这个是新的设备加入，需要插入节点，更新状态
			  OSMutexPend ((OS_MUTEX  *)&List,                  //申请互斥信号量 mutex
									   (OS_TICK    )0,                       //无期限等待
									   (OS_OPT     )OS_OPT_PEND_BLOCKING,    //如果申请不到就堵塞任务
									   (CPU_TS    *)0,                       //不想获得时间戳
									   (OS_ERR    *)&err);                   //返回错误类型		

        Insert_Node(Msg); 
				
		    OSMutexPost ((OS_MUTEX  *)&List,                 //释放互斥信号量 mutex
								     (OS_OPT     )OS_OPT_POST_NONE,       //进行任务调度
								     (OS_ERR    *)&err); 				
				
				
			}
			else               //设备不存在于链表，查找也没有回复   
			{
			//没有这个设备，继续查找下一个设备
			
			}	
		}


		/* 退还内存块 */
		OSMemPut ((OS_MEM  *)&Mem,                                 //指向内存管理对象
							(void    *)Msg,                                 //内存块的首地址
							(OS_ERR  *)&err);		                             //返回错误类型				

		macLED2_TOGGLE();	
		OS_CRITICAL_EXIT();                                        //退出临界段							
	  }
}


static  void  AppTaskList(void *p_arg)
{
	OS_ERR      err;
  uint32_t *List_Msg=0;
	
  OS_MSG_SIZE List_Msg_Size;
	
	CPU_SR_ALLOC();
	
	(void)p_arg;

					 
	while (DEF_TRUE)
	{
/* 阻塞任务，等待任务消息 */
		 List_Msg = OSTaskQPend ((OS_TICK        )0,                    //无期限等待
											       (OS_OPT         )OS_OPT_PEND_BLOCKING, //没有消息就阻塞任务
											       (OS_MSG_SIZE   *)&List_Msg_Size,            //返回消息长度
											       (CPU_TS        *)0,                    //返回消息被发布的时间戳
											       (OS_ERR        *)&err);                //返回错误�

		OS_CRITICAL_ENTER();      		
	  OSMutexPend ((OS_MUTEX  *)&List,                  //申请互斥信号量 mutex
								 (OS_TICK    )0,                       //无期限等待
								 (OS_OPT     )OS_OPT_PEND_BLOCKING,    //如果申请不到就堵塞任务
								 (CPU_TS    *)0,                       //不想获得时间戳
								 (OS_ERR    *)&err);                   //返回错误类型		
	
	  USART1_Send_Data(List_Msg,(u16)List_Msg_Size);
		macLED1_TOGGLE();	
		OSMutexPost ((OS_MUTEX  *)&List,                 //释放互斥信号量 mutex
								 (OS_OPT     )OS_OPT_POST_NONE,       //进行任务调度
								 (OS_ERR    *)&err);                  //返回错误类型	

		
		
		
		OSMemPut ((OS_MEM  *)&Mem,                                 //指向内存管理对象
							(void    *)List_Msg,                                 //内存块的首地址
							(OS_ERR  *)&err);		                             //返回错误类型	
		
    OS_CRITICAL_EXIT();                                        //退出临界段							
		
	}
}

static  void  AppTaskTCPServer (void *p_arg)
{
	
	OS_ERR      err;	
	(void)p_arg;
	
	
	W5500_GPIO_Init();
	W5500_Hardware_Reset();  //硬件复位W5500		
	W5500_Parameters_Init();		//W5500初始配置

	printf(" 野火网络适配版作为TCP 服务器，建立侦听，等待PC作为TCP Client建立连接 \r\n");
	printf(" W5500监听端口为： %d \r\n",local_port);
	printf(" 连接成功后，TCP Client发送数据给W5500，W5500将返回对应数据 \r\n");	
		
	
	while (DEF_TRUE)
	{
		do_tcp_server();
		OSTimeDlyHMSM ( 0, 0, 0,50, OS_OPT_TIME_DLY, &err);	
	}


}	










