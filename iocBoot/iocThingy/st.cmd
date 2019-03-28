#!../../bin/linux-x86_64/thingy

< envPaths

## Register all support components
dbLoadDatabase "$(TOP)/dbd/thingy.dbd"
thingy_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords "$(TOP)/db/notify.db"
dbLoadRecords "$(TOP)/db/readwrite.db"

iocInit
dbpf("XF:10IDB{THINGY:001}TemperatureNotifier.SCAN","1 second")
