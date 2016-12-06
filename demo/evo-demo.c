#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <time.h>

#include "evo-demo.h"
#include "../lib/domoticzclient.h"
#include "../lib/evohomeclient.h"
#include "../lib/webclient.h"

#include "../lib/evohomeclientv2.h"


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
#define LOCKFILE "/var/tmp/evo-noup.tmp"
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
std::map<std::string,std::string> evo_get_zone_data(evo_zone zone)
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

	parse_args(argc, argv);

// set defaults
	evoconfig["hwname"] = "evohome";

// override defaults with settings from config file
	read_evoconfig();

// connect to Domoticz server
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
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);

// retrieve Evohome installation
	eclient.full_installation();

	// set Evohome heating system
	evo_temperatureControlSystem* tcs = NULL;

	if ( evoconfig.find("systemId") != evoconfig.end() ) {
		if (verbose)
			cout << "using systemId from " << CONF_FILE << endl;
 		tcs = eclient.get_temperatureControlSystem_by_ID(evoconfig["systemId"]);
		if (tcs == NULL)
			exit_error(ERROR+"the Evohome systemId specified in "+CONF_FILE+" cannot be found");
	}
	else if (eclient.is_single_heating_system())
		tcs = &eclient.locations[0].gateways[0].temperatureControlSystems[0];
	else
		select_temperatureControlSystem(eclient);

	if (tcs == NULL)
		exit_error(ERROR+"multiple Evohome systems found - don't know which one to use for status");

// retrieve Evohome status
	if ( !	eclient.get_status(tcs->locationId) )  cout << "status fail" << endl;


// retrieving schedules is painfully slow as we can only fetch them one zone at a time.
// luckily schedules do not change very often, so we can use a local cache
	if ( ! eclient.read_schedules_from_file(SCHEDULE_CACHE) )
	{
		if ( ! eclient.schedules_backup(SCHEDULE_CACHE) )
			exit_error(ERROR+"failed to open schedule cache file '"+SCHEDULE_CACHE+"'");
		eclient.read_schedules_from_file(SCHEDULE_CACHE);
	}



// start demo output
	cout << "Model Type = " << eclient.json_get_val(tcs->installationInfo, "modelType") << endl;
	cout << "System ID = " << eclient.json_get_val(tcs->installationInfo, "systemId") << endl;
	cout << "System mode = " << eclient.json_get_val(tcs->status, "systemModeStatus", "mode") << endl;

	cout << endl;

	int newzone = 0;
	cout << "idx    ID         temp    mode              setpoint   until                   name\n";
	for (std::map<int, evo_zone>::iterator it=tcs->zones.begin(); it!=tcs->zones.end(); ++it)
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

	eclient.cleanup();
	dclient.cleanup();


// Evohome v2 API
//	EvohomeClientV2 ev2client = EvohomeClientV2(evoconfig["usr"],evoconfig["pw"]);
//	ev2client.full_installation();
//	ev2client.cleanup();

	return 0;
}

