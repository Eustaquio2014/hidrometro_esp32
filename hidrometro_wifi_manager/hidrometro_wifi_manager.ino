#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <FS.h>
#include <WiFiManager.h>
#include <SocketIoClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "esp_task_wdt.h"

//Objetos Globais
SocketIoClient webSocket;
WiFiManager wifiManager;
SemaphoreHandle_t xMutex;

byte mac[6];
char smac[18];

//Hidrometro
float vazao;
volatile int pulsos;
float fluxoAcumulado = 0;
float metroCubico = 0;
const char* dispositivo = "ESP32_Hidromedro1";

// Controle
boolean status_solenoide = false;
boolean status_sensor_presenca = true;

// Portas
const int porta_Hidrometro = GPIO_NUM_35;
const int porta_Presenca = GPIO_NUM_12;
const int porta_Rele = GPIO_NUM_23;
const int porta_Led = 2;

// Configuração do Wi-Fi
char user_socket_server[40];
char user_socket_port[6];

/* SOCKET SERVER */
const char* socket_server;
int socket_port;


/* INTERRUPÇÃO Interrupção do sensor a nível de hardware */
void IRAM_ATTR gpio_isr_handler_up(void* arg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  pulsos++;
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* INICIALIAZA sensor leitura com interrupção na subida de um pulso */
void LER_FLUXO(gpio_num_t Port) {
  gpio_set_direction(Port, GPIO_MODE_INPUT);
  gpio_set_intr_type(Port, GPIO_INTR_NEGEDGE);
  gpio_set_pull_mode(Port, GPIO_PULLUP_ONLY);
  gpio_intr_enable(Port);
  gpio_install_isr_service(0);
  gpio_isr_handler_add(Port, gpio_isr_handler_up, (void*)Port);
}

bool shouldSaveConfig = false;
void saveConfigCallback() {
  Serial.println("Salvando configurações...");
  shouldSaveConfig = true;
}

void format_SPIFFS() {
  bool formatted = SPIFFS.format();
  if (formatted) {
    Serial.println("\n\nSuccess formatting");
  } else {
    Serial.println("\n\nError formatting");
  }
}

void read_SPIFFS() {
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(2048);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if (!deserializeError) {
          Serial.println("\nparsed json");
          strcpy(user_socket_server, json["socket_server"]);
          strcpy(user_socket_port, json["socket_port"]);
        } else {
          Serial.println("failed to load json config");
          format_SPIFFS();
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
    format_SPIFFS();
  }
  delay(1000);
}

void write_SPIFFS() {
  Serial.println("saving config");
  DynamicJsonDocument json(2048);
  json["socket_server"] = user_socket_server;
  json["socket_port"] = user_socket_port;
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }
  serializeJson(json, Serial);
  serializeJson(json, configFile);
  configFile.close();
}

void SocketEvent(const char* payload, size_t length) {
  Serial.printf("Servidor message: %s\n", payload);
  DynamicJsonDocument doc(2048);
  auto deserializeError = deserializeJson(doc, payload);
  serializeJson(doc, Serial);
  if (deserializeError) {
    Serial.print("ERRO JSON OBJECT: ");
    Serial.print(deserializeError.c_str());
    return;
  }

  if (!deserializeError) {
    if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      status_solenoide = doc["status_solenoide"];
      status_sensor_presenca = doc["status_sensor_presenca"];
      xSemaphoreGive(xMutex);
    }
  }

  if (status_sensor_presenca == false) {
    Serial.println("SENSOR DE PRESENÇA DESABILITADO");
    if (status_solenoide == false) {
      Serial.println("FLUXO PERMITIDO");
      digitalWrite(porta_Rele, HIGH);
      digitalWrite(porta_Led, LOW);
    }
    if (status_solenoide == true) {
      Serial.println("FLUXO INTERROMPIDO");
      digitalWrite(porta_Rele, LOW);
      digitalWrite(porta_Led, HIGH);
    }
  }
}


void rotinaHidrometro(void* pvParameters) {
  Serial.print("Tarefa 1: SENSOR HIDROMETRO rodando no núcleo ");
  Serial.println(xPortGetCoreID());
  for (;;) {
    int localPulsos = 0;

    // Proteção do acesso à variável `pulsos`
    if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      localPulsos = pulsos;
      pulsos = 0;  // Resetar após leitura
      xSemaphoreGive(xMutex);
    }

    vazao = (localPulsos * 2.25);  // A cada revolução 2.25 ml
    vazao = (vazao * 60);          // ml por minuto
    vazao = (vazao / 1000);        // Litros por minuto

    // Enviar dados somente se houver fluxo
    if (vazao > 0) {
      DynamicJsonDocument msgObj(256);
      msgObj["dispositivo"] = dispositivo;
      msgObj["fluxo"] = vazao;
      msgObj["mac"] = smac;

      if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
        msgObj["status_solenoide"] = status_solenoide;
        msgObj["status_sensor"] = status_sensor_presenca;
        xSemaphoreGive(xMutex);
      }

      char buffer[256];
      serializeJson(msgObj, buffer, sizeof(buffer));
      webSocket.emit("EspEmit", buffer);
    }

    delay(1000);
  }
}


void rotinaControleAutomacao(void* pvParameters) {
  Serial.print("Tarefa 2: SENSOR DE PRESENÇA rodando no núcleo ");
  Serial.println(xPortGetCoreID());
  for (;;) {
    webSocket.loop();

    int localStatusPresenca = 0;
    int localStatusSolenoide = 0;

    if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      localStatusPresenca = status_sensor_presenca;
      localStatusSolenoide = status_solenoide;
      xSemaphoreGive(xMutex);
    }

    int presenca = digitalRead(porta_Presenca);

    if (localStatusPresenca) {
      Serial.println("Sensor presença habilitado");
      if (presenca) {
        digitalWrite(porta_Rele, LOW);
        digitalWrite(porta_Led, HIGH);
        Serial.println("- Movimento detectado");
      }
      if (!presenca) {
        digitalWrite(porta_Rele, HIGH);
        digitalWrite(porta_Led, LOW);
        Serial.println("- Nenhum movimento");
      }
    } else {
      Serial.println("Sensor presença desabilitado");
    }

    delay(100);
  }
}

TaskHandle_t Task1;
TaskHandle_t Task2;

void ESP_CORES() {
  //Tarefa 1 rotinaHidrometro(), será executada com prioridade 0 no nucleo 0 ------------
  xTaskCreatePinnedToCore(
    rotinaHidrometro,    // Tarefa 1
    "rotinaHidrometro",  // Nome da Tarefa
    15000,               // Tamanho da Pilha da Tarefa
    NULL,                // Parâmetro da tarefa
    0,                   // Prioridade de Execução
    &Task1,              // Identificador de tarefa para acompanhar a tarefa criada
    0);                  // Núcleo que Será Executada = 0

  delay(500);

  //Tarefa 2 rotinaControleAutomacao(), será executada com prioridade 1 no nucleo 1 --------------
  xTaskCreatePinnedToCore(
    rotinaControleAutomacao,    // Tarefa 2
    "rotinaControleAutomacao",  // Nome da Tarefa
    10000,                      // Tamanho da Pilha da Tarefa
    NULL,                       // Parâmetro da tarefa
    1,                          // Prioridade de Execução
    &Task2,                     // Identificador de tarefa para acompanhar a tarefa criada
    1);                         // Núcleo que Será Executada = 1
}

void SetupConexaoWifi() {
  Serial.println("SetupConexaoWifi");
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setWiFiAutoReconnect(true);     /* Auto-Reconexão                      */
  wifiManager.setConnectTimeout(10);          /* Tempo aguardando Auto-Reconexão (10 segundos)                            */
  wifiManager.setConfigPortalTimeout(60 * 5); /* Tempo no qual o  modo de configuração WiFi ficará disponível (3 minutos) */

  WiFiManagerParameter custom_socket_server("server", "IP do servidor", user_socket_server, 40);
  WiFiManagerParameter custom_socket_port("port", "Porta do servidor", user_socket_port, 6);

  wifiManager.addParameter(&custom_socket_server);
  wifiManager.addParameter(&custom_socket_port);

  /*Iniciando modo de configuração Wifi*/
  if (!wifiManager.autoConnect("HidroMonitor 1.0v", "@1234567")) {
    Serial.println("Desconectado!");
    Serial.println("Iniciando modo de Configuração");
    shouldSaveConfig = true;
  } else {
    Serial.println("Conectado!");
    strcpy(user_socket_server, custom_socket_server.getValue());
    strcpy(user_socket_port, custom_socket_port.getValue());
    write_SPIFFS();
  }
  delay(1000);
}

void StatusConexao() {
  Serial.println("Conexão Wi-fi check status ");
  for (;;) {
    if (wifiManager.getWLStatusString() != "WL_CONNECTED") {
      Serial.println("Conexão perdida!");
      ESP.restart();
    }
    delay(30000);
  }
}

void setup() {
  Serial.begin(115200);
  // Criar o mutex
  xMutex = xSemaphoreCreateMutex();
  if (xMutex == NULL) {
    Serial.println("Erro ao criar mutex");
    while (true)
      ;  // Travar se o mutex não for criado
  }

  //Serial.setDebugOutput(true);
  Serial.println("MAIN SETUP");
  read_SPIFFS();
  SetupConexaoWifi();

  WiFi.macAddress(mac);
  sprintf(smac, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.print("MAC address: ");
  Serial.println(smac);

  pinMode(porta_Presenca, INPUT);
  pinMode(porta_Led, OUTPUT);
  pinMode(porta_Rele, OUTPUT);

  digitalWrite(porta_Rele, LOW);

  socket_server = &user_socket_server[0];
  socket_port = atoi(user_socket_port);

  webSocket.on(smac, SocketEvent);
  webSocket.begin(socket_server, socket_port, "/socket.io/?transport=websocket");
  Serial.println("Socket Conectado!");

  LER_FLUXO((gpio_num_t)porta_Hidrometro);
  ESP_CORES();

  StatusConexao();

  delay(2000);
}

void loop() {
}