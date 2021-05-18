#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <EEPROM.h>

uint eepaddr = 0;

#define MAX_SETUP_INPUT_LENGHT 64
#define MQTT_PAYLOAD_SIZE 64
#define IR_RECV_PIN D2
#define MQTT_PORT 1883

struct {
	char eepWIFI_SSID[MAX_SETUP_INPUT_LENGHT - 1];
	char eepWIFI_PASSWORD[MAX_SETUP_INPUT_LENGHT - 1];
	char eepBROKER_ADDRESS[MAX_SETUP_INPUT_LENGHT - 1];
	char eepMQTT_TOPIC[MAX_SETUP_INPUT_LENGHT - 1];
} EEPROMdata;

WiFiClient wificlient;
PubSubClient MQTTClient(wificlient);
IRrecv irrecv(IR_RECV_PIN);
decode_results IRDecodedResult;
char mqttPayload[MQTT_PAYLOAD_SIZE];

void setup() 
{
	Serial.begin(115200);
	EEPROM.begin(MAX_SETUP_INPUT_LENGHT*5);
	delay(500);

	setupWizard();

	EEPROM.get(eepaddr, EEPROMdata);
	setupInit();

	MQTTClient.setServer(EEPROMdata.eepBROKER_ADDRESS, MQTT_PORT);
	MQTTClient.setCallback(MQTTCallback);
	irrecv.enableIRIn();
}

unsigned long lastMillis = 0;

void loop() 
{
	char _strbuf[128];

	if (!MQTTClient.connected())
	{
		MQTTConnect();
	}
	MQTTClient.loop();
  
	if (irrecv.decode(&IRDecodedResult)) 
	{
		irrecv.resume();
		uint32_t lowbyte=IRDecodedResult.value & 0x00000000FFFFFFFF;
		uint32_t highbyte=IRDecodedResult.value<<32 & 0x00000000FFFFFFFF;

		char decodeType[16];
		switch (IRDecodedResult.decode_type)
		{
		case NEC: sprintf(decodeType,"NEC");
		case RC5: sprintf(decodeType, "RC5");
		case SONY:sprintf(decodeType, "SONY");
		case UNKNOWN:sprintf(decodeType, "UNKNOWN");
		}
		
		sprintf(_strbuf, "%s/%s", EEPROMdata.eepMQTT_TOPIC, "ir_data");
		snprintf(mqttPayload, MQTT_PAYLOAD_SIZE, "%s:%lX%lX", decodeType,highbyte,lowbyte);
		MQTTClient.publish(_strbuf, mqttPayload);
	}

	if (millis() - lastMillis > 10000)
	{
		lastMillis = millis();
		sprintf(_strbuf, "%s/%s", EEPROMdata.eepMQTT_TOPIC, "watchdog");
		snprintf(mqttPayload, MQTT_PAYLOAD_SIZE, "%d", millis());
		MQTTClient.publish(_strbuf, mqttPayload);
	}
}

void setupInit()
{
	delay(100);

	char _strbuf[128];
	sprintf(_strbuf, "Device parameters:\nSSID:  %s\nPASS:  %s\nBroker/topic: %s/%s", EEPROMdata.eepWIFI_SSID, EEPROMdata.eepWIFI_PASSWORD, EEPROMdata.eepBROKER_ADDRESS, EEPROMdata.eepMQTT_TOPIC);
	Serial.println(_strbuf);

	Serial.print("Connecting to network ");	Serial.print(EEPROMdata.eepWIFI_SSID);

	WiFi.begin((const char*)EEPROMdata.eepWIFI_SSID, (const char*)EEPROMdata.eepWIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}

	randomSeed(micros());
	Serial.println("");
	Serial.print("Connected, IP address: ");
	Serial.println(WiFi.localIP());
}

void MQTTCallback(char* topic, byte* payload, unsigned int length)
{
	Serial.print(topic);
	Serial.print("=");
	Serial.println((char*)payload);
}

void MQTTConnect()
{
	char _strbuf[32];
	while (!MQTTClient.connected())
	{
		Serial.print("Connecting to MQTT broker as ");
		sprintf(_strbuf, "IR2MQTT-%lX", random(0xffff));
		Serial.print(_strbuf);

		//if (MQTTClient.connect(MQTTClientId,user,pass))
		if (MQTTClient.connect(_strbuf))
		{
			Serial.println(" OK!");
			//MQTTClient.subscribe("IRCOMMAND");
		}
		else
		{
			Serial.print(" failed, MQTTClient.state()=");
			Serial.println(MQTTClient.state());
		}
	}
}

void setupWizard()
{
	Serial.println();
	Serial.println();
	Serial.println();
	Serial.println(F("Press any key to setup wifi and MQTT or let the system boot."));

	int i = 5;
	while (!Serial.available() && i > 0)
	{
		Serial.print(i--); Serial.print(" ");
		delay(1000);
	}
	Serial.println();
	if (i == 0) return;

	char _tempStr[MAX_SETUP_INPUT_LENGHT];

	Serial.print(F("Enter Wifi SSID, confirm with enter:"));
	serialReadString(_tempStr, MAX_SETUP_INPUT_LENGHT);	
	strcpy(EEPROMdata.eepWIFI_SSID, _tempStr);

	Serial.print(F("Enter password, confirm with enter:"));
	serialReadString(_tempStr, MAX_SETUP_INPUT_LENGHT);
	strcpy(EEPROMdata.eepWIFI_PASSWORD, _tempStr);

	Serial.print(F("Enter MQTT broker address (IP or name), confirm with enter:"));
	serialReadString(_tempStr, MAX_SETUP_INPUT_LENGHT);
	strcpy(EEPROMdata.eepBROKER_ADDRESS, _tempStr);

	Serial.print(F("Enter MQTT topic used by this device, confirm with enter:"));
	serialReadString(_tempStr, MAX_SETUP_INPUT_LENGHT);
	strcpy(EEPROMdata.eepMQTT_TOPIC, _tempStr);

	EEPROM.put(eepaddr, EEPROMdata);
	EEPROM.commit();
}

void serialReadString(char* inputStr, int maxInputLength)
{
	char c;
	int j = 0;
	bool terminateInput = false;

	//remove the leftover chars from serial buffer
	while (Serial.available()) Serial.read();

	while (!terminateInput)
	{
		if (Serial.available())
		{
			c = Serial.read();
			terminateInput = (c == '\n') || (j == maxInputLength - 1);

			if (terminateInput)
			{
				inputStr[j - 1] = 0;
			}
			else
			{
				inputStr[j] = c;
				j++;
			}
		}
	}
}