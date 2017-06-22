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
#include "../domoticzclient/domoticzclient.h"
#include "../evohomeclient/evohomeclient.h"
#include "../evohomeclient/evohomeoldclient.h"

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
#include "evo-common.cpp"



std::string backupfile;

time_t now;
int tzoffset=-1;


/*
 * Create an associative array with the zone information we need
 */
std::map<std::string,std::string> evo_get_zone_data(EvohomeClient::zone *zone)
{
	map<std::string,std::string> ret;


	ret["zoneId"] = (*zone->status)["zoneId"].asString();
	ret["name"] = (*zone->status)["name"].asString();
	ret["temperature"] = (*zone->status)["temperatureStatus"]["temperature"].asString();
	ret["setpointMode"] = (*zone->status)["heatSetpointStatus"]["setpointMode"].asString();
	ret["targetTemperature"] = (*zone->status)["heatSetpointStatus"]["targetTemperature"].asString();
	ret["until"] = (*zone->status)["heatSetpointStatus"]["until"].asString();

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

// set defaults
	evoconfig["hwname"] = "evohome";

// override defaults with settings from config file
	read_evoconfig();

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
/*
	if ( ! eclient.read_schedules_from_file(SCHEDULE_CACHE) )
	{
		if ( ! eclient.schedules_backup(SCHEDULE_CACHE) )
			exit_error(ERROR+"failed to open schedule cache file '"+SCHEDULE_CACHE+"'");
		eclient.read_schedules_from_file(SCHEDULE_CACHE);
	}
*/
//eclient.read_schedules_from_file(SCHEDULE_CACHE);

// start demo output
	cout << "\nModel Type = " << (*tcs->installationInfo)["modelType"] << endl;
	cout << "System ID = " << (*tcs->installationInfo)["systemId"] << endl;
	cout << "System mode = " << (*tcs->status)["systemModeStatus"]["mode"] << endl;

	cout << endl;

	std::string lastzone = "";
	cout << "  ID       temp      mode           setpoint      until                name\n";
	for (std::vector<EvohomeClient::zone>::size_type i = 0; i < tcs->zones.size(); ++i)
	{
		std::map<std::string,std::string> zone = evo_get_zone_data(&tcs->zones[i]);
		if (zone["until"].length() == 0)
			zone["until"] = eclient.get_next_switchpoint(zone["zoneId"]);
		else
//		if (zone["until"].length() > 0)

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

		cout << zone["zoneId"];
		cout << " => " << zone["temperature"];
		cout << " => " << zone["setpointMode"];
		cout << " => " << zone["targetTemperature"];
		cout << " => " << zone["until"];
		cout << " => " << zone["name"];
		cout << endl;

		lastzone = zone["zoneId"];
	}

	cout << endl;


	EvohomeClient::zone* myzone = eclient.get_zone_by_ID(lastzone);
	cout << "\nDump of installationinfo for zone" << lastzone << "\n";
	cout << (*myzone->installationInfo).toStyledString() << "\n";
	cout << "\nDump of statusinfo for zone" << lastzone << "\n";
	cout << (*myzone->status).toStyledString() << "\n";


	cout << "\nDump of full installationinfo\n";
	cout << eclient.j_fi.toStyledString() << "\n";

	eclient.cleanup();

	return 0;
}

