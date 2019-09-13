#ifndef MAIN_H__
#define MAIN_H__


#include "zb_multi_sensor.h"

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


static sensor_device_ctx_t m_ms_dev_ctx;

ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_ms_attr_list, &m_ms_dev_ctx.identify_attr.identify_time);

ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_ms_attr_list,
                                     &m_ms_dev_ctx.basic_attr.zcl_version,
                                     &m_ms_dev_ctx.basic_attr.app_version,
                                     &m_ms_dev_ctx.basic_attr.stack_version,
                                     &m_ms_dev_ctx.basic_attr.hw_version,
                                     m_ms_dev_ctx.basic_attr.mf_name,
                                     m_ms_dev_ctx.basic_attr.model_id,
                                     m_ms_dev_ctx.basic_attr.date_code,
                                     &m_ms_dev_ctx.basic_attr.power_source,
                                     m_ms_dev_ctx.basic_attr.location_id,
                                     &m_ms_dev_ctx.basic_attr.ph_env,
                                     m_ms_dev_ctx.basic_attr.sw_ver);

ZB_ZCL_DECLARE_TEMP_MEASUREMENT_ATTRIB_LIST(temperature_ms_attr_list, 
                                            &m_ms_dev_ctx.temp_attr.measure_value,
                                            &m_ms_dev_ctx.temp_attr.min_measure_value, 
                                            &m_ms_dev_ctx.temp_attr.max_measure_value, 
                                            &m_ms_dev_ctx.temp_attr.tolerance);

ZB_ZCL_DECLARE_REL_HUMIDITY_MEASUREMENT_ATTRIB_LIST(humidity_ms_attr_list, 
                                            &m_ms_dev_ctx.humm_attr.measure_value,
                                            &m_ms_dev_ctx.humm_attr.min_measure_value, 
                                            &m_ms_dev_ctx.humm_attr.max_measure_value,
                                            &m_ms_dev_ctx.humm_attr.tolerance);

ZB_DECLARE_MULTI_SENSOR_CLUSTER_LIST(multi_sensor_clusters,
                                     basic_ms_attr_list,
                                     identify_ms_attr_list,
                                     temperature_ms_attr_list,
                                     humidity_ms_attr_list);

ZB_ZCL_DECLARE_MULTI_SENSOR_EP(multi_sensor_ep,
                               MULTI_SENSOR_ENDPOINT,
                               multi_sensor_clusters);

//ZBOSS_DECLARE_DEVICE_CTX_1_EP(multi_sensor_ctx, multi_sensor_ep);

#endif // MAIN_H__
