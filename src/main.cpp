/*
  RadioLib SX126x Ping-Pong Example

  This example is intended to run on two SX126x radios,
  and send packets between the two.

  For default module settings, see the wiki page
  https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx126x---lora-modem

  For full API reference, see the GitHub Pages
  https://jgromes.github.io/RadioLib/
*/

/*
  TO DO
  - password as the only thing needed for creating chat and joining --------- []
  - messages many to many with retransmission ------------------------------- []
  - visible user status (obline/offline) ------------------------------------ []
  - CSMA - Carrier-Sense Multiple Access ------------------------------------ [] https://nws.sg/amalinda/lmac/
  - encrypted messages ------------------------------------------------------ []
  - ACK for receiving message and changing channel/spreading factor --------- []
  - FHSS - frequency hopping ------------------------------------------------ []
*/

// include the library
#include <Arduino.h>
#include <RadioLib.h>
#include <aes.hpp>
#include <vector>

// uncomment the following only on one
// of the nodes to initiate the pings
#define MODE_PIN 7

#define MAX_USER_NAME_LENGTH 8
#define MAX_MESSAGE_LENGTH 252 - MAX_USER_NAME_LENGTH // 256 byte max Lora packet size - 1 byte for id - 1 byte for length - 8 bytes for username - 1 byte for user name length

struct message_packet
{
  uint8_t id; // 0 - ack message
  uint8_t user_name_length;
  char* user_name[MAX_USER_NAME_LENGTH];
  uint8_t message_length;
  char message[MAX_MESSAGE_LENGTH];
};

message_packet mp;

// SX1262 has the following connections:
// NSS pin:   10
// DIO1 pin:  2
// NRST pin:  3
// BUSY pin:  9
SX1262 radio = new Module(3, 20, 15, 2, SPI1, DEFAULT_SPI_SETTINGS);

// or detect the pinout automatically using RadioBoards
// https://github.com/radiolib-org/RadioBoards
/*
#define RADIO_BOARD_AUTO
#include <RadioBoards.h>
Radio radio = new RadioModule();
*/

// save transmission states between loops
int transmissionState = RADIOLIB_ERR_NONE;

// flag to indicate transmission or reception state
bool transmitFlag = false;
bool transmitter = false;

// flag to indicate that a packet was sent or received
volatile bool operationDone = false;

// this function is called when a complete packet
// is transmitted or received by the module
// IMPORTANT: this function MUST be 'void' type
//            and MUST NOT have any arguments!
void setFlag(void)
{
  // we sent or received a packet, set the flag
  operationDone = true;
}

void setup()
{
  SPI1.setSCK(10);
  SPI1.setTX(11);
  SPI1.setRX(12);
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
  SPI1.begin();

  // Pin setting mode of the device
  pinMode(MODE_PIN, INPUT_PULLUP);

  // LED
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);
  delay(3000);

  // initialize SX1262 with default settings
  Serial.print(F("[SX1262] Initializing ... "));
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE)
  {
    Serial.println(F("success!"));
  }
  else
  {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true)
    {
      delay(10);
    }
  }

  // set the function that will be called
  // when new packet is received
  radio.setDio1Action(setFlag);

  // Set output powet to 3dB
  radio.setOutputPower(3);

  delay(100);

  if (digitalRead(MODE_PIN) == LOW)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    transmitter = true;
    operationDone = true;
  }

  else
  {
    // start listening for LoRa packets on this node
    Serial.print(F("[SX1262] Starting to listen ... "));
    state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE)
    {
      Serial.println(F("success!"));
    }
    else
    {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true)
      {
        delay(10);
      }
    }
  }
}

void loop()
{
  // check if the previous operation finished
  if (operationDone)
  {
    // reset flag
    operationDone = false;

    if (transmitter)
    {
      Serial.print(F("[SX1262] Sending another packet ... "));
      
      mp.id = 1; // 1 - Text message 
      char user_name[] = "user1";
      char text[] = "Hello world!";

      mp.user_name_length = strlen(user_name);
      mp.message_length = strlen(text);
      memcpy(mp.user_name, user_name, mp.user_name_length);
      memcpy(mp.message, text, mp.message_length);

      uint8_t *message = (uint8_t *)&mp;
  
      transmissionState = radio.transmit(message, sizeof(message_packet));
      transmitFlag = true;

      if (transmissionState == RADIOLIB_ERR_NONE)
      {
        // packet was successfully sent
        Serial.println(F("transmission finished!"));
      }
      else
      {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);
      }
      delay(5000);
    }
    
    else
    {
      // the previous operation was reception
      // print data and send another packet
      uint8_t buf[sizeof(message_packet)];
      int state = radio.readData(buf, 0);

      if (state == RADIOLIB_ERR_NONE)
      {

        mp.id = buf[0];
        mp.user_name_length = buf[1];
        memcpy(mp.user_name, &buf[2], mp.user_name_length);
        mp.message_length = buf[10];
        memcpy(mp.message, &buf[11], mp.message_length);

        // packet was successfully received
        Serial.println("Packet received!");
        Serial.print("Packet ID: ");
        Serial.print(mp.id);
        Serial.print(" User name length: ");
        Serial.print(mp.user_name_length);
        Serial.print(" User name: ");
        Serial.print(mp.user_name);
        Serial.print(" Message length: ");
        Serial.print(mp.message_length);
        Serial.print(" Message: ");
        Serial.print(mp.message);
        Serial.println();

        // print RSSI (Received Signal Strength Indicator)
        // Serial.print(F("[SX1262] RSSI:\t\t"));
        // Serial.print(radio.getRSSI());
        // Serial.println(F(" dBm"));

        // // print SNR (Signal-to-Noise Ratio)
        // Serial.print(F("[SX1262] SNR:\t\t"));
        // Serial.print(radio.getSNR());
        // Serial.println(F(" dB"));
      }
      delay(10);

      // send another one
    }
  }
}