#!/bin/bash

# compile and remove sCalcout records that IOC doesnt like
make clean uninstall
make
chmod +w dbd/thingy.dbd
sed -i '/recordtype(scalcout)/,+1d' dbd/thingy.dbd
sed -i '/scalcout/d' dbd/thingy.dbd
chmod -w dbd/thingy.dbd
