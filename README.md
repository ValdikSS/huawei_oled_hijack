Huawei OLED hijacking library
=============================

This library adds advanced menu to the LED/OLED display of various Huawei porable LTE routers based on Balong V7R1, V7R2, V7R11 and V7R22 architecture.  
This is achieved by hijacking certain library calls in the "oled" executable file to create custom menus and handlers over the original informational page.

Supported & tested devices: E5372, E5770, E5885, E5577, E5377

Currently this library adds 8 new menu items:

* **Network Mode**  
 (Auto, GSM Only, UMTS Only, LTE Only, LTE -> UMTS, LTE -> GSM, UMTS -> GSM)
* **TTL Mangler** (set TTL of outbound packets to 64 or 128)
* **Anticensorship** ([DPI circumvention utility](https://github.com/bol-van/zapret))
* **DNS over TLS with optional local Ad Blocker** (using DNS blacklists)
* **IMEI Changer** (switch between stock, random Android and random Windows Phone IMEI)
* **Remote Access Control** (block telnet, ADB and web interface from being accessed over Wi-Fi and USB)
* **No Battery Mode** (allows device to work without battery)
* **USB Mode** (change USB composite device mode between stock, AT/Network/SD, AT/Network and Debug modes)
* **Custom script** (allows you to run your custom script handler from the menu)

![Screenshot 1](https://i.imgur.com/LioaPph.png) ![Screenshot 2](https://i.imgur.com/Z8UlVX4.png) ![Screenshot 3](https://i.imgur.com/mDXC7Yc.png) ![Screenshot 4](https://i.imgur.com/nR6fORk.png) ![Screenshot 5](https://i.imgur.com/hDS5V3O.png) ![Screenshot 6](https://i.imgur.com/ekUsutI.png)

## How does it work
This library hijacks certain functions to inject into oled process.

Fist, the architecture of OLED binary for 128×128 and 128×64 screens is very different. 128×128 has functional menus that trigger action, while 128×64 binary is very simple and only shows connection and Wi-Fi information and does not have functional menus.

On all Balong family, `register_notify_handler()` is used to register synchronous and asynchronous handlers. Async handler is replaced with proxy handler by the library to intercept key presses and handle bus information.  

On 128×128 devices, the main logic is performed by hijacking `sprintf()` call to detect `printf`'ed on-screen text.  
Custom menu is drawn in "Device Information" page, hijacking "Homepage" string. The library listens for POWER button press events on the exact page, executes page handler and redraws menu again, by leaving it and opening that same page again.  

On 128×64 the main logic is implemented in asynchronous handler itself, but it does essentially the same thing: replaces Homepage text. 128×64 menu is implemented in a carrousel-like style, without any additional on-screen elements, and the architecture does not require leaving-entering emulation, allowing the hijacked menu to look like a native one.

Some other functions, like `puts()`, `osa_print_log_ex()` and `osa_printf_log_null()` are also hijacked to handle things "from another thread", earlier or later than the button handler.

This library is very simple, yet effective.

## Used software

[Huaweicalc](https://github.com/forth32/huaweicalc) couresy of forth32  
**imei_generator** courtesy of Egor Grushko  
**atc** courtesy of rust3028

**Contributions are welcome**.
