
#include <Arduino.h>
#include <ArduinoJson.h>
#include "FS.h"

void MountFS()
{
	Serial.println("Mounting FileSystem...");
	if (!SPIFFS.begin()) {
		Serial.println("Failed to mount file system");
		return;
	}
}

JsonObject LoadConfig(String filename)
{
	StaticJsonDocument<800> jsonDocument;
	JsonObject jsonObj =  jsonDocument.to<JsonObject>();

	File configFile = SPIFFS.open("/"+ filename +".json", "r");
	if (!configFile) {
		Serial.println("Failed to open "+ filename +" file");
		return  jsonObj;
	}
	size_t size = configFile.size();
	if (size > 1024) {
		Serial.println(filename +" file size is too large");
		return jsonObj;
	}
	String contentStr = configFile.readString();
	configFile.flush();
	configFile.close();
	char contentBuf[size];

	contentStr.toCharArray(contentBuf, size);
	deserializeJson(jsonDocument, contentBuf);

    jsonObj = jsonDocument.as<JsonObject>();

	if (jsonObj.isNull()) {
		Serial.println("Failed to parse " + filename + " file");
		return jsonObj;
	}
	return jsonObj;
}
bool ClearConfig(String filename)
{
	StaticJsonDocument<800> jsonBuffer;
	JsonObject json = jsonBuffer.to<JsonObject>();
	File configFile = SPIFFS.open("/"+ filename +".json", "w");
	serializeJson(json, configFile);
	configFile.flush();
	configFile.close();
}

JsonObject CreateJsonObject(String keys[], String values[], int length)
{
	StaticJsonDocument<800> jsonBuffer;
	JsonObject json = jsonBuffer.to<JsonObject>();

	for (int i = 0; i < length; i++)
	{
		json[keys[i]] = values[i];
	}

	return json;
}

bool  SaveConfig(JsonObject json, String filename) {

	File configFile = SPIFFS.open("/" + filename + ".json", "w");

	if (!configFile) {
		Serial.println("Failed to open "+ filename +" file for writing!");
		return false;
	}
	serializeJson(json, configFile);
	configFile.flush();
	configFile.close();
	return true;
}
