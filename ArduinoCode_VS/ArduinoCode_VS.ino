/***************************************************
 Name:	通电玻璃门交互	ArduinoCode_VS.ino
 Created:	2019/1/17/周四 17:20:40
 Author:	liuguo
****************************************************/
/***************************************************
CurrentVersion:V1.0   Time:	2019/1/17/周四 17:20:40
Content:http,udp
    
****************************************************/

/*NOTE:如何防止WDT复位
The ESP8266 is a little different than the standard Arduino boards in that it has the watchdog(WDT) turned on by default.
If the watchdog timer isn't periodically reset then it will automatically reset your ESP8266.
The watchdog is reset every time loop() runs or you call delay() or yield()
but if you have blocking code like the above then the watchdog may time out,
resulting in your reset. So to fix this you can change the above code to:
Code: [Select]
while(digitalRead(lockedSensor) != LOW) {
stepper.turnClockwise(2);
yield();
}
You should do the same to any other blocking code(that doesn't call delay()) you might use.
This is the mos simple solution but in general it's not a good idea to write blocking code
so you might consider reworking your code so that loop() continues to run unimpeded.
*/

#include "Myheader.h"
#define DEBUG_SERIAL Serial

/************IO端口设置***********/
const int CLOSE_GLASS_OUTPUT_PORT = 14; //D5  关闭通电玻璃开关控制输出端口
const int CLOSE_GLASS_INPUT_PORT = 4; //D2  关闭通电玻璃信号输入端口
const int D00R_HANDLE_INPUT_PORT = 5;//D1 门把手电容开关输入端口
/********************************/

/*****防止传感器输入信号误触发********/
long CLOSE_GLASS_INPUT_PORT_COUNT = 0;
long D00R_HANDLE_INPUT_PORT_COUNT = 0;
int MAX_LOW_COUNT = 2;
/*************************************/


/***************模拟多线程参数*********************/
#define PT_USE_SEM
#define PT_USE_TIMER
#include "pt.h"
static struct pt  checkSensorThread, receiveNTPThread;
static struct pt_sem sem_LED;
/*************************************************/

/***************其他设置*********************/
boolean sensor_close;
boolean sensor_handle;

const int remotePort = 17942;
bool isInitialized = false; //初始化
/*******************************************/




void setup() {

	DEBUG_SERIAL.begin(115200);
	DEBUG_SERIAL.println("Start all initilization.....");

	InitializeWeb();//初始化网络通信参数
	InitializeIO();
	PT_INIT(&checkSensorThread);//初始化模拟多线程
	PT_INIT(&receiveNTPThread);//初始化模拟多线程
	isInitialized = true;
}

void InitializeIO()
{
	pinMode(CLOSE_GLASS_INPUT_PORT, INPUT_PULLUP);
	pinMode(D00R_HANDLE_INPUT_PORT, INPUT);
}

static int receiveNTPThread_entry(struct pt *pt)
{
	String receivedStr;
	PT_BEGIN(pt);
	while (1) {
		receivedStr = ReadUDPMsg();
		if (receivedStr != "")
			DEBUG_SERIAL.println("Received UDP resopnse msg:" + receivedStr);

		PT_TIMER_DELAY(pt, 50);//间隔50毫秒执行一次
		PT_YIELD(pt);//跳到下一帧继续执行,让出cpu给其他任务
	}
	PT_END(pt);
}


static int checkSensorThread_entry(struct pt *pt)
{
	PT_BEGIN(pt);
	while (1) {

		CheckSensorState();

		PT_TIMER_DELAY(pt, 50);//间隔50毫秒执行一次
		PT_YIELD(pt);//跳到下一帧继续执行,让出cpu给其他任务
	}
	PT_END(pt);
}

void CheckSensorState()
{
	String msg = "";
	sensor_close = digitalRead(CLOSE_GLASS_INPUT_PORT);
	sensor_handle = digitalRead(D00R_HANDLE_INPUT_PORT);
	if (sensor_handle == true)
		D00R_HANDLE_INPUT_PORT_COUNT++;
	else
		D00R_HANDLE_INPUT_PORT_COUNT = 0;

	if (D00R_HANDLE_INPUT_PORT_COUNT > MAX_LOW_COUNT)
		sensor_handle = true;
	else
		sensor_handle = false;

	if (sensor_close == false)
		CLOSE_GLASS_INPUT_PORT_COUNT++;
	else
		CLOSE_GLASS_INPUT_PORT_COUNT = 0;

	if (CLOSE_GLASS_INPUT_PORT_COUNT > MAX_LOW_COUNT)
		sensor_close = false;
	else
		sensor_close = true;

	String  close = sensor_close ? "1" : "0";
	String  handle = sensor_handle ? "1" : "0";
	msg = "close:" + close + "," + "door:" + handle;
	SendAdvUdpMsg(msg, remotePort);
	DEBUG_SERIAL.println(msg);
}


void InitializeWeb() {
	DEBUG_SERIAL.println();
	StartAP("xr-lab","66666666","192.168.1.1");
	BeginUDP(8000);
}


void loop() {
	checkSensorThread_entry(&checkSensorThread);
	receiveNTPThread_entry(&receiveNTPThread);
}
