/*
 * sensord
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
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
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/file.h>
#include <openbmc/pal.h>
#include <openbmc/gpio.h>
#include <openbmc/ocp-dbg-lcd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define POWER_ON_STR        "on"
#define POWER_OFF_STR       "off"

#define TOUCH(path) \
{\
  int fd = creat(path, 0644);\
  if (fd) close(fd);\
}

#define POLL_TIMEOUT -1 /* Forever */
static uint8_t CATERR_irq = 0;
static uint8_t MSMI_irq = 0;
static long int reset_sec = 0, power_on_sec = 0;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline long int reset_timer(long int *val) {
  pthread_mutex_lock(&timer_mutex);
  *val = 0;
  pthread_mutex_unlock(&timer_mutex);
  return *val;
}
static inline long int increase_timer(long int *val) {
  pthread_mutex_lock(&timer_mutex);
  (*val)++;
  pthread_mutex_unlock(&timer_mutex);
  return *val;
}
static inline long int decrease_timer(long int *val) {
  pthread_mutex_lock(&timer_mutex);
  (*val)--;
  pthread_mutex_unlock(&timer_mutex);
  return *val;
}

struct delayed_log {
  useconds_t usec;
  char msg[256];
};
// Thread for delay event
static void *
delay_log(void *arg)
{
  struct delayed_log* log = (struct delayed_log*)arg;

  pthread_detach(pthread_self());

  if (arg) {
    usleep(log->usec);
    syslog(LOG_CRIT, log->msg);

    free(arg);
  }

  pthread_exit(NULL);
}

static void log_gpio_change(gpio_poll_st *gp, useconds_t log_delay)
{
  if (log_delay == 0) {
    syslog(LOG_CRIT, "%s: %s - %s\n", gp->value ? "DEASSERT": "ASSERT", gp->name, gp->desc);
  } else {
    pthread_t tid_delay_log;
    struct delayed_log *log = (struct delayed_log *)malloc(sizeof(struct delayed_log));
    if (log) {
      log->usec = log_delay;
      snprintf(log->msg, 256, "%s: %s - %s\n", gp->value ? "DEASSERT" : "ASSERT", gp->name, gp->desc);
      if (pthread_create(&tid_delay_log, NULL, delay_log, (void *)log)) {
        free(log);
        log = NULL;
      }
    }
    if (!log) {
      syslog(LOG_CRIT, "%s: %s - %s\n", gp->value ? "DEASSERT": "ASSERT", gp->name, gp->desc);
    }
  }
}

// Event Handler for GPIOR5 platform reset changes
static void platform_reset_event_handle(void *p)
{
  gpio_poll_st *gp = (gpio_poll_st*) p;

  if (gp->gs.gs_gpio == gpio_num("GPIOR5"))
  {
    // Use GPIOR5 to filter some gpio logging
    reset_timer(&reset_sec);
    TOUCH("/tmp/rst_touch");
  }

  log_gpio_change(gp, 0);

}

// Generic Event Handler for GPIO changes
static void gpio_event_handle(void *p)
{
  gpio_poll_st *gp = (gpio_poll_st*) p;
  char cmd[128] = {0};

  if (gp->gs.gs_gpio == gpio_num("GPIOB6")) { // Power OK
    reset_timer(&power_on_sec);
  }
  else if (gp->gs.gs_gpio == gpio_num("GPIOL0")) { // IRQ_UV_DETECT_N
    log_gpio_change(gp, 20*1000);
    return;
  }

  log_gpio_change(gp, 0);

  if (gp->gs.gs_gpio == gpio_num("GPIOQ6")) { // FM_POST_CARD_PRES_BMC_N
    if(gp->value == 1)
      usb_dbg_reset();
  } else if (gp->gs.gs_gpio == gpio_num("GPIOB6")) { // Power OK
    if(gp->value == 0)
      usb_dbg_reset();
  }

  //LCD debug card thermal trip critical SEL support
  if (gp->gs.gs_gpio == gpio_num("GPIOM4") || gp->gs.gs_gpio == gpio_num("GPIOM5")) {
    if (gp->gs.gs_gpio == gpio_num("GPIOM4"))
      strcat(cmd, "CPU0 ");
    else
      strcat(cmd, "CPU1 ");
    strcat(cmd, "thermtrip ");
    if (gp->value) {
      strcat(cmd, "DEASSERT");
    } else {
      strcat(cmd, "ASSERT");
    }
    pal_add_cri_sel(cmd);
  }
}

// Generic Event Handler for GPIO changes, but only logs event when MB is ON
static void gpio_event_handle_power(void *p)
{
  uint8_t status = 0;
  gpio_poll_st *gp = (gpio_poll_st*) p;
  char cmd[128] = {0};

  pal_get_server_power(1, &status);
  if (status != SERVER_POWER_ON) {
    return;
  }

  //Additional filter condition
  if (gp->gs.gs_gpio == gpio_num("GPIOS0") ){ //FM_THROTTLE_N filter for ME limitation
    if (power_on_sec < 5)
      return;
  }
  if (gp->gs.gs_gpio == gpio_num("GPIOD6") || //FM_CPU_ERR0_LVT3_BMC_N
      gp->gs.gs_gpio == gpio_num("GPIOD7") || //FM_CPU_ERR1_LVT3_BMC_N
      gp->gs.gs_gpio == gpio_num("GPIOG0") || //FM_CPU_ERR2_LVT3_N
      gp->gs.gs_gpio == gpio_num("GPION3") || //FM_CPU_MSMI_LVT3_N
      gp->gs.gs_gpio == gpio_num("GPIOG1") || //FM_CPU_CATERR_LVT3_N
      gp->gs.gs_gpio == gpio_num("GPIOE6") || //FM_CPU0_PROCHOT_LVT3_BMC_N
      gp->gs.gs_gpio == gpio_num("GPIOE7") ){ //FM_CPU1_PROCHOT_LVT3_BMC_N
    if (power_on_sec < 3)
      return;
  }

  //Special handle for some GPIO pin
  if (gp->gs.gs_gpio == gpio_num("GPIOG1") ){ //FM_CPU_CATERR_LVT3_N
    CATERR_irq++;
    return;
  }
  if (gp->gs.gs_gpio == gpio_num("GPION3") ){ //FM_CPU_MSMI_LVT3_N
    MSMI_irq++;
    return;
  }

  log_gpio_change(gp, 0);

  //LCD debug card critical SEL support
  if (gp->gs.gs_gpio == gpio_num("GPIOE6") || gp->gs.gs_gpio == gpio_num("GPIOE7")) {
    if (gp->gs.gs_gpio == gpio_num("GPIOE6"))
      strcat(cmd, "CPU0 FPH");
    else
      strcat(cmd, "CPU1 FPH");
    if (gp->value) {
      strcat(cmd, " DEASSERT");
    } else {
      strcat(cmd, " by");
      if (gpio_get(gpio_num("GPIOL0")) == GPIO_VALUE_LOW)
        strcat(cmd, " UV");
      if (gpio_get(gpio_num("GPIOL1")) == GPIO_VALUE_LOW)
        strcat(cmd, " OC");
      if (gpio_get(gpio_num("GPIOL2")) == GPIO_VALUE_LOW)
        strcat(cmd, " timer exp");
      if (gpio_get(gpio_num("GPIOAA1")) == GPIO_VALUE_LOW)
        strcat(cmd, " PMBus alert");
    }
    pal_add_cri_sel(cmd);
  }

}


// Generic Event Handler for GPIO changes, but only logs event when PLTRST_N is high and power on
static void gpio_event_handle_PLTRST(void *p)
{
  uint8_t status = 0;
  gpio_poll_st *gp = (gpio_poll_st*) p;
  FILE *fp;
  int rc;
  int val;
  char path[64] = {0};

  // check PLTRST
  sprintf(path, "/sys/class/gpio/gpio%d/value", gpio_num("GPIOR5"));
  if (fp = fopen(path, "r")) {
    rc = fscanf(fp, "%d", &val);
    fclose(fp);
  }
  if (!fp || rc != 1) {
#ifdef DEBUG
    syslog(LOG_INFO, "failed to read device %s", path);
#endif
    return;
  }
  // If PLTRST is low
  if (!val) {
    return;
  }

  // Check power
  pal_get_server_power(1, &status);
  if (status != SERVER_POWER_ON) {
    return;
  }

  if (gp->gs.gs_gpio == gpio_num("GPIOG2") ){ //FM_PCH_BMC_THERMTRIP_N
    if (reset_sec < 10)
      return;
  }

  log_gpio_change(gp, 0);
}

// GPIO table to be monitored when MB is ON
static gpio_poll_st g_gpios[] = {
  // {{gpio, fd}, edge, gpioValue, call-back function, GPIO description}
  {{1, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOB6", "PWRGD_SYS_PWROK" },
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOB7", "IRQ_PVDDQ_GHJ_VRHOT_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOD6", "FM_CPU_ERR0_LVT3_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOD7", "FM_CPU_ERR1_LVT3_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOE0", "RST_SYSTEM_BTN_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOE2", "FM_PWR_BTN_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOE4", "FP_NMI_BTN_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOE6", "FM_CPU0_PROCHOT_LVT3_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOE7", "FM_CPU1_PROCHOT_LVT3_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOF0", "IRQ_PVDDQ_ABC_VRHOT_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOF2", "IRQ_PVCCIN_CPU0_VRHOT_LVC3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOF3", "IRQ_PVCCIN_CPU1_VRHOT_LVC3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOF4", "IRQ_PVDDQ_KLM_VRHOT_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOG0", "FM_CPU_ERR2_LVT3_N"},
  {{0, 0}, GPIO_EDGE_FALLING, 0, gpio_event_handle_power, "GPIOG1", "FM_CPU_CATERR_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_PLTRST, "GPIOG2", "FM_PCH_BMC_THERMTRIP_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOG3", "FM_CPU0_SKTOCC_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_PLTRST, "GPIOI0", "FM_CPU0_FIVR_FAULT_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_PLTRST, "GPIOI1", "FM_CPU1_FIVR_FAULT_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOL0", "IRQ_UV_DETECT_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOL1", "IRQ_OC_DETECT_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOL2", "FM_HSC_TIMER_EXP_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOL4", "FM_MEM_THERM_EVENT_PCH_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOM0", "FM_CPU0_RC_ERROR_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOM1", "FM_CPU1_RC_ERROR_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOM4", "FM_CPU0_THERMTRIP_LATCH_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOM5", "FM_CPU1_THERMTRIP_LATCH_LVT3_N"},
  {{0, 0}, GPIO_EDGE_FALLING, 0, gpio_event_handle_power, "GPION3", "FM_CPU_MSMI_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOQ6", "FM_POST_CARD_PRES_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, platform_reset_event_handle, "GPIOR5", "RST_BMC_PLTRST_BUF_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOS0", "FM_THROTTLE_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOX4", "H_CPU0_MEMABC_MEMHOT_LVT3_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOX5", "H_CPU0_MEMDEF_MEMHOT_LVT3_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOX6", "H_CPU1_MEMGHJ_MEMHOT_LVT3_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOX7", "H_CPU1_MEMKLM_MEMHOT_LVT3_BMC_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOZ2", "IRQ_PVDDQ_DEF_VRHOT_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle_power, "GPIOAA0", "FM_CPU1_SKTOCC_LVT3_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOAA1", "IRQ_SML1_PMBUS_ALERT_N"},
  {{0, 0}, GPIO_EDGE_BOTH, 0, gpio_event_handle, "GPIOAB0", "IRQ_HSC_FAULT_N"},
};

static int g_count = sizeof(g_gpios) / sizeof(gpio_poll_st);

typedef enum {
  POWER_OFF = 0,
  POWER_ON = 1,
  DONT_CARE = 2,
} pwr_sts;

typedef struct {
  char gpio_name[8];
  uint8_t trigger_pwr_sts;
  uint8_t trigger_val;
} gpio_chk;

// GPIO pre-check table
static gpio_chk pre_check_table[] = {
  {"GPIOG3", DONT_CARE, GPIO_VALUE_INVALID},  //FM_CPU0_SKTOCC_LVT3_N
  {"GPIOAA0", DONT_CARE, GPIO_VALUE_INVALID}, //FM_CPU1_SKTOCC_LVT3_N
  {"GPIOG1", POWER_ON, GPIO_VALUE_LOW}, //FM_CPU_CATERR_LVT3_N
  {"GPION3", POWER_ON, GPIO_VALUE_LOW}, //FM_CPU_MSMI_LVT3_N
};

static int chk_table_count = sizeof(pre_check_table) / sizeof(gpio_chk);

// Thread for gpio timer
static void
gpio_pre_check() {
  int i, j;
  uint8_t status = 0;
  int ret;

  for ( i = 0; i < chk_table_count; i++) {
    for ( j = 0 ; j < g_count ; j++ ) {
      if( gpio_num(pre_check_table[i].gpio_name) == g_gpios[j].gs.gs_gpio ) {
        ret = gpio_get(gpio_num(pre_check_table[i].gpio_name));
        if(ret == -1) {
          syslog(LOG_WARNING, "gpio_pre_check:%s fail", pre_check_table[i].gpio_name);
          continue;
        }
        g_gpios[j].value = ret;
        if (pre_check_table[i].trigger_pwr_sts != DONT_CARE) {
          pal_get_server_power(1, &status);
          if ( (status == pre_check_table[i].trigger_pwr_sts) && ( gpio_read(&g_gpios[j].gs) == pre_check_table[i].trigger_val) ) {
            if( g_gpios[j].gs.gs_gpio == gpio_num("GPIOG1") )
              CATERR_irq++;
            else if( g_gpios[j].gs.gs_gpio == gpio_num("GPION3") )
              MSMI_irq++;
            else
              log_gpio_change(&g_gpios[j], 0);
          }
        } else {
          log_gpio_change(&g_gpios[j], 0);
        }
      }
    }
  }
}

// Thread for gpio timer
static void *
gpio_timer() {
  uint8_t status = 0;
  uint8_t fru = 1;
  long int pot;
  char str[MAX_VALUE_LEN] = {0};
  int tread_time = 0 ;

  while (1) {
    sleep(1);

    pal_get_server_power(fru, &status);
    if (status == SERVER_POWER_ON) {
      increase_timer(&reset_sec);
      pot = increase_timer(&power_on_sec);
    } else {
      pot = decrease_timer(&power_on_sec);
    }

    // Wait power-on.sh finished, then update pwr_server_last_state
    if (tread_time < 20) {
      tread_time++;
    } else {
      if (pal_get_last_pwr_state(fru, str) < 0)
        str[0] = '\0';
      if (status == SERVER_POWER_ON) {
        if (strncmp(str, POWER_ON_STR, strlen(POWER_ON_STR)) != 0) {
          pal_set_last_pwr_state(fru, POWER_ON_STR);
          syslog(LOG_INFO, "last pwr state updated to on\n");
        }
      } else {
        // wait until PowerOnTime < -2 to make sure it's not AC lost
        // Handle corner case during sled-cycle due to BMC residual electricity (1.2sec)
        if (pot < -2 && strncmp(str, POWER_OFF_STR, strlen(POWER_OFF_STR)) != 0) {
          pal_set_last_pwr_state(fru, POWER_OFF_STR);
          syslog(LOG_INFO, "last pwr state updated to off\n");
        }
      }
    }

  }
}
// The function for setting GPIO value
static void set_gpio_value(uint8_t *pin, uint8_t value)
{
  int ret = -1;
  uint8_t pin_number=0;

  pin_number = gpio_num(pin);

  ret = gpio_set(pin_number, value);

  if ( ret < 0 )
  {
      syslog(LOG_WARNING, "Fail to set the value %d to %s.\n", value, pin);
  }
}

// Thread for IERR/MCERR event detect
static void *
ierr_mcerr_event_handler() {
  uint8_t CATERR_ierr_time_count = 0;
  uint8_t MSMI_ierr_time_count = 0;
  uint8_t status = 0;

  while (1) {
    if ( CATERR_irq > 0 ){
      CATERR_ierr_time_count++;
      if ( CATERR_ierr_time_count == 2 ){
        if ( CATERR_irq == 1 ){
          pal_get_CPU_CATERR(1, &status);
          if (status == 0) {
            syslog(LOG_CRIT, "ASSERT: IERR/CATERR\n");
            pal_add_cri_sel("CPU IERR/CATERR");
          } else {
            syslog(LOG_CRIT, "ASSERT: MCERR/CATERR\n");
            pal_add_cri_sel("CPU MCERR/CATERR");
          }
            CATERR_irq--;
            CATERR_ierr_time_count = 0;
            //light up the fault LED
            set_gpio_value("GPIOU5", GPIO_VALUE_LOW);
            system("/usr/local/bin/autodump.sh &");
          } else if ( CATERR_irq > 1 ){
                   while (CATERR_irq > 1){
                        syslog(LOG_CRIT, "ASSERT: MCERR/CATERR\n");
                        pal_add_cri_sel("CPU MCERR/CATERR");
                        CATERR_irq = CATERR_irq - 1;
                   }
                   CATERR_ierr_time_count = 1;
                 }
        }
    }

    if ( MSMI_irq > 0 ){
      MSMI_ierr_time_count++;
      if ( MSMI_ierr_time_count == 2 ){
        if ( MSMI_irq == 1 ){
          pal_get_CPU_MSMI(1, &status);
          if (status == 0) {
            syslog(LOG_CRIT, "ASSERT: IERR/MSMI\n");
            pal_add_cri_sel("CPU IERR/MSMI");
          } else {
            syslog(LOG_CRIT, "ASSERT: MCERR/MSMI\n");
            pal_add_cri_sel("CPU MCERR/MSMI");
          }
          MSMI_irq--;
          MSMI_ierr_time_count = 0;
          //light up the fault LED
          set_gpio_value("GPIOU5", GPIO_VALUE_LOW);
          system("/usr/local/bin/autodump.sh &");
        } else if ( MSMI_irq > 1 ){
                 while (MSMI_irq > 1){
                      syslog(LOG_CRIT, "ASSERT: MCERR/MSMI\n");
                      pal_add_cri_sel("CPU MCERR/MSMI");
                      MSMI_irq = MSMI_irq - 1;
                 }
                 MSMI_ierr_time_count = 1;
               }
       }
    }
    usleep(25000); //25ms
   }
}

int
main(int argc, void **argv) {
  int dev, rc, pid_file;
  uint8_t status = 0;
  pthread_t tid_ierr_mcerr_event;
  pthread_t tid_gpio_timer;

  pid_file = open("/var/run/gpiod.pid", O_CREAT | O_RDWR, 0666);
  rc = flock(pid_file, LOCK_EX | LOCK_NB);
  if(rc) {
    if(EWOULDBLOCK == errno) {
      syslog(LOG_ERR, "Another gpiod instance is running...\n");
      exit(-1);
    }
  } else {

    daemon(0,1);
    openlog("gpiod", LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "gpiod: daemon started");

    //Create thread for IERR/MCERR event detect
    if (pthread_create(&tid_ierr_mcerr_event, NULL, ierr_mcerr_event_handler, NULL) < 0) {
      syslog(LOG_WARNING, "pthread_create for ierr_mcerr_event_handler\n");
      exit(1);
    }
    //Create thread for platform reset event filter check
    if (pthread_create(&tid_gpio_timer, NULL, gpio_timer, NULL) < 0) {
      syslog(LOG_WARNING, "pthread_create for platform_reset_filter_handler\n");
      exit(1);
    }

    gpio_poll_open(g_gpios, g_count);
    //GPIO status pre check
    gpio_pre_check();
    gpio_poll(g_gpios, g_count, POLL_TIMEOUT);
    gpio_poll_close(g_gpios, g_count);
  }

  return 0;
}
