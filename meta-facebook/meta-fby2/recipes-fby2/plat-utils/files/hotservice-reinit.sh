
. /usr/local/fbpackages/utils/ast-functions

echo "Hot service is triggered, re-init process... "

function device_config() {
      SLOT_N=$1
      SLOT_B=$2

      case $SLOT_N in
          1)
            PE_BUFF_OE_0=B4
            PE_BUFF_OE_1=B5
            I2C_REG=0x1e78a084
          ;;
          3)
            PE_BUFF_OE_0=B6
            PE_BUFF_OE_1=B7
            I2C_REG=0x1e78a184
          ;;
      esac
    
      # Delete devices on I2C bus 1 and bus 5 if there are device cards
      # EEPROM, 0xA2
      i2c_remove_device $SLOT_B 0x51
      # Inlet temp sensor, 0x9A
      i2c_remove_device $SLOT_B 0x4d
      # outlet temp sensor, 0x9C
      i2c_remove_device $SLOT_B 0x4e
      # I2C mux, 0xE2
      i2c_remove_device $SLOT_B 0x71
      # I2C GPIO, 0x40
      i2c_remove_device $SLOT_B 0x20
      # Voltage sensor, 0x80
      i2c_remove_device $SLOT_B 0x40

      if [ $(is_server_prsnt $(($SLOT_N+1))) == "1" ] && [ $(get_slot_type $(($SLOT_N+1))) == "0" ] ; then
        if [ $(is_server_prsnt $SLOT_N) == "1" ] && [ $(get_slot_type $SLOT_N) != "0" ] ; then
           gpio_set $PE_BUFF_OE_0 0
           gpio_set $PE_BUFF_OE_1 0
          
           devmem $I2C_REG w 0xFFF77304
        
           # EEPROM, 0xA2
           i2c_add_device $SLOT_B 0x51 24c128
           # Inlet temp sensor, 0x9A
           i2c_add_device $SLOT_B 0x4d tmp75
           # outlet temp sensor, 0x9C
           i2c_add_device $SLOT_B 0x4e tmp75
           # I2C mux, 0xE2
           i2c_add_device $SLOT_B 0x71 pca9551
           # I2C GPIO, 0x40
           i2c_add_device $SLOT_B 0x20 pca9551
           # Voltage sensor, 0x80
           i2c_add_device $SLOT_B 0x40 ina230
        else
           gpio_set $PE_BUFF_OE_0 1
           gpio_set $PE_BUFF_OE_1 1
       
           devmem $I2C_REG w 0xFFF99300
        fi
      fi
}

function set_sysconfig() {
      SLOT_N=$1
      SLOT_B=$2 

      case $SLOT_N in
          1)
            device_config $SLOT_N $SLOT_B
          ;;
          2)
            # Detect invalid configuration and then power-down the 12V for that slot(e.g. GP/CF plugged in to slot#2/slot#4)
            if [ $(is_server_prsnt $SLOT_N) == "1" ] && [ $(get_slot_type $SLOT_N) != "0" ] ; then
                  gpio_set O5 0
                  logger -p user.crit "Invalid configuration on SLOT$SLOT_N"

                  if [ $(is_server_prsnt $(($SLOT_N-1))) == "1" ] && [ $(get_slot_type $(($SLOT_N-1)))  != "0" ] ; then
                     gpio_set O4 0
                  fi
            fi
          ;;
          3)
            device_config $SLOT_N $SLOT_B
          ;;
          4)
            # Detect invalid configuration and then power-down the 12V for that slot(e.g. GP/CF plugged in to slot#2/slot#4)
            if [ $(is_server_prsnt $SLOT_N) == "1" ] && [ $(get_slot_type $SLOT_N) != "0" ] ; then
                  gpio_set O7 0
                  logger -p user.crit "Invalid configuration on SLOT$SLOT_N"
                  if [ $(is_server_prsnt $(($SLOT_N-1))) == "1" ] && [ $(get_slot_type $(($SLOT_N-1)))  != "0" ] ; then
                     gpio_set O6 0
                  fi
            fi
          ;;
      esac
}

SLOT=$1
OPTION=$2

case $SLOT in
    slot1)
      SLOT_NUM=1
      ;;
    slot2)
      SLOT_NUM=2
      ;;
    slot3)
      SLOT_NUM=3
      ;;
    slot4)
      SLOT_NUM=4
      ;;
    *)
      N=${0##*/}
      N=${N#[SK]??}
      echo "Usage: $N {slot1|slot2|slot3|slot4} {start}"
      exit 1
      ;;
esac

SLOT_BUS=$(get_slot_bus $SLOT_NUM)

case $OPTION in
    start)
      if [ $(is_server_prsnt $SLOT_NUM) == 0 ]; then
         exit 1
      fi
      echo "start to re-init for slot$SLOT_NUM insertion"

      # Delay 1 second for slot_type voltage ready               
      sleep 2

      # System Configuration
      echo "reset system configuration for $SLOT $OPTION"
      /etc/init.d/setup-platform.sh

      # Remove Service for new device/server
      # Sensor
      sv stop sensord
      rm -rf /tmp/cache_store/$SLOT*
      set_sysconfig $SLOT_NUM $SLOT_BUS
      
      # GPIO
      ps | grep -v 'grep' | grep 'gpiod' |awk '{print $1}'| xargs kill

      # IPMB
      sv stop ipmbd_$SLOT_BUS

      # Console
      ps | grep -v 'grep' | grep 'consoled '$SLOT'' |awk '{print $1}'| xargs kill

      # Delay to wait for sv stop
      sleep 3

      # Restart Service for new device/server
      echo "restart consoled for $SLOT $OPTION"
      if [[ $(is_server_prsnt $SLOT_NUM) == "1" && $(get_slot_type $SLOT_NUM) == "0" ]] ; then
         /usr/local/bin/consoled slot$SLOT_NUM --buffer
      fi

      echo "restart ipmbd for $SLOT $OPTION"
      if [[ $(is_server_prsnt $SLOT_NUM) == "1" && $(get_slot_type $SLOT_NUM) == "0" ]]; then
         sv start ipmbd_$SLOT_BUS
         /usr/local/bin/bic-cached $SLOT_NUM > /dev/null 2>&1 &
      fi

      echo "restart gpiod for $SLOT $OPTION"
      /etc/init.d/setup-gpiod.sh

      echo "restart sensord for $SLOT $OPTION"
      sv start sensord 

      ;;
    *)
      N=${0##*/}
      N=${N#[SK]??}
      echo "Usage: $N {slot1|slot2|slot3|slot4} {start}"
      exit 1
      ;;
esac

echo "done."
