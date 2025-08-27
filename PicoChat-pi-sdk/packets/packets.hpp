#include <pico/stdlib.h>
#include <cstring>
#include <cstdlib>
#include <RadioLib.h>
#include "CRC.h"

#define MAX_USER_NAME_LENGTH 8
#define MAX_MESSAGE_LENGTH 249 // 256 byte max Lora packet size - 1 byte for id - 1 byte for length - 8 bytes for username - 1 byte for user name length

#define PACKET_TYPE_HEARTBEAT 0
#define PACKET_TYPE_ACK 1
#define PACKET_TYPE_MESSAGE 2

// Debug messages code
#define HEARTBEAT_REVEIVED 111
#define ACK_REVEIVED 112
#define MESSAGE_REVEIVED 113
#define UNKNOWN_PACKET_REVEIVED 114
#define CHANNEL_BUSY 115

// COM8 i COM9 - urządzenia LoRa

// Default message packet structure
/*
 @brief Structure representing a PicoChat packet.
*/

enum chat_stage
{
  /*
   */
  IDLE,               // Waiting for heartbeats from other users and periodically sending own one
  SENDING_HEARTBEAT,  // Sending heartbeat to other users
  WAITING_FOR_PACKET, //
  SENDING_PACKET,
  WAITING_FOR_ACK,
  SENDING_ACK,
};

class ChatPacket
{
public:
  /*uint8_t id; // 0 - heartbeat, 1 - ack, 2 - message
  uint8_t user_name_length;
  uint8_t message_length;
  char *user_name;
  char *message;*/

  uint8_t type; // 
  uint8_t id;
  uint8_t number;
  uint8_t message_length;
  char *message;
  uint8_t control_sum; // 1 byte checksum for the packet
  uint8_t split_message; // 0 - not split, 1 - split message

  /*
  @param id Packet ID
  */
  /*ChatPacket(uint8_t id, const char *usr_name, const char *text)
  {
    this->id = id;
    this->user_name_length = (strlen(usr_name) > MAX_USER_NAME_LENGTH) ? MAX_USER_NAME_LENGTH : strlen(usr_name);
    if (text != NULL)
    {
      this->message_length = (strlen(text) > MAX_MESSAGE_LENGTH) ? MAX_MESSAGE_LENGTH : strlen(text);
    }
    else
    {
      this->message_length = 0;
    }
    user_name = (char *)malloc(user_name_length + 1);
    message = (char *)malloc(message_length + 1);
    memcpy(user_name, usr_name, user_name_length);
    memcpy(message, text, message_length);
  }*/

  ChatPacket(uint8_t type, uint8_t id, uint8_t number, const char *msg)
  {
    this->type = type;
    this->id = id;
    this->number = number;
    this->split_message = 0; // Default to not split

    if (msg != NULL){
    this->message_length = (strlen(msg) > MAX_MESSAGE_LENGTH) ? MAX_MESSAGE_LENGTH : strlen(msg);
      if (strlen(msg) > MAX_MESSAGE_LENGTH) {
        this->split_message = 1; // Message is split
        this->message_length = MAX_MESSAGE_LENGTH; // Limit to max message length
      }
      else {
        this->split_message = 0; // Message is not split
        this->message_length = strlen(msg);
      }
    }
    else {
      this->message_length = 0;
    }
    message = (char *)malloc(message_length + 1);
    memcpy(message, msg, message_length);
    printf("Message from packet: %s\n", message);

    this->control_sum = CyclicRedundancyCode(this->type, this->id, this->number, msg, this->message_length, this->split_message);
  }

  /*ChatPacket(uint8_t *buf)
  {
    this->id = buf[0];
    this->user_name_length = buf[1];
    this->message_length = buf[2];
    user_name = (char *)malloc(user_name_length + 1);
    if (message_length > 0)
    {
      message = (char *)malloc(message_length + 1);
    }
    else
    {
      message = NULL;
    }
    memcpy(user_name, &buf[3], user_name_length);
    memcpy(message, &buf[3 + user_name_length], message_length);
    user_name[user_name_length] = '\0';
    message[message_length] = '\0';
  }*/

  ChatPacket(uint8_t *buf)
  {
    this->type = buf[0];
    this->id = buf[1];
    this->number = buf[2];
    this->message_length = buf[3];
    this->split_message = buf[4];
    this->control_sum = buf[5];
    if (message_length > 0)
    {
      message = (char *)malloc(message_length + 1);
    }
    else
    {
      message = NULL;
    }
    memcpy(message, &buf[6], message_length);
    message[message_length] = '\0';
  }

  uint16_t getPacketSize() const
  {
    //return 3 + user_name_length + message_length;
    return 6 + message_length;
  }

  // Funkcja serializująca strukturę do tablicy bajtów
  /*uint8_t *toByteArray() const
  {
    uint16_t total_size = getPacketSize();
    uint8_t *buffer = (uint8_t *)malloc(total_size);

    if (buffer != NULL)
    {
      // Zapisz nagłówek
      buffer[0] = id;
      buffer[1] = user_name_length;
      buffer[2] = message_length;
      memcpy(&buffer[3], user_name, user_name_length);
      memcpy(&buffer[3 + user_name_length], message, message_length);
    }
    return buffer;
  }*/

  uint8_t *toByteArray() const
  {
    uint16_t total_size = getPacketSize();
    uint8_t *buffer = (uint8_t *)malloc(total_size);

    if (buffer != NULL)
    {
      // Zapisz nagłówek
      buffer[0] = type;
      buffer[1] = id;
      buffer[2] = number;
      buffer[3] = message_length;
      buffer[4] = split_message;
      buffer[5] = control_sum;
      memcpy(&buffer[6], message, message_length);
    }
    return buffer;
  }

  uint8_t *toByteArrayNoCRC(uint8_t type, uint8_t id, uint8_t number, const char *msg, uint8_t length, uint8_t split) const
  {
    uint16_t total_size = getPacketSize();
    uint8_t *buffer = (uint8_t *)malloc(total_size);

    if (buffer != NULL)
    {
      // Zapisz nagłówek
      buffer[0] = type;
      buffer[1] = id;
      buffer[2] = number;
      buffer[3] = length;
      buffer[4] = split;
      memcpy(&buffer[5], msg, length);
    }
    return buffer;
  }

  uint8_t CyclicRedundancyCode(uint8_t type, uint8_t id, uint8_t number, const char *msg, uint8_t length, uint8_t split) const
  {
    uint8_t* array = toByteArrayNoCRC(type, id, number, msg, length, split);

    uint8_t crc = CRC::Calculate<uint8_t, 8>(array, getPacketSize() - 1, CRC::CRC_8_WCDMA()); //WCDMA to rodzaj CRC 8-bitowego, dostępne rodzaje CRC są w CRC.h
    // https://en.wikipedia.org/wiki/Cyclic_redundancy_check#Polynomial_representations - tutaj można znaleźć różne rodzaje CRC
    free(array); // Free the allocated memory for the byte array
    return crc;
  }

  /*~ChatPacket() 
  {
    free(user_name);
    free(message);
  }*/

  ~ChatPacket()
  {
    free(message);
  }
};

struct chat_user
{
  char user_name[8];
  bool is_active = false;
  uint32_t last_seen = 0;
};

class DebugSerialMessages
{
public:
  bool enabled = false;

  void serialHeartbeatReceived(uint8_t id)
  {
    if (enabled)
    {
      // printf(char(HEARTBEAT_REVEIVED) + user_name);
      // printf("Heartbeat received from: %s\n", user_name);
      printf("Heartbeat receieved, ID: %i\n", id);
    }
  }

  void serialACKReceived(uint8_t id)
  {
    if (enabled)
    {
      // printf(char(ACK_REVEIVED) + user_name);
      printf("ACK received, ID: %i\n", id);
    }
  }
};

// class SerialMessages{

// }
