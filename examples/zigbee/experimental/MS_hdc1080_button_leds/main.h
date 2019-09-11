#ifndef MAIN_H__
#define MAIN_H__

// --------------------------------------------------------------------------------------------------
// По идее это должно быть добавлено в SDK - \external\zboss\include\zcl\zb_zcl_rel_humidity_measurement.h 

 /* (See: Table 4.13 Temperature Measurement Information Attribute Set) */

/** @brief Tolerance attribute minimum value */
#define ZB_ZCL_REL_HUMIDITY_MEASUREMENT_TOLERANCE_MIN_VALUE            0x0000

/** @brief Tolerance attribute maximum value */
#define ZB_ZCL_REL_HUMIDITY_MEASUREMENT_TOLERANCE_MAX_VALUE            0x2710

#define ZB_SET_ATTR_DESCR_WITH_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID(data_ptr) \
{                                                       \
  ZB_ZCL_ATTR_REL_HUMIDITY_TOLERANCE_ID,    \
  ZB_ZCL_ATTR_TYPE_U16,                                 \
  ZB_ZCL_ATTR_ACCESS_READ_ONLY,                         \
  (zb_voidp_t) data_ptr                                 \
}

#ifdef ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST
#undef ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST
#define ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(attr_list,          \
    value, min_value, max_value, tolerance)                                     \
  ZB_ZCL_START_DECLARE_ATTRIB_LIST(attr_list)                                   \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, (value))          \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID, (min_value))  \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID, (max_value))  \
  ZB_ZCL_SET_ATTR_DESC(ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_TOLERANCE_ID, (tolerance))  \
  ZB_ZCL_FINISH_DECLARE_ATTRIB_LIST
#endif // ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST

// -----------------------------------------------------------------------------------------------


static sensor_device_ctx_t m_dev_ctx;

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &m_dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list,
                                     &m_dev_ctx.basic_attr.zcl_version,
                                     &m_dev_ctx.basic_attr.app_version,
                                     &m_dev_ctx.basic_attr.stack_version,
                                     &m_dev_ctx.basic_attr.hw_version,
                                     m_dev_ctx.basic_attr.mf_name,
                                     m_dev_ctx.basic_attr.model_id,
                                     m_dev_ctx.basic_attr.date_code,
                                     &m_dev_ctx.basic_attr.power_source,
                                     m_dev_ctx.basic_attr.location_id,
                                     &m_dev_ctx.basic_attr.ph_env,
                                     m_dev_ctx.basic_attr.sw_ver);

ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(temperature_attr_list, 
                                            &m_dev_ctx.temp_attr.measure_value,
                                            &m_dev_ctx.temp_attr.min_measure_value, 
                                            &m_dev_ctx.temp_attr.max_measure_value, 
                                            &m_dev_ctx.temp_attr.tolerance);

ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(humidity_attr_list, 
                                            &m_dev_ctx.humm_attr.measure_value,
                                            &m_dev_ctx.humm_attr.min_measure_value, 
                                            &m_dev_ctx.humm_attr.max_measure_value,
                                            &m_dev_ctx.humm_attr.tolerance);

ZB_DECLARE_MULTI_SENSOR_CLUSTER_LIST(multi_sensor_clusters,
                                     basic_attr_list,
                                     identify_attr_list,
                                     temperature_attr_list,
                                     humidity_attr_list);

ZB_ZCL_DECLARE_MULTI_SENSOR_EP(multi_sensor_ep,
                               MULTI_SENSOR_ENDPOINT,
                               multi_sensor_clusters);



// --------- BULB --------------------------

#define HA_DIMMABLE_LIGHT_ENDPOINT        9                                    /**< Device endpoint, used to receive light controlling commands. */

/* Declare endpoint for Dimmable Light device with scenes. */
#define ZB_HA_DECLARE_LIGHT_EP(ep_name, ep_id, cluster_list)                         \
  ZB_ZCL_DECLARE_HA_DIMMABLE_LIGHT_SIMPLE_DESC(ep_name, ep_id,                       \
    ZB_HA_DIMMABLE_LIGHT_IN_CLUSTER_NUM, ZB_HA_DIMMABLE_LIGHT_OUT_CLUSTER_NUM);      \
  ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info## device_ctx_bulb_name,               \
                                     ZB_HA_DIMMABLE_LIGHT_REPORT_ATTR_COUNT);        \
  ZBOSS_DEVICE_DECLARE_LEVEL_CONTROL_CTX(cvc_alarm_info## device_ctx_bulb_name,           \
                                         ZB_HA_DIMMABLE_LIGHT_CVC_ATTR_COUNT);       \
  ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID,                   \
                              0,     \
                              NULL,                 \
                              ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t),\
                              cluster_list,                                          \
                              (zb_af_simple_desc_1_1_t*)&simple_desc_##ep_name,      \
                              ZB_HA_DIMMABLE_LIGHT_REPORT_ATTR_COUNT,                \
                              reporting_info## device_ctx_bulb_name,                      \
                              ZB_HA_DIMMABLE_LIGHT_CVC_ATTR_COUNT,                   \
                              cvc_alarm_info## device_ctx_bulb_name)

/* Main application customizable context. Stores all settings and static values. */
typedef struct
{
    // zb_zcl_basic_attrs_ext_t         basic_attr;
    // zb_zcl_identify_attrs_t          identify_attr;
    zb_zcl_scenes_attrs_t            scenes_attr;
    zb_zcl_groups_attrs_t            groups_attr;
    zb_zcl_on_off_attrs_ext_t        on_off_attr;
    zb_zcl_level_control_attrs_t     level_control_attr;
} bulb_device_ctx_t;


APP_PWM_INSTANCE(BULB_PWM_NAME, BULB_PWM_TIMER);
static bulb_device_ctx_t m_bulb_dev_ctx;

//ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &m_dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_GROUPS_ATTRIB_LIST(groups_attr_list, &m_bulb_dev_ctx.groups_attr.name_support);

ZB_ZCL_DECLARE_SCENES_ATTRIB_LIST(scenes_attr_list,
                                  &m_bulb_dev_ctx.scenes_attr.scene_count,
                                  &m_bulb_dev_ctx.scenes_attr.current_scene,
                                  &m_bulb_dev_ctx.scenes_attr.current_group,
                                  &m_bulb_dev_ctx.scenes_attr.scene_valid,
                                  &m_bulb_dev_ctx.scenes_attr.name_support);

// ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_attr_list,
//                                      &m_bulb_dev_ctx.basic_attr.zcl_version,
//                                      &m_bulb_dev_ctx.basic_attr.app_version,
//                                      &m_bulb_dev_ctx.basic_attr.stack_version,
//                                      &m_bulb_dev_ctx.basic_attr.hw_version,
//                                      m_bulb_dev_ctx.basic_attr.mf_name,
//                                      m_bulb_dev_ctx.basic_attr.model_id,
//                                      m_bulb_dev_ctx.basic_attr.date_code,
//                                      &m_bulb_dev_ctx.basic_attr.power_source,
//                                      m_bulb_dev_ctx.basic_attr.location_id,
//                                      &m_bulb_dev_ctx.basic_attr.ph_env,
//                                      m_bulb_dev_ctx.basic_attr.sw_ver);

/* On/Off cluster attributes additions data */
ZB_ZCL_DECLARE_ON_OFF_ATTRIB_LIST_EXT(on_off_attr_list,
                                      &m_bulb_dev_ctx.on_off_attr.on_off,
                                      &m_bulb_dev_ctx.on_off_attr.global_scene_ctrl,
                                      &m_bulb_dev_ctx.on_off_attr.on_time,
                                      &m_bulb_dev_ctx.on_off_attr.off_wait_time);

ZB_ZCL_DECLARE_LEVEL_CONTROL_ATTRIB_LIST(level_control_attr_list,
                                         &m_bulb_dev_ctx.level_control_attr.current_level,
                                         &m_bulb_dev_ctx.level_control_attr.remaining_time);

ZB_HA_DECLARE_DIMMABLE_LIGHT_CLUSTER_LIST(dimmable_light_clusters,
                                          basic_attr_list,
                                          identify_attr_list,
                                          groups_attr_list,
                                          scenes_attr_list,
                                          on_off_attr_list,
                                          level_control_attr_list);

ZB_HA_DECLARE_LIGHT_EP(dimmable_light_ep,
                       HA_DIMMABLE_LIGHT_ENDPOINT,
                       dimmable_light_clusters);

// ZB_HA_DECLARE_DIMMABLE_LIGHT_CTX(dimmable_light_ctx,
//                                  dimmable_light_ep);

#if (APP_BULB_USE_WS2812_LED_CHAIN)
/**@brief Timer responsible for triggering periodic led chain refresh */
APP_TIMER_DEF(m_ws2812_refresh_timer);

/** @brief Requests a led chain refresh */
static volatile bool m_ws2812_refresh_request;
#endif

ZBOSS_DECLARE_DEVICE_CTX_2_EP(multi_sensor_ctx, multi_sensor_ep, dimmable_light_ep);

#endif // MAIN_H__
