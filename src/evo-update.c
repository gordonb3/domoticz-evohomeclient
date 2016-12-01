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
#include "evo-update.h"



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
#define SETZONE_SCRIPT "/evo-settemp.sh {deviceid} {mode} {setpoint} {until}"

#define HARDWARE_TYPE "40"

#define CONTROLLER_SUBTYPE "Evohome"
#define CONTROLLER_SUBTYPE_ID "69"

#define HOTWATER_SUBTYPE "Hot Water"
#define HOTWATER_SUBTYPE_ID "71"

#define ZONE_SUBTYPE "Zone"
#define ZONE_SUBTYPE_ID "70"


using namespace std;


std::string configfile;
std::map<std::string,std::string> evoconfig;

time_t now;
int tzoffset;

std::string ERROR = "ERROR: ";
std::string WARN = "WARNING: ";

bool createdev = false;
bool updatedev = true;
bool verbose = false;

/*
 * Read the config file
 */
bool read_evoconfig()
{
	ifstream myfile (configfile.c_str());
	if ( myfile.is_open() )
	{
		stringstream key,val;
		bool isKey = true;
		string line;
		unsigned int i;
		while ( getline(myfile,line) )
		{
			if ( (line[0] == '#') || (line[0] == ';') )
				continue;
			for (i = 0; i < line.length(); i++)
			{
				if ( (line[i] == ' ') || (line[i] == '\'') || (line[i] == '"') || (line[i] == 0x0d) )
					continue;
				if (line[i] == '=')
				{
					isKey = false;
					continue;
				}
				if (isKey)
					key << line[i];
				else
					val << line[i];
			}
			if ( ! isKey )
			{
				string skey = key.str();
				evoconfig[skey] = val.str();
				isKey = true;
				key.str("");
				val.str("");
			}
		}
		myfile.close();
		return true;
	}
	return false;
}


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
		} else {
			usage("badparm");
		}
		i++;
	}
	if (createdev)
		updatedev = true;
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


std::string get_next_switchpoint(evo_temperatureControlSystem* tcs, int zoneId)
{
	const std::string weekdays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
	struct tm ltime;
	localtime_r(&now,&ltime);
	int year = ltime.tm_year;
	int month = ltime.tm_mon;
	int day = ltime.tm_mday;
	std::string stime;
	int sdays = json_object_array_length(tcs->zones[zoneId].schedule);
	int d, i;

	for (d = 0; d < 7; d++)
	{
		int wday = (ltime.tm_wday + d) % 7;
		std::string s_wday = (std::string)weekdays[wday];
		json_object *j_day, *j_dayname;
		for (i = 0; i < sdays; i++)
		{
			j_day = json_object_array_get_idx(tcs->zones[zoneId].schedule, i);
			if ( (json_object_object_get_ex(j_day, "dayOfWeek", &j_dayname)) && (strcmp(json_object_get_string(j_dayname), s_wday.c_str()) == 0) )
				i = sdays;
		}

		json_object *j_list, *j_sp, *j_tim;
		json_object_object_get_ex( j_day, "switchpoints", &j_list);

		int l = json_object_array_length(j_list);
		for (i = 0; i < l; i++)
		{
			j_sp = json_object_array_get_idx(j_list, i);
			json_object_object_get_ex(j_sp, "timeOfDay", &j_tim);
			stime=json_object_get_string(j_tim);
			ltime.tm_isdst = -1;
			ltime.tm_year = year;
			ltime.tm_mon = month;
			ltime.tm_mday = day + d;
			ltime.tm_hour = atoi(stime.substr(0, 2).c_str());
			ltime.tm_min = atoi(stime.substr(3, 2).c_str());
			ltime.tm_sec = atoi(stime.substr(6, 2).c_str());
			time_t ntime = mktime(&ltime);
			if (ntime > now)
			{
				i = l;
				d = 7;
			}
		}
	}
	char rdata[30];
	sprintf(rdata,"%04d-%02d-%02dT%sZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,stime.c_str());
	return string(rdata);
}


void exit_error(std::string message)
{
	cerr << message << endl;
	exit(1);
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
	localtime_r(&now, &ltime);
	ltime.tm_isdst = -1;
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
	localtime_r(&now, &ltime);
	ltime.tm_isdst = -1;
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


int main(int argc, char** argv)
{
// get current time
	now = time(0);

	ifstream myfile (LOCKFILE);
	if ( myfile.is_open() )
	{
		string line;
		getline(myfile,line);
		myfile.close();

		if ( (unsigned long)now < strtoul(line.c_str(),0,10) )
		{
			cout << "Update not allowed at this time - please try again after " << LOCKSECONDS << " seconds\n";
			exit(0);
		}
		else
			if( remove(LOCKFILE) != 0 )
				cout << "Error deleting lockfile\n";
	}

// set defaults
	evoconfig["hwname"] = "evohome";
	configfile = CONF_FILE;
	tzoffset = -1;


	parse_args(argc, argv);

	if ( ! read_evoconfig() )
		exit_error(ERROR+"can't read config file");
		

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
	int location = 0;
	int gateway = 0;
	int temperatureControlSystem = 0;

	if ( evoconfig.find("locationId") != evoconfig.end() ) {
		location = eclient.get_location_by_ID(evoconfig["locationId"]);
		if (location == -1)
			exit_error(ERROR+"the Evohome location ID specified in "+CONF_FILE+" cannot be found");
	}
	if ( evoconfig.find("gatewayId") != evoconfig.end() ) {
		gateway = eclient.get_gateway_by_ID(location, evoconfig["gatewayId"]);
		if (gateway == -1)
			exit_error(ERROR+"the Evohome gateway ID specified in "+CONF_FILE+" cannot be found");
	}
	if ( evoconfig.find("systemId") != evoconfig.end() ) {
		temperatureControlSystem = eclient.get_temperatureControlSystem_by_ID(location, gateway, evoconfig["systemId"]);
		if (temperatureControlSystem == -1)
			exit_error(ERROR+"the Evohome system ID specified in "+CONF_FILE+" cannot be found");
	}
	evo_temperatureControlSystem* tcs = &eclient.locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];

// retrieve Evohome status
	if ( !	eclient.get_status(location) )  cout << "status fail" << endl;

/* retrieving schedules is painfully slow as we can only fetch them one zone at a time.
 * luckily schedules do not change very often, so we can use a local cache
 */
	if ( ! eclient.read_schedules_from_file(SCHEDULE_CACHE) )
	{
		if ( ! eclient.schedules_backup(SCHEDULE_CACHE) )
			exit_error(ERROR+"failed to open schedule cache file '"+SCHEDULE_CACHE+"'");
		eclient.read_schedules_from_file(SCHEDULE_CACHE);
	}

// Update system status
	std::string systemId = eclient.json_get_val(tcs->installationInfo, "systemId");
	if (verbose)
	{
		cout << "Change Evohome system status to '" << eclient.json_get_val(tcs->status, "systemModeStatus", "mode") << "'\n";
	}
	dclient.update_system_mode(dclient.devices[systemId].idx, eclient.json_get_val(tcs->status, "systemModeStatus", "mode"));
	if (updatedev)
	{
		stringstream sms;
		sms << evoconfig["srt"] << SETMODE_SCRIPT;
		if (verbose)
		{
			cout << "Change Evohome system name to '" << eclient.json_get_val(tcs->installationInfo, "modelType") << "'\n";
			cout << "Change setmode script path to '" << sms.str() << "'\n";
		}
		if (dclient.devices.find(systemId) == dclient.devices.end())


		dclient.update_system_dev(dclient.devices[systemId].idx, systemId, eclient.json_get_val(tcs->installationInfo, "modelType"), sms.str());
	}


/* Update hot water status
 * GB3: I hope I got this right - I have no way of checking this because my installation does not have such a device
 */
	if (eclient.has_dhw(tcs))
	{
		json_object *j_dhw;
		json_object_object_get_ex(tcs->status, "dhw", &j_dhw);

		std::string dhw_until;
		std::string dhwId = eclient.json_get_val(j_dhw, "dhwId");
		std::string dhw_temperature = eclient.json_get_val(j_dhw, "temperatureStatus", "temperature");
		std::string dhw_state = eclient.json_get_val(j_dhw, "stateStatus", "state");
		std::string dhw_zonemode = eclient.json_get_val(j_dhw, "stateStatus", "mode");
		if (dhw_zonemode == "TemporaryOverride")
			dhw_until = eclient.json_get_val(j_dhw, "stateStatus", "until");
		else
			dhw_until = local_to_utc(get_next_switchpoint(tcs, atoi(dhwId.c_str())));

		std::string idx;
		if (dclient.devices.find(dhwId) == dclient.devices.end())
		{
			int newzone = 0;
			std::string s_newzone = int_to_string(newzone);
			while ( (dclient.devices.find(s_newzone) != dclient.devices.end()) && (dclient.devices[s_newzone].SubType != "domesticHotWater") )
			{
				newzone++;
				s_newzone = int_to_string(newzone);
			}
			if (dclient.devices.find(int_to_string(newzone)) == dclient.devices.end())
				cerr << "WARNING: Can't register new Hot Water device because you have no free zones available for this hardware in Domoticz\n";
			else
				idx = dclient.devices[s_newzone].idx;
			newzone++;
		}
		else
			idx = dclient.devices[dhwId].idx;


		if (verbose)
		{
			cout << "Change status of Hot Water device: temperature = " << dhw_temperature << ", state = " << dhw_state;
			cout << ", mode = " << dhw_zonemode << ", until = " << dhw_until << endl;
		}
		dclient.update_zone_status(idx, dhw_temperature, dhw_state, dhw_zonemode, dhw_until);

		if (updatedev)
		{
			stringstream sms;
			sms << evoconfig["srt"] << SETDHW_SCRIPT;
			if (verbose)
			{
				cout << "Change hotwater script path to '" << sms.str() << "'\n";
			}
			dclient.update_zone_dev(idx, dhwId, "Hot Water", sms.str());
		}
	}



// Update zones
	for (std::map<int, evo_zone>::iterator it=tcs->zones.begin(); it!=tcs->zones.end(); ++it)
	{
		std::map<std::string,std::string> zone = evo_get_zone_data(it->second);
		if (zone["until"].length() == 0)
			zone["until"] = local_to_utc(get_next_switchpoint(tcs, it->first));
		std::string idx;
		if (dclient.devices.find(zone["zoneId"]) == dclient.devices.end())
		{
			int newzone = 0;
			std::string s_newzone = int_to_string(newzone);
			while ( (dclient.devices.find(s_newzone) != dclient.devices.end()) && (dclient.devices[s_newzone].SubType != "Zone") )
			{
				newzone++;
				s_newzone = int_to_string(newzone);
			}
			if (dclient.devices.find(int_to_string(newzone)) == dclient.devices.end())
				cerr << "WARNING: Can't register new Evohome zone because you have no free zones available for this hardware in Domoticz\n";
			else
				idx = dclient.devices[s_newzone].idx;
			newzone++;
		}
		else
			idx = dclient.devices[zone["zoneId"]].idx;

		if (updatedev)
		{
			stringstream sms;
			sms << evoconfig["srt"] << SETZONE_SCRIPT;
			if (verbose)
			{
				cout << "Set name of zone device " << idx << " to '" << zone["name"] << "'\n";
				cout << "Change zone script path to '" << sms.str() << "'\n";
			}
			dclient.update_zone_dev(idx, zone["zoneId"], zone["name"], sms.str());
		}
		if (verbose)
		{
			cout << "Change status of zone " << zone["name"] << ": temperature = " << zone["temperature"] << ", setpoint = ";
			cout  << zone["targetTemperature"] << ", mode = " << zone["setpointMode"] << ", until = " << zone["until"] << endl;
		}
		dclient.update_zone_status(idx, zone["temperature"], zone["targetTemperature"], zone["setpointMode"], zone["until"]);
	}

	eclient.cleanup();
	dclient.cleanup();

	return 0;
}

