#include <pico/stdlib.h>
#include <cstring>
#include <cstdlib>
#include <RadioLib.h>
#include "CRC.h"

//#define MAX_MESSAGE_LENGTH 249 // 256 byte max Lora packet size - 1 byte for id - 1 byte for length - 8 bytes for username - 1 byte for user name length
#define PAYLOAD_SIZE 8
#define PACKET_SIZE (2 + PAYLOAD_SIZE) // 1 byte for type + payload
#define PACKET_TYPE_ACK 0
#define PACKET_TYPE_MESSAGE 1

// COM8 i COM9 - urządzenia LoRa

// Default message packet structure
/*
 @brief Structure representing a PicoChat packet.
*/

enum transmission_stage
{
  /*
   */
  IDLE,               // Waiting for heartbeats from other users and periodically sending own one
  WAITING_FOR_PACKET, //
  SENDING_PACKET,
  WAITING_FOR_ACK,
  SENDING_ACK,
};

class Packet
{
public:
  uint8_t type; // 
  uint8_t length;
  uint8_t payload[PAYLOAD_SIZE];
  
  // Statyczny bufor dla toByteArray() - eliminuje wyciek pamięci
  static uint8_t tx_buffer[PACKET_SIZE];

  /**
   * @brief Konstruktor pakietu z danymi
   * @param type Typ pakietu (ACK lub MESSAGE)
   * @param length Rzeczywista długość danych w payload
   * @param payload Wskaźnik do danych (kopiuje tylko 'length' bajtów)
   * @note Optymalizacja: kopiuje tylko length bajtów zamiast całego bufora
   */
  Packet(uint8_t type, uint8_t length, const uint8_t* payload)
  {
    this->type = type;
    this->length = length;
    // Kopiuj tylko rzeczywistą ilość danych (length), nie cały bufor
    if (length > 0 && length <= PAYLOAD_SIZE) {
      memcpy(this->payload, payload, length);
    }
  }

  /**
   * @brief Konstruktor pakietu z odebranego bufora
   * @param buf Bufor zawierający dane pakietu
   * @note Optymalizacja: kopiuje tylko length bajtów na podstawie buf[1]
   */
  Packet(uint8_t *buf)
  {
    this->type = buf[0];
    this->length = buf[1];
    // Kopiuj tylko rzeczywistą ilość danych zapisaną w length
    if (this->length > 0 && this->length <= PAYLOAD_SIZE) {
      memcpy(payload, &buf[2], this->length);
    }
  }

  /**
   * @brief Konwertuje pakiet do tablicy bajtów
   * @return Wskaźnik do statycznego bufora tx_buffer (nie wymaga free!)
   * @note Używa statycznego bufora - nie trzeba zwalniać pamięci
   * @note Optymalizacja: kopiuje tylko length bajtów zamiast całego PAYLOAD_SIZE
   */
  uint8_t *toByteArray()
  {
    tx_buffer[0] = type;
    tx_buffer[1] = length;
    // Kopiuj tylko rzeczywistą ilość danych (length), nie cały bufor
    if (length > 0 && length <= PAYLOAD_SIZE) {
      memcpy(&tx_buffer[2], payload, length);
    }
    return tx_buffer;
  }
};

//   uint8_t *toByteArrayNoCRC(uint8_t type, uint8_t id, uint8_t number, const char *msg, uint8_t length, uint8_t split) const
//   {
//     uint16_t total_size = getPacketSize();
//     uint8_t *buffer = (uint8_t *)malloc(total_size);

//     if (buffer != NULL)
//     {
//       // Zapisz nagłówek
//       buffer[0] = type;
//       buffer[1] = id;
//       buffer[2] = number;
//       buffer[3] = length;
//       buffer[4] = split;
//       memcpy(&buffer[5], msg, length);
//     }
//     return buffer;
//   }

//   uint8_t CyclicRedundancyCode(uint8_t type, uint8_t id, uint8_t number, const char *msg, uint8_t length, uint8_t split) const
//   {
//     uint8_t* array = toByteArrayNoCRC(type, id, number, msg, length, split);

//     uint8_t crc = CRC::Calculate<uint8_t, 8>(array, getPacketSize() - 1, CRC::CRC_8_WCDMA()); //WCDMA to rodzaj CRC 8-bitowego, dostępne rodzaje CRC są w CRC.h
//     // https://en.wikipedia.org/wiki/Cyclic_redundancy_check#Polynomial_representations - tutaj można znaleźć różne rodzaje CRC
//     free(array); // Free the allocated memory for the byte array
//     return crc;
//   }

//   /*~ChatPacket() 
//   {
//     free(user_name);
//     free(message);
//   }*/

//   ~ChatPacket()
//   {
//     free(message);
//   }
// };


// class DebugSerialMessages
// {
// public:
//   bool enabled = false;

//   void serialHeartbeatReceived(uint8_t id)
//   {
//     if (enabled)
//     {
//       // printf(char(HEARTBEAT_REVEIVED) + user_name);
//       // printf("Heartbeat received from: %s\n", user_name);
//       printf("Heartbeat receieved, ID: %i\n", id);
//     }
//   }

//   void serialACKReceived(uint8_t id)
//   {
//     if (enabled)
//     {
//       // printf(char(ACK_REVEIVED) + user_name);
//       printf("ACK received, ID: %i\n", id);
//     }
//   }
// };

// class SerialMessages{

// 

// Definicja statycznego bufora transmisyjnego dla klasy Packet
uint8_t Packet::tx_buffer[PACKET_SIZE];
