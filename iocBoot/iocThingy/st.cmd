#!../../bin/linux-x86_64/thingy

< envPaths


## Register all support components
dbLoadDatabase "$(TOP)/dbd/thingy.dbd"
thingy_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords "$(TOP)/db/notifyNumber.db"
dbLoadRecords "$(TOP)/db/notifyString.db"
dbLoadRecords "$(TOP)/db/readwrite.db"

iocInit
dbpf("XF:10IDB{THINGY:001}LEDReader.SCAN","1 second")
dbpf("XF:10IDB{THINGY:001}BatteryReader.SCAN","1 second")
