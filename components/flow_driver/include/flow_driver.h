#pragma once

#include "esp_err.h"
#include "drone_types.h"

/**
 * Inizializza UART1 a 19200 baud sui pin D6(TX)/D7(RX) per il sensore
 * optical flow PMW3901 + ToF VL53L1X (protocollo CXOF).
 */
esp_err_t flow_init(void);

/**
 * Legge byte dalla UART e parsa frame CXOF.
 * Ritorna ESP_OK e riempie data se un frame valido è stato ricevuto.
 * Ritorna ESP_ERR_NOT_FOUND se non ci sono frame completi disponibili.
 * Non-bloccante: legge solo i byte già nel buffer UART.
 */
esp_err_t flow_read(flow_data_t *data);
