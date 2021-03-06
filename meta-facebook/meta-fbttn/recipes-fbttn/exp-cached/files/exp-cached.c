/*
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
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <openbmc/ipmi.h>
#include <openbmc/ipmb.h>
#include <openbmc/pal.h>
#include <facebook/exp.h>

void
fruid_cache_init(void) {
  // Initialize Slot0's fruid
  int ret;
  int i;
  char fruid_temp_path[64] = {0};
  char fruid_path[64] = {0};

  sprintf(fruid_temp_path, "/tmp/tfruid_scc.bin");
  sprintf(fruid_path, "/tmp/fruid_scc.bin");

  ret = exp_read_fruid(fruid_temp_path, FRU_SCC);
  if (ret) {
    syslog(LOG_ERR, "%s: exp_read_fruid failed with FRU:%d\n", __func__, FRU_SCC);
  }

  rename(fruid_temp_path, fruid_path);

  sprintf(fruid_temp_path, "/tmp/tfruid_dpb.bin");
  sprintf(fruid_path, "/tmp/fruid_dpb.bin");

  //delay 3 seconds betwenn for two continuous commands
  sleep(3);

  ret = exp_read_fruid(fruid_temp_path, FRU_DPB);
  if (ret) {
    syslog(LOG_ERR, "%s: exp_read_fruid failed with FRU:%d\n", __func__, FRU_DPB);
  }

  rename(fruid_temp_path, fruid_path);
  return;
}

int
main (int argc, char * const argv[])
{
  fruid_cache_init();
  return 0;
}
