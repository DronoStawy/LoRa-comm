#include <pico/stdlib.h>
#include <cstring>
#include <cstdlib>
#include <RadioLib.h>
#include "CRC.h"

// ============================================================
// PERFORMANCE MODE CONFIGURATION
// ============================================================
// Uncomment ONE of the following modes:

// MODE 1: RELIABLE (100% success rate, 0.16 KB/s, 25ms recommended delay)
// Best for: Production, critical data, zero data loss required
#define MODE_RELIABLE

// MODE 2: HIGH_PERFORMANCE (98% success rate, 0.17 KB/s, 23ms recommended delay)
// Best for: Maximum throughput, 2% data loss acceptable
// #define MODE_HIGH_PERFORMANCE

// ============================================================

//#define MAX_MESSAGE_LENGTH 249 // 256 byte max Lora packet size - 1 byte for id - 1 byte for length - 8 bytes for username - 1 byte for user name length
#define PAYLOAD_SIZE 8  // Works reliably at 100% success rate
#define ACK_PAYLOAD_SIZE 3  // Small ACK packets for faster transmission
#define PACKET_SIZE (2 + PAYLOAD_SIZE) // 1 byte for type + payload - ALL packets same size
#define ACK_PACKET_SIZE (2 + ACK_PAYLOAD_SIZE) // Smaller size for ACK packets
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


  Packet(uint8_t type, uint8_t length, const uint8_t* payload)
  {
    this->type = type;
    this->length = length;
    memcpy(this->payload, payload, length); // Copy only actual data length
    memset(this->payload + length, 0, PAYLOAD_SIZE - length); // Zero the rest
  }

  Packet(uint8_t *buf)
  {
    this->type = buf[0];
    this->length = buf[1];
    memcpy(payload, &buf[2], PAYLOAD_SIZE);
  }


  uint8_t *toByteArray()
  {
    uint8_t *buffer = (uint8_t *)malloc(PACKET_SIZE);
    if (buffer != NULL)
    {
      buffer[0] = type;
      buffer[1] = length;
      memcpy(&buffer[2], payload, PAYLOAD_SIZE);
    }
    return buffer;
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
