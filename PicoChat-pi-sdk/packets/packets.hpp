#include <pico/stdlib.h>
#include <cstring>
#include <cstdlib>
#include <RadioLib.h>

#define MAX_USER_NAME_LENGTH 8
#define MAX_MESSAGE_LENGTH 252 - MAX_USER_NAME_LENGTH // 256 byte max Lora packet size - 1 byte for id - 1 byte for length - 8 bytes for username - 1 byte for user name length

#define PACKET_TYPE_HEARTBEAT 0
#define PACKET_TYPE_ACK 1
#define PACKET_TYPE_MESSAGE 2

// Debug messages code
#define HEARTBEAT_REVEIVED 0b00000001
#define ACK_REVEIVED 0b00000010
#define MESSAGE_REVEIVED 0b00000011
#define UNKNOWN_PACKET_REVEIVED 0b00000100
#define CHANNEL_BUSY 0b00000101


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


struct chat_packet
{
public:
  uint8_t id; // 0 - heartbeat, 1 - ack, 2 - message
  uint8_t user_name_length;
  uint8_t message_length;
  char *user_name;
  char *message;

  /*
  @param id Packet ID
  */
  chat_packet(uint8_t id, const char *usr_name, const char *text)
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
  }

  chat_packet(uint8_t *buf)
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
  }

  uint16_t getPacketSize() const
  {
    return 3 + user_name_length + message_length;
  }

  // Funkcja serializująca strukturę do tablicy bajtów
  uint8_t *toByteArray() const
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
  }

  ~chat_packet()
  {
    free(user_name);
    free(message);
  }
};

struct chat_user
{
  char user_name[8];
  bool is_active = false;
  uint32_t last_seen = 0;
};