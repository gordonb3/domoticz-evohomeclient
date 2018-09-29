# Domoticz-Evohomeclient

An implementation of the C++ hack to the Honeywell Evohome API

## Description

This project takes a lot of its ideas from watchforstock's Python based evohomeclient and an implementation of this client as posted on the Domoticz wiki.

Reason for redeveloping this in C++ is that I run Domoticz on a rather tiny machine and Python is quite heavy 
on resources. And slow. Of course, if you like the libs you can use them for any other project.

## Implementation

With the completion of the Honeywell Evohome API library this project now also features a full blown Evohome client for Domoticz, offering the same functionality as the python scripts on the Domoticz wiki and more. And most important: with a significant increased performance.

To build the client yourself simply run ` make ` from the project main folder. You can change various default values by making use of the EXTRAFLAGS parameter, e.g.

    make EXTRAFLAGS=" -DCONF_FILE=\\\"evoconfig\\\" -DSCHEDULE_CACHE=\\\"evohome-schedules.json\\\" -DLOCK_FILE=\\\"/tmp/evo-lastup\\\" "

Lookin messy? By mixing single quotes and double quotes you can make this more readable

    make EXTRAFLAGS=' -DCONF_FILE=\"evoconfig\" -DSCHEDULE_CACHE=\"evohome-schedules.json\" -DLOCK_FILE=\"/tmp/evo-lastup\" '

These are actually the default values if you are not on Windows. As of version 1.1.0.6 file locations that do not start with a root (` / `) reference ( windows: ` <drive>: ` ) are relative to the application rather than whatever may be the current directory.


## Prerequisites for building the Evohome client for Domoticz yourself

A computer running Domoticz of course! If you compile your own beta versions of Domoticz you should be all set already for building this client. If not, you'll need a C++ compiler and supporting developer library curl with ssl support. For Debian based systems such as Raspbian this should get you going:

    apt-get install build-essential
    apt-get install libcurl4-openssl-dev


## Running the client

You need to fill in some details in evoconfig. A sample file with instructions is included in the source and binary archives.

1. update usr and pw with your username and password credentials from the evohome web portal http://www.mytotalconnect.com.
1. update srt with the complete - absolute - path to the client application<br>!!! do not use spaces in this path
1. update url and port if applicable (e.g. different port, different server, different webroot)

Run ` evo-client --init ` to add your evohome installation to Domoticz (if you migrated from the python scripts you can skip this step). The client's default action, i.e. when run without parameters, is to update Domoticz with the current status values of your evohome installation. Note that evohome schedules are cached, so if you change those on the controller you will need to refresh them in the client to have Domoticz show the correct until time.<br>

Run ` evo-client --help ` to view more options.

## Feedback Welcome!

If you have any problems, questions or comments regarding this project, feel free to contact me! (gordon@bosvangennip.nl)

