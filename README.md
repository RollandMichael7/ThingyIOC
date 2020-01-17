# ThingyIOC

Author: Michael Rolland  
Corresponding Author: Kazimierz Gofron  
Created: March 8, 2019  
Last Updated: July 16, 2020   
Copyright (c): 2019 Brookhaven National Laboratory  

**An EPICS IOC for the Nordic Thingy:52**

Supported sensors:
- Step counter
- Air quality (gas)
- Gravity vectors
- Quaternions
- Tap
- Battery
- Accelerometer
- Gyroscope
- Compass
- Roll, pitch, yaw
- Heading
- Temperature
- Humidity
- Pressure
- Button
- LED (read only)

## Known Issues ##
- Due to a concurrency issue, if the IOC is started without the Thingy paired to the host device the IOC will crash as several threads attempt to connect at once. Simply restart the IOC and it will work.

## Requirements ##
- Bluetooth Low Energy connectivity
- A Thingy
- Software requirements:
  - Bluetooth GATT C library
    - https://github.com/labapart/gattlib
    - Follow instructions there to build, pack and install the library
  - GLib C library
  - EPICS Base
  
## Setup ##
  To connect to your thingy, its MAC address must be known. There are several Bluetooth command-line tools to do this. Try:

```
$ bluetoothctl
[bluetooth]# scan on
```

```$ hcitool lescan```

If your Bluetooth is set up correctly, either of these commands should give a list of nearby Bluetooth devices with their MAC 
address and name. Unless you've changed it, the thingy's name should be Thingy. You may see another device named ThingyDFU; this 
is used for remote device firmware updates, so do not connect to it! Once you've found your thingy's MAC address, enter it 
into ```iocBoot/iocThingy/st.cmd``` as the argument to ```thingyConfig()```. This is all the set up that is necessary, however if 
you want to configure which sensors are read you can edit the ```.substitutions``` files in ```ThingyApp/Db``` to pick the UUIDs 
you want to read. Comment out any sensors you don't want to use with the hashtag symbol ```#```. You can find the UUIDs for each sensor on Nordic's site: https://nordicsemiconductor.github.io/Nordic-Thingy52-FW/documentation/firmware_architecture.html

Note that not all of the sensors are currently supported. 

## Running the IOC ##

Before running the IOC, edit the ```.substitutions``` files in ```ThingyApp/Db``` to configure the names of your PVs. The PV 
names are of the form ${Sys}${Dev}${Attr}. For UUIDs which return several values, use the CHOICE field to pick between them. For 
read/write UUIDs, configure a periodic scan of the PV in ```iocBoot/iocThingy/st.cmd```. After configuring the PVs, edit 
```configure/RELEASE``` to point to your installation of EPICS, compile the IOC with ```make``` and run it with the ```st.cmd``` 
file.

If the IOC is failing to connect to the Thingy, try installing [bluepy](https://github.com/IanHarvey/bluepy). Somehow it helps 
enable connecting to the thingy.
