#include <ESP8266WiFi.h>
#include <MQTT.h>
#include <EEPROM.h>

// defines msg
#define SOS 0xff
#define INIT 0xfe
#define ERR 0xfd
#define TMP 0xfc
#define AIR 0xfb
#define LUM 0xfa
#define END 0x0a //é um \n
#define MAXIMUM_BUFFER_SIZE 32

// Definir o DEBUG como 1 faz com que os prints apareçam
#define DEBUG 1

// O SSID é o nome da rede a que o vosso computador se vai conectar
// A password é a da rede de internet a qual te estas a conectar
#define AP_SSID     "Pedro"
#define AP_PASSWORD "souincrivel99"


// Antes de se definir a password e o username é preciso criar conta em:
// https://easyiot-cloud.com
#define EIOTCLOUD_USERNAME "MDemonKing"
#define EIOTCLOUD_PASSWORD "<(AB$[t~!_zt52@y"


// MQTT é um protocolo de transmissão de mensagens para IoT (Internet of Things) da OASIS (Organization for the Advancement of Structured Information Standards)
// O endereço que definimos aqui é onde se vai buscar a informação
#define EIOT_CLOUD_ADDRESS "cloud.iot-playground.com"


// Definir a localização na EEPROM (memória não volátil) onde se vai armazenar o ficheiro de configuração
#define CONFIG_START 0
#define CONFIG_VERSION "SERv101" //as to be of size 8bytes

// Estrutura dos dados que estão guardados na memoria
struct StoreStruct {
  // Este array de caracteres serve apenas para verificar se a versão está correcta
  char version[8];
  //Aqui definem-se as variáveis  a guardar na EEPROM
  uint moduleId;  // id do módulo
} storage = {
  CONFIG_VERSION, // Guarda NEECv01
  0, // Valor default do módulo 0
};


// Definem-se os nomes dos parâmetros que se vão poder modificar a partir da cloud
#define PARAM_SOS           "Sensor.Parameter1"
#define PARAM_TEMPERATURE   "Sensor.Parameter2"
#define PARAM_LUMINOSITY    "Sensor.Parameter3"
#define PARAM_AIR_QUALITY   "Sensor.Parameter4"

// Definir o tempo que esperamos pelas mensagens da cloud
#define MS_IN_SEC  1000 // 1S

// Criar objecto de MQTT
MQTT myMqtt("", EIOT_CLOUD_ADDRESS, 1883);

// Variáveis globais
String valueStr(""); // Guarda o valor a enviar para a cloud
String topic(""); // Guarda uma string relativamente ao topico a que nos queremos referir quando enviamos uma mensagem para a cloud
boolean result; // Recebe o resultado da comunicação com a cloud
boolean sos; // Indica se existe sos
boolean sosOld; // Indica se existe uma mudança no sinal de sos
int temperature; // guarda a Temperatura
int temperatureOld; // Indica se existe uma mudança no valor da temperatura
int luminosity; // guarda a luminosidade
int luminosityOld; // Indica se existe uma mudança no valor da luminosidade
int air_quality; // guarda a qualidade do ar
int air_qualityOld; // Indica se existe uma mudança no valor da qualidade do ar
boolean stringComplete; // Indica se a string está prota para ser processada
boolean startTransmission; // Indica se a string está prota para ser processada
String inputString = "";         // a String to hold incoming data



// Esta função vai correr só uma vez quando se liga o nodeMCU à corrente
void setup() {
  sos = 0;
  temperature=0; // guarda a Temperatura
  temperatureOld=0;
  luminosity=0; // guarda a luminosidade
  luminosityOld=0;
  air_quality=0;
  air_qualityOld=0;

  // initialize serial:
  Serial.begin(9600);
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);

  // Conecta à rede Wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(AP_SSID, AP_PASSWORD);

  // Os prints que se seguem servem para verificar se o nodeMCU está conectado à rede
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(AP_SSID);

  while (WiFi.status() != WL_CONNECTED) { // Espera até estar conectado
    delay(500);
    Serial.print(".");
  };

  Serial.println("WiFi connected");
  Serial.println("Connecting to MQTT server");
  // Acaba aqui

  // Inicia a comunicação com a EEPROM
  EEPROM.begin(512);
  // Carrega as configurações do utilizador
  loadConfig();


  // Define o id do client
  // Gerar o nome do cliente baseando-se no endereço MAC e nos ultimos 8 bits do contador de microsegundos
  String clientName;
  uint8_t mac[6];
  WiFi.macAddress(mac); // Vai buscar o endereço MAC
  //clientName += "esp8266-";
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);
  myMqtt.setClientId((char*) clientName.c_str()); // .c_str() retorna um ponteiro para a string terminada com \0 (como na linguagem C)

  // Imprime o nome dado ao cliente
  Serial.print("MQTT client id:");
  Serial.println(clientName);

  // Definir as funções que vão ocorrer em resposta a certos eventos
  myMqtt.onConnected(myConnectedCb);
  myMqtt.onDisconnected(myDisconnectedCb);
  myMqtt.onPublished(myPublishedCb);
  myMqtt.onData(myDataCb);

  // Conectar à cloud por MQTT
  myMqtt.setUserPwd(EIOTCLOUD_USERNAME, EIOTCLOUD_PASSWORD);
  myMqtt.connect();

  delay(500); // Esperar 0.5 sec

  // Imprime o id do módulo em utilização
  Serial.print("ModuleId: ");
  Serial.println(storage.moduleId);


  // Se necessário cria um módulo
  if (storage.moduleId == 0)
  {
    //Criar módulo
    Serial.println("create module: /NewModule");
    storage.moduleId = myMqtt.NewModule();

    if (storage.moduleId == 0)
    {
      Serial.println("Module NOT created. Check module limit");
      while (1)
        delay(100);
    }

    // Definir tipo de módulo
    Serial.println("Set module type");
    myMqtt.SetModuleType(storage.moduleId, "SER_PROJECT");


    // Criar o Sensor.Parameter1
    // Sensor.Parameter1 - modo No problem  0 - SOS 1
    Serial.println("new parameter: /" + String(storage.moduleId) + "/" + PARAM_SOS);
    myMqtt.NewModuleParameter(storage.moduleId, PARAM_SOS);
    Serial.println("set isCommand: /" + String(storage.moduleId) + "/" + PARAM_SOS);
    myMqtt.SetParameterIsCommand(storage.moduleId, PARAM_SOS, true);

    // Criar o Sensor.Parameter2
    // Sensor.Parameter2 - Temperatura
    Serial.println("new parameter: /" + String(storage.moduleId) + "/" + PARAM_TEMPERATURE);
    myMqtt.NewModuleParameter(storage.moduleId, PARAM_TEMPERATURE);
    Serial.println("set description: /" + String(storage.moduleId) + "/" + PARAM_TEMPERATURE);
    myMqtt.SetParameterDescription(storage.moduleId, PARAM_TEMPERATURE, "Temperatura: ");
    Serial.println("set Unit: /" + String(storage.moduleId) + "/" + PARAM_TEMPERATURE);
    myMqtt.SetParameterUnit(storage.moduleId, PARAM_TEMPERATURE, "C");
    Serial.println("set Unit: /" + String(storage.moduleId) + "/" + PARAM_LUMINOSITY);
    myMqtt.SetParameterDBLogging(storage.moduleId, PARAM_LUMINOSITY, true);

    // Criar o Sensor.Parameter3
    // Sensor.Parameter3 - luminosidade
    Serial.println("new parameter: /" + String(storage.moduleId) + "/" + PARAM_LUMINOSITY);
    myMqtt.NewModuleParameter(storage.moduleId, PARAM_LUMINOSITY);
    Serial.println("set description: /" + String(storage.moduleId) + "/" + PARAM_LUMINOSITY);
    myMqtt.SetParameterDescription(storage.moduleId, PARAM_LUMINOSITY, "Luminosidade: ");
    Serial.println("set Unit: /" + String(storage.moduleId) + "/" + PARAM_LUMINOSITY);
    myMqtt.SetParameterUnit(storage.moduleId, PARAM_LUMINOSITY, "%");
    Serial.println("set Unit: /" + String(storage.moduleId) + "/" + PARAM_LUMINOSITY);
    myMqtt.SetParameterDBLogging(storage.moduleId, PARAM_LUMINOSITY, true);

    // Criar o Sensor.Parameter4
    // Sensor.Parameter4 - Qualidade do ar
    Serial.println("new parameter: /" + String(storage.moduleId) + "/" + PARAM_AIR_QUALITY);
    myMqtt.NewModuleParameter(storage.moduleId, PARAM_AIR_QUALITY);
    Serial.println("set description: /" + String(storage.moduleId) + "/" + PARAM_AIR_QUALITY);
    myMqtt.SetParameterDescription(storage.moduleId, PARAM_AIR_QUALITY, "Qualidade do ar: ");
    Serial.println("set Unit: /" + String(storage.moduleId) + "/" + PARAM_AIR_QUALITY);
    myMqtt.SetParameterUnit(storage.moduleId, PARAM_AIR_QUALITY, "%");
    Serial.println("set Unit: /" + String(storage.moduleId) + "/" + PARAM_AIR_QUALITY);
    myMqtt.SetParameterDBLogging(storage.moduleId, PARAM_AIR_QUALITY, true);

    // Guarda as novas configurações na EEPROM
    saveConfig();
  }

  // É preciso subscrever aos topicos
  subscribe();
  Serial.print("End Setup");
}

void loop() {
  // Verifica se estamos conectados ao Wifi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }

  // print the string when a newline arrives:
  if (stringComplete) {
    process_string();
    // clear the string:
    inputString = "";
    stringComplete = false;
  }
  if (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    if (inChar == INIT) {
#ifdef DEBUG
      Serial.println("Init");
#endif
      inputString = "";
      startTransmission = true;
    }
    else if (startTransmission) {
      // add it to the inputString:
      inputString += inChar;
    }
    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it:
    if ((inChar == END) && (startTransmission)) {
#ifdef DEBUG
      Serial.println("End");
#endif
      startTransmission = false;
      stringComplete = true;
    }
  }

  // Se o valor de limiar mudar vamos atualizá-lo
  if (sos != sosOld)
  {
    sosOld = sos;
    valueStr = String(sos);

    topic  = "/" + String(storage.moduleId) + "/" + PARAM_SOS;
    result = myMqtt.publish(topic, valueStr, 0, 1);

#ifdef DEBUG
    Serial.print("Publish topic: ");
    Serial.print(topic);
    Serial.print(" value: ");
    Serial.println(valueStr);
#endif
  }
  if (temperature != temperatureOld)
  {
    sosOld = sos;
    valueStr = String(temperature);

    topic  = "/" + String(storage.moduleId) + "/" + PARAM_TEMPERATURE;
    result = myMqtt.publish(topic, valueStr, 0, 1);

#ifdef DEBUG
    Serial.print("Publish topic: ");
    Serial.print(topic);
    Serial.print(" value: ");
    Serial.println(valueStr);
#endif
  }
  if (luminosity != luminosityOld)
  {
    sosOld = sos;
    valueStr = String(luminosity);

    topic  = "/" + String(storage.moduleId) + "/" + PARAM_SOS;
    result = myMqtt.publish(topic, valueStr, 0, 1);

#ifdef DEBUG
    Serial.print("Publish topic: ");
    Serial.print(topic);
    Serial.print(" value: ");
    Serial.println(valueStr);
#endif
  }
  if (air_quality != air_qualityOld)
  {
    sosOld = sos;
    valueStr = String(sos);

    topic  = "/" + String(storage.moduleId) + "/" + PARAM_SOS;
    result = myMqtt.publish(topic, valueStr, 0, 1);

#ifdef DEBUG
    Serial.print("Publish topic: ");
    Serial.print(topic);
    Serial.print(" value: ");
    Serial.println(valueStr);
#endif
  }
}

void process_string() {
  char buf[MAXIMUM_BUFFER_SIZE];
  inputString.toCharArray(buf, MAXIMUM_BUFFER_SIZE);
#ifdef DEBUG
  Serial.println("Enter Process.");
  Serial.println((buf[0] == SOS));
  Serial.println((buf[0] == TMP));
  Serial.println((buf[0] == LUM));
  Serial.println((buf[0] == AIR));
#endif
  if (buf[0] == SOS) {
    sos = 1;
  } else if (buf[0] == TMP) {
    temperature = int(buf[1]);
  } else if (buf[0] == LUM) {
    luminosity = int(buf[1]);
  } else if (buf[0] == AIR) {
    air_quality = int(buf[1]);
  }
}

/*
   A função loadConfig vai carregar as configurações lá guardadas
*/
void loadConfig() {
  // Precisamos de verificar se a versão na memoria corresponde à nossa. É uma maneira fácil de verificar se esta foi corrompida ou não
  bool flag = true;
  int i = 0;
  Serial.println("Loading Config.");
  for ( i = 0; i < sizeof(storage.version); i++)
    if (EEPROM.read(CONFIG_START + i) != CONFIG_VERSION[i]) flag = false;
  if (flag) {
#if DEBUG
    Serial.println("Carregando a config antiga");
#endif
    for (int t = 0; t < sizeof(storage); t++)
      *((char*)&storage + t) = EEPROM.read(CONFIG_START + t);
  }
}

/*
   A função saveConfig vai guardar as configurações na EEPROM
*/
void saveConfig() {
  Serial.println("Saving Config.");
  for (unsigned int t = 0; t < sizeof(storage); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&storage + t));

  EEPROM.commit();
}

/*
   A função macToStr vai transformar o endereço mac numa string
*/
String macToStr(const uint8_t* mac)
{
  String result; //explicar que as funções dão prioridade de nome as variaveis locais
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}


/*
   A função subscribe vai fazer com que o nodeMCU fique à escuta de certos topicos na conexao atraves do protocolo MQTT.
*/
void subscribe()
{
  if (storage.moduleId != 0)
  {
    // Sensor.Parameter1 - pump on/ pump off
    myMqtt.subscribe("/" + String(storage.moduleId) + "/" + PARAM_SOS);
  }
}

/*
   A função myConnectedCb vai executar quando o nodeMCU establecer a conexão por MQTT à cloud
*/
void myConnectedCb() {
#ifdef DEBUG
  Serial.println("Connected to MQTT server");
#endif
  subscribe();
}

/*
   A função myDisconnectedCb vai executar quando o nodeMCU se deconectar da ligação por MQTT à cloud
*/
void myDisconnectedCb() {
#ifdef DEBUG
  Serial.println("disconnected. try to reconnect...");
#endif
  delay(500);
  myMqtt.connect();
}

/*
   A função myPublishedCb vai executar quando o nodeMCU enviar algo para a cloud
*/
void myPublishedCb() {
#ifdef DEBUG
  Serial.println("published.");
#endif
}

/*
   A função myDataCb vai executar quando o nodeMCU receber dados pela conexão MQTT à cloud
*/
void myDataCb(String& topic, String& data) {
#ifdef DEBUG
  Serial.print("Receive topic: ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(data);
#endif
  if (topic == String("/" + String(storage.moduleId) + "/" + PARAM_SOS)) // Executa se a mensagem que receber for relativa a ligar ou desligar o modo automatico
  {
    sos = (data == String("1"));
    if(sos == 0){
      valueStr = String("0");

      topic  = "/" + String(storage.moduleId) + "/" + PARAM_SOS;
      result = myMqtt.publish(topic, valueStr, 0, 1);
    }
#ifdef DEBUG
    Serial.println("SOS mode");
    Serial.println(data);
#endif
  }
}
