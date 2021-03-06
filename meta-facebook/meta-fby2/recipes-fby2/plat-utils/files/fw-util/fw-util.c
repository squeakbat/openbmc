/*
 * fw-util
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
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/file.h>
#include <facebook/bic.h>
#include <openbmc/pal.h>
#include <openbmc/ipmi.h>

#define MAX_NUM_OPTIONS 6
enum {
  OPT_SLOT1 = 1,
  OPT_SLOT2 = 2,
  OPT_SLOT3 = 3,
  OPT_SLOT4 = 4,
  OPT_NIC   = 5,
  OPT_ALL   = 6,
};

static void
print_usage_help(void) {
  printf("Usage: fw-util <all|slot1|slot2|slot3|slot4|nic> <--version>\n");
  printf("       fw-util <all|slot1|slot2|slot3|slot4|nic> <--update> <--cpld|--bios|--bic|--bicbl|--nic|--vr> <path>\n");
}

static void
print_bmc_version(void) {
  char vers[128] = "NA";
  FILE *fp = fopen("/etc/issue", "r");
  if (fp) {
    fscanf(fp, "OpenBMC Release %s\n", vers);
    fclose(fp);
  }
  printf("BMC Version: %s\n", vers);
}

static void
print_slot_version(uint8_t slot_id) {
  int i;
  uint8_t ver[32] = {0};
  uint8_t status;
  int ret;

  if (slot_id < OPT_SLOT1 || slot_id > OPT_SLOT4) {
    printf("Not Supported Operation\n");
    return;
  }

  ret = pal_is_fru_prsnt(slot_id, &status);
  if (ret < 0) {
     printf("pal_is_fru_prsnt failed for fru: %d\n", slot_id);
     return;
  }
  if (status == 0) {
    printf("slot%d is empty!\n", slot_id);
    return;
  }

  ret = pal_is_server_12v_on(slot_id, &status);
  if(ret < 0 || 0 == status) {
    printf("slot%d 12V is off\n", slot_id);
    return;
  }

  if(!pal_is_slot_server(slot_id)) {
    printf("slot%d is not server\n", slot_id);
    return;
  }

  // Print CPLD Version
  if (bic_get_fw_ver(slot_id, FW_CPLD, ver)) {
    printf("CPLD Version: NA\n");
  }
  else {
    printf("CPLD Version: 0x%02x%02x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  }

  // Print Bridge-IC Version
  if (bic_get_fw_ver(slot_id, FW_BIC, ver)) {
    printf("Bridge-IC Version: NA\n");
  }
  else {
    printf("Bridge-IC Version: v%x.%02x\n", ver[0], ver[1]);
  }

  // Print Bridge-IC Bootloader Version
  if (bic_get_fw_ver(slot_id, FW_BIC_BOOTLOADER, ver)) {
    printf("Bridge-IC Bootloader Version: NA\n");
  }
  else {
    printf("Bridge-IC Bootloader Version: v%x.%02x\n", ver[0], ver[1]);
  }

  // Print ME Version
  if (bic_get_fw_ver(slot_id, FW_ME, ver)){
    printf("ME Version: NA\n");
  }
  else {
    printf("ME Version: %x.%x.%x.%x%x\n", ver[0], ver[1], ver[2], ver[3], ver[4]);
  }

  // Print PVCCIO VR Version
  if (bic_get_fw_ver(slot_id, FW_PVCCIO_VR, ver)){
    printf("PVCCIO VR Version: NA\n");
  }
  else {
    printf("PVCCIO VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  }

  // Print PVCCIN VR Version
  if (bic_get_fw_ver(slot_id, FW_PVCCIN_VR, ver)){
    printf("PVCCIN VR Version: NA\n");
  }
  else {
    printf("PVCCIN VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  }

  // Print PVCCSA VR Version
  if (bic_get_fw_ver(slot_id, FW_PVCCSA_VR, ver)){
    printf("PVCCSA VR Version: NA\n");
  }
  else {
    printf("PVCCSA VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  }

  // Print DDRAB VR Version
  if (bic_get_fw_ver(slot_id, FW_DDRAB_VR, ver)){
    printf("DDRAB VR Version: NA\n");
  }
  else {
    printf("DDRAB VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  }

  // Print DDRDE VR Version
  if (bic_get_fw_ver(slot_id, FW_DDRDE_VR, ver)){
    printf("DDRDE VR Version: NA\n");
  }
  else {
    printf("DDRDE VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  }

  // Print PVNNPCH VR Version
  if (bic_get_fw_ver(slot_id, FW_PVNNPCH_VR, ver)){
    printf("PVNNPCH VR Version: NA\n");
  }
  else {
    printf("PVNNPCH VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  }

  // Print P1V05 VR Version
  if (bic_get_fw_ver(slot_id, FW_P1V05_VR, ver)){
    printf("P1V05 VR Version: NA\n");
  }
  else {
    printf("P1V05 VR Version: 0x%02x%02x, 0x%02x%02x\n", ver[0], ver[1], ver[2], ver[3]);
  } 
    
  // Print BIOS version
  if (pal_get_sysfw_ver(slot_id, ver)) {
    printf("BIOS Version: NA\n");
  } 
  else {
    // BIOS version response contains the length at offset 2 followed by ascii string
    printf("BIOS Version: ");
    for (i = 3; i < 3+ver[2]; i++) {
      printf("%c", ver[i]);
    }
    printf("\n");
  }
}

static void
print_nic_version(uint8_t slot_id) {
  // TODO: Need to read nic card firmware version for print
  // There are 2 nic card vendors, BCRM and MEZZ 
  printf("NIC Card Version: NA\n");
}

// TODO: Need to confirm the interpretation of firmware version for print
// Right now using decimal to print the versions
static void
print_fw_ver(uint8_t slot_id) {
  int i;
  uint8_t ver[32] = {0};

  switch(slot_id) {
    case OPT_SLOT1:
    case OPT_SLOT2:
    case OPT_SLOT3:
    case OPT_SLOT4:
      printf("Get version info for slot%d\n", slot_id);
      print_slot_version(slot_id);
      break;
    case OPT_NIC:
      print_nic_version(slot_id);
      break;
  }

}

static int
check_dup_process(uint8_t slot_id) {
  int pid_file;
  char path[64];

  sprintf(path, "/var/run/fw-util_%d.lock", slot_id);
  pid_file = open(path, O_CREAT | O_RDWR, 0666);
  if (flock(pid_file, LOCK_EX | LOCK_NB) && (errno == EWOULDBLOCK)) {
    return -1;
  }

  return 0;
}

int
fw_update_slot(char **argv, uint8_t slot_id) {

  uint8_t status;
  int ret;
  char cmd[80];

  ret = pal_is_fru_prsnt(slot_id, &status);
  if (ret < 0) {
     printf("pal_is_fru_prsnt failed for fru: %d\n", slot_id);
     goto err_exit;
  }
  if (status == 0) {
    printf("slot%d is empty!\n", slot_id);
    goto err_exit;
  }

  ret = pal_is_server_12v_on(slot_id, &status);
  if(ret < 0 || 0 == status) {
    printf("slot%d 12V is off\n", slot_id);
    goto err_exit;
  }

  if(!pal_is_slot_server(slot_id)) {
    printf("slot%d is not server\n", slot_id);
    goto err_exit;
  }  

  if (!strcmp(argv[3], "--cpld")) {
     return bic_update_fw(slot_id, UPDATE_CPLD, argv[4]);
  }
  if (!strcmp(argv[3], "--bios")) {
    sprintf(cmd, "/usr/local/bin/power-util slot%u off", slot_id);
    system(cmd);
    ret = bic_update_fw(slot_id, UPDATE_BIOS, argv[4]);
    sleep(1);
    sprintf(cmd, "/usr/local/bin/power-util slot%u 12V-cycle", slot_id);
    system(cmd);
    sleep(5);
    sprintf(cmd, "/usr/local/bin/power-util slot%u on", slot_id);
    system(cmd);
    return ret;
  }
  if (!strcmp(argv[3], "--bic")) {
    return bic_update_fw(slot_id, UPDATE_BIC, argv[4]);
  }
  if (!strcmp(argv[3], "--bicbl")) {
    return bic_update_fw(slot_id, UPDATE_BIC_BOOTLOADER, argv[4]);
  }
  if (!strcmp(argv[3], "--vr")) {
    // TODO : Need to implement vr firmware update function
    goto err_exit;
  }

err_exit:
  print_usage_help();
  return -1;
}

int
fw_update_nic (char **argv, uint8_t slot_id) {
  // TODO: Need to implement nic card firmware update for BCRM and MEZZ
}

int
update_fw(char **argv, uint8_t slot_id) {
  int ret = 0;
  int opt = 0;

  switch(slot_id) {
    case OPT_SLOT1:
    case OPT_SLOT2:
    case OPT_SLOT3:
    case OPT_SLOT4:
      if (check_dup_process(slot_id) < 0) {
        printf("fw_util: another instance is running...\n");
        ret = -1;
      } 
      else {
        ret = fw_update_slot(argv, slot_id);
      }
      break;
    case OPT_NIC:
      if (check_dup_process(slot_id) < 0) {
        printf("fw_util: another instance is running...\n");
        ret = -1;
      }
      else {
        ret = fw_update_nic(argv, slot_id);
      }
      break;
    case OPT_ALL:
      for (opt = OPT_SLOT1 ; opt <= OPT_SLOT4; opt++) {
        if (check_dup_process(opt) < 0) {
          printf("fw_util: another instance is running...\n");
          ret = -1;
          break;
        }
      }

      printf("Updating all slots....\n");
      for (opt = OPT_SLOT1; opt <= OPT_SLOT4; opt++) {
        if (fw_update_slot(argv, opt)) {
          printf("fw_util: updating %s on slot %d failed!\n", argv[3], opt);
          ret = -1;
        }
      }
      break;
  }

  return ret;
}

void
print_update_result(int ret, uint8_t slot_id) {

  switch(slot_id) {
    case OPT_SLOT1:
    case OPT_SLOT2:
    case OPT_SLOT3:
    case OPT_SLOT4:
      if (ret < 0) {
        printf("fw_util: updating slot %d failed!\n",slot_id);
      }
      else {
        printf("fw_util: updated slot %d successfully!\n",slot_id);
      }
      break;
    case OPT_NIC:
      if (ret < 0) {
        printf("fw_util: updating nic fw failed!\n");
      }
      else {
        printf("fw_util: updated nic fw successfully!\n");
      }      
      break;
    case OPT_ALL:
      if (ret < 0) {
        printf("fw_util: updating all slots failed!\n");
      }
      else {
        printf("fw_util: updated all slots successfully!\n");
      }
      break;
  }
}

int
main(int argc, char **argv) {

  uint8_t slot_id;
  int ret = 0;
  char cmd[80];
  // Check for border conditions
  if ((argc != 3) && (argc != 5)) {
    goto err_exit;
  }

  // Derive slot_id from first parameter
  if (!strcmp(argv[1], "slot1")) {
    slot_id = OPT_SLOT1;
  } else if (!strcmp(argv[1] , "slot2")) {
    slot_id = OPT_SLOT2;
  } else if (!strcmp(argv[1] , "slot3")) {
    slot_id = OPT_SLOT3;
  } else if (!strcmp(argv[1] , "slot4")) {
    slot_id = OPT_SLOT4;
  } else if (!strcmp(argv[1] , "nic")) {
    slot_id = OPT_NIC;
  } else if (!strcmp(argv[1] , "all")) {
    slot_id = OPT_ALL;
  } else {
      goto err_exit;
  }
  
  // check operation to perform
  if (!strcmp(argv[2], "--version")) {
     
     if (slot_id < OPT_ALL) {
       print_fw_ver(slot_id);
       return 0;
     }

     // Print BMC Version
     print_bmc_version();
     printf("\n");
     for (slot_id = OPT_SLOT1; slot_id < MAX_NUM_OPTIONS; slot_id++) {
        print_fw_ver(slot_id);
        printf("\n");
     }
     return 0;
  }
  if (!strcmp(argv[2], "--update")) {
    if (argc != 5) {
      goto err_exit;
    }

    ret = update_fw(argv, slot_id);

    print_update_result(ret, slot_id);
    return ret; 
  }
err_exit:
  print_usage_help();
  return -1;
}
