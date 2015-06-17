SparkFun Speed Trap
=======

This is a project demonstrating how to display instantaneous speed from a LIDAR on two [large 7-segment displays](https://www.sparkfun.com/products/8530).

![Speed displayed on wall](https://github.com/sparkfun/Speed_Trap/blob/master/Wall%20Display.png)

*Note the handprints on the wall...*

The new [LIDAR-Lite](https://www.sparkfun.com/products/13167) from PulsedLight is pretty nice. It outputs readings very quickly. From multiple distance readings we can calculate speed (velocity is the derivative of position).

Repository Contents
-------------------

* **/firmware** - The code that runs on the Arduino
* **/hardware** - An Arduino shield to easily connect a computer PSU and the 6 wires for the large digit driver backpack

License Information
-------------------

This design is [OSHW](http://www.oshwa.org/definition/) and public domain but you buy me a beer if you use this and we meet someday ([Beerware license](http://en.wikipedia.org/wiki/Beerware)).