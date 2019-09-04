/**
 * Copyright (c) 2018 - 2019, Nordic Semiconductor ASA
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
 * @defgroup zigbee_examples_multiprotocol_nus_switch zigbee_color_dimmer_switch.h
 * @{
 * @ingroup zigbee_examples
 * @brief Macros for instantiating multiple endpoints in one Color Dimmer Switch device.
 *
 * This file contains macro definitions that are adjusted to support defining several endpoints per device in one file.
 */

#ifndef ZIGBEE_COLOR_DIMMER_SWITCH_H__
#define ZIGBEE_COLOR_DIMMER_SWITCH_H__

#include <stdint.h>
#include "zboss_api_addons.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZB_HA_DEVICE_VER_COLOR_DIMMER_SWITCH 0                                      /**< Version of the HA device. */
#define ZB_HA_COLOR_DIMMER_SWITCH_IN_CLUSTER_NUM   2                                /**< Number of Color Dimmer Switch IN (server) clusters. */
#define ZB_HA_COLOR_DIMMER_SWITCH_OUT_CLUSTER_NUM  6                                /**< Number of Color Dimmer Switch OUT (client) clusters. */

/** @brief Declare simple descriptor for Color Dimmer Switch device.
    @param ep_name - Endpoint variable name.
    @param ep_id - Endpoint ID.
    @param in_clust_num - Number of supported input clusters.
    @param out_clust_num - Number of supported output clusters.
*/
#define ZB_ZCL_DECLARE_COLOR_DIMMER_SWITCH_SIMPLE_DESC_VA(                                   \
    ep_name, ep_id, in_clust_num, out_clust_num)                                             \
    ZB_DECLARE_SIMPLE_DESC_VA(in_clust_num, out_clust_num, ep_name);                         \
    ZB_AF_SIMPLE_DESC_TYPE_VA(in_clust_num, out_clust_num, ep_name) simple_desc_##ep_name =  \
    {                                                                                        \
        ep_id,                                                                               \
        ZB_AF_HA_PROFILE_ID,                                                                 \
        ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID,                                                 \
        ZB_HA_DEVICE_VER_COLOR_DIMMER_SWITCH,                                                \
        0,                                                                                   \
        in_clust_num,                                                                        \
        out_clust_num,                                                                       \
        {                                                                                    \
            ZB_ZCL_CLUSTER_ID_BASIC,                                                         \
            ZB_ZCL_CLUSTER_ID_IDENTIFY,                                                      \
            ZB_ZCL_CLUSTER_ID_ON_OFF,                                                        \
            ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,                                                 \
            ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,                                                 \
            ZB_ZCL_CLUSTER_ID_SCENES,                                                        \
            ZB_ZCL_CLUSTER_ID_GROUPS,                                                        \
            ZB_ZCL_CLUSTER_ID_IDENTIFY,                                                      \
        }                                                                                    \
    }

/**@brief Declare Color Dimmer Switch endpoint.
 *
 * @param[IN] ep_name                Endpoint variable name.
 * @param[IN] ep_id [IN]             Endpoint ID.
 * @param[IN] cluster_list [IN]      List of endpoint clusters.
 */
#define ZB_ZCL_DECLARE_COLOR_DIMMER_SWITCH_EP(ep_name, ep_id, cluster_list)                           \
    ZB_ZCL_DECLARE_COLOR_DIMMER_SWITCH_SIMPLE_DESC_VA(                                                \
        ep_name,                                                                                      \
        ep_id,                                                                                        \
        ZB_HA_COLOR_DIMMER_SWITCH_IN_CLUSTER_NUM,                                                     \
        ZB_HA_COLOR_DIMMER_SWITCH_OUT_CLUSTER_NUM);                                                   \
    ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID, 0, NULL,                         \
                                ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list, \
                                (zb_af_simple_desc_1_1_t*)&simple_desc_##ep_name,                     \
                                0, NULL, /* No reporting ctx. */                                       \
                                0, NULL) /* No CVC ctx. */

/** @brief Declare cluster list for Color Dimmer Switch device.
    @param cluster_list_name - Cluster list variable name.
    @param basic_attr_list - Attribute list for Basic cluster.
    @param identify_attr_list - Attribute list for Identify cluster.
 */
#define ZB_HA_DECLARE_COLOR_DIMMER_SWITCH_CLUSTER_LIST(   \
    cluster_list_name,                                    \
    basic_attr_list,                                      \
    identify_attr_list)                                   \
zb_zcl_cluster_desc_t cluster_list_name[] =               \
{                                                         \
  ZB_ZCL_CLUSTER_DESC(                                    \
    ZB_ZCL_CLUSTER_ID_IDENTIFY,                           \
    ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t), \
    (identify_attr_list),                                 \
    ZB_ZCL_CLUSTER_SERVER_ROLE,                           \
    ZB_ZCL_MANUF_CODE_INVALID                             \
  ),                                                      \
  ZB_ZCL_CLUSTER_DESC(                                    \
    ZB_ZCL_CLUSTER_ID_BASIC,                              \
    ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t),    \
    (basic_attr_list),                                    \
    ZB_ZCL_CLUSTER_SERVER_ROLE,                           \
    ZB_ZCL_MANUF_CODE_INVALID                             \
  ),                                                      \
  ZB_ZCL_CLUSTER_DESC(                                    \
    ZB_ZCL_CLUSTER_ID_IDENTIFY,                           \
    0,                                                    \
    NULL,                                                 \
    ZB_ZCL_CLUSTER_CLIENT_ROLE,                           \
    ZB_ZCL_MANUF_CODE_INVALID                             \
  ),                                                      \
  ZB_ZCL_CLUSTER_DESC(                                    \
    ZB_ZCL_CLUSTER_ID_SCENES,                             \
    0,                                                    \
    NULL,                                                 \
    ZB_ZCL_CLUSTER_CLIENT_ROLE,                           \
    ZB_ZCL_MANUF_CODE_INVALID                             \
  ),                                                      \
  ZB_ZCL_CLUSTER_DESC(                                    \
    ZB_ZCL_CLUSTER_ID_GROUPS,                             \
    0,                                                    \
    NULL,                                                 \
    ZB_ZCL_CLUSTER_CLIENT_ROLE,                           \
    ZB_ZCL_MANUF_CODE_INVALID                             \
  ),                                                      \
  ZB_ZCL_CLUSTER_DESC(                                    \
    ZB_ZCL_CLUSTER_ID_ON_OFF,                             \
    0,                                                    \
    NULL,                                                 \
    ZB_ZCL_CLUSTER_CLIENT_ROLE,                           \
    ZB_ZCL_MANUF_CODE_INVALID                             \
  ),                                                      \
  ZB_ZCL_CLUSTER_DESC(                                    \
    ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,                      \
    0,                                                    \
    NULL,                                                 \
    ZB_ZCL_CLUSTER_CLIENT_ROLE,                           \
    ZB_ZCL_MANUF_CODE_INVALID                             \
  ),                                                      \
  ZB_ZCL_CLUSTER_DESC(                                    \
    ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,                      \
    0,                                                    \
    NULL,                                                 \
    ZB_ZCL_CLUSTER_CLIENT_ROLE,                           \
    ZB_ZCL_MANUF_CODE_INVALID                             \
  )                                                       \
}

/**@brief Declare Color Dimmer Switch cluster attribute list.
 *
 * @param[IN] dev_ctx_name                          Name of the variable to store the device context in.
 * @param[IN] ciolor_dimmer_switch_cluster_list     Name of the list of clusters.
 */
#define ZB_DECLARE_COLOR_DIMMER_SWITCH_CLUSTER_ATTR_LIST(dev_ctx_name, color_dimmer_switch_cluster_list) \
    ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST(dev_ctx_name## _basic_attr_list,                                    \
                                     &dev_ctx_name.basic_attr.zcl_version,                               \
                                     &dev_ctx_name.basic_attr.power_source);                             \
    ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(dev_ctx_name## _identify_attr_list,                              \
                                        &dev_ctx_name.identify_attr.identify_time);                      \
    ZB_HA_DECLARE_COLOR_DIMMER_SWITCH_CLUSTER_LIST(color_dimmer_switch_cluster_list,                     \
                                                   dev_ctx_name## _basic_attr_list,                      \
                                                   dev_ctx_name## _identify_attr_list);

/* Zigbee Color Dimmer Switch cluster context. Stores all settings and static values. */
typedef struct
{
    zb_zcl_basic_attrs_ext_t    basic_attr;
    zb_zcl_identify_attrs_t     identify_attr;
} zb_color_dimmer_switch_dev_ctx_t;

#ifdef __cplusplus
}
#endif
#endif // ZIGBEE_COLOR_DIMMER_SWITCH_H__


/**
 * @}
 */
