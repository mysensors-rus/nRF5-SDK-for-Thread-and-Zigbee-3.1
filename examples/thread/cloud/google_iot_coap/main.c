/**
 * Copyright (c) 2018-2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup gcp_cloud_coap_client_example_main main.c
 * @{
 * @ingroup gcp_cloud_coap_client_example_example
 * @brief Google Cloud CoAP Client Example Application main file.
 *
 * @details This example demonstrates a CoAP client application that sends emulated
 *          counter value and measured temperature to the Google Cloud Platform. Example uses NAT64 on the
 *          Thread Border Router soulution for IPv4 connectivity.
 *
 *
 */

#include <stdint.h>
#include <stdio.h>

#include "app_scheduler.h"
#include "app_timer.h"
#include "bsp_thread.h"
#include "nrf_assert.h"
#include "nrf_log_ctrl.h"
#include "nrf_log.h"
#include "nrf_log_default_backends.h"
#include "nrf_temp.h"

#include "thread_utils.h"

#include <openthread/coap.h>
#include <openthread/coap_secure.h>
#include <openthread/crypto.h>
#include <openthread/ip6.h>
#include <openthread/link.h>
#include <openthread/message.h>
#include <openthread/sntp.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/platform/alarm-milli.h>
#include <openthread/platform/openthread-system.h>

#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>

/***************************************************************************************************
 * @section Example Configuration.
 **************************************************************************************************/

/**
 * APP scheduler settings.
 */
#define SCHED_QUEUE_SIZE                      32
#define SCHED_EVENT_DATA_SIZE                 APP_TIMER_SCHED_EVENT_DATA_SIZE

/**
 * APP timer objects.
 */
APP_TIMER_DEF(m_sntp_timer);
APP_TIMER_DEF(m_temp_timer);

#define TEMP_MEASUREMENT_INTERVAL             5000
#define SNTP_QUERY_INTERVAL                   120000

/**
 * Counter limits.
 */
#define COUNT_MIN                             15
#define COUNT_MAX                             30

/**
 * Temporary buffer size.
 */
#define BUFFER_LENGTH                         512

/**
 * SNTP server address.
 */
#define SNTP_SERVER_ADDRESS                   "64:ff9b::d8ef:2308"
#define SNTP_SERVER_PORT                      123

/**
 * Google Cloud Platform CoAP server parameters.
 */
#define GCP_COAP_IOT_CORE_SERVER_PORT         5683
#define GCP_COAP_IOT_CORE_SERVER_SECURE_PORT  5684

/**
 * Google Cloud Platform project configuration.
 * Must be configured by the user.
 */
#define GCP_COAP_IOT_CORE_SERVER_ADDRESS      "64:ff9b::xxxx:xxxx"
#define GCP_COAP_IOT_CORE_PATH                "gcp"
#define GCP_COAP_IOT_CORE_PROJECT_ID          "project-name-xxxx"
#define GCP_COAP_IOT_CORE_REGISTRY_ID         "coap-demo"
#define GCP_COAP_IOT_CORE_REGION              "us-central1"
#define GCP_COAP_IOT_CORE_PUBLISH             "publishEvent"
#define GCP_COAP_IOT_CORE_CONFIG              "config"

/**
 * CoAP transport configuration.
 * Must be configured by the user.
 */
#define GCP_COAP_SECURE_ENABLED               0
#define GCP_COAP_SECURE_PSK                   "some_secret"
#define GCP_COAP_SECURE_PSK_INDENTITY         "my_identity"

/**
 * Google Cloud Platform device configuration.
 * Must be configured by the user.
 */
#define GCP_COAP_IOT_CORE_DEVICE_ID          "demo-device"
#define GCP_COAP_IOT_CORE_DEVICE_KEY                                   \
"-----BEGIN EC PRIVATE KEY-----\r\n"                                   \
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n" \
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n" \
"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx==\r\n"                             \
"-----END EC PRIVATE KEY-----\r\n"

/**
 * Device configuration config. Only for demonstration purposes.
 */
#define CONFIG_LED1 "LED1"
#define CONFIG_LED2 "LED2"
#define CONFIG_LED3 "LED3"
#define CONFIG_LED4 "LED4"

/**
 * The JSON representation of the header with ES256 algorithm.
 */
#define JWT_HEADER_TYPE_ES256 \
    "{\"alg\":\"ES256\",\"typ\":\"JWT\"}" 

/**
 * The maximum size of the JWT signature.
 */
#define JWT_SIGNATURE_SIZE 64

/**
 * The size of key length for ES256.
 */
#define JWT_KEY_LENGTH_ES256 32

/**
 * The JWT delimeter used to separete header, claim and signature.
 *
 */
#define JWT_DELIMETER '.'

/**
 * this variable indicates that Thread node is attached.
 */
static bool m_is_attached;

/**
 * Time obtained from SNTP.
 */
static uint64_t m_unix_time;

/**
 * Last reported temperature.
 */
static int32_t m_temp = 24;

/**
 * Last reported counter value.
 */
static uint32_t m_counter = 22;

#if GCP_COAP_SECURE_ENABLED
/**
 * Variable indicates if config has been requested.
 */
static bool m_config_requested = false;
#endif

/**
 * Forward declarations.
 */
static void coap_publish(void);
static void coap_config_get(void);
static void sntp_query(void);

/***************************************************************************************************
 * @section Buttons
 **************************************************************************************************/

static void bsp_event_handler(bsp_event_t event)
{
    if (!m_is_attached)
    {
        return;
    }

    switch (event)
    {
        case BSP_EVENT_KEY_0:
            coap_config_get();
            break;

        case BSP_EVENT_KEY_1:
            break;

        case BSP_EVENT_KEY_2:
            if (m_counter <= COUNT_MIN)
            {
                break;
            }

            m_counter--;

            coap_publish();
            break;

        case BSP_EVENT_KEY_3:
            if (m_counter >= COUNT_MAX)
            {
                break;
            }

            m_counter++;

            coap_publish();
            break;

        default:
            return; // no implementation needed
    }
}

/***************************************************************************************************
 * @section Callbacks
 **************************************************************************************************/

/**@brief Thread state changed handler.
 */
static void thread_state_changed_callback(uint32_t flags, void * p_context)
{
    uint32_t error;

    UNUSED_PARAMETER(p_context);

    if (flags & OT_CHANGED_THREAD_ROLE)
    {
        switch(otThreadGetDeviceRole(thread_ot_instance_get()))
        {
            case OT_DEVICE_ROLE_CHILD:
            case OT_DEVICE_ROLE_ROUTER:
            case OT_DEVICE_ROLE_LEADER:
                m_is_attached = true;

                // Send SNTP query.
                sntp_query();

                error = app_timer_start(m_temp_timer, APP_TIMER_TICKS(TEMP_MEASUREMENT_INTERVAL), NULL);
                ASSERT(error == NRF_SUCCESS);

                error = app_timer_start(m_sntp_timer, APP_TIMER_TICKS(SNTP_QUERY_INTERVAL), NULL);
                ASSERT(error == NRF_SUCCESS);
                break;

            case OT_DEVICE_ROLE_DISABLED:
            case OT_DEVICE_ROLE_DETACHED:
                m_is_attached = false;

                error = app_timer_stop(m_temp_timer);
                ASSERT(error == NRF_SUCCESS);

                error = app_timer_stop(m_sntp_timer);
                ASSERT(error == NRF_SUCCESS);
                break;

            default:
                break;
        }
    }

    NRF_LOG_INFO("State changed! Flags: 0x%08x Current role: %d\r\n", flags,
                 otThreadGetDeviceRole(p_context));
}

static void coap_default_handler(void                * p_context,
                                 otMessage           * p_message,
                                 const otMessageInfo * p_message_info)
{
    (void)p_context;
    (void)p_message;
    (void)p_message_info;

    NRF_LOG_INFO("Received CoAP message that does not match any request or resource\r\n");
}


/**@brief SNTP timer handler. */
static void sntp_timer_handler(void * p_context)
{
    (void)p_context;

    sntp_query();
}


/**@brief SNTP response handler. */
static void sntp_response_handler(void *p_context, uint64_t unix_time, otError result)
{
    (void)p_context;

    if (result == OT_ERROR_NONE)
    {
        m_unix_time = unix_time;
    }
}


/**@brief CoAP response handler. */
static void coap_response_handler(void                * p_context,
                                  otMessage           * p_message,
                                  const otMessageInfo * p_message_info,
                                  otError               result)
{
    (void)p_message_info;

    // Handle payload only if its response for config option.
    if (p_context == (void *)GCP_COAP_IOT_CORE_CONFIG)
    {
        do
        {
            if (otCoapMessageGetType(p_message) != OT_COAP_TYPE_NON_CONFIRMABLE)
            {
                break;
            }

            if (otCoapMessageGetCode(p_message) != OT_COAP_CODE_CONTENT)
            {
                break;
            }

            char     config[BUFFER_LENGTH];
            uint16_t config_length = 0;

            if ((config_length = otMessageRead(p_message, otMessageGetOffset(p_message), config, BUFFER_LENGTH - 1)) == 0)
            {
                NRF_LOG_INFO("Config includes incorrect data\r\n");
                break;
            }

            ASSERT(config_length < BUFFER_LENGTH);
            config[config_length] = 0;

            LEDS_OFF(LEDS_MASK);

            if (strncmp(config, CONFIG_LED1, strlen(CONFIG_LED1)) == 0)
            {
                LEDS_ON(BSP_LED_0_MASK);
            }
            else if (strncmp(config, CONFIG_LED2, strlen(CONFIG_LED2)) == 0)
            {
                LEDS_ON(BSP_LED_1_MASK);
            }
            else if (strncmp(config, CONFIG_LED3, strlen(CONFIG_LED3)) == 0)
            {
                LEDS_ON(BSP_LED_2_MASK);
            }
            else if (strncmp(config, CONFIG_LED4, strlen(CONFIG_LED4)) == 0)
            {
                LEDS_ON(BSP_LED_3_MASK);
            }
        } while (false);
    }
}


/**@brief Temperature measurement timer handler. */
static void temp_timer_handler(void * p_context)
{
    (void)p_context;

    NRF_TEMP->TASKS_START = 1;
    /* Busy wait while temperature measurement is not finished. */
    while (NRF_TEMP->EVENTS_DATARDY == 0)
    {
        // Do nothing.
    }
    NRF_TEMP->EVENTS_DATARDY = 0;

    int32_t temp = nrf_temp_read() / 4;

    NRF_TEMP->TASKS_STOP = 1;

    if (m_temp != temp)
    {
        m_temp = temp;
        coap_publish();
    }
}

/***************************************************************************************************
 * @section Simple Network Time Protocol query.
 **************************************************************************************************/

static void sntp_query(void)
{
    otError error = OT_ERROR_NONE;

    otMessageInfo message_info;
    memset(&message_info, 0, sizeof(message_info));

    message_info.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;
    message_info.mPeerPort   = SNTP_SERVER_PORT;
    error = otIp6AddressFromString(SNTP_SERVER_ADDRESS, &message_info.mPeerAddr);
    ASSERT(error == OT_ERROR_NONE);

    otSntpQuery query;
    query.mMessageInfo = &message_info;

    error = otSntpClientQuery(thread_ot_instance_get(), &query, sntp_response_handler, NULL);
    ASSERT(error == OT_ERROR_NONE);
}


/***************************************************************************************************
 * @section JWT generation.
 **************************************************************************************************/

static otError base64_url_encode(uint8_t *p_output, uint16_t *p_output_len, const uint8_t *p_buff, uint16_t length)
{
    otError error = OT_ERROR_NONE;
    int     result;
    size_t  encoded_len = 0;

    result = mbedtls_base64_encode(p_output, *p_output_len, &encoded_len, p_buff, length);
    
    if (result != 0)
    {
        return OT_ERROR_NO_BUFS;
    }

    // JWT uses URI as defined in RFC4648, while mbedtls as is in RFC1421.
    for (uint32_t index = 0; index < encoded_len; index++)
    {
        if (p_output[index] == '+')
        {
            p_output[index] = '-';
        }
        else if (p_output[index] == '/')
        {
            p_output[index] = '_';
        }
        else if (p_output[index] == '=')
        {
            p_output[index] = 0;
            encoded_len  = index;
            break;
        }
    }

    *p_output_len = encoded_len;

    return error;
}
 
static otError jwt_create(uint8_t       * p_output,
                          uint16_t      * p_output_len,
                          const uint8_t * p_claims,
                          uint16_t        claims_len,
                          const uint8_t * p_private_key,
                          uint16_t        private_key_len)
{
    otError                error = OT_ERROR_NONE;
    uint8_t                hash[32];
    uint8_t                signature[JWT_SIGNATURE_SIZE];
    uint16_t               signature_size    = JWT_SIGNATURE_SIZE;
    uint16_t               output_max_length = *p_output_len;
    uint16_t               length;

    // Encode JWT Header using Base64 URL.
    length = output_max_length;

    error = base64_url_encode(p_output, &length, (const uint8_t *)JWT_HEADER_TYPE_ES256,
                              strlen(JWT_HEADER_TYPE_ES256));
    ASSERT(error == OT_ERROR_NONE);

    *p_output_len = length;

    // Append delimiter.
    p_output[*p_output_len] = JWT_DELIMETER;
    *p_output_len += 1;

    // Encode JWT Claim using Base64 URL.
    length = output_max_length - *p_output_len;

    error = base64_url_encode(p_output + *p_output_len, &length, p_claims, claims_len);
    ASSERT(error == OT_ERROR_NONE);

    *p_output_len += length;

    // Create SHA256 Hash from encoded JWT Header and JWT Claim.
    int err = mbedtls_sha256_ret(p_output, *p_output_len, hash, 0);
    ASSERT(err == 0);

    // Append delimiter.
    p_output[*p_output_len] = JWT_DELIMETER;
    *p_output_len += 1;

    // Create ECDSA Sign.
    error = otCryptoEcdsaSign(signature, &signature_size, hash, sizeof(hash), p_private_key, private_key_len);
    ASSERT(error == OT_ERROR_NONE);

    // Encode JWT Sign using Base64 URL.
    length = output_max_length - *p_output_len;

    error = base64_url_encode(p_output + *p_output_len, &length, signature, signature_size);
    ASSERT(error == OT_ERROR_NONE);

    *p_output_len += length;

    return error;
}

/***************************************************************************************************
 * @section CoAP messages.
 **************************************************************************************************/

#if GCP_COAP_SECURE_ENABLED
void coap_secure_connect_handler(bool connected, void *aContext)
{
    if (connected)
    {
        if (m_config_requested)
        {
            m_config_requested = false;
            coap_config_get();
        }
        else
        {
            coap_publish();
        }
    }
}
#endif

static void coap_header_proxy_uri_append(otMessage * p_message, const char * p_action)
{
    otError error = OT_ERROR_NONE;
    char    jwt[BUFFER_LENGTH];
    char    claims[BUFFER_LENGTH];

    memset(jwt, 0, sizeof(jwt));
    memset(claims, 0, sizeof(claims));

    uint16_t offset = snprintf(jwt, sizeof(jwt), "%s/%s/%s/%s/%s?jwt=",
                               GCP_COAP_IOT_CORE_PROJECT_ID, GCP_COAP_IOT_CORE_REGION,
                               GCP_COAP_IOT_CORE_REGISTRY_ID, GCP_COAP_IOT_CORE_DEVICE_ID,
                               p_action);

    uint16_t output_len = sizeof(jwt) - offset;

    uint64_t timeout = m_unix_time + (SNTP_QUERY_INTERVAL/1000) * 2;

    uint16_t length = snprintf(claims, sizeof(claims), "{\"iat\":%ld,\"exp\":%ld,\"aud\":\"%s\"}",
                               (uint32_t)(m_unix_time), (uint32_t)(timeout), GCP_COAP_IOT_CORE_PROJECT_ID);
    ASSERT(length > 0);

    error = jwt_create((uint8_t *)&jwt[offset], &output_len, (const uint8_t *)claims, strlen(claims),
                               (const uint8_t *)GCP_COAP_IOT_CORE_DEVICE_KEY, sizeof(GCP_COAP_IOT_CORE_DEVICE_KEY));
    ASSERT(error == OT_ERROR_NONE);

    error = otCoapMessageAppendProxyUriOption(p_message, jwt);
    ASSERT(error == OT_ERROR_NONE);
}


static void coap_payload_append(otMessage * p_message)
{
    char payload[BUFFER_LENGTH];

    memset(payload, 0, sizeof(payload));

    uint16_t length = snprintf(payload, sizeof(payload), "{\"temp\":%ld,\"counter\":%ld}",
                               m_temp, m_counter);

    otError error = otMessageAppend(p_message, payload, length);
    ASSERT(error == OT_ERROR_NONE);
}

#if GCP_COAP_SECURE_ENABLED
void coap_secure_connect(void)
{
    otError error = OT_ERROR_NONE;

    otSockAddr sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));

    sock_addr.mPort = GCP_COAP_IOT_CORE_SERVER_SECURE_PORT;

    error = otIp6AddressFromString(GCP_COAP_IOT_CORE_SERVER_ADDRESS, &sock_addr.mAddress);
    ASSERT(error == OT_ERROR_NONE);

    error = otCoapSecureSetPsk(thread_ot_instance_get(),
                               (const uint8_t *)GCP_COAP_SECURE_PSK,
                               strlen(GCP_COAP_SECURE_PSK),
                               (const uint8_t *)GCP_COAP_SECURE_PSK_INDENTITY,
                               strlen(GCP_COAP_SECURE_PSK_INDENTITY));
    ASSERT(error == OT_ERROR_NONE);

    error = otCoapSecureConnect(thread_ot_instance_get(),
                                        &sock_addr,
                                        coap_secure_connect_handler,
                                        NULL);
    ASSERT(error == OT_ERROR_NONE);
}
#endif

static void coap_publish(void)
{
    otError       error = OT_ERROR_NONE;
    otMessage   * p_message;
    otMessageInfo message_info;

#if GCP_COAP_SECURE_ENABLED
    if (!otCoapSecureIsConnected(thread_ot_instance_get()) && !otCoapSecureIsConnectionActive(thread_ot_instance_get()))
    {
        coap_secure_connect();
        return;
    }
#endif

    do
    {
        p_message = otCoapNewMessage(thread_ot_instance_get(), NULL);
        if (p_message == NULL)
        {
            break;
        }

        otCoapMessageInit(p_message, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_POST);
        otCoapMessageGenerateToken(p_message, 2);

        error = otCoapMessageAppendUriPathOptions(p_message, GCP_COAP_IOT_CORE_PATH);
        ASSERT(error == OT_ERROR_NONE);

        coap_header_proxy_uri_append(p_message, GCP_COAP_IOT_CORE_PUBLISH);

        error = otCoapMessageSetPayloadMarker(p_message);
        ASSERT(error == OT_ERROR_NONE);

        coap_payload_append(p_message);

        // Set message info structure to point on GCP server.
        memset(&message_info, 0, sizeof(message_info));
        message_info.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;
        message_info.mPeerPort    = GCP_COAP_IOT_CORE_SERVER_PORT;

        error = otIp6AddressFromString(GCP_COAP_IOT_CORE_SERVER_ADDRESS, &message_info.mPeerAddr);
        ASSERT(error == OT_ERROR_NONE);

#if GCP_COAP_SECURE_ENABLED
        error = otCoapSecureSendRequest(thread_ot_instance_get(),
                                        p_message,
                                        coap_response_handler,
                                        NULL);
#else
        error = otCoapSendRequest(thread_ot_instance_get(),
                                  p_message,
                                  &message_info,
                                  coap_response_handler,
                                  NULL);
#endif
    } while (false);

    if (error != OT_ERROR_NONE && p_message != NULL)
    {
        otMessageFree(p_message);
    }
}


static void coap_config_get(void)
{
    otError       error = OT_ERROR_NONE;
    otMessage   * p_message;
    otMessageInfo message_info;

#if GCP_COAP_SECURE_ENABLED
    if (!otCoapSecureIsConnected(thread_ot_instance_get()) && !otCoapSecureIsConnectionActive(thread_ot_instance_get()))
    {
        m_config_requested = true;
        coap_secure_connect();
        return;
    }
#endif

    do
    {
        p_message = otCoapNewMessage(thread_ot_instance_get(), NULL);
        if (p_message == NULL)
        {
            break;
        }

        otCoapMessageInit(p_message, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_POST);
        otCoapMessageGenerateToken(p_message, 2);

        error = otCoapMessageAppendUriPathOptions(p_message, GCP_COAP_IOT_CORE_PATH);
        ASSERT(error == OT_ERROR_NONE);

        coap_header_proxy_uri_append(p_message, GCP_COAP_IOT_CORE_CONFIG);

        error = otCoapMessageSetPayloadMarker(p_message);
        ASSERT(error == OT_ERROR_NONE);

        coap_payload_append(p_message);

        // Set message info structure to point on GCP server.
        memset(&message_info, 0, sizeof(message_info));
        message_info.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;
        message_info.mPeerPort    = GCP_COAP_IOT_CORE_SERVER_PORT;

        error = otIp6AddressFromString(GCP_COAP_IOT_CORE_SERVER_ADDRESS, &message_info.mPeerAddr);
        ASSERT(error == OT_ERROR_NONE);

#if GCP_COAP_SECURE_ENABLED
        error = otCoapSecureSendRequest(thread_ot_instance_get(),
                                        p_message,
                                        coap_response_handler,
                                        GCP_COAP_IOT_CORE_CONFIG);
#else
        error = otCoapSendRequest(thread_ot_instance_get(),
                                  p_message,
                                  &message_info,
                                  coap_response_handler,
                                  GCP_COAP_IOT_CORE_CONFIG);
#endif
    } while (false);

    if (error != OT_ERROR_NONE && p_message != NULL)
    {
        otMessageFree(p_message);
    }
}

/***************************************************************************************************
 * @section Initialization
 **************************************************************************************************/

 /**@brief Function for initializing the Thread Board Support Package.
 */
static void thread_bsp_init(void)
{
    uint32_t error_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
    APP_ERROR_CHECK(error_code);

    error_code = bsp_thread_init(thread_ot_instance_get());
    APP_ERROR_CHECK(error_code);
}


/**@brief Function for initializing the Application Timer Module.
 */
static void timer_init(void)
{
    uint32_t error_code = app_timer_init();
    APP_ERROR_CHECK(error_code);

    error_code = app_timer_create(&m_sntp_timer,
                                  APP_TIMER_MODE_REPEATED,
                                  sntp_timer_handler);
    APP_ERROR_CHECK(error_code);

    error_code = app_timer_create(&m_temp_timer,
                                  APP_TIMER_MODE_REPEATED,
                                  temp_timer_handler);
    APP_ERROR_CHECK(error_code);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing the Thread Stack.
 */
static void thread_instance_init(void)
{
    thread_configuration_t thread_configuration =
    {
        .radio_mode            = THREAD_RADIO_MODE_RX_ON_WHEN_IDLE,
        .autocommissioning     = true,
        .default_child_timeout = 10,
    };

    thread_init(&thread_configuration);
    thread_cli_init();
    thread_state_changed_callback_set(thread_state_changed_callback);
}


/**@brief Function for initializing the Constrained Application Protocol Module.
 */
static void thread_coap_init(void)
{
#if GCP_COAP_SECURE_ENABLED
    otError error = otCoapSecureStart(thread_ot_instance_get(), OT_DEFAULT_COAP_SECURE_PORT);
    ASSERT(error == OT_ERROR_NONE);

    otCoapSecureSetDefaultHandler(thread_ot_instance_get(), coap_default_handler, NULL);

#else // GCP_COAP_SECURE_ENABLED
    otError error = otCoapStart(thread_ot_instance_get(), OT_DEFAULT_COAP_PORT);
    ASSERT(error == OT_ERROR_NONE);

    otCoapSetDefaultHandler(thread_ot_instance_get(), coap_default_handler, NULL);
#endif
}


/**@brief Function for initializing scheduler module.
 */
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

/***************************************************************************************************
 * @section Main
 **************************************************************************************************/

int main(int argc, char * argv[])
{
    log_init();
    scheduler_init();
    timer_init();
    nrf_temp_init();

    thread_instance_init();
    thread_coap_init();
    thread_bsp_init();

    while (true)
    {
        thread_process();
        app_sched_execute();

        if (NRF_LOG_PROCESS() == false)
        {
            thread_sleep();
        }
    }
}

/**
 *@}
 **/
