/*
 Name:		ArduinoCode_VS.ino
 Created:	2019/1/17/周四 17:20:40
 Author:	Administrator
*/

/*************************************************
项目名；通电玻璃门交互
created by lg 2019 / 01 / 17
*************************************************/
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
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#define DEBUG_SERIAL Serial

/****IO端口设置********/

const int CLOSE_GLASS_OUTPUT_PORT = 14; //D5  关闭通电玻璃开关控制输出端口
const int CLOSE_GLASS_INPUT_PORT = 4; //D2  关闭通电玻璃信号输入端口
const int D00R_HANDLE_INPUT_PORT = 5;//D1 门把手电容开关输入端口

const int LIGHT_OUTPUT_PORT = 16; //D0 信号灯输出端口
								  /****************************************/

								  /*****防止传感器输入信号误触发********/
long CLOSE_GLASS_INPUT_PORT_COUNT = 0;
long D00R_HANDLE_INPUT_PORT_COUNT = 0;
int MAX_LOW_COUNT = 2;
/******************************/

/*************网络相关设置************/
char ssid[] = "liuguo_home";
char pass[] = "7777788888";
WiFiUDP udp;

IPAddress localip(192, 168, 1, 101); //静态ip
IPAddress gateway(192, 168, 1, 1); //网关
IPAddress subnet(255, 255, 255, 0); //子网掩码

IPAddress advIP(255, 255, 255, 255); //广播ip地址
IPAddress hostIP(192, 168, 1, 102); //主机ip地址
unsigned int localPort = 8000;      //本地监听端口
unsigned int remotePort = 8001;     //主机监听端口

/***************************************/


/***************其他设置*********************/

boolean sensor_close;
boolean sensor_handle;

bool isInitialized = false; //初始化

#define PT_USE_SEM
#define PT_USE_TIMER
#include "pt.h"

String infoStr;
static struct pt sendNTPThread, receiveNTPThread, checkSensorThread;
static struct pt_sem sem_LED;

/*******************************************/

void setup() {
	DEBUG_SERIAL.begin(115200);
	DEBUG_SERIAL.println("Start all initilization.....");

	InitializeWeb();//初始化网络通信参数

	PT_INIT(&sendNTPThread);//初始化模拟多线程
	PT_INIT(&receiveNTPThread);//初始化模拟多线程
	PT_INIT(&checkSensorThread);//初始化模拟多线程

}
static int sendNTPThread_entry(struct pt *pt)
{
	PT_BEGIN(pt);
	while (1) {
		sendNTPString(hostIP, infoStr);
		PT_TIMER_DELAY(pt, 50);//间隔50毫秒执行一次
		PT_YIELD(pt);//跳到下一帧继续执行,让出cpu给其他任务
	}
	PT_END(pt);
}

static int receiveNTPThread_entry(struct pt *pt)
{
	String receivedStr;
	PT_BEGIN(pt);
	while (1) {
		receivedStr = ReadNTPpacket();
		HandleCmd(receivedStr);
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
		if (dirType == 1)//left:clockwise
		{
			if (isClockwise == true && sensor_left == false)
			{
				LightOff();
				startmove = false;
				PT_TIMER_DELAY(pt, 500);//停顿0.5毫秒
				count = 0;
				MoveToRight();

			}
			else if (isClockwise == false && sensor_right == false)
			{
				LightOff();
				startmove = false;
				PT_TIMER_DELAY(pt, 500);//停顿0.5毫秒
				MoveToLeft();

			}
		}
		else if (dirType == 2)//left:unClockwise
		{
			if (isClockwise == false && sensor_left == false)
			{
				LightOff();
				startmove = false;
				PT_TIMER_DELAY(pt, 500);//停顿0.5毫秒
				MoveToRight();
				count = 0;

			}
			else if (isClockwise == true && sensor_right == false)
			{
				LightOff();
				startmove = false;
				PT_TIMER_DELAY(pt, 500);//停顿0.5毫秒
				MoveToLeft();

			}
		}
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
		sensor_handle = false;
	else
		sensor_handle = true;

	if (sensor_close == false)
		CLOSE_GLASS_INPUT_PORT++;
	else
		CLOSE_GLASS_INPUT_PORT = 0;
	if (CLOSE_GLASS_INPUT_PORT > MAX_LOW_COUNT)
		sensor_close = false;
	else
		sensor_close = true;
	String  close = sensor_close ? "1" : "0";
	String  handle = sensor_handle ? "1" : "0";
	msg = "close:" + close + "," + "door:" + handle;
	sendNTPString(msg);
	DEBUG_SERIAL.println(msg);
	infoStr = msg;
}


void InitializeWeb() {

	DEBUG_SERIAL.println();
	DEBUG_SERIAL.println();
	DEBUG_SERIAL.println();

	for (uint8_t t = 4; t > 0; t--) {
		DEBUG_SERIAL.printf("[SETUP] WAIT %d...\n", t);
		DEBUG_SERIAL.flush();
		delay(1000);
	}
	WiFi.config(localip, gateway, subnet);
	DEBUG_SERIAL.print("Connecting to ");
	DEBUG_SERIAL.println(ssid);
	WiFi.begin(ssid, pass);
	while (WiFi.status() != WL_CONNECTED) {
		delay(100);
		LightOn() :
			delay(100);
		LightOff();
		delay(100);
		LightOn() :
			delay(100);
		LightOff();
		delay(100);
		DEBUG_SERIAL.print(".");
	}
	DEBUG_SERIAL.println("");
	DEBUG_SERIAL.println("WiFi connected");
	DEBUG_SERIAL.print("IP address: ");
	DEBUG_SERIAL.println(WiFi.localIP());

	DEBUG_SERIAL.println("Starting UDP...");
	udp.begin(localPort);
	DEBUG_SERIAL.print("Local port: ");
	DEBUG_SERIAL.println(udp.localPort());
	LightOn() :
}
String  ReadNTPpacket()
{
	String _msg = "";
	int cb = udp.parsePacket();
	if (!cb) {
	}
	else {
		char  _value[cb];
		udp.read(_value, cb);
		for (int i = 0; i < cb; i++) {
			_msg += _value[i];
		}
	}
	return _msg;
}

unsigned long sendNTPString(IPAddress & address, String data)
{
	int _length = data.length();
	char _value[_length];
	data.toCharArray(_value, _length);
	udp.beginPacket(address, remotePort);
	udp.write(_value, _length);
	udp.endPacket();
	return 1;
}
unsigned long sendNTPpacket(IPAddress & address, char* data, int _length)
{
	udp.beginPacket(address, remotePort);
	udp.write(data, _length);
	udp.endPacket();
}


void LightOn()
{
	digitalWrite(LIGHT_PORT, LOW);
}
void LightOff()
{
	digitalWrite(LIGHT_PORT, HIGH);
}
void loop() {
	sendNTPThread_entry(&sendNTPThread);
	receiveNTPThread_entry(&receiveNTPThread);
	checkSensorThread_entry(&checkSensorThread);
}
