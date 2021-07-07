#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <Stepper.h>
#include <ArduinoP13.h>

#define MEU_ENDERECO 2

#define stepsPerRevolution 4076 // 2038 com reducao 2:1

#define espDadosPin 12
#define espDisplayPin A0

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11);

enum Estado
{
    INIT,
    AWAITING_TLE_DATA,
    GET_DATA,
    AWAITING_DATA,
    UPDATE_DISPLAY,
    AWAITING_ACK
};

Estado estado = INIT;

char msgRecebida[33];
bool aguardando_mensagem;

const char deg[8] PROGMEM = {B11100, B10100, B11100, B00000, B00000, B00000, B00000, B00000};

//----------------------------------------------------------------
void receiveEvent(int howMany);
void requestEvent();
void pulsoEspDados();
void receber_TLE();
void receber_RTC();
void calcular_variaveis();
void corrigir_norte();
void operar_motores();
void pulsoEspDisplay();
void espera_mensagem();
void receiveEvent(int howMany);
void requestEvent();

/*-------------------- ArduinoP13 library --------------------------------
   Copyright (c) 2019 Thorsten Godau (dl9sec). All rights reserved.
*/
#define MAP_MAXX 240
#define MAP_MAXY 120

const char *tleName = "";
char *tlel1 = "1 25544U 98067A   21167.52208797  .00000857  00000-0  23726-4 0  9991";
char *tlel2 = "2 25544  51.6440 346.0129 0003477  98.6798 357.2899 15.48988832288495";

const char *pcMyName = "";    // Observer name
double dMyLAT = -25.45607841; // Latitude (Breitengrad): N -> +, S -> -
double dMyLON = -49.29113181; // Longitude (Längengrad): E -> +, W -> -
double dMyALT = 930.0;        // Altitude ASL (m)

int iYear = 0;   // Set start year
int iMonth = 0;  // Set start month
int iDay = 0;    // Set start day
int iHour = 0;   // Set start hour
int iMinute = 0; // Set start minute
int iSecond = 0; // Set start second

double dSatLAT = 0; // Satellite latitude
double dSatLON = 0; // Satellite longitude
double dSatAZ = 0;  // Satellite azimuth
double dSatEL = 0;  // Satellite elevation
/*
double dSunLAT = 0; // Sun latitude
double dSunLON = 0; // Sun longitude
double dSunAZ = 0;  // Sun azimuth
double dSunEL = 0;  // Sun elevation
*/
int ixQTH = 0; // Map pixel coordinate x of QTH
int iyQTH = 0; // Map pixel coordinate y of QTH
int ixSAT = 0; // Map pixel coordinate x of satellite
int iySAT = 0; // Map pixel coordinate y of satellite
/*
int ixSUN = 0; // Map pixel coordinate x of sun
int iySUN = 0; // Map pixel coordinate y of sun
*/

char acBuffer[20]; // Buffer for ASCII time

//---------------------------------------------------------------
void setup()
{
    Serial.begin(9600);
    Serial.println();

    pinMode(espDadosPin, OUTPUT);
    pinMode(espDisplayPin, OUTPUT);
    digitalWrite(espDadosPin, LOW);
    digitalWrite(espDisplayPin, LOW);

    myservo.attach(5);
    myStepper.setSpeed(4);

    delay(1000);
    myservo.writeMicroseconds(2390);
    delay(500);
    myservo.writeMicroseconds(620);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.createChar(0, deg);

    Wire.begin(MEU_ENDERECO);
    Wire.onReceive(receiveEvent);
    Wire.onRequest(requestEvent);
}

void loop()
{
    Serial.print(F("Estado = "));
    switch (estado)
    {
    case INIT:
        Serial.println(F("INIT"));
        pulsoEspDados();
        //servoTest();
        estado = AWAITING_TLE_DATA;
        break;

    case AWAITING_TLE_DATA:
        Serial.println(F("AWAITING_TLE_DATA"));
        receber_TLE();
        estado = GET_DATA;
        break;

    case GET_DATA:
        Serial.println(F("GET_DATA"));
        delay(1000);
        pulsoEspDados();
        estado = AWAITING_DATA;
        break;

    case AWAITING_DATA:
        Serial.println(F("AWAITING_DATA"));
        receber_RTC();
        calcular_variaveis();
        corrigir_norte();
        operar_motores();
        estado = UPDATE_DISPLAY;
        break;

    case UPDATE_DISPLAY:
        Serial.println(F("UPDATE_DISPLAY"));
        // precisamos que o esp display solicite os dados
        // enviamos pulso -> ESP display manda requestFrom -> retornamos os dados e aguardamos ACK
        pulsoEspDisplay();
        estado = AWAITING_ACK;
        break;

    case AWAITING_ACK:
        Serial.println(F("AWAITING_ACK"));
        espera_mensagem();
        Serial.println(msgRecebida);
        if (msgRecebida[0] == 'a')

            Serial.println(F("ACK OK"));
        else
            Serial.println(F("ACK FAIL"));

        estado = GET_DATA;
        break;
    }
}

void pulsoEspDados()
{
    digitalWrite(espDadosPin, HIGH); // manda pulso high para ESP32 Dados
    delay(10);
    digitalWrite(espDadosPin, LOW);
}

void pulsoEspDisplay()
{
    /*
    * sinaliza para interrupt do esp que mensagem está pronta
    * 
    * dadaos recebidos anteriormente sao processados e quando o nano estiver pronto
    * envia sinal HIGH a pino conectado a interrupt no esp32master, indicando ready.
    * 
    * esp32master recebe esse sinal e sabe que o arduino nano esta pronto para transmitir.
    * 
    * esp32master cria solicitacao Wire.requestfrom(), arduino recebe e executa evento, 
    * enviando "altura,azimute,latitude,longitude".
    */
    digitalWrite(espDisplayPin, HIGH);
    delay(10);
    digitalWrite(espDisplayPin, LOW); //interrupt no esp -> falling
}

void receber_TLE()
{
    /* aguardar recebimento de dados do TLE.
    dados chegam no seguinte formato: dado(bytes)
    buffer maximo do i2c do arduino = 32 bytes
    TLE1(32) TLE1(32) TLE1(5)
    TLE2(32) TLE2(32) TLE2(5)    */
    espera_mensagem();
    strcpy(tlel1, msgRecebida); //TLE1(32)
    espera_mensagem();
    strcat(tlel1, msgRecebida); //TLE1(32)
    espera_mensagem();
    strcat(tlel1, msgRecebida); //TLE1(5)
    espera_mensagem();
    strcpy(tlel2, msgRecebida); //TLE2(32)
    espera_mensagem();
    strcat(tlel2, msgRecebida); //TLE2(32)
    espera_mensagem();
    strcat(tlel2, msgRecebida); //TLE2(5)
}

void receber_RTC()
{
    /* aguardar recebimento de dados do RTC.
    dados chegam no seguinte formato: dado(bytes)
    YYYY;MM;DD;HH;MM;SS(19)*/

    espera_mensagem();
    char *token = strtok(msgRecebida, ";");
    int i = 0;
    while (token != NULL)
    {
        switch (i)
        {
        case 0:
            iYear = atoi(token);
            break;
        case 1:
            iMonth = atoi(token);
            break;
        case 2:
            iDay = atoi(token);
            break;
        case 3:
            iHour = atoi(token);
            break;
        case 4:
            iMinute = atoi(token);
            break;
        case 5:
            iSecond = atoi(token);
            break;
        default:
            break;
        }
        i++;
        token = strtok(NULL, ";");
    }
}

void corrigir_norte()
{
    // recebe dados da bussola e corrige norte
}

void calcular_variaveis()
{
    //----------- Aruino P13 --------------------
    //P13Sun Sun;                                                       // Create object for the sun
    P13DateTime MyTime(iYear, iMonth, iDay, iHour, iMinute, iSecond); // Set start time for the prediction
    P13Observer MyQTH(pcMyName, dMyLAT, dMyLON, dMyALT);              // Set observer coordinates
    P13Satellite MySAT(tleName, tlel1, tlel2);

    //ixQTH, iyQTH = meu xy
    latlon2xy(ixQTH, iyQTH, dMyLAT, dMyLON, MAP_MAXX, MAP_MAXY);

    //Serial.print("xQTH = ");
    //Serial.print(ixQTH);
    //Serial.print(" yQTH = ");
    //Serial.println(iyQTH);

    MyTime.ascii(acBuffer);            // Get time for prediction as ASCII string
    MySAT.predict(MyTime);             // Predict ISS for specific time
    MySAT.latlon(dSatLAT, dSatLON);    // Get the rectangular coordinates
    MySAT.elaz(MyQTH, dSatEL, dSatAZ); // Get azimut and elevation for MyQTH

    // ixSAT, iySAT == sat xy
    latlon2xy(ixSAT, iySAT, dSatLAT, dSatLON, MAP_MAXX, MAP_MAXY);

    //Serial.print("xSAT = ");
    //Serial.print(ixSAT);
    //Serial.print(" ySAT = ");
    //Serial.println(iySAT);

    //Serial.print(acBuffer);
    //Serial.print(" Lat: ");
    //Serial.print(dSatLAT, 4);
    //Serial.print(" Lon: ");
    //Serial.println(dSatLON, 4);

    //Serial.print("Az: ");
    //Serial.print(dSatAZ, 2);
    //Serial.print(" El: ");
    //Serial.println(dSatEL, 2);
    //Serial.println("");

    //----------------------------------------------------------------
    char buffer[16];

    lcd.clear();
    lcd.setCursor(0, 0);
    sprintf(buffer, "%02d-%02d-%d %02d:%02d", iDay, iMonth, iYear % 100, iHour, iMinute, iSecond);
    lcd.print(buffer);
    lcd.setCursor(0, 1);
    sprintf(buffer, "AZ %03d  EL %+.2d", (int)dSatAZ, (int)dSatEL);
    lcd.print(buffer);
    lcd.setCursor(6, 1);
    lcd.write(0);
    lcd.setCursor(14, 1);
    lcd.write(0);
    //-------------------------------------------
}

int currentStep = 0;
void operar_motores()
{
    // Step motor
    int step = map((long)(dSatAZ * 10), 0, 3600, 0, stepsPerRevolution);

    // motor de passo usa a rotacao mais eficiente possivel
    if ((currentStep - step) < ((-1) * (int)(stepsPerRevolution / 2)))
        currentStep += stepsPerRevolution;
    else if ((currentStep - step) > ((int)(stepsPerRevolution / 2)))
        currentStep -= stepsPerRevolution;

    myStepper.step(currentStep - step);
    currentStep = step;

    // Servo
    int i = map((long)(dSatEL * 10), -900, 750, 620, 2420);
    //int i = map((long)(dSatEL * 10), -900, 850, 700, 2400);
    myservo.writeMicroseconds(i);
}

void espera_mensagem()
{
    aguardando_mensagem = true;
    while (aguardando_mensagem)
    {
        Serial.print(""); // sem isso ele nao sai do loop sla pq
        delay(5);
    }
}

void receiveEvent(int howMany)
{
    //Serial.print("incoming ");
    //Serial.print(howMany);
    //Serial.println(" bytes");

    int count = 0;

    for (; count < howMany; count++)
    {
        char c = Wire.read();
        msgRecebida[count] = c;
    }
    msgRecebida[count] = '\0';
    //Serial.print("-> ");
    //Serial.println(msgRecebida);
    aguardando_mensagem = false;
}

void requestEvent()
{
    // ESP Display solicitou dados
    char temp1[10];
    char temp2[10];
    char buffer[32];
    sprintf(buffer, "%d;%d;%d;%d;%s;%s", ixSAT, iySAT, ixQTH, iyQTH, dtostrf(dSatLAT, 3, 3, temp1), dtostrf(dSatLON, 3, 3, temp2));

    // devolvemos dados de posicao
    Serial.println(buffer);
    Wire.write(buffer);
}