ASHS Driver - ACPI Wireless Switch
===

ASUS Wireless switch Driver to handle Fn key action.

Device /sys/bus/acpi/devices/ATK4002:00 (ACPI path: \_SB_.ASHS). 

Currently implements exact functionality of the ACPI handler without 
any other improvement - makes sense if you don't want to disable OSI
"!Windows 2012"

ACPI objects:
==
\_SB_.ASHS.HSWC() - Toggles WL LED. Not doing much else
\OWGD(st) - actual method which does LED switch in above
\OWGS() - returns current status of the led
\WLDP - Wireless Device Presence flag
\BTDP - Bluetooth Device Presence flag
\WRST - Wireless Runtime Status
\BRST - Bluetooth Runtime Status
\OWLD(st) - sets Operational state of Wireless Device
\OBTD(st) - sets Operational state of Bluetooth Device
\_SB_.ATKD.WAPF - Wireless switch Application Function

when bit 2 (LSB - WAPF & 0x4) is set - switch only sends key event assuming 
software will control the wireless (eg. NetworkManager)
when bit 0 is set (LSB) - acts as airplane mode switch - toggles both radios
otherwise switches radios in sequence WL/BT->wl/BT->WL/bt->wl/bt->WL/BT

also see modinfo asus-nb-wmi on how to set it

\_SB_.ATKD.IANE - sends ACPI key event
when wireless mode is toggled various ACPI key codes are sent:
0x88 - (WAPF & 0x4)==1, Wireless key is pressed, handle in software
0x73 - Airplane mode is off - radios enabled
0x74 - Airplane mode is on - radios disabled
0x5E - Wireless enabled
0x5F - Wireless disabled
0x7D - Bluetooth enabled
0x7E - Bluetooth disabled

\_SB_.PCI0.LPCB.EC0_._Q0B - Wireless key(Fn+F2) ACPI handler
\MSOS() - ACPU function returning current OSI level

Driver is used by ACPI handler when MSOS>=OSW8 (Win8) otherwise same WAPF
handling is processed stright in handler without involving software driver.
Software driver under Win8 is supposed to better handle power management.
