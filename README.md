# pluto

## Description
Pi Pico2 W applicaiton core functionality.  This repository is indended to be used as submodule. Add it to a project quickly to quickly get a new project up and running. See pluto_hello_world for a working example of how to use this repository.  While you can use this repository directly, it is recommended that you start with https://github.com/NewmanIsTheStar/pluto_hello_world.git 

## Main Components
- Pi Pico SDK
- LwIP IP protocol stack
- FreeRTOS premptive task scheduler
- picoFS file system
  - written specifically for Pi Pico2
  - files are stored in contiguous blocks
  - multiple executables may be stored in the file system at once
    - memory translation is used to allow unmodified executables to run from the file system
    - this is similar to using paritions but much more flexible 
- Web Server
  - Support for configuration of standard features
    - WiFi
    - Time server
    - Syslog
    - MQTT
    - GPIO
  - Support for OTA software upgrades  
- Shell over HTTP provides a command line interface
  - supports simplified verions of many commands familar to Linux users e.g. ls, cp, rm, df, stat, wget
- A framework for adding application configuration
- A framework for adding application tasks
- Watchdog, Stack and Heap monitoring
- BASIC interpeter

## Installation of tools on Ubuntu Linux
```
sudo apt install git build-essential cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```
## Clone and build the "Hello World" example code
```
git clone --recurse-submodules https://github.com/NewmanIsTheStar/pluto_hello_world.git 
cd pluto_hello_world
mkdir build
cd build
cmake ..
make
```
Upon completion of a successful build the file pluto_hello_world.uf2 should be created.  This may be loaded onto the Pico2 W in the usual manner: by dragging and dropping the file onto the Pi Pcio2 W mass storage device.

## Initial Configuration
- The Pico will initially create a WiFi network called **pluto**.  Connect to this WiFi network and then point your web browser to http://192.168.4.1
  - Note that many web browsers automatically change the URL from http:// to https:// so if it is not connecting you might need to reenter the URL.
- Set the WiFi country, network and password then hit save and reboot.  The Pico will attempt to connect to the WiFi network.  If it fails then it will fall back to AP mode and you can once again connect to the pluto network and correct your mistakes.  
- Refer to the [Wiki](https://github.com/NewmanIsTheStar/pluto_hello_world/wiki) for more details

