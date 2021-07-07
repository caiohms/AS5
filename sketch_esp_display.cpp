#include <Wire.h>
#include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip
#include <SPI.h>
#include "worldmap.h"
#include "FreeRTOS.h"

#define ARDUINO_RDY_PIN 34

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

enum Estado
{
    UP_TO_DATE,
    REQUESTING_DATA,
    AWAITING_DATA,
    SENDING_ACK
};

Estado estado = UP_TO_DATE;

uint8_t spriteYoffset = 60;
char msgRecebida[32];
bool arduino_ready = false;
int ixSAT = 0, iySAT = 0, ixQTH = 0, iyQTH = 0;
double dSatLAT = 0, dSatLON = 0;

//----------------------------------------------------------------

//void receberDados();
void sendACK();
void arduino_rdy();
void requestDados();
void processData();
void drawScreen();

//----------------------------------------------------------------
void setup(void)
{
    Serial.begin(9600);
    Serial.println();
    pinMode(ARDUINO_RDY_PIN, INPUT); // arduino envia sinal HIGH quando estiver pronto
    attachInterrupt(ARDUINO_RDY_PIN, arduino_rdy, FALLING);

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.println("inicializando...");
}

void loop()
{
    Serial.print("Estado = ");
    switch (estado)
    {
    case UP_TO_DATE:
        Serial.println("UP_TO_DATE");
        while (!arduino_ready)
        {
            delay(50);
        }
        arduino_ready = false;
        estado = REQUESTING_DATA;
        break;

    case REQUESTING_DATA:
        Serial.println("REQUESTING_DATA");
        requestDados();
        estado = AWAITING_DATA;
        break;

    case AWAITING_DATA:
        Serial.println("AWAITING_DATA");
        //ixSAT , iySAT , ixQTH , iyQTH , dSatLAT , dSatLON
        processData();
        drawScreen();
        estado = SENDING_ACK;
        break;

    case SENDING_ACK:
        Serial.println("SENDING_ACK");
        delay(500);
        sendACK();
        estado = UP_TO_DATE;
        break;
    }
}

void arduino_rdy()
{
    arduino_ready = true;
    Serial.println("HIGH");
}

void drawScreen()
{
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(3);
    tft.println(" ISS Tracker    Grupo 6");
    spr.createSprite(240, 120);
    spr.setSwapBytes(true);
    spr.pushImage(0, 0, worldmapWidth, worldmapHeight, worldmapData);
    spr.pushSprite(0, spriteYoffset);
    tft.drawLine(ixSAT, spriteYoffset, ixSAT, spriteYoffset + 120, TFT_RED);     // linha vertical
    tft.drawLine(0, iySAT + spriteYoffset, 240, iySAT + spriteYoffset, TFT_RED); // linha horizontal
    tft.fillCircle(ixQTH, iyQTH + spriteYoffset, 3, TFT_YELLOW);                 // minha posicao
    tft.setCursor(0, 190, 1);
    tft.printf("LAT:% 5.2f\nLON:% 5.2f", dSatLAT, dSatLON);
}

void requestDados()
{
    TwoWire meuWire(0); // preciso criar Wire neste escopo para que seja destruido quando teminar, liberando o i2c bus

    meuWire.begin();
    Serial.println("requesting 32 bytes");

    // raramente e sem causa aparente aqui acontece um crash na funcao freertos/queue.c
    // signed portBASE_TYPE xQueueGenericReceive( xQueueHandle pxQueue, void * const pvBuffer, portTickType xTicksToWait, portBASE_TYPE xJustPeeking ) PRIVILEGED_FUNCTION;
    // origem a ser determinada...

    meuWire.requestFrom(2, 32);
    delay(5);

    int count = 0;
    char c = 0;
    while (c != 255 && count < 32)
    {
        c = meuWire.read();
        if (c == 255)
            msgRecebida[count] = '\0';
        else
            msgRecebida[count] = c;
        count++;
    }

    Serial.printf("recebido %d bytes -> msg = %s\n", count, msgRecebida);
}

void processData()
{
    char *token = strtok(msgRecebida, ";");
    int i = 0;

    while (token != NULL)
    {
        switch (i)
        {
        case 0:
            ixSAT = atoi(token);
            break;
        case 1:
            iySAT = atoi(token);
            break;
        case 2:
            ixQTH = atoi(token);
            break;
        case 3:
            iyQTH = atoi(token);
            break;
        case 4:
            dSatLAT = atof(token);
            break;
        case 5:
            dSatLON = atof(token);
            break;
        }
        i++;
        token = strtok(NULL, ";");
    };
}

void sendACK()
{
    TwoWire meuWire(0);
    meuWire.begin();
    meuWire.beginTransmission(2);
    meuWire.write("a");
    meuWire.endTransmission();
}