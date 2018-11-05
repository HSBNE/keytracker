# keytracker
Are the keys in the key-cupboard? 

Method
======
Board is a Wemos D1 Mini. Now with OTA reprogramming!

Key-rings are attached to 1/4" jack plugs (each containing a E12 resistor).
The s/w tracks which keys are present and shouts out to the world via..
  QMTT topics : keytracker/removed, keytracker/returned, keytracker/status.
  JSON collection : http://sense-keytracker.local/status
  Web page : http://sense-keytracker.local/index.html

**TODO: change to poll portal.hsbne.org/api/ with a heartbeat and status changes.

OTA is now used to publish hostname 'SENSE-keytracker'.

**TODO: Test OTA does not require a manual restart.

A0 is pulled up by a 3.9k resistor.
One side of each jack is attached to A0. The other end is attached to a digital pin.
  Slight kink : 'A0' is actually one end of a 2.2k/1k potential divider. 
Each key is a 1/4" mono jack with an E12 resistor in it.
Digital outputs are left floating.
In sequence, each digital pin is pulled to 0v and A0 is sampled. 10bit result is sent to serial for debugging.

