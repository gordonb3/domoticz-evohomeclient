#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <time.h>

#include "evo.h"
#include "../lib/domoticzclient.h"
#include "../lib/evohomeclient.h"
#include "../lib/webclient.h"


using namespace std;


#define CONF_FILE "evoconfig"




std::map<std::string,std::string> evoconfig;


int tzoffset=-1;


void read_evoconfig()
{
	ifstream myfile (CONF_FILE);
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
	}
}


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


/*
 * Create an associative array with the zone information
 */
std::map<std::string,std::string> evo_get_zone_data(evo_zone zone)
{
	map<std::string,std::string> ret;
	json_object *j_zone = json_tokener_parse(zone.status.c_str());

	json_object_object_foreach(j_zone, key, val)
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


int main()
{
	read_evoconfig();


	DomoticzClient dom = DomoticzClient(get_domoticz_host(evoconfig["url"], evoconfig["port"]));
	dom.get_evo_devices();


	EvohomeClient client = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);
	client.full_installation();
	if ( !	client.get_status(0) )  cout << "status fail" << endl;

	evo_temperatureControlSystem* tcs = &client.locations[0].gateways[0].temperatureControlSystems[0];

	cout << "Model Type = " << client.json_get_val(tcs->content, "modelType") << endl;
	cout << "System ID = " << client.json_get_val(tcs->content, "systemId") << endl << endl;

	cout << "idx    ID         temp    mode              setpoint   until                   name\n";
	for (std::map<int, evo_zone>::iterator it=tcs->zones.begin(); it!=tcs->zones.end(); ++it)
		{

		std::map<std::string,std::string> zone = evo_get_zone_data(it->second);
		if (zone["until"].length() == 0)
			zone["until"] = client.get_next_switchpoint(zone["zoneId"]);
		else
		{
			// this is stupid: overrides are registered against UTC
			time_t now = time(0);
			if (tzoffset == -1)
			{
				struct tm utime;
				gmtime_r(&now,&utime);
				tzoffset = difftime(mktime(&utime),now);
			}
			struct tm ltime;
			localtime_r(&now,&ltime);
			time_t ntime;
			ltime.tm_isdst = -1;
			ltime.tm_hour = atoi(zone["until"].substr(11, 2).c_str());
			ltime.tm_min = atoi(zone["until"].substr(14, 2).c_str());
			ltime.tm_sec = atoi(zone["until"].substr(17, 2).c_str()) - tzoffset;
			ntime = mktime(&ltime);
			ntime--;
			char until[40];
			sprintf(until,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
			zone["until"] = string(until);
		}

		cout << dom.devices[zone["zoneId"]].idx << " => " << zone["zoneId"];
		cout << " => " << zone["temperature"] ;
		cout << " => " << zone["setpointMode"];
		cout << " => " << zone["targetTemperature"];
		cout << " => " << zone["until"];
		cout << " => " << zone["name"];
		cout << endl;
	}

	client.cleanup();

	dom.cleanup();
	return 0;
}

