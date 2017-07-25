Huawei OLED hijacking library
=============================

This library adds advanced menu to the OLED display of Huawei E5372 porable LTE router (and probably others).  
This is achieved by hijacking certain library calls in the "oled" executable file.

Currently this library adds 3 new menu items:

* **Network Mode**  
 (Auto, GSM Only, UMTS Only, LTE Only, LTE -> UMTS, LTE -> GSM, UMTS -> GSM)
* **TTL Mangler** (set TTL of outbound packets to 64 or 128)
* **IMEI Changer** (switch between stock, random Android and random Windows Phone IMEI)

## How does it work
This library hijacks `sprintf()`, `osa_print_log_ex()` and `register_notify_handler()`.

`sprintf()` is hijacked to inject custom menu items, `register_notify_handler()` registers it's own proxy handler which intercepts button press events.

Custom menu is drawn in "Device Information" page, hijacking "Homepage" string. The library listens for POWER button press events on the exact page, executes page handler and redraws menu again, by leaving it and opening that same page again.

Very simple, yet effective.

**Contributions are welcome**.