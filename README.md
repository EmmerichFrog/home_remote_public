# DESCRIPTION
Software written as a learning exercise in C and flipper zero apps. 
Can control an eink picture frame running a flask app and query and send command to home assistant vie ha_proxy [https://github.com/EmmerichFrog/ha_proxy].

# HOW TO USE
The software offers a configuration screen to setup ssids and related data. A json config file is created and loaded from the apps_data folder and a Home Assistant token should be added to the file (KEY "ha_token") to authenticate.

The included fork of FlipperHTTP has increased buffer sizes to handle the payload. Supports putting the wifi devboard to sleep (needs a forked esp32 firmware).

There is probably not much use to this app since it's custom to my setup but maybe it can be found useful by some people.
