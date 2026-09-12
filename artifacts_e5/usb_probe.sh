#!/system/bin/sh
# E5 USB / gadget / touch state dump - run as root on the device.
#
# Usage (host side):
#   adb push artifacts_e5/usb_probe.sh /data/local/tmp/
#   adb shell su -c 'sh /data/local/tmp/usb_probe.sh' > artifacts_e5/<state>.txt
#
# The point is to be able to diff the *same* dump between the stock image and
# our kernel.  Key fields: UDC state (configured == host enumerated us), the
# function links under configs/b.1, the musb module parameters and the tail of
# dmesg.

echo "== uname =="; uname -a
echo "== uptime =="; cat /proc/uptime
echo "== cmdline =="; cat /proc/cmdline | tr ' ' '\n' | grep -v '^$'
echo "== bootconfig =="; cat /proc/bootconfig 2>/dev/null

echo "== udc =="
ls /sys/class/udc/ 2>/dev/null
for u in /sys/class/udc/*; do
	[ -e "$u" ] && echo "$(basename $u): state=$(cat $u/state 2>/dev/null)"
done

echo "== gadget =="
echo "UDC=$(cat /config/usb_gadget/g1/UDC 2>/dev/null)"
echo "idVendor=$(cat /config/usb_gadget/g1/idVendor 2>/dev/null)"
echo "idProduct=$(cat /config/usb_gadget/g1/idProduct 2>/dev/null)"
ls -l /config/usb_gadget/g1/configs/b.1/ 2>/dev/null | grep '^l'
echo "current=$(getprop sys.usb.config) / state=$(getprop sys.usb.state)"
echo "persist=$(getprop persist.sys.usb.config)"

echo "== module params =="
lsmod 2>/dev/null | grep -iE 'musb|phy|usbm|extcon|bc1p2|typec|tlsc|charger|fuel' 
for m in musb_hdrc musb_sprd phy_sprd_qogirn6lite sprd_usbm sprd_bc1p2; do
	echo "-- $m"
	for p in /sys/module/$m/parameters/*; do
		[ -e "$p" ] && echo "   $(basename $p)=$(cat $p 2>/dev/null)"
	done
done

echo "== power_supply =="
for s in /sys/class/power_supply/*; do
	[ -e "$s" ] || continue
	echo "$(basename $s): type=$(cat $s/type 2>/dev/null) online=$(cat $s/online 2>/dev/null) status=$(cat $s/status 2>/dev/null)"
done

echo "== input devices =="
grep -E '^N:|^S:' /proc/bus/input/devices 2>/dev/null | head -40

echo "== i2c devices =="
ls /sys/bus/i2c/devices/ 2>/dev/null | tr '\n' ' '; echo
echo "== touch node (3-002e) =="
ls /sys/bus/i2c/devices/3-002e/ 2>/dev/null | tr '\n' ' '; echo

echo "== dmesg: usb / touch / watchdog =="
dmesg 2>/dev/null | grep -iE 'musb|hsphy|vbus|udc|gadget|pullup|tlsc|wdt|watchdog' | tail -60

echo "== pstore =="
ls -l /sys/fs/pstore/ 2>/dev/null
echo "== done =="
