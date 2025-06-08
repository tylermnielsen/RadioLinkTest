#include "pico/stdlib.h"
#include <stdio.h>
#include <RadioLib.h>
#include "PicoHal.h"

#define USING_RFM 0
#define USING_ON_BREADBOARD 1

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


PicoHal *picoHal = new PicoHal(spi0, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_SCK_PIN);
#if USING_RFM 
RFM98 radioRFM = new Module(picoHal, RADIO_RFM_NSS_PIN, RADIO_RFM_DIO0_PIN, RADIO_RFM_NRST_PIN, RADIO_RFM_DIO1_PIN); //RADIOLIB_NC); // RFM98 is an alias for SX1278
PhysicalLayer* radio = &radioRFM;
int radio_state_RFM = RADIO_STATE_NO_ATTEMPT; 
#else
SX1268 radioSX = new Module(picoHal, RADIO_SX_NSS_PIN, RADIO_SX_DIO1_PIN, RADIO_SX_NRST_PIN, RADIO_SX_BUSY_PIN); 
PhysicalLayer* radio = &radioSX;
int radio_state_SX = RADIO_STATE_NO_ATTEMPT;
#endif


int main() { 
  stdio_init_all(); 

  while(true){
    printf("Hello, World!"); 
    sleep_ms(1000); 
  }
  

  return 0; 
}