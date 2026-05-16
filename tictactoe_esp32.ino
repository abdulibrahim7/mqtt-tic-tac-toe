#include <WiFi.h>
#include <PubSubClient.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

const char* WIFI_SSID     = "REDACTED_WIFI_SSID";
const char* WIFI_PASSWORD = "REDACTED_WIFI_PASSWORD";
const char* MQTT_BROKER   = "diazcs2600.duckdns.org";
const int   MQTT_PORT     = 8883;

WiFiClient   espClient;
PubSubClient client(espClient);

#define SDA 13
#define SCL 14
LiquidCrystal_I2C lcd(0x27, 16, 2);

char keys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[4] = {27, 26, 25, 33};
byte colPins[4] = {15, 21, 22, 23};
Keypad myKeypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);


char currentPlayer = 'X';   // global var

bool i2CAddrTest(uint8_t addr) {
  Wire.begin();
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    return true;
  }
  return false;
}


void displayBoard(String message) {
  // need to fix scroll !!!!
  String slots[9];
  int slotIndex = 0;
  String current = "";

  for (int i = 0; i <= (int)message.length(); i++) {
    if (i == (int)message.length() || message[i] == ' ') {
      if (current.length() > 0) {
        slots[slotIndex++] = current;
        current = "";
      }
    } else {
      current += message[i];
    }
  }

  // Show rows 1 and 2 first
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Row 1: " + slots[0] + "|" + slots[1] + "|" + slots[2]);  // row 1
  lcd.setCursor(0, 1);
  lcd.print("Row 2: " + slots[3] + "|" + slots[4] + "|" + slots[5]);  // row 2

  delay(3000);  // let player read rows 1 and 2
  //*** what if i say "row 1:"? 
  //FIX SCROLL !!!
  // Scroll to show row 3
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Row 3: " + slots[6] + "|" + slots[7] + "|" + slots[8]);  // row 3
  delay(3000);
  lcd.setCursor(0, 1);
  lcd.print("Enter 1-9:");       // prompt player for input
}

void displayStatus(String message) {
  if (message == "X_TURN") {
    currentPlayer = 'X';
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Your turn: X");
  } else if (message == "O_TURN") {
    currentPlayer = 'O';
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Your turn: O");
  } else if (message == "X_WINS") {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  X Wins!!  ");
    lcd.setCursor(0, 1);
    lcd.print("Game Over");
  } else if (message == "O_WINS") {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  O Wins!!  ");
    lcd.setCursor(0, 1);
    lcd.print("Game Over");
  } else if (message == "DRAW") {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Its a Draw  ");
    lcd.setCursor(0, 1);
    lcd.print("Game Over");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // Route to the correct handler based on which topic sent the message
  if (String(topic) == "tictactoe/board") {
    displayBoard(message);
  } else if (String(topic) == "tictactoe/status") {
    displayStatus(message);
  }
}

void connectWiFi() {
  Serial.println("Starting WiFi connection...");
  Serial.print("Connecting to SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;

    if (attempts % 20 == 0) {
      Serial.print("\nStatus code: ");
      Serial.println(WiFi.status());
    }

    if (attempts > 60) {
      Serial.println("\nFailed to connect. Check SSID/password or WiFi band.");
      return;
    }
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    if (client.connect("ESP32_TicTacToe")) {
      Serial.println("connected!");
      
      client.subscribe("tictactoe/board");
      client.subscribe("tictactoe/status");
      Serial.println("Subscribed to tictactoe/board and tictactoe/status");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" — retrying in 3 seconds");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ESP32 is ready!");

  // Connect to WiFi and MQTT broker
  connectWiFi();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback);
  connectMQTT();

  // Initialize LCD
  Wire.begin(SDA, SCL);
  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Waiting for");
  lcd.setCursor(0, 1);
  lcd.print("game to start...");
}

void loop() {
  // Reconnect if connection dropped
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();  // watch for incoming MQTT messages

  // Read keypad 
  char input = myKeypad.getKey();

  // Only send a move if it's valid
  if (input >= '1' && input <= '9') {
    // Format: "X:5" means player X chose position 5
    char message[4];
    snprintf(message, sizeof(message), "%c:%c", currentPlayer, input);

    client.publish("tictactoe/move", message);
    Serial.print("Sent move: ");
    Serial.println(message);

    lcd.setCursor(0, 1);
    lcd.print("Sent: ");
    lcd.print(message);
    lcd.print("          ");
  }
}
