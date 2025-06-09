#include <RadioLib.h>
#include <stdio.h>

#include "PicoHal.h"
#include "pico/stdlib.h"

#define USING_RFM 0
#define USING_ON_BREADBOARD 1
#define TRANSMIT_INTERVAL_MS 1000

#define RADIO_FREQ 434.0
#define RADIO_BW 125.0
#define RADIO_SF 9
#define RADIO_CR 7
#define RADIO_SYNC_WORD 18
#define RADIO_PREAMBLE_LEN 8
#define RADIO_RFM_GAIN 0
#define RADIO_SX_TXCO_VOLT 0.0
#define RADIO_SX_USE_REG_LDO false

#define RADIO_STATE_NO_ATTEMPT 1
#define RADIO_ERROR_CUSTOM 2
#define RADIO_RECEIVE_TIMEOUT_MS 1000
#define RADIO_TRANSMIT_TIMEOUT_MS 10000
#define RADIO_NO_CONTACT_PANIC_TIME_MS \
  (1000UL * 60 * 60 * 24 * 7)  //  7 days in ms

#define RADIO_RF_SWITCH_PIN 12
#define RADIO_SX_POWER_PIN 7
#define RADIO_RFM_POWER_PIN 14

#define RADIO_RF_SWITCH_RFM 1
#define RADIO_RF_SWITCH_SX 0

#if USING_ON_BREADBOARD
// pinout for on breadboard
#define RADIO_SX_NSS_PIN 28
#define RADIO_SX_DIO1_PIN 15
#define RADIO_SX_NRST_PIN 27
#define RADIO_SX_BUSY_PIN 5

#define RADIO_RFM_NSS_PIN 7
#define RADIO_RFM_DIO0_PIN 17
#define RADIO_RFM_NRST_PIN 22
#define RADIO_RFM_DIO1_PIN 26

#else
// pinout for on pcb
#define RADIO_SX_NSS_PIN 5
#define RADIO_SX_DIO1_PIN 22
#define RADIO_SX_NRST_PIN 24
#define RADIO_SX_BUSY_PIN 23

#define RADIO_RFM_NSS_PIN 17
#define RADIO_RFM_DIO0_PIN 27
#define RADIO_RFM_NRST_PIN 20
#define RADIO_RFM_DIO1_PIN 29

#endif

uint8_t radio_transmit_power = 2;

PicoHal* picoHal =
    new PicoHal(spi0, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_RX_PIN,
                PICO_DEFAULT_SPI_SCK_PIN);

RFM98 radioRFM = new Module(
    picoHal, RADIO_RFM_NSS_PIN, RADIO_RFM_DIO0_PIN, RADIO_RFM_NRST_PIN,
    RADIO_RFM_DIO1_PIN);  // RADIOLIB_NC); // RFM98 is an alias for SX1278
SX1268 radioSX = new Module(picoHal, RADIO_SX_NSS_PIN, RADIO_SX_DIO1_PIN,
                            RADIO_SX_NRST_PIN, RADIO_SX_BUSY_PIN);
int radio_state_SX;
int radio_state_RFM;

#if USING_RFM
PhysicalLayer* radio = &radioRFM;
#else
PhysicalLayer* radio = &radioSX;
#endif

// isrs
volatile bool operation_done_RFM = false;
volatile bool cad_detected_RFM = false;
volatile bool general_flag_SX = false;

void radio_operation_done_RFM() { operation_done_RFM = true; }

void radio_cad_detected_RFM() { cad_detected_RFM = true; }

void radio_general_flag_SX() { general_flag_SX = true; }
// end isrs

int radio_hardware_switch_to(PhysicalLayer* new_radio) {
  if (new_radio == &radioRFM) {
    // turn off SX
    gpio_put(RADIO_SX_POWER_PIN, 0);
    // turn on RFM
    gpio_put(RADIO_RFM_POWER_PIN, 1);
    // rf switch to RFM
    gpio_put(RADIO_RF_SWITCH_PIN, RADIO_RF_SWITCH_RFM);

  } else {
    // turn off RFM
    gpio_put(RADIO_RFM_POWER_PIN, 0);
    // turn on SX
    gpio_put(RADIO_SX_POWER_PIN, 1);
    // rf switch to SX
    gpio_put(RADIO_RF_SWITCH_PIN, RADIO_RF_SWITCH_SX);
  }

  // wait for 100 ms (arbitrary) for power on
  sleep_ms(100);
  return 0;
}

int main() {
  stdio_init_all();

  printf("Starting...\n");

  // initialize rf switch and power switch gpio
  gpio_init(RADIO_RF_SWITCH_PIN);
  gpio_set_dir(RADIO_RF_SWITCH_PIN, 1);
  gpio_put(RADIO_RF_SWITCH_PIN, RADIO_RF_SWITCH_RFM);  // 0 for RFM

  gpio_init(RADIO_SX_POWER_PIN);
  gpio_set_dir(RADIO_SX_POWER_PIN, 1);
  gpio_put(RADIO_SX_POWER_PIN, 0);

  gpio_init(RADIO_RFM_POWER_PIN);
  gpio_set_dir(RADIO_RFM_POWER_PIN, 1);
  gpio_put(RADIO_RFM_POWER_PIN, 0);

  // switch hardware to RFM

  int state;
#if USING_RFM
  radio_hardware_switch_to(&radioRFM);
  state =
      radioRFM.begin(RADIO_FREQ, RADIO_BW, RADIO_SF, RADIO_CR, RADIO_SYNC_WORD,
                     radio_transmit_power, RADIO_PREAMBLE_LEN, RADIO_RFM_GAIN);

  printf("State: %d\n", state);

  radioRFM.setDio0Action(radio_operation_done_RFM, GPIO_IRQ_EDGE_RISE);
  radioRFM.setDio1Action(radio_cad_detected_RFM, GPIO_IRQ_EDGE_RISE);
#else
  radio_hardware_switch_to(&radioSX);
  radio_state_SX =
      radioSX.begin(RADIO_FREQ, RADIO_BW, RADIO_SF, RADIO_CR, RADIO_SYNC_WORD,
                    radio_transmit_power, RADIO_PREAMBLE_LEN,
                    RADIO_SX_TXCO_VOLT, RADIO_SX_USE_REG_LDO);
  printf("State: %d\n", state);

  radioSX.setDio1Action(radio_general_flag_SX);

#endif

  int channel_scan_state = radio->startChannelScan();
  printf("Channel scan state: %d\n", channel_scan_state);

  int it = 0;
  while (true) {
    printf("It: %d", it++);

    bool receiving = false;
    bool transmitting = false;
    int state = 0;
    uint32_t operation_start_time = to_ms_since_boot(get_absolute_time());
    uint32_t last_receive_time = to_ms_since_boot(get_absolute_time());
    int transmission_size = 0;
    uint32_t last_send = to_ms_since_boot(get_absolute_time());

    while (true) {
      // save now time since boot
      uint32_t radio_now = to_ms_since_boot(get_absolute_time());

      // check operation duration to avoid hanging in an operation mode
      if (receiving && abs((long long)(radio_now - operation_start_time)) >
                           RADIO_RECEIVE_TIMEOUT_MS) {
#if RADIO_LOGGING
        printf("receive timeout\n");
#endif
        receiving = false;
        radio->startChannelScan();
      } else if (transmitting &&
                 abs((long long)(radio_now - operation_start_time)) >
                     RADIO_TRANSMIT_TIMEOUT_MS) {
#if TEMP_ON || RADIO_LOGGING
        printf("transmit timeout\n");
#endif
        transmitting = false;
        radio->finishTransmit();
        radio->startChannelScan();
      }

      // handle interrupt flags
      if (cad_detected_RFM || operation_done_RFM || general_flag_SX) {
        // handle finished transmission
        if (transmitting && operation_done_RFM) {
          transmitting = false;
          radio->finishTransmit();
        }

        // handle finished receive
        else if (receiving) {
          size_t packet_size = radio->getPacketLength();
          uint8_t packet[packet_size];

          int packet_state = radio->readData(packet, packet_size);

          if (packet_state == RADIOLIB_ERR_NONE) {
            last_receive_time = to_ms_since_boot(get_absolute_time());

            printf("Received packet: ");
            for (int i = 0; i < packet_size; i++) {
              printf("%c", packet[i]);
            }
            printf("\n");
            printf("Stats: RSSI: %d SNR: %d Freq Error: %d\n", radio->getRSSI(),
                   radio->getSNR(),
                   (radio == &radioSX) ? radioSX.getFrequencyError()
                                       : radioRFM.getFrequencyError());
          }

          // clear receiving flag
          receiving = false;
        }
        // handle other interrupts (CAD done)
        // this is the main difference between the 2 modules
        else {
          if (radio == &radioRFM && cad_detected_RFM) {
            state = radio->startReceive();
            operation_start_time = to_ms_since_boot(get_absolute_time());

            receiving = true;
          } else if (radio == &radioSX) {  // radio is radioSX
            state =
                radioSX.getChannelScanResult();  // not a PhysicalLayer function

            if (state == RADIOLIB_LORA_DETECTED) {
              state = radio->startReceive();

              operation_start_time = to_ms_since_boot(get_absolute_time());
              receiving = true;
            }
          }
        }

        if (!receiving && !transmitting) {
          state = radio->startChannelScan();
        }

        // clear flags
        general_flag_SX = false;
        cad_detected_RFM = false;
        operation_done_RFM = false;
      }

      if (!transmitting && !receiving &&
          (last_send - radio_now) > TRANSMIT_INTERVAL_MS) {
        printf("Transmitting...");
        operation_start_time = to_ms_since_boot(get_absolute_time());
        int transmit_state = radio->startTransmit("Hello, this is a test");
        transmitting = true;
      }

      sleep_ms(1000);
    }

    return 0;
  }
}