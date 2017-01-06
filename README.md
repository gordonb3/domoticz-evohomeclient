# Evohomeclient

C++ hack to the Honeywell Evohome API

## Description

This project takes a lot of its ideas from watchforstock's Python based evohomeclient and an implementation of this client as posted on the Domoticz wiki.

Reason for redeveloping this in C++ is that I run Domoticz on a rather tiny machine and Python is quite heavy 
on resources. And slow. Of course, if you like the lib you can use it for any other project.

## Demo

There are several (simple) demos included in this project that show how the library can be used. Feel free to use whatever you like from these sources to build your own project using this library. Run ` make demo ` to build en check out these demos.

## Implementation

With the completion of the Honeywell Evohome API library this project now also features a full blown Evohome client for Domoticz, offering the same functionality as the python scripts on the Domoticz wiki and more. And most important: with a significant increased performance.

To build the client yourself simply run ` make ` from the project main folder. You can change various default values by making use of the EXTRAFLAGS parameter, e.g.

    make EXTRAFLAGS=" -DCONF_FILE=\\\"evoconfig\\\" -DSCHEDULE_CACHE=\\\"evohome-schedules.json\\\" -DLOCK_FILE=\\\"/tmp/evo-lastup\\\" "

Lookin messy? By mixing single quotes and double quotes you can make this more readable

    make EXTRAFLAGS=' -DCONF_FILE=\"evoconfig\" -DSCHEDULE_CACHE=\"evohome-schedules.json\" -DLOCK_FILE=\"/tmp/evo-lastup\" '

These are actually the default values if you are not on Windows. As of version 1.1.0.6 file locations that do not start with a root (` / `) reference ( windows: ` <drive>: ` ) are relative to the application rather than whatever may be the current directory.


## Prerequisites for building the Evohome client for Domoticz yourself

A computer running Domoticz of course! If you compile your own beta versions of Domoticz you should be all set already for building this client. If not, you'll need a C++ compiler and supporting developer libraries json-c and curl with ssl support. For Debian based systems such as Raspbian this should get you going:

    apt-get install build-essential
    apt-get install json-c-dev libcurl4-openssl-dev


## Binaries

Scared about building yourself? I'm providing prebuilt binaries for the following systems:

1. [Windows 32bit]( ../../releases/download/1.1.0.6/evo-client-win-x86-1.1.0.6.zip )
1. [Linux 32bit]( ../../releases/download/1.1.0.6/evo-client-linux-x86-1.1.0.6.zip )
1. [Linux 64bit]( ../../releases/download/1.1.0.6/evo-client-linux-x64-1.1.0.6.zip )
1. [Linux armv5te]( ../../releases/download/1.1.0.6/evo-client-linux-x64-1.1.0.6.zip )
(e.g. Excito B3, Sheeva plug, ...)



## Feedback Welcome!

If you have any problems, questions or comments regarding this project, feel free to contact me! (gordon@bosvangennip.nl)

