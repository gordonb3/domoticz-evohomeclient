/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Demo app for connecting to Evohome and Domoticz
 *
 *
 *
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <time.h>
#include "../lib/domoticzclient.h"
#include "../lib/evohomeclient.h"
#include "../lib/evohomeoldclient.h"

#ifndef CONF_FILE
#define CONF_FILE "evoconfig"
#endif

#ifndef BACKUP_FILE
#define BACKUP_FILE "schedules.backup"
#endif

#ifndef SCHEDULE_CACHE
#define SCHEDULE_CACHE "schedules.json"
#endif

#ifndef LOCKFILE
#define LOCKFILE "/tmp/evo-noup.tmp"
#endif

#define HARDWARE_TYPE "40"

#define CONTROLLER_SUBTYPE "Evohome"
#define CONTROLLER_SUBTYPE_ID "69"

#define HOTWATER_SUBTYPE "Hot Water"
#define HOTWATER_SUBTYPE_ID "71"

#define ZONE_SUBTYPE "Zone"
#define ZONE_SUBTYPE_ID "70"


using namespace std;

// Include common functions
#include "evo-common.c"



std::string backupfile;

time_t now;
int tzoffset=-1;

bool createdev = false;
bool updatedev = true;



/*
 * Convert domoticz host settings into fully qualified url prefix
 */
std::string get_domoticz_host(std::string url, std::string port)
{
	stringstream ss;
	if (url.substr(0,4) != "http")
		ss << "http://";
	ss << url;
	if (port.length() > 0)
		ss << ":" << port;
	return ss.str();
}


void usage(std::string mode)
{
	cout << "Usage: " << mode << endl;
}


void parse_args(int argc, char** argv) {
	int i=1;
	std::string word;
	while (i < argc) {
		word = argv[i];
		if (word.length() > 1 && word[0] == '-' && word[1] != '-') {
			for (size_t j=1;j<word.length();j++) {
				if (word[j] == 'h') {
					usage("short");
					exit(0);
				} else if (word[j] == 'i') {
					createdev = true;
				} else if (word[j] == 'v') {
					verbose = true;
				} else {
					usage("badparm");
					exit(1);
				}
			}
		} else if (word == "--help") {
			usage("long");
			exit(0);
		} else if (word == "--init") {
			createdev = true;
		} else if (word == "--verbose") {
			verbose = true;
		} else {
			usage("badparm");
			exit(1);
		}
		i++;
	}
}


/*
 * Create an associative array with the zone information we need
 */
std::map<std::string,std::string> evo_get_zone_data(EvohomeClient::zone zone)
{
	map<std::string,std::string> ret;

	json_object_object_foreach(zone.status, key, val)
	{
		if ( (strcmp(key, "zoneId") == 0) || (strcmp(key, "name") == 0) )
		{
			ret[key] = json_object_get_string(val);
		}
		else if ( (strcmp(key, "temperatureStatus") == 0) || (strcmp(key, "heatSetpointStatus") == 0) )
		{
			json_object_object_foreach(val, key2, val2)
			{
				ret[key2] = json_object_get_string(val2);
			}
		}
	}
	return ret;
}




std::string int_to_string(int myint)
{
	stringstream ss;
	ss << myint;
	return ss.str();
}


int main(int argc, char** argv)
{
// get current time
	now = time(0);

	backupfile = BACKUP_FILE;
	configfile = CONF_FILE;
	parse_args(argc, argv);

// set defaults
	evoconfig["hwname"] = "evohome";

// override defaults with settings from config file
	read_evoconfig();

cout << "connect to Domoticz server\n";
	DomoticzClient dclient = DomoticzClient(get_domoticz_host(evoconfig["url"], evoconfig["port"]));

	int hwid = dclient.get_hwid(HARDWARE_TYPE, evoconfig["hwname"]);

	if (createdev)
	{
		if (hwid >= 0)
			cout << "WARNING: Hardware device " << evoconfig["hwname"] << " already exists\n";
		else
			hwid = dclient.create_hardware(HARDWARE_TYPE, evoconfig["hwname"]);
	}

	if (hwid == -1)
		exit_error(ERROR+"evohome hardware not found");

	dclient.get_devices(hwid);

	if (createdev)
	{
		int controllers = 0;
		int dhws = 0;
		int zones = 0;
		for (std::map<std::string, domoticz_device>::iterator it=dclient.devices.begin(); it!=dclient.devices.end(); ++it)
		{
			if (it->second.SubType == CONTROLLER_SUBTYPE)
				controllers++;
			else if (it->second.SubType == HOTWATER_SUBTYPE)
				dhws++;
			else if (it->second.SubType == ZONE_SUBTYPE)
				zones++;
			else
				cout << "WARNING: got device with unknown SubType '" << it->second.SubType << "'\n";
		}

		if (controllers == 0)
			dclient.create_evohome_device(hwid, CONTROLLER_SUBTYPE_ID);
		if (dhws == 0)
			dclient.create_evohome_device(hwid, HOTWATER_SUBTYPE_ID);
		int i;
		for (i = zones; i < 12; i++)
			dclient.create_evohome_device(hwid, ZONE_SUBTYPE_ID);

		if ( (controllers == 0) || (dhws == 0) || (zones < 12) )
			dclient.get_devices(hwid);
	}
	



// connect to Evohome server
cout << "connect to Evohome server\n";
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);
cout << "connected" << endl;

// retrieve Evohome installation
cout << "retrieve Evohome installation\n";
	eclient.full_installation();

// set Evohome heating system
	int location = 0;
	int gateway = 0;
	int temperatureControlSystem = 0;

	if ( evoconfig.find("locationId") != evoconfig.end() ) {
		while ( (eclient.locations[location].locationId != evoconfig["locationId"])  && (location < (int)eclient.locations.size()) )
			location++;
		if (location == (int)eclient.locations.size())
			exit_error(ERROR+"the Evohome location ID specified in "+CONF_FILE+" cannot be found");
	}
	if ( evoconfig.find("gatewayId") != evoconfig.end() ) {
		while ( (eclient.locations[location].gateways[gateway].gatewayId != evoconfig["gatewayId"])  && (gateway < (int)eclient.locations[location].gateways.size()) )
			gateway++;
		if (gateway == (int)eclient.locations[location].gateways.size())
			exit_error(ERROR+"the Evohome gateway ID specified in "+CONF_FILE+" cannot be found");
	}
	if ( evoconfig.find("systemId") != evoconfig.end() ) {
		while ( (eclient.locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].systemId != evoconfig["systemId"])  && (temperatureControlSystem < (int)eclient.locations[location].gateways[gateway].temperatureControlSystems.size()) )
			temperatureControlSystem++;
		if (temperatureControlSystem == (int)eclient.locations[location].gateways[gateway].temperatureControlSystems.size())
			exit_error(ERROR+"the Evohome system ID specified in "+CONF_FILE+" cannot be found");
	}
	EvohomeClient::temperatureControlSystem* tcs = &eclient.locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];


// retrieve Evohome status
	if ( !	eclient.get_status(location) )  cout << "status fail" << endl;




// retrieving schedules is painfully slow as we can only fetch them one zone at a time.
// luckily schedules do not change very often, so we can use a local cache
	if ( ! eclient.read_schedules_from_file(SCHEDULE_CACHE) )
	{
		if ( ! eclient.schedules_backup(SCHEDULE_CACHE) )
			exit_error(ERROR+"failed to open schedule cache file '"+SCHEDULE_CACHE+"'");
		eclient.read_schedules_from_file(SCHEDULE_CACHE);
	}



// start demo output
	cout << "\nModel Type = " << eclient.json_get_val(tcs->installationInfo, "modelType") << endl;
	cout << "System ID = " << eclient.json_get_val(tcs->installationInfo, "systemId") << endl;
	cout << "System mode = " << eclient.json_get_val(tcs->status, "systemModeStatus", "mode") << endl;

	cout << endl;

	int newzone = 0;
	cout << "idx    ID         temp    mode              setpoint   until                   name\n";
	for (std::map<int, EvohomeClient::zone>::iterator it=tcs->zones.begin(); it!=tcs->zones.end(); ++it)
	{
		std::map<std::string,std::string> zone = evo_get_zone_data(it->second);
		if (zone["until"].length() == 0)
			zone["until"] = eclient.get_next_switchpoint(tcs, it->first);
		else
		{
			// this is stupid: Honeywell is mixing UTC and localtime in their returns
			// for display we need to convert overrides to localtime
			if (tzoffset == -1)
			{
				// calculate timezone offset once
				struct tm utime;
				gmtime_r(&now, &utime);
				tzoffset = difftime(mktime(&utime), now);
			}
			struct tm ltime;
			localtime_r(&now, &ltime);
			ltime.tm_isdst = -1;
			ltime.tm_hour = atoi(zone["until"].substr(11, 2).c_str());
			ltime.tm_min = atoi(zone["until"].substr(14, 2).c_str());
			ltime.tm_sec = atoi(zone["until"].substr(17, 2).c_str()) - tzoffset;
			time_t ntime = mktime(&ltime);
			ntime--; // prevent compiler warning
			char until[40];
			sprintf(until,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
			zone["until"] = string(until);
		}

		if (dclient.devices.find(zone["zoneId"]) == dclient.devices.end())
		{
			while ( (dclient.devices.find(int_to_string(newzone)) != dclient.devices.end()) && (dclient.devices[int_to_string(newzone)].SubType != "Zone") )
				newzone++;
			if (dclient.devices.find(int_to_string(newzone)) == dclient.devices.end())
				cerr << "WARNING: Can't register new Evohome zone because you have no free zones available for this hardware in Domoticz\n";
			else
				cout << dclient.devices[int_to_string(newzone)].idx << "* => " << zone["zoneId"];
			newzone++;
		}
		else
			cout << dclient.devices[zone["zoneId"]].idx << "  => " << zone["zoneId"];
		cout << " => " << zone["temperature"] ;
		cout << " => " << zone["setpointMode"];
		cout << " => " << zone["targetTemperature"];
		cout << " => " << zone["until"];
		cout << " => " << zone["name"];
		cout << endl;
	}

	cout << endl;


//cout << json_object_to_json_string_ext(eclient.get_zone_by_ID("1605795")->schedule,JSON_C_TO_STRING_PRETTY) << endl;

//eclient._get_zone_by_ID("1605795");



cout << eclient.get_next_switchpoint("1605795") << endl;

cout << eclient.get_next_switchpoint(tcs, 0) << endl;

eclient.set_temperature("1605795", "16.2", "2016-12-04T17:00:00Z");



cout << "SystemId = " << eclient.get_zone_temperatureControlSystem(eclient.get_zone_by_ID("1605795"))->systemId << endl;

	eclient.cleanup();
	dclient.cleanup();



/*
// Evohome v2 API
	EvohomeClientV2 ev2client = EvohomeClientV2(evoconfig["usr"],evoconfig["pw"]);
	ev2client.full_installation();
	ev2client.cleanup();
*/
	return 0;
}

