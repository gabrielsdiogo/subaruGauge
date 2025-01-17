#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pino do botão
#define buttonPin D5

//pino do buzzer
#define buzzer D6
// Configurações da rede Wi-Fi (modo AP)
const char* ssid = "SubaruGauge";
const char* password = "lukebigcock";

//config do display
LiquidCrystal_I2C lcd(0x27, 20, 4); 


int menuIndex = 4;  // Índice atual do menu
bool inMenu = false;
unsigned long buttonPressStart = 0;
bool buttonHeld = false;
unsigned long previousMillis = 0;

//Sensores maximos e atuais
int maxBoostValue = 0;  // Variável para armazenar o valor máximo
int currentBoostValue = 0;

// Itens do menu
const char* menuItems[] = {
  "Temperatura Agua",
  "Temperatura Oleo",
  "Pressao Oleo",
  "Pressao Turbo",
  "Geral"
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

  lcd.setCursor(0,1);
  lcd.print("Hello Luke Big Cock");
  delay(2000);
  lcd.clear();
  mainScreen();
}

void loop() {
  ArduinoOTA.handle(); // Mantenha o OTA ativo
  monitoringBtn();
  if(!inMenu){
    switch (menuIndex){
      case 0://Temperatura Agua
        waterTemp();
        monitoringBtn();
        break;
      case 1://Temperatura Oleo
        oilTemp();
        monitoringBtn();
        break;
      case 2://Pressao Oleo
        oilPressure();
        monitoringBtn();
        break;
      case 3://Pressao Turbo
        boost();
        monitoringBtn();
        break;
      case 4:// geral
        mainScreen();
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
  lcd.print("BAR");

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
  lcd.print("Bar");

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
  lcd.print("Bar");

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