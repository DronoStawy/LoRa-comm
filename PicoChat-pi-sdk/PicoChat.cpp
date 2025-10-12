#include <pico/stdlib.h>
#include <pico/rand.h>
#include <pico/stdio_usb.h>
#include <tusb.h>
#include "hardware/watchdog.h"
#include <RadioLib.h>
#include "hal/RPiPico/PicoHal.h"
#include "packets/packets.hpp"
#include <chrono>
#include <thread>
#include <string>
#include <tusb.h>

#define LORA_SCK 18
#define LORA_MISO 16
#define LORA_MOSI 19
#define LORA_CS 17
#define LORA_RST 26
#define LORA_DIO1 22
#define LORA_BUSY 15
#define LORA_ANT_SW 17

#define ACK_TIMEOUT_MS 50
#define ACK_RETRIES 2

// CSMA timing parameters
#define CSMA_BACKOFF_MIN_MS 2
#define CSMA_BACKOFF_MAX_MS 10

// ACK timing variables
uint32_t curr_ack_check_time = 0;
uint32_t prev_ack_check_time = 0;
uint8_t ack_retries = 0;

// CSMA timing variables
uint32_t prev_csma_time = 0;
int backoff_time = 0;

// create a new instance of the HAL class
PicoHal *hal = new PicoHal(spi0, LORA_MISO, LORA_MOSI, LORA_SCK);
LR1121 radio = new Module(hal, LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

const uint32_t rfswitch_dio_pins[] = {
    RADIOLIB_LR11X0_DIO5, // Corresponds to RFSW0
    RADIOLIB_LR11X0_DIO6, // Corresponds to RFSW1
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC};

const Module::RfSwitchMode_t rfswitch_table[] = {
    // RadioLib Mode {RFSW0 (DIO5)}, {RFSW1 (DIO6)}
    {LR11x0::MODE_STBY,   {0, 0}},
    {LR11x0::MODE_RX,     {0, 1}},    // Ebyte SDK: .rx = RFSW1_HIGH
    {LR11x0::MODE_TX,     {1, 1}},    // Ebyte SDK: .tx = RFSW0_HIGH | RFSW1_HIGH
    {LR11x0::MODE_TX_HP,  {1, 0}}, // Ebyte SDK: .tx_hp = RFSW0_HIGH
    {LR11x0::MODE_TX_HF,  {1, 1}}, // From p7 table, seems consistent with TX LP
    END_OF_MODE_TABLE,
};

volatile bool interrupt_flag = false;
bool idle_listen_flag = false;
bool waiting_for_ack_flag = false;
bool sending_packet_flag = false;
bool sending_ack_flag = false;

bool new_serial_data = false;
char serial_received_chars[PAYLOAD_SIZE];

// Packet variables
bool retransmission_flag = false; // flag to indicate if the message is being retransmitted
uint8_t crc_calculated = 0;

uint8_t length = 0;

void intFlag()
{
  interrupt_flag = true;
}

// Function prototypes
int radioInit();
int checkState(int state);
int randomRange(int min, int max);
bool doCSMA();
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

  prev_ack_check_time = to_ms_since_boot(get_absolute_time());

  radioInit();
  transmission_stage stage = IDLE;

  for (;;)
  {
    switch (stage)
    {
    case IDLE:
    {
      ledOff();
      if (waiting_for_ack_flag)
      {
        curr_ack_check_time = to_ms_since_boot(get_absolute_time());
        if (curr_ack_check_time - prev_ack_check_time > ACK_TIMEOUT_MS)
        {
          if (ack_retries < ACK_RETRIES)
          {
            //printf("ACK timeout, resending packet...\n");
            waiting_for_ack_flag = true; // Reset the flag
            ack_retries++;
            stage = SENDING_PACKET;
            break;
          }
          else
          {
            //printf("Max ACK retries reached, giving up...\n");
            waiting_for_ack_flag = false; // Reset the flag
            ack_retries = 0;              // Reset the retry counter
            stage = IDLE;
            break;
          }
        }
      }
      else
      {
        readSerialData();
      }
      if (new_serial_data && !waiting_for_ack_flag)
      {
        stage = SENDING_PACKET;
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
        uint8_t buf[PACKET_SIZE];
        int state = radio.readData(buf, PACKET_SIZE);
        interrupt_flag = false;
        if (state == RADIOLIB_ERR_NONE)
        {
          Packet packet(buf);
          if (packet.type == PACKET_TYPE_MESSAGE)
          {
            ledOn();
            tud_cdc_write(packet.payload, packet.length);
            tud_cdc_write_flush();
            stage = SENDING_ACK;
          }
          else if (packet.type == PACKET_TYPE_ACK)
          {
            prev_ack_check_time = to_ms_since_boot(get_absolute_time());
            waiting_for_ack_flag = false; // Reset the waiting for ACK flag
            stage = IDLE;
            break;
          }
          else
          {
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
        Packet packet(PACKET_TYPE_ACK, 3, (const uint8_t *)"ACK");
        int state = radio.transmit(packet.toByteArray(), PACKET_SIZE);
        checkState(state);
        idle_listen_flag = false; // Start listening for new packets again
        interrupt_flag = false;   // Reset the interrupt flag
        stage = IDLE;
        break;
      }
      else
      {
        stage = SENDING_ACK;
        break;
      }
    }
    case SENDING_PACKET:
    {
      if (doCSMA())
      {
        ledOn();
        Packet packet(PACKET_TYPE_MESSAGE, length, (const uint8_t *)serial_received_chars);
        int state = radio.transmit(packet.toByteArray(), PACKET_SIZE);
        checkState(state);
        prev_ack_check_time = to_ms_since_boot(get_absolute_time()); // Start waiting for ACK after sending the packet
        interrupt_flag = false;   // Reset the interrupt flag
        idle_listen_flag = false; // Start listening for new packets again
        waiting_for_ack_flag = true;
        stage = IDLE;
        break;
      }
      else
      {
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
  int state = radio.begin();
  radio.setFrequency(2400.0);
  radio.setBandwidth(RADIOLIB_LR11X0_LORA_BW_406_25, true);
  radio.setSpreadingFactor(5);
  radio.setCRC(true);
  radio.setIrqAction(intFlag);
  radio.setOutputPower(10);
  radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

  if (state != RADIOLIB_ERR_NONE)
  {
    printf("failed, code %d\n", state);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    return state;
  }
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
  uint32_t curr_csma_time = to_ms_since_boot(get_absolute_time());
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
      prev_csma_time = curr_csma_time;
      return true; // Channel is free, proceed with transmission
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

void readSerialData()
{
  length = 0;
  memset(serial_received_chars, 0, PAYLOAD_SIZE); // Clear the buffer
  while (length < PAYLOAD_SIZE && tud_cdc_available() > 0)
  {
    serial_received_chars[length] = getchar();
    length++;
  }
  new_serial_data = (length > 0);
}

void ledOn()
{
  gpio_put(PICO_DEFAULT_LED_PIN, 1);
}
void ledOff()
{
  gpio_put(PICO_DEFAULT_LED_PIN, 0);
}