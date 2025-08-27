/*
  TO DO
  - password as the only thing needed for creating chat and joining --------- []
  - messages many to many with retransmission ------------------------------- []
  - visible user status (obline/offline) ------------------------------------ []
  - read indication --------------------------------------------------------- []
  - CSMA - Carrier-Sense Multiple Access ------------------------------------ [x] https://nws.sg/amalinda/lmac/
  - encrypted messages ------------------------------------------------------ []
  - ACK for receiving message and changing channel/spreading factor --------- []
  - FHSS - frequency hopping ------------------------------------------------ []
*/

#include <pico/stdlib.h>
#include <pico/rand.h>
#include "hardware/watchdog.h"
#include <RadioLib.h>
#include "hal/RPiPico/PicoHal.h"
#include "packets/packets.hpp"
#include <aes.hpp>
#include <chrono>
#include <thread>
#include <string>

// Pico - z kabelkiem
// Chat - bez kabelka

#define LORA_SCK 14
#define LORA_MISO 24
#define LORA_MOSI 15
#define LORA_CS 13
#define LORA_RST 23
#define LORA_DIO1 16
#define LORA_BUSY 18
#define LORA_ANT_SW 17

#define SERIAL_PORT uart0
// Set which module is a master
#define USER_PIN 7

#define MAX_CHAT_USERS 4

// Set ACK timeout to 2 seconds
#define ACK_TIMEOUT_MS 800
#define USER_STATUS_TIMEOUT_MS 5000
#define HEARTBEAT_PERIOD_MS 3000

// CSMA timing parameters
#define CSMA_BACKOFF_MIN_MS 50
#define CSMA_BACKOFF_MAX_MS 300

using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono; // nanoseconds, system_clock, seconds

// ACK timing variables
uint32_t curr_ack_check_time = 0;
uint32_t prev_ack_check_time = 0;

// Heartbeat timing variables
uint32_t curr_heartbeat_time = 0;
uint32_t prev_heartbeat_time = 0;

// CSMA timing variables
uint32_t curr_csma_time = 0;
uint32_t prev_csma_time = 0;
int backoff_time = 0;
// create a new instance of the HAL class
PicoHal *hal = new PicoHal(spi1, LORA_MISO, LORA_MOSI, LORA_SCK);
SX1262 radio = new Module(hal, LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

volatile bool interrupt_flag = false;
bool idle_listen_flag = false;
bool waiting_for_ack_flag = false;
bool sending_packet_flag = false;
bool sending_ack_flag = false;

chat_user users[MAX_CHAT_USERS];

bool new_serial_data = false;
const uint16_t char_buf_size = 1200;
char serial_received_chars[char_buf_size];

DebugSerialMessages deb_serial;

// Our user variables
char user_name[MAX_USER_NAME_LENGTH];

// Packet variables
uint8_t id = 0; // message ID
uint8_t packet_number = 0; // message number
bool split_message = false; // flag to indicate if the message is split
bool retransmission_flag = false; // flag to indicate if the message is being retransmitted
uint8_t crc_calculated = 0;

void intFlag()
{
  interrupt_flag = true;
}

// Function prototypes
int radioInit();
int checkState(int state);
int checkUserList(char *usr_name);
void updateUserStatus(char *usr_name);
void updateUserList();
int randomRange(int min, int max);
bool doCSMA();
void updateHeartbeatTimer();
void updateAckTimer();
void readSerialData();
void ledOn();
void ledOff();
bool checkAckReceived();
void parseSerialData();

int main()
{
  stdio_init_all();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  gpio_init(USER_PIN);
  gpio_set_dir(USER_PIN, GPIO_IN);
  gpio_pull_up(USER_PIN);

  uart_set_hw_flow(SERIAL_PORT, false, false);

  // Enable debug messages
  deb_serial.enabled = true;

  // Initialize timers
  prev_ack_check_time = to_ms_since_boot(get_absolute_time());
  prev_heartbeat_time = to_ms_since_boot(get_absolute_time());

  radioInit();
  chat_stage stage = IDLE;
  sleep_ms(randomRange(100, 500)); // Random delay to avoid collisions at startup

  if (gpio_get(USER_PIN) == 0)
  {
    strcpy(user_name, "Pico");
  }
  else
  {
    strcpy(user_name, "Chat");
  }

  for (;;)
  {
    // Check timers
    curr_heartbeat_time = to_ms_since_boot(get_absolute_time());
    
    if (curr_heartbeat_time - prev_heartbeat_time > 1000 && split_message)
    {
      stage = SENDING_PACKET;
    }
    switch (stage)
    {
    case IDLE:
    {
      ledOff();
      readSerialData(); // Read serial data if available
      if (new_serial_data)
      {
        stage = SENDING_PACKET; // Parse the serial data for command
        break;
      }
      if (checkAckReceived())
      {
        retransmission_flag = false; // Reset retransmission flag if ACK received
        stage = IDLE;
      }
      else
      {
        stage = SENDING_PACKET;
        retransmission_flag = true; // Set retransmission flag if ACK not received
        prev_heartbeat_time = to_ms_since_boot(get_absolute_time());
        break;
      }
      if (!idle_listen_flag)
      {
        int state = radio.startReceive();
        checkState(state);
        idle_listen_flag = true;
      }
      if (interrupt_flag)
      {
        uint8_t buf[256];
        int state = radio.readData(buf, sizeof(buf));
        interrupt_flag = false;
        if (state == RADIOLIB_ERR_NONE)
        {
          ChatPacket packet(buf);
          if (packet.type == PACKET_TYPE_HEARTBEAT)
          {
            // printf("Heartbeat received from %s\n", packet.user_name);
            deb_serial.serialHeartbeatReceived(packet.id);
            //updateUserStatus(packet.user_name);
            stage = IDLE;
            break;
          }

          else if (packet.type == PACKET_TYPE_MESSAGE)
          {
            ledOn();
            printf("Message: %s\n", packet.message);
            printf("Message received from ID: %i, Number: %i\n", packet.id, packet.number);
            printf("Message length: %i\n", packet.message_length);
            // Update status of the user who sent the message
            //updateUserStatus(packet.user_name);
            //interrupt_flag = false;
            id = packet.id; // Update the ID to the one from the received packet
            packet_number = packet.number; // Update the packet number to the one from the received packet
            crc_calculated = packet.CyclicRedundancyCode(packet.type, packet.id, packet.number, packet.message, packet.message_length, packet.split_message);
            printf("CRC calculated: %i\n", crc_calculated);
            printf("CRC from packet: %i\n", packet.control_sum);
            if (crc_calculated == packet.control_sum){
              stage = SENDING_ACK;
              break;
            }
            else {
              stage = IDLE; //gdy crc się nie zgadza, to po prostu nie wysyłamy potwierdzenia odbioru informacji
              break;
            }
          }
          else if (packet.type == PACKET_TYPE_ACK)
          {
            //printf("ACK received from %s\n", packet.user_name);
            //deb_serial.serialACKReceived(packet.id);
            printf("ACK received, ID: %i, Number: %i\n", packet.id, packet.number);
            // Update status of the user who sent the ACK
            //updateUserStatus(packet.user_name);
            // interrupt_flag = false;
            waiting_for_ack_flag = false; // Reset the waiting for ACK flag
            stage = IDLE;
            break;
          }
          else
          {
            printf("Unknown packet type\n");
            // interrupt_flag = false;
            stage = IDLE;
            break;
          }
        }
      }
    }
    break;
    case SENDING_HEARTBEAT:
    {
      if (doCSMA())
      {
        ledOn();
        id = (id != 255) ? id + 1 : 0; // Increment ID, reset to 0 if it reaches 255
        ChatPacket packet(PACKET_TYPE_HEARTBEAT, id, packet_number, NULL); 
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        int state = radio.transmit(packet.toByteArray(), packet.getPacketSize());
        checkState(state);
        prev_heartbeat_time = curr_heartbeat_time;
        idle_listen_flag = false; // Start listening for new packets again
        interrupt_flag = false;   // Reset the interrupt flag
        stage = IDLE;
        break;
      }
      else
      {
        printf("Channel busy, waiting...\n");
        stage = SENDING_HEARTBEAT; // Go back to IDLE state if channel is busy
        break;
      }
    }
    case SENDING_ACK:
    {
      if (doCSMA())
      {
        //id = (id != 255) ? id + 1 : 0; // Increment ID, reset to 0 if it reaches 255
        ChatPacket packet(PACKET_TYPE_ACK, id, packet_number, NULL);
        int state = radio.transmit(packet.toByteArray(), packet.getPacketSize()); // do odkomentowania
        checkState(state); //do odkomentowania
        updateHeartbeatTimer();
        idle_listen_flag = false; // Start listening for new packets again
        interrupt_flag = false;   // Reset the interrupt flag
        stage = IDLE;
        break;
      }
      else
      {
        printf("Channel busy, waiting...\n");
        stage = SENDING_ACK; // Go back to IDLE state if channel is busy
        printf("ACK not sent, waiting for channel to be free...\n");
        break;
      }
    }
    case SENDING_PACKET:
    {
      if (doCSMA())
      {
        ledOn();
        if (split_message){
          if (!retransmission_flag)
          {
            int i = 0;
            while (true)
            {
              if (i == char_buf_size || serial_received_chars[i] == '\0')
              {
                break; // Exit the loop if we reach the end of the buffer or a null character
              }
              i++;
            }
            printf("Buffer size: %i\n", i);

            for (int j = 0; j < char_buf_size; j++){
              if (j < (i - MAX_MESSAGE_LENGTH)){
                serial_received_chars[j] = serial_received_chars[j + MAX_MESSAGE_LENGTH];
              }
              else
              {
                serial_received_chars[j] = '\0'; // Clear the buffer
              }
              //else{
                //serial_received_chars[j] = NULL; // Clear the buffer
              //}
            }
          }
          if (serial_received_chars[0] != '\n' && serial_received_chars[0] != '\0' && serial_received_chars[0] != '\r'){
            if (!retransmission_flag) packet_number++;
            ChatPacket packet(PACKET_TYPE_MESSAGE, id, packet_number, serial_received_chars);
            int state = radio.transmit(packet.toByteArray(), packet.getPacketSize());
            checkState(state);
            printf("Packet sent, ID: %i, Number: %i\n", packet.id, packet.number);
            updateAckTimer();         // Start waiting for ACK after sending the packet
            updateHeartbeatTimer();   // Update the heartbeat timer after sending a message

            
            printf("Split packet: %p\n", packet.split_message);
            split_message = (packet.split_message == 1) ? true : false; // Check if the message is split
          }
          else {
            split_message = false; // Reset the split message flag if the buffer is empty
          }
        }
        else{
          if (!retransmission_flag){
            id = (id != 255) ? id + 1 : 0; // Increment ID, reset to 0 if it reaches 255
            packet_number = 0; // Reset packet number for new message
          }
          ChatPacket packet(PACKET_TYPE_MESSAGE, id, packet_number, serial_received_chars);
          int state = radio.transmit(packet.toByteArray(), packet.getPacketSize());
          checkState(state);
          printf("Packet sent, ID: %i, Number: %i\n", packet.id, packet.number);
          updateAckTimer();         // Start waiting for ACK after sending the packet
          updateHeartbeatTimer();   // Update the heartbeat timer after sending a message
          split_message = (packet.split_message == 1) ? true : false; // Check if the message is split
        }
        
        new_serial_data = false;  // Reset the flag after sending
        interrupt_flag = false;   // Reset the interrupt flag
        idle_listen_flag = false; // Start listening for new packets again
        stage = IDLE;
        break;
      }
      else
      {
        printf("Channel busy, waiting...\n");
        stage = SENDING_PACKET; // Go back to IDLE state if channel is busy
        break;
      }
    }

    default:
      break;
    }
    updateUserList();
  }
  return (0);
}

int radioInit()
{
  // initialize the radio
  int state = radio.begin(868.0, 125.0, 7, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 3, 8, 0, true);
  if (state != RADIOLIB_ERR_NONE)
  {
    printf("failed, code %d\n", state);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    return state;
  }
  printf("success!\n");
  radio.setDio1Action(intFlag);
  radio.setOutputPower(0);
  return state;
}

int checkState(int state)
{
  if (state != RADIOLIB_ERR_NONE)
  {
    printf("failed, code %d\n", state);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    return state;
  }
  // printf("success!\n");
  return RADIOLIB_ERR_NONE;
}

int checkUserList(char *usr_name)
{
  for (int i = 0; i < MAX_CHAT_USERS; i++)
  {
    if (strcmp(users[i].user_name, usr_name) == 0)
    {
      return i;
    }
  }
  return -1;
}

void updateUserStatus(char *usr_name)
{
  int index = checkUserList(usr_name);
  if (index == -1)
  {
    for (int i = 0; i < MAX_CHAT_USERS; i++)
    {
      if (users[i].user_name[0] == '\0')
      {
        strcpy(users[i].user_name, usr_name);
        break;
      }
    }
  }
  else
  {
    // User already exists in the list, update the timestamp and status
    users[index].last_seen = to_ms_since_boot(get_absolute_time());
    users[index].is_active = true;
  }
}

void updateUserList()
{
  uint32_t current_time = to_ms_since_boot(get_absolute_time());
  for (int i = 0; i < MAX_CHAT_USERS; i++)
  {
    if (users[i].is_active && (current_time - users[i].last_seen) > USER_STATUS_TIMEOUT_MS)
    {
      users[i].is_active = false;
    }
  }
}

int randomRange(int min, int max)
{
  if (min > max)
  {
    // Swap if min > max
    int temp = min;
    min = max;
    max = temp;
  }

  uint32_t range = max - min + 1;
  uint32_t random_val = get_rand_32();

  return min + (random_val % range);
}

bool doCSMA()
{
  // Check if enough time has passed since the last CSMA check
  curr_csma_time = to_ms_since_boot(get_absolute_time());
  if (curr_csma_time - prev_csma_time >= backoff_time)
  {
    int state = radio.scanChannel();

    if (state == RADIOLIB_LORA_DETECTED)
    {
      // Perform random backoff
      backoff_time = randomRange(CSMA_BACKOFF_MIN_MS, CSMA_BACKOFF_MAX_MS);
      prev_csma_time = curr_csma_time;
      return false; // Channel is busy, do not transmit
    }
    else if (state == RADIOLIB_CHANNEL_FREE)
    {
      // Perform transmission
      return true; // Channel is free, proceed with transmission
      prev_csma_time = curr_csma_time;
    }
    else
    {
      printf("Error scanning channel, code %d\n", state);
      return false;
    }
  }
  else
  {
    // Not enough time has passed, wait for the next check
    return false; // Do not transmit yet
  }
}

void goToBootloader()
{
  watchdog_hw->scratch[0] = 1;
  watchdog_reboot(0, 0, 10);
  while (1)
  {
    continue;
  }
}

void updateHeartbeatTimer()
{
  prev_heartbeat_time = to_ms_since_boot(get_absolute_time());
}

void updateAckTimer()
{
  prev_ack_check_time = to_ms_since_boot(get_absolute_time());
  waiting_for_ack_flag = true; // Set the flag to wait for ACK
}

void readSerialData()
{
  static uint16_t ndx = 0;
  char endMarker = '\n';
  char rc;
  while (uart_is_readable(SERIAL_PORT) && new_serial_data == false)
  {
    rc = uart_getc(SERIAL_PORT); // Read a character from the UART

    if (rc != endMarker)
    {
      serial_received_chars[ndx] = rc;
      ndx++;
      if (ndx >= char_buf_size)
      {
        ndx = char_buf_size - 1;
      }
    }
    else
    {
      serial_received_chars[ndx] = '\0'; // terminate the string
      if (serial_received_chars[0] == '/' && ndx > 1)
      {
        parseSerialData();
        ndx = 0;
        new_serial_data = false; // Reset the flag after parsing
      }
      else
      {
        ndx = 0;
        new_serial_data = true;
      }
    }
  }
}

void ledOn()
{
  gpio_put(PICO_DEFAULT_LED_PIN, 1);
}
void ledOff()
{
  gpio_put(PICO_DEFAULT_LED_PIN, 0);
}

bool checkAckReceived()
{
  if (waiting_for_ack_flag)
  {
    curr_ack_check_time = to_ms_since_boot(get_absolute_time());
    if (curr_ack_check_time - prev_ack_check_time > ACK_TIMEOUT_MS)
    {
      printf("ACK timeout, resending packet...\n");
      waiting_for_ack_flag = false; // Reset the flag
      return false;                 // ACK not received
    }
    else
    {
      return true; // ACK received within timeout
    }
  }
  return true; // No ACK waiting, return true to avoid blocking
}

void parseSerialData()
{
  // Parse the serial data for commands
  if (strcmp(serial_received_chars, "/bootloader") == 0)
  {
    goToBootloader();
  }
  else if (strcmp(serial_received_chars, "/test") == 0) {
    printf("Test command received\n");
  }
  else
  {
    printf("Unknown command: %s\n", serial_received_chars);
  }
}