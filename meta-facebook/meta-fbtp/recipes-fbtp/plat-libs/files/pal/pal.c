/*
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This file contains code to support IPMI2.0 Specificaton available @
 * http://www.intel.com/content/www/us/en/servers/ipmi/ipmi-specifications.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "pal.h"
#include <openbmc/vr.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/stat.h>
#include <openbmc/gpio.h>

#define BIT(value, index) ((value >> index) & 1)

#define FBTP_PLATFORM_NAME "fbtp"
#define LAST_KEY "last_key"
#define FBTP_MAX_NUM_SLOTS 1
#define GPIO_VAL "/sys/class/gpio/gpio%d/value"
#define GPIO_DIR "/sys/class/gpio/gpio%d/direction"

#define GPIO_POWER 35
#define GPIO_POWER_GOOD 14
#define GPIO_POWER_LED 210
#define GPIO_POWER_RESET 33

#define GPIO_RST_BTN 144
#define GPIO_PWR_BTN 24

#define GPIO_HB_LED 165

#define GPIO_POSTCODE_0 48
#define GPIO_POSTCODE_1 49
#define GPIO_POSTCODE_2 50
#define GPIO_POSTCODE_3 51
#define GPIO_POSTCODE_4 124
#define GPIO_POSTCODE_5 125
#define GPIO_POSTCODE_6 126
#define GPIO_POSTCODE_7 127

#define GPIO_DBG_CARD_PRSNT 134

#define GPIO_BMC_READY_N  28

#define GPIO_BAT_SENSE_EN_N 46

#define GPIO_BOARD_SKU_ID0 120
#define GPIO_BOARD_SKU_ID1 121
#define GPIO_BOARD_SKU_ID2 122
#define GPIO_BOARD_SKU_ID3 123
#define GPIO_BOARD_SKU_ID4 124
#define GPIO_MB_SLOT_ID0 125
#define GPIO_MB_SLOT_ID1 126
#define GPIO_MB_SLOT_ID2 127
#define GPIO_BOARD_REV_ID0 25
#define GPIO_BOARD_REV_ID1 27
#define GPIO_BOARD_REV_ID2 29
#define GPIO_SLT_CFG0 142
#define GPIO_SLT_CFG1 143
#define GPIO_FM_CPU_CATERR_LVT3_N 49
#define GPIO_FM_CPU_MSMI_LVT3_N 107
#define GPIO_FM_CPU0_SKTOCC_LVT3_N 51
#define GPIO_FM_CPU1_SKTOCC_LVT3_N 208
#define GPIO_FM_BIOS_POST_CMPLT_N 215
#define GPIO_FM_SLPS4_N 193

#define PAGE_SIZE  0x1000
#define AST_SCU_BASE 0x1e6e2000
#define PIN_CTRL1_OFFSET 0x80
#define PIN_CTRL2_OFFSET 0x84
#define RST_STS_OFFSET 0x3c

#define AST_LPC_BASE 0x1e789000
#define HICRA_OFFSET 0x9C
#define HICRA_MASK_UART1 0x70000
#define HICRA_MASK_UART2 0x380000
#define HICRA_MASK_UART3 0x1C00000
#define UART1_TO_UART3 0x5
#define UART2_TO_UART3 0x6
#define UART3_TO_UART1 0x5
#define UART3_TO_UART2 0x4
#define DEBUG_TO_UART1 0x0

#define UART1_TXD (1 << 22)
#define UART2_TXD (1 << 30)
#define UART3_TXD (1 << 22)
#define UART4_TXD (1 << 30)

#define DELAY_GRACEFUL_SHUTDOWN 1
#define DELAY_POWER_OFF 6
#define DELAY_POWER_CYCLE 10

#define CRASHDUMP_BIN       "/usr/local/bin/dump.sh"
#define CRASHDUMP_FILE      "/mnt/data/crashdump_"

#define LARGEST_DEVICE_NAME 120
#define PWM_DIR "/sys/devices/platform/ast_pwm_tacho.0"
#define PWM_UNIT_MAX 96

#define FAN0_TACH_INPUT 0
#define FAN1_TACH_INPUT 2

#define TACH_DIR "/sys/devices/platform/ast_pwm_tacho.0"
#define ADC_DIR "/sys/devices/platform/ast_adc.0"

#define MB_INLET_TEMP_DEVICE "/sys/devices/platform/ast-i2c.6/i2c-6/6-004e/hwmon/hwmon*"
#define MB_OUTLET_TEMP_DEVICE "/sys/devices/platform/ast-i2c.6/i2c-6/6-004f/hwmon/hwmon*"
#define MEZZ_TEMP_DEVICE "/sys/devices/platform/ast-i2c.8/i2c-8/8-001f/hwmon/hwmon*"
#define HSC_DEVICE "/sys/devices/platform/ast-i2c.7/i2c-7/7-0011/hwmon/hwmon*"

#define FAN_TACH_RPM "tacho%d_rpm"
#define ADC_VALUE "adc%d_value"
#define HSC_IN_VOLT "in1_input"
#define HSC_OUT_CURR "curr1_input"
#define HSC_TEMP "temp1_input"

#define UNIT_DIV 1000

#define MAX_SENSOR_NUM 0xFF
#define ALL_BYTES 0xFF
#define LAST_REC_ID 0xFFFF

#define RISER_BUS_ID 0x1

#define GUID_SIZE 16
#define OFFSET_SYS_GUID 0x17F0
#define OFFSET_DEV_GUID 0x1800
#define FRU_EEPROM "/sys/devices/platform/ast-i2c.6/i2c-6/6-0054/eeprom"

#define READING_NA -2
#define READING_SKIP 1

#define NIC_MAX_TEMP 125
#define PLAT_ID_SKU_MASK 0x10 // BIT4: 0- Single Side, 1- Double Side

#define MAX_READ_RETRY 10
#define POST_CODE_FILE       "/sys/devices/platform/ast-snoop-dma.0/data_history"

#define CPLD_BUS_ID 0x6
#define CPLD_ADDR 0xA0

static uint8_t gpio_rst_btn[] = { 0, GPIO_POWER_RESET};
const static uint8_t gpio_id_led[] = { 0, 41, 40, 43, 42 };
const static uint8_t gpio_prsnt[] = { 0, 61, 60, 63, 62 };
const char pal_fru_list[] = "all, mb, nic";
const char pal_server_list[] = "mb";

size_t pal_pwm_cnt = 2;
size_t pal_tach_cnt = 2;
const char pal_pwm_list[] = "0, 1";
const char pal_tach_list[] = "0, 1";

static uint8_t g_plat_id = 0x0;

static int key_func_por_policy (int event, void *arg);
static int key_func_lps (int event, void *arg);
static int key_func_ntp (int event, void *arg);
static int key_func_tz (int event, void *arg);

static uint8_t power_fail_log = 0;

enum key_event {
  KEY_BEFORE_SET,
  KEY_AFTER_INI,
};

struct pal_key_cfg {
  char *name;
  char *def_val;
  int (*function)(int, void*);
} key_cfg[] = {
  /* name, default value, function */
  {"pwr_server_last_state", "on", key_func_lps},
  {"sysfw_ver_server", "0", NULL},
  {"identify_sled", "off", NULL},
  {"timestamp_sled", "0", NULL},
  {"server_por_cfg", "lps", key_func_por_policy},
  {"server_sensor_health", "1", NULL},
  {"nic_sensor_health", "1", NULL},
  {"server_sel_error", "1", NULL},
  {"server_boot_order", "0000000", NULL},
  {"ntp_server", "", key_func_ntp},
  {"time_zone", "UTC", key_func_tz},
  /* Add more Keys here */
  {LAST_KEY, LAST_KEY, NULL} /* This is the last key of the list */
};

// List of MB sensors to be monitored
const uint8_t mb_sensor_list[] = {
  MB_SENSOR_INLET_TEMP,
  MB_SENSOR_OUTLET_TEMP,
  MB_SENSOR_INLET_REMOTE_TEMP,
  MB_SENSOR_OUTLET_REMOTE_TEMP,
  MB_SENSOR_FAN0_TACH,
  MB_SENSOR_FAN1_TACH,
  MB_SENSOR_P3V3,
  MB_SENSOR_P5V,
  MB_SENSOR_P12V,
  MB_SENSOR_P1V05,
  MB_SENSOR_PVNN_PCH_STBY,
  MB_SENSOR_P3V3_STBY,
  MB_SENSOR_P5V_STBY,
  MB_SENSOR_P3V_BAT,
  MB_SENSOR_HSC_IN_VOLT,
  MB_SENSOR_HSC_OUT_CURR,
  MB_SENSOR_HSC_IN_POWER,
  MB_SENSOR_CPU0_TEMP,
  MB_SENSOR_CPU0_TJMAX,
  MB_SENSOR_CPU0_PKG_POWER,
  MB_SENSOR_CPU1_TEMP,
  MB_SENSOR_CPU1_TJMAX,
  MB_SENSOR_CPU1_PKG_POWER,
  MB_SENSOR_PCH_TEMP,
  MB_SENSOR_CPU0_DIMM_GRPA_TEMP,
  MB_SENSOR_CPU0_DIMM_GRPB_TEMP,
  MB_SENSOR_CPU1_DIMM_GRPC_TEMP,
  MB_SENSOR_CPU1_DIMM_GRPD_TEMP,
  MB_SENSOR_VR_CPU0_VCCIN_TEMP,
  MB_SENSOR_VR_CPU0_VCCIN_CURR,
  MB_SENSOR_VR_CPU0_VCCIN_VOLT,
  MB_SENSOR_VR_CPU0_VCCIN_POWER,
  MB_SENSOR_VR_CPU0_VSA_TEMP,
  MB_SENSOR_VR_CPU0_VSA_CURR,
  MB_SENSOR_VR_CPU0_VSA_VOLT,
  MB_SENSOR_VR_CPU0_VSA_POWER,
  MB_SENSOR_VR_CPU0_VCCIO_TEMP,
  MB_SENSOR_VR_CPU0_VCCIO_CURR,
  MB_SENSOR_VR_CPU0_VCCIO_VOLT,
  MB_SENSOR_VR_CPU0_VCCIO_POWER,
  MB_SENSOR_VR_CPU0_VDDQ_GRPA_TEMP,
  MB_SENSOR_VR_CPU0_VDDQ_GRPA_CURR,
  MB_SENSOR_VR_CPU0_VDDQ_GRPA_VOLT,
  MB_SENSOR_VR_CPU0_VDDQ_GRPA_POWER,
  MB_SENSOR_VR_CPU0_VDDQ_GRPB_TEMP,
  MB_SENSOR_VR_CPU0_VDDQ_GRPB_CURR,
  MB_SENSOR_VR_CPU0_VDDQ_GRPB_VOLT,
  MB_SENSOR_VR_CPU0_VDDQ_GRPB_POWER,
  MB_SENSOR_VR_CPU1_VCCIN_TEMP,
  MB_SENSOR_VR_CPU1_VCCIN_CURR,
  MB_SENSOR_VR_CPU1_VCCIN_VOLT,
  MB_SENSOR_VR_CPU1_VCCIN_POWER,
  MB_SENSOR_VR_CPU1_VSA_TEMP,
  MB_SENSOR_VR_CPU1_VSA_CURR,
  MB_SENSOR_VR_CPU1_VSA_VOLT,
  MB_SENSOR_VR_CPU1_VSA_POWER,
  MB_SENSOR_VR_CPU1_VCCIO_TEMP,
  MB_SENSOR_VR_CPU1_VCCIO_CURR,
  MB_SENSOR_VR_CPU1_VCCIO_VOLT,
  MB_SENSOR_VR_CPU1_VCCIO_POWER,
  MB_SENSOR_VR_CPU1_VDDQ_GRPC_TEMP,
  MB_SENSOR_VR_CPU1_VDDQ_GRPC_CURR,
  MB_SENSOR_VR_CPU1_VDDQ_GRPC_VOLT,
  MB_SENSOR_VR_CPU1_VDDQ_GRPC_POWER,
  MB_SENSOR_VR_CPU1_VDDQ_GRPD_TEMP,
  MB_SENSOR_VR_CPU1_VDDQ_GRPD_CURR,
  MB_SENSOR_VR_CPU1_VDDQ_GRPD_VOLT,
  MB_SENSOR_VR_CPU1_VDDQ_GRPD_POWER,
  MB_SENSOR_VR_PCH_PVNN_TEMP,
  MB_SENSOR_VR_PCH_PVNN_CURR,
  MB_SENSOR_VR_PCH_PVNN_VOLT,
  MB_SENSOR_VR_PCH_PVNN_POWER,
  MB_SENSOR_VR_PCH_P1V05_TEMP,
  MB_SENSOR_VR_PCH_P1V05_CURR,
  MB_SENSOR_VR_PCH_P1V05_VOLT,
  MB_SENSOR_VR_PCH_P1V05_POWER,
  MB_SENSOR_C2_AVA_FTEMP,
  MB_SENSOR_C2_AVA_RTEMP,
  MB_SENSOR_C2_1_NVME_CTEMP,
  MB_SENSOR_C2_2_NVME_CTEMP,
  MB_SENSOR_C2_3_NVME_CTEMP,
  MB_SENSOR_C2_4_NVME_CTEMP,
  MB_SENSOR_C3_AVA_FTEMP,
  MB_SENSOR_C3_AVA_RTEMP,
  MB_SENSOR_C3_1_NVME_CTEMP,
  MB_SENSOR_C3_2_NVME_CTEMP,
  MB_SENSOR_C3_3_NVME_CTEMP,
  MB_SENSOR_C3_4_NVME_CTEMP,
  MB_SENSOR_C4_AVA_FTEMP,
  MB_SENSOR_C4_AVA_RTEMP,
  MB_SENSOR_C4_1_NVME_CTEMP,
  MB_SENSOR_C4_2_NVME_CTEMP,
  MB_SENSOR_C4_3_NVME_CTEMP,
  MB_SENSOR_C4_4_NVME_CTEMP,
  MB_SENSOR_C2_P12V_INA230_VOL,
  MB_SENSOR_C2_P12V_INA230_CURR,
  MB_SENSOR_C2_P12V_INA230_PWR,
  MB_SENSOR_C3_P12V_INA230_VOL,
  MB_SENSOR_C3_P12V_INA230_CURR,
  MB_SENSOR_C3_P12V_INA230_PWR,
  MB_SENSOR_C4_P12V_INA230_VOL,
  MB_SENSOR_C4_P12V_INA230_CURR,
  MB_SENSOR_C4_P12V_INA230_PWR,
  MB_SENSOR_CONN_P12V_INA230_VOL,
  MB_SENSOR_CONN_P12V_INA230_CURR,
  MB_SENSOR_CONN_P12V_INA230_PWR,
};

// List of NIC sensors to be monitored
const uint8_t nic_sensor_list[] = {
  MEZZ_SENSOR_TEMP,
};

// List of MB discrete sensors to be monitored
const uint8_t mb_discrete_sensor_list[] = {
  MB_SENSOR_POWER_FAIL,
  MB_SENSOR_MEMORY_LOOP_FAIL,
  MB_SENSOR_PROCESSOR_FAIL,
};

float mb_sensor_threshold[MAX_SENSOR_NUM][MAX_SENSOR_THRESHOLD + 1] = {0};
float nic_sensor_threshold[MAX_SENSOR_NUM][MAX_SENSOR_THRESHOLD + 1] = {0};

size_t mb_sensor_cnt = sizeof(mb_sensor_list)/sizeof(uint8_t);
size_t nic_sensor_cnt = sizeof(nic_sensor_list)/sizeof(uint8_t);
size_t mb_discrete_sensor_cnt = sizeof(mb_discrete_sensor_list)/sizeof(uint8_t);

uint8_t g_sys_guid[GUID_SIZE] = {0};
uint8_t g_dev_guid[GUID_SIZE] = {0};

static uint8_t g_board_rev_id = BOARD_REV_EVT;
static uint8_t g_vr_cpu0_vddq_abc;
static uint8_t g_vr_cpu0_vddq_def;
static uint8_t g_vr_cpu1_vddq_ghj;
static uint8_t g_vr_cpu1_vddq_klm;


typedef struct _inlet_corr_t {
  uint8_t duty;
  int8_t delta_t;
} inlet_corr_t;

static inlet_corr_t g_ict[] = {
  { 10, 7 },
  { 12, 6 },
  { 14, 5 },
  { 18, 4 },
  { 20, 3 },
  { 24, 2 },
  { 32, 1 },
  { 41, 0 },
};

static uint8_t g_ict_count = sizeof(g_ict)/sizeof(inlet_corr_t);

static bool is_cpu0_socket_occupy(void);
static bool is_cpu1_socket_occupy(void);
static void _print_sensor_discrete_log(uint8_t fru, uint8_t snr_num, char *snr_name,
    uint8_t val, char *event);

static void apply_inlet_correction(float *value) {
  static int8_t dt = 0;
  int i;
  uint8_t pwm[2] = {0};

  // Get PWM value
  if (pal_get_pwm_value(0, &pwm[0]) || pal_get_pwm_value(1, &pwm[1])) {
    // If error reading PWM value, use the previous deltaT
    *value -= dt;
    return;
  }
  pwm[0] = (pwm[0] + pwm[1]) /2;

  // Scan through the correction table to get correction value for given PWM
  dt=g_ict[0].delta_t;
  for (i=0; i< g_ict_count; i++) {
    if (pwm[0] >= g_ict[i].duty)
      dt = g_ict[i].delta_t;
    else
      break;
  }

  // Apply correction for the sensor
  *(float*)value -= dt;
}

static void
init_board_sensors(void) {
  pal_get_board_rev_id(&g_board_rev_id);

  if (g_board_rev_id == BOARD_REV_POWERON ||
      g_board_rev_id == BOARD_REV_EVT ) {
    g_vr_cpu0_vddq_abc = VR_CPU0_VDDQ_ABC_EVT;
    g_vr_cpu0_vddq_def = VR_CPU0_VDDQ_DEF_EVT;
    g_vr_cpu1_vddq_ghj = VR_CPU1_VDDQ_GHJ_EVT;
    g_vr_cpu1_vddq_klm = VR_CPU1_VDDQ_KLM_EVT;
  } else {
    g_vr_cpu0_vddq_abc = VR_CPU0_VDDQ_ABC;
    g_vr_cpu0_vddq_def = VR_CPU0_VDDQ_DEF;
    g_vr_cpu1_vddq_ghj = VR_CPU1_VDDQ_GHJ;
    g_vr_cpu1_vddq_klm = VR_CPU1_VDDQ_KLM;
  }
}

//Dynamic change CPU Temp threshold
static void
dyn_sensor_thresh_array_init() {
  static bool init_cpu0 = false;
  static bool init_cpu1 = false;
  static bool init_done = false;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};

  // Return if both cpu thresholds are initialized
  if (init_done) {
    return;
  }

  // Checkd if cpu0 threshold needs to be initialized
  if (init_cpu0) {
    goto dyn_cpu1_init;
  }

  sprintf(key, "mb_sensor%d", MB_SENSOR_CPU0_TJMAX);
  if( edb_cache_get(key,str) >= 0 && (float) (strtof(str, NULL) - 4) > 0) {
    mb_sensor_threshold[MB_SENSOR_CPU0_TEMP][UCR_THRESH] = (float) (strtof(str, NULL) - 4);
    init_cpu0 = true;
  }else{
    mb_sensor_threshold[MB_SENSOR_CPU0_TEMP][UCR_THRESH] = 104;
  }

  // Check if cpu1 threshold needs to be initialized
dyn_cpu1_init:
  if (init_cpu1) {
    goto dyn_thresh_exit;
  }

  sprintf(key, "mb_sensor%d", MB_SENSOR_CPU1_TJMAX);
  if( edb_cache_get(key,str) >= 0 && (float) (strtof(str, NULL) - 4) > 0 ) {
    mb_sensor_threshold[MB_SENSOR_CPU1_TEMP][UCR_THRESH] = (float) (strtof(str, NULL) - 4);
    init_cpu1 = true;
  }else{
    mb_sensor_threshold[MB_SENSOR_CPU1_TEMP][UCR_THRESH] = 104;
  }

  // Mark init complete only if both thresholds are initialized
dyn_thresh_exit:
  if (init_cpu0 && init_cpu1) {
    init_done = true;
  }
}

static void
sensor_thresh_array_init() {
  static bool init_done = false;

  dyn_sensor_thresh_array_init();

  if (init_done)
    return;

  mb_sensor_threshold[MB_SENSOR_INLET_TEMP][UCR_THRESH] = 40;
  mb_sensor_threshold[MB_SENSOR_OUTLET_TEMP][UCR_THRESH] = 90;
  mb_sensor_threshold[MB_SENSOR_INLET_REMOTE_TEMP][UCR_THRESH] = 40;
  mb_sensor_threshold[MB_SENSOR_OUTLET_REMOTE_TEMP][UCR_THRESH] = 75;

  // Assign UCT based on the system is Single Side or Double Side
  if (!(pal_get_platform_id(&g_plat_id)) && !(g_plat_id & PLAT_ID_SKU_MASK)) {
    mb_sensor_threshold[MB_SENSOR_FAN0_TACH][UCR_THRESH] = 9000;
    mb_sensor_threshold[MB_SENSOR_FAN1_TACH][UCR_THRESH] = 9000;
  } else {
    mb_sensor_threshold[MB_SENSOR_FAN0_TACH][UCR_THRESH] = 13500;
    mb_sensor_threshold[MB_SENSOR_FAN1_TACH][UCR_THRESH] = 13500;
  }

  mb_sensor_threshold[MB_SENSOR_FAN0_TACH][LCR_THRESH] = 500;
  mb_sensor_threshold[MB_SENSOR_FAN1_TACH][LCR_THRESH] = 500;

  mb_sensor_threshold[MB_SENSOR_P3V3][UCR_THRESH] = 3.621;
  mb_sensor_threshold[MB_SENSOR_P3V3][LCR_THRESH] = 2.975;
  mb_sensor_threshold[MB_SENSOR_P5V][UCR_THRESH] = 5.486;
  mb_sensor_threshold[MB_SENSOR_P5V][LCR_THRESH] = 4.524;
  mb_sensor_threshold[MB_SENSOR_P12V][UCR_THRESH] = 13.23;
  mb_sensor_threshold[MB_SENSOR_P12V][LCR_THRESH] = 10.773;
  mb_sensor_threshold[MB_SENSOR_P1V05][UCR_THRESH] = 1.15;
  mb_sensor_threshold[MB_SENSOR_P1V05][LCR_THRESH] = 0.94;
  mb_sensor_threshold[MB_SENSOR_PVNN_PCH_STBY][UCR_THRESH] = 1.1;
  mb_sensor_threshold[MB_SENSOR_PVNN_PCH_STBY][LCR_THRESH] = 0.76;
  mb_sensor_threshold[MB_SENSOR_P3V3_STBY][UCR_THRESH] = 3.621;
  mb_sensor_threshold[MB_SENSOR_P3V3_STBY][LCR_THRESH] = 2.975;
  mb_sensor_threshold[MB_SENSOR_P5V_STBY][UCR_THRESH] = 5.486;
  mb_sensor_threshold[MB_SENSOR_P5V_STBY][LCR_THRESH] = 4.524;
  mb_sensor_threshold[MB_SENSOR_P3V_BAT][UCR_THRESH] = 3.738;
  mb_sensor_threshold[MB_SENSOR_P3V_BAT][LCR_THRESH] = 2.73;
  mb_sensor_threshold[MB_SENSOR_HSC_IN_VOLT][UCR_THRESH] = 13.2;
  mb_sensor_threshold[MB_SENSOR_HSC_IN_VOLT][LCR_THRESH] = 10.8;
  mb_sensor_threshold[MB_SENSOR_HSC_OUT_CURR][UCR_THRESH] = 47.705;
  mb_sensor_threshold[MB_SENSOR_HSC_IN_POWER][UCR_THRESH] = 790.40;
  mb_sensor_threshold[MB_SENSOR_PCH_TEMP][UCR_THRESH] = 84;
  mb_sensor_threshold[MB_SENSOR_CPU0_DIMM_GRPA_TEMP][UCR_THRESH] = 81;
  mb_sensor_threshold[MB_SENSOR_CPU0_DIMM_GRPB_TEMP][UCR_THRESH] = 81;
  mb_sensor_threshold[MB_SENSOR_CPU1_DIMM_GRPC_TEMP][UCR_THRESH] = 81;
  mb_sensor_threshold[MB_SENSOR_CPU1_DIMM_GRPD_TEMP][UCR_THRESH] = 81;

  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIN_TEMP][UCR_THRESH] = 100;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIN_CURR][UCR_THRESH] = 235;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIN_POWER][UCR_THRESH] = 414;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIN_VOLT][LCR_THRESH] = 1.45;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIN_VOLT][UCR_THRESH] = 2.05;

  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VSA_TEMP][UCR_THRESH] = 100;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VSA_CURR][UCR_THRESH] = 20;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VSA_POWER][UCR_THRESH] = 25;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VSA_VOLT][LCR_THRESH] = 0.45;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VSA_VOLT][UCR_THRESH] = 1.2;


  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIO_TEMP][UCR_THRESH] = 100;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIO_CURR][UCR_THRESH] = 24;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIO_POWER][UCR_THRESH] = 32;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIO_VOLT][LCR_THRESH] = 0.8;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VCCIO_VOLT][UCR_THRESH] = 1.2;


  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPA_TEMP][UCR_THRESH] = 90;
  if (!(pal_get_platform_id(&g_plat_id)) && !(g_plat_id & PLAT_ID_SKU_MASK)) {
    mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPA_CURR][UCR_THRESH] = 40;
    mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPA_POWER][UCR_THRESH] = 66;
  } else {
    mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPA_CURR][UCR_THRESH] = 95;
    mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPA_POWER][UCR_THRESH] = 115;
  }
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPA_VOLT][LCR_THRESH] = 1.08;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPA_VOLT][UCR_THRESH] = 1.32;

  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPB_TEMP][UCR_THRESH] = 90;
  if (!(pal_get_platform_id(&g_plat_id)) && !(g_plat_id & PLAT_ID_SKU_MASK)) {
    mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPB_CURR][UCR_THRESH] = 40;
    mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPB_POWER][UCR_THRESH] = 66;
  } else {
    mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPB_CURR][UCR_THRESH] = 95;
    mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPB_POWER][UCR_THRESH] = 115;
  }
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPB_VOLT][LCR_THRESH] = 1.08;
  mb_sensor_threshold[MB_SENSOR_VR_CPU0_VDDQ_GRPB_VOLT][UCR_THRESH] = 1.32;

  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIN_TEMP][UCR_THRESH] = 100;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIN_CURR][UCR_THRESH] = 235;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIN_POWER][UCR_THRESH] = 420;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIN_VOLT][LCR_THRESH] = 1.45;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIN_VOLT][UCR_THRESH] = 2.05;

  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VSA_TEMP][UCR_THRESH] = 100;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VSA_CURR][UCR_THRESH] = 20;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VSA_POWER][UCR_THRESH] = 25;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VSA_VOLT][LCR_THRESH] = 0.45;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VSA_VOLT][UCR_THRESH] = 1.2;

  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIO_TEMP][UCR_THRESH] = 100;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIO_CURR][UCR_THRESH] = 24;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIO_POWER][UCR_THRESH] = 32;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIO_VOLT][LCR_THRESH] = 0.8;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VCCIO_VOLT][UCR_THRESH] = 1.2;

  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPC_TEMP][UCR_THRESH] = 90;
  if (!(pal_get_platform_id(&g_plat_id)) && !(g_plat_id & PLAT_ID_SKU_MASK)) {
    mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPC_CURR][UCR_THRESH] = 40;
    mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPC_POWER][UCR_THRESH] = 66;
  } else {
    mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPC_CURR][UCR_THRESH] = 95;
    mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPC_POWER][UCR_THRESH] = 115;
  }
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPC_VOLT][LCR_THRESH] = 1.08;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPC_VOLT][UCR_THRESH] = 1.32;

  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPD_TEMP][UCR_THRESH] = 90;
  if (!(pal_get_platform_id(&g_plat_id)) && !(g_plat_id & PLAT_ID_SKU_MASK)) {
    mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPD_CURR][UCR_THRESH] = 40;
    mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPD_POWER][UCR_THRESH] = 66;
  } else {
    mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPD_CURR][UCR_THRESH] = 95;
    mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPD_POWER][UCR_THRESH] = 115;
  }
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPD_VOLT][LCR_THRESH] = 1.08;
  mb_sensor_threshold[MB_SENSOR_VR_CPU1_VDDQ_GRPD_VOLT][UCR_THRESH] = 1.32;

  mb_sensor_threshold[MB_SENSOR_VR_PCH_PVNN_TEMP][UCR_THRESH] = 80;
  mb_sensor_threshold[MB_SENSOR_VR_PCH_PVNN_CURR][UCR_THRESH] = 23;
  mb_sensor_threshold[MB_SENSOR_VR_PCH_PVNN_POWER][UCR_THRESH] = 28;
  mb_sensor_threshold[MB_SENSOR_VR_PCH_PVNN_VOLT][LCR_THRESH] = 0.76;
  mb_sensor_threshold[MB_SENSOR_VR_PCH_PVNN_VOLT][UCR_THRESH] = 1.1;

  mb_sensor_threshold[MB_SENSOR_VR_PCH_P1V05_TEMP][UCR_THRESH] = 80;
  mb_sensor_threshold[MB_SENSOR_VR_PCH_P1V05_CURR][UCR_THRESH] = 19;
  mb_sensor_threshold[MB_SENSOR_VR_PCH_P1V05_POWER][UCR_THRESH] = 26;
  mb_sensor_threshold[MB_SENSOR_VR_PCH_P1V05_VOLT][LCR_THRESH] = 0.94;
  mb_sensor_threshold[MB_SENSOR_VR_PCH_P1V05_VOLT][UCR_THRESH] = 1.15;

  mb_sensor_threshold[MB_SENSOR_C2_AVA_FTEMP][UCR_THRESH] = 60;
  mb_sensor_threshold[MB_SENSOR_C2_AVA_RTEMP][UCR_THRESH] = 80;
  mb_sensor_threshold[MB_SENSOR_C2_1_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C2_2_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C2_3_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C2_4_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C3_AVA_FTEMP][UCR_THRESH] = 60;
  mb_sensor_threshold[MB_SENSOR_C3_AVA_RTEMP][UCR_THRESH] = 80;
  mb_sensor_threshold[MB_SENSOR_C3_1_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C3_2_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C3_3_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C3_4_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C4_AVA_FTEMP][UCR_THRESH] = 60;
  mb_sensor_threshold[MB_SENSOR_C4_AVA_RTEMP][UCR_THRESH] = 80;
  mb_sensor_threshold[MB_SENSOR_C4_1_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C4_2_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C4_3_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C4_4_NVME_CTEMP][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C2_P12V_INA230_VOL][UCR_THRESH] = 12.96;
  mb_sensor_threshold[MB_SENSOR_C2_P12V_INA230_VOL][LCR_THRESH] = 11.04;
  mb_sensor_threshold[MB_SENSOR_C2_P12V_INA230_CURR][UCR_THRESH] = 5.5;
  mb_sensor_threshold[MB_SENSOR_C2_P12V_INA230_PWR][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C3_P12V_INA230_VOL][UCR_THRESH] = 12.96;
  mb_sensor_threshold[MB_SENSOR_C3_P12V_INA230_VOL][LCR_THRESH] = 11.04;
  mb_sensor_threshold[MB_SENSOR_C3_P12V_INA230_CURR][UCR_THRESH] = 5.5;
  mb_sensor_threshold[MB_SENSOR_C3_P12V_INA230_PWR][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_C4_P12V_INA230_VOL][UCR_THRESH] = 12.96;
  mb_sensor_threshold[MB_SENSOR_C4_P12V_INA230_VOL][LCR_THRESH] = 11.04;
  mb_sensor_threshold[MB_SENSOR_C4_P12V_INA230_CURR][UCR_THRESH] = 5.5;
  mb_sensor_threshold[MB_SENSOR_C4_P12V_INA230_PWR][UCR_THRESH] = 75;
  mb_sensor_threshold[MB_SENSOR_CONN_P12V_INA230_VOL][UCR_THRESH] = 12.96;
  mb_sensor_threshold[MB_SENSOR_CONN_P12V_INA230_VOL][LCR_THRESH] = 11.04;
  mb_sensor_threshold[MB_SENSOR_CONN_P12V_INA230_CURR][UCR_THRESH] = 20;
  mb_sensor_threshold[MB_SENSOR_CONN_P12V_INA230_PWR][UCR_THRESH] = 250;
  nic_sensor_threshold[MEZZ_SENSOR_TEMP][UCR_THRESH] = 95;

  init_board_sensors();
  init_done = true;
}

// Helper Functions
static int
i2c_io(int fd, uint8_t addr, uint8_t *tbuf, uint8_t tcount, uint8_t *rbuf, uint8_t rcount) {
  struct i2c_rdwr_ioctl_data data;
  struct i2c_msg msg[2];
  int n_msg = 0;
  int rc;

  memset(&msg, 0, sizeof(msg));

  if (tcount) {
    msg[n_msg].addr = addr >> 1;
    msg[n_msg].flags = 0;
    msg[n_msg].len = tcount;
    msg[n_msg].buf = tbuf;
    n_msg++;
  }

  if (rcount) {
    msg[n_msg].addr = addr >> 1;
    msg[n_msg].flags = I2C_M_RD;
    msg[n_msg].len = rcount;
    msg[n_msg].buf = rbuf;
    n_msg++;
  }

  data.msgs = msg;
  data.nmsgs = n_msg;

  rc = ioctl(fd, I2C_RDWR, &data);
  if (rc < 0) {
    // syslog(LOG_ERR, "Failed to do raw io");
    return -1;
  }

  return 0;
}

static int
pal_control_mux(int fd, uint8_t addr, uint8_t channel) {
  uint8_t tcount = 1, rcount = 0;
  uint8_t tbuf[16] = {0};
  uint8_t rbuf[16] = {0};

  // PCA9544A
  if (channel < 4)
    tbuf[0] = 0x04 + channel;
  else
    tbuf[0] = 0x00; // close all channels

  return i2c_io(fd, addr, tbuf, tcount, rbuf, rcount);
}

static int
pal_control_switch(int fd, uint8_t addr, uint8_t channel) {
  uint8_t tcount = 1, rcount = 0;
  uint8_t tbuf[16] = {0};
  uint8_t rbuf[16] = {0};

  // PCA9846
  if (channel < 4)
    tbuf[0] = 0x01 << channel;
  else
    tbuf[0] = 0x00; // close all channels

  return i2c_io(fd, addr, tbuf, tcount, rbuf, rcount);
}

static int
read_device(const char *device, int *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "r");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    syslog(LOG_INFO, "failed to open device %s", device);
#endif
    return err;
  }

  rc = fscanf(fp, "%d", value);
  fclose(fp);
  if (rc != 1) {
#ifdef DEBUG
    syslog(LOG_INFO, "failed to read device %s", device);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

static int
read_device_float(const char *device, float *value) {
  FILE *fp;
  int rc;
  char tmp[10];

  fp = fopen(device, "r");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    syslog(LOG_INFO, "failed to open device %s", device);
#endif
    return err;
  }

  rc = fscanf(fp, "%s", tmp);
  fclose(fp);

  if (rc != 1) {
#ifdef DEBUG
    syslog(LOG_INFO, "failed to read device %s", device);
#endif
    return ENOENT;
  }

  *value = atof(tmp);

  return 0;
}

static int
read_device_hex(const char *device, int *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "r");
  if (!fp) {
#ifdef DEBUG
    syslog(LOG_INFO, "failed to open device %s", device);
#endif
    return errno;
  }

  rc = fscanf(fp, "%x", value);
  fclose(fp);
  if (rc != 1) {
#ifdef DEBUG
    syslog(LOG_INFO, "failed to read device %s", device);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

static int
write_device(const char *device, const char *value) {
  FILE *fp;
  int rc;

  fp = fopen(device, "w");
  if (!fp) {
    int err = errno;
#ifdef DEBUG
    syslog(LOG_INFO, "failed to open device for write %s", device);
#endif
    return err;
  }

  rc = fputs(value, fp);
  fclose(fp);

  if (rc < 0) {
#ifdef DEBUG
    syslog(LOG_INFO, "failed to write device %s", device);
#endif
    return ENOENT;
  } else {
    return 0;
  }
}

static int
read_temp_attr(const char *device, const char *attr, float *value) {
  char full_name[LARGEST_DEVICE_NAME + 1];
  char dir_name[LARGEST_DEVICE_NAME + 1];
  int tmp;
  FILE *fp;
  int size;

  // Get current working directory
  snprintf(
      full_name, LARGEST_DEVICE_NAME, "cd %s;pwd", device);

  fp = popen(full_name, "r");
  fgets(dir_name, LARGEST_DEVICE_NAME, fp);
  pclose(fp);

  // Remove the newline character at the end
  size = strlen(dir_name);
  dir_name[size-1] = '\0';

  snprintf(
      full_name, LARGEST_DEVICE_NAME, "%s/%s", dir_name, attr);

  if (read_device(full_name, &tmp)) {
    return -1;
  }

  *value = ((float)tmp)/UNIT_DIV;

  return 0;
}

static int
read_temp(const char *device, float *value) {
  return read_temp_attr(device, "temp1_input", value);
}

static int
read_nic_temp(const char *device, float *value) {
  char full_name[LARGEST_DEVICE_NAME + 1];
  char dir_name[LARGEST_DEVICE_NAME + 1];
  int tmp;
  FILE *fp;
  int size;
  int ret = 0;
  static unsigned int retry = 0;

  // Get current working directory
  snprintf(
      full_name, LARGEST_DEVICE_NAME, "cd %s;pwd", device);

  fp = popen(full_name, "r");
  fgets(dir_name, LARGEST_DEVICE_NAME, fp);
  pclose(fp);

  // Remove the newline character at the end
  size = strlen(dir_name);
  dir_name[size-1] = '\0';

  snprintf(
      full_name, LARGEST_DEVICE_NAME, "%s/temp2_input", dir_name);

  if (read_device(full_name, &tmp)) {
    ret = READING_NA;
  }

  *value = ((float)tmp)/UNIT_DIV;

  // Workaround: handle when NICs wrongly report higher temperatures
  if (*value > NIC_MAX_TEMP) {
    ret = READING_NA;
  } else {
    retry = 0;
  }

  if (ret == READING_NA && ++retry <= 3)
    ret = READING_SKIP;

  return ret;

}

static int
read_fan_value(const int fan, const char *device, int *value) {
  char device_name[LARGEST_DEVICE_NAME];
  char full_name[LARGEST_DEVICE_NAME];

  snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", TACH_DIR, device_name);
  return read_device(full_name, value);
}

static int
read_fan_value_f(const int fan, const char *device, float *value) {
  char device_name[LARGEST_DEVICE_NAME];
  char full_name[LARGEST_DEVICE_NAME];
  int ret;

  snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", TACH_DIR, device_name);
  ret = read_device_float(full_name, value);
  if (*value < 500) {
    sleep(2);
    ret = read_device_float(full_name, value);
  }

  return ret;
}

static int
write_fan_value(const int fan, const char *device, const int value) {
  char full_name[LARGEST_DEVICE_NAME];
  char device_name[LARGEST_DEVICE_NAME];
  char output_value[LARGEST_DEVICE_NAME];

  snprintf(device_name, LARGEST_DEVICE_NAME, device, fan);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", PWM_DIR, device_name);
  snprintf(output_value, LARGEST_DEVICE_NAME, "%d", value);
  return write_device(full_name, output_value);
}

static int
read_adc_value(const int pin, const char *device, float *value) {
  char device_name[LARGEST_DEVICE_NAME];
  char full_name[LARGEST_DEVICE_NAME];

  snprintf(device_name, LARGEST_DEVICE_NAME, device, pin);
  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", ADC_DIR, device_name);
  return read_device_float(full_name, value);
}

static int
read_hsc_value(const char *device, float *value) {
  char full_name[LARGEST_DEVICE_NAME];
  char dir_name[LARGEST_DEVICE_NAME + 1];
  int tmp;
  FILE *fp;
  int size;

  // Get current working directory
  snprintf(
      full_name, LARGEST_DEVICE_NAME, "cd %s;pwd", HSC_DEVICE);

  fp = popen(full_name, "r");
  fgets(dir_name, LARGEST_DEVICE_NAME, fp);
  pclose(fp);

  // Remove the newline character at the end
  size = strlen(dir_name);
  dir_name[size-1] = '\0';

  snprintf(full_name, LARGEST_DEVICE_NAME, "%s/%s", dir_name, device);
  if(read_device(full_name, &tmp)) {
    return -1;
  }

  *value = ((float) tmp)/UNIT_DIV;

  return 0;
}

static int
read_hsc_current_value(float *value) {
  uint8_t bus_id = 0x4; //TODO: ME's address 0x2c in FBTP
  uint8_t tbuf[256] = {0x00};
  uint8_t rbuf[256] = {0x00};
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  float hsc_b = 20475;
  float Rsence;
  ipmb_req_t *req;
  ipmb_res_t *res;
  char path[64] = {0};
  int val=0;
  int ret = 0;
  static int retry = 0;

  req = (ipmb_req_t*)tbuf;

  req->res_slave_addr = 0x2C; //ME's Slave Address
  req->netfn_lun = NETFN_NM_REQ<<2;
  req->hdr_cksum = req->res_slave_addr + req->netfn_lun;
  req->hdr_cksum = ZERO_CKSUM_CONST - req->hdr_cksum;

  req->req_slave_addr = 0x20;
  req->seq_lun = 0x00;

  req->cmd = CMD_NM_SEND_RAW_PMBUS;
  req->data[0] = 0x57;
  req->data[1] = 0x01;
  req->data[2] = 0x00;
  req->data[3] = 0x86;
  //HSC slave addr check for SS and DS
  sprintf(path, GPIO_VAL, GPIO_BOARD_SKU_ID4);
  read_device(path, &val);
  if (val){ //DS
    req->data[4] = 0x8A;
    Rsence = 0.265;
  }else{    //SS
    req->data[4] = 0x22;
    Rsence = 0.505;
  }
  req->data[5] = 0x00;
  req->data[6] = 0x00;
  req->data[7] = 0x01;
  req->data[8] = 0x02;
  req->data[9] = 0x8C;
  tlen = 16;

  // Invoke IPMB library handler
  lib_ipmb_handle(bus_id, tbuf, tlen+1, &rbuf, &rlen);

  if (rlen == 0) {
#ifdef DEBUG
    syslog(LOG_DEBUG, "read_hsc_current_value: Zero bytes received\n");
#endif
    ret = READING_NA;
  }
  if (rbuf[6] == 0)
  {
    *value = ((float) (rbuf[11] << 8 | rbuf[10])*10-hsc_b )/(800*Rsence);
    retry = 0;
  } else {
    ret = READING_NA;
  }

  if (ret == READING_NA) {
    retry++;
    if (retry <= 3 )
      ret = READING_SKIP;
  }

  return ret;
}

static int
read_sensor_reading_from_ME(uint8_t snr_num, float *value) {
  uint8_t bus_id = 0x4; //TODO: ME's address 0x2c in FBTP
  uint8_t tbuf[256] = {0x00};
  uint8_t rbuf[256] = {0x00};
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  ipmb_req_t *req;
  ipmb_res_t *res;
  int ret = 0;
  enum {
    e_HSC_PIN,
    e_HSC_VIN,
    e_PCH_TEMP,
    e_MAX,
  };
  static uint8_t retry[e_MAX] = {0};

  req = (ipmb_req_t*)tbuf;
  req->res_slave_addr = 0x2C; //ME's Slave Address
  req->netfn_lun = NETFN_SENSOR_REQ<<2;
  req->hdr_cksum = req->res_slave_addr + req->netfn_lun;
  req->hdr_cksum = ZERO_CKSUM_CONST - req->hdr_cksum;

  req->req_slave_addr = 0x20;
  req->seq_lun = 0x00;
  req->cmd = CMD_SENSOR_GET_SENSOR_READING;
  req->data[0] = snr_num;
  tlen = 7;

  // Invoke IPMB library handler
  lib_ipmb_handle(bus_id, tbuf, tlen+1, &rbuf, &rlen);

  if (rlen == 0) {
  //ME no response
#ifdef DEBUG
    syslog(LOG_DEBUG, "read HSC %x from_ME: Zero bytes received\n", snr_num);
#endif
   ret = READING_NA;
  } else {
    if (rbuf[6] == 0)
    {
        if (rbuf[8] & 0x20) {
          //not available
          ret = READING_NA;
        }
    } else {
      ret = READING_NA;
    }
  }

  if(snr_num == MB_SENSOR_HSC_IN_POWER) {
    if (!ret) {
      *value = (((float) rbuf[7])*0x20 + 0 )/10 ;
      retry[e_HSC_PIN] = 0;
    } else {
      retry[e_HSC_PIN]++;
      if (retry[e_HSC_PIN] <= 3)
        ret = READING_SKIP;
    }
  } else if(snr_num == MB_SENSOR_HSC_IN_VOLT) {
    if (!ret) {
      *value = (((float) rbuf[7])*0x02 + (0x5e*10) )/100 ;
      retry[e_HSC_VIN] = 0;
    } else {
      retry[e_HSC_VIN]++;
      if (retry[e_HSC_VIN] <= 3)
        ret = READING_SKIP;
    }
  } else if(snr_num == MB_SENSOR_PCH_TEMP) {
    if (!ret) {
      *value = (float) rbuf[7];
      retry[e_PCH_TEMP] = 0;
    } else {
      retry[e_PCH_TEMP]++;
      if (retry[e_PCH_TEMP] <= 3)
        ret = READING_SKIP;
    }
  }
  return ret;
}

static int
read_cpu_temp(uint8_t snr_num, float *value) {
  int ret = 0;
  uint8_t bus_id = 0x4; //TODO: ME's address 0x2c in FBTP
  uint8_t tbuf[256] = {0x00};
  uint8_t rbuf1[256] = {0x00};
  static uint8_t tjmax[2] = {0x00};
  static uint8_t tjmax_flag[2] = {0};
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  ipmb_req_t *req;
  ipmb_res_t *res;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  int cpu_index;
  int16_t dts;
  static uint8_t retry[2] = {0x00}; // CPU0 and CPU1

  switch (snr_num) {
    case MB_SENSOR_CPU0_TEMP:
      cpu_index = 0;
      break;
    case MB_SENSOR_CPU1_TEMP:
      cpu_index = 1;
      break;
    default:
      return -1;
  }

  req = (ipmb_req_t*)tbuf;

  req->res_slave_addr = 0x2C; //ME's Slave Address

  req->netfn_lun = NETFN_NM_REQ<<2;
  req->hdr_cksum = req->res_slave_addr + req->netfn_lun;
  req->hdr_cksum = ZERO_CKSUM_CONST - req->hdr_cksum;

  req->req_slave_addr = 0x20;
  req->seq_lun = 0x00;

  if( tjmax_flag[cpu_index] == 0 ) { // First time to get CPU0/CPU1 Tjmax reading
    //Get CPU0/CPU1 Tjmax
    req->cmd = CMD_NM_SEND_RAW_PECI;
    req->data[0] = 0x57;
    req->data[1] = 0x01;
    req->data[2] = 0x00;
    req->data[3] = 0x30 + cpu_index;
    req->data[4] = 0x05;
    req->data[5] = 0x05;
    req->data[6] = 0xa1;
    req->data[7] = 0x00;
    req->data[8] = 0x10;
    req->data[9] = 0x00;
    req->data[10] = 0x00;
    tlen = 17;
    // Invoke IPMB library handler
    lib_ipmb_handle(bus_id, tbuf, tlen+1, &rbuf1, &rlen);
    if (rlen == 0) {
    //ME no response
#ifdef DEBUG
      syslog(LOG_DEBUG, "%s(%d): Zero bytes received\n", __func__, __LINE__);
#endif
    } else {
      if (rbuf1[6] == 0)
      {
        // If PECI command successes and got a reasonable value
        if ( (rbuf1[10] == 0x40) && rbuf1[13] > 50) {
          tjmax[cpu_index] = rbuf1[13];
          tjmax_flag[cpu_index] = 1;
        }
      }
    }
  }

  //Updated CPU Tjmax cache
  sprintf(key, "mb_sensor%d", (cpu_index?MB_SENSOR_CPU1_TJMAX:MB_SENSOR_CPU0_TJMAX));
  if (tjmax_flag[cpu_index] != 0) {
    sprintf(str, "%.2f",(float) tjmax[cpu_index]);
  } else {
    //ME no response or PECI command completion code error. Set "NA" in sensor cache.
    strcpy(str, "NA");
  }
  edb_cache_set(key, str);

  // Get CPU temp if BMC got TjMax
  ret = READING_NA;
  if (tjmax_flag[cpu_index] != 0) {
    rlen = 0;
    memset( rbuf1,0x00,sizeof(rbuf1) );
    //Get CPU Temp
    req->cmd = CMD_NM_SEND_RAW_PECI;
    req->data[0] = 0x57;
    req->data[1] = 0x01;
    req->data[2] = 0x00;
    req->data[3] = 0x30 + cpu_index;
    req->data[4] = 0x05;
    req->data[5] = 0x05;
    req->data[6] = 0xa1;
    req->data[7] = 0x00;
    req->data[8] = 0x02;
    req->data[9] = 0xff;
    req->data[10] = 0x00;
    tlen = 17;

    // Invoke IPMB library handler
    lib_ipmb_handle(bus_id, tbuf, tlen+1, &rbuf1, &rlen);

    if (rlen == 0) {
      //ME no response
#ifdef DEBUG
      syslog(LOG_DEBUG, "%s(%d): Zero bytes received\n", __func__, __LINE__);
#endif
    } else {
      if (rbuf1[6] == 0) { // ME Completion Code
        if ( (rbuf1[10] == 0x40) ) { // PECI Completion Code
          dts = (rbuf1[11] | rbuf1[12] << 8);
          // Intel Doc#554767 p.58: Reserved Values 0x8000~0x81ff
          if (dts <= -32257) {
            ret = READING_NA;
          } else {
            // 16-bit, 2s complement [15]Sign Bit;[14:6]Integer Value;[5:0]Fractional Value
            *value = (float) (tjmax[0] + (dts >> 6));
            ret = 0;
          }
        }
      }
    }
  }

  if (ret != 0) {
    retry[cpu_index]++;
    if (retry[cpu_index] <= 3) {
      ret = READING_SKIP;
      return ret;
    }
  } else
    retry[cpu_index] = 0;

  return ret;
}

static int
read_dimm_temp(uint8_t snr_num, float *value) {
  int ret = READING_NA;
  uint8_t bus_id = 0x4; //TODO: ME's address 0x2c in FBTP
  uint8_t tbuf[256] = {0x00};
  uint8_t rbuf1[256] = {0x00};
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  ipmb_req_t *req;
  ipmb_res_t *res;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  int dimm_index, i;
  int max = 0;
  static uint8_t retry[4] = {0x00};
  int val;
  char path[64] = {0};
  static int odm_id = -1;
  uint8_t BoardInfo;

  //Use FM_BOARD_SKU_ID0 to identify ODM to apply filter
  if (odm_id == -1) {
    ret = pal_get_platform_id(&BoardInfo);
    if (ret == 0) {
      odm_id = (int) (BoardInfo & 0x1);
    }
  }

  // show NA if BIOS has not completed POST.
  sprintf(path, GPIO_VAL, GPIO_FM_BIOS_POST_CMPLT_N);
  if (read_device(path, &val) || val) {
    return ret;
  }

  switch (snr_num) {
    case MB_SENSOR_CPU0_DIMM_GRPA_TEMP:
      dimm_index = 0;
      break;
    case MB_SENSOR_CPU0_DIMM_GRPB_TEMP:
      dimm_index = 1;
      break;
    case MB_SENSOR_CPU1_DIMM_GRPC_TEMP:
      dimm_index = 2;
      break;
    case MB_SENSOR_CPU1_DIMM_GRPD_TEMP:
      dimm_index = 3;
      break;
    default:
      return -1;
  }

  req = (ipmb_req_t*)tbuf;

  req->res_slave_addr = 0x2C; //ME's Slave Address

  req->netfn_lun = NETFN_NM_REQ<<2;
  req->hdr_cksum = req->res_slave_addr + req->netfn_lun;
  req->hdr_cksum = ZERO_CKSUM_CONST - req->hdr_cksum;

  req->req_slave_addr = 0x20;
  req->seq_lun = 0x00;

  for (i=0; i<3; i++) { // Get 3 channel for each DIMM group
    //Get DIMM Temp per channel
    req->cmd = CMD_NM_SEND_RAW_PECI;
    req->data[0] = 0x57;
    req->data[1] = 0x01;
    req->data[2] = 0x00;
    req->data[3] = 0x30 + (dimm_index / 2);
    req->data[4] = 0x05;
    req->data[5] = 0x05;
    req->data[6] = 0xa1;
    req->data[7] = 0x00;
    req->data[8] = 0x0e;
    req->data[9] = 0x00 + (dimm_index % 2 * 3) + i;
    req->data[10] = 0x00;
    tlen = 17;

    // Invoke IPMB library handler
    lib_ipmb_handle(bus_id, tbuf, tlen+1, &rbuf1, &rlen);
    if (rlen == 0) {
    //ME no response
#ifdef DEBUG
      syslog(LOG_DEBUG, "%s(%d): Zero bytes received\n", __func__, __LINE__);
#endif
    } else {
      if (rbuf1[6] == 0)
      {
        // If PECI command successes
        if ( (rbuf1[10] == 0x40)) {
          if (rbuf1[11] > max)
            max = rbuf1[11];
          if (rbuf1[12] > max)
            max = rbuf1[11];
        }
      }
    }
  }

  if (odm_id == 1) {
    // Filter abnormal values: 0x0 and 0xFF
    if (max != 0 && max != 0xFF)
      ret = 0;
  } else {
    // Filter abnormal values: 0x0
    if (max != 0)
      ret = 0;
  }

  if (ret != 0) {
    retry[dimm_index]++;
    if (retry[dimm_index] <= 3) {
      ret = READING_SKIP;
      return ret;
    }
  } else
    retry[dimm_index] = 0;

  if (ret == 0) {
    *value = (float)max;
  }

  return ret;
}

static int
read_cpu_package_power(uint8_t snr_num, float *value) {
  int ret = READING_NA;
  uint8_t bus_id = 0x4; //TODO: ME's address 0x2c in FBTP
  uint8_t tbuf[256] = {0x00};
  uint8_t rbuf1[256] = {0x00};
  // Energy units: Intel Doc#554767, p37, 2^(-ENERGY UNIT) J, ENERGY UNIT defalut is 14
  // Run Time units: Intel Doc#554767, p33, msec
  // 2^(-14)*1000 = 0.06103515625
  float unit = 0.06103515625f;
  static uint32_t last_pkg_energy[2] = {0}, last_run_time[2] = {0};
  uint32_t pkg_energy, run_time, diff_energy, diff_time;
  uint8_t tlen = 0;
  uint8_t rlen = 0;
  ipmb_req_t *req;
  ipmb_res_t *res;
  int cpu_index;
  static uint8_t retry[2] = {0x00}; // CPU0 and CPU1

  switch (snr_num) {
    case MB_SENSOR_CPU0_PKG_POWER:
      cpu_index = 0;
      break;
    case MB_SENSOR_CPU1_PKG_POWER:
      cpu_index = 1;
      break;
    default:
      return -1;
  }

  req = (ipmb_req_t*)tbuf;

  req->res_slave_addr = 0x2C; //ME's Slave Address

  req->netfn_lun = NETFN_NM_REQ<<2;
  req->hdr_cksum = req->res_slave_addr + req->netfn_lun;
  req->hdr_cksum = ZERO_CKSUM_CONST - req->hdr_cksum;

  req->req_slave_addr = 0x20;
  req->seq_lun = 0x00;

  // Get CPU package power and run time
  rlen = 0;
  memset( rbuf1,0x00,sizeof(rbuf1) );
  //Read Accumulated Energy Pkg and Accumulated Run Time
  req->cmd = CMD_NM_AGGREGATED_SEND_RAW_PECI;
  req->data[0] = 0x57;
  req->data[1] = 0x01;
  req->data[2] = 0x00;
  req->data[3] = 0x30 + cpu_index;
  req->data[4] = 0x05;
  req->data[5] = 0x05;
  req->data[6] = 0xa1;
  req->data[7] = 0x00;
  req->data[8] = 0x03;
  req->data[9] = 0xff;
  req->data[10] = 0x00;
  req->data[11] = 0x30 + cpu_index;
  req->data[12] = 0x05;
  req->data[13] = 0x05;
  req->data[14] = 0xa1;
  req->data[15] = 0x00;
  req->data[16] = 0x1F;
  req->data[17] = 0x00;
  req->data[18] = 0x00;
  tlen = 25;

  // Invoke IPMB library handler
  lib_ipmb_handle(bus_id, tbuf, tlen+1, &rbuf1, &rlen);

  if (rlen == 0) {
    //ME no response
#ifdef DEBUG
    syslog(LOG_DEBUG, "%s(%d): Zero bytes received\n", __func__, __LINE__);
#endif
    goto error_exit;
  } else {
    if (rbuf1[6] == 0) { // ME Completion Code
      if ( (rbuf1[10] == 0x00) && (rbuf1[11] == 0x40) && // 1st ME CC & PECI CC
           (rbuf1[16] == 0x00) && (rbuf1[17] == 0x40) ){ // 2nd ME CC & PECI CC
        pkg_energy = rbuf1[15];
        pkg_energy = (pkg_energy << 8) | rbuf1[14];
        pkg_energy = (pkg_energy << 8) | rbuf1[13];
        pkg_energy = (pkg_energy << 8) | rbuf1[12];

        run_time = rbuf1[21];
        run_time = (run_time << 8) | rbuf1[20];
        run_time = (run_time << 8) | rbuf1[19];
        run_time = (run_time << 8) | rbuf1[18];

        ret = 0;
      }
    }
  }

  // need at least 2 entries to calculate
  if (last_pkg_energy[cpu_index] == 0 && last_run_time[cpu_index] == 0) {
    last_pkg_energy[cpu_index] = pkg_energy;
    last_run_time[cpu_index] = run_time;
    ret = READING_NA;
  }

  if(!ret) {
    if(pkg_energy >= last_pkg_energy[cpu_index])
      diff_energy = pkg_energy - last_pkg_energy[cpu_index];
    else
      diff_energy = pkg_energy + (0xffffffff - last_pkg_energy[cpu_index] + 1);
    last_pkg_energy[cpu_index] = pkg_energy;

    if(run_time >= last_run_time[cpu_index])
      diff_time = run_time - last_run_time[cpu_index];
    else
      diff_time = run_time + (0xffffffff - last_run_time[cpu_index] + 1);
    last_run_time[cpu_index] = run_time;

    if(diff_time == 0)
      ret = READING_NA;
    else
      *value = ((float)diff_energy / (float)diff_time * unit);
  }

error_exit:
  if (ret != 0) {
    retry[cpu_index]++;
    if (retry[cpu_index] <= 3) {
      ret = READING_SKIP;
      return ret;
    }
  } else
    retry[cpu_index] = 0;

  return ret;
}

static int
read_ava_temp(uint8_t sensor_num, float *value) {
  int fd = 0;
  char fn[32];
  int ret = READING_NA;;
  static unsigned int retry[6] = {0};
  uint8_t i_retry;
  uint8_t tcount, rcount, slot_cfg, addr, mux_chan, mux_addr = 0xe2;
  uint8_t tbuf[16] = {0};
  uint8_t rbuf[16] = {0};

  if (pal_get_slot_cfg_id(&slot_cfg) < 0)
    slot_cfg = SLOT_CFG_EMPTY;

  switch(sensor_num) {
    case MB_SENSOR_C2_AVA_FTEMP:
      i_retry = 0; break;
    case MB_SENSOR_C2_AVA_RTEMP:
      i_retry = 1; break;
    case MB_SENSOR_C3_AVA_FTEMP:
      i_retry = 2; break;
    case MB_SENSOR_C3_AVA_RTEMP:
      i_retry = 3; break;
    case MB_SENSOR_C4_AVA_FTEMP:
      i_retry = 4; break;
    case MB_SENSOR_C4_AVA_RTEMP:
      i_retry = 5; break;
  }

  switch(sensor_num) {
    case MB_SENSOR_C2_AVA_FTEMP:
    case MB_SENSOR_C2_AVA_RTEMP:
      if(slot_cfg == SLOT_CFG_EMPTY)
        return READING_NA;
      mux_chan = 0;
      break;
    case MB_SENSOR_C3_AVA_FTEMP:
    case MB_SENSOR_C3_AVA_RTEMP:
      if(slot_cfg == SLOT_CFG_EMPTY)
        return READING_NA;
      mux_chan = 1;
      break;
    case MB_SENSOR_C4_AVA_FTEMP:
    case MB_SENSOR_C4_AVA_RTEMP:
      if(slot_cfg != SLOT_CFG_SS_3x8)
        return READING_NA;
      mux_chan = 2;
      break;
    default:
      return READING_NA;
  }

  switch(sensor_num) {
    case MB_SENSOR_C2_AVA_FTEMP:
    case MB_SENSOR_C3_AVA_FTEMP:
    case MB_SENSOR_C4_AVA_FTEMP:
      addr = 0x92;
      break;
    case MB_SENSOR_C2_AVA_RTEMP:
    case MB_SENSOR_C3_AVA_RTEMP:
    case MB_SENSOR_C4_AVA_RTEMP:
      addr = 0x90;
      break;
  }

  snprintf(fn, sizeof(fn), "/dev/i2c-%d", RISER_BUS_ID);
  fd = open(fn, O_RDWR);
  if (fd < 0) {
    ret = READING_NA;
    goto error_exit;
  }

  // control multiplexer to target channel.
  ret = pal_control_mux(fd, mux_addr, mux_chan);
  if (ret < 0) {
    ret = READING_NA;
    goto error_exit;
  }

  // Read 2 bytes from TMP75
  tbuf[0] = 0x00;
  tcount = 1;
  rcount = 2;

  ret = i2c_io(fd, addr, tbuf, tcount, rbuf, rcount);
  if (ret < 0) {
    ret = READING_NA;
    goto error_exit;
  }
  ret = 0;
  retry[i_retry] = 0;

  // rbuf:MSB, LSB; 12-bit value on Bit[15:4], unit: 0.0625
  *value = (float)(signed char)rbuf[0];

error_exit:
  if (fd > 0) {
    pal_control_mux(fd, mux_addr, 0xff); // close
    close(fd);
  }

  if (ret == READING_NA && ++retry[i_retry] <= 3)
    ret = READING_SKIP;

  return ret;
}

static int
read_INA230 (uint8_t sensor_num, float *value, int pot) {
  int fd = 0;
  char fn[32];
  int ret = READING_NA;;
  static unsigned int retry[12] = {0}, initialized[4] = {0};
  uint8_t i_retry;
  uint8_t tcount, rcount, slot_cfg, addr, mux_chan, mux_addr = 0xe2;
  uint8_t tbuf[16] = {0};
  uint8_t rbuf[16] = {0};
  int16_t temp;

  if (pal_get_slot_cfg_id(&slot_cfg) < 0)
    slot_cfg = SLOT_CFG_EMPTY;

  switch(sensor_num) {
    case MB_SENSOR_C2_P12V_INA230_VOL:
      i_retry = 0; break;
    case MB_SENSOR_C2_P12V_INA230_CURR:
      i_retry = 1; break;
    case MB_SENSOR_C2_P12V_INA230_PWR:
      i_retry = 2; break;
    case MB_SENSOR_C3_P12V_INA230_VOL:
      i_retry = 3; break;
    case MB_SENSOR_C3_P12V_INA230_CURR:
      i_retry = 4; break;
    case MB_SENSOR_C3_P12V_INA230_PWR:
      i_retry = 5; break;
    case MB_SENSOR_C4_P12V_INA230_VOL:
      i_retry = 6; break;
    case MB_SENSOR_C4_P12V_INA230_CURR:
      i_retry = 7; break;
    case MB_SENSOR_C4_P12V_INA230_PWR:
      i_retry = 8; break;
    case MB_SENSOR_CONN_P12V_INA230_VOL:
      i_retry = 9; break;
    case MB_SENSOR_CONN_P12V_INA230_CURR:
      i_retry = 10; break;
    case MB_SENSOR_CONN_P12V_INA230_PWR:
      i_retry = 11; break;
  }

  switch(sensor_num) {
    case MB_SENSOR_C2_P12V_INA230_VOL:
    case MB_SENSOR_C2_P12V_INA230_CURR:
    case MB_SENSOR_C2_P12V_INA230_PWR:
    case MB_SENSOR_C3_P12V_INA230_VOL:
    case MB_SENSOR_C3_P12V_INA230_CURR:
    case MB_SENSOR_C3_P12V_INA230_PWR:
    case MB_SENSOR_CONN_P12V_INA230_VOL:
    case MB_SENSOR_CONN_P12V_INA230_CURR:
    case MB_SENSOR_CONN_P12V_INA230_PWR:
      if(slot_cfg == SLOT_CFG_EMPTY)
        return READING_NA;
      break;
    case MB_SENSOR_C4_P12V_INA230_VOL:
    case MB_SENSOR_C4_P12V_INA230_CURR:
    case MB_SENSOR_C4_P12V_INA230_PWR:
      if(slot_cfg != SLOT_CFG_SS_3x8)
        return READING_NA;
      break;
    default:
      return READING_NA;
  }

  // If Power On Time == 1, re-initialize INA230
  if (pot == 1 && (i_retry % 3) == 0)
    initialized[i_retry/3] = 0;

  //use channel 4
  mux_chan = 0x3;

  snprintf(fn, sizeof(fn), "/dev/i2c-%d", RISER_BUS_ID);
  fd = open(fn, O_RDWR);
  if (fd < 0) {
    ret = READING_NA;
    goto error_exit;
  }

  //control multiplexer to target channel.
  ret = pal_control_mux(fd, mux_addr, mux_chan);
  if (ret < 0) {
    ret = READING_NA;
    goto error_exit;
  }

  switch(sensor_num) {
    case MB_SENSOR_C2_P12V_INA230_VOL:
    case MB_SENSOR_C2_P12V_INA230_CURR:
    case MB_SENSOR_C2_P12V_INA230_PWR:
      addr = 0x80;
      break;
    case MB_SENSOR_C3_P12V_INA230_VOL:
    case MB_SENSOR_C3_P12V_INA230_CURR:
    case MB_SENSOR_C3_P12V_INA230_PWR:
      addr = 0x82;
      break;
    case MB_SENSOR_C4_P12V_INA230_VOL:
    case MB_SENSOR_C4_P12V_INA230_CURR:
    case MB_SENSOR_C4_P12V_INA230_PWR:
      addr = 0x88;
      break;
    case MB_SENSOR_CONN_P12V_INA230_VOL:
    case MB_SENSOR_CONN_P12V_INA230_CURR:
    case MB_SENSOR_CONN_P12V_INA230_PWR:
      addr = 0x8A;
      break;
    default:
        syslog(LOG_WARNING, "read_INA230: undefined sensor number") ;
      break;
    }

  if (initialized[i_retry/3] == 0) {
    //Set Configuration register
    tbuf[0] = 0x00, tbuf[1] = 0x49; tbuf[2] = 0x27;
    ret = i2c_io(fd, addr, tbuf, 3, rbuf, 0);
    if (ret < 0) {
      ret = READING_NA;
      goto error_exit;
    }

    //Set Calibration register
    tbuf[0] = 0x05, tbuf[1] = 0x9; tbuf[2] = 0xd9;
    ret = i2c_io(fd, addr, tbuf, 3, rbuf, 0);
    if (ret < 0) {
      ret = READING_NA;
      goto error_exit;
    }
    initialized[i_retry/3] = 1;
  }

  // Delay for 2 cycles and check INA230 init done
  if(pot < 3 || initialized[i_retry/3] == 0){
    ret = READING_NA;
    goto error_exit;
  }

  //Get registers data
  switch(sensor_num) {
    case MB_SENSOR_C2_P12V_INA230_VOL:
    case MB_SENSOR_C3_P12V_INA230_VOL:
    case MB_SENSOR_C4_P12V_INA230_VOL:
    case MB_SENSOR_CONN_P12V_INA230_VOL:
      tbuf[0] = 0x02;
      break;
    case MB_SENSOR_C2_P12V_INA230_CURR:
    case MB_SENSOR_C3_P12V_INA230_CURR:
    case MB_SENSOR_C4_P12V_INA230_CURR:
    case MB_SENSOR_CONN_P12V_INA230_CURR:
      tbuf[0] = 0x04;
      break;
    case MB_SENSOR_C2_P12V_INA230_PWR:
    case MB_SENSOR_C3_P12V_INA230_PWR:
    case MB_SENSOR_C4_P12V_INA230_PWR:
    case MB_SENSOR_CONN_P12V_INA230_PWR:
      tbuf[0] = 0x03;
      break;
    default:
        syslog(LOG_WARNING, "read_INA230: undefined sensor number") ;
      break;
    }

  tbuf[1] = 0x0; tbuf[2] = 0x0;

  ret = i2c_io(fd, addr, tbuf, 1, rbuf, 2);
  if (ret < 0) {
    ret = READING_NA;
    goto error_exit;
  }

  switch(sensor_num) {
    case MB_SENSOR_C2_P12V_INA230_VOL:
    case MB_SENSOR_C3_P12V_INA230_VOL:
    case MB_SENSOR_C4_P12V_INA230_VOL:
    case MB_SENSOR_CONN_P12V_INA230_VOL:
      *value = ((rbuf[1] + rbuf[0] *256) *0.00125) ;
      break;
    case MB_SENSOR_C2_P12V_INA230_CURR:
    case MB_SENSOR_C3_P12V_INA230_CURR:
    case MB_SENSOR_C4_P12V_INA230_CURR:
    case MB_SENSOR_CONN_P12V_INA230_CURR:
      temp = rbuf[0];
      temp = (temp <<8) + rbuf[1];
      *value = temp * 0.001;
      if(*value < 0)
        *value = 0;
      break;
    case MB_SENSOR_C2_P12V_INA230_PWR:
    case MB_SENSOR_C3_P12V_INA230_PWR:
    case MB_SENSOR_C4_P12V_INA230_PWR:
    case MB_SENSOR_CONN_P12V_INA230_PWR:
      *value = (rbuf[1] + rbuf[0] * 256)*0.025;
      if(*value < 1)
        *value = 0;
      break;
    default:
        syslog(LOG_WARNING, "read_INA230: undefined sensor number") ;
      break;
    }

    ret = 0;
    retry[i_retry] = 0;

error_exit:
  if (fd > 0) {
    pal_control_mux(fd, mux_addr, 0xff); // close
    close(fd);
  }

  if (ret == READING_NA && ++retry[i_retry] <= 3)
    ret = READING_SKIP;

  return ret;
}

static int
read_nvme_temp(uint8_t sensor_num, float *value) {
  int fd = 0;
  char fn[32];
  int ret = READING_NA;
  static unsigned int retry[12] = {0};
  uint8_t i_retry;
  uint8_t tcount, rcount, slot_cfg, addr = 0xd4, mux_chan, mux_addr = 0xe2;
  uint8_t switch_chan, switch_addr=0xe6;
  uint8_t tbuf[16] = {0};
  uint8_t rbuf[16] = {0};

  if (pal_get_slot_cfg_id(&slot_cfg) < 0)
    slot_cfg = SLOT_CFG_EMPTY;

  switch(sensor_num) {
    case MB_SENSOR_C2_1_NVME_CTEMP:
      i_retry = 0; break;
    case MB_SENSOR_C2_2_NVME_CTEMP:
      i_retry = 1; break;
    case MB_SENSOR_C2_3_NVME_CTEMP:
      i_retry = 2; break;
    case MB_SENSOR_C2_4_NVME_CTEMP:
      i_retry = 3; break;
    case MB_SENSOR_C3_1_NVME_CTEMP:
      i_retry = 4; break;
    case MB_SENSOR_C3_2_NVME_CTEMP:
      i_retry = 5; break;
    case MB_SENSOR_C3_3_NVME_CTEMP:
      i_retry = 6; break;
    case MB_SENSOR_C3_4_NVME_CTEMP:
      i_retry = 7; break;
    case MB_SENSOR_C4_1_NVME_CTEMP:
      i_retry = 8; break;
    case MB_SENSOR_C4_2_NVME_CTEMP:
      i_retry = 9; break;
    case MB_SENSOR_C4_3_NVME_CTEMP:
      i_retry = 10; break;
    case MB_SENSOR_C4_4_NVME_CTEMP:
      i_retry = 11; break;
  }

  switch(sensor_num) {
    case MB_SENSOR_C2_1_NVME_CTEMP:
    case MB_SENSOR_C2_2_NVME_CTEMP:
    case MB_SENSOR_C2_3_NVME_CTEMP:
    case MB_SENSOR_C2_4_NVME_CTEMP:
      if(slot_cfg == SLOT_CFG_EMPTY)
        return READING_NA;
      mux_chan = 0;
      break;
    case MB_SENSOR_C3_1_NVME_CTEMP:
    case MB_SENSOR_C3_2_NVME_CTEMP:
    case MB_SENSOR_C3_3_NVME_CTEMP:
    case MB_SENSOR_C3_4_NVME_CTEMP:
      if(slot_cfg == SLOT_CFG_EMPTY)
        return READING_NA;
      mux_chan = 1;
      break;
    case MB_SENSOR_C4_1_NVME_CTEMP:
    case MB_SENSOR_C4_2_NVME_CTEMP:
    case MB_SENSOR_C4_3_NVME_CTEMP:
    case MB_SENSOR_C4_4_NVME_CTEMP:
      if(slot_cfg != SLOT_CFG_SS_3x8)
        return READING_NA;
      mux_chan = 2;
      break;
    default:
      return READING_NA;
  }

  switch(sensor_num) {
    case MB_SENSOR_C2_1_NVME_CTEMP:
    case MB_SENSOR_C3_1_NVME_CTEMP:
    case MB_SENSOR_C4_1_NVME_CTEMP:
      switch_chan = 0;
      break;
    case MB_SENSOR_C2_2_NVME_CTEMP:
    case MB_SENSOR_C3_2_NVME_CTEMP:
    case MB_SENSOR_C4_2_NVME_CTEMP:
      switch_chan = 1;
      break;
    case MB_SENSOR_C2_3_NVME_CTEMP:
    case MB_SENSOR_C3_3_NVME_CTEMP:
    case MB_SENSOR_C4_3_NVME_CTEMP:
      switch_chan = 2;
      break;
    case MB_SENSOR_C2_4_NVME_CTEMP:
    case MB_SENSOR_C3_4_NVME_CTEMP:
    case MB_SENSOR_C4_4_NVME_CTEMP:
      switch_chan = 3;
      break;
  }

  snprintf(fn, sizeof(fn), "/dev/i2c-%d", RISER_BUS_ID);
  fd = open(fn, O_RDWR);
  if (fd < 0) {
    ret = READING_NA;
    goto error_exit;
  }

  // control I2C multiplexer to target channel.
  ret = pal_control_mux(fd, mux_addr, mux_chan);
  if (ret < 0) {
    ret = READING_NA;
    goto error_exit;
  }

  // control I2C switch to target channel.
  ret = pal_control_switch(fd, switch_addr, switch_chan);
  // Report temp of PCIe card on MB_SENSOR_CX_1_NVME_CTEMP senosrs,
  // no I2C Switch on PCIe Card
  if (ret < 0 && switch_chan != 0) {
    ret = READING_NA;
    goto error_exit;
  }

  // Read 8 bytes from NVMe
  tbuf[0] = 0x00;
  tcount = 1;
  rcount = 8;

  ret = i2c_io(fd, addr, tbuf, tcount, rbuf, rcount);
  if (ret < 0) {
    ret = READING_NA;
    goto error_exit;
  }
  ret = 0;
  retry[i_retry] = 0;

  // Cmd 0: length, SFLGS, SMART Warnings, CTemp, PDLU, Reserved, Reserved, PEC
  *value = (float)(signed char)rbuf[3];

error_exit:
  if (fd > 0) {
    pal_control_switch(fd, switch_addr, 0xff); // close
    pal_control_mux(fd, mux_addr, 0xff); // close
    close(fd);
  }

  if (ret == READING_NA && ++retry[i_retry] <= 3)
    ret = READING_SKIP;

  return ret;
}

static void
add_CPLD_event (uint8_t fru, uint8_t snr_num, uint8_t reg, uint8_t snr_val, uint8_t value) {
  char sensor_name[32] = {0}, event_str[30] = {0};
  pal_get_sensor_name(fru, snr_num, sensor_name);

  sprintf(event_str, "");
  switch(reg) {
      case PWRDATA1_REG :
	 if (value == 0x40)
	   strcat(event_str, "FM_CTNR_PS_ON power rail fails");
	 else if (value == 0x00)
	   strcat(event_str, "PWRGD_P12V_MAIN power rail fails");
	 else if (value == 0xc0)
	   strcat(event_str, "PWRGD_P5V power rail fails");
	 else if (value == 0xe0)
	   strcat(event_str, "PWRGD_P3V3 power rail fails");
	 else if (value == 0xf7)
	   strcat(event_str, "PWRGD_PVPP_ABC power rail fails");
	 else if (value == 0xfb)
	   strcat(event_str, "PWRGD_PVPP_DEF power rail fails");
	 else if (value == 0xfd)
	   strcat(event_str, "PWRGD_PVPP_GHJ power rail fails");
	 else if (value == 0xfe)
	   strcat(event_str, "PWRGD_PVPP_KLM power rail fails");
	 else
	   strcat(event_str, "Unknown power rail fails(PWRDATA1)");
        break;
      case PWRDATA2_REG :
	 if (value == 0x55)
	   strcat(event_str, "PWRGD_PVTT_CPU0 power rail fails");
	 else if (value == 0xaa)
	   strcat(event_str, "PWRGD_PVTT_CPU1 power rail fails");
	 else if (value == 0xd5)
	   strcat(event_str, "PWRGD_PVCCIO_CPU0 power rail fails");
	 else if (value == 0xea)
	   strcat(event_str, "PWRGD_PVCCIO_CPU1 power rail fails");
	 else if (value == 0xf7)
	   strcat(event_str, "PWRGD_PVCCIN_CPU0 power rail fails");
	 else if (value == 0xfb)
	   strcat(event_str, "PWRGD_PVCCIN_CPU1 power rail fails");
	 else if (value == 0xfd)
	   strcat(event_str, "PWRGD_PVSA_CPU0 power rail fails");
	 else if (value == 0xfe)
	   strcat(event_str, "PWRGD_PVSA_CPU1 power rail fails");
	 else
	   strcat(event_str, "Unknown power rail fails(PWRDATA2)");
        break;
      case PWRDATA3_REG :
	 if (value == 0x40)
	   strcat(event_str, "PWRGD_CPUPWRGD power rail fails");
	 else if (value == 0x80)
	   strcat(event_str, "RST_PLTRST_N power rail fails");
	 else
	   strcat(event_str, "Unknown power rail fails(PWRDATA3)");
        break;
    }
    if(power_fail_log == 0){
      _print_sensor_discrete_log(fru, snr_num, sensor_name, reg, event_str);
      pal_add_cri_sel(event_str);
      power_fail_log = 1;
    }
}

static int
read_CPLD_power_fail_sts (uint8_t fru, uint8_t sensor_num, float *value, int pot) {
  int fd = 0;
  char fn[32];
  int ret = READING_NA, i;
  static unsigned int retry=0;
  uint8_t tcount, rcount;
  static uint8_t power_fail = 0;
  uint8_t tbuf[16] = {0};
  uint8_t rbuf[16] = {0}, data_chk;
  uint8_t sensor_value;
  int val;
  char path[64] = {0};

  //Check SLPS4 is high before start monitor CPLD power fail
  sprintf(path, GPIO_VAL, GPIO_FM_SLPS4_N);
  if (read_device(path, &val)) {
    goto error_exit;
  }
  if (val == 0x0) {
    power_fail = 0;
    goto error_exit;
  }

  snprintf(fn, sizeof(fn), "/dev/i2c-%d", CPLD_BUS_ID);
  fd = open(fn, O_RDWR);
  if (fd < 0) {
    goto error_exit;
  }

  for(i=0;i<3;i++) {
    switch(i) {
      case MAIN_PWR_STS_REG :
        data_chk = MAIN_PWR_STS_VAL;
        break;
      case CPU0_PWR_STS_REG :
        data_chk = CPU0_PWR_STS_VAL;
        break;
      case CPU1_PWR_STS_REG :
        data_chk = CPU1_PWR_STS_VAL;
        break;
    }

    tbuf[0] = i;
    ret = i2c_io(fd, CPLD_ADDR, tbuf, 1, rbuf, 1);
    if (ret < 0) {
      ret = READING_NA;
      goto error_exit;
    }
    if ( rbuf[0] != data_chk ) {
      power_fail++;
      break;
    }
  }

  if(power_fail <= 3) {
    ret = 0;
    *value = 0;
    power_fail_log = 0;
  } else {
    for(i=3;i<6;i++) {
      switch(i) {
        case PWRDATA1_REG :
          data_chk = PWRDATA1_VAL;
          break;
        case PWRDATA2_REG :
          data_chk = PWRDATA2_VAL;
          break;
        case PWRDATA3_REG :
          data_chk = PWRDATA3_VAL;
          break;
      }
      // Read 1 byte in offset 00h
      tbuf[0] = i;
      ret = i2c_io(fd, CPLD_ADDR, tbuf, 1, rbuf, 1);
      if (ret < 0) {
        ret = READING_NA;
        goto error_exit;
      }
      if ( (rbuf[0] != data_chk) && (power_fail > 3)) {
        sensor_value = 0x01<<i;
        *value = sensor_value;
        add_CPLD_event(fru, sensor_num, i , sensor_value, rbuf[0]);
        break;
      }
    }
  }

error_exit:
  if (fd > 0) {
    close(fd);
  }
  if ((ret == READING_NA) && (retry < MAX_READ_RETRY)){
    ret = READING_SKIP;
    retry++;
  } else {
    retry = 0;
  }

  return ret;
}

static int
pal_key_index(char *key) {

  int ret;
  int i;

  i = 0;
  while(strcmp(key_cfg[i].name, LAST_KEY)) {

    // If Key is valid, return success
    if (!strcmp(key, key_cfg[i].name))
      return i;

    i++;
  }

#ifdef DEBUG
  syslog(LOG_WARNING, "pal_key_index: invalid key - %s", key);
#endif
  return -1;
}

int
pal_get_key_value(char *key, char *value) {
  int index;

  // Check is key is defined and valid
  if ((index = pal_key_index(key)) < 0)
    return -1;

  return kv_get(key, value);
}

int
pal_set_key_value(char *key, char *value) {
  int index, ret;
  char cmd[64];
  // Check is key is defined and valid
  if ((index = pal_key_index(key)) < 0)
    return -1;

  if (key_cfg[index].function) {
    ret = key_cfg[index].function(KEY_BEFORE_SET, value);
    if (ret < 0)
      return ret;
  }

  return kv_set(key, value);
}

static int
key_func_por_policy (int event, void *arg)
{
  char cmd[MAX_VALUE_LEN];
  char value[MAX_VALUE_LEN];

  switch (event) {
    case KEY_BEFORE_SET:
      if (pal_is_fw_update_ongoing(FRU_MB))
        return -1;
      // sync to env
      snprintf(cmd, MAX_VALUE_LEN, "/sbin/fw_setenv por_policy %s", (char *)arg);
      system(cmd);
      break;
    case KEY_AFTER_INI:
      // sync to env
      kv_get("server_por_cfg", value);
      snprintf(cmd, MAX_VALUE_LEN, "/sbin/fw_setenv por_policy %s", value);
      system(cmd);
      break;
  }

  return 0;
}

static int
key_func_lps (int event, void *arg)
{
  char cmd[MAX_VALUE_LEN];
  char value[MAX_VALUE_LEN];

  switch (event) {
    case KEY_BEFORE_SET:
      if (pal_is_fw_update_ongoing(FRU_MB))
        return -1;
      snprintf(cmd, MAX_VALUE_LEN, "/sbin/fw_setenv por_ls %s", (char *)arg);
      system(cmd);
      break;
    case KEY_AFTER_INI:
      kv_get("pwr_server_last_state", value);
      snprintf(cmd, MAX_VALUE_LEN, "/sbin/fw_setenv por_ls %s", value);
      system(cmd);
      break;
  }

  return 0;
}

static int
key_func_ntp (int event, void *arg)
{
  char cmd[MAX_VALUE_LEN];
  char ntp_server_new[MAX_VALUE_LEN];
  char ntp_server_old[MAX_VALUE_LEN];

  switch (event) {
    case KEY_BEFORE_SET:
      // Remove old NTP server
      kv_get("ntp_server", ntp_server_old);
      if (strlen(ntp_server_old) > 2) {
        snprintf(cmd, MAX_VALUE_LEN, "sed -i '/^server %s$/d' /etc/ntp.conf", ntp_server_old);
        system(cmd);
      }
      // Add new NTP server
      snprintf(ntp_server_new, MAX_VALUE_LEN, "%s", (char *)arg);
      if (strlen(ntp_server_new) > 2) {
        snprintf(cmd, MAX_VALUE_LEN, "echo \"server %s\" >> /etc/ntp.conf", ntp_server_new);
        system(cmd);
      }
      // Restart NTP server
      snprintf(cmd, MAX_VALUE_LEN, "/etc/init.d/ntpd restart > /dev/null &");
      system(cmd);
      break;
    case KEY_AFTER_INI:
      break;
  }

  return 0;
}

static int
key_func_tz (int event, void *arg)
{
  char cmd[MAX_VALUE_LEN];
  char timezone[MAX_VALUE_LEN];
  char path[MAX_VALUE_LEN];

  switch (event) {
    case KEY_BEFORE_SET:
      snprintf(timezone, MAX_VALUE_LEN, "%s", (char *)arg);
      snprintf(path, MAX_VALUE_LEN, "/usr/share/zoneinfo/%s", (char *)arg);
      if( access(path, F_OK) != -1 ) {
        snprintf(cmd, MAX_VALUE_LEN, "echo %s > /etc/timezone", timezone);
        system(cmd);
        snprintf(cmd, MAX_VALUE_LEN, "ln -fs %s /etc/localtime", path);
        system(cmd);
      } else {
        return -1;
      }
      break;
    case KEY_AFTER_INI:
      break;
  }

  return 0;
}

// Power Button Override
int
pal_PBO(void) {
  char vpath[64] = {0};

  sprintf(vpath, GPIO_VAL, GPIO_POWER);

  if (write_device(vpath, "1")) {
    return -1;
  }
  if (write_device(vpath, "0")) {
    return -1;
  }
  sleep(6);
  if (write_device(vpath, "1")) {
    return -1;
  }
  return 0;
}

// Power On the server in a given slot
static int
server_power_on(void) {
  char vpath[64] = {0};

  sprintf(vpath, GPIO_VAL, GPIO_POWER);

  if (write_device(vpath, "1")) {
    return -1;
  }

  if (write_device(vpath, "0")) {
    return -1;
  }

  sleep(1);

  if (write_device(vpath, "1")) {
    return -1;
  }

  sleep(2);

  system("/usr/bin/sv restart fscd >> /dev/null");

  return 0;
}

// Power Off the server in given slot
static int
server_power_off(bool gs_flag) {
  char vpath[64] = {0};

  sprintf(vpath, GPIO_VAL, GPIO_POWER);

  system("/usr/bin/sv stop fscd >> /dev/null");

  if (write_device(vpath, "1")) {
    return -1;
  }

  sleep(1);

  if (write_device(vpath, "0")) {
    return -1;
  }

  if (gs_flag) {
    sleep(DELAY_GRACEFUL_SHUTDOWN);
  } else {
    sleep(DELAY_POWER_OFF);
  }

  if (write_device(vpath, "1")) {
    return -1;
  }

  return 0;
}

// Power reset the server in given slot
static int
server_power_reset(void) {
  char vpath[64] = {0};

  sprintf(vpath, GPIO_VAL, GPIO_POWER_RESET);

  system("/usr/bin/sv stop fscd >> /dev/null");

  if (write_device(vpath, "1")) {
    return -1;
  }

  sleep(1);

  if (write_device(vpath, "0")) {
    return -1;
  }

  sleep(1);

  if (write_device(vpath, "1")) {
    return -1;
  }

  system("/etc/init.d/setup-fan.sh >> /dev/null");
  system("/usr/bin/sv start fscd >> /dev/null");

  return 0;
}

// Debug Card's UART and BMC/SoL port share UART port and need to enable only one
static int
control_sol_txd(uint8_t fru) {
  uint32_t lpc_fd;
  uint32_t ctrl;
  void *lpc_reg;
  void *lpc_hicr;

  lpc_fd = open("/dev/mem", O_RDWR | O_SYNC );
  if (lpc_fd < 0) {
#ifdef DEBUG
    syslog(LOG_WARNING, "control_sol_txd: open fails\n");
#endif
    return -1;
  }

  lpc_reg = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, lpc_fd,
             AST_LPC_BASE);
  lpc_hicr = (char*)lpc_reg + HICRA_OFFSET;

  // Read HICRA register
  ctrl = *(volatile uint32_t*) lpc_hicr;
  // Clear bits for UART2 and UART3 routing
  ctrl &= (~HICRA_MASK_UART2);
  ctrl &= (~HICRA_MASK_UART3);

  // Route UART2 to UART3 for SoL purpose
  ctrl |= (UART2_TO_UART3 << 22);

  // Route DEBUG to UART1 for TXD control
  ctrl |= (UART3_TO_UART2 << 19);

  *(volatile uint32_t*) lpc_hicr = ctrl;

  munmap(lpc_reg, PAGE_SIZE);
  close(lpc_fd);

  return 0;
}

// Display the given POST code using GPIO port
static int
pal_post_display(uint8_t status) {
  char path[64] = {0};
  int ret;
  char *val;

#ifdef DEBUG
  syslog(LOG_WARNING, "pal_post_display: status is %d\n", status);
#endif

  sprintf(path, GPIO_VAL, GPIO_POSTCODE_0);

  if (BIT(status, 0)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  sprintf(path, GPIO_VAL, GPIO_POSTCODE_1);
  if (BIT(status, 1)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  sprintf(path, GPIO_VAL, GPIO_POSTCODE_2);
  if (BIT(status, 2)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  sprintf(path, GPIO_VAL, GPIO_POSTCODE_3);
  if (BIT(status, 3)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  sprintf(path, GPIO_VAL, GPIO_POSTCODE_4);
  if (BIT(status, 4)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  sprintf(path, GPIO_VAL, GPIO_POSTCODE_5);
  if (BIT(status, 5)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  sprintf(path, GPIO_VAL, GPIO_POSTCODE_6);
  if (BIT(status, 6)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

  sprintf(path, GPIO_VAL, GPIO_POSTCODE_7);
  if (BIT(status, 7)) {
    val = "1";
  } else {
    val = "0";
  }

  ret = write_device(path, val);
  if (ret) {
    goto post_exit;
  }

post_exit:
  if (ret) {
#ifdef DEBUG
    syslog(LOG_WARNING, "write_device failed for %s\n", path);
#endif
    return -1;
  } else {
    return 0;
  }
}

// Platform Abstraction Layer (PAL) Functions
int
pal_get_platform_name(char *name) {
  strcpy(name, FBTP_PLATFORM_NAME);

  return 0;
}

int
pal_get_num_slots(uint8_t *num) {
  *num = FBTP_MAX_NUM_SLOTS;

  return 0;
}

int
pal_is_fru_prsnt(uint8_t fru, uint8_t *status) {
  *status = 1;

  return 0;
}

int
pal_is_fru_ready(uint8_t fru, uint8_t *status) {
  *status = 1;

  return 0;
}

int
pal_is_slot_server(uint8_t fru) {
  if (fru == FRU_MB)
    return 1;
  return 0;
}

int
pal_get_CPU_CATERR(uint8_t fru, uint8_t *status) {
  int val;
  char path[64] = {0};

  sprintf(path, GPIO_VAL, GPIO_FM_CPU_CATERR_LVT3_N);

  if (read_device(path, &val)) {
    return -1;
  }

  if (val == 0x0) {
     *status = 0;
  } else {
     *status = 1;
  }
  return 0;
}

int
pal_get_CPU_MSMI(uint8_t fru, uint8_t *status) {
  int val;
  char path[64] = {0};

  sprintf(path, GPIO_VAL, GPIO_FM_CPU_MSMI_LVT3_N);

  if (read_device(path, &val)) {
    return -1;
  }

  if (val == 0x0) {
    *status = 0;
   } else {
    *status = 1;
   }

   return 0;
}

int
pal_is_debug_card_prsnt(uint8_t *status) {
  int val;
  char path[64] = {0};

  sprintf(path, GPIO_VAL, GPIO_DBG_CARD_PRSNT);

  if (read_device(path, &val)) {
    return -1;
  }

  if (val == 0x0) {
    *status = 1;
  } else {
    *status = 0;
  }

  return 0;
}

int
pal_get_server_power(uint8_t fru, uint8_t *status) {
  int val;
  char path[64] = {0};

  sprintf(path, GPIO_VAL, GPIO_POWER_GOOD);

  if (read_device(path, &val)) {
    return -1;
  }

  if (val == 0x0) {
    *status = 0;
  } else {
    *status = 1;
  }

  return 0;
}

static bool
is_server_off(void) {
  int ret;
  uint8_t status;
  ret = pal_get_server_power(FRU_MB, &status);
  if (ret) {
    return false;
  }

  if (status == SERVER_POWER_OFF) {
    return true;
  } else {
    return false;
  }
}


// Power Off, Power On, or Power Reset the server in given slot
int
pal_set_server_power(uint8_t fru, uint8_t cmd) {
  uint8_t status;
  bool gs_flag = false;

  if (pal_get_server_power(fru, &status) < 0) {
    return -1;
  }

  switch(cmd) {
    case SERVER_POWER_ON:
      if (status == SERVER_POWER_ON)
        return 1;
      else
        return server_power_on();
      break;

    case SERVER_POWER_OFF:
      if (status == SERVER_POWER_OFF)
        return 1;
      else
        return server_power_off(gs_flag);
      break;

    case SERVER_POWER_CYCLE:
      if (status == SERVER_POWER_ON) {
        if (server_power_off(gs_flag))
          return -1;

        sleep(DELAY_POWER_CYCLE);

        return server_power_on();

      } else if (status == SERVER_POWER_OFF) {

        return (server_power_on());
      }
      break;

    case SERVER_GRACEFUL_SHUTDOWN:
      if (status == SERVER_POWER_OFF)
        return 1;
      else
        gs_flag = true;
        return server_power_off(gs_flag);
      break;

   case SERVER_POWER_RESET:
      if (status == SERVER_POWER_ON)
        return server_power_reset();
      else if (status == SERVER_POWER_OFF)
        return server_power_on();
      break;

    default:
      return -1;
  }

  return 0;
}

int
pal_sled_cycle(void) {
  // Send command to HSC power cycle
  // Single Side
  system("i2cset -y 7 0x11 0xd9 c &> /dev/null");

  // Double Side
  system("i2cset -y 7 0x45 0xd9 c &> /dev/null");

  return 0;
}

// Read the Front Panel Hand Switch and return the position
int
pal_get_hand_sw(uint8_t *pos) {
  return 0;
}

// Return the Front panel Power Button
int
pal_get_pwr_btn(uint8_t *status) {
  char path[64] = {0};
  int val;

  sprintf(path, GPIO_VAL, GPIO_PWR_BTN);
  if (read_device(path, &val)) {
    return -1;
  }

  if (val) {
    *status = 0x0;
  } else {
    *status = 0x1;
  }

  return 0;
}

// Return the front panel's Reset Button status
int
pal_get_rst_btn(uint8_t *status) {
  char path[64] = {0};
  int val;

  sprintf(path, GPIO_VAL, GPIO_RST_BTN);
  if (read_device(path, &val)) {
    return -1;
  }

  if (val) {
    *status = 0x0;
  } else {
    *status = 0x1;
  }

  return 0;
}

// Update the Reset button input to the server at given slot
int
pal_set_rst_btn(uint8_t slot, uint8_t status) {
  char path[64] = {0};
  char *val;

  if (slot < 1 || slot > 4) {
    return -1;
  }

  if (status) {
    val = "1";
  } else {
    val = "0";
  }

  sprintf(path, GPIO_VAL, gpio_rst_btn[slot]);
  if (write_device(path, val)) {
    return -1;
  }

  return 0;
}

// Update the LED for the given slot with the status
int
pal_set_led(uint8_t fru, uint8_t status) {
  char path[64] = {0};
  char *val;

//TODO: Need to check power LED control from CPLD
  return 0;

  if (status) {
    val = "1";
  } else {
    val = "0";
  }

  sprintf(path, GPIO_VAL, GPIO_POWER_LED);
  if (write_device(path, val)) {
    return -1;
  }

  return 0;
}

// Update Heartbeet LED
int
pal_set_hb_led(uint8_t status) {
  char cmd[64] = {0};
  char *val;

  if (status) {
    val = "1";
  } else {
    val = "0";
  }

  sprintf(cmd, "devmem 0x1e6c0064 32 %s", val);

  system(cmd);

  return 0;
}

// Update the Identification LED for the given fru with the status
int
pal_set_id_led(uint8_t fru, uint8_t status) {
  char path[64] = {0};
  char *val;

  if (status) {
    val = "1";
  } else {
    val = "0";
  }

  sprintf(path, GPIO_VAL, GPIO_POWER_LED);

  if (write_device(path, val)) {
    return -1;
  }

  return 0;
}

static int
set_usb_mux(uint8_t state) {
  return 0;
}

// Update the USB Mux to the server at given slot
int
pal_switch_usb_mux(uint8_t slot) {
  return 0;
}

// Switch the UART mux to the given fru
int
pal_switch_uart_mux(uint8_t fru) {
  return control_sol_txd(fru);
}

// Enable POST buffer for the server in given slot
int
pal_post_enable(uint8_t slot) {
  int ret;
  int i;

  return 0;
}

// Disable POST buffer for the server in given slot
int
pal_post_disable(uint8_t slot) {
  int ret;
  int i;

  return 0;
}

// Get the last post code of the given slot
int
pal_post_get_last(uint8_t slot, uint8_t *status) {
  int ret;
  uint8_t len;
  int i;

  return 0;
}

// Handle the received post code, for now display it on debug card
int
pal_post_handle(uint8_t slot, uint8_t status) {
  uint8_t prsnt, pos;
  int ret;

  // Check for debug card presence
  ret = pal_is_debug_card_prsnt(&prsnt);
  if (ret) {
    return ret;
  }

  // No debug card  present, return
  if (!prsnt) {
    return 0;
  }

  // Get the hand switch position
  ret = pal_get_hand_sw(&pos);
  if (ret) {
    return ret;
  }

  // If the give server is not selected, return
  if (pos != slot) {
    return 0;
  }

  // Display the post code in the debug card
  ret = pal_post_display(status);
  if (ret) {
    return ret;
  }

  return 0;
}

static int
check_postcodes(uint8_t fru_id, uint8_t sensor_num, float *value) {
  static int log_asserted = 0;
  const int loop_threshold = 3;
  const int longest_loop_code = 4;
  int i, nearest_00, len, loop_count, check_until;
  unsigned char buff[256];
  uint8_t location, maj_err, min_err, mem_train_fail;
  int ret = READING_NA, rc;
  static unsigned int retry=0;
  char sensor_name[32] = {0};
  char str[32] = {0};

  if (fru_id != 1) {
    syslog(LOG_ERR, "Not Supported Operation for fru %d", fru_id);
    goto error_exit;
  }

  if (is_server_off()) {
    log_asserted = 0;
    goto error_exit;
  }

  len = 0; // clear higher bits
  rc = pal_get_80port_record(FRU_MB, NULL, 0, buff, (uint8_t *)&len);
  if (rc != PAL_EOK)
    goto error_exit;

  mem_train_fail = 0;
  loop_count = 0;
  check_until = len - (longest_loop_code * (loop_threshold+1) );
  // Check post code from tail
  for(i = len - 1; i >= 0 && i >= check_until; i--) {
    if (buff[i] == 0x00) {
      if (loop_count < loop_threshold) {
        // found 00
        loop_count++;
        nearest_00 = i;
        continue;
      } else {
        // found (loop_threshold+1)-th 00 from tail
        if (!memcmp(&buff[i], &buff[nearest_00], len - nearest_00)) {
          // PostCode looping over loop_threshold times
          if ((nearest_00 - i) == 4) {
            // Loop Convention1
            mem_train_fail = 1;
            location = buff[i+1];
            maj_err = buff[i+2];
            min_err = buff[i+3];
          }
          if ((nearest_00 - i) == 3) {
            // Loop Convention2
            mem_train_fail = 1;
            location = 0x00;
            maj_err = buff[i+1];
            min_err = buff[i+2];
          }
        }
        // break after 2nd 00
        break;
      }
    }
  }

  if (mem_train_fail) {
    if (!log_asserted) {
      pal_get_sensor_name(fru_id, sensor_num, sensor_name);
      if (location) {
        snprintf(str, sizeof(str), "Location:%02X Err:%02X %02X",location, maj_err, min_err);
        _print_sensor_discrete_log(fru_id, sensor_num, sensor_name, 0x01, str);
        snprintf(str, sizeof(str), "DIMM %02X initial fails",location);
        pal_add_cri_sel(str);
        //syslog(LOG_CRIT, "Memory training failure at %02X MajErr:%02X, MinErr:%02X", location, maj_err, min_err);
      } else {
        snprintf(str, sizeof(str), "Location Unknown Err:%02X %02X", maj_err, min_err);
        _print_sensor_discrete_log(fru_id, sensor_num, sensor_name, 0x01, str);
        //syslog(LOG_CRIT, "Memory training failure MajErr:%02X, MinErr:%02X", maj_err, min_err);
        snprintf(str, sizeof(str), "DIMM XX initial fails");
        pal_add_cri_sel(str);
      }
    }
    log_asserted = 1;
  }
  else
  {
    log_asserted = 0;
  }
  *value = (float)log_asserted;
  ret = 0;

error_exit:
  if ((ret == READING_NA) && (retry < MAX_READ_RETRY)){
    ret = READING_SKIP;
    retry++;
  } else {
    retry = 0;
  }

  return ret;
}

static int
check_frb3(uint8_t fru_id, uint8_t sensor_num, float *value) {
  static unsigned int retry = 0;
  static uint8_t frb3_fail = 0x10; // bit 4: FRB3 failure
  static time_t rst_time = 0;
  static char postcodes_last[256] = {0};
  char postcodes[256] = {0};
  struct stat file_stat;
  int ret = READING_NA, rc, len;
  char sensor_name[32] = {0};
  char error[32] = {0};

  if (fru_id != 1) {
    syslog(LOG_ERR, "Not Supported Operation for fru %d", fru_id);
    return READING_NA;
  }

  if (stat("/tmp/rst_touch", &file_stat) == 0 && file_stat.st_mtime > rst_time) {
    rst_time = file_stat.st_mtime;
    // assume fail till we know it is not
    frb3_fail = 0x10; // bit 4: FRB3 failure
    retry = 0;
    // cache current postcode buffer
    memset(postcodes_last, 0, sizeof(postcodes_last));
    pal_get_80port_record(FRU_MB, NULL, 0, postcodes_last, (uint8_t *)&len);
  }

  if (frb3_fail) {
    // KCS transaction
    if (stat("/tmp/kcs_touch", &file_stat) == 0 && file_stat.st_mtime > rst_time)
      frb3_fail = 0;

    // Port 80 updated
    memset(postcodes, 0, sizeof(postcodes_last));
    rc = pal_get_80port_record(FRU_MB, NULL, 0, postcodes, (uint8_t *)&len);
    if (rc == PAL_EOK && memcmp(postcodes_last, postcodes, 256) != 0) {
      frb3_fail = 0;
    }

    // BIOS POST COMPLT, in case BMC reboot when system idle in OS
    if (gpio_get(GPIO_FM_BIOS_POST_CMPLT_N) == GPIO_VALUE_LOW)
      frb3_fail = 0;
  }

  if (frb3_fail)
    retry++;
  else
    retry = 0;

  if (retry == MAX_READ_RETRY) {
    pal_get_sensor_name(fru_id, sensor_num, sensor_name);
    snprintf(error, sizeof(error), "FRB3 failure");
    _print_sensor_discrete_log(fru_id, sensor_num, sensor_name, frb3_fail, error);
  }

  *value = (float)frb3_fail;
  ret = 0;

  return ret;
}

int
pal_get_fru_list(char *list) {

  strcpy(list, pal_fru_list);
  return 0;
}

int
pal_get_fru_id(char *str, uint8_t *fru) {
  if (!strcmp(str, "all")) {
    *fru = FRU_ALL;
  } else if (!strcmp(str, "mb")) {
    *fru = FRU_MB;
  } else if (!strcmp(str, "nic")) {
    *fru = FRU_NIC;
  } else if (!strncmp(str, "fru", 3)) {
    *fru = atoi(&str[3]);
    if (*fru <= FRU_NIC || *fru > MAX_NUM_FRUS)
      return -1;
  } else {
    syslog(LOG_WARNING, "pal_get_fru_id: Wrong fru#%s", str);
    return -1;
  }

  return 0;
}

int
pal_get_fru_name(uint8_t fru, char *name) {
  switch(fru) {
    case FRU_MB:
      strcpy(name, "mb");
      break;
    case FRU_NIC:
      strcpy(name, "nic");
      break;
    default:
      if (fru > MAX_NUM_FRUS)
        return -1;
      sprintf(name, "fru%d", fru);
      break;
  }
  return 0;
}

int
pal_get_fru_sdr_path(uint8_t fru, char *path) {
  return -1;
}

int
pal_sensor_sdr_init(uint8_t fru, sensor_info_t *sinfo) {
  return -1;
}

int
pal_get_fru_sensor_list(uint8_t fru, uint8_t **sensor_list, int *cnt) {
  switch(fru) {
  case FRU_MB:
    *sensor_list = (uint8_t *) mb_sensor_list;
    *cnt = mb_sensor_cnt;
    break;
  case FRU_NIC:
    *sensor_list = (uint8_t *) nic_sensor_list;
    *cnt = nic_sensor_cnt;
    break;
  default:
    if (fru > MAX_NUM_FRUS)
      return -1;
    // Nothing to read yet.
    *sensor_list = NULL;
    *cnt = 0;
  }

  return 0;
}

int
pal_fruid_write(uint8_t fru, char *path) {
  return 0;
}

int
pal_sensor_read(uint8_t fru, uint8_t sensor_num, void *value) {
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  int ret;

  switch(fru) {
  case FRU_MB:
    sprintf(key, "mb_sensor%d", sensor_num);
    break;
  case FRU_NIC:
    sprintf(key, "nic_sensor%d", sensor_num);
    break;
  default:
    return -1;
  }
  ret = edb_cache_get(key, str);
  if(ret < 0) {
#ifdef DEBUG
    syslog(LOG_WARNING, "pal_sensor_read: cache_get %s failed.", key);
#endif
    return ret;
  }

  if(strcmp(str, "NA") == 0)
    return -1;

  *((float*)value) = atof(str);

  return ret;
}

int
pal_sensor_read_raw(uint8_t fru, uint8_t sensor_num, void *value) {
  uint8_t status;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  int ret;
  float volt, curr;
  static uint8_t poweron_10s_flag = 0;
  uint8_t retry = MAX_READ_RETRY;
  bool server_off;

  switch(fru) {
  case FRU_MB:
    sprintf(key, "mb_sensor%d", sensor_num);
    server_off = is_server_off();
    if (server_off) {
      poweron_10s_flag = 0;
      // Power is OFF, so only some of the sensors can be read
      switch(sensor_num) {
      // Temp. Sensors
      case MB_SENSOR_INLET_TEMP:
        ret = read_temp(MB_INLET_TEMP_DEVICE, (float*) value);
        break;
      case MB_SENSOR_OUTLET_TEMP:
        ret = read_temp(MB_OUTLET_TEMP_DEVICE, (float*) value);
        break;
      case MB_SENSOR_INLET_REMOTE_TEMP:
        ret = read_temp_attr(MB_INLET_TEMP_DEVICE, "temp2_input", (float*) value);
        if (!ret)
          apply_inlet_correction((float *) value);
        break;
      case MB_SENSOR_OUTLET_REMOTE_TEMP:
        ret = read_temp_attr(MB_OUTLET_TEMP_DEVICE, "temp2_input", (float*) value);
        break;
      case MB_SENSOR_P12V:
        ret = read_adc_value(ADC_PIN2, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P1V05:
        ret = read_adc_value(ADC_PIN3, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_PVNN_PCH_STBY:
        ret = read_adc_value(ADC_PIN4, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P3V3_STBY:
        ret = read_adc_value(ADC_PIN5, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P5V_STBY:
        ret = read_adc_value(ADC_PIN6, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P3V_BAT:
        gpio_set(GPIO_BAT_SENSE_EN_N, 0);
        msleep(10);
        ret = read_adc_value(ADC_PIN7, ADC_VALUE, (float*) value);
        gpio_set(GPIO_BAT_SENSE_EN_N, 1);
        break;

      // Hot Swap Controller
      case MB_SENSOR_HSC_IN_VOLT:
        ret = read_sensor_reading_from_ME(MB_SENSOR_HSC_IN_VOLT, (float*) value);
        break;
      case MB_SENSOR_HSC_OUT_CURR:
        ret = read_hsc_current_value((float*) value);
        break;
      case MB_SENSOR_HSC_IN_POWER:
        ret = read_sensor_reading_from_ME(MB_SENSOR_HSC_IN_POWER, (float*) value);
        break;
      case MB_SENSOR_POWER_FAIL:
        ret = read_CPLD_power_fail_sts (fru, sensor_num, (float*) value, poweron_10s_flag);
        break;
      default:
        ret = READING_NA;
        break;
      }
    } else {
      if((poweron_10s_flag < 5) && ((sensor_num == MB_SENSOR_HSC_IN_VOLT) ||
         (sensor_num == MB_SENSOR_HSC_OUT_CURR) || (sensor_num == MB_SENSOR_HSC_IN_POWER) ||
         (sensor_num == MB_SENSOR_FAN0_TACH) || (sensor_num == MB_SENSOR_FAN1_TACH))) {
        if(sensor_num == MB_SENSOR_HSC_IN_POWER){
          poweron_10s_flag++;
        }
        ret = READING_NA;
        break;
      }
      switch(sensor_num) {
      // Temp. Sensors
      case MB_SENSOR_INLET_TEMP:
        ret = read_temp(MB_INLET_TEMP_DEVICE, (float*) value);
        break;
      case MB_SENSOR_OUTLET_TEMP:
        ret = read_temp(MB_OUTLET_TEMP_DEVICE, (float*) value);
        break;
      case MB_SENSOR_INLET_REMOTE_TEMP:
        ret = read_temp_attr(MB_INLET_TEMP_DEVICE, "temp2_input", (float*) value);
        if (!ret)
          apply_inlet_correction((float *) value);
        break;
      case MB_SENSOR_OUTLET_REMOTE_TEMP:
        ret = read_temp_attr(MB_OUTLET_TEMP_DEVICE, "temp2_input", (float*) value);
        break;
      // Fan Sensors
      case MB_SENSOR_FAN0_TACH:
        ret = read_fan_value_f(FAN0, FAN_TACH_RPM, (float*) value);
        break;
      case MB_SENSOR_FAN1_TACH:
        ret = read_fan_value_f(FAN1, FAN_TACH_RPM, (float*) value);
        break;
      // Various Voltages
      case MB_SENSOR_P3V3:
        ret = read_adc_value(ADC_PIN0, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P5V:
        ret = read_adc_value(ADC_PIN1, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P12V:
        ret = read_adc_value(ADC_PIN2, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P1V05:
        ret = read_adc_value(ADC_PIN3, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_PVNN_PCH_STBY:
        ret = read_adc_value(ADC_PIN4, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P3V3_STBY:
        ret = read_adc_value(ADC_PIN5, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P5V_STBY:
        ret = read_adc_value(ADC_PIN6, ADC_VALUE, (float*) value);
        break;
      case MB_SENSOR_P3V_BAT:
        gpio_set(GPIO_BAT_SENSE_EN_N, 0);
        msleep(10);
        ret = read_adc_value(ADC_PIN7, ADC_VALUE, (float*) value);
        gpio_set(GPIO_BAT_SENSE_EN_N, 1);
        break;

      // Hot Swap Controller
      case MB_SENSOR_HSC_IN_VOLT:
        ret = read_sensor_reading_from_ME(MB_SENSOR_HSC_IN_VOLT, (float*) value);
        break;
      case MB_SENSOR_HSC_OUT_CURR:
        ret = read_hsc_current_value((float*) value);
        break;
      case MB_SENSOR_HSC_IN_POWER:
        ret = read_sensor_reading_from_ME(MB_SENSOR_HSC_IN_POWER, (float*) value);
        break;
      //CPU, DIMM, PCH Temp
      case MB_SENSOR_CPU0_TEMP:
      case MB_SENSOR_CPU1_TEMP:
        ret = read_cpu_temp(sensor_num, (float*) value);
        break;
      case MB_SENSOR_CPU0_DIMM_GRPA_TEMP:
      case MB_SENSOR_CPU0_DIMM_GRPB_TEMP:
      case MB_SENSOR_CPU1_DIMM_GRPC_TEMP:
      case MB_SENSOR_CPU1_DIMM_GRPD_TEMP:
        ret = read_dimm_temp(sensor_num, (float*) value);
        break;
      case MB_SENSOR_CPU0_PKG_POWER:
      case MB_SENSOR_CPU1_PKG_POWER:
        ret = read_cpu_package_power(sensor_num, (float*) value);
        break;
      case MB_SENSOR_PCH_TEMP:
        ret = read_sensor_reading_from_ME(MB_SENSOR_PCH_TEMP, (float*) value);
        break;
      //VR Sensors
      case MB_SENSOR_VR_CPU0_VCCIN_TEMP:
        ret = vr_read_temp(VR_CPU0_VCCIN, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VCCIN_CURR:
        ret = vr_read_curr(VR_CPU0_VCCIN, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VCCIN_VOLT:
        ret = vr_read_volt(VR_CPU0_VCCIN, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VCCIN_POWER:
        ret = vr_read_power(VR_CPU0_VCCIN, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VSA_TEMP:
        ret = vr_read_temp(VR_CPU0_VSA, VR_LOOP_PAGE_1, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VSA_CURR:
        ret = vr_read_curr(VR_CPU0_VSA, VR_LOOP_PAGE_1, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VSA_VOLT:
        ret = vr_read_volt(VR_CPU0_VSA, VR_LOOP_PAGE_1, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VSA_POWER:
        ret = vr_read_power(VR_CPU0_VSA, VR_LOOP_PAGE_1, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VCCIO_TEMP:
        ret = vr_read_temp(VR_CPU0_VCCIO, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VCCIO_CURR:
        ret = vr_read_curr(VR_CPU0_VCCIO, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VCCIO_VOLT:
        ret = vr_read_volt(VR_CPU0_VCCIO, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VCCIO_POWER:
        ret = vr_read_power(VR_CPU0_VCCIO, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VDDQ_GRPA_TEMP:
        ret = vr_read_temp(g_vr_cpu0_vddq_abc, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VDDQ_GRPA_CURR:
        ret = vr_read_curr(g_vr_cpu0_vddq_abc, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VDDQ_GRPA_VOLT:
        ret = vr_read_volt(g_vr_cpu0_vddq_abc, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VDDQ_GRPA_POWER:
        ret = vr_read_power(g_vr_cpu0_vddq_abc, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VDDQ_GRPB_TEMP:
        ret = vr_read_temp(g_vr_cpu0_vddq_def, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VDDQ_GRPB_CURR:
        ret = vr_read_curr(g_vr_cpu0_vddq_def, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VDDQ_GRPB_VOLT:
        ret = vr_read_volt(g_vr_cpu0_vddq_def, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU0_VDDQ_GRPB_POWER:
        ret = vr_read_power(g_vr_cpu0_vddq_def, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_CPU1_VCCIN_TEMP:
        if (is_cpu1_socket_occupy())
          ret = vr_read_temp(VR_CPU1_VCCIN, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VCCIN_CURR:
        if (is_cpu1_socket_occupy())
          ret = vr_read_curr(VR_CPU1_VCCIN, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VCCIN_VOLT:
        if (is_cpu1_socket_occupy())
          ret = vr_read_volt(VR_CPU1_VCCIN, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VCCIN_POWER:
        if (is_cpu1_socket_occupy())
          ret = vr_read_power(VR_CPU1_VCCIN, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VSA_TEMP:
        if (is_cpu1_socket_occupy())
          ret = vr_read_temp(VR_CPU1_VSA, VR_LOOP_PAGE_1, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VSA_CURR:
        if (is_cpu1_socket_occupy())
          ret = vr_read_curr(VR_CPU1_VSA, VR_LOOP_PAGE_1, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VSA_VOLT:
        if (is_cpu1_socket_occupy())
          ret = vr_read_volt(VR_CPU1_VSA, VR_LOOP_PAGE_1, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VSA_POWER:
        if (is_cpu1_socket_occupy())
          ret = vr_read_power(VR_CPU1_VSA, VR_LOOP_PAGE_1, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VCCIO_TEMP:
        if (is_cpu1_socket_occupy())
          ret = vr_read_temp(VR_CPU1_VCCIO, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VCCIO_CURR:
        if (is_cpu1_socket_occupy())
          ret = vr_read_curr(VR_CPU1_VCCIO, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VCCIO_VOLT:
        if (is_cpu1_socket_occupy())
          ret = vr_read_volt(VR_CPU1_VCCIO, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VCCIO_POWER:
        if (is_cpu1_socket_occupy())
          ret = vr_read_power(VR_CPU1_VCCIO, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VDDQ_GRPC_TEMP:
        if (is_cpu1_socket_occupy())
          ret = vr_read_temp(g_vr_cpu1_vddq_ghj, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VDDQ_GRPC_CURR:
        if (is_cpu1_socket_occupy())
          ret = vr_read_curr(g_vr_cpu1_vddq_ghj, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VDDQ_GRPC_VOLT:
        if (is_cpu1_socket_occupy())
          ret = vr_read_volt(g_vr_cpu1_vddq_ghj, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VDDQ_GRPC_POWER:
        if (is_cpu1_socket_occupy())
          ret = vr_read_power(g_vr_cpu1_vddq_ghj, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VDDQ_GRPD_TEMP:
        if (is_cpu1_socket_occupy())
          ret = vr_read_temp(g_vr_cpu1_vddq_klm, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VDDQ_GRPD_CURR:
        if (is_cpu1_socket_occupy())
          ret = vr_read_curr(g_vr_cpu1_vddq_klm, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VDDQ_GRPD_VOLT:
        if (is_cpu1_socket_occupy())
          ret = vr_read_volt(g_vr_cpu1_vddq_klm, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_CPU1_VDDQ_GRPD_POWER:
        if (is_cpu1_socket_occupy())
          ret = vr_read_power(g_vr_cpu1_vddq_klm, VR_LOOP_PAGE_0, (float*) value);
        else
          ret = READING_NA;
        break;
      case MB_SENSOR_VR_PCH_PVNN_TEMP:
        ret = vr_read_temp(VR_PCH_PVNN, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_PCH_PVNN_CURR:
        ret = vr_read_curr(VR_PCH_PVNN, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_PCH_PVNN_VOLT:
        ret = vr_read_volt(VR_PCH_PVNN, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_PCH_PVNN_POWER:
        ret = vr_read_power(VR_PCH_PVNN, VR_LOOP_PAGE_0, (float*) value);
        break;
      case MB_SENSOR_VR_PCH_P1V05_TEMP:
        ret = vr_read_temp(VR_PCH_P1V05, VR_LOOP_PAGE_1, (float*) value);
        break;
      case MB_SENSOR_VR_PCH_P1V05_CURR:
        ret = vr_read_curr(VR_PCH_P1V05, VR_LOOP_PAGE_1, (float*) value);
        break;
      case MB_SENSOR_VR_PCH_P1V05_VOLT:
        ret = vr_read_volt(VR_PCH_P1V05, VR_LOOP_PAGE_1, (float*) value);
        break;
      case MB_SENSOR_VR_PCH_P1V05_POWER:
        ret = vr_read_power(VR_PCH_P1V05, VR_LOOP_PAGE_1, (float*) value);
        break;
      case MB_SENSOR_C2_AVA_FTEMP:
      case MB_SENSOR_C2_AVA_RTEMP:
      case MB_SENSOR_C3_AVA_FTEMP:
      case MB_SENSOR_C3_AVA_RTEMP:
      case MB_SENSOR_C4_AVA_FTEMP:
      case MB_SENSOR_C4_AVA_RTEMP:
        ret = read_ava_temp(sensor_num, (float*) value);
        break;
      case MB_SENSOR_C2_1_NVME_CTEMP:
      case MB_SENSOR_C2_2_NVME_CTEMP:
      case MB_SENSOR_C2_3_NVME_CTEMP:
      case MB_SENSOR_C2_4_NVME_CTEMP:
      case MB_SENSOR_C3_1_NVME_CTEMP:
      case MB_SENSOR_C3_2_NVME_CTEMP:
      case MB_SENSOR_C3_3_NVME_CTEMP:
      case MB_SENSOR_C3_4_NVME_CTEMP:
      case MB_SENSOR_C4_1_NVME_CTEMP:
      case MB_SENSOR_C4_2_NVME_CTEMP:
      case MB_SENSOR_C4_3_NVME_CTEMP:
      case MB_SENSOR_C4_4_NVME_CTEMP:
        ret = read_nvme_temp(sensor_num, (float*) value);
        break;
      case MB_SENSOR_C2_P12V_INA230_VOL:
      case MB_SENSOR_C3_P12V_INA230_VOL:
      case MB_SENSOR_C4_P12V_INA230_VOL:
      case MB_SENSOR_CONN_P12V_INA230_VOL:
      case MB_SENSOR_C2_P12V_INA230_CURR:
      case MB_SENSOR_C3_P12V_INA230_CURR:
      case MB_SENSOR_C4_P12V_INA230_CURR:
      case MB_SENSOR_CONN_P12V_INA230_CURR:
      case MB_SENSOR_C2_P12V_INA230_PWR:
      case MB_SENSOR_C3_P12V_INA230_PWR:
      case MB_SENSOR_C4_P12V_INA230_PWR:
      case MB_SENSOR_CONN_P12V_INA230_PWR:
        ret = read_INA230 (sensor_num, (float*) value, poweron_10s_flag);
        break;
      case MB_SENSOR_POWER_FAIL:
        ret = read_CPLD_power_fail_sts (fru, sensor_num, (float*) value, poweron_10s_flag);
        break;
      case MB_SENSOR_MEMORY_LOOP_FAIL:
        ret = check_postcodes(FRU_MB, sensor_num, (float*) value);
        break;
      case MB_SENSOR_PROCESSOR_FAIL:
        ret = check_frb3(FRU_MB, sensor_num, (float*) value);
        break;

      default:
        return -1;
      }
    }
    if (is_server_off() != server_off) {
      /* server power status changed while we were reading the sensor.
       * this sensor is potentially NA. */
      return pal_sensor_read_raw(fru, sensor_num, value);
    }
    break;
  case FRU_NIC:
    sprintf(key, "nic_sensor%d", sensor_num);
    switch(sensor_num) {
    case MEZZ_SENSOR_TEMP:
      ret = read_nic_temp(MEZZ_TEMP_DEVICE, (float*) value);
      break;
    default:
      return -1;
    }
    break;
  default:
    return -1;
  }

  if (ret) {
    if (ret == READING_NA || ret == -1) {
      strcpy(str, "NA");
    } else {
      return ret;
    }
  } else {
    sprintf(str, "%.2f",*((float*)value));
  }

  if(edb_cache_set(key, str) < 0) {
#ifdef DEBUG
     syslog(LOG_WARNING, "pal_sensor_read_raw: cache_set key = %s, str = %s failed.", key, str);
#endif
    return -1;
  } else {
    return ret;
  }

  return 0;
}

int
pal_sensor_threshold_flag(uint8_t fru, uint8_t snr_num, uint16_t *flag) {

  return 0;
}

int
pal_get_sensor_threshold(uint8_t fru, uint8_t sensor_num, uint8_t thresh, void *value) {
  float *val = (float*) value;
  sensor_thresh_array_init();
  switch(fru) {
  case FRU_MB:
    *val = mb_sensor_threshold[sensor_num][thresh];
    break;
  case FRU_NIC:
    *val = nic_sensor_threshold[sensor_num][thresh];
    break;
  default:
    return -1;
  }
  return 0;
}

int
pal_get_sensor_name(uint8_t fru, uint8_t sensor_num, char *name) {
  switch(fru) {
  case FRU_MB:
    switch(sensor_num) {
    case MB_SENSOR_INLET_TEMP:
      sprintf(name, "MB_INLET_TEMP");
      break;
    case MB_SENSOR_OUTLET_TEMP:
      sprintf(name, "MB_OUTLET_TEMP");
      break;
    case MB_SENSOR_INLET_REMOTE_TEMP:
      sprintf(name, "MB_INLET_REMOTE_TEMP");
      break;
    case MB_SENSOR_OUTLET_REMOTE_TEMP:
      sprintf(name, "MB_OUTLET_REMOTE_TEMP");
      break;
    case MB_SENSOR_FAN0_TACH:
      sprintf(name, "MB_FAN0_TACH");
      break;
    case MB_SENSOR_FAN1_TACH:
      sprintf(name, "MB_FAN1_TACH");
      break;
    case MB_SENSOR_P3V3:
      sprintf(name, "MB_P3V3");
      break;
    case MB_SENSOR_P5V:
      sprintf(name, "MB_P5V");
      break;
    case MB_SENSOR_P12V:
      sprintf(name, "MB_P12V");
      break;
    case MB_SENSOR_P1V05:
      sprintf(name, "MB_P1V05");
      break;
    case MB_SENSOR_PVNN_PCH_STBY:
      sprintf(name, "MB_PVNN_PCH_STBY");
      break;
    case MB_SENSOR_P3V3_STBY:
      sprintf(name, "MB_P3V3_STBY");
      break;
    case MB_SENSOR_P5V_STBY:
      sprintf(name, "MB_P5V_STBY");
      break;
    case MB_SENSOR_P3V_BAT:
      sprintf(name, "MB_P3V_BAT");
      break;
    case MB_SENSOR_HSC_IN_VOLT:
      sprintf(name, "MB_HSC_IN_VOLT");
      break;
    case MB_SENSOR_HSC_OUT_CURR:
      sprintf(name, "MB_HSC_OUT_CURR");
      break;
    case MB_SENSOR_HSC_IN_POWER:
      sprintf(name, "MB_HSC_IN_POWER");
      break;
    case MB_SENSOR_CPU0_TEMP:
      sprintf(name, "MB_CPU0_TEMP");
      break;
    case MB_SENSOR_CPU0_TJMAX:
      sprintf(name, "MB_CPU0_TJMAX");
      break;
    case MB_SENSOR_CPU0_PKG_POWER:
      sprintf(name, "MB_CPU0_PKG_POWER");
      break;
    case MB_SENSOR_CPU1_TEMP:
      sprintf(name, "MB_CPU1_TEMP");
      break;
    case MB_SENSOR_CPU1_TJMAX:
      sprintf(name, "MB_CPU1_TJMAX");
      break;
    case MB_SENSOR_CPU1_PKG_POWER:
      sprintf(name, "MB_CPU1_PKG_POWER");
      break;
    case MB_SENSOR_PCH_TEMP:
      sprintf(name, "MB_PCH_TEMP");
      break;
    case MB_SENSOR_CPU0_DIMM_GRPA_TEMP:
      sprintf(name, "MB_CPU0_DIMM_GRPA_TEMP");
      break;
    case MB_SENSOR_CPU0_DIMM_GRPB_TEMP:
      sprintf(name, "MB_CPU0_DIMM_GRPB_TEMP");
      break;
    case MB_SENSOR_CPU1_DIMM_GRPC_TEMP:
      sprintf(name, "MB_CPU1_DIMM_GRPC_TEMP");
      break;
    case MB_SENSOR_CPU1_DIMM_GRPD_TEMP:
      sprintf(name, "MB_CPU1_DIMM_GRPD_TEMP");
      break;
    case MB_SENSOR_VR_CPU0_VCCIN_TEMP:
      sprintf(name, "MB_VR_CPU0_VCCIN_TEMP");
      break;
    case MB_SENSOR_VR_CPU0_VCCIN_CURR:
      sprintf(name, "MB_VR_CPU0_VCCIN_CURR");
      break;
    case MB_SENSOR_VR_CPU0_VCCIN_VOLT:
      sprintf(name, "MB_VR_CPU0_VCCIN_VOLT");
      break;
    case MB_SENSOR_VR_CPU0_VCCIN_POWER:
      sprintf(name, "MB_VR_CPU0_VCCIN_POWER");
      break;
    case MB_SENSOR_VR_CPU0_VSA_TEMP:
      sprintf(name, "MB_VR_CPU0_VSA_TEMP");
      break;
    case MB_SENSOR_VR_CPU0_VSA_CURR:
      sprintf(name, "MB_VR_CPU0_VSA_CURR");
      break;
    case MB_SENSOR_VR_CPU0_VSA_VOLT:
      sprintf(name, "MB_VR_CPU0_VSA_VOLT");
      break;
    case MB_SENSOR_VR_CPU0_VSA_POWER:
      sprintf(name, "MB_VR_CPU0_VSA_POWER");
      break;
    case MB_SENSOR_VR_CPU0_VCCIO_TEMP:
      sprintf(name, "MB_VR_CPU0_VCCIO_TEMP");
      break;
    case MB_SENSOR_VR_CPU0_VCCIO_CURR:
      sprintf(name, "MB_VR_CPU0_VCCIO_CURR");
      break;
    case MB_SENSOR_VR_CPU0_VCCIO_VOLT:
      sprintf(name, "MB_VR_CPU0_VCCIO_VOLT");
      break;
    case MB_SENSOR_VR_CPU0_VCCIO_POWER:
      sprintf(name, "MB_VR_CPU0_VCCIO_POWER");
      break;
    case MB_SENSOR_VR_CPU0_VDDQ_GRPA_TEMP:
      sprintf(name, "MB_VR_CPU0_VDDQ_GRPA_TEMP");
      break;
    case MB_SENSOR_VR_CPU0_VDDQ_GRPA_CURR:
      sprintf(name, "MB_VR_CPU0_VDDQ_GRPA_CURR");
      break;
    case MB_SENSOR_VR_CPU0_VDDQ_GRPA_VOLT:
      sprintf(name, "MB_VR_CPU0_VDDQ_GRPA_VOLT");
      break;
    case MB_SENSOR_VR_CPU0_VDDQ_GRPA_POWER:
      sprintf(name, "MB_VR_CPU0_VDDQ_GRPA_POWER");
      break;
    case MB_SENSOR_VR_CPU0_VDDQ_GRPB_TEMP:
      sprintf(name, "MB_VR_CPU0_VDDQ_GRPB_TEMP");
      break;
    case MB_SENSOR_VR_CPU0_VDDQ_GRPB_CURR:
      sprintf(name, "MB_VR_CPU0_VDDQ_GRPB_CURR");
      break;
    case MB_SENSOR_VR_CPU0_VDDQ_GRPB_VOLT:
      sprintf(name, "MB_VR_CPU0_VDDQ_GRPB_VOLT");
      break;
    case MB_SENSOR_VR_CPU0_VDDQ_GRPB_POWER:
      sprintf(name, "MB_VR_CPU0_VDDQ_GRPB_POWER");
      break;
    case MB_SENSOR_VR_CPU1_VCCIN_TEMP:
      sprintf(name, "MB_VR_CPU1_VCCIN_TEMP");
      break;
    case MB_SENSOR_VR_CPU1_VCCIN_CURR:
      sprintf(name, "MB_VR_CPU1_VCCIN_CURR");
      break;
    case MB_SENSOR_VR_CPU1_VCCIN_VOLT:
      sprintf(name, "MB_VR_CPU1_VCCIN_VOLT");
      break;
    case MB_SENSOR_VR_CPU1_VCCIN_POWER:
      sprintf(name, "MB_VR_CPU1_VCCIN_POWER");
      break;
    case MB_SENSOR_VR_CPU1_VSA_TEMP:
      sprintf(name, "MB_VR_CPU1_VSA_TEMP");
      break;
    case MB_SENSOR_VR_CPU1_VSA_CURR:
      sprintf(name, "MB_VR_CPU1_VSA_CURR");
      break;
    case MB_SENSOR_VR_CPU1_VSA_VOLT:
      sprintf(name, "MB_VR_CPU1_VSA_VOLT");
      break;
    case MB_SENSOR_VR_CPU1_VSA_POWER:
      sprintf(name, "MB_VR_CPU1_VSA_POWER");
      break;
    case MB_SENSOR_VR_CPU1_VCCIO_TEMP:
      sprintf(name, "MB_VR_CPU1_VCCIO_TEMP");
      break;
    case MB_SENSOR_VR_CPU1_VCCIO_CURR:
      sprintf(name, "MB_VR_CPU1_VCCIO_CURR");
      break;
    case MB_SENSOR_VR_CPU1_VCCIO_VOLT:
      sprintf(name, "MB_VR_CPU1_VCCIO_VOLT");
      break;
    case MB_SENSOR_VR_CPU1_VCCIO_POWER:
      sprintf(name, "MB_VR_CPU1_VCCIO_POWER");
      break;
    case MB_SENSOR_VR_CPU1_VDDQ_GRPC_TEMP:
      sprintf(name, "MB_VR_CPU1_VDDQ_GRPC_TEMP");
      break;
    case MB_SENSOR_VR_CPU1_VDDQ_GRPC_CURR:
      sprintf(name, "MB_VR_CPU1_VDDQ_GRPC_CURR");
      break;
    case MB_SENSOR_VR_CPU1_VDDQ_GRPC_VOLT:
      sprintf(name, "MB_VR_CPU1_VDDQ_GRPC_VOLT");
      break;
    case MB_SENSOR_VR_CPU1_VDDQ_GRPC_POWER:
      sprintf(name, "MB_VR_CPU1_VDDQ_GRPC_POWER");
      break;
    case MB_SENSOR_VR_CPU1_VDDQ_GRPD_TEMP:
      sprintf(name, "MB_VR_CPU1_VDDQ_GRPD_TEMP");
      break;
    case MB_SENSOR_VR_CPU1_VDDQ_GRPD_CURR:
      sprintf(name, "MB_VR_CPU1_VDDQ_GRPD_CURR");
      break;
    case MB_SENSOR_VR_CPU1_VDDQ_GRPD_VOLT:
      sprintf(name, "MB_VR_CPU1_VDDQ_GRPD_VOLT");
      break;
    case MB_SENSOR_VR_CPU1_VDDQ_GRPD_POWER:
      sprintf(name, "MB_VR_CPU1_VDDQ_GRPD_POWER");
      break;
    case MB_SENSOR_VR_PCH_PVNN_TEMP:
      sprintf(name, "MB_VR_PCH_PVNN_TEMP");
      break;
    case MB_SENSOR_VR_PCH_PVNN_CURR:
      sprintf(name, "MB_VR_PCH_PVNN_CURR");
      break;
    case MB_SENSOR_VR_PCH_PVNN_VOLT:
      sprintf(name, "MB_VR_PCH_PVNN_VOLT");
      break;
    case MB_SENSOR_VR_PCH_PVNN_POWER:
      sprintf(name, "MB_VR_PCH_PVNN_POWER");
      break;
    case MB_SENSOR_VR_PCH_P1V05_TEMP:
      sprintf(name, "MB_VR_PCH_P1V05_TEMP");
      break;
    case MB_SENSOR_VR_PCH_P1V05_CURR:
      sprintf(name, "MB_VR_PCH_P1V05_CURR");
      break;
    case MB_SENSOR_VR_PCH_P1V05_VOLT:
      sprintf(name, "MB_VR_PCH_P1V05_VOLT");
      break;
    case MB_SENSOR_VR_PCH_P1V05_POWER:
      sprintf(name, "MB_VR_PCH_P1V05_POWER");
      break;
    case MB_SENSOR_C2_AVA_FTEMP:
      sprintf(name, "MB_C2_AVA_FTEMP");
      break;
    case MB_SENSOR_C2_AVA_RTEMP:
      sprintf(name, "MB_C2_AVA_RTEMP");
      break;
    case MB_SENSOR_C2_1_NVME_CTEMP:
      sprintf(name, "MB_C2_0_NVME_CTEMP");
      break;
    case MB_SENSOR_C2_2_NVME_CTEMP:
      sprintf(name, "MB_C2_1_NVME_CTEMP");
      break;
    case MB_SENSOR_C2_3_NVME_CTEMP:
      sprintf(name, "MB_C2_2_NVME_CTEMP");
      break;
    case MB_SENSOR_C2_4_NVME_CTEMP:
      sprintf(name, "MB_C2_3_NVME_CTEMP");
      break;
    case MB_SENSOR_C3_AVA_FTEMP:
      sprintf(name, "MB_C3_AVA_FTEMP");
      break;
    case MB_SENSOR_C3_AVA_RTEMP:
      sprintf(name, "MB_C3_AVA_RTEMP");
      break;
    case MB_SENSOR_C3_1_NVME_CTEMP:
      sprintf(name, "MB_C3_0_NVME_CTEMP");
      break;
    case MB_SENSOR_C3_2_NVME_CTEMP:
      sprintf(name, "MB_C3_1_NVME_CTEMP");
      break;
    case MB_SENSOR_C3_3_NVME_CTEMP:
      sprintf(name, "MB_C3_2_NVME_CTEMP");
      break;
    case MB_SENSOR_C3_4_NVME_CTEMP:
      sprintf(name, "MB_C3_3_NVME_CTEMP");
      break;
    case MB_SENSOR_C4_AVA_FTEMP:
      sprintf(name, "MB_C4_AVA_FTEMP");
      break;
    case MB_SENSOR_C4_AVA_RTEMP:
      sprintf(name, "MB_C4_AVA_RTEMP");
      break;
    case MB_SENSOR_C4_1_NVME_CTEMP:
      sprintf(name, "MB_C4_0_NVME_CTEMP");
      break;
    case MB_SENSOR_C4_2_NVME_CTEMP:
      sprintf(name, "MB_C4_1_NVME_CTEMP");
      break;
    case MB_SENSOR_C4_3_NVME_CTEMP:
      sprintf(name, "MB_C4_2_NVME_CTEMP");
      break;
    case MB_SENSOR_C4_4_NVME_CTEMP:
      sprintf(name, "MB_C4_3_NVME_CTEMP");
      break;
    case MB_SENSOR_C2_P12V_INA230_VOL:
      sprintf(name, "MB_C2_P12V_INA230_VOL");
      break;
    case MB_SENSOR_C2_P12V_INA230_CURR:
      sprintf(name, "MB_C2_P12V_INA230_CURR");
      break;
    case MB_SENSOR_C2_P12V_INA230_PWR:
      sprintf(name, "MB_C2_P12V_INA230_PWR");
      break;
    case MB_SENSOR_C3_P12V_INA230_VOL:
      sprintf(name, "MB_C3_P12V_INA230_VOL");
      break;
    case MB_SENSOR_C3_P12V_INA230_CURR:
      sprintf(name, "MB_C3_P12V_INA230_CURR");
      break;
    case MB_SENSOR_C3_P12V_INA230_PWR:
      sprintf(name, "MB_C3_P12V_INA230_PWR");
      break;
    case MB_SENSOR_C4_P12V_INA230_VOL:
      sprintf(name, "MB_C4_P12V_INA230_VOL");
      break;
    case MB_SENSOR_C4_P12V_INA230_CURR:
      sprintf(name, "MB_C4_P12V_INA230_CURR");
      break;
    case MB_SENSOR_C4_P12V_INA230_PWR:
      sprintf(name, "MB_C4_P12V_INA230_PWR");
      break;
    case MB_SENSOR_CONN_P12V_INA230_VOL:
      sprintf(name, "MB_CONN_P12V_INA230_VOL");
      break;
    case MB_SENSOR_CONN_P12V_INA230_CURR:
      sprintf(name, "MB_CONN_P12V_INA230_CURR");
      break;
    case MB_SENSOR_CONN_P12V_INA230_PWR:
      sprintf(name, "MB_CONN_P12V_INA230_PWR");
      break;
    case MB_SENSOR_POWER_FAIL:
      sprintf(name, "MB_POWER_FAIL");
      break;
    case MB_SENSOR_MEMORY_LOOP_FAIL:
      sprintf(name, "MB_MEMORY_LOOP_FAIL");
      break;
    case MB_SENSOR_PROCESSOR_FAIL:
      sprintf(name, "MB_PROCESSOR_FAIL");
      break;

    default:
      return -1;
    }
    break;
  case FRU_NIC:
    switch(sensor_num) {
    case MEZZ_SENSOR_TEMP:
      sprintf(name, "MEZZ_SENSOR_TEMP");
      break;
    default:
      return -1;
    }
    break;
  default:
    return -1;
  }
  return 0;
}

int
pal_get_sensor_units(uint8_t fru, uint8_t sensor_num, char *units) {
  switch(fru) {
  case FRU_MB:
    switch(sensor_num) {
    case MB_SENSOR_INLET_TEMP:
    case MB_SENSOR_OUTLET_TEMP:
    case MB_SENSOR_INLET_REMOTE_TEMP:
    case MB_SENSOR_OUTLET_REMOTE_TEMP:
    case MB_SENSOR_CPU0_TEMP:
    case MB_SENSOR_CPU0_TJMAX:
    case MB_SENSOR_CPU1_TEMP:
    case MB_SENSOR_CPU1_TJMAX:
    case MB_SENSOR_PCH_TEMP:
    case MB_SENSOR_CPU0_DIMM_GRPA_TEMP:
    case MB_SENSOR_CPU0_DIMM_GRPB_TEMP:
    case MB_SENSOR_CPU1_DIMM_GRPC_TEMP:
    case MB_SENSOR_CPU1_DIMM_GRPD_TEMP:
    case MB_SENSOR_VR_CPU0_VCCIN_TEMP:
    case MB_SENSOR_VR_CPU0_VSA_TEMP:
    case MB_SENSOR_VR_CPU0_VCCIO_TEMP:
    case MB_SENSOR_VR_CPU0_VDDQ_GRPA_TEMP:
    case MB_SENSOR_VR_CPU0_VDDQ_GRPB_TEMP:
    case MB_SENSOR_VR_CPU1_VCCIN_TEMP:
    case MB_SENSOR_VR_CPU1_VSA_TEMP:
    case MB_SENSOR_VR_CPU1_VCCIO_TEMP:
    case MB_SENSOR_VR_CPU1_VDDQ_GRPC_TEMP:
    case MB_SENSOR_VR_CPU1_VDDQ_GRPD_TEMP:
    case MB_SENSOR_VR_PCH_PVNN_TEMP:
    case MB_SENSOR_VR_PCH_P1V05_TEMP:
    case MB_SENSOR_C2_AVA_FTEMP:
    case MB_SENSOR_C2_AVA_RTEMP:
    case MB_SENSOR_C2_1_NVME_CTEMP:
    case MB_SENSOR_C2_2_NVME_CTEMP:
    case MB_SENSOR_C2_3_NVME_CTEMP:
    case MB_SENSOR_C2_4_NVME_CTEMP:
    case MB_SENSOR_C3_AVA_FTEMP:
    case MB_SENSOR_C3_AVA_RTEMP:
    case MB_SENSOR_C3_1_NVME_CTEMP:
    case MB_SENSOR_C3_2_NVME_CTEMP:
    case MB_SENSOR_C3_3_NVME_CTEMP:
    case MB_SENSOR_C3_4_NVME_CTEMP:
    case MB_SENSOR_C4_AVA_FTEMP:
    case MB_SENSOR_C4_AVA_RTEMP:
    case MB_SENSOR_C4_1_NVME_CTEMP:
    case MB_SENSOR_C4_2_NVME_CTEMP:
    case MB_SENSOR_C4_3_NVME_CTEMP:
    case MB_SENSOR_C4_4_NVME_CTEMP:
      sprintf(units, "C");
      break;
    case MB_SENSOR_FAN0_TACH:
    case MB_SENSOR_FAN1_TACH:
      sprintf(units, "RPM");
      break;
    case MB_SENSOR_P3V3:
    case MB_SENSOR_P5V:
    case MB_SENSOR_P12V:
    case MB_SENSOR_P1V05:
    case MB_SENSOR_PVNN_PCH_STBY:
    case MB_SENSOR_P3V3_STBY:
    case MB_SENSOR_P5V_STBY:
    case MB_SENSOR_P3V_BAT:
    case MB_SENSOR_HSC_IN_VOLT:
    case MB_SENSOR_VR_CPU0_VCCIN_VOLT:
    case MB_SENSOR_VR_CPU0_VSA_VOLT:
    case MB_SENSOR_VR_CPU0_VCCIO_VOLT:
    case MB_SENSOR_VR_CPU0_VDDQ_GRPA_VOLT:
    case MB_SENSOR_VR_CPU0_VDDQ_GRPB_VOLT:
    case MB_SENSOR_VR_CPU1_VCCIN_VOLT:
    case MB_SENSOR_VR_CPU1_VSA_VOLT:
    case MB_SENSOR_VR_CPU1_VCCIO_VOLT:
    case MB_SENSOR_VR_CPU1_VDDQ_GRPC_VOLT:
    case MB_SENSOR_VR_CPU1_VDDQ_GRPD_VOLT:
    case MB_SENSOR_VR_PCH_PVNN_VOLT:
    case MB_SENSOR_VR_PCH_P1V05_VOLT:
    case MB_SENSOR_C2_P12V_INA230_VOL:
    case MB_SENSOR_C3_P12V_INA230_VOL:
    case MB_SENSOR_C4_P12V_INA230_VOL:
    case MB_SENSOR_CONN_P12V_INA230_VOL:
      sprintf(units, "Volts");
      break;
    case MB_SENSOR_HSC_OUT_CURR:
    case MB_SENSOR_VR_CPU0_VCCIN_CURR:
    case MB_SENSOR_VR_CPU0_VSA_CURR:
    case MB_SENSOR_VR_CPU0_VCCIO_CURR:
    case MB_SENSOR_VR_CPU0_VDDQ_GRPA_CURR:
    case MB_SENSOR_VR_CPU0_VDDQ_GRPB_CURR:
    case MB_SENSOR_VR_CPU1_VCCIN_CURR:
    case MB_SENSOR_VR_CPU1_VSA_CURR:
    case MB_SENSOR_VR_CPU1_VCCIO_CURR:
    case MB_SENSOR_VR_CPU1_VDDQ_GRPC_CURR:
    case MB_SENSOR_VR_CPU1_VDDQ_GRPD_CURR:
    case MB_SENSOR_VR_PCH_PVNN_CURR:
    case MB_SENSOR_VR_PCH_P1V05_CURR:
    case MB_SENSOR_C2_P12V_INA230_CURR:
    case MB_SENSOR_C3_P12V_INA230_CURR:
    case MB_SENSOR_C4_P12V_INA230_CURR:
    case MB_SENSOR_CONN_P12V_INA230_CURR:
      sprintf(units, "Amps");
      break;
    case MB_SENSOR_HSC_IN_POWER:
    case MB_SENSOR_VR_CPU0_VCCIN_POWER:
    case MB_SENSOR_VR_CPU0_VSA_POWER:
    case MB_SENSOR_VR_CPU0_VCCIO_POWER:
    case MB_SENSOR_VR_CPU0_VDDQ_GRPA_POWER:
    case MB_SENSOR_VR_CPU0_VDDQ_GRPB_POWER:
    case MB_SENSOR_VR_CPU1_VCCIN_POWER:
    case MB_SENSOR_VR_CPU1_VSA_POWER:
    case MB_SENSOR_VR_CPU1_VCCIO_POWER:
    case MB_SENSOR_VR_CPU1_VDDQ_GRPC_POWER:
    case MB_SENSOR_VR_CPU1_VDDQ_GRPD_POWER:
    case MB_SENSOR_VR_PCH_PVNN_POWER:
    case MB_SENSOR_VR_PCH_P1V05_POWER:
    case MB_SENSOR_CPU0_PKG_POWER:
    case MB_SENSOR_CPU1_PKG_POWER:
    case MB_SENSOR_C2_P12V_INA230_PWR:
    case MB_SENSOR_C3_P12V_INA230_PWR:
    case MB_SENSOR_C4_P12V_INA230_PWR:
    case MB_SENSOR_CONN_P12V_INA230_PWR:
      sprintf(units, "Watts");
      break;
    default:
      return -1;
    }
    break;
  case FRU_NIC:
    switch(sensor_num) {
    case MEZZ_SENSOR_TEMP:
      sprintf(units, "C");
      break;
    default:
      return -1;
    }
    break;
  default:
    return -1;
  }
  return 0;
}

int
pal_get_fruid_path(uint8_t fru, char *path) {
  char fname[16] = {0};

  switch(fru) {
  case FRU_MB:
    sprintf(fname, "mb");
    break;
  case FRU_NIC:
    sprintf(fname, "nic");
    break;
  default:
    return -1;
  }

  sprintf(path, "/tmp/fruid_%s.bin", fname);

  return 0;
}

int
pal_get_fruid_eeprom_path(uint8_t fru, char *path) {
  switch(fru) {
  case FRU_MB:
    sprintf(path, "/sys/devices/platform/ast-i2c.6/i2c-6/6-0054/eeprom");
    break;
  default:
    return -1;
  }

  return 0;
}

int
pal_get_fruid_name(uint8_t fru, char *name) {
  switch(fru) {
  case FRU_MB:
    sprintf(name, "Mother Board");
    break;
  case FRU_NIC:
    sprintf(name, "NIC Mezzanine");
    break;
  default:
    return -1;
  }
  return 0;
}

int
pal_set_def_key_value() {

  int ret;
  int i;
  int fru;
  char key[MAX_KEY_LEN] = {0};
  char kpath[MAX_KEY_PATH_LEN] = {0};
  char value[MAX_VALUE_LEN] = {0};
  char cmd[MAX_VALUE_LEN] = {0};

  i = 0;
  while(strcmp(key_cfg[i].name, LAST_KEY)) {

    memset(key, 0, MAX_KEY_LEN);
    memset(kpath, 0, MAX_KEY_PATH_LEN);

    sprintf(kpath, KV_STORE, key_cfg[i].name);

    if (access(kpath, F_OK) == -1) {

      if ((ret = kv_set(key_cfg[i].name, key_cfg[i].def_val)) < 0) {
#ifdef DEBUG
          syslog(LOG_WARNING, "pal_set_def_key_value: kv_set failed. %d", ret);
#endif
      }
    }

    if (key_cfg[i].function) {
      key_cfg[i].function(KEY_AFTER_INI, key_cfg[i].name);
    }

    i++;
  }

  /* Actions to be taken on Power On Reset */
  if (pal_is_bmc_por()) {
    /* Clear all the SEL errors */
    memset(key, 0, MAX_KEY_LEN);

    /* Write the value "1" which means FRU_STATUS_GOOD */
    ret = pal_set_key_value(key, "1");

    /* Clear all the sensor health files*/
    memset(key, 0, MAX_KEY_LEN);

    /* Write the value "1" which means FRU_STATUS_GOOD */
    ret = pal_set_key_value(key, "1");
  }

  return 0;
}

int
pal_get_fru_devtty(uint8_t fru, char *devtty) {
  sprintf(devtty, "/dev/ttyS1");
  return 0;
}

void
pal_dump_key_value(void) {
  int i;
  int ret;

  char value[MAX_VALUE_LEN] = {0x0};

  while (strcmp(key_cfg[i].name, LAST_KEY)) {
    printf("%s:", key_cfg[i].name);
    if (ret = kv_get(key_cfg[i].name, value) < 0) {
      printf("\n");
    } else {
      printf("%s\n",  value);
    }
    i++;
    memset(value, 0, MAX_VALUE_LEN);
  }
}

int
pal_set_last_pwr_state(uint8_t fru, char *state) {

  int ret;
  char key[MAX_KEY_LEN] = {0};

  sprintf(key, "%s", "pwr_server_last_state");

  ret = pal_set_key_value(key, state);
  if (ret < 0) {
#ifdef DEBUG
    syslog(LOG_WARNING, "pal_set_last_pwr_state: pal_set_key_value failed for "
        "fru %u", fru);
#endif
  }

  return ret;
}

int
pal_get_last_pwr_state(uint8_t fru, char *state) {
  int ret;
  char key[MAX_KEY_LEN] = {0};

  sprintf(key, "%s", "pwr_server_last_state");

  ret = pal_get_key_value(key, state);
  if (ret < 0) {
#ifdef DEBUG
    syslog(LOG_WARNING, "pal_get_last_pwr_state: pal_get_key_value failed for "
        "fru %u", fru);
#endif
  }

  return ret;
}

// GUID for System and Device
static int
pal_get_guid(uint16_t offset, char *guid) {
  int fd = 0;
  uint64_t tmp[GUID_SIZE];
  ssize_t bytes_rd;

  errno = 0;

  // Check if file is present
  if (access(FRU_EEPROM, F_OK) == -1) {
      syslog(LOG_ERR, "pal_get_guid: unable to access the %s file: %s",
          FRU_EEPROM, strerror(errno));
      return errno;
  }

  // Open the file
  fd = open(FRU_EEPROM, O_RDONLY);
  if (fd == -1) {
    syslog(LOG_ERR, "pal_get_guid: unable to open the %s file: %s",
        FRU_EEPROM, strerror(errno));
    return errno;
  }

  // seek to the offset
  lseek(fd, offset, SEEK_SET);

  // Read bytes from location
  bytes_rd = read(fd, guid, GUID_SIZE);
  if (bytes_rd != GUID_SIZE) {
    syslog(LOG_ERR, "pal_get_guid: read to %s file failed: %s",
        FRU_EEPROM, strerror(errno));
    goto err_exit;
  }

err_exit:
  close(fd);
  return errno;
}

static int
pal_set_guid(uint16_t offset, char *guid) {
  int fd = 0;
  uint64_t tmp[GUID_SIZE];
  ssize_t bytes_wr;
  int i = 0;

  errno = 0;

  // Check for file presence
  if (access(FRU_EEPROM, F_OK) == -1) {
      syslog(LOG_ERR, "pal_set_guid: unable to access the %s file: %s",
          FRU_EEPROM, strerror(errno));
      return errno;
  }

  // Open file
  fd = open(FRU_EEPROM, O_WRONLY);
  if (fd == -1) {
    syslog(LOG_ERR, "pal_set_guid: unable to open the %s file: %s",
        FRU_EEPROM, strerror(errno));
    return errno;
  }

  // Seek the offset
  lseek(fd, offset, SEEK_SET);

  // Write GUID data
  bytes_wr = write(fd, guid, GUID_SIZE);
  if (bytes_wr != GUID_SIZE) {
    syslog(LOG_ERR, "pal_set_guid: write to %s file failed: %s",
        FRU_EEPROM, strerror(errno));
    goto err_exit;
  }

err_exit:
  close(fd);
  return errno;
}

// GUID based on RFC4122 format @ https://tools.ietf.org/html/rfc4122
static void
pal_populate_guid(uint8_t *guid, uint8_t *str) {
  unsigned int secs;
  unsigned int usecs;
  struct timeval tv;
  uint8_t count;
  uint8_t lsb, msb;
  int i, r;

  // Populate time
  gettimeofday(&tv, NULL);

  secs = tv.tv_sec;
  usecs = tv.tv_usec;
  guid[0] = usecs & 0xFF;
  guid[1] = (usecs >> 8) & 0xFF;
  guid[2] = (usecs >> 16) & 0xFF;
  guid[3] = (usecs >> 24) & 0xFF;
  guid[4] = secs & 0xFF;
  guid[5] = (secs >> 8) & 0xFF;
  guid[6] = (secs >> 16) & 0xFF;
  guid[7] = (secs >> 24) & 0x0F;

  // Populate version
  guid[7] |= 0x10;

  // Populate clock seq with randmom number
  //getrandom(&guid[8], 2, 0);
  srand(time(NULL));
  //memcpy(&guid[8], rand(), 2);
  r = rand();
  guid[8] = r & 0xFF;
  guid[9] = (r>>8) & 0xFF;

  // Use string to populate 6 bytes unique
  // e.g. LSP62100035 => 'S' 'P' 0x62 0x10 0x00 0x35
  count = 0;
  for (i = strlen(str)-1; i >= 0; i--) {
    if (count == 6) {
      break;
    }

    // If alphabet use the character as is
    if (isalpha(str[i])) {
      guid[15-count] = str[i];
      count++;
      continue;
    }

    // If it is 0-9, use two numbers as BCD
    lsb = str[i] - '0';
    if (i > 0) {
      i--;
      if (isalpha(str[i])) {
        i++;
        msb = 0;
      } else {
        msb = str[i] - '0';
      }
    } else {
      msb = 0;
    }
    guid[15-count] = (msb << 4) | lsb;
    count++;
  }

  // zero the remaining bytes, if any
  if (count != 6) {
    memset(&guid[10], 0, 6-count);
  }

}

int
pal_set_sys_guid(uint8_t fru, char *str) {
  pal_populate_guid(g_sys_guid, str);

  return pal_set_guid(OFFSET_SYS_GUID, g_sys_guid);
}

int
pal_set_dev_guid(uint8_t fru, char *str) {
  pal_populate_guid(g_dev_guid, str);

  return pal_set_guid(OFFSET_DEV_GUID, g_dev_guid);
}

int
pal_get_sys_guid(uint8_t fru, char *guid) {
  pal_get_guid(OFFSET_SYS_GUID, g_sys_guid);
  memcpy(guid, g_sys_guid, GUID_SIZE);

  return 0;
}

int
pal_get_dev_guid(uint8_t fru, char *guid) {

  pal_get_guid(OFFSET_DEV_GUID, g_dev_guid);

  memcpy(guid, g_dev_guid, GUID_SIZE);

  return 0;
}

void
pal_get_chassis_status(uint8_t slot, uint8_t *req_data, uint8_t *res_data, uint8_t *res_len) {

   char str_server_por_cfg[64];
   char *buff[MAX_VALUE_LEN];
   int policy = 3;
   uint8_t status, ret;
   unsigned char *data = res_data;

   // Platform Power Policy
   memset(str_server_por_cfg, 0 , sizeof(char) * 64);
   sprintf(str_server_por_cfg, "%s", "server_por_cfg");

   if (pal_get_key_value(str_server_por_cfg, buff) == 0)
   {
     if (!memcmp(buff, "off", strlen("off")))
       policy = 0;
     else if (!memcmp(buff, "lps", strlen("lps")))
       policy = 1;
     else if (!memcmp(buff, "on", strlen("on")))
       policy = 2;
     else
       policy = 3;
   }
   *data++ = 0x01 | (policy << 5);
   *data++ = 0x00;   // Last Power Event
   *data++ = 0x40;   // Misc. Chassis Status
   *data++ = 0x00;   // Front Panel Button Disable
   *res_len = data - res_data;
}

int
pal_set_sysfw_ver(uint8_t fru, uint8_t *ver) {
  int i;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char tstr[10] = {0};

  sprintf(key, "sysfw_ver_server");

  for (i = 0; i < SIZE_SYSFW_VER; i++) {
    sprintf(tstr, "%02x", ver[i]);
    strcat(str, tstr);
  }

  return pal_set_key_value(key, str);
}

int
pal_get_sysfw_ver(uint8_t fru, uint8_t *ver) {
  int i;
  int j = 0;
  int ret;
  int msb, lsb;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char tstr[4] = {0};

  sprintf(key, "sysfw_ver_server");

  ret = pal_get_key_value(key, str);
  if (ret) {
    return ret;
  }

  for (i = 0; i < 2*SIZE_SYSFW_VER; i += 2) {
    sprintf(tstr, "%c\n", str[i]);
    msb = strtol(tstr, NULL, 16);

    sprintf(tstr, "%c\n", str[i+1]);
    lsb = strtol(tstr, NULL, 16);
    ver[j++] = (msb << 4) | lsb;
  }

  return 0;
}

int
pal_set_boot_order(uint8_t slot, uint8_t *boot, uint8_t *res_data, uint8_t *res_len) {
  int i;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char tstr[10] = {0};
  *res_len = 0;
  sprintf(key, "server_boot_order");

  for (i = 0; i < SIZE_BOOT_ORDER; i++) {
    snprintf(tstr, 3, "%02x", boot[i]);
    strncat(str, tstr, 3);
  }

  return pal_set_key_value(key, str);
}

int
pal_get_boot_order(uint8_t slot, uint8_t *req_data, uint8_t *boot, uint8_t *res_len) {
  int i;
  int j = 0;
  int ret;
  int msb, lsb;
  char key[MAX_KEY_LEN] = {0};
  char str[MAX_VALUE_LEN] = {0};
  char tstr[4] = {0};

  sprintf(key, "server_boot_order");

  ret = pal_get_key_value(key, str);
  if (ret) {
    *res_len = 0;
    return ret;
  }

  for (i = 0; i < 2*SIZE_BOOT_ORDER; i += 2) {
    sprintf(tstr, "%c\n", str[i]);
    msb = strtol(tstr, NULL, 16);

    sprintf(tstr, "%c\n", str[i+1]);
    lsb = strtol(tstr, NULL, 16);
    boot[j++] = (msb << 4) | lsb;
  }
  *res_len = SIZE_BOOT_ORDER;
  return 0;
}

int
pal_is_bmc_por(void) {
  uint32_t scu_fd;
  uint32_t rst_sts;
  void *scu_reg;

  scu_fd = open("/dev/mem", O_RDWR | O_SYNC );
  if (scu_fd < 0) {
    return 0;
  }

  scu_reg = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, scu_fd,
             AST_SCU_BASE);

  rst_sts = *(volatile uint32_t*) (scu_reg + RST_STS_OFFSET);

  munmap(scu_reg, PAGE_SIZE);
  close(scu_fd);

  // BIT 1: EXTRST; BIT 2: WDT1
  if (rst_sts & 0x6) {
    return 0;
  } else {
    return 1;
  }
}

int
pal_get_fru_discrete_list(uint8_t fru, uint8_t **sensor_list, int *cnt) {
  switch(fru) {
  case FRU_MB:
    *sensor_list = (uint8_t *) mb_discrete_sensor_list;
    *cnt = mb_discrete_sensor_cnt;
    break;
  default:
    if (fru > MAX_NUM_FRUS)
      return -1;
    // Nothing to read yet.
    *sensor_list = NULL;
    *cnt = 0;
  }
    return 0;
}

static void
_print_sensor_discrete_log(uint8_t fru, uint8_t snr_num, char *snr_name,
    uint8_t val, char *event) {
  if (val) {
    syslog(LOG_CRIT, "ASSERT: %s discrete - raised - FRU: %d, num: 0x%X,"
        " snr: %-16s val: 0x%X", event, fru, snr_num, snr_name, val);
  } else {
    syslog(LOG_CRIT, "DEASSERT: %s discrete - settled - FRU: %d, num: 0x%X,"
        " snr: %-16s val: 0x%X", event, fru, snr_num, snr_name, val);
  }
  pal_update_ts_sled();
}

int
pal_sensor_discrete_check(uint8_t fru, uint8_t snr_num, char *snr_name,
    uint8_t o_val, uint8_t n_val) {

  char name[32];
  bool valid = false;
  uint8_t diff = o_val ^ n_val;
  return 0;
}

static int
pal_store_crashdump(uint8_t fru) {

  return 0;
}

int
pal_sel_handler(uint8_t fru, uint8_t snr_num, uint8_t *event_data) {

  char key[MAX_KEY_LEN] = {0};
  char cvalue[MAX_VALUE_LEN] = {0};

  /* Write the value "0" which means FRU_STATUS_BAD */
  return pal_set_key_value(key, "0");
}

int
pal_get_event_sensor_name(uint8_t fru, uint8_t *sel, char *name) {
  uint8_t snr_type = sel[10];
  uint8_t snr_num = sel[11];

  switch (snr_type) {
    case OS_BOOT:
      // OS_BOOT used by OS
      sprintf(name, "OS");
      return 0;
  }

  switch(snr_num) {
    case SYSTEM_EVENT:
      sprintf(name, "SYSTEM_EVENT");
      break;
    case THERM_THRESH_EVT:
      sprintf(name, "THERM_THRESH_EVT");
      break;
    case CRITICAL_IRQ:
      sprintf(name, "CRITICAL_IRQ");
      break;
    case POST_ERROR:
      sprintf(name, "POST_ERROR");
      break;
    case MACHINE_CHK_ERR:
      sprintf(name, "MACHINE_CHK_ERR");
      break;
    case PCIE_ERR:
      sprintf(name, "PCIE_ERR");
      break;
    case IIO_ERR:
      sprintf(name, "IIO_ERR");
      break;
    case MEMORY_ECC_ERR:
      sprintf(name, "MEMORY_ECC_ERR");
      break;
    case MEMORY_ERR_LOG_DIS:
      sprintf(name, "MEMORY_ERR_LOG_DIS");
      break;
    case PWR_ERR:
      sprintf(name, "PWR_ERR");
      break;
    case CATERR:
      sprintf(name, "CATERR");
      break;
    case CPU_DIMM_HOT:
      sprintf(name, "CPU_DIMM_HOT");
      break;
    case SOFTWARE_NMI:
      sprintf(name, "SOFTWARE_NMI");
      break;
    case CPU0_THERM_STATUS:
      sprintf(name, "CPU0_THERM_STATUS");
      break;
    case CPU1_THERM_STATUS:
      sprintf(name, "CPU1_THERM_STATUS");
      break;
    case ME_POWER_STATE:
      sprintf(name, "ME_POWER_STATE");
      break;
    case SPS_FW_HEALTH:
      sprintf(name, "SPS_FW_HEALTH");
      break;
    case NM_EXCEPTION:
      sprintf(name, "NM_EXCEPTION");
      break;
    case PWR_THRESH_EVT:
      sprintf(name, "PWR_THRESH_EVT");
      break;
    case HPR_WARNING:
      sprintf(name, "HPR_WARNING");
      break;
    default:
      sprintf(name, "Unknown");
      break;
  }

  return 0;
}

int
pal_parse_sel(uint8_t fru, uint8_t *sel, char *error_log) {

  uint8_t snr_type = sel[10];
  uint8_t snr_num = sel[11];
  char *event_data = &sel[10];
  char *ed = &event_data[3];
  char temp_log[128] = {0};
  uint8_t temp;

  switch (snr_type) {
    case OS_BOOT:
      // OS_BOOT used by OS
      sprintf(error_log, "");
      switch (ed[0] & 0xF) {
        case 0x07:
          strcat(error_log, "Base OS/Hypervisor Installation started");
          break;
        case 0x08:
          strcat(error_log, "Base OS/Hypervisor Installation completed");
          break;
        case 0x09:
          strcat(error_log, "Base OS/Hypervisor Installation aborted");
          break;
        case 0x0A:
          strcat(error_log, "Base OS/Hypervisor Installation failed");
          break;
        default:
          strcat(error_log, "Unknown");
          break;
      }
      return 0;
  }

  switch(snr_num) {
    case SYSTEM_EVENT:
      sprintf(error_log, "");
      if (ed[0] == 0xE5) {
        strcat(error_log, "Cause of Time change - ");

        if (ed[2] == 0x00)
          strcat(error_log, "NTP");
        else if (ed[2] == 0x01)
          strcat(error_log, "Host RTL");
        else if (ed[2] == 0x02)
          strcat(error_log, "Set SEL time cmd ");
        else if (ed[2] == 0x03)
          strcat(error_log, "Set SEL time UTC offset cmd");
        else
          strcat(error_log, "Unknown");

        if (ed[1] == 0x00)
          strcat(error_log, " - First Time");
        else if(ed[1] == 0x80)
          strcat(error_log, " - Second Time");

      }
      break;

    case THERM_THRESH_EVT:
      sprintf(error_log, "");
      if (ed[0] == 0x1)
        strcat(error_log, "Limit Exceeded");
      else
        strcat(error_log, "Unknown");
      break;

    case CRITICAL_IRQ:
      sprintf(error_log, "");
      if (ed[0] == 0x0)
        strcat(error_log, "NMI / Diagnostic Interrupt");
      else if (ed[0] == 0x03)
        strcat(error_log, "Software NMI");
      else
        strcat(error_log, "Unknown");
      break;

    case POST_ERROR:
      sprintf(error_log, "");
      if ((ed[0] & 0x0F) == 0x0)
        strcat(error_log, "System Firmware Error");
      else
        strcat(error_log, "Unknown");
      if (((ed[0] >> 6) & 0x03) == 0x3) {
        // TODO: Need to implement IPMI spec based Post Code
        strcat(error_log, ", IPMI Post Code");
      } else if (((ed[0] >> 6) & 0x03) == 0x2) {
        sprintf(temp_log, ", OEM Post Code 0x%02X%02X", ed[2], ed[1]);
        strcat(error_log, temp_log);
        switch ((ed[2] << 8) | ed[1]) {
          case 0xA105:
            sprintf(temp_log, ", BMC Failed (No Response)");
            strcat(error_log, temp_log);
            break;
          case 0xA106:
            sprintf(temp_log, ", BMC Failed (Self Test Fail)");
            strcat(error_log, temp_log);
            break;
          default:
            break;
        }
      }
      break;

    case MACHINE_CHK_ERR:
      sprintf(error_log, "");
      if ((ed[0] & 0x0F) == 0x0B) {
        strcat(error_log, "Uncorrectable");
      } else if ((ed[0] & 0x0F) == 0x0C) {
        strcat(error_log, "Correctable");
      } else {
        strcat(error_log, "Unknown");
      }

      sprintf(temp_log, ", Machine Check bank Number %d ", ed[1]);
      strcat(error_log, temp_log);
      sprintf(temp_log, ", CPU %d, Core %d ", ed[2] >> 5, ed[2] & 0x1F);
      strcat(error_log, temp_log);

      break;

    case PCIE_ERR:
      sprintf(error_log, "");
      if ((ed[0] & 0xF) == 0x4)
        strcat(error_log, "PCI PERR");
      else if ((ed[0] & 0xF) == 0x5)
        strcat(error_log, "PCI SERR");
      else if ((ed[0] & 0xF) == 0x7)
        strcat(error_log, "Correctable");
      else if ((ed[0] & 0xF) == 0x8)
        strcat(error_log, "Uncorrectable");
      else if ((ed[0] & 0xF) == 0xA)
        strcat(error_log, "Bus Fatal");
      else
        strcat(error_log, "Unknown");

      sprintf(temp_log, " (Bus %02X / Dev %02X / Fun %02X)", ed[2], ed[1] >> 3, ed[1] & 0x7);
      strcat(error_log, temp_log);
      break;

    case IIO_ERR:
      sprintf(error_log, "");
      if ((ed[0] & 0xF) == 0) {

        sprintf(temp_log, "CPU %d, Error ID 0x%X", (ed[2] & 0xE0) >> 5,
            ed[1]);
        strcat(error_log, temp_log);

        temp = ed[2] & 0x7;
        if (temp == 0x0)
          strcat(error_log, " - IRP0");
        else if (temp == 0x1)
          strcat(error_log, " - IRP1");
        else if (temp == 0x2)
          strcat(error_log, " - IIO-Core");
        else if (temp == 0x3)
          strcat(error_log, " - VT-d");
        else if (temp == 0x4)
          strcat(error_log, " - Intel Quick Data");
        else if (temp == 0x5)
          strcat(error_log, " - Misc");
        else
          strcat(error_log, " - Reserved");
      } else
        strcat(error_log, "Unknown");
      break;

    case MEMORY_ECC_ERR:
    case MEMORY_ERR_LOG_DIS:
      sprintf(error_log, "");
      if (snr_num == MEMORY_ERR_LOG_DIS) {
        if ((ed[0] & 0x0F) == 0x0)
          strcat(error_log, "Correctable Memory Error Logging Disabled");
        else
          strcat(error_log, "Unknown");
      } else {
        if ((ed[0] & 0x0F) == 0x0){
          strcat(error_log, "Correctable");
          sprintf(temp_log, "DIMM%02X ECC err", ed[2]);
          pal_add_cri_sel(temp_log);
        }else if ((ed[0] & 0x0F) == 0x1){
          strcat(error_log, "Uncorrectable");
          sprintf(temp_log, "DIMM%02X UECC err", ed[2]);
          pal_add_cri_sel(temp_log);
        }else if ((ed[0] & 0x0F) == 0x2){
          strcat(error_log, "Parity");
        }else if ((ed[0] & 0x0F) == 0x5){
          strcat(error_log, "Correctable ECC error Logging Limit Reached");
        }else{
          strcat(error_log, "Unknown");
        }
      }

      sprintf(temp_log, " (DIMM %02X)", ed[2]);
      strcat(error_log, temp_log);

      sprintf(temp_log, " Logical Rank %d", ed[1] & 0x03);
      strcat(error_log, temp_log);

      switch(ed[1] & 0x0C) {
        case 0x00:
          //Ignore when " All info available"
          break;
        case 0x01:
          strcat(error_log, " DIMM info not valid");
          break;
        case 0x02:
          strcat(error_log, " CHN info not valid");
          break;
        case 0x03:
          strcat(error_log, " CPU info not valid");
          break;
        default:
          strcat(error_log, " Unknown");
      }

      break;

    case PWR_ERR:
      sprintf(error_log, "");
      if (ed[0] == 0x2)
        strcat(error_log, "PCH_PWROK failure");
      else
        strcat(error_log, "Unknown");
      break;

    case CATERR:
      sprintf(error_log, "");
      if (ed[0] == 0x0)
        strcat(error_log, "IERR/CATERR");
      else if (ed[0] == 0xB)
        strcat(error_log, "MCERR/CATERR");
      else
        strcat(error_log, "Unknown");
      break;

    case MSMI:
      sprintf(error_log, "");
      if (ed[0] == 0x0)
        strcat(error_log, "IERR/MSMI");
      else if (ed[0] == 0xB)
        strcat(error_log, "MCERR/MSMI");
      else
        strcat(error_log, "Unknown");
      break;

    case CPU_DIMM_HOT:
      sprintf(error_log, "");
      if ((ed[0] << 16 | ed[1] << 8 | ed[2]) == 0x01FFFF)
        strcat(error_log, "SOC MEMHOT");
      else
        strcat(error_log, "Unknown");
      break;

    case SOFTWARE_NMI:
      sprintf(error_log, "");
      if ((ed[0] << 16 | ed[1] << 8 | ed[2]) == 0x03FFFF)
        strcat(error_log, "Software NMI");
      else
        strcat(error_log, "Unknown");
      break;

    case ME_POWER_STATE:
      sprintf(error_log, "");
      switch (ed[0]) {
        case 0:
          sprintf(error_log, "RUNNING");
          break;
        case 2:
          sprintf(error_log, "POWER_OFF");
          break;
        default:
          sprintf(error_log, "Unknown[%d]", ed[0]);
          break;
      }
      break;
    case SPS_FW_HEALTH:
      sprintf(error_log, "");
      if (event_data[0] == 0xDC && ed[1] == 0x06) {
        strcat(error_log, "FW UPDATE");
        return 1;
      } else if (ed[1] == 0x01) {
        strcat(error_log, "Image execution failed");
        return 1;
      } else if (ed[1] == 0x02) {
        strcat(error_log, "Flash erase error");
        return 1;
      } else if (ed[1] == 0x03) {
        strcat(error_log, "Flash state information");
        return 1;
      } else if (ed[1] == 0x04) {
        strcat(error_log, "Internal error");
        return 1;
      } else if (ed[1] == 0x05) {
        strcat(error_log, "BMC did not respond");
        return 1;
      } else if (ed[1] == 0x07) {
        strcat(error_log, "Manufacturing error");
        return 1;
      } else if (ed[1] == 0x08) {
        strcat(error_log, "Automatic Restore to Factory Presets");
        return 1;
      } else if (ed[1] == 0x09) {
        strcat(error_log, "Firmware Exception");
        return 1;
      } else if (ed[1] == 0x0A) {
        strcat(error_log, "Flash Wear-Out Protection Warning");
        return 1;
      } else if (ed[1] == 0x0D) {
        strcat(error_log, "DMI interface error");
        return 1;
      } else if (ed[1] == 0x0E) {
        strcat(error_log, "MCTP interface error");
        return 1;
      } else if (ed[1] == 0x0F) {
        strcat(error_log, "Auto-configuration finished");
        return 1;
      } else if (ed[1] == 0x10) {
        strcat(error_log, "Unsupported Segment Defined Feature");
        return 1;
      } else if (ed[1] == 0x12) {
        strcat(error_log, "CPU Debug Capability Disabled");
        return 1;
      } else if (ed[1] == 0x13) {
        strcat(error_log, "UMA operation error");
        return 1;
      } else
         strcat(error_log, "Unknown");
      break;

    case NM_EXCEPTION:
      sprintf(error_log, "");
      if (ed[0] == 0xA8) {
        strcat(error_log, "Policy Correction Time Exceeded");
        return 1;
      } else
         strcat(error_log, "Unknown");
      break;

    case CPU0_THERM_STATUS:
    case CPU1_THERM_STATUS:
      sprintf(error_log, "");
      if (ed[0] == 0x00)
        strcat(error_log, "CPU Critical Temperature");
      else if (ed[0] == 0x01)
        strcat(error_log, "PROCHOT#");
      else if (ed[0] == 0x02)
        strcat(error_log, "TCC Activation");
      else
        strcat(error_log, "Unknown");
      break;

    case PWR_THRESH_EVT:
      sprintf(error_log, "");
      if (ed[0]  == 0x00)
        strcat(error_log, "Limit Not Exceeded");
      else if (ed[0]  == 0x01)
        strcat(error_log, "Limit Exceeded");
      else
        strcat(error_log, "Unknown");
      break;

    case HPR_WARNING:
      sprintf(error_log, "");
      if (ed[2]  == 0x01) {
        if (ed[1]  = 0xFF)
          strcat(temp_log, "Infinite Time");
        else
          sprintf(temp_log, "%d minutes",ed[1]);
        strcat(error_log, temp_log);
      } else {
        strcat(error_log, "Unknown");
      }
      break;

    default:
      sprintf(error_log, "Unknown");
      break;
  }
  if (((event_data[2] & 0x80) >> 7) == 0) {
    sprintf(temp_log, " Assertion");
    strcat(error_log, temp_log);
  } else {
    sprintf(temp_log, " Deassertion");
    strcat(error_log, temp_log);
  }
  return 0;
}

// Helper function for msleep
void
msleep(int msec) {
  struct timespec req;

  req.tv_sec = 0;
  req.tv_nsec = msec * 1000 * 1000;

  while(nanosleep(&req, &req) == -1 && errno == EINTR) {
    continue;
  }
}

static int
pal_get_sensor_health_key(uint8_t fru, char *key)
{
  switch (fru) {
    case FRU_MB:
      sprintf(key, "server_sensor_health");
      break;
    case FRU_NIC:
      sprintf(key, "nic_sensor_health");
      break;
    default:
      return -1;
  }
  return 0;
}

int
pal_set_sensor_health(uint8_t fru, uint8_t value) {

  char key[MAX_KEY_LEN] = {0};
  char cvalue[MAX_VALUE_LEN] = {0};

  if (pal_get_sensor_health_key(fru, key))
    return -1;

  sprintf(cvalue, (value > 0) ? "1": "0");

  return pal_set_key_value(key, cvalue);
}

int
pal_get_fru_health(uint8_t fru, uint8_t *value) {

  char cvalue[MAX_VALUE_LEN] = {0};
  char key[MAX_KEY_LEN] = {0};
  int ret;

  if (pal_get_sensor_health_key(fru, key)) {
    return ERR_NOT_READY;
  }

  ret = pal_get_key_value(key, cvalue);
  if (ret) {
    syslog(LOG_INFO, "pal_get_fru_health(%d): getting value for %s failed\n", fru, key);
    return ret;
  }

  *value = atoi(cvalue);

  if (fru != FRU_MB)
    return 0;

  // If MB, get SEL error status.
  sprintf(key, "server_sel_error");
  memset(cvalue, 0, MAX_VALUE_LEN);

  ret = pal_get_key_value(key, cvalue);
  if (ret) {
    syslog(LOG_INFO, "pal_get_fru_health(%d): getting value for %s failed\n", fru, key);
    return ret;
  }

  *value = *value & atoi(cvalue);
  return 0;
}

void
pal_inform_bic_mode(uint8_t fru, uint8_t mode) {
}

int
pal_get_fan_name(uint8_t num, char *name) {

  switch(num) {

    case FAN_0:
      sprintf(name, "Fan 0");
      break;

    case FAN_1:
      sprintf(name, "Fan 1");
      break;

    default:
      return -1;
  }

  return 0;
}


int
pal_set_fan_speed(uint8_t fan, uint8_t pwm) {
  int unit;
  int ret;

  if (fan >= pal_pwm_cnt) {
    syslog(LOG_INFO, "pal_set_fan_speed: fan number is invalid - %d", fan);
    return -1;
  }

  // Do not allow setting fan when server is off.
  if (is_server_off()) {
    return PAL_ENOTREADY;
  }

  // Convert the percentage to our 1/96th unit.
  unit = pwm * PWM_UNIT_MAX / 100;

  // For 0%, turn off the PWM entirely
  if (unit == 0) {
    write_fan_value(fan, "pwm%d_en", 0);
    if (ret < 0) {
      syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
      return -1;
    }
    return 0;

  // For 100%, set falling and rising to the same value
  } else if (unit == PWM_UNIT_MAX) {
    unit = 0;
  }

  ret = write_fan_value(fan, "pwm%d_type", 0);
  if (ret < 0) {
    syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
    return -1;
  }

  ret = write_fan_value(fan, "pwm%d_rising", 0);
  if (ret < 0) {
    syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
    return -1;
  }

  ret = write_fan_value(fan, "pwm%d_falling", unit);
  if (ret < 0) {
    syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
    return -1;
  }

  ret = write_fan_value(fan, "pwm%d_en", 1);
  if (ret < 0) {
    syslog(LOG_INFO, "set_fan_speed: write_fan_value failed");
    return -1;
  }

  return 0;
}

int
pal_get_fan_speed(uint8_t fan, int *rpm) {
  if (fan == 0) {
    return read_fan_value(FAN0_TACH_INPUT, "tacho%d_rpm", rpm);
  } else if (fan == 1) {
    return read_fan_value(FAN1_TACH_INPUT, "tacho%d_rpm", rpm);
  } else {
    syslog(LOG_INFO, "get_fan_speed: invalid fan#:%d", fan);
    return -1;
  }
}

void
pal_update_ts_sled()
{
  char key[MAX_KEY_LEN] = {0};
  char tstr[MAX_VALUE_LEN] = {0};
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);
  sprintf(tstr, "%d", ts.tv_sec);

  sprintf(key, "timestamp_sled");

  pal_set_key_value(key, tstr);
}

int
pal_handle_dcmi(uint8_t fru, uint8_t *request, uint8_t req_len, uint8_t *response, uint8_t *rlen) {
  return me_xmit(request, req_len, response, rlen);
}

int
pal_get_board_id(uint8_t slot, uint8_t *req_data, uint8_t req_len, uint8_t *res_data, uint8_t *res_len)
{
  int ret;
  uint8_t platform_id  = 0x00;
  uint8_t board_rev_id = 0x00;
  uint8_t mb_slot_id = 0x00;
  uint8_t raiser_card_slot_id = 0x00;
  int completion_code=CC_UNSPECIFIED_ERROR;

  ret = pal_get_platform_id(&platform_id);
  if (ret) {
    *res_len = 0x00;
    return completion_code;
  }

  ret = pal_get_board_rev_id(&board_rev_id);
  if (ret) {
    *res_len = 0x00;
    return completion_code;
  }

  ret = pal_get_mb_slot_id(&mb_slot_id);
  if (ret) {
    *res_len = 0x00;
    return completion_code;
  }

  ret = pal_get_slot_cfg_id(&raiser_card_slot_id);
  if (ret) {
    *res_len = 0x00;
    return completion_code;
  }

  // Prepare response buffer
  completion_code = CC_SUCCESS;
  res_data[0] = platform_id;
  res_data[1] = board_rev_id;
  res_data[2] = mb_slot_id;
  res_data[3] = raiser_card_slot_id;
  *res_len = 0x04;

  return completion_code;
}

int
pal_get_platform_id(uint8_t *id) {
  int val;
  char path[64] = {0};

  sprintf(path, GPIO_VAL, GPIO_BOARD_SKU_ID0);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = val&0x01;

  sprintf(path, GPIO_VAL, GPIO_BOARD_SKU_ID1);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<1);

  sprintf(path, GPIO_VAL, GPIO_BOARD_SKU_ID2);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<2);

  sprintf(path, GPIO_VAL, GPIO_BOARD_SKU_ID3);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<3);

  sprintf(path, GPIO_VAL, GPIO_BOARD_SKU_ID4);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<4);

  return 0;
}

int
pal_get_board_rev_id(uint8_t *id) {
  int val;
  char path[64] = {0};

  sprintf(path, GPIO_VAL, GPIO_BOARD_REV_ID0);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = val&0x01;

  sprintf(path, GPIO_VAL, GPIO_BOARD_REV_ID1);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<1);

  sprintf(path, GPIO_VAL, GPIO_BOARD_REV_ID2);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<2);

  return 0;
}

int
pal_get_mb_slot_id(uint8_t *id) {
  int val;
  char path[64] = {0};

  sprintf(path, GPIO_VAL, GPIO_MB_SLOT_ID0);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = val&0x01;

  sprintf(path, GPIO_VAL, GPIO_MB_SLOT_ID1);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<1);

  sprintf(path, GPIO_VAL, GPIO_MB_SLOT_ID2);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<2);

  return 0;
}


int
pal_get_slot_cfg_id(uint8_t *id) {
  int val;
  char path[64] = {0};

  sprintf(path, GPIO_VAL, GPIO_SLT_CFG0);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = val&0x01;

  sprintf(path, GPIO_VAL, GPIO_SLT_CFG1);
  if (read_device(path, &val)) {
    return -1;
  }
  *id = *id | (val<<1);

   return 0;
}

void
pal_log_clear(char *fru) {
  if (!strcmp(fru, "mb")) {
    pal_set_key_value("server_sensor_health", "1");
    pal_set_key_value("server_sel_error", "1");
  } else if (!strcmp(fru, "nic")) {
    pal_set_key_value("nic_sensor_health", "1");
  } else if (!strcmp(fru, "all")) {
    pal_set_key_value("server_sensor_health", "1");
    pal_set_key_value("server_sel_error", "1");
    pal_set_key_value("nic_sensor_health", "1");
  }
}

//For OEM command "CMD_OEM_GET_PLAT_INFO" 0x7e
int
pal_get_plat_sku_id(void) {

  return 0;
}

int
pal_get_poss_pcie_config(uint8_t slot, uint8_t *req_data, uint8_t req_len, uint8_t *res_data, uint8_t *res_len) {

  return 0;
}

int
pal_get_pwm_value(uint8_t fan_num, uint8_t *value) {
  char path[LARGEST_DEVICE_NAME] = {0};
  char device_name[LARGEST_DEVICE_NAME] = {0};
  int val = 0;
  int pwm_enable = 0;

  if (fan_num < 0 || fan_num >= pal_pwm_cnt) {
    syslog(LOG_INFO, "pal_get_pwm_value: fan number is invalid - %d", fan_num);
    return -1;
  }

  // Need check pwmX_en to determine the PWM is 0 or 100.
  snprintf(device_name, LARGEST_DEVICE_NAME, "pwm%d_en", fan_num);
  snprintf(path, LARGEST_DEVICE_NAME, "%s/%s", PWM_DIR, device_name);
  if (read_device(path, &pwm_enable)) {
    syslog(LOG_INFO, "pal_get_pwm_value: read %s failed", path);
    return -1;
  }

  if(pwm_enable) {
    snprintf(device_name, LARGEST_DEVICE_NAME, "pwm%d_falling", fan_num);
    snprintf(path, LARGEST_DEVICE_NAME, "%s/%s", PWM_DIR, device_name);
    if (read_device_hex(path, &val)) {
      syslog(LOG_INFO, "pal_get_pwm_value: read %s failed", path);
      return -1;
    }

    if(val == 0)
      *value = 100;
    else
      *value = (100 * val + (PWM_UNIT_MAX-1)) / PWM_UNIT_MAX;
  } else {
    *value = 0;
  }

  return 0;
}

int
pal_fan_dead_handle(int fan_num) {

  // TODO: Add action in case of fan dead
  return 0;
}

int
pal_fan_recovered_handle(int fan_num) {

  // TODO: Add action in case of fan recovered
  return 0;
}

static bool
is_cpu0_socket_occupy(void) {
  char path[64] = {0};
  int val;

  sprintf(path, GPIO_VAL, GPIO_FM_CPU0_SKTOCC_LVT3_N);
  if (read_device(path, &val)) {
    return false;
  }

  if (val) {
    return false;
  } else {
    return true;
  }

}

static bool
is_cpu1_socket_occupy(void) {
  char path[64] = {0};
  int val;

  sprintf(path, GPIO_VAL, GPIO_FM_CPU1_SKTOCC_LVT3_N);
  if (read_device(path, &val)) {
    return false;
  }

  if (val) {
    return false;
  } else {
    return true;
  }

}

void
pal_sensor_assert_handle(uint8_t snr_num, float val, uint8_t thresh) {
  char cmd[128];
  char thresh_name[10];

  switch (thresh) {
    case UNR_THRESH:
        sprintf(thresh_name, "UNR");
      break;
    case UCR_THRESH:
        sprintf(thresh_name, "UCR");
      break;
    case UNC_THRESH:
        sprintf(thresh_name, "UNCR");
      break;
    case LNR_THRESH:
        sprintf(thresh_name, "LNR");
      break;
    case LCR_THRESH:
        sprintf(thresh_name, "LCR");
      break;
    case LNC_THRESH:
        sprintf(thresh_name, "LNCR");
      break;
    default:
      syslog(LOG_WARNING, "pal_sensor_assert_handle: wrong thresh enum value");
      exit(-1);
  }

  switch(snr_num) {
    case MB_SENSOR_FAN0_TACH:
      sprintf(cmd, "Fan0 %s %.0fRPM - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_FAN1_TACH:
      sprintf(cmd, "Fan1 %s %.0fRPM - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_CPU0_TEMP:
      sprintf(cmd, "P0 Temp %s %.0f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_CPU1_TEMP:
      sprintf(cmd, "P1 Temp %s %.0f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P3V_BAT:
      sprintf(cmd, "P3V_BAT %s %.2f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P3V3:
      sprintf(cmd, "P3V3 %s %.2f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P5V:
      sprintf(cmd, "P5V %s %.2f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P12V:
      sprintf(cmd, "P12V %s %.2f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P1V05:
      sprintf(cmd, "P1V05 %s %.2f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_PVNN_PCH_STBY:
      sprintf(cmd, "PVNN_PCH_STBY %s %.2f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P3V3_STBY:
      sprintf(cmd, "P3V3_STBY %s %.2f - ASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P5V_STBY:
      sprintf(cmd, "P5V_STBY %s %.2f - ASSERT", thresh_name, val);
      break;
    default:
      return;
  }
  pal_add_cri_sel(cmd);

}

void
pal_sensor_deassert_handle(uint8_t snr_num, float val, uint8_t thresh) {
  char cmd[128];
  char thresh_name[10];

  switch (thresh) {
    case UNR_THRESH:
        sprintf(thresh_name, "UNR");
      break;
    case UCR_THRESH:
        sprintf(thresh_name, "UCR");
      break;
    case UNC_THRESH:
        sprintf(thresh_name, "UNCR");
      break;
    case LNR_THRESH:
        sprintf(thresh_name, "LNR");
      break;
    case LCR_THRESH:
        sprintf(thresh_name, "LCR");
      break;
    case LNC_THRESH:
        sprintf(thresh_name, "LNCR");
      break;
    default:
      syslog(LOG_WARNING, "pal_sensor_assert_handle: wrong thresh enum value");
      exit(-1);
  }

  switch(snr_num) {
    case MB_SENSOR_FAN0_TACH:
      sprintf(cmd, "Fan0 %s %3.0fRPM - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_FAN1_TACH:
      sprintf(cmd, "Fan1 %s %3.0fRPM - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_CPU0_TEMP:
      sprintf(cmd, "P0 Temp %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_CPU1_TEMP:
      sprintf(cmd, "P1 Temp %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P3V_BAT:
      sprintf(cmd, "P3V_BAT %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P3V3:
      sprintf(cmd, "P3V3 %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P5V:
      sprintf(cmd, "P5V %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P12V:
      sprintf(cmd, "P12V %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P1V05:
      sprintf(cmd, "P1V05 %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_PVNN_PCH_STBY:
      sprintf(cmd, "PVNN_PCH_STBY %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P3V3_STBY:
      sprintf(cmd, "P3V3_STBY %s %3.0f - DEASSERT", thresh_name, val);
      break;
    case MB_SENSOR_P5V_STBY:
      sprintf(cmd, "P5V_STBY %s %3.0f - DEASSERT", thresh_name, val);
      break;

    default:
      return;
  }
  pal_add_cri_sel(cmd);

}

void
pal_post_end_chk(uint8_t *post_end_chk) {
  static uint8_t post_end = 1;

  if (*post_end_chk == 1) {
    post_end = 1;
  } else if (*post_end_chk == 0) {
    *post_end_chk = post_end;
    post_end = 0;
  }
}

void
pal_set_post_end(uint8_t slot, uint8_t *req_data, uint8_t *res_data, uint8_t *res_len)
{
  uint8_t post_end = 1;

  *res_len = 0;

  //Set post end chk flag to update LCD info page
  pal_post_end_chk(&post_end);

  // Sync time with system
  system("/usr/local/bin/sync_date.sh &");
}

int
pal_get_fw_info(unsigned char target, unsigned char* res, unsigned char* res_len)
{
  return -1;
}

void
pal_add_cri_sel(char *str)
{
  char cmd[128];
  snprintf(cmd, 128, "logger -p local0.err \"%s\"",str);
  system(cmd);
}

int
pal_is_fw_update_ongoing(uint8_t fru) {
  char key[MAX_KEY_LEN];
  char value[MAX_VALUE_LEN] = {0};
  int ret;
  struct timespec ts;

  if (fru != FRU_MB)
    return 0;

  ret = system("pidof flashcp &> /dev/null");
  if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0)
    return 1;

  strcpy(key, "mb_fwupd");
  ret = edb_cache_get(key, value);
  if (ret < 0) {
     return 0;
  }

  clock_gettime(CLOCK_MONOTONIC, &ts);
  if (strtoul(value, NULL, 10) > ts.tv_sec)
     return 1;

  return 0;
}

void
set_fw_update_ongoing(uint8_t fru, uint16_t tmout) {
  char key[64];
  char value[64];
  struct timespec ts;

  if (fru != FRU_MB)
    return;

  strcpy(key, "mb_fwupd");
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ts.tv_sec += tmout;
  sprintf(value, "%d", ts.tv_sec);

  if (edb_cache_set(key, value) < 0) {
     return -1;
  }
}

int
pal_get_last_boot_time(uint8_t slot, uint8_t *last_boot_time) {
  return 0;
}

uint8_t
pal_set_power_restore_policy(uint8_t slot, uint8_t *pwr_policy, uint8_t *res_data) {

  uint8_t completion_code;
  completion_code = CC_SUCCESS;  // Fill response with default values
  unsigned char policy = *pwr_policy & 0x07;  // Power restore policy

  switch (policy)
  {
      case 0:
        if (pal_set_key_value("server_por_cfg", "off") != 0)
          completion_code = CC_UNSPECIFIED_ERROR;
        break;
      case 1:
        if (pal_set_key_value("server_por_cfg", "lps") != 0)
          completion_code = CC_UNSPECIFIED_ERROR;
        break;
      case 2:
        if (pal_set_key_value("server_por_cfg", "on") != 0)
          completion_code = CC_UNSPECIFIED_ERROR;
        break;
      case 3:
        // no change (just get present policy support)
        break;
      default:
        completion_code = CC_PARAM_OUT_OF_RANGE;
        break;
  }
  return completion_code;
}

uint8_t
pal_get_status(void) {
  char str_server_por_cfg[64];
  char *buff[MAX_VALUE_LEN];
  int policy = 3;
  uint8_t status, data, ret;

  // Platform Power Policy
  memset(str_server_por_cfg, 0 , sizeof(char) * 64);
  sprintf(str_server_por_cfg, "%s", "server_por_cfg");

  if (pal_get_key_value(str_server_por_cfg, buff) == 0)
  {
    if (!memcmp(buff, "off", strlen("off")))
      policy = 0;
    else if (!memcmp(buff, "lps", strlen("lps")))
      policy = 1;
    else if (!memcmp(buff, "on", strlen("on")))
      policy = 2;
    else
      policy = 3;
  }

  data = 0x01 | (policy << 5);

  return data;
}

unsigned char option_offset[] = {0,1,2,3,4,6,11,20,37,164};
unsigned char option_size[]   = {1,1,1,1,2,5,9,17,127};

void
pal_set_boot_option(unsigned char para,unsigned char* pbuff)
{
  return;
}

int
pal_get_boot_option(unsigned char para,unsigned char* pbuff)
{
  unsigned char size = option_size[para];
  memset(pbuff, 0, size);
  return size;
}

int
pal_parse_oem_sel(uint8_t fru, uint8_t *sel, char *error_log)
{
  char str[128];
  uint8_t record_type = (uint8_t) sel[2];
  uint32_t mfg_id;
  error_log[0] = '\0';

  /* Manufacturer ID (byte 9:7) */
  mfg_id = (*(uint32_t*)&sel[7]) & 0xFFFFFF;

  if (record_type == 0xc0 && mfg_id == 0x1c4c) {
    snprintf(str, sizeof(str), "Slot %d PCIe err", sel[14]);
    pal_add_cri_sel(str);
  }

  return 0;
}

int
pal_set_machine_configuration(uint8_t slot, uint8_t *req_data, uint8_t req_len, uint8_t *res_data, uint8_t *res_len)
{
  machine_config_info *mach_config_info = &req_data[3];

  return 0;
}

void
pal_sensor_sts_check(uint8_t snr_num, float val, uint8_t *thresh) {
  int ret;
  int fru = 1;
  float ucr_thresh_val,lcr_thresh_val;

  ret = pal_get_sensor_threshold(fru, snr_num, UCR_THRESH, &ucr_thresh_val);
  if (ret) {
    syslog(LOG_WARNING, "get ucr fail:%f",lcr_thresh_val);
  }

  ret = pal_get_sensor_threshold(fru, snr_num, LCR_THRESH, &lcr_thresh_val);
  if (ret) {
    syslog(LOG_WARNING, "get lcr fail:%f",lcr_thresh_val);
  }

  if(val >= ucr_thresh_val)
    *thresh = UCR_THRESH;
  else if(val <= lcr_thresh_val)
    *thresh = LCR_THRESH;
  else
    *thresh = 0;
}

int
pal_get_80port_record(uint8_t slot, uint8_t *req_data, uint8_t req_len, uint8_t *res_data, uint8_t *res_len)
{
  FILE *fp=NULL;
  int i;
  unsigned char postcode;

  if (res_data == NULL)
    return -1;

  if (slot != FRU_MB) {
    syslog(LOG_WARNING, "pal_get_80port_record: slot %d is not supported", slot);
    return PAL_ENOTSUP;
  }

  fp = fopen(POST_CODE_FILE, "r");
  if (fp == NULL) {
    syslog(LOG_WARNING, "pal_get_80port_record: Cannot open %s", POST_CODE_FILE);
    return PAL_ENOTSUP;
  }

  for (i=0; i<256; i++) {
    // %hhx: unsigned char*
    if (fscanf(fp, "%hhx", &postcode) == 1) {
      res_data[i] = postcode;
    } else {
      break;
    }
  }
  if (res_len)
    *(unsigned short *)res_len = i;

  fclose(fp);

  return PAL_EOK;
}
