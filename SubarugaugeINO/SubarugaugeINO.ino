#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>   
#include <FS.h> 

// Pino do botão
#define buttonPin D5

//pino do buzzer
#define buzzer D6

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); 

const char htmlCode[] = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Subaru Gauges</title>
    <style>
        @keyframes progress {
            0% { --percentage: 0; }
            100% { --percentage: var(--value); }
        }

        @property --percentage {
            syntax: '<number>';
            inherits: true;
            initial-value: 0;
        }

        [role="progressbar"], [role="fuelTemp"] {
            animation: progress 2s 0.5s forwards;
            width: 140px;
            aspect-ratio: 2 / 1;
            border-radius: 50% / 100% 100% 0 0;
            position: relative;
            overflow: hidden;
            display: flex;
            align-items: flex-end;
            justify-content: center;
        }

        [role="progressbar"]::before, [role="fuelTemp"]::before {
            --percentage: var(--value);
            content: "";
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: conic-gradient(
                from 0.75turn at 50% 100%, 
                var(--primary, #339974) calc(var(--percentage) * 1% / 2), 
                white calc(var(--percentage) * 1% / 2 + 0.1%)
            );
            mask: radial-gradient(at 50% 100%, white 55%, transparent 55.5%);
            -webkit-mask: radial-gradient(at 50% 100%, #0000 55%, #000 55.5%);
        }

        [role="progressbar"]::after, [role="fuelTemp"]::after {
            counter-reset: percentage var(--value);
            content: counter(percentage) var(--unit, '%');
            font-family: Helvetica, Arial, sans-serif;
            font-size: 18px;
            color: white;
        }

        /* Layout */
        .container {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 20px;
            padding: 10px;
        }

        .graph-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 5px;
            width: 45%; /* Ocupa 45% para dois gráficos lado a lado */
            max-width: 160px;
        }

        span {
            color: antiquewhite;
            font-size: 14px;
            font-family: Helvetica, Arial, sans-serif;
            text-align: center;
        }

        body {
            margin: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            background: #282a35;
            color: antiquewhite;
            font-family: Helvetica, Arial, sans-serif;
            height: 100vh;
            padding: 10px;
            box-sizing: border-box;
        }

        h1 {
            font-size: 22px;
            text-align: center;
            margin-bottom: 20px;
        }

        .btn-config {
            background-color: #339974;
            color: white;
            font-family: Helvetica, Arial, sans-serif;
            font-size: 16px;
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            margin-top: 20px;
            transition: background-color 0.3s;
        }

        .btn-config:hover {
            background-color: #2b7d63;
        }

        .form-container {
            display: none;
            position: fixed;
            top: 50%;
            left: 47%;
            transform: translate(-47%, -50%);
            background: #393e4d;
            padding: 34px;
            border-radius: 10px;
            box-shadow: 0 5px 15px rgba(0, 0, 0, 0.5);
            z-index: 1000;
            width: 80%;
            max-width: 400px;
        }

        .form-container label {
            display: block;
            font-size: 14px;
            margin-bottom: 5px;
        }

        .form-container input {
            width: 100%;
            padding: 10px;
            margin-bottom: 15px;
            border: 1px solid #ccc;
            border-radius: 5px;
        }

        .form-container .btn-save {
            background-color: #339974;
            color: white;
            font-size: 16px;
            padding: 10px 20px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            float: right;
            transition: background-color 0.3s;
        }

        .form-container .btn-save:hover {
            background-color: #2b7d63;
        }

        .form-container .close-btn {
            background: none;
            border: none;
            color: white;
            font-size: 20px;
            position: absolute;
            top: 10px;
            right: 10px;
            cursor: pointer;
        }

        .message {
            display: none;
            background: #339974;
            color: white;
            font-size: 16px;
            padding: 10px 20px;
            border-radius: 5px;
            text-align: center;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <h1>Subaru Gauges</h1>
    <div class="container">
        <div class="graph-container">
            <div id="waterTemp" role="progressbar" style="--value: 0; --unit: '°C';"></div>
            <span>Temperatura de água</span>
        </div>
        <div class="graph-container">
            <div id="oilTemp" role="fuelTemp" style="--value: 0; --unit: '°C';"></div>
            <span>Temperatura de óleo</span>
        </div>
        <div class="graph-container">
            <div id="turboPressure" role="progressbar" style="--value: 0; --unit: 'Bar';"></div>
            <span>Pressão de turbo</span>
        </div>
        <div class="graph-container">
            <div id="oilPressure" role="fuelTemp" style="--value: 0; --unit: 'Bar';"></div>
            <span>Pressão de óleo</span>
        </div>
    </div>
    <button class="btn-config" onclick="openForm()">Configurar avisos</button>
    <div class="message" id="message">Os avisos foram configurados</div>

    <div class="form-container" id="configForm">
        <button class="close-btn" onclick="closeForm()">×</button>
        <label for="waterTempInput">Temperatura de água</label>
        <input type="text" id="waterTempInput">
        <label for="oilTempInput">Temperatura de óleo</label>
        <input type="text" id="oilTempInput">
        <label for="turboPressureInput">Pressão de turbo</label>
        <input type="text" id="turboPressureInput">
        <label for="oilPressureInput">Pressão de óleo</label>
        <input type="text" id="oilPressureInput">
        <button class="btn-save" onclick="saveForm()">Salvar</button>
    </div>

    <script>
        function openForm() {
            document.getElementById('configForm').style.display = 'block';
        }

        function closeForm() {
            document.getElementById('configForm').style.display = 'none';
        }

        function saveForm() {
            closeForm();
            const message = document.getElementById('message');
            message.style.display = 'block';
            setTimeout(() => {
                message.style.display = 'none';
            }, 3000); // Exibe a mensagem por 3 segundos
        }

        var Socket;
        function init() {
            Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
            Socket.onmessage = function (event) {
                processCommand(event);
            };
        }

        function processCommand(event) {
            var obj = JSON.parse(event.data);

            document.getElementById('waterTemp').style = "--value: " + obj.waterTemp + "; --unit: '°C';";
            document.getElementById('oilTemp').style = "--value: " + obj.oilTemp + "; --unit: '°C';";
            document.getElementById('turboPressure').style = "--value: " + obj.turboPressure + "; --unit: 'Bar';";
            document.getElementById('oilPressure').style = "--value: " + obj.oilPressure + "; --unit: 'Bar';";
        }

        window.onload = function (event) {
            init();
        }
    </script>
</body>
</html>
)=====";

// Configurações da rede Wi-Fi (modo AP)
const char* ssid = "SubaruGauge";
const char* password = "lukebigcock";

//config do display
LiquidCrystal_I2C lcd(0x27, 20, 4); 


int menuIndex = 0;  // Índice atual do menu
bool inMenu = false;
unsigned long buttonPressStart = 0;
bool buttonHeld = false;
unsigned long previousMillis = 0;
const char* unidadeMedida[] = {"Bar", "PSI"};
int unidadeMedidaIndex;
int menuOld;

//Sensores maximos e atuais
int maxBoostValue = 0;  // Variável para armazenar o valor máximo
int currentBoostValue = 0;

// Itens do menu
const char* menuItems[] = {
  "Geral",
  "Temperatura Agua",
  "Temperatura Oleo",
  "Pressao Oleo",
  "Pressao Turbo",
  "Alterar Medicao"
};

byte block[8] = {
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};

const int menuSize = sizeof(menuItems) / sizeof(menuItems[0]);

void handleRoot() {
  server.send(200, "text/html", htmlCode);
}

void setup() {
  Serial.begin(115200);
  lcd.init();                       
  lcd.backlight();                  
  lcd.clear();  

  

  // Configuração do botão
  pinMode(buttonPin, INPUT_PULLUP);  // Botão ativo em LOW

  pinMode(buzzer, OUTPUT); 

  // Configurando o ESP como Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP do Access Point: ");
  Serial.println(IP);
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
  webSocket.begin();

  // Configuração do OTA
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Iniciando atualização OTA: " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\nAtualização finalizada.");
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Erro [%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Falha de autenticação");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Erro ao iniciar");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Erro de conexão");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Erro ao receber dados");
    } else if (error == OTA_END_ERROR) {
      Serial.println("Erro ao finalizar");
    }
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA iniciado.");

  if (!SPIFFS.begin()) {
    Serial.println("Erro ao inicializar SPIFFS");
    return;
  }
  unidadeMedidaIndex = loadPreferences("config.json");
  lcd.setCursor(0,1);
  lcd.print("Hello Luke Big Cock");
  delay(2000);
  lcd.clear();
  mainScreen();
  Serial.println(unidadeMedidaIndex);
}

void loop() {
  ArduinoOTA.handle(); // Mantenha o OTA ativo
  server.handleClient();
  webSocket.loop();
  monitoringBtn();
  if(!inMenu){
    switch (menuIndex){
      case 0:// geral
        mainScreen();
        monitoringBtn();
        break;     
      case 1://Temperatura Agua
        waterTemp();
        monitoringBtn();
        break;
      case 2://Temperatura Oleo
        oilTemp();
        monitoringBtn();
        break;
      case 3://Pressao Oleo
        oilPressure();
        monitoringBtn();
        break;
      case 4://Pressao Turbo
        boost();
        monitoringBtn();
        break;
      case 5://Alterar unidade de medição
        changeMeasure();
        monitoringBtn();
        break;
    }
  }
 
}

void monitoringBtn(){
  bool buttonState = digitalRead(buttonPin) == LOW;

  // Lógica para segurar o botão
  if (buttonState) {
    if (!buttonHeld) {
      buttonPressStart = millis();
      buttonHeld = true;
    } else if (millis() - buttonPressStart >= 3000 && !inMenu) {
      // Abrir o menu após segurar por 3 segundos
      inMenu = true;
      menuOld = menuIndex;
      menuIndex = 0;
      lcd.clear();
      displayMenu();
    } else if (millis() - buttonPressStart == 2000 && inMenu) {
      // Selecionar a opção após segurar por 2 segundos
      executeMenuOption(menuIndex);
      inMenu = false;
    }
  } else {
    if (buttonHeld) {
      if (millis() - buttonPressStart < 3000 && inMenu) {
        // Mover o cursor se botão for pressionado brevemente
        menuIndex = (menuIndex + 1) % menuSize;
        displayMenu();
        
      }
    }
    buttonHeld = false;
  }
}


void displayMenu() {
  lcd.clear();
  for (int i = 0; i < 4; i++) {
    int itemIndex = i + menuIndex;
    if (itemIndex >= menuSize) break; // Evita acessar fora da lista

    lcd.setCursor(0, i);
    if (itemIndex == menuIndex) {
      lcd.print("> ");  // Cursor indicando a opção selecionada
    } else {
      lcd.print("  ");  // Espaço em branco para as opções não selecionadas
    }
    lcd.print(menuItems[itemIndex]);
  }
}



// Função para executar a opção selecionada
void executeMenuOption(int option) {
  lcd.clear();
  lcd.print("Opcao selecionada:");
  lcd.setCursor(0, 1);
  lcd.print(menuItems[option]);

  if (option == menuSize - 1) {
    // Última opção (Sair)
    lcd.clear();
  }
  lcd.clear();
}

void mainScreen(){
  
  lcd.setCursor(3, 0);
  lcd.print("Temp");

  lcd.setCursor(14, 0);
  lcd.print(String(unidadeMedida[unidadeMedidaIndex]));

  lcd.setCursor(0, 1);
  lcd.print("Agua:");

  lcd.setCursor(0, 3);
  lcd.print("Oleo:");

  
  unsigned long currentMillis = millis();  // Tempo atual

  // Verifica se o intervalo de 1 segundo passou
  if (currentMillis - previousMillis >= 500) {
    // Atualiza o tempo do último número gerado
    previousMillis = currentMillis;

    // Gera um número aleatório entre 0 e 100
    lcd.setCursor(6, 1);
    lcd.print(random(0, 90));


    lcd.setCursor(6, 3);
    lcd.print(random(0, 90));

    lcd.setCursor(15, 1);
    lcd.print(random(0, 90));


    lcd.setCursor(15, 3);
    lcd.print(random(0, 90));
    
  }

}

void changeMeasure(){

  if(unidadeMedidaIndex == 0){
    unidadeMedidaIndex = 1;
  }else if(unidadeMedidaIndex == 1){
    unidadeMedidaIndex = 0;
  }

  savePreferences("config.json", unidadeMedidaIndex);
  
  lcd.setCursor(1,1);
  lcd.print("Unidade de medicao" );

  lcd.setCursor(1,3);
  lcd.print("alterada para " + String(unidadeMedida[unidadeMedidaIndex]));

  delay(2000);
  lcd.clear();

  menuIndex = menuOld;

}

void boost(){
  if (currentBoostValue > maxBoostValue) {
    maxBoostValue = currentBoostValue;  // Atualiza o valor máximo
  }

  lcd.setCursor(2,0);
  lcd.print("Pressao de turbo");

  lcd.setCursor(0,1);
  lcd.print("Atual:");

  lcd.setCursor(14, 1);
  lcd.print("Max:");

  lcd.setCursor(4, 2);
  lcd.print(String(unidadeMedida[unidadeMedidaIndex]));

  unsigned long currentMillis = millis();  // Tempo atual

  // Verifica se o intervalo de 1 segundo passou
  if (currentMillis - previousMillis >= 500) {
    // Atualiza o tempo do último número gerado
    previousMillis = currentMillis;
    currentBoostValue = random(0, 90);
    displayProgress(currentBoostValue);
    // Gera um número aleatório entre 0 e 100
    lcd.setCursor(1, 2);
    lcd.print(currentBoostValue);


    lcd.setCursor(16, 2);
    lcd.print(maxBoostValue);
    
  }
}

void oilPressure(){
  if (currentBoostValue > maxBoostValue) {
    maxBoostValue = currentBoostValue;  // Atualiza o valor máximo
  }

  lcd.setCursor(2,0);
  lcd.print("Pressao de oleo");

  lcd.setCursor(0,1);
  lcd.print("Atual:");

  lcd.setCursor(14, 1);
  lcd.print("Max:");

  lcd.setCursor(4, 2);
  lcd.print(String(unidadeMedida[unidadeMedidaIndex]));

  unsigned long currentMillis = millis();  // Tempo atual

  // Verifica se o intervalo de 1 segundo passou
  if (currentMillis - previousMillis >= 500) {
    // Atualiza o tempo do último número gerado
    previousMillis = currentMillis;
    currentBoostValue = random(0, 90);
    displayProgress(currentBoostValue);
    // Gera um número aleatório entre 0 e 100
    lcd.setCursor(1, 2);
    lcd.print(currentBoostValue);


    lcd.setCursor(16, 2);
    lcd.print(maxBoostValue);
    
  }
}

void waterTemp(){
  if (currentBoostValue > maxBoostValue) {
    maxBoostValue = currentBoostValue;  // Atualiza o valor máximo
  }

  lcd.setCursor(1,0);
  lcd.print("Temperatura de agua");

  lcd.setCursor(0,1);
  lcd.print("Atual:");

  lcd.setCursor(14, 1);
  lcd.print("Max:");

  lcd.setCursor(4, 2);
  lcd.print("Graus");

  unsigned long currentMillis = millis();  // Tempo atual

  // Verifica se o intervalo de 1 segundo passou
  if (currentMillis - previousMillis >= 500) {
    // Atualiza o tempo do último número gerado
    previousMillis = currentMillis;
    currentBoostValue = random(0, 90);
    displayProgress(currentBoostValue);
    // Gera um número aleatório entre 0 e 100
    lcd.setCursor(1, 2);
    lcd.print(currentBoostValue);


    lcd.setCursor(16, 2);
    lcd.print(maxBoostValue);
    
  }
}

void oilTemp(){
  if (currentBoostValue > maxBoostValue) {
    maxBoostValue = currentBoostValue;  // Atualiza o valor máximo
  }

  lcd.setCursor(1,0);
  lcd.print("Temperatura de oleo");

  lcd.setCursor(0,1);
  lcd.print("Atual:");

  lcd.setCursor(14, 1);
  lcd.print("Max:");

  lcd.setCursor(4, 2);
  lcd.print("Graus");

  unsigned long currentMillis = millis();  // Tempo atual

  // Verifica se o intervalo de 1 segundo passou
  if (currentMillis - previousMillis >= 500) {
    // Atualiza o tempo do último número gerado
    previousMillis = currentMillis;
    currentBoostValue = random(0, 90);
    displayProgress(currentBoostValue);
    // Gera um número aleatório entre 0 e 100
    lcd.setCursor(1, 2);
    lcd.print(currentBoostValue);


    lcd.setCursor(16, 2);
    lcd.print(maxBoostValue);
    
  }
}

void displayProgress(int percent) {
  lcd.createChar(0, block);
  int totalBlocks = 16; // Número total de colunas na linha
  int blocksToFill = map(percent, 0, 100, 0, totalBlocks); // Mapeia porcentagem para blocos

  lcd.setCursor(0, 3); // Define o cursor para a segunda linha
  for (int i = 0; i < totalBlocks; i++) {
    if (i < blocksToFill) {
      lcd.write(byte(0)); // Desenha um bloco cheio
    } else {
      lcd.print(" "); // Deixa o espaço vazio
    }
  }
}

void savePreferences(const char* filename, int unidade) {
  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.println("Erro ao abrir arquivo para escrita");
    return;
  }

  StaticJsonDocument<128> doc;
  doc["unidadeMedicao"] = unidade;

  serializeJson(doc, file);
  file.close();
  Serial.println("Preferências salvas.");
}


int loadPreferences(const char* filename) {
  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("Erro ao abrir arquivo para leitura");
  }

  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print("Erro ao ler JSON: ");
    Serial.println(error.f_str());
  }

  int unidade = doc["unidadeMedicao"];
  file.close();

  return unidade; 
}