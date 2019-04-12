# ThingyIOC
An EPICS IOC for Nordic Thingy 52

## Requirements ##
- Bluetooth Low Energy connectivity
- A Thingy
- Software requirements:
  - The Bluetooth GATT C library:
    - https://github.com/labapart/gattlib
    - Follow instructions there to build, pack and install the library
  - GLib C library
  - EPICS Base, asyn
  
## Setup ##
  To connect to your Thingy, its MAC address must be known. There are several Bluetooth command-line tools to do this. Try:

```
$ bluetoothctl
[bluetooth]# scan on
```

```$ hcitool lescan```

Either of these commands should give a list of nearby Bluetooth devices with their MAC address and name. Unless you've changed 
it, the Thingy's name should be Thingy. Find its MAC address, and enter it into ```ThingyApp/src/bluetooth.c``` as the 
```MAC_ADDRESS```. This is all the set up that is necessary, however if you want to configure which sensors are read you can edit 
the ```.substitions``` files in ```ThingyApp/Db``` to pick the UUIDs you want to read. You can find the UUIDs for each sensor on 
Nordic's site: https://nordicsemiconductor.github.io/Nordic-Thingy52-FW/documentation/firmware_architecture.html


Note that not all of the sensors are 
currently supported; see ```RELEASE.md``` for more info. 

## Running the IOC ##

Before running the IOC, edit the ```.substitutions``` files in ```ThingyApp/Db``` to configure the names of your PVs. The PV 
names are of the form ${Sys}${Dev}${Attr}. For UUIDs which return several values, use the CHOICE field to pick between them. For 
read/write UUIDs, configure a periodic scan of the PV in ```iocBoot/iocThingy/st.cmd```. After configuring the PVs, compile the 
IOC with ```make``` and run it with ```st.cmd```.

If the IOC is failing to connect to the Thingy, try installing [bluepy](https://github.com/IanHarvey/bluepy). Somehow it helps enable connecting to the Thingy.
