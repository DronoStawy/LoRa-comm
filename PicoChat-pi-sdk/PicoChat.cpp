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

// ============================================================
// PROTOCOL TIMING CONFIGURATION
// ============================================================
#define ACK_TIMEOUT_MS 100  // Time to wait for ACK response
#define ACK_RETRIES 2       // Number of retransmission attempts

// Processing delay after transmission
#define TX_PROCESSING_DELAY_MS 3

// ============================================================
// RECOMMENDED PACKET DELAY FOR TEST SCRIPTS
// ============================================================
// When using controlled_test.py or similar scripts, use these delays:
//
// MODE_RELIABLE:        25ms delay → 100% success, 0.16 KB/s
// MODE_HIGH_PERFORMANCE: 23ms delay → 98% success, 0.17 KB/s
//
// Example usage:
//   python3 controlled_test.py /dev/ttyUSB0 /dev/ttyUSB1 100 25
// ============================================================

// ACK timing variables
uint32_t curr_ack_check_time = 0;
uint32_t prev_ack_check_time = 0;
uint8_t ack_retries = 0;

// Debug counters
uint32_t packets_sent = 0;
uint32_t packets_received = 0;
uint32_t acks_sent = 0;
uint32_t acks_received = 0;
uint32_t timeouts = 0;

// create a new instance of the HAL class
PicoHal *hal = new PicoHal(spi0, LORA_MISO, LORA_MOSI, LORA_SCK);
LR1121 radio = new Module(hal, LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY);

const uint32_t rfswitch_dio_pins[] = {
    RADIOLIB_LR11X0_DIO5, // Corresponds to RFSW0
    RADIOLIB_LR11X0_DIO6, // Corresponds to RFSW1
    RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC};

const Module::RfSwitchMode_t rfswitch_table[] = {
    // RadioLib Mode {RFSW0 (DIO5)}, {RFSW1 (DIO6)}
    {LR11x0::MODE_STBY, {0, 0}},
    {LR11x0::MODE_RX, {0, 1}},    // Ebyte SDK: .rx = RFSW1_HIGH
    {LR11x0::MODE_TX, {1, 1}},    // Ebyte SDK: .tx = RFSW0_HIGH | RFSW1_HIGH
    {LR11x0::MODE_TX_HP, {1, 0}}, // Ebyte SDK: .tx_hp = RFSW0_HIGH
    {LR11x0::MODE_TX_HF, {1, 1}}, // From p7 table, seems consistent with TX LP
    END_OF_MODE_TABLE,
};

// State management
volatile bool interrupt_flag = false;
bool new_serial_data = false;
char serial_received_chars[PAYLOAD_SIZE];
bool is_radio_listening = false;

// Idle substates
enum idle_substate_t {
  IDLE_LISTENING,         // Normal listening for incoming packets
  IDLE_WAITING_FOR_ACK    // Waiting for ACK after sending packet
};

// Serial buffer for queuing packets
#define SERIAL_BUFFER_SIZE 512  // Increased to handle larger payloads
uint8_t serial_buffer[SERIAL_BUFFER_SIZE];
uint16_t serial_buffer_head = 0;
uint16_t serial_buffer_tail = 0;

// Packet variables
bool retransmission_flag = false; // flag to indicate if the message is being retransmitted
uint8_t crc_calculated = 0;

uint8_t length = 0;

void intFlag()
{
  interrupt_flag = true;
}

// Function prototypes - Radio
int radioInit();
int checkState(int state);
void startReceiveMode();

// Function prototypes - Serial & Buffer
void fillSerialBuffer();
void readSerialData();

// Function prototypes - Packet handlers
transmission_stage handleIncomingPacket(idle_substate_t& idle_substate);
void handleMessagePacket(Packet& packet);
void handleAckPacket(idle_substate_t& idle_substate);
void sendAckPacket();
void sendDataPacket();

// Function prototypes - Timeout & ACK management
bool checkAckTimeout();
void retryPacketTransmission();
void failPacketTransmission();
void resetAckWaiting();

// Function prototypes - Utility
void ledOn();
void ledOff();

// ============================================================
// MAIN PROGRAM
// ============================================================

int main()
{
  // Initialize hardware
  stdio_usb_init();
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

  prev_ack_check_time = to_ms_since_boot(get_absolute_time());

  // Initialize LoRa radio
  radioInit();
  
  // Main state machine variables
  transmission_stage stage = IDLE;
  idle_substate_t idle_substate = IDLE_LISTENING;

  // Main event loop
  for (;;)
  {
    switch (stage)
    {
    case IDLE:
    {
      ledOff();
      
      // Continuously manage serial buffer and prepare packets
      fillSerialBuffer();  // Always read from USB to prevent overflow
      readSerialData();    // Try to prepare next packet if ready
      
      // ========================================
      // IDLE STATE - Handle two substates:
      // 1. IDLE_LISTENING: Normal operation, listening for incoming packets
      // 2. IDLE_WAITING_FOR_ACK: Waiting for acknowledgment after sending
      // ========================================
      
      switch (idle_substate)
      {
      case IDLE_LISTENING:
      {
        // Check if we have new data to send
        if (new_serial_data)
        {
          ack_retries = 0;
          stage = SENDING_PACKET;
          idle_substate = IDLE_WAITING_FOR_ACK;
          break;
        }
        
        // Ensure radio is in receive mode
        startReceiveMode();
        
        // Process incoming packets (messages from remote device)
        if (interrupt_flag)
        {
          stage = handleIncomingPacket(idle_substate);
        }
        break;
      }
      
      case IDLE_WAITING_FOR_ACK:
      {
        // Check if ACK timeout occurred
        if (checkAckTimeout())
        {
          if (ack_retries < ACK_RETRIES)
          {
            // Retry sending the packet
            retryPacketTransmission();
            stage = SENDING_PACKET;
          }
          else
          {
            // Max retries reached, give up on this packet
            failPacketTransmission();
            idle_substate = IDLE_LISTENING;
          }
          break;
        }
        
        // Keep listening for ACK response
        startReceiveMode();
        
        // Process incoming packets (expecting ACK)
        if (interrupt_flag)
        {
          stage = handleIncomingPacket(idle_substate);
        }
        break;
      }
      }
      break;
    }
    
    case SENDING_ACK:
    {
      sendAckPacket();
      stage = IDLE;
      idle_substate = IDLE_LISTENING;
      break;
    }
    
    case SENDING_PACKET:
    {
      sendDataPacket();
      stage = IDLE;
      idle_substate = IDLE_WAITING_FOR_ACK;
      break;
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
  radio.setBandwidth(RADIOLIB_LR11X0_LORA_BW_812_50, true);
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

void fillSerialBuffer()
{
  // Always read from USB CDC into circular buffer
  while (tud_cdc_available() > 0)
  {
    uint16_t next_head = (serial_buffer_head + 1) % SERIAL_BUFFER_SIZE;
    if (next_head != serial_buffer_tail) // Buffer not full
    {
      serial_buffer[serial_buffer_head] = getchar();
      serial_buffer_head = next_head;
    }
    else
    {
      // Buffer full, stop reading to prevent data loss
      break;
    }
  }
}

void readSerialData()
{
  // Try to prepare a packet from the buffer (only if not already waiting to send)
  if (!new_serial_data && serial_buffer_head != serial_buffer_tail)
  {
    length = 0;
    memset(serial_received_chars, 0, PAYLOAD_SIZE);
    
    // Extract up to PAYLOAD_SIZE bytes from buffer
    while (length < PAYLOAD_SIZE && serial_buffer_tail != serial_buffer_head)
    {
      serial_received_chars[length] = serial_buffer[serial_buffer_tail];
      serial_buffer_tail = (serial_buffer_tail + 1) % SERIAL_BUFFER_SIZE;
      length++;
    }
    
    new_serial_data = (length > 0);
  }
}

// ============================================================
// RADIO FUNCTIONS
// ============================================================

void startReceiveMode()
{
  if (!is_radio_listening)
  {
    int state = radio.startReceive();
    checkState(state);
    is_radio_listening = true;
  }
}

// ============================================================
// PACKET HANDLING FUNCTIONS
// ============================================================

transmission_stage handleIncomingPacket(idle_substate_t& idle_substate)
{
  interrupt_flag = false; // Clear flag immediately
  is_radio_listening = false; // Need to restart receive after processing
  
  uint8_t buf[PACKET_SIZE];
  int state = radio.readData(buf, PACKET_SIZE);
  
  transmission_stage next_stage = IDLE;
  
  if (state == RADIOLIB_ERR_NONE)
  {
    Packet packet(buf);
    
    if (packet.type == PACKET_TYPE_MESSAGE)
    {
      handleMessagePacket(packet);
      next_stage = SENDING_ACK;
    }
    else if (packet.type == PACKET_TYPE_ACK)
    {
      handleAckPacket(idle_substate);
      next_stage = IDLE;
    }
  }
  
  // Only restart receive if staying in IDLE
  // If we need to send ACK, let SENDING_ACK state handle it
  if (next_stage == IDLE)
  {
    startReceiveMode();
  }
  
  return next_stage;
}

void handleMessagePacket(Packet& packet)
{
  ledOn();
  packets_received++;
  
  // Forward to USB
  tud_cdc_write(packet.payload, packet.length);
  tud_cdc_write_flush();
}

void handleAckPacket(idle_substate_t& idle_substate)
{
  acks_received++;
  ledOff();
  resetAckWaiting();
  idle_substate = IDLE_LISTENING;
}

void sendAckPacket()
{
  fillSerialBuffer(); // Keep buffer filled
  
  acks_sent++;
  Packet packet(PACKET_TYPE_ACK, 3, (const uint8_t *)"ACK");
  uint8_t* packet_data = packet.toByteArray();
  
  int state = radio.transmit(packet_data, PACKET_SIZE);
  free(packet_data);
  checkState(state);
  
  // Minimal delay for ACK processing
  sleep_ms(2);
  
  // Restart receive mode
  is_radio_listening = false;
  interrupt_flag = false;
  startReceiveMode();
}

void sendDataPacket()
{
  fillSerialBuffer(); // Keep buffer filled
  
  ledOn();
  packets_sent++;
  
  Packet packet(PACKET_TYPE_MESSAGE, length, (const uint8_t *)serial_received_chars);
  uint8_t* packet_data = packet.toByteArray();
  
  int state = radio.transmit(packet_data, PACKET_SIZE);
  free(packet_data);
  checkState(state);
  
  // Processing delay after transmission
  sleep_ms(TX_PROCESSING_DELAY_MS);
  
  // Start listening for ACK
  is_radio_listening = false;
  interrupt_flag = false;
  prev_ack_check_time = to_ms_since_boot(get_absolute_time());
  startReceiveMode();
}

// ============================================================
// TIMEOUT & ACK MANAGEMENT FUNCTIONS
// ============================================================

bool checkAckTimeout()
{
  curr_ack_check_time = to_ms_since_boot(get_absolute_time());
  return (curr_ack_check_time - prev_ack_check_time > ACK_TIMEOUT_MS);
}

void retryPacketTransmission()
{
  timeouts++;
  ack_retries++;
  // Keep new_serial_data and serial_received_chars for retransmission
}

void failPacketTransmission()
{
  timeouts++;
  resetAckWaiting();
}

void resetAckWaiting()
{
  ack_retries = 0;
  new_serial_data = false;
  prev_ack_check_time = to_ms_since_boot(get_absolute_time());
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

void ledOn()
{
  gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

void ledOff()
{
  gpio_put(PICO_DEFAULT_LED_PIN, 0);
}