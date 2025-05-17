#include <pico/stdlib.h>
#include "hardware/watchdog.h"
#include <RadioLib.h>
#include "hal/RPiPico/PicoHal.h"
#include "packets/packets.hpp"

#define LORA_SCK 14
#define LORA_MISO 24
#define LORA_MOSI 15
#define LORA_CS 13
#define LORA_RST 23
#define LORA_DIO1 16
#define LORA_BUSY 18
#define LORA_ANT_SW 17

#define MODE_PIN 7

// create a new instance of the HAL class
PicoHal *hal = new PicoHal(spi1, LORA_MISO, LORA_MOSI, LORA_SCK);
SX1262 radio = new Module(hal, LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

int main()
{
  stdio_init_all();

  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  bool led_state = false;

  printf("[SX1262] Initializing ... ");
  int state = radio.begin(868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 0, true);
  if (state != RADIOLIB_ERR_NONE)
  {
    printf("failed, code %d\n", state);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    return state;
  }
  printf("success!\n");

  for (;;)
  {
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    // send a packet
    message_packet packet(1, "RP2040", "Hello from pico SDK!");
    printf("[SX1262] Transmitting packet ... ");
    state = radio.transmit(packet.toByteArray(), packet.getPacketSize());
    if(state == RADIOLIB_ERR_NONE) {
      // the packet was successfully transmitted
      printf("success!\n");

      // wait for a second before transmitting again
      gpio_put(PICO_DEFAULT_LED_PIN, 0);
      hal->delay(2000);

    } else {
      printf("failed, code %d\n", state);

    }
  }

  return (0);
}