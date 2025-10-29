# Home Remote
## Description
I wrote this app as an experiemnt, to practice C and learn the how to program for the Flipper Zero.
It is very custom and as is I don't expect it to be very useful to other people, but maybe others will find the code useful for their own projects.

An app that might be useful to others that I started as a spin-off of this app is [BT Home Remote](https://github.com/EmmerichFrog/bt_home_remote): that app will work for other setups too and I intend to publish it to the Flipper App Store

This app allows me to control an e-paper picture frame I built and some of my Home Assistant automations from the Flipper Zero.
The project is composed of:
- This Flipper App;
- A fork of jblanked FlipperHTTP Esp32 firmware, mostly to add the ability to automatically shutdown and turn on the Esp32 when the app is launched;
- A python flask app [https://github.com/EmmerichFrog/ha_proxy], to act as a proxy between Home Assistant and the Flipper. This is not strictly necessary, but greatly simplifies json handling since the json response from HA is quite big for my setup (around 55kB); (needed for wifi backend)
- An ESP32 firmware [https://github.com/EmmerichFrog/sghz_connector/tree/main] to receive data from HA and send it via Subghz; (needed for the sghz backend)
- A python app to send the data from HA via BT serial (repo coming soon).

Also the code for the e-paper picture frame can be found in my other repository.

## Screenshot

TBD