ASHS Driver - ACPI Wireless Switch
===

ASUS Wireless switch Driver to handle Fn key action.

Note: The driver is written for ASUS laptops with Airplane mode LED
(eg. Transformer Trio) so the led should be on only when all radios 
are off. On boot LED is on, presumably indicating that till drivers
are loaded radios are off. Hence the driver is switching off the led
on load - if at least one radio is on (rfkill preserves its state).

Note: The driver deals directly with ACPI internals, which may interfere
with RFKILL subsystem, especially WMI rfkills. However WMI rfkills may
also affect real PCI devices. Together with systemd-rfkill@ service
restoring last rfkill state - may create quite a headache.
Use direct ACPI knobs (owg, wld) to change hard state of the rfkill.
To function properly wld requires wapf to remain 0.

Device /sys/bus/acpi/devices/ATK4002:00 (ACPI path: \_SB_.ASHS). 

Currently implements exact functionality of the ACPI handler without 
any other improvement - makes sense if you don't want to disable OSI
"!Windows 2012"

Current implementation uses mixed mode of hardware switch via acpi and
rfkill switch triggered through input event handler for input event
emited by asus-nb-wmi for acpi key event triggered by this driver.

When WAPF = 4 - driver sends ACPI scancode 0x88 which is converted
by asus-wmi to RFKILL key, which is processed by all registerd rfkill
drivers to toggle their state.

When WAPF = 2 - driver performs hardware toggle (via ACPI call) of the
Bluetooth radio - (dis)connects from USB, and emits scancode 0x5D which
is translated to WLAN key causing all WL rfkills to toggle.

For other values of WAPF it sends 0x5E,0x5F,0x7D,0x7E sequence which is
supposed to toggle radios in sequence, although asus-wmi just converts
them all to WL and BT toggle events hence result is the same.

Low Level Details:
==
ACPI objects:
===
- \_SB_.ASHS.HSWC() - Toggles WL LED. Not doing much else
- \OWGD(st) - actual method which does LED switch in above
- \OWGS() - returns current status of the led
- \WLDP - Wireless Device Presence flag
- \BTDP - Bluetooth Device Presence flag
- \OHWR - Hardware Resources - LSB bit 8 reflects BTDP
- \WRST - Wireless Runtime Status
- \BRST - Bluetooth Runtime Status
- \OWLD(st) - sets Operational state of Wireless Device
- \OBTD(st) - sets Operational state of Bluetooth Device
- \_SB_.ATKD.WAPF - Wireless switch Application Function

when bit 2 (LSB - WAPF & 0x4) is set - switch only sends key event assuming 
software will control the wireless (eg. NetworkManager)
when bit 0 is set (LSB) - acts as airplane mode switch - toggles both radios
otherwise switches radios in sequence WL/BT->wl/BT->WL/bt->wl/bt->WL/BT

Additionally, OWLD does not perform actual hardware switch unless bits 0 and
2 are not zero (either of them is set) making dry run. 

also see modinfo asus-nb-wmi on how to set WAPF from userspace

- \_SB_.ATKD.CWAP - Change WAPF - this call is used by ACPI for WMI set.

Note: CWAP uses bitwise OR to set bits in WAPF - that means you can only
set bits, not clear them. So once WAPF is non-zero you can never reset it back.
Only after reboot.

- \_SB_.ATKD.IANE - sends ACPI key event

when wireless mode is toggled various ACPI key codes are sent:
- 0x88 - (WAPF & 0x4)==1, Wireless key is pressed, handle in software(toggle)
- 0x73 - Airplane mode is off - radios enabled
- 0x74 - Airplane mode is on - radios disabled
- 0x5E - Wireless enabled
- 0x5F - Wireless disabled
- 0x7D - Bluetooth enabled
- 0x7E - Bluetooth disabled

- \_SB_.PCI0.LPCB.EC0_._Q0B - Wireless key(Fn+F2) ACPI handler
- \MSOS() - ACPU function returning current OSI level

Driver is used by ACPI handler when MSOS>=OSW8 (Win8) otherwise same WAPF
handling is processed stright in handler without involving software driver.
Software driver under Win8 is supposed to better handle power management.

