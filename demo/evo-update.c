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
#include <stdlib.h>
#include "../lib/domoticzclient.h"
#include "../lib/evohomeclient.h"
#include "../lib/evohomeclientv2.h"


#ifndef CONF_FILE
#define CONF_FILE "evoconfig"
#endif

#ifndef SCHEDULE_CACHE
#define SCHEDULE_CACHE "schedules.json"
#endif

#ifndef LOCKFILE
#define LOCKFILE "/var/tmp/evo-noup.tmp"
#endif

#ifndef LOCKSECONDS
#define LOCKSECONDS 60
#endif

#define SETMODE_SCRIPT "/evo-setmode.sh {status}"
#define SETDHW_SCRIPT "/evo-setdhw.sh {deviceid} {mode} {state} {until}"
#define SETTEMP_SCRIPT "/evo-settemp.sh {deviceid} {mode} {setpoint} {until}"

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
int tzoffset;

bool createdev = false;
bool updatedev = true;
bool reloadcache = false;



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
	if (mode == "badparm")
	{
		cout << "Bad parameter" << endl;
		exit(1);
	}
	if (mode == "short")
	{
		cout << "Usage: evo-update [-hiuv] [-c file]" << endl;
		cout << "Type \"evo-update --help\" for more help" << endl;
		exit(0);
	}
	cout << "Usage: evo-update [OPTIONS]" << endl;
	cout << endl;
	cout << "  -i, --init              create Evohome devices in Domoticz" << endl;
	cout << "  -k, --keepdevs          do not update device settings - only status" << endl;
	cout << "  -u, --utc               assume all times are UTC" << endl;
	cout << "  -v, --verbose           print a lot of information" << endl;
	cout << "  -c, --conf=FILE         use FILE for server settings and credentials" << endl;
	cout << "      --reload-cache      reload the schedules cache from the Evohome server" << endl;
	cout << "  -h, --help              display this help and exit" << endl;
	exit(0);
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
				} else if (word[j] == 'i') {
					createdev = true;
				} else if (word[j] == 'k') {
					updatedev = false;
				} else if (word[j] == 'u') {
					tzoffset = 0;
				} else if (word[j] == 'v') {
					verbose = true;
				} else if (word[j] == 'c') {
					if (j+1 < word.length())
						usage("badparm");
					i++;
					configfile = argv[i];
				} else {
					usage("badparm");
				}
			}
		} else if (word == "--help") {
			usage("long");
			exit(0);
		} else if (word == "--init") {
			createdev = true;
		} else if (word == "--keepdevs") {
			updatedev = false;
		} else if (word == "--utc") {
			tzoffset = 0;
		} else if (word == "--verbose") {
			verbose = true;
		} else if (word.substr(0,7) == "--conf=") {
			configfile = word.substr(7);
		} else if (word == "--reload-cache") {
			reloadcache = true;
		} else {
			usage("badparm");
		}
		i++;
	}
	if (createdev)
		updatedev = true;
}


map<std::string, std::string> evo_get_system_data(EvohomeClient::temperatureControlSystem* tcs)
{
	map<std::string, std::string> ret;
	json_object *j_tmp, *j_res;
	if ( ( json_object_object_get_ex(tcs->status, "systemModeStatus", &j_tmp)) && (json_object_object_get_ex( j_tmp, "mode", &j_res)) )
		ret["systemMode"] = json_object_get_string(j_res);
	ret["systemId"] = tcs->systemId;
	if (updatedev)
	{
		if (json_object_object_get_ex(tcs->installationInfo, "modelType", &j_res))
			ret["modelType"] = json_object_get_string(j_res);
	}
	return ret;
}


map<std::string, std::string> evo_get_dhw_data(EvohomeClient::temperatureControlSystem* tcs)
{
	map<std::string, std::string> ret;
	json_object *j_dhw, *j_tmp, *j_res;
	if (json_object_object_get_ex(tcs->status, "dhw", &j_dhw))
	{
		ret["until"] = "";
		if (json_object_object_get_ex(j_dhw, "dhwId", &j_res))
			ret["dhwId"] = json_object_get_string(j_res);
		if ( (json_object_object_get_ex(tcs->status, "temperatureStatus", &j_tmp)) && (json_object_object_get_ex( j_tmp, "temperature", &j_res)) )
			ret["temperature"] = json_object_get_string(j_res);
		if ( json_object_object_get_ex(tcs->status, "stateStatus", &j_tmp))
		{
			if (json_object_object_get_ex(j_tmp, "state", &j_res))
				ret["state"] = json_object_get_string(j_res);
			if (json_object_object_get_ex(j_tmp, "mode", &j_res))
				ret["mode"] = json_object_get_string(j_res);
			if ( (ret["mode"] == "TemporaryOverride") && (json_object_object_get_ex(j_tmp, "until", &j_res)) )
				ret["until"] = json_object_get_string(j_res);
		}
	}
	return ret;
}


map<std::string, std::string> evo_get_zone_data(EvohomeClient::temperatureControlSystem* tcs, int zoneindex)
{
	map<std::string, std::string> ret;

	json_object_object_foreach(tcs->zones[zoneindex].status, key, val)
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


std::string local_to_utc(std::string utc_time)
{
	if (tzoffset == -1)
	{
		// calculate timezone offset once
		struct tm utime;
		gmtime_r(&now, &utime);
		tzoffset = difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
	ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
	ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
	ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str()) + tzoffset;
	time_t ntime = mktime(&ltime);
	ntime--; // prevent compiler warning
	char until[40];
	sprintf(until,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
	return string(until);
}


/*
std::string utc_to_local(std::string utc_time)
{
	if (tzoffset == -1)
	{
		// calculate timezone offset once
		struct tm utime;
		gmtime_r(&now, &utime);
		tzoffset = difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
	ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
	ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
	ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str()) - tzoffset;
	time_t ntime = mktime(&ltime);
	ntime--; // prevent compiler warning
	char until[40];
	sprintf(until,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
	return string(until);
}
*/


// Get Evohome hardware ID from Domoticz
int get_evohome_hardwareId(DomoticzClient &dclient)
{
	int hwid = dclient.get_hwid(HARDWARE_TYPE, evoconfig["hwname"]);
	if (verbose)
		cout << "got ID '" << hwid << "' for Evohome hardware with name '" << evoconfig["hwname"] << "'\n";
	if (createdev)
	{
		if (verbose)
			cout << "init mode enabled\ncreate hardware in Domoticz\n";
		if (hwid >= 0)
			cout << "WARNING: hardware device " << evoconfig["hwname"] << " already exists\n";
		else
			hwid = dclient.create_hardware(HARDWARE_TYPE, evoconfig["hwname"]);
	}

	if (hwid == -1)
		exit_error(ERROR+"evohome hardware not found");
	return hwid;
}


// get Evohome devices from Domoticz
void get_evohome_devices(DomoticzClient &dclient, int hwid)
{
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
		{
			if (verbose)
				cout << "create evohome controller in Domoticz\n";
			dclient.create_evohome_device(hwid, CONTROLLER_SUBTYPE_ID);
		}
		if (dhws == 0)
		{
			if (verbose)
				cout << "create hot water device in Domoticz\n";
			dclient.create_evohome_device(hwid, HOTWATER_SUBTYPE_ID);
		}
		int i;
		for (i = zones; i < 12; i++)
		{
			if (verbose)
				cout << "create heating device in Domoticz\n";
			dclient.create_evohome_device(hwid, ZONE_SUBTYPE_ID);
		}

		if ( (controllers == 0) || (dhws == 0) || (zones < 12) )
			dclient.get_devices(hwid);
	}
}


void update_system(DomoticzClient &dclient, map<std::string,std::string> systemdata)
{
	if (verbose)
		cout << " - change Evohome system status to '" << systemdata["systemMode"] << "'\n";
	dclient.update_system_mode(dclient.devices[systemdata["systemId"]].idx, systemdata["systemMode"]);
	if (updatedev)
	{
		stringstream sms;
		sms << evoconfig["srt"] << SETMODE_SCRIPT;
		if (verbose)
			cout << " - change Evohome system name to '" << systemdata["modelType"] << "'\n - change setmode script path to '" << sms.str() << "'\n";
		if (dclient.devices.find(systemdata["systemId"]) == dclient.devices.end())
		dclient.update_system_dev(dclient.devices[systemdata["systemId"]].idx, systemdata["systemId"], systemdata["modelType"], sms.str());
	}
}


void update_dhw(DomoticzClient &dclient, map<std::string,std::string> dhwdata)
{
	std::string idx;
	if ( updatedev && (dclient.devices.find(dhwdata["dhwId"]) == dclient.devices.end()) )
	{
		int newzone = 0;
		std::string s_newzone = int_to_string(newzone);
		while ( (dclient.devices.find(s_newzone) != dclient.devices.end()) && (dclient.devices[s_newzone].SubType != "domesticHotWater") )
		{
			newzone++;
			s_newzone = int_to_string(newzone);
		}
		if (dclient.devices.find(int_to_string(newzone)) == dclient.devices.end())
			cerr << "WARNING: can't register new Hot Water device because you have no free zones available for this hardware in Domoticz\n";
		else
			idx = dclient.devices[s_newzone].idx;
		newzone++;
	}
	else
		idx = dclient.devices[dhwdata["dhwId"]].idx;
	if (verbose)
		cout << " - change status of Hot Water device: temperature = " << dhwdata["temperature"] << ", state = " << dhwdata["state"] << ", mode = " << dhwdata["mode"] << ", until = " << dhwdata["until"] << endl;
	dclient.update_zone_status(idx, dhwdata["temperature"], dhwdata["state"], dhwdata["mode"], dhwdata["until"]);
	if (updatedev)
	{
		stringstream sms;
		sms << evoconfig["srt"] << SETDHW_SCRIPT;
		if (verbose)
			cout << " - change hotwater script path to '" << sms.str() << "'\n";
		dclient.update_zone_dev(idx, dhwdata["dhwId"], "Hot Water", sms.str());
	}
}


void update_zone(DomoticzClient &dclient, map<std::string,std::string> zonedata)
{
	std::string idx;
	if ( updatedev && (dclient.devices.find(zonedata["zoneId"]) == dclient.devices.end()) )
	{
		int newzone = 0;
		std::string s_newzone = int_to_string(newzone);
		while ( (dclient.devices.find(s_newzone) != dclient.devices.end()) && (dclient.devices[s_newzone].SubType != "Zone") )
		{
			newzone++;
			s_newzone = int_to_string(newzone);
		}
		if (dclient.devices.find(int_to_string(newzone)) == dclient.devices.end())
			cerr << "WARNING: can't register new Evohome zone because you have no free zones available for this hardware in Domoticz\n";
		else
			idx = dclient.devices[s_newzone].idx;
		newzone++;
	}
	else
		idx = dclient.devices[zonedata["zoneId"]].idx;
	if (updatedev)
	{
		stringstream sms;
		sms << evoconfig["srt"] << SETTEMP_SCRIPT;
		if (verbose)
			cout << " - set name of zone device " << idx << " to '" << zonedata["name"] << "'\n - change zone script path to '" << sms.str() << "'\n";
		dclient.update_zone_dev(idx, zonedata["zoneId"], zonedata["name"], sms.str());
	}
	if (verbose)
		cout << " - change status of zone " << zonedata["name"] << ": temperature = " << zonedata["temperature"] << ", setpoint = " << zonedata["targetTemperature"] << ", mode = " << zonedata["setpointMode"] << ", until = " << zonedata["until"] << endl;
	dclient.update_zone_status(idx, zonedata["temperature"], zonedata["targetTemperature"], zonedata["setpointMode"], zonedata["until"]);
}





int main(int argc, char** argv)
{
	// get current time
	now = time(0);

	check_lock_status();

	// set defaults
	evoconfig["hwname"] = "evohome";
	configfile = CONF_FILE;
	tzoffset = -1;
	parse_args(argc, argv);
	if ( ! read_evoconfig() )
		exit_error(ERROR+"can't read config file");

	// connect to Domoticz server
	if (verbose)
		cout << "connect to Domoticz server\n";
	DomoticzClient dclient = DomoticzClient(get_domoticz_host(evoconfig["url"], evoconfig["port"]));

	// Get Evohome hardware ID from Domoticz
	int hwid = get_evohome_hardwareId(dclient);

	// get Evohome devices from Domoticz
	get_evohome_devices(dclient, hwid);


	// connect to Evohome server
	if (verbose)
		cout << "connect to Evohome server\n";
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);

	// get Evohome installation info
	if (verbose)
		cout << "retrieve Evohome installation info\n";
	eclient.full_installation();

	// get Evohome heating system
	EvohomeClient::temperatureControlSystem* tcs = NULL;
	if ( evoconfig.find("systemId") != evoconfig.end() )
	{
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


	// get status for Evohome heating system
	if (verbose)
		cout << "retrieve status of Evohome heating system\n";
	if ( !	eclient.get_status(tcs->locationId) )
		exit_error(ERROR+"failed to retrieve status");


	/* retrieving schedules is painfully slow as we can only fetch them one zone at a time.
	 * luckily schedules do not change very often, so we can use a local cache
	 */
	if ( reloadcache || ( ! eclient.read_schedules_from_file(SCHEDULE_CACHE) ) )
	{
		if (verbose)
			cout << "reloading schedules cache\n";
		if ( ! eclient.schedules_backup(SCHEDULE_CACHE) )
			exit_error(ERROR+"failed to open schedule cache file '"+SCHEDULE_CACHE+"'");
		eclient.read_schedules_from_file(SCHEDULE_CACHE);
	}
	if (verbose)
		cout << "read schedules from cache\n";


	// Update system status
	if (verbose)
		cout << "start write of Evohome data to Domoticz:\n";
	map<std::string, std::string> systemdata = evo_get_system_data(tcs);
	update_system(dclient, systemdata);


	// Update hot water status
	if (eclient.has_dhw(tcs))
	{
		map<std::string, std::string> dhwdata = evo_get_dhw_data(tcs);
		if (dhwdata["until"] == "")
			dhwdata["until"] = local_to_utc(eclient.get_next_switchpoint(tcs, atoi(dhwdata["dhwId"].c_str())));
		update_dhw(dclient, dhwdata);
	}


	// Update zones
	for (std::map<int, EvohomeClient::zone>::iterator it=tcs->zones.begin(); it!=tcs->zones.end(); ++it)
	{
		std::map<std::string, std::string> zonedata = evo_get_zone_data(tcs, it->first);
		if (zonedata["until"].length() == 0)
			zonedata["until"] = local_to_utc(eclient.get_next_switchpoint(tcs, it->first));
		update_zone(dclient, zonedata);
	}

	if (verbose)
		cout << "Done!\n";

	// cleanup
	eclient.cleanup();
	dclient.cleanup();

	return 0;
}

