#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>

typedef std::function<void(void)> THandlerFunction;

/******************************************************************************************
*******************************网络配置参数（不需要修改）**********************************
*******************************************************************************************/



WiFiUDP udp;
ESP8266WebServer server(80);//建立本地服务器
ESP8266WiFiMulti WiFiMulti;

//bool ssid_sta_setted = false;
bool wifiConnected = false;

const int LIGHT_PORT = 16; //pin0 指示灯:用于指示网络当前网络状态

int connectTimeout = 15000; //连接超时设置

 /******************************网络配置参数结束*************************************************************/


unsigned long SendAdvUdpMsg(String msg ,int remotePort)
{
	IPAddress ad_ip(255, 255, 255, 255); //广播地址
	//int ad_port = 17924; //广播端口
    int size = msg.length();
	char _data[size + 1];
	msg.toCharArray(_data, size + 1);
	udp.beginPacket(ad_ip, remotePort);
	udp.write(_data, size + 1);
	udp.endPacket();
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
void SetConnectTimeout(int timeout )
{
	connectTimeout = timeout;
}
void CloseAP()
{
	WiFi.softAPdisconnect(true);

}
void HandleClient() {
	server.handleClient();
}
void AddHttpRequestHandler(const char* uri, HTTPMethod method, THandlerFunction fn) {
	server.on(uri, method, fn);
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

IPAddress CovertIpStrToIPAddress(String ip)
{

	int firstDotIndex = ip.indexOf('.');
	String firstOctet = ip.substring(0, firstDotIndex);

	String value = ip.substring(firstDotIndex + 1);

	int secondDotIndex = value.indexOf('.');
	String secondOctt = value.substring(0, secondDotIndex);
	value = value.substring(secondDotIndex + 1);

	int thirdDotIndex = value.indexOf('.');
	String thirdOctet = value.substring(0, thirdDotIndex);
	String fourthOctet = value.substring(thirdDotIndex + 1);

	IPAddress address(firstOctet.toInt(), secondOctt.toInt(), thirdOctet.toInt(), fourthOctet.toInt());

	return address;

}

unsigned long SendUDPString(String ip, int remotePort, String data)
{

	int _length = data.length();
	char _value[_length];
	data.toCharArray(_value, _length);
	udp.beginPacket(CovertIpStrToIPAddress(ip), remotePort);
	udp.write(_value, _length);
	udp.endPacket();
	return 1;
}

unsigned long SendUDPBytes(String ip,int remotePort, char* data, int _length)
{
	udp.beginPacket(CovertIpStrToIPAddress(ip), remotePort);
	udp.write(data, _length);
	udp.endPacket();
}




void receiveTip()
{
	digitalWrite(LIGHT_PORT, LOW);
	delay(200);
	digitalWrite(LIGHT_PORT, HIGH);
}

void lightTwinkle()
{
	pinMode(LIGHT_PORT, OUTPUT);
	digitalWrite(LIGHT_PORT, HIGH);
	delay(100);
	digitalWrite(LIGHT_PORT, LOW);
	delay(100);
	digitalWrite(LIGHT_PORT, HIGH);
	delay(100);
	digitalWrite(LIGHT_PORT, LOW);
	delay(100);
	digitalWrite(LIGHT_PORT, HIGH);
	delay(100);
	digitalWrite(LIGHT_PORT, LOW);
	delay(100);

}

bool TryConnectWiFi(String ssid,String psd,String _thirdOctet,String _fourthOctet)
{
	Serial.println();

	char _ssid[ssid.length() + 1]; //注意char要比String 多一位 才行，代表结束符。
	char _psd[psd.length() + 1];

	ssid.toCharArray(_ssid, ssid.length() + 1);
	psd.toCharArray(_psd, psd.length() + 1);


	String thirdOctet = _thirdOctet;
	String fourthOctet = _fourthOctet;

	IPAddress sta_local_IP(192, 168, thirdOctet.toInt(), fourthOctet.toInt());
	IPAddress sta_gateway(192, 168, thirdOctet.toInt(), 1);
	IPAddress sta_subnet(255, 255, 255, 0);


	WiFiMulti.addAP(_ssid, _psd);
	WiFi.config(sta_local_IP, sta_gateway, sta_subnet);

	delay(1000);

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
	if (String(WiFi.SSID()) != String(ssid))
	{
		WiFi.disconnect();
		Serial.println();
		Serial.println("连接" + String(ssid) + "失败！请确认目标WiFi成功开启！");
		return false;
	}

	digitalWrite(LIGHT_PORT, HIGH);

	Serial.println();
	Serial.println("成功连接目标WiFi:" + WiFi.SSID());
	Serial.print("Local IP:");

	Serial.println(WiFi.localIP());

	return true;
}




//void InitializeNetwork()
//{
//	InitializeLight();
//
//	if (!loadWiFiSettings()) {
//		Serial.println("No WiFi settings are loaded!");
//		CreateAPForSetting();
//		WiFi.softAPdisconnect(true);
//	}
//	delay(1000);
//	bool _connectedWifi = false;
//	while (!_connectedWifi)
//	{
//		if (TryConnectWiFi())
//		{
//			_connectedWifi = true;
//			wifiConnected = true;
//		}
//		else
//		{
//			_connectedWifi = false;
//			wifiConnected = false;
//			CreateAPForSetting();
//			WiFi.softAPdisconnect(false);
//			delay(1000);
//		}
//	}
//}
//
