/*
Freezer Passional - Primeiro código compilado contendo bibliotecas, variáveis, protótipos de funções e definições globais.

Program for controlling an FREZER from a Telegram Bot
You must have a Telegram Bot for this example to work. To make one,
1. Open Telegram (on mobile, web, or desktop)
2. Start a chat with BotFather (@BotFather)
3. Send /start to BotFather, followed by /newbot
4. Send a friendly name for your bot (this isn't the username of bot)
5. Type in and send the username for your bot (ending in bot)
6. Copy the token provided by BotFather and paste it at BOTtoken below
Telegram Bot API documentation available at https://core.telegram.org/bots/api
Note: As of 3rd Jan. 2018, it is necessary to use espressif32_stage
platform for PlatformIO
written by Giacarlo Bacchio (Gianbacchio on Github)
adapted by Brian Lough ( witnessmenow ) for UniversalTelegramBot library
adapted by Pranav Sharma ( PRO2XY ) for ESP32 on PlatformIO
Library related discussions on https://t.me/arduino_telegram_library

https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot/blob/master/platformioExamples/AdvancedLED/src/main.cpp
*******************************************************************/

extern "C" {
  #include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <UniversalTelegramBot.h>

// Needed for WiFiManager library.
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// For LED status
#include <Ticker.h>
Ticker ticker;

#include <EEPROM.h>
#define temp_EEaddress 0
#define heater_EEaddress 4

// Libraries to read temperature.
#include <OneWire.h>
#include <DallasTemperature.h>

// Define modo Debug 
//#define DEBUG

/*
Configuração do sensor de temepratura DS18B20.
Assign the unique addresses of your 1-Wire temp sensors.
See the tutorial on how to obtain these addresses:
http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html
*/
#define ONE_WIRE_BUS D6														// Porta do pino de sinal do DS18B20
OneWire oneWire(ONE_WIRE_BUS);												// Define uma instancia do oneWire para comunicação com o sensor
DallasTemperature sensors(&oneWire);										// Pass our oneWire reference to Dallas Temperature.
DeviceAddress sensor1 = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};	// Array to hold sensor address. Place here your sensor address.


// Parâmetros do Bot

#define BOTtoken "XXXXXXXXX:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"	// your Bot Token (Get from Botfather)
unsigned long Bot_mtbs = 1000;				// Mean time between scan messages. Aloca primeiramente o período curto.
unsigned long Bot_lasttime;		// Last time messages' scan has been done

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Pinout
const int wifiledPin = D5;						// Port connected to Led indicating Wifi Status.
const int freezerPin = D1; 						// Port connected to Led or freezer actuator
const int heaterledPin = D2;					// Port connected to Led or heater actuator.
const int newtork_configPin = D0;				// Port connected to switch for network configuration.

// FREEZER and HEATER parameters and flags
#define min_Temp 0.0							// Menor set point permitido.
#define max_Temp 30.0							// Maior set point permitido.
enum statusModes {ON = 0, OFF}; 				// Define modes of operation for FREEZER.
enum statusModes freezermode = OFF; 			// Start freezer with off.
enum statusModes heatermode = OFF;				// Start heater with off.
bool temp_command_flag = false;					// Flag to indicate that /SetTemp command was sent.
bool temp_change_flag = false;					// Flag to save commands sent to change freezer temp.
bool heater_command_flag = false;				// Flag to save commands sent to change heater status.

#ifdef DEBUG
	#define RETARDO 30
#else
	#define RETARDO (6UL*(1000UL*60UL))			// Retardo após desacionamento do relé em milisegundos: (t(s) *(1000 milisegundos * 60))
#endif
#define FREEZER_HISTERESE 0.2					// °C, Parâmetro define a diferença de temperatura entre liga e desliga do relé.

float temp_set;									// Variável para recuperar temperatura do freezer salva na flash.

// Function prototypes (PlatformIO doesn't make these for you automatically)
void handleNewMessages(int numNewMessages); 																	// parses new messages and sends them to msgInterpretation
void msgInterpretation(String from_name, String from_id, String text, String chat_id, String message_type);
String read_temperature(void);																					// Comanda a leitura de temperatura utilizando o sensor Dallas DS18B20
void clear_flags(void);																							// Torna false os flags relacionados ao Freezer
bool isNumeric(String str);																						// Checa se o valor passado como String está dentro do formato de número float.
bool inRange(String str, float minimum, float maximum);															// Checa se o valor passado como String está dentro do range estabelecido.
void decide_freezer_state(float set_temp);																		// Determina qual será o estado do freezer chamando a função set_freezer_state.
void set_freezer_state(const bool new_state);																	// Altera o estado do freezer para o estado determinado pela função decide_freezer_state.
void set_heater_state(String chat_id);																			// Altera o estado do aqeucimento interno do freezer.
float read_flash_temp(void);																					// Lê o valor de temperatura salvo na Flash do ESP8266.
void write_flash_temp(float new_temp);																			// Escreve o valor de temperatura passado como parâmetro na Flash do ESP8266.
void send_status(String chat_id);																				// Função para ler a temperatura e o estado do freezer e enviar como mensagem ao usuário.
void tick(int pin);																								// Inverte estado do LED utilizado como status de ligado e de conexão do wifi.
void check_heater_status(void);																					// Checa status do aquecimento na inicialização.