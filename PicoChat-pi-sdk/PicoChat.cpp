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

#include <pico/stdlib.h>
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

#define MODE_PIN 7

// Set ACK timeout to 2 seconds
#define ACK_TIMEOUT_MS 2000

// ACK timing variables
uint32_t curr_ack_check_time = 0;
uint32_t prev_ack_check_time = 0;

// Heartbeat timing variables
uint32_t curr_heartbeat_time = 0;
uint32_t prev_heartbeat_time = 0;

// create a new instance of the HAL class
PicoHal *hal = new PicoHal(spi1, LORA_MISO, LORA_MOSI, LORA_SCK);
SX1262 radio = new Module(hal, LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

volatile bool interrupt_flag = false;
bool waiting_for_packet_flag = false;
bool waiting_for_ack_flag = false;
bool sending_packet_flag = false;
bool sending_ack_flag = false;

void intFlag()
{
  interrupt_flag = true;
}

// Function prototypes
int radioInit();
int sendMessage(char *usr_name, char *message);
int checkState(int state);

int main()
{
  stdio_init_all();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  uart_set_hw_flow(uart0, false, false);

  bool led_state = false;

  radioInit();
  chat_stage stage = IDLE;

  for (;;)
  {
    switch (stage)
    {
    case IDLE:
    {
      if (!waiting_for_packet_flag)
      {
        int state = radio.startReceive();
        checkState(state);
        waiting_for_packet_flag = true;
      }
      if (interrupt_flag)
      {
        printf("Packet received!\n");
        uint8_t buf[255];
        int state = radio.readData(buf, sizeof(buf));
        if (checkState(state))
        {
          chat_packet packet(buf);
          printf("Packet id: %s\n", packet.id);
          printf("User name: %s\n", packet.user_name);

          if(packet.id == PACKET_TYPE_HEARTBEAT){
            printf("Heartbeat received from %s\n", packet.user_name);
            interrupt_flag = false;
            stage = IDLE;
            waiting_for_packet_flag = false;
            break;
          }

          else if (packet.id == PACKET_TYPE_MESSAGE)
          {
            printf("Message: %s\n", packet.message);
            interrupt_flag = false;
            stage = SENDING_ACK;
            waiting_for_packet_flag = false;
            break;
          }
          else if (packet.id == PACKET_TYPE_ACK)
          {
            printf("ACK received from %s\n", packet.user_name);
            interrupt_flag = false;
            stage = IDLE;
            waiting_for_packet_flag = false;
            break;
          }
          else
          {
            printf("Unknown packet type\n");
            interrupt_flag = false;
            stage = IDLE;
            waiting_for_packet_flag = false;
            break;
          }
        }
      }
    }

    break;

    default:
      break;
    }
  }
  return (0);
}

int radioInit()
{
  // initialize the radio
  int state = radio.begin(868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 0, true);
  if (state != RADIOLIB_ERR_NONE)
  {
    printf("failed, code %d\n", state);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    return state;
  }
  printf("success!\n");
  return RADIOLIB_ERR_NONE;

  radio.setDio1Action(intFlag);
  radio.setOutputPower(0);
}

int sendMessage(char *usr_name, char *message)
{
  // create a new packet
  chat_packet packet(PACKET_TYPE_MESSAGE, usr_name, message);
  radio.transmit(packet.toByteArray(), packet.getPacketSize());
}

int checkState(int state)
{
  if (state != RADIOLIB_ERR_NONE)
  {
    printf("failed, code %d\n", state);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    return state;
  }
  printf("success!\n");
  return RADIOLIB_ERR_NONE;
}