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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy zb_ha_dimmable_color_light.h
 * @{
 * @ingroup zigbee_examples
 */
#ifndef ZB_HA_COLOR_DIMMABLE_LIGHT_H
#define ZB_HA_COLOR_DIMMABLE_LIGHT_H 1

#include "zboss_api_addons.h"

#define ZB_HA_COLOR_DIMMABLE_LIGHT_VERSION      0                                   /**< Color light device version. */
#define ZB_HA_COLOR_CONTROL_IN_CLUSTER_NUM      7                                   /**< Color light input clusters number. */
#define ZB_HA_COLOR_CONTROL_OUT_CLUSTER_NUM     0                                   /**< Color light output clusters number. */

#define ZB_ZCL_COLOR_DIMMABLE_LIGHT_CVC_ATTR_COUNT (ZB_HA_DIMMABLE_LIGHT_CVC_ATTR_COUNT + 3)

/**@brief Declare cluster list for Color light device.
 *
 * @param[IN] cluster_list_name [IN]             cluster list variable name.
 * @param[IN] basic_attr_list [IN]               attribute list for Basic cluster.
 * @param[IN] identify_attr_list [IN]            attribute list for Identify cluster.
 * @param[IN] groups_attr_list [IN]              attribute list for Groups cluster.
 * @param[IN] scenes_attr_list [IN]              attribute list for Scenes cluster.
 * @param[IN] on_off_attr_list [IN]              attribute list for On/Off cluster.
 * @param[IN] level_control_attr_list [IN]       attribute list for Level Control cluster.
 * @param[IN] color_control_attr_list [IN]       attribute list for Color Control cluster.
 */
#define ZB_HA_DECLARE_COLOR_DIMMABLE_LIGHT_CLUSTER_LIST(                                        \
    cluster_list_name,                                                                          \
    basic_attr_list,                                                                            \
    identify_attr_list,                                                                         \
    groups_attr_list,                                                                           \
    scenes_attr_list,                                                                           \
    on_off_attr_list,                                                                           \
    level_control_attr_list,                                                                    \
    color_control_attr_list)                                                                    \
    zb_zcl_cluster_desc_t cluster_list_name[] =                                                 \
    {                                                                                           \
        ZB_ZCL_CLUSTER_DESC(                                                                    \
        ZB_ZCL_CLUSTER_ID_BASIC,                                                                \
        ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t),                                      \
        (basic_attr_list),                                                                      \
        ZB_ZCL_CLUSTER_SERVER_ROLE,                                                             \
        ZB_ZCL_MANUF_CODE_INVALID                                                               \
        ),                                                                                      \
        ZB_ZCL_CLUSTER_DESC(                                                                    \
        ZB_ZCL_CLUSTER_ID_IDENTIFY,                                                             \
        ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t),                                   \
        (identify_attr_list),                                                                   \
        ZB_ZCL_CLUSTER_SERVER_ROLE,                                                             \
        ZB_ZCL_MANUF_CODE_INVALID                                                               \
        ),                                                                                      \
        ZB_ZCL_CLUSTER_DESC(                                                                    \
        ZB_ZCL_CLUSTER_ID_GROUPS,                                                               \
        ZB_ZCL_ARRAY_SIZE(groups_attr_list, zb_zcl_attr_t),                                     \
        (groups_attr_list),                                                                     \
        ZB_ZCL_CLUSTER_SERVER_ROLE,                                                             \
        ZB_ZCL_MANUF_CODE_INVALID                                                               \
        ),                                                                                      \
        ZB_ZCL_CLUSTER_DESC(                                                                    \
        ZB_ZCL_CLUSTER_ID_SCENES,                                                               \
        ZB_ZCL_ARRAY_SIZE(scenes_attr_list, zb_zcl_attr_t),                                     \
        (scenes_attr_list),                                                                     \
        ZB_ZCL_CLUSTER_SERVER_ROLE,                                                             \
        ZB_ZCL_MANUF_CODE_INVALID                                                               \
        ),                                                                                      \
        ZB_ZCL_CLUSTER_DESC(                                                                    \
        ZB_ZCL_CLUSTER_ID_ON_OFF,                                                               \
        ZB_ZCL_ARRAY_SIZE(on_off_attr_list, zb_zcl_attr_t),                                     \
        (on_off_attr_list),                                                                     \
        ZB_ZCL_CLUSTER_SERVER_ROLE,                                                             \
        ZB_ZCL_MANUF_CODE_INVALID                                                               \
        ),                                                                                      \
        ZB_ZCL_CLUSTER_DESC(                                                                    \
        ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,                                                        \
        ZB_ZCL_ARRAY_SIZE(level_control_attr_list, zb_zcl_attr_t),                              \
        (level_control_attr_list),                                                              \
        ZB_ZCL_CLUSTER_SERVER_ROLE,                                                             \
        ZB_ZCL_MANUF_CODE_INVALID                                                               \
        ),                                                                                      \
        ZB_ZCL_CLUSTER_DESC(                                                                    \
        ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,                                                        \
        ZB_ZCL_ARRAY_SIZE(color_control_attr_list, zb_zcl_attr_t),                              \
        (color_control_attr_list),                                                              \
        ZB_ZCL_CLUSTER_SERVER_ROLE,                                                             \
        ZB_ZCL_MANUF_CODE_INVALID                                                               \
        )                                                                                       \
    }

/**@brief Declare Color Dimmable Light simple descriptor.
 *
 * @param[IN] ep_name                endpoint variable name.
 * @param[IN] ep_id [IN]             endpoint ID.
 * @param[IN] in_clust_num           number of supported input clusters.
 * @param[IN] out_clust_num          number of supported output clusters.
 *
 * @note in_clust_num, out_clust_num should be defined by numeric constants, not variables or any
 * definitions, because these values are used to form simple descriptor type name.
 */
#define ZB_ZCL_DECLARE_COLOR_DIMMABLE_LIGHT_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num)             \
    ZB_DECLARE_SIMPLE_DESC_VA(in_clust_num, out_clust_num, ep_name);                            \
    ZB_AF_SIMPLE_DESC_TYPE_VA(in_clust_num, out_clust_num, ep_name)  simple_desc_## ep_name =   \
    {                                                                                           \
        ep_id,                                                                                  \
        ZB_AF_HA_PROFILE_ID,                                                                    \
        ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,                                                   \
        ZB_HA_COLOR_DIMMABLE_LIGHT_VERSION,                                                     \
        0,                                                                                      \
        in_clust_num,                                                                           \
        out_clust_num,                                                                          \
        {                                                                                       \
          ZB_ZCL_CLUSTER_ID_BASIC,                                                              \
          ZB_ZCL_CLUSTER_ID_IDENTIFY,                                                           \
          ZB_ZCL_CLUSTER_ID_GROUPS,                                                             \
          ZB_ZCL_CLUSTER_ID_SCENES,                                                             \
          ZB_ZCL_CLUSTER_ID_ON_OFF,                                                             \
          ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,                                                      \
          ZB_ZCL_CLUSTER_ID_COLOR_CONTROL                                                       \
        }                                                                                       \
    }

/**@brief Declare Color Dimmable Light endpoint.
 *
 * @param[IN] ep_name                endpoint variable name.
 * @param[IN] ep_id [IN]             endpoint ID.
 * @param[IN] cluster_list [IN]      list of endpoint clusters
 */
#define ZB_ZCL_DECLARE_COLOR_DIMMABLE_LIGHT_EP(ep_name, ep_id, cluster_list)        \
    ZB_ZCL_DECLARE_COLOR_DIMMABLE_LIGHT_SIMPLE_DESC(                                \
        ep_name,                                                                    \
        ep_id,                                                                      \
        ZB_HA_COLOR_CONTROL_IN_CLUSTER_NUM,                                         \
        ZB_HA_COLOR_CONTROL_OUT_CLUSTER_NUM);                                       \
    ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info## ep_name,                    \
        (ZB_ZCL_COLOR_CONTROL_REPORT_ATTR_COUNT));                                  \
    ZBOSS_DEVICE_DECLARE_LEVEL_CONTROL_CTX(cvc_alarm_info## ep_name,                \
        ZB_ZCL_COLOR_DIMMABLE_LIGHT_CVC_ATTR_COUNT);                                \
    ZB_AF_DECLARE_ENDPOINT_DESC(                                                    \
        ep_name,                                                                    \
        ep_id,                                                                      \
        ZB_AF_HA_PROFILE_ID,                                                        \
        0,                                                                          \
        NULL,                                                                       \
        ZB_ZCL_ARRAY_SIZE(                                                          \
            cluster_list,                                                           \
            zb_zcl_cluster_desc_t),                                                 \
        cluster_list,                                                               \
        (zb_af_simple_desc_1_1_t*)&simple_desc_## ep_name,                          \
        ZB_ZCL_COLOR_CONTROL_REPORT_ATTR_COUNT,                                     \
        reporting_info## ep_name,                                                   \
        ZB_ZCL_COLOR_DIMMABLE_LIGHT_CVC_ATTR_COUNT,                                 \
        cvc_alarm_info## ep_name)

#endif /* ZB_HA_COLOR_DIMMABLE_LIGHT_H */

/** @} */
