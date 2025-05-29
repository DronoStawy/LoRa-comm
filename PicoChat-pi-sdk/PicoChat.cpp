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
#define ACK_TIMEOUT_MS 2000
#define USER_STATUS_TIMEOUT_MS 5000
#define HEARTBEAT_PERIOD_MS 3000

// CSMA timing parameters
#define CSMA_BACKOFF_MIN_MS 50
#define CSMA_BACKOFF_MAX_MS 300

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
const uint8_t char_buf_size = 140;
char serial_received_chars[char_buf_size];

// Our user variables
char user_name[MAX_USER_NAME_LENGTH];

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

int main()
{
  stdio_init_all();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  gpio_init(USER_PIN);
  gpio_set_dir(USER_PIN, GPIO_IN);
  gpio_pull_up(USER_PIN);

  uart_set_hw_flow(SERIAL_PORT, false, false);

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
    if (curr_heartbeat_time - prev_heartbeat_time > 1000)
    {
      stage = SENDING_HEARTBEAT;
    }
    switch (stage)
    {
    case IDLE:
    {
      ledOff();
      readSerialData();                                  // Check if we have new serial data
      stage = (checkAckReceived()) ? SENDING_ACK : IDLE; // If message was sent, check for ACK
      if (new_serial_data)
      {
        stage = SENDING_PACKET;
      }
      if (!idle_listen_flag)
      {
        int state = radio.startReceive();
        checkState(state);
        idle_listen_flag = true;
      }
      if (interrupt_flag)
      {
        interrupt_flag = false;
        uint8_t buf[255];
        int state = radio.readData(buf, sizeof(buf));
        if (state == RADIOLIB_ERR_NONE)
        {
          chat_packet packet(buf);
          if (packet.id == PACKET_TYPE_HEARTBEAT)
          {
            printf("Heartbeat received from %s\n", packet.user_name);
            // Check if the user is already in the list
            updateUserStatus(packet.user_name);
            // interrupt_flag = false;
            stage = IDLE;
            break;
          }

          else if (packet.id == PACKET_TYPE_MESSAGE)
          {
            ledOn();
            printf("Message: %s\n", packet.message);
            // Update status of the user who sent the message
            updateUserStatus(packet.user_name);
            // interrupt_flag = false;
            stage = SENDING_ACK;
            break;
          }
          else if (packet.id == PACKET_TYPE_ACK)
          {
            printf("ACK received from %s\n", packet.user_name);
            // Update status of the user who sent the ACK
            updateUserStatus(packet.user_name);
            // interrupt_flag = false;
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
        chat_packet packet(PACKET_TYPE_HEARTBEAT, user_name, NULL);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        int state = radio.transmit(packet.toByteArray(), packet.getPacketSize());
        checkState(state);
        prev_heartbeat_time = curr_heartbeat_time;
        idle_listen_flag = false; // Start listening for new packets again
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
        chat_packet packet(PACKET_TYPE_ACK, user_name, NULL);
        int state = radio.transmit(packet.toByteArray(), packet.getPacketSize());
        checkState(state);
        updateHeartbeatTimer();
        idle_listen_flag = false; // Start listening for new packets again
        stage = IDLE;
        break;
      }
      else
      {
        printf("Channel busy, waiting...\n");
        stage = SENDING_ACK; // Go back to IDLE state if channel is busy
        break;
      }
    }
    case SENDING_PACKET:
    {
      if (doCSMA())
      {
        ledOn();
        // Create a packet with the received serial data
        chat_packet packet(PACKET_TYPE_MESSAGE, user_name, serial_received_chars);
        int state = radio.transmit(packet.toByteArray(), packet.getPacketSize());
        checkState(state);
        updateAckTimer();         // Start waiting for ACK after sending the packet
        updateHeartbeatTimer();   // Update the heartbeat timer after sending a message
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
  static uint8_t ndx = 0;
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
      ndx = 0;
      new_serial_data = true;
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