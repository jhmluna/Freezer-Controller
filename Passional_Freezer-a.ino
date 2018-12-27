/* Freezer Passional - Segundo código compilado contendo restante do programa.
Example for controlling an FREEZER from a Telegram Bot
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

/* Procedimento para configurar a conexão com rede sem fio:

1 - Desligar o controlador da tomada.
2 - Pressionar e manter pressionado o botão frontal.
3 - Energizar o controlador.
4 - O led fronta de status irá piscar num período aproximado de meio segundo.
5 - Pode soltar o botão frontal.
6 - Com o celular, conectar na rede cujo nome (SSID) é NetworkConfig.
7 - Abrir um navegador no celular e digitar o endereço: http://192.168.4.1.
8 - Clicar no botão Configure Wifi.
9 - Seleciona dentre a lista de redes aquela à ser concetada e escrever a senha.
10 - Clicar em Save. Pronto, o controlador irá reiniciar já conectado à rede. Caso contrário, repita a operação.
*/

void setup() {
	Serial.begin(115200);
	//bot._debug=true; // uncomment to see debug messages from bot library

	pinMode(freezerPin, OUTPUT); 				// initialize freezerPin as an output.
	pinMode(wifiledPin, OUTPUT); 				// initialize wifi ledPin as an output.
	pinMode(heaterledPin, OUTPUT); 				// initialize heater ledPin as an output.
	pinMode(newtork_configPin, INPUT_PULLUP);	// initialize network config switch pin as an input.
	delay(5);
	
	digitalWrite(freezerPin, LOW);		// initialize pin as off
	digitalWrite(wifiledPin, LOW);		// initialize wifi led pin as off
	digitalWrite(heaterledPin, LOW);	// initialize heater led pin as off
	
	// Is configuration portal requested?
	if (digitalRead(newtork_configPin) == LOW) {
		// WiFiManager Local intialization. Once its business is done, there is no need to keep it around
		WiFiManager wifiManager;

		// Sets timeout until configuration portal gets turned off
		// Useful to make it all retry or go to sleep in seconds
		wifiManager.setTimeout(180);

		// Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
		//wifiManager.setAPCallback(configModeCallback);
		
		#ifdef DEBUG
		// Gets called when WiFiManager enters configuration mode.
			Serial.println(F("Entered config mode"));
			Serial.println(WiFi.softAPIP());
			//if you used auto generated SSID, print it
			Serial.println(wifiManager.getConfigPortalSSID());
		#endif
		// Entered config mode, make led toggle faster
		ticker.attach(0.2, tick, wifiledPin);

		// Fetches ssid and pass and tries to connect. If it does not connect, it starts an access point with the specified name
		// here "NetworkConfig" and goes into a blocking loop awaiting configuration.
		if (!wifiManager.startConfigPortal("NetworkConfig")) {
			#ifdef DEBUG
				Serial.println(F("Failed to connect and hit timeout. Now it will reset."));
			#endif
			//reset and try again
			ESP.reset();
		}
		// If you get here you have connected to the WiFi
		Serial.println(F("End of setup... :)"));
		ticker.detach();						// Turn off flashing of wifi status led.
		digitalWrite(wifiledPin, HIGH);			// Turn on wifi status led.
	}
	
	// Polling time in message checking.
	bot.longPoll = 60;

	/*
	Using the ESP8266 EEPROM is different from the standard Arduino EEPROM class.
	You need to call EEPROM.begin(size) before you can start reading or writing, where the size parameter is the number of bytes you want to use store
	Size can be anywhere between a minimum of 4 and maximum of 4096 bytes.
	https://github.com/G6EJD/Using-ESP8266-EEPROM/blob/master/ESP8266_Reading_and_Writing_EEPROM.ino
	*/
	EEPROM.begin(8);  				// EEPROM.begin(Size)
	temp_set = read_flash_temp();	// Lê a temperatura salva na memória flash.

	check_heater_status();
	
	#ifdef DEBUG	
		Serial.printf("Setup heap size: %u\n", system_get_free_heap_size());
	#endif
}

void loop() {
	static bool tick_on = false;		// Salva status do led, se está piscando ou não.
	
	// Every "Bot_mtbs" the bot checks if any messages have arrived
	if (millis() > (unsigned long)(Bot_lasttime + Bot_mtbs)) {

		#ifdef DEBUG
			Serial.print(F("Checking for messages..."));
		#endif
		
		int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
		ESP.wdtFeed();
		
		#ifdef DEBUG
			Serial.print(numNewMessages); Serial.println(F(" new messages"));
			Serial.printf("Loop heap size: %u\n", system_get_free_heap_size());
		#endif

		while(numNewMessages) {
			for (int i = 0; i < numNewMessages; i++) {
				#ifdef DEBUG
					Serial.print(F("Got it. Messages is: "));
					Serial.println(bot.messages[i].text);
				#endif
				handleNewMessages(numNewMessages);
			}
			numNewMessages = bot.getUpdates(bot.last_message_received + 1);
		}

		if ((WiFi.status() != WL_CONNECTED) && (tick_on == false)) {
		// If off line, make led toggle.
			tick_on = true;
			ticker.attach(0.6, tick, wifiledPin);
			#ifdef DEBUG
				Serial.printf("Waiting for connection to %s network.\n", WiFi.SSID().c_str());
			#endif
		}
		else if ((WiFi.status() == WL_CONNECTED) && (tick_on == true)) {
		// If on line and flag tick_on still true, make led on.
			tick_on = false;
			ticker.detach();
			digitalWrite(wifiledPin, HIGH);
			#ifdef DEBUG
				Serial.printf("Connected to %s network.\n", WiFi.SSID().c_str());
			#endif
		}
		
		#ifdef DEBUG
			// Print temperatura salva.
			float temperature_variable = read_flash_temp();
			Serial.print(F("Set Point de temperatura salvo na EEPROM é: "));
			Serial.println(temperature_variable,1);
			// Print temperatura atual.
			float temperature = sensors.getTempC(sensor1);
			Serial.print(F("Temperatura atual é: "));
			Serial.println(temperature,1);
			// Print status.
			Serial.print(F("Freezer atualmente está "));
			Serial.println(freezermode);
			Serial.print(F("Aquecimento interno atualmente está "));
			Serial.println(heatermode);
			Serial.println(F(""));
		#endif

		decide_freezer_state(temp_set);
		Bot_lasttime = millis();
	}
}

void handleNewMessages(int numNewMessages) {
	// Parse new messages and send them for interpretation
	
	ESP.wdtFeed();
	
	// Extract info from the message
	for (int i = 0; i < numNewMessages; i++)
	{
		#ifdef DEBUG
			Serial.println(F("Handling message "));
			Serial.print(i+1);
		#endif
		
		String chat_id = String(bot.messages[i].chat_id);
		String text = bot.messages[i].text;
		String from_name = bot.messages[i].from_name;
		String from_id = bot.messages[i].from_id;
		
		bot.sendChatAction(chat_id, "typing");
		
		if ((from_id == "place here your ID") || (from_id == "case exist, place here your second user's ID"))
		{
			// Call the function for understand the message
			msgInterpretation(from_name, text, chat_id, bot.messages[i].type);
		}
		else
		{
			String goodbye = "Desculpe " + from_name + ".\n";
			bot.sendMessage(chat_id, goodbye, "");
			bot.sendMessage(chat_id, F("Você não está autorizado a acessar este dispositivo!\n\n"), "");
		}
	}
}

void msgInterpretation(String from_name, String text, String chat_id, String message_type) {
	// Interpreta as mensagens que são recebidas.
	#ifdef DEBUG
		Serial.print(F("Interpreting message: ")); Serial.println(text);
		Serial.print(F("Type: ")); Serial.println(message_type);
		Serial.print(F("From: ")); Serial.println(from_name);
		Serial.printf("msgInterpretation heap size: %u\n", system_get_free_heap_size());
	#endif

	if ((text == "/start") || (text == "/help"))
	{ // First interaction of user
		clear_flags();
		String welcome = "Olá " + from_name + "!\n";
		bot.sendMessage(chat_id, welcome, "");
		bot.sendMessageWithReplyKeyboard(chat_id, F("Sou seu Bot Passional rodando no ESP8266 para controlar a temperatura de fermentação da cerveja.\n\n"
		"Selecione uma das opções abaixo. - /options\n\n"), "Markdown", F("[[\"/SetTemp\"],[\"/status\"],[\"/Heater\"]]"), true);
	}
	else if (text == "/options")
	{ // List out the custom keyboard
		clear_flags();
		bot.sendMessageWithReplyKeyboard(chat_id, F("Selecione uma dentre as seguintes opções abaixo:"), "", F("[[\"/SetTemp\"],[\"/status\"],[\"/Heater\"]]"), true);
	}
	else if (text == "/status")
	{  // Report present freezermode to user
		clear_flags();
		send_status(chat_id);
	}
	else if (text == "/SetTemp")
	{	// Send an inline keyboard for confirming changing in Temp values
		// For inline keyboard markup, see https://core.telegram.org/bots/api#inlinekeyboardmarkup
		temp_command_flag = true;
		bot.sendMessageWithInlineKeyboard(chat_id, F("Você deseja realmente alterar a temperatura do freezer?\n"), "",
		F("[[{ \"text\" : \"Sim\", \"callback_data\" : \"/yes\" }],[{ \"text\" : \"Não\", \"callback_data\" : \"/no\" }]]"));
	}
	else if (text == "/Heater")
	{	// Send an inline keyboard for confirming changing in heater status
		heater_command_flag = true;
		String response = "Aquecimento interno atualmente está ";
		switch (heatermode)
		{
			case ON:
				response += "LIGADO.\n";
				break;
			case OFF:
				response += "DESLIGADO.\n";
				break;
		}
		response += "Você deseja realmente alterar o status do aquecimento interno do freezer?\n";
		bot.sendMessageWithInlineKeyboard(chat_id, response, "",
		F("[[{ \"text\" : \"Sim\", \"callback_data\" : \"/yes\" }],[{ \"text\" : \"Não\", \"callback_data\" : \"/no\" }]]"));
	}
	else if ((message_type == "callback_query") && (temp_command_flag == true) && (text == "/yes"))
	{	// Received when user taps a button on inline keyboard on change temp asking
		clear_flags();
		temp_change_flag = true;
		bot.sendMessage(chat_id, F("Digite o novo valor de temperatura utilizando apenas números, sem espaços, com um ponto para indicar o separador decimal.\n\n"
		"Atenção! O valor deve estar entre 0.0 e 30.0.\n"), "");
	}
	else if ((message_type == "callback_query") && (heater_command_flag == true) && (text == "/yes"))
	{	// Received when user taps a button on inline keyboard on heater status change asking.
		clear_flags();
		set_heater_state(chat_id);
		send_status(chat_id);
	}
	else if ((message_type == "callback_query") && ((temp_command_flag == true) || (heater_command_flag == true)) && (text == "/no"))
	{  // Report present freezermode to user
		clear_flags();
		bot.sendMessage(chat_id, F("Ok, não haverá mudança!\n"),"");
		send_status(chat_id);
	}
	else if (temp_change_flag == true)
	{	// Trata o valor recebido para ajuste da temperatura.
		clear_flags();
		bool isNumber = false;
		bool isinRange = false;
		isNumber = isNumeric(text);
		isinRange = inRange(text, min_Temp, max_Temp);
		if (isNumber && !isinRange)
			{
				#ifdef DEBUG
					Serial.print(F("Temperatura de fermentação definida está fora do range de "));
					Serial.print(min_Temp);
					Serial.print(F(" a "));
					Serial.print(max_Temp);
				#endif
				bot.sendMessage(chat_id,F("Temperatura de fermentação definida está fora do range de 0.0 a 30.0.\n"),"");
				send_status(chat_id);
			}
		else if (isNumber && isinRange)
			{
				temp_set = text.toFloat();
				#ifdef DEBUG
					Serial.print(F("Nova temperatura de fermentação aceita. "));
					Serial.println(temp_set);
				#endif
				write_flash_temp(temp_set);
				bot.sendMessage(chat_id,F("Nova temperatura de fermentação aceita. "),"");
				decide_freezer_state(temp_set);
				send_status(chat_id);
			}
		else
			{
				bot.sendMessageWithReplyKeyboard(chat_id, F("Desculpe! \nO formato ou o valor de temperatura não está correto.\n\n"
				"Por favor, reinicie a operação selecionando uma das opções abaixo. - /options\n"), "",F("[[\"/SetTemp\"],[\"/status\"],[\"/Heater\"]]"), true);
			}
	}
	else
	{	// Conditional to unknow command.
		bot.sendMessageWithReplyKeyboard(chat_id, F("Desculpe! \nO seu comando não está correto ou você não o enviou na sequência exata.\n\n"
		"Selecione uma dentre as seguintes opções abaixo:"), "", F("[[\"/SetTemp\"],[\"/status\"],[\"/Heater\"]]"), true);
	}
}

String read_temperature(void) {
	// Lê a temperatura do sensor onewire.
	sensors.requestTemperatures();					// Sends command for all devices on the bus to perform a temperature conversion
	float temperature = sensors.getTempC(sensor1);	// Returns temperature in degrees C
	String TempVal = String(temperature);
	return TempVal;
}

void send_status(String chat_id) {
	// Envia o status do controlador como mensagem para o Telegram.
	String stringVal = read_temperature();
	String temp_val = String(temp_set, 1);
	String response = "A temperatura atual do freezer é " + stringVal + " °C.\n";
	response += "A temperatura de fermentação definida é " + temp_val + " °C.\n\n";
	response += "Freezer atualmente está ";
	switch (freezermode)
	{
		case ON:
			response += "LIGADO.\n\n";
			break;
		case OFF:
			response += "DESLIGADO.\n\n";
			break;
	}
	response += "Aquecimento interno atualmente está ";
	switch (heatermode)
	{
		case ON:
			response += "LIGADO.\n";
			break;
		case OFF:
			response += "DESLIGADO.\n";
			break;
	}
	#ifdef DEBUG
		Serial.println(response);
	#endif
	bot.sendMessage(chat_id, response, "");
}

void clear_flags(void) {
	// Zera as variáveis utilizadas como flag de controle.
	temp_command_flag = false;
	temp_change_flag = false;
	heater_command_flag = false;
}

bool isNumeric(String str) {
	// Checa se o valor passado como string é um número válido.
   //  http://tripsintech.com/arduino-isnumeric-function/

	unsigned int stringLength = str.length();
 
    if (stringLength == 0) return false;
 
    boolean seenDecimal = false;
 
    for(unsigned int i = 0; i < stringLength; ++i)
	{
        if (isDigit(str.charAt(i))) continue;
 
        if (str.charAt(i) == '.')
		{
			if (seenDecimal) return false;
			seenDecimal = true;
            continue;
        }
        return false;
    }
    return true;
}

bool inRange(String str, float minimum, float maximum) {
	// Checa se o valor passado como string está dentro do range.
	float value = str.toFloat();
	return ((minimum <= value) && (value <= maximum));
}

void decide_freezer_state(float new_temp) {
	// Define qual será o estado do freezer em função da temperatura atual e da temperatura ajustada.
	String tempVal = read_temperature();
	float temperature_value = tempVal.toFloat();
	if (freezermode == ON)
	{ // freezer was on
		if (temperature_value <= (new_temp - FREEZER_HISTERESE))
		{
			set_freezer_state(OFF);
			#ifdef DEBUG
				Serial.println(F("set_freezer_state(OFF)."));
			#endif
		}
	}
	else
	{ // freezer was off
		if (temperature_value >= (new_temp + FREEZER_HISTERESE))
		{
			set_freezer_state(ON);
			#ifdef DEBUG
				Serial.println(F("set_freezer_state(ON)."));
			#endif
		}
	}
}

void set_freezer_state(const bool new_state) {
	// Chamada pela função decide_freezer_state para ligar ou desligar o freezer quando passado o tempo de retardo.
	static unsigned long previousUpdate = 0;        // Will store last time freezer was updated
	unsigned long currentMillis = millis();
	
	if ((freezermode == OFF) && (new_state == ON))
	{ // Se está desligado,e o novo estado é ligado, checa se o período de retardo após desacionamento já foi superado para poder ligar novamente.
		#ifdef DEBUG
			Serial.println(currentMillis - previousUpdate);
		#endif
		if ((unsigned long)(currentMillis - previousUpdate) >= RETARDO)
		{
			digitalWrite(freezerPin,HIGH);				// Liga o freezer
			freezermode = ON;							// Set flag de status do freezer para on.
			#ifdef DEBUG
				Serial.println(F("Enviado comando para ligar o freezer."));
			#endif
		}
		if (heatermode == ON)
		{ // Desliga automaticamente o aquecimento quando novo estado do freezer é ON e o estado atual do aquecimento é ON.
			digitalWrite(heaterledPin,LOW);			// Desliga o aquecedor.
			heatermode = OFF;						// Set flag de status do aquecedor para off.
			#ifdef DEBUG
				Serial.println(F("Enviado comando para DESLIGAR o aquecimento interno do freezer."));
			#endif
		}
	}
	else if ((freezermode == ON) && (new_state == OFF))
	{ // Se está ligado,e o novo estado é desligado, desliga o freezer imediatamente.
		previousUpdate = currentMillis;				// Salva a última vez que desligou o freezer
		digitalWrite(freezerPin,LOW);				// Desliga o freezer
		freezermode = OFF;							// Set flag de status do freezer para off.
		#ifdef DEBUG
			Serial.println(F("Enviado comando para desligar o freezer."));
		#endif
	}
}

void set_heater_state(String chat_id) {
	// Inverte o estado do acionamento do aquecimento interno.
	switch (heatermode)
	{
		case ON:
		{
			digitalWrite(heaterledPin,LOW);			// Desliga o aquecedor.
			heatermode = OFF;						// Set flag de status do aquecedor para off.
			bot.sendMessage(chat_id,F("Enviado comando para DESLIGAR o aquecimento interno do freezer."),"");
			#ifdef DEBUG
				Serial.println(F("Enviado comando para DESLIGAR o aquecimento interno do freezer."));
			#endif
			break;
		}
		case OFF:
		{
			digitalWrite(heaterledPin,HIGH);			// Liga o aquecedor
			heatermode = ON;							// Set flag de status do aquecedor para on.
			bot.sendMessage(chat_id,F("Enviado comando para LIGAR o aquecimento interno do freezer."),"");
			#ifdef DEBUG
				Serial.println(F("Enviado comando para LIGAR o aquecimento interno do freezer."));
			#endif
			break;
		}
	}
	EEPROM.put(heater_EEaddress,heatermode);
	EEPROM.commit();
}

float read_flash_temp(void) {
	// Lê o valor de temperatura salvo na Flash
	float temperature_variable;						// Variável temporária para alocar a temperatura salva.
	EEPROM.get(temp_EEaddress,temperature_variable);
	return temperature_variable;
}

void write_flash_temp(float new_temp) {
	// Escreve o valor de temperatura salvo na Flash*/ MD
	float temperature_variable;			// Variável temporária para alocar a temperatura à ser salva.
	EEPROM.get(temp_EEaddress,temperature_variable);
	if (temperature_variable != new_temp)
	{
		EEPROM.put(temp_EEaddress,new_temp);
		EEPROM.commit();
	}
	#ifdef DEBUG
		EEPROM.get(temp_EEaddress,temperature_variable);
		Serial.print(F("EEPROM contents at Address is: "));
		Serial.println(temperature_variable,1);
	#endif
}

void tick(int pin) {
	// Inverte estado do LED utilizado como status de ligado e de conexão do wifi.
	int state = digitalRead(pin);  // get the current state of led pin
	digitalWrite(pin, !state);     // set pin to the opposite state
}

void check_heater_status(void) {
	// Checa na memória EEPROM o status do aquecimento durante inicialização.
	EEPROM.get(heater_EEaddress,heatermode);
	
	if ((heatermode != ON) && (heatermode != OFF)) heatermode = OFF;
	
	switch (heatermode)
	{
		case ON:
		{
			digitalWrite(heaterledPin, HIGH);			// Liga o aquecedor.
			#ifdef DEBUG
				Serial.println(F("Enviado comando para LIGAR o aquecimento interno do freezer."));
			#endif
			break;
		}
		case OFF:
		{
			digitalWrite(heaterledPin, LOW);			// Desliga o aquecedor
			#ifdef DEBUG
				Serial.println(F("Enviado comando para DESLIGAR o aquecimento interno do freezer."));
			#endif
			break;
		}
	}
}