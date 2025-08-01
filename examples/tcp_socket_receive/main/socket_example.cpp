/**
 * @file socket_example.cpp
 * @author Daan Pape <daan@dptechnics.com>
 * @date 24 Apr 2025
 * @copyright DPTechnics bv
 * @brief Walter Modem library examples
 *
 * @section LICENSE
 *
 * Copyright (C) 2025, DPTechnics bv
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of DPTechnics bv nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *   4. This software, with or without modification, must only be used with a
 *      Walter board from DPTechnics bv.
 *
 *   5. Any software provided in binary form under this license must not be
 *      reverse engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY DPTECHNICS BV “AS IS” AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL DPTECHNICS BV OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 *
 * This example demonstrates how to configure the modem in Walter to send sample
 * data to the Walter Demo server on walterdemo.quickspot.io.
 */

#include "WalterModem.h"

#include <driver/uart.h>
#include <esp_log.h>
#include <esp_mac.h>

/**
 * @brief Cellular APN for SIM card. Leave empty to autodetect APN.
 */
CONFIG(CELLULAR_APN, const char *, "")

/**
 * @brief Time delay in ms of data sent to the Walter demo server.
 */
CONFIG_UINT16(SEND_DELAY_MS, 10000)

/**
 * @brief The address of the Walter demo server.
 */
CONFIG(SERV_ADDR, const char *, "example.com")

/**
 * @brief The UDP port of the Walter demo server.
 */
CONFIG_INT(SERV_PORT, 80)

/**
 * @brief ESP-IDF log prefix.
 */
static constexpr const char *TAG = "socket_example";

/**
 * @brief The modem instance.
 */
WalterModem modem;

/**
 * @brief Response object containing command response information.
 */
WalterModemRsp rsp;

/**
 * @brief The buffer to transmit to the demo server. The first 6 bytes will be
 * the MAC address of the Walter this code is running on.
 */
uint8_t dataBuf[1500] = {0};

uint16_t dataAvailable = 0;

/**
 * @brief This function checks if we are connected to the lte network
 *
 * @return True when connected, False otherwise
 */
bool lteConnected()
{
    WalterModemNetworkRegState regState = modem.getNetworkRegState();
    return (
        regState == WALTER_MODEM_NETWORK_REG_REGISTERED_HOME ||
        regState == WALTER_MODEM_NETWORK_REG_REGISTERED_ROAMING);
}

/**
 * @brief This function waits for the modem to be connected to the Lte network.
 * @return true if the connected, else false on timeout.
 */
bool waitForNetwork()
{
    /* Wait for the network to become available */
    int timeout = 0;
    while (!lteConnected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout += 1;
        if (timeout > 300)
            return false;
    }
    ESP_LOGI(TAG, "Connected to the network");
    return true;
}

/**
 * @brief This function tries to connect the modem to the cellular network.
 * @return true if the connection attempt is successful, else false.
 */
bool lteConnect()
{
    if (modem.setOpState(WALTER_MODEM_OPSTATE_NO_RF)) {
        ESP_LOGI(TAG, "Successfully set operational state to NO RF");
    } else {
        ESP_LOGI(TAG, "Could not set operational state to NO RF");
        return false;
    }

    if (modem.setRadioBands(WalterModemRAT::WALTER_MODEM_RAT_LTEM, WALTER_MODEM_BAND_B20)) {
        ESP_LOGI(TAG, "Succesfully set the radio band");
    } else {
        ESP_LOGI(TAG, "Could not set operational state to NO RF");
        return false;
    }

    /* Create PDP context */
    if (modem.definePDPContext(1, CELLULAR_APN)) {
        ESP_LOGI(TAG, "Created PDP context");
    } else {
        ESP_LOGI(TAG, "Could not create PDP context");
        return false;
    }

    /* Set the operational state to full */
    if (modem.setOpState(WALTER_MODEM_OPSTATE_FULL)) {
        ESP_LOGI(TAG, "Successfully set operational state to FULL");
    } else {
        ESP_LOGI(TAG, "Could not set operational state to FULL");
        return false;
    }

    /* Set the network operator selection to automatic */
    if (modem.setNetworkSelectionMode(WALTER_MODEM_NETWORK_SEL_MODE_AUTOMATIC)) {
        ESP_LOGI(TAG, "Network selection mode to was set to automatic");
    } else {
        ESP_LOGI(TAG, "Could not set the network selection mode to automatic");
        return false;
    }

    return waitForNetwork();
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Walter UDP Socket example v1");

    /* Get the MAC address for board validation */
    esp_read_mac(dataBuf, ESP_MAC_WIFI_STA);
    ESP_LOGI(
        TAG,
        "Walter's MAC is: %02X:%02X:%02X:%02X:%02X:%02X",
        dataBuf[0],
        dataBuf[1],
        dataBuf[2],
        dataBuf[3],
        dataBuf[4],
        dataBuf[5]);

    /* Initialize the modem */
    if (WalterModem::begin(UART_NUM_1)) {
        ESP_LOGI(TAG, "Successfully initialized modem");
    } else {
        ESP_LOGE(TAG, "Could not initialize modem");
        return;
    }

    /* Connect the modem to the lte network */
    if (!lteConnect()) {
        ESP_LOGE(TAG, "Could Not Connect to LTE");
        return;
    }

    /* Construct a socket */
    if (modem.socketConfig(&rsp)) {
        ESP_LOGI(TAG, "Created a new socket");
    } else {
        ESP_LOGE(TAG, "Could not create a new socket");
        return;
    }

    /* disable socket tls as the demo server does not use it */
    if (modem.socketConfigSecure(false)) {
        ESP_LOGI(TAG, "Successfully disabled socket TLS");
    } else {
        ESP_LOGE(TAG, "Could not disable socket TLS");
        return;
    }

    /* Construct a socket */
    if (modem.socketConfigExtended(&rsp)) {
        ESP_LOGI(TAG, "Defined socket extended config params");
    } else {
        ESP_LOGE(TAG, "Could not define socket extended config params");
        return;
    }

    /* Connect to the demo server */
    if (modem.socketDial(
            SERV_ADDR, SERV_PORT, 0, NULL, NULL, NULL, WALTER_MODEM_SOCKET_PROTO_TCP)) {
        ESP_LOGI(TAG, "Connected to demo server %s:%d", SERV_ADDR, SERV_PORT);
    } else {
        ESP_LOGE(TAG, "Could not connect demo socket");
        return;
    }

    for (;;) {
        if (modem.socketSend((char *)"GET / HTTP/1.1\r\nHost: example.com\r\n\r\n")) {
            ESP_LOGI(TAG, "Transmitted GET request");
        } else {
            ESP_LOGE(TAG, "Could not transmit GET request");
            esp_restart();
        }

        vTaskDelay(pdMS_TO_TICKS(SEND_DELAY_MS));
        while (modem.socketAvailable() > 0) {
            // 1500 is the max amount of bytes the modem can read at a time.
            uint16_t dataToRead = (modem.socketAvailable() > 1500) ? 1500 : modem.socketAvailable();
            ESP_LOGI(TAG,"Reading: %u bytes",dataToRead);
            if (modem.socketReceive(dataToRead, sizeof(dataBuf), dataBuf)) {
                ESP_LOGI(TAG, "Remaining: %u | Data: %.*s", modem.socketAvailable(), dataToRead, dataBuf);
            }
        }
    }
}
