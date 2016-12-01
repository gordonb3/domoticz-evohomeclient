/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Demo app for connecting to Evohome and Domoticz
 *
 *
 *
 */

#include "../lib/domoticzclient.h"
#include "../lib/evohomeclient.h"
#include "../lib/evohomeclientv2.h"



bool read_evoconfig();
std::string get_domoticz_host(std::string url, std::string port);
std::map<std::string,std::string> evo_get_zone_data(evo_zone zone);
std::string get_next_switchpoint(evo_temperatureControlSystem* tcs, int zoneId);
std::string int_to_string(int myint);

