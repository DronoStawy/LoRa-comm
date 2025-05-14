/*
  TO DO
  - password as the only thing needed for creating chat and joining --------- []
  - messages many to many with retransmission ------------------------------- []
  - visible user status (obline/offline) ------------------------------------ []
  - read indication --------------------------------------------------------- []
  - CSMA - Carrier-Sense Multiple Access ------------------------------------ [] https://nws.sg/amalinda/lmac/
  - encrypted messages ------------------------------------------------------ []
  - ACK for receiving message and changing channel/spreading factor --------- []
  - FHSS - frequency hopping ------------------------------------------------ []
*/

#include <Arduino.h>
#include <RadioLib.h>
#include <PicoChat.hpp>
#include <aes.hpp>

#define MODE_PIN 7

#define MAX_USER_NAME_LENGTH 8
#define MAX_MESSAGE_LENGTH 252 - MAX_USER_NAME_LENGTH // 256 byte max Lora packet size - 1 byte for id - 1 byte for length - 8 bytes for username - 1 byte for user name length

#define ACK_TIMEOUT_MS 2000

uint32_t current_check_time = 0;
uint32_t last_check_time = 0;

bool waiting_for_ack = false;
bool waiting_for_packet = false;

enum chat_stage
{
  WAITING_FOR_PACKET,
  SENDING_PACKET,
  WAITING_FOR_ACK,
  SENDING_ACK,
};

chat_stage stage = SENDING_PACKET;

// SX1262 has the following connections:
// NSS pin:   10
// DIO1 pin:  2
// NRST pin:  3
// BUSY pin:  9
SX1262 radio = new Module(3, 20, 15, 2, SPI1, DEFAULT_SPI_SETTINGS);

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

  // Set output powet to 0dB
  radio.setOutputPower(0);

  delay(100);

  if (digitalRead(MODE_PIN) == LOW)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    transmitter = true;
    stage = SENDING_PACKET;
  }
  else
  {
    stage = WAITING_FOR_PACKET;
  }
}

void loop()
{
  // check if the previous operation finished
  if (transmitter)
  {
    switch (stage)
    {
    case SENDING_PACKET:
    {
      Serial.print(("[SX1262] Sending another packet ... \n"));

      // create a packet
      message_packet mp(1, "user_tx", "Very long message that is going to be sent over the radio. This is a test message to check if the packet is being sent correctly and if the receiver can handle it. Let's see how it goes!");
      // Serial.println(mp.id);
      // Serial.println(mp.user_name);
      // Serial.println(mp.message);

      uint8_t *message = mp.toByteArray();
      uint8_t message_size = mp.getPacketSize();
      // send the packet
      transmissionState = radio.transmit(message, message_size);

      if (transmissionState == RADIOLIB_ERR_NONE)
      {
        // packet was successfully sent
        Serial.println(F("transmission finished!"));
        operationDone = true;
      }
      else
      {
        Serial.print(F("failed, code "));
        Serial.println(transmissionState);
      }

      free(message);
      stage = WAITING_FOR_ACK;
      operationDone = false;
      break;
    }

    case WAITING_FOR_ACK:
    {
      bool timeout = false;

      if (!waiting_for_ack)
      {
        int state = radio.startReceive();
        waiting_for_ack = true;
        if (state == RADIOLIB_ERR_NONE)
        {
          Serial.println(F("Waiting for ACK..."));
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

      if (last_check_time == 0)
      {
        last_check_time = millis();
      }
      current_check_time = millis();

      if (current_check_time - last_check_time > ACK_TIMEOUT_MS)
      {
        timeout = true;
        last_check_time = 0;
      }

      if (!timeout)
      {
        if (operationDone)
        {
          // ACK was received
          Serial.println(F("Packet received!"));
          uint8_t buf[256];
          int state = radio.readData(buf, sizeof(buf));

          if (state == RADIOLIB_ERR_NONE)
          {
            message_packet mp(buf);
            if (mp.id == 0)
            {
              // ACK received
              Serial.print(F("ACK received from user: "));
              Serial.println(mp.user_name);
              stage = SENDING_PACKET;
              waiting_for_ack = false;
              operationDone = false;
              last_check_time = 0;
              break;
            }
            else
            {
              Serial.println(F("Not an ACK packet!"));
              stage = SENDING_PACKET;
              operationDone = false;
              last_check_time = 0;
              break;
            }
          }
        }
        else
        {
          break;
        }
      }
      else
      {
        // Timeout reached, stop waiting for ACK
        Serial.println(F("ACK timeout!"));
        stage = SENDING_PACKET;
        waiting_for_ack = false;
        operationDone = false;
        last_check_time = 0;
        break;
      }
      last_check_time = current_check_time;
    }
    }
    delay(10);
  }

  else
  {
    switch (stage)
    {
    case WAITING_FOR_PACKET:
    {
      if(!waiting_for_packet)
      {
        int state = radio.startReceive();
        if (state == RADIOLIB_ERR_NONE)
        {
          Serial.println(F("Waiting for packet..."));
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
        waiting_for_packet = true;
      }
      if (operationDone)
      {
        // packet was received
        Serial.println(F("Packet received!"));
        uint8_t buf[256];
        int state = radio.readData(buf, sizeof(buf));

        if (state == RADIOLIB_ERR_NONE)
        {
          message_packet mp(buf);
          Serial.println("=========================");
          Serial.println(mp.id);
          Serial.println(mp.user_name_length);
          Serial.println(mp.message_length);
          Serial.println(mp.user_name);
          Serial.println(mp.message);

          operationDone = false;
          stage = SENDING_ACK;
          waiting_for_packet = false;
          break;
        }
      }
      else{
        break;
      }
    }
    case SENDING_ACK:
    {
      Serial.print("Sending ACK...");
      // create a packet
      message_packet mp(0, "user_rx", "");
      uint8_t *message = mp.toByteArray();
      uint8_t message_size = mp.getPacketSize();
      // send the packet
      transmissionState = radio.transmit(message, message_size);
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
      free(message);
      operationDone = false;
      stage = WAITING_FOR_PACKET;
      break;
    }

      // packet was successfully received

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