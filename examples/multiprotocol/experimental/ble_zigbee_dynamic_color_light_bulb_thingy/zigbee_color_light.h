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
 * @defgroup zigbee_examples_ble_zigbee_color_light_bulb_thingy zigbee_color_light.h
 * @{
 * @ingroup zigbee_examples
 */

#ifndef ZIGBEE_COLOR_LIGHT_H__
#define ZIGBEE_COLOR_LIGHT_H__

#include <stdint.h>
#include "zboss_api_addons.h"
#include "rgb_led.h"
#include "zb_ha_dimmable_color_light.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Declares attribute list for Level Control cluster, defined as variadic macro.
 *
 * @param[IN] attr_list          Attribure list name.
 * @param[IN] current_level      Pointer to variable to store the current_level attribute value.
 * @param[IN] remaining_time     Pointer to variable to store the remaining_time attribute value.
 * @param[IN] ...                Optional argument to concatenate to the variable name.
 */
#define ZB_ZCL_DECLARE_LEVEL_CONTROL_ATTRIB_LIST_VA(attr_list, current_level, remaining_time, ...)  \
  zb_zcl_level_control_move_status_t move_status_data_ctx## __VA_ARGS__## _attr_list ;              \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST(attr_list)                                                       \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, (current_level))                 \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_LEVEL_CONTROL_REMAINING_TIME_ID, (remaining_time))               \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_LEVEL_CONTROL_MOVE_STATUS_ID,                                    \
                       (&(move_status_data_ctx## __VA_ARGS__## _attr_list)))                        \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST

/**@brief Declares color light bulb cluster attribute list.
 *
 * @param[IN] dev_ctx_name                       Name of the variable to store the device contex in.
 * @param[IN] color_light_bulb_cluster_list      Name of the list of clusters.
 */
#define ZB_DECLARE_COLOR_CONTROL_CLUSTER_ATTR_LIST(dev_ctx_name, color_light_bulb_cluster_list)                                                  \
    ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(dev_ctx_name## _identify_attr_list, &dev_ctx_name.identify_attr.identify_time);                          \
    ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(dev_ctx_name## _groups_attr_list, &dev_ctx_name.groups_attr.name_support);                                 \
    ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST(dev_ctx_name## _scenes_attr_list,                                                                          \
                                      &dev_ctx_name.scenes_attr.scene_count,                                                                     \
                                      &dev_ctx_name.scenes_attr.current_scene,                                                                   \
                                      &dev_ctx_name.scenes_attr.current_group,                                                                   \
                                      &dev_ctx_name.scenes_attr.scene_valid,                                                                     \
                                      &dev_ctx_name.scenes_attr.name_support);                                                                   \
    ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(dev_ctx_name## _basic_attr_list,                                                                        \
                                         &dev_ctx_name.basic_attr.zcl_version,                                                                   \
                                         &dev_ctx_name.basic_attr.app_version,                                                                   \
                                         &dev_ctx_name.basic_attr.stack_version,                                                                 \
                                         &dev_ctx_name.basic_attr.hw_version,                                                                    \
                                         dev_ctx_name.basic_attr.mf_name,                                                                        \
                                         dev_ctx_name.basic_attr.model_id,                                                                       \
                                         dev_ctx_name.basic_attr.date_code,                                                                      \
                                         &dev_ctx_name.basic_attr.power_source,                                                                  \
                                         dev_ctx_name.basic_attr.location_id,                                                                    \
                                         &dev_ctx_name.basic_attr.ph_env,                                                                        \
                                         dev_ctx_name.basic_attr.sw_ver);                                                                        \
    ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST_EXT(dev_ctx_name## _on_off_attr_list,                                                                      \
                                          &dev_ctx_name.on_off_attr.on_off,                                                                      \
                                          &dev_ctx_name.on_off_attr.global_scene_ctrl,                                                           \
                                          &dev_ctx_name.on_off_attr.on_time,                                                                     \
                                          &dev_ctx_name.on_off_attr.off_wait_time);                                                              \
    ZB_ZCL_DECLARE_LEVEL_CONTROL_ATTRIB_LIST_VA(dev_ctx_name## _level_control_attr_list,                                                         \
                                              &dev_ctx_name.level_control_attr.current_level,                                                    \
                                              &dev_ctx_name.level_control_attr.remaining_time,                                                   \
                                              dev_ctx_name);                                                                                     \
    ZB_ZCL_DECLARE_COLOR_CONTROL_ATTRIB_LIST_EXT(dev_ctx_name## _color_control_attr_list,                                                        \
                                                 &dev_ctx_name.color_control_attr.set_color_info.current_hue,                                    \
                                                 &dev_ctx_name.color_control_attr.set_color_info.current_saturation,                             \
                                                 &dev_ctx_name.color_control_attr.set_color_info.remaining_time,                                 \
                                                 &dev_ctx_name.color_control_attr.set_color_info.current_X,                                      \
                                                 &dev_ctx_name.color_control_attr.set_color_info.current_Y,                                      \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_temperature,                              \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_mode,                                     \
                                                 &dev_ctx_name.color_control_attr.set_color_info.options,                                        \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.number_primaries,                   \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_1_X,                        \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_1_Y,                        \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_1_intensity,                \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_2_X,                        \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_2_Y,                        \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_2_intensity,                \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_3_X,                        \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_3_Y,                        \
                                                 &dev_ctx_name.color_control_attr.set_defined_primaries_info.primary_3_intensity,                \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_4_X,             \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_4_Y,             \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_4_intensity,     \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_5_X,             \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_5_Y,             \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_5_intensity,     \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_6_X,             \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_6_Y,             \
                                                 &dev_ctx_name.color_control_attr.set_additional_defined_primaries_info.primary_6_intensity,     \
                                                 &dev_ctx_name.color_control_attr.set_color_info.enhanced_current_hue,                           \
                                                 &dev_ctx_name.color_control_attr.set_color_info.enhanced_color_mode,                            \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_loop_active,                              \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_loop_direction,                           \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_loop_time,                                \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_loop_start_enhanced_hue,                  \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_loop_stored_enhanced_hue,                 \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_capabilities,                             \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_temp_physical_min_mireds,                 \
                                                 &dev_ctx_name.color_control_attr.set_color_info.color_temp_physical_max_mireds,                 \
                                                 &dev_ctx_name.color_control_attr.set_color_info.couple_color_temp_to_level_min_mireds,          \
                                                 &dev_ctx_name.color_control_attr.set_color_info.start_up_color_temp_mireds);                    \
    ZB_HA_DECLARE_COLOR_DIMMABLE_LIGHT_CLUSTER_LIST(color_light_bulb_cluster_list,                                                               \
                                                    dev_ctx_name## _basic_attr_list,                                                             \
                                                    dev_ctx_name## _identify_attr_list,                                                          \
                                                    dev_ctx_name## _groups_attr_list,                                                            \
                                                    dev_ctx_name## _scenes_attr_list,                                                            \
                                                    dev_ctx_name## _on_off_attr_list,                                                            \
                                                    dev_ctx_name## _level_control_attr_list,                                                     \
                                                    dev_ctx_name## _color_control_attr_list);

/* Zigbee color light device context. Stores all settings and static values. */
typedef struct
{
    led_params_t                led_params;             /**< Table to store RGB color values to control the LED on Thingy. */
    uint8_t                     ep_id;                  /**< Endpoint ID. */
    uint8_t                     value_unstable: 1;      /**< Variable used as flag when detecting changing value in Level Control attribute. */
    uint8_t                     value_debounce_time: 7; /**< Value in ms for debounce level change. */
    uint8_t                     prev_lvl_ctrl_value;    /**< Variable used to store the previous attribute value when detecting changing value in Level Control attribute. */

    zb_zcl_basic_attrs_ext_t    basic_attr;
    zb_zcl_identify_attrs_t     identify_attr;
    zb_zcl_scenes_attrs_t       scenes_attr;
    zb_zcl_groups_attrs_t       groups_attr;
    zb_zcl_on_off_attrs_ext_t   on_off_attr;
    zb_zcl_level_control_attrs_t level_control_attr;
    zb_zcl_color_control_attrs_t color_control_attr;
} zb_color_light_ctx_t;

/**@brief Initialize module.
 */
void zb_color_light_init(void);

/**
 * @brief Initializes color light context object.
 *
 * @param[in] p_light_ctx A pointer to light context object.
 * @param[in] ep_id       Endpoint ID
 * @param[in] identify_cb A callback which should be called upon Identify Request.
 */
void zb_color_light_init_ctx(zb_color_light_ctx_t * p_light_ctx,
                             uint8_t                ep_id,
                             zb_callback_t          identify_cb);

/**@brief Does Identify effect on color light object.
 *
 * @param[in] p_light_ctx  A pointer to light context object.
 * @param[in] effect       Identify effect to play.
 *
 * @return RET_OK on success or error code on failure.
 */
zb_ret_t zb_color_light_do_identify_effect(zb_color_light_ctx_t * p_light_ctx,
                                           zb_uint8_t             effect);

/**@brief Sets color light attribute.
 *
 * This function sets physical property of the light according to the provided
 * attribute value.
 *
 * @param[in] p_light_ctx  Pointer to light context object.
 * @param[in] p_savp       Pointer to attribute value.
 *
 * @return RET_OK on success or error code on failure.
 */
zb_ret_t zb_color_light_set_attribute(zb_color_light_ctx_t          * p_light_ctx,
                                      zb_zcl_set_attr_value_param_t * p_savp);

/**@brief Sets color light brightness.
 *
 * @param[in] p_light_ctx  Pointer to light context object.
 * @param[in] value        Brightness level.
 *
 * @return RET_OK on success or error code on failure.
 */
zb_ret_t zb_color_light_set_level(zb_color_light_ctx_t * p_light_ctx,
                                  zb_uint8_t             value);

#ifdef __cplusplus
}
#endif
#endif // ZIGBEE_COLOR_LIGHT_H__


/**
 * @}
 */
