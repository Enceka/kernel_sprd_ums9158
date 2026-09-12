#!/system/bin/sh
# Dump the *runtime* device tree nodes for USB / PHY / touch.  Run as root.
# Reason: our in-tree phy/musb drivers complain about missing properties
# (sprd,hsphy-tuneeq / refclk_cfg), so we need to know which properties this
# board's device tree actually has.

echo "== device-tree top =="
ls /proc/device-tree 2>/dev/null | tr '\n' ' '; echo

echo "== find usb/phy nodes =="
for base in /proc/device-tree/soc /proc/device-tree/soc@0 /proc/device-tree; do
	[ -d "$base" ] || continue
	for n in "$base"/*usb* "$base"/*hsphy* "$base"/*usbphy*; do
		[ -d "$n" ] || continue
		echo "--- $n"
		echo "    props: $(ls $n | tr '\n' ' ')"
		for p in compatible dr_mode status; do
			[ -f "$n/$p" ] && echo "    $p = $(tr '\0' ' ' < $n/$p)"
		done
	done
done

echo "== hsphy tuning properties =="
for hp in /proc/device-tree/soc/*hsphy* /proc/device-tree/*hsphy*; do
	[ -d "$hp" ] || continue
	echo "--- $hp"
	for p in "sprd,hsphy-tunehsamp" "sprd,hsphy-tuneeq" "sprd,hsphy-tfregres" \
		 "sprd,refclk_cfg" "refclk-cfg" "sprd,phy-tune" "sprd,hsphy-efuse"; do
		if [ -e "$hp/$p" ]; then
			echo "    $p = $(od -An -tu1 "$hp/$p" | tr -s ' ')"
		fi
	done
done

echo "== typec / charger nodes =="
for n in /proc/device-tree/soc/*spi*/*typec* /proc/device-tree/soc/*i2c*/*charger*; do
	[ -d "$n" ] || continue
	echo "--- $n"
	echo "    compatible = $(tr '\0' ' ' < $n/compatible 2>/dev/null)"
done

echo "== touch node =="
for n in /proc/device-tree/soc/*i2c*/*tlsc6x* /proc/device-tree/soc/*i2c*/*sitronix*; do
	[ -d "$n" ] || continue
	echo "--- $n"
	echo "    compatible = $(tr '\0' ' ' < $n/compatible 2>/dev/null)"
	echo "    props: $(ls $n | tr '\n' ' ')"
done

echo "== /proc/interrupts (usb/typec/touch) =="
grep -iE 'usb|typec|hsphy|tlsc|2270000' /proc/interrupts 2>/dev/null

echo "== done =="
