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

// nowy pakiet 1B - typ  8B - payload  4B - sterowanie silnikami

#include <pico/stdlib.h>
#include <pico/rand.h>
#include <pico/stdio_usb.h>
#include <tusb.h>
#include "hardware/watchdog.h"
#include <RadioLib.h>
#include "hal/RPiPico/PicoHal.h"
#include "packets/packets.hpp"
// #include <aes.hpp>
#include <chrono>
#include <thread>
#include <string>
#include <tusb.h>
// Pico - z kabelkiem
// Chat - bez kabelka

#define LORA_SCK 18
#define LORA_MISO 16
#define LORA_MOSI 19
#define LORA_CS 17
#define LORA_RST 26
#define LORA_DIO1 22
#define LORA_BUSY 15
#define LORA_ANT_SW 17

// Set which module is a master

// Set ACK timeout to 2 seconds
#define ACK_TIMEOUT_MS 5000

// CSMA timing parameters
#define CSMA_BACKOFF_MIN_MS 50
#define CSMA_BACKOFF_MAX_MS 300

using namespace std::this_thread; // sleep_for, sleep_until
using namespace std::chrono;      // nanoseconds, system_clock, seconds

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
PicoHal *hal = new PicoHal(spi0, LORA_MISO, LORA_MOSI, LORA_SCK);
LR1121 radio = new Module(hal, LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

const uint32_t rfswitch_dio_pins[] = {
RADIOLIB_LR11X0_DIO5, // Corresponds to RFSW0
RADIOLIB_LR11X0_DIO6, // Corresponds to RFSW1
RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
};

const Module::RfSwitchMode_t rfswitch_table[] = {
// RadioLib Mode {RFSW0 (DIO5)}, {RFSW1 (DIO6)}
{LR11x0::MODE_STBY,  {0, 0}},
{LR11x0::MODE_RX,    {0, 1}}, // Ebyte SDK: .rx = RFSW1_HIGH
{LR11x0::MODE_TX,    {1, 1}}, // Ebyte SDK: .tx = RFSW0_HIGH | RFSW1_HIGH
{LR11x0::MODE_TX_HP, {1, 0}}, // Ebyte SDK: .tx_hp = RFSW0_HIGH
{LR11x0::MODE_TX_HF, {1, 1}}, // From p7 table, seems consistent with TX LP
END_OF_MODE_TABLE,
};

volatile bool interrupt_flag = false;
bool idle_listen_flag = false;
bool waiting_for_ack_flag = false;
bool sending_packet_flag = false;
bool sending_ack_flag = false;

bool new_serial_data = false;
const uint16_t char_buf_size = 1200;
char serial_received_chars[char_buf_size];

// Packet variables
bool retransmission_flag = false; // flag to indicate if the message is being retransmitted
uint8_t crc_calculated = 0;

void intFlag()
{
  interrupt_flag = true;
}

// Function prototypes
int radioInit();
int checkState(int state);
int randomRange(int min, int max);
bool doCSMA();
void updateAckTimer();
void readSerialData();
void ledOn();
void ledOff();
bool checkAckReceived();
void parseSerialData();

int main()
{
  stdio_usb_init();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  uart_set_hw_flow(SERIAL_PORT, false, false);

  // Initialize timers
  prev_ack_check_time = to_ms_since_boot(get_absolute_time());
  prev_heartbeat_time = to_ms_since_boot(get_absolute_time());

  radioInit();
  transmission_stage stage = IDLE;
  sleep_ms(randomRange(100, 500)); // Random delay to avoid collisions at startup

  for (;;)
  {
    // printf("DZIALA\n");
    //  Check timers
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
      // if (checkAckReceived())
      // {
      //   retransmission_flag = false; // Reset retransmission flag if ACK received
      //   stage = IDLE;
      // }
      // else
      // {
      //   stage = SENDING_PACKET;
      //   retransmission_flag = false; // Set retransmission flag if ACK not received
      //   prev_heartbeat_time = to_ms_since_boot(get_absolute_time());
      //   break;
      // }
      if (!idle_listen_flag)
      {
        int state = radio.startReceive();
        checkState(state);
        idle_listen_flag = true;
      }
      if (interrupt_flag)
      {
        uint8_t buf[PACKET_SIZE];
        int state = radio.readData(buf, PACKET_SIZE);
        interrupt_flag = false;
        if (state == RADIOLIB_ERR_NONE)
        {
          Packet packet(buf);
          if (packet.type == PACKET_TYPE_MESSAGE)
          {
            ledOn();
            printf("Type: %i\n", packet.type);
            printf("Payload: %s\n", packet.payload);
            stage = SENDING_ACK;
            // if (crc_calculated == packet.control_sum)
            // {
            //   stage = SENDING_ACK;
            //   break;
            // }
            // else
            // {
            //   stage = IDLE; // gdy crc się nie zgadza, to po prostu nie wysyłamy potwierdzenia odbioru informacji
            //   break;
            // }
          }
          else if (packet.type == PACKET_TYPE_ACK)
          {
            // printf("ACK received from %s\n", packet.user_name);
            // deb_serial.serialACKReceived(packet.id);
            printf("ACK received\n");
            printf("Type: %i\n", packet.type);
            printf("Payload: %s\n", packet.payload);
            // Update status of the user who sent the ACK
            // updateUserStatus(packet.user_name);
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
    case SENDING_ACK:
    {
      if (doCSMA())
      {
        // id = (id != 255) ? id + 1 : 0; // Increment ID, reset to 0 if it reaches 255
        Packet packet(0, (const uint8_t *)"OKOKOKOK");
        printf("Sending ACK...\n");
        printf("Type: %i\n", packet.type);
        printf("Payload: %s\n", packet.payload);
        int state = radio.transmit(packet.toByteArray(), PACKET_SIZE);
        checkState(state);        // do odkomentowania
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
        Packet packet(2, (const uint8_t *)serial_received_chars);
        printf("Sending packet...\n");
        printf("Type: %i\n", packet.type);
        printf("Payload: %s\n", packet.payload);
        int state = radio.transmit(packet.toByteArray(), PACKET_SIZE);
        checkState(state);
        printf("Packet sent\n");
        updateAckTimer(); // Start waiting for ACK after sending the packet

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
  }
  return (0);
}

int radioInit()
{
  // initialize the radio
  // int state = radio.begin(2400.0, 812, 7, 7, RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE, 3, 8, 0);
  int state = radio.begin();
  radio.setFrequency(2400.0);
  radio.setBandwidth(200.0, true);
  radio.setSpreadingFactor(5);
  radio.setCRC(true);
  radio.setIrqAction(intFlag);
  radio.setOutputPower(0);
  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

  if (state != RADIOLIB_ERR_NONE)
  {
    printf("failed, code %d\n", state);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    return state;
  }
  printf("success!\n");

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
  while (tud_cdc_available() && new_serial_data == false)
  {
    // printf("Reading serial data...\n");
    rc = getchar();
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
      // serial_received_chars[ndx] = '\0'; // terminate the string
      // printf("Serial data received: %s\n", serial_received_chars);
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

void parseSerialData()
{
  // Parse the serial data for commands
  if (strcmp(serial_received_chars, "/bootloader") == 0)
  {
    goToBootloader();
  }
  else if (strcmp(serial_received_chars, "/test") == 0)
  {
    printf("Test command received\n");
  }
  else
  {
    printf("Unknown command: %s\n", serial_received_chars);
  }
}