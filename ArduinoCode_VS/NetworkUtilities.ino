#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include "FS.h"

/******************************************************************************************
*******************************网络配置参数（不需要修改）**********************************
*******************************************************************************************/

/*模块AP模式的ssid和password*/
String apssid = "XR-LAB-001";//当8266模块设置为AP模式时的SSID
String appassword = "swl697188";//当8266模块设置为AP模式时的连接密码，可设置为空，则为开放网络，WPA2-PSK模式下，至少8位才有效

								/*目标WiFi的ssid和psd*/
String  ssid_sta = "";  //不能用char* 来定义 ，否则会出现地址冲突的问题，可使用String.toCharArray；但是可以用const char* 来定义，定义后不能修改。
String  password_sta = "";

/*ap模式下的网络参数设置*/
//IPAddress ap_local_IP(192, 168, 1, 1);
//IPAddress ap_gateway(192, 168, 1, 1);
//IPAddress ap_subnet(255, 255, 255, 0);

/*station模式下的网络参数设置*/
String thirdOctet = "1";
String fourthOctet = "2";
String deviceName = "";
IPAddress sta_local_IP(192, 168, thirdOctet.toInt(), fourthOctet.toInt());
IPAddress sta_gateway(192, 168, thirdOctet.toInt(), 1);
IPAddress sta_subnet(255, 255, 255, 0);
IPAddress ad_ip(255, 255, 255, 255); //广播地址
int ad_port = 17924; //广播端口

WiFiUDP udp;
ESP8266WebServer server(80);//建立本地服务器

ESP8266WiFiMulti WiFiMulti;

bool ssid_sta_setted = false;
bool wifiConnected = false;
const int LIGHT_PORT_WAITING = 2; //pin4 指示灯：用于指示网络是否正在配置中.
const int LIGHT_PORT_RIGHT = 16; //pin0 指示灯:用于指示网络是否正确连接.
const int LIGHT_PORT = 16; //pin0 指示灯:用于指示网络当前网络状态
const int connectTimeout = 15000; //连接超时设置

 /******************************网络配置参数结束*************************************************************/
bool loadWiFiSettings()
{
	File configFile = SPIFFS.open("/config.json", "r");
	if (!configFile) {
		Serial.println("Failed to open config file");
		return false;
	}
	size_t size = configFile.size();
	if (size > 1024) {
		Serial.println("Config file size is too large");
		return false;
	}
	String contentStr = configFile.readString();
	configFile.flush();
	configFile.close();
	char contentBuf[size];
	contentStr.toCharArray(contentBuf, size);
	StaticJsonDocument<800> jsonDocument;
	deserializeJson(jsonDocument, contentBuf);
	JsonObject jsonObj = jsonDocument.as<JsonObject>();
	if (jsonObj.isNull()) {
		Serial.println("Failed to parse config file");
		return false;
	}
	String ssid = jsonObj["ssid"];
	String psd = jsonObj["psd"];
	String third = jsonObj["third"];
	String fourth = jsonObj["fourth"];
	String devicename = jsonObj["devicename"];
	ssid.trim();
	psd.trim();
	third.trim();
	fourth.trim();
	devicename.trim();

	Serial.println("   ");
	Serial.println("开始加载网络参数...");
	Serial.print("Loaded ssid_sta: ");
	Serial.println(ssid);
	Serial.print("Loaded password_sta: ");
	Serial.println(psd);
	Serial.print("Loaded third Octet: ");
	Serial.println(third);
	Serial.print("Loaded fourth Octet: ");
	Serial.println(fourth);
	Serial.print("Loaded device name: ");
	Serial.println(devicename);

	if (devicename != "")
	{
		deviceName = devicename;
	}

	if (third != "" && fourth != "")
	{
		thirdOctet = third;
		fourthOctet = fourth;
		sta_local_IP[2] = thirdOctet.toInt();
		sta_local_IP[3] = fourthOctet.toInt();
		sta_gateway[2] = thirdOctet.toInt();

	}

	if (ssid == "" && psd == "")
	{
		return false;
	}
	else
	{
		ssid_sta = ssid;
		password_sta = psd;
		return true;
	}
}

unsigned long sendAdUdp()
{
	int size = deviceName.length();
	char _data[size + 1];
	deviceName.toCharArray(_data, size + 1);
	udp.beginPacket(ad_ip, ad_port);
	udp.write(_data, size + 1);
	udp.endPacket();
}

bool clearConfig()
{
	StaticJsonDocument<800> jsonBuffer;
	JsonObject json = jsonBuffer.to<JsonObject>();
	File configFile = SPIFFS.open("/config.json", "w");
	serializeJson(json, configFile);
	configFile.flush();
	configFile.close();
}

bool saveConfig() {
	StaticJsonDocument<800> jsonBuffer;
	JsonObject json = jsonBuffer.to<JsonObject>();

	json["ssid"] = ssid_sta;
	json["psd"] = password_sta;
	json["third"] = thirdOctet;
	json["fourth"] = fourthOctet;
	json["devicename"] = deviceName;
	File configFile = SPIFFS.open("/config.json", "w");
	if (!configFile) {
		Serial.println("Failed to open config file for writing");
		return false;
	}
	serializeJson(json, configFile);
	configFile.flush();
	configFile.close();
	return true;
}

//浏览器访问网关的响应
void ApHandleRoot() {
	receiveTip();
	server.send(200, "text/html", "<h1>Current device state:waiting for WiFi settings.</h1>");
}
void staHandleRoot() {
	receiveTip();
	server.send(200, "text/html", "<h1>Current device state:Working!</h1>");
}
void handleGetDeviceName()
{
	receiveTip();
	return server.send(200, "text/plain", deviceName);
}
void handleGetNetworkState()
{
	receiveTip();
	if (wifiConnected)
		return server.send(200, "text/plain", "connected");
	else
		return server.send(200, "text/plain", "waiting");
}
void handleSetConfig() {
	receiveTip();

	if (server.args() == 0)
		return server.send(200, "text/plain", "failed");

	Serial.println();
	Serial.println("args count: " + String(server.args()));
	Serial.println("arg1: " + server.arg("ssid"));
	Serial.println("arg2: " + server.arg("psd"));
	Serial.println("arg3: " + server.arg("third"));
	Serial.println("arg4: " + server.arg("fourth"));
	Serial.println("arg5: " + server.arg("devicename"));

	ssid_sta = server.arg(0);
	password_sta = server.arg(1);
	thirdOctet = server.arg(2);
	fourthOctet = server.arg(3);
	deviceName = server.arg(4);

	ssid_sta.trim();
	password_sta.trim();
	thirdOctet.trim();
	fourthOctet.trim();
	deviceName.trim();

	if (thirdOctet != "" && fourthOctet != "")
	{
		sta_local_IP[2] = thirdOctet.toInt();
		sta_local_IP[3] = fourthOctet.toInt();
		sta_gateway[2] = thirdOctet.toInt();
	}


	Serial.println("Received thirdOctet: " + thirdOctet);
	Serial.println("Received fourthOctet: " + fourthOctet);
	Serial.println("Received ssid_sta: " + ssid_sta);
	Serial.println("Received sta_psd: " + password_sta);
	Serial.println("Received device name: " + deviceName);


	if (saveConfig()) {
		ssid_sta_setted = true;
		Serial.println("成功保存至flash！ ");
		server.send(200, "text/plain", "ok");
	}
	else
	{
		server.send(200, "text/plain", "failed");
	}

}


void StartAP(String ssid, String psd) {
	pinMode(LIGHT_PORT, OUTPUT);
	digitalWrite(LIGHT_PORT, LOW);
	char _apssid[ssid.length() + 1];
	char _appassword[psd.length() + 1];
	ssid.toCharArray(_apssid, ssid.length() + 1);
	psd.toCharArray(_appassword, psd.length() + 1);

	Serial.println("Start create AP ,ssid:" + String(_apssid) + "......");
	IPAddress ap_local_IP(192, 168, 1, 1);
	IPAddress ap_gateway(192, 168, 1, 1);
	IPAddress ap_subnet(255, 255, 255, 0);
	WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet);
	delay(1000);
	WiFi.softAP(_apssid, _appassword);
	IPAddress myIP = WiFi.softAPIP();
	Serial.print("Create successfullly,IP address: ");
	Serial.println(myIP);
	digitalWrite(LIGHT_PORT, HIGH);
}
void InitializeWebServer(int port = 80) {
	ESP8266WebServer _server(port);
	server = _server;
}
void BeginWebServer() {
	server.begin();
}
void HandleClient() {
	server.handleClient();
}
void AddHttpRequestHandler(const char* uri, HTTPMethod method, THandlerFunction fn) {
	server.on(uri, method, fn);
}


void CreateAPForSetting()
{
	Serial.println();
	ssid_sta_setted = false;
	/*创建AP热点ssid和psd*/
	char _apssid[apssid.length() + 1];
	char _appassword[appassword.length() + 1];
	apssid.toCharArray(_apssid, apssid.length() + 1);
	appassword.toCharArray(_appassword, appassword.length() + 1);

	Serial.println("开始建立ssid：" + String(_apssid) + "的热点！");
	Serial.println("Configuring access point...");
	IPAddress ap_local_IP(192, 168, 1, 1);
	IPAddress ap_gateway(192, 168, 1, 1);
	IPAddress ap_subnet(255, 255, 255, 0);
	WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet);
	delay(1000);
	WiFi.softAP(_apssid, _appassword);
	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);
	server.on("/", ApHandleRoot);
	server.on("/setConfig", handleSetConfig);
	server.begin();
	Serial.println("HTTP server started");
	Serial.println("等待用户上传网络参数...");

	digitalWrite(LIGHT_PORT_WAITING, LOW);
	while (!ssid_sta_setted) {
		server.handleClient();
	}
	digitalWrite(LIGHT_PORT_WAITING, HIGH);
}

void receiveTip()
{
	digitalWrite(LIGHT_PORT_WAITING, LOW);
	delay(200);
	digitalWrite(LIGHT_PORT_WAITING, HIGH);
}

void lightTwinkle()
{
	digitalWrite(LIGHT_PORT_WAITING, LOW);
	delay(100);
	digitalWrite(LIGHT_PORT_WAITING, HIGH);
	delay(100);
	digitalWrite(LIGHT_PORT_WAITING, LOW);
	delay(100);
	digitalWrite(LIGHT_PORT_WAITING, HIGH);
	delay(100);
	digitalWrite(LIGHT_PORT_WAITING, LOW);
	delay(100);
	digitalWrite(LIGHT_PORT_WAITING, HIGH);
}

bool TryConnectWiFi()
{
	Serial.println();

	char _ssid[ssid_sta.length() + 1]; //注意char要比String 多一位 才行，代表结束符。
	char _psd[password_sta.length() + 1];

	ssid_sta.toCharArray(_ssid, ssid_sta.length() + 1);
	password_sta.toCharArray(_psd, password_sta.length() + 1);


	WiFiMulti.addAP(_ssid, _psd);
	WiFi.config(sta_local_IP, sta_gateway, sta_subnet);

	Serial.println("尝试连接目标WiFi  " + String(_ssid) + "...");
	int count = 0;
	while (WiFiMulti.run() != WL_CONNECTED)
	{
		lightTwinkle();

		Serial.print(".");
		count++;
		if (count * 500 == connectTimeout) //超过Timeout认为没连接上，则返回连接失败
		{
			Serial.println();
			Serial.println("连接" + String(_ssid) + "失败！请确认目标WiFi成功开启！");
			return false;
		}
	}

	if (String(WiFi.SSID()) != String(ssid_sta))
	{
		WiFi.disconnect();
		Serial.println();
		Serial.println("连接" + String(ssid_sta) + "失败！请确认目标WiFi成功开启！");
		return false;
	}

	digitalWrite(LIGHT_PORT_RIGHT, LOW);
	Serial.println();
	Serial.println("成功连接目标WiFi:" + WiFi.SSID());
	Serial.print("Local IP:");
	// Serial.println(WiFi.hostname());//主机名：默认为ESP_20F700
	Serial.println(WiFi.localIP());
	WiFi.softAPdisconnect(true);

	server.on("/", staHandleRoot);//处理只有IP地址的请求
	server.on("/setConfig", handleSetConfig);//处理网络参数设置请求
	server.on("/setSoftSerial", handleSetSoftSerial);//处理串口操作请求
	server.on("/setSwitch", handleSetSwitch);//处理开关操作请求
	server.on("/getNetworkState", handleGetNetworkState);//处理网络状态请求
	server.on("/getDeviceName", handleGetDeviceName);//处理设备名称请求
	server.begin();

	return true;
}

void MountFS()
{
	Serial.println("Mounting FileSystem...");
	if (!SPIFFS.begin()) {
		Serial.println("Failed to mount file system");
		return;
	}
}
void InitializeLight()
{
	Serial.println("初始化指示灯端口...");
	pinMode(LIGHT_PORT_WAITING, OUTPUT);
	digitalWrite(LIGHT_PORT_WAITING, HIGH);
	pinMode(LIGHT_PORT_RIGHT, OUTPUT);
	digitalWrite(LIGHT_PORT_RIGHT, HIGH);
}
void InitializeNetwork()
{
	InitializeLight();
	MountFS();
	if (!loadWiFiSettings()) {
		Serial.println("No WiFi settings are loaded!");
		CreateAPForSetting();
		WiFi.softAPdisconnect(true);
	}
	delay(1000);
	bool _connectedWifi = false;
	while (!_connectedWifi)
	{
		if (TryConnectWiFi())
		{
			_connectedWifi = true;
			wifiConnected = true;
		}
		else
		{
			_connectedWifi = false;
			wifiConnected = false;
			CreateAPForSetting();
			WiFi.softAPdisconnect(false);
			delay(1000);
		}
	}
}

