EPICS IOC for Nordic thingy:52

R1-0
=================

R1-0-0 3/29/19
-----

- Supports temperature, humidity, pressure, orientation, button UUIDs (notify) and LED UUID (read)
- Notify UUIDs split between numeric values and string values (notifyNumber.template and notifyString.template)
- Notify UUID records trigger their own processing upon receipt of notifcation from device, read UUIDs must be
  triggered manually with a periodic scan
- Parse device response according to specific UUID, using Nordic's specifications
	- https://nordicsemiconductor.github.io/Nordic-Thingy52-FW/documentation/firmware_architecture.html
