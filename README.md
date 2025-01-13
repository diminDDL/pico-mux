# pico-mux
A Raspberry Pi Pico-based USB-controlled GPIO MUX.

This is a simple USB-controlled GPIO MUX based on the Raspberry Pi Pico and its PIO peripheral. I created it because I needed to brute-force a JTAG pinout with over 100 permutations.

The MUX features 8 unidirectional channels. The PIO handling the pin mappings runs at 400 MHz and requires only 2 clock cycles at worst to detect a change in the input pin. This means that digital signals up to 200 MHz can pass through the MUX. (To actually achieve this speed, you might need to disable the GPIO input synchronizer, but it is currently left enabled.)

The MUX can be controlled via USB serial using the following format (`\n` terminated):
```
[(2,6),(3,7),(4,8),(5,9),(10,14),(11,15),(12,16),(13,17)]
```
This maps GPIO `2` to `6`, `3` to `7`, ... `13` to `17`. Incidentally, this is the default configuration. 

To clarify, `(2, 6)` means that the signal on GPIO `2` will be read and output on GPIO `6`. The MUX is unidirectional, so the signal on GPIO `6` will **not** be read back on GPIO `2`.

You can also use the `controller.py` script to programmatically control the MUX.

The parser quite robust input validation, but it may not handle all edge cases perfectly. Double-check your scripts before running them, especially if connected to sensitive hardware.

If you need to adjust the voltage levels, you can power the Raspberry Pi Pico via the 3.3V pin with the 3V3_EN pin tied to ground. The official voltage range is 1.8V to 3.3V, but it can be pushed up to 4.5V or even 5V if you're willing to accept the risk of damaging the Pico over time. Or you can use a level shifter.
