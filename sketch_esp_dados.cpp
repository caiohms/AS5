#include <Arduino.h>
#include "FS.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Wire.h>
#include <iostream>
#include <string>

#define ARD_SLAVE_ENDERECO 2

#define ARDUINO_RDY_PIN 23

#define APSSID "Copel 303"
#define APPWD "11028190"

bool debugando = false; // remover

//----------------------------------------------------------------
void get_TLE();
void atualiza_nano_tle();
void atualiza_nano_rtc();
void envia_mensagem(const char message[], int address);
void arduino_rdy();
//void printDateTime(const RtcDateTime &dt);
void writeFile(fs::FS &fs, const char *path, const char *message);
//----------------------------------------------------------------

ThreeWire myWire(2, 15, 4); // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

WiFiMulti wifiMulti;
RtcDateTime now;

enum Estado
{
	AGUARDANDO_INIT,
	RESPONDENDO_INIT,
	AGUARDANDO,
	RESPONDENDO,
};

Estado estado = AGUARDANDO_INIT;

char msgRecebida[33];
bool aguardando_mensagem;
bool arduino_ready = false;

void setup()
{
	Serial.begin(9600);
	Serial.println();
	pinMode(5, OUTPUT);
	digitalWrite(5, HIGH);			 // preciso de mais uma saida de 3.3v para o RTC :)
	pinMode(ARDUINO_RDY_PIN, INPUT); // arduino envia sinal HIGH quando estiver pronto
	attachInterrupt(ARDUINO_RDY_PIN, arduino_rdy, FALLING);
	Rtc.Begin();
}

void loop()
{
	Serial.print("Estado = ");
	switch (estado)
	{
	case AGUARDANDO_INIT:
		Serial.println("AGUARDANDO_INIT");
		while (!arduino_ready)
		{
			delay(50);
		}
		delay(5000);
		arduino_ready = false;
		estado = RESPONDENDO_INIT;
		break;
	case RESPONDENDO_INIT:
	{
		Serial.println("RESPONDENDO_INIT");
		get_TLE();
		delay(100);
		atualiza_nano_tle();
		delay(200);
		estado = AGUARDANDO;
	}
	break;
	case AGUARDANDO:
		Serial.println("AGUARDANDO");
		arduino_ready = false;
		while (!arduino_ready)
		{
			delay(50);
		}
		delay(500);
		arduino_ready = false;
		estado = RESPONDENDO;
		break;
	case RESPONDENDO:
		Serial.println("RESPONDENDO");
		atualiza_nano_rtc();
		estado = AGUARDANDO;
		break;
	}
}

void arduino_rdy()
{
	arduino_ready = true;
	Serial.println("HIGH");
}

// recebe dados TLE da internet (Two Line Element)
void get_TLE()
{
	if (!debugando)
		wifiMulti.addAP(APSSID, APPWD);

	if (!SPIFFS.begin(true))
	{
		Serial.println("SPIFFS Mount Failed");
		return;
	}
	if (!debugando)
	{
		if ((wifiMulti.run() == WL_CONNECTED))
		{
			HTTPClient http;
			http.begin("http://www.celestrak.com/NORAD/elements/stations.txt"); 
			int httpCode = http.GET();

			if (httpCode > 0)
			{
				if (httpCode == HTTP_CODE_OK)
				{
					String payload = http.getString();
					writeFile(SPIFFS, "/stations.txt", payload.c_str());
					Serial.println("arquivo escrito");
				}
			}
			else
			{
				Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
			}
			http.end();
		}
	}
}

// envia mensagem com TLE atualizado para o arduino
void atualiza_nano_tle()
{
	File file = SPIFFS.open("/stations.txt", "r");
	char buffer1[30];									 // buffer para a primeira palavra "ISS (ZARYA)" - nao usado
	char tlel1[70];										 // Two line element line 1
	char tlel2[70];										 // Two line element line 2
	file.readBytesUntil('\n', buffer1, sizeof(buffer1)); // pula primeira string para chegar na 2a linha

	for (int i = 0; i < 70; i++)
	{
		tlel1[i] = file.read(); // read TLE_1
	}
	file.read(); // caractere '\n' ao final da primeira linha
	for (int i = 0; i < 70; i++)
	{
		tlel2[i] = file.read(); // read TLE_2
	}
	tlel1[69] = '\0'; // null ending
	tlel2[69] = '\0'; // null ending
	//Serial.println(tlel1);
	//Serial.println(tlel2);
	file.close();

	char msg[32]; // buffer maximo i2c arduino = 32 bytes

	//Serial.println();

	std::string t1(tlel1);
	std::string t2(tlel2);

	// arduino buffer length 32 (wire.h)
	//tle1
	sprintf(msg, "%s", t1.substr(0, 32).c_str());
	envia_mensagem(msg, ARD_SLAVE_ENDERECO);
	delay(50);
	sprintf(msg, "%s", t1.substr(32, 32).c_str());
	envia_mensagem(msg, ARD_SLAVE_ENDERECO);
	delay(50);
	sprintf(msg, "%s", t1.substr(64, 5).c_str());
	envia_mensagem(msg, ARD_SLAVE_ENDERECO);
	delay(50);
	//tle2
	sprintf(msg, "%s", t2.substr(0, 32).c_str());
	envia_mensagem(msg, ARD_SLAVE_ENDERECO);
	delay(50);
	sprintf(msg, "%s", t2.substr(32, 32).c_str());
	envia_mensagem(msg, ARD_SLAVE_ENDERECO);
	delay(100);
	sprintf(msg, "%s", t2.substr(64, 5).c_str());
	envia_mensagem(msg, ARD_SLAVE_ENDERECO);
	delay(100);
}

// envia mensagem com RTC atualizado para o arduino (Real Time Clock)
void atualiza_nano_rtc()
{
	char msg[32]; // buffer maximo = 32 bytes
	now = Rtc.GetDateTime();
	sprintf(msg, "%d;%d;%d;%d;%d;%d", now.Year(), now.Month(), now.Day(), now.Hour(), now.Minute(), now.Second()); // 19

	Serial.println(msg);
	envia_mensagem(msg, ARD_SLAVE_ENDERECO);
}

// envia mensagem char message[] ao endereco int address
void envia_mensagem(const char message[], int address)
{
	//Serial.printf("transmitindo %s para %X \n", message, address);

	TwoWire meui2c(0); // preciso criar Wire neste escopo para que seja destruido quando teminar, liberando o i2c bus
	meui2c.begin();

	meui2c.beginTransmission(address);
	//Serial.print("size = ");
	meui2c.write(message);
	meui2c.endTransmission();
}

// do exemplo DS1302_Simple
#define countof(a) (sizeof(a) / sizeof(a[0]))
void printDateTime(const RtcDateTime &dt)
{
	char datestring[20];

	snprintf_P(datestring,
			   countof(datestring),
			   PSTR("%04u/%02u/%02u %02u:%02u:%02u"),
			   dt.Year(),
			   dt.Month(),
			   dt.Day(),
			   dt.Hour(),
			   dt.Minute(),
			   dt.Second());
	Serial.println(datestring);
}

// do exemplo SPIFFS_Test
void writeFile(fs::FS &fs, const char *path, const char *message)
{
	Serial.printf("Writing file: %s\r\n", path);

	File file = fs.open(path, FILE_WRITE);
	if (!file)
	{
		Serial.println("- failed to open file for writing");
		return;
	}
	if (file.print(message))
	{
		Serial.println("- file written");
	}
	else
	{
		Serial.println("- frite failed");
	}
}
