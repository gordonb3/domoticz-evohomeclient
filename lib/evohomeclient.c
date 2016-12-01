/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Evohome
 *
 *
 *
 */



#include <malloc.h>
#include <cstring>
#include <ctime>
#include "webclient.h"
#include "evohomeclient.h"


#define EVOHOME_HOST "https://tccna.honeywell.com"

using namespace std;

const std::string weekdays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const std::string evo_modes[7] = {"Auto", "HeatingOff", "AutoWithEco", "Away", "DayOff", "", "Custom"};

/*
 * Class construct
 */
EvohomeClient::EvohomeClient(std::string user, std::string password)
{
	evoheader = NULL;
	init();
	login(user, password);
}


/************************************************************************
 *									*
 *	Curl helpers							*
 *									*
 ************************************************************************/

/*
 * Initialize curl web client
 */
void EvohomeClient::init()
{
	web_connection_init("evohome");
}


/*
 * Cleanup curl web client
 */
void EvohomeClient::cleanup()
{
	curl_slist_free_all(evoheader);
	web_connection_cleanup("evohome");
}

/*
 * Execute web query
 */
std::string EvohomeClient::send_receive_data(std::string url, curl_slist *header)
{
	return send_receive_data(url, "", header);
}

std::string EvohomeClient::send_receive_data(std::string url, std::string postdata, curl_slist *header)
{

	stringstream ss_url;
	ss_url << EVOHOME_HOST << url;
	return web_send_receive_data("evohome", ss_url.str(), postdata, header);
}

std::string EvohomeClient::put_receive_data(std::string url, std::string postdata, curl_slist *header)
{

	stringstream ss_url;
	ss_url << EVOHOME_HOST << url;
	return web_send_receive_data("evohome", ss_url.str(), postdata, header, "PUT");
}

/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/


/* 
 * login to evohome web server
 */
void EvohomeClient::login(std::string user, std::string password)
{
	auth_info.clear();
	struct curl_slist *lheader = NULL;
	lheader = curl_slist_append(lheader,"Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	lheader = curl_slist_append(lheader,"Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	lheader = curl_slist_append(lheader,"content-type: application/json");
	lheader = curl_slist_append(lheader,"charsets: utf-8");

	stringstream pdata;

	pdata << "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8";
	pdata << "&Host=rs.alarmnet.com%2F";
	pdata << "&Cache-Control=no-store%20no-cache";
	pdata << "&Pragma=no-cache";
	pdata << "&grant_type=password";
	pdata << "&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous%20EMEA-V1-Get-Current-User-Account";
	pdata << "&Username=" << urlencode(user);
	pdata << "&Password=" << urlencode(password);
	pdata << "&Connection=Keep-Alive";

/*
	pdata << "{ ";
	pdata << "'installationInfo-Type': 'application/x-www-form-urlencoded;charset=utf-8'";
	pdata << ", ";
	pdata << "'Host': 'rs.alarmnet.com/'";
	pdata << ", ";
	pdata << "'Cache-Control': 'no-store% o-cache'";
	pdata << ", ";
	pdata << "'Pragma': 'no-cache'";
	pdata << ", ";
	pdata << "'grant_type': 'password'";
	pdata << ", ";
	pdata << "'scope': 'EMEA-V1-Basic MEA-V1-Anonymous EMEA-V1-Get-Current-User-Account'";
	pdata << ", ";
	pdata << "'Username': '" << user << "'";
	pdata << ", ";
	pdata << "'Password': '" << password << "'";
	pdata << ", ";
	pdata << "'Connection': 'Keep-Alive'";
	pdata << " }";
*/
	std::string s_res = send_receive_data("/Auth/OAuth/Token", pdata.str(), lheader);
cout << s_res << endl;
	if (s_res.find("<title>") != std::string::npos)
	{
		cout << "Login to Evohome server failed - server responds: ";
		int i = s_res.find("<title>");
		char* html = &s_res[i];
		i = 7;
		char c = html[i];
		while (c != '<')
		{
			cout << c;
			i++;
			c = html[i];
		}
		cout << endl << endl;
		exit(1);
	}

	json_object *j_ret = json_tokener_parse(s_res.c_str());

	json_object_object_foreach(j_ret, key, val)
	{
		auth_info[key] = json_object_get_string(val);
	}

	stringstream atoken;
	atoken << "Authorization: bearer " << auth_info["access_token"];
	evoheader = curl_slist_append(evoheader,atoken.str().c_str());
	evoheader = curl_slist_append(evoheader,"applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	evoheader = curl_slist_append(evoheader,"accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	evoheader = curl_slist_append(evoheader,"content-type: application/json");
	evoheader = curl_slist_append(evoheader,"charsets: utf-8");

	curl_slist_free_all(lheader);
	user_account();
}


/* 
 * Retrieve evohome user info
 */
void EvohomeClient::user_account()
{
	account_info.clear();
	std::string s_res = send_receive_data("/WebAPI/emea/api/v1/userAccount", evoheader);
	json_object *j_ret = json_tokener_parse(s_res.c_str());
	json_object_object_foreach(j_ret, key, val)
	{
		account_info[key] = json_object_get_string(val);
	}
}


/************************************************************************
 *									*
 *	Evohome heating installations					*
 *									*
 ************************************************************************/

void EvohomeClient::get_zones(int location, int gateway, int temperatureControlSystem)
{
	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones.clear();
	json_object *j_tcs = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].installationInfo;
	json_object *j_list;
	json_object_object_get_ex(j_tcs, "zones", &j_list);
	int l = json_object_array_length(j_list);
	int i;
	for (i=0; i<l; i++)
	{
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].installationInfo = json_object_array_get_idx(j_list, i);

		json_object *j_zoneId;
		json_object_object_get_ex(locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].installationInfo, "zoneId", &j_zoneId);
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].zoneId = json_object_get_string(j_zoneId);
	}
}

void EvohomeClient::get_temperatureControlSystems(int location, int gateway)
{
	locations[location].gateways[gateway].temperatureControlSystems.clear();
	json_object *j_gw = locations[location].gateways[gateway].installationInfo;

	json_object *j_list;
	if ( json_object_object_get_ex(j_gw, "temperatureControlSystems", &j_list) )
	{
		int l = json_object_array_length(j_list);
		int i;
		for (i = 0; i < l; i++)
		{
			locations[location].gateways[gateway].temperatureControlSystems[i].installationInfo = json_object_array_get_idx(j_list, i);

			json_object *j_tcsId;
			json_object_object_get_ex(locations[location].gateways[gateway].temperatureControlSystems[i].installationInfo, "systemId", &j_tcsId);
			locations[location].gateways[gateway].temperatureControlSystems[i].systemId = json_object_get_string(j_tcsId);

			get_zones(location, gateway, i);
		}
	}
}


void EvohomeClient::get_gateways(int location)
{
	locations[location].gateways.clear();
	json_object *j_loc = locations[location].installationInfo;
	json_object *j_list;
	if ( json_object_object_get_ex(j_loc, "gateways", &j_list) )
	{
		int l = json_object_array_length(j_list);
		int i;
		for (i = 0; i < l; i++)
		{
			locations[location].gateways[i].installationInfo = json_object_array_get_idx(j_list, i);

			json_object *j_gwInfo, *j_gwId;
			json_object_object_get_ex(locations[location].gateways[i].installationInfo, "gatewayInfo", &j_gwInfo);
			json_object_object_get_ex(j_gwInfo, "gatewayId", &j_gwId);
			locations[location].gateways[i].gatewayId = json_object_get_string(j_gwId);

			get_temperatureControlSystems(location,i);
		}
	}
}


void EvohomeClient::full_installation()
{
	locations.clear();
	stringstream url;
	url << "/WebAPI/emea/api/v1/location/installationInfo?userId=" << account_info["userId"] << "&includeTemperatureControlSystems=True";

	// evohome v1 interface does not correctly format the json output
	stringstream ss_jdata;
	ss_jdata << "{\"locations\": " << send_receive_data(url.str(), evoheader) << "}";
	json_object *j_fi = json_tokener_parse(ss_jdata.str().c_str());
	json_object *j_list;
	json_object_object_get_ex(j_fi, "locations", &j_list);
	int l = json_object_array_length(j_list);
	int i;
	for (i=0; i<l; i++)
	{
		locations[i].installationInfo = json_object_array_get_idx(j_list, i);

		json_object *j_locInfo, *j_locId;
		json_object_object_get_ex(locations[i].installationInfo, "locationInfo", &j_locInfo);
		json_object_object_get_ex(j_locInfo, "locationId", &j_locId);
		locations[i].locationId = json_object_get_string(j_locId);

		get_gateways(i);
	}
}


/************************************************************************
 *									*
 *	Evohome system status						*
 *									*
 ************************************************************************/

bool EvohomeClient::get_status(int location)
{
	if ( (locations.size() == 0) || ( json_object_is_type(locations[location].installationInfo,json_type_null) ) )
	{
		return false;
	}

	bool valid_json = true;
	stringstream url;
	url << "/WebAPI/emea/api/v1/location/" << get_locationId(location) << "/status?includeTemperatureControlSystems=True";
	locations[location].status = json_tokener_parse(send_receive_data(url.str(), evoheader).c_str());

	// get gateway status
	json_object *j_gwlist;
	if ( json_object_object_get_ex(locations[location].status, "gateways", &j_gwlist) )
	{
		int lgw = json_object_array_length(j_gwlist);
		int igw;
		for (igw = 0; igw < lgw; igw++)
		{
			locations[location].gateways[igw].status = json_object_array_get_idx(j_gwlist, igw);
			// get temperatureControlSystem status
			json_object *j_tcslist;
			if ( json_object_object_get_ex(locations[location].gateways[igw].status, "temperatureControlSystems", &j_tcslist) )
			{
				int ltcs = json_object_array_length(j_tcslist);
				int itcs;
				for (itcs = 0; itcs < ltcs; itcs++)
				{
					locations[location].gateways[igw].temperatureControlSystems[itcs].status = json_object_array_get_idx(j_tcslist, itcs);
					// get zone status
					json_object *j_zlist;
					if ( json_object_object_get_ex(locations[location].gateways[igw].temperatureControlSystems[itcs].status, "zones", &j_zlist) )
					{
						int lz = json_object_array_length(j_zlist);
						int iz;
						for (iz = 0; iz < lz; iz++)
						{
							locations[location].gateways[igw].temperatureControlSystems[itcs].zones[iz].status = json_object_array_get_idx(j_zlist, iz);
						}
					}
					else
						valid_json = false;
				}
			}
			else
				valid_json = false;
		}
	}
	else
		valid_json = false;

	return valid_json;
}


/************************************************************************
 *									*
 *	Schedule							*
 *									*
 ************************************************************************/


void EvohomeClient::get_schedule(std::string zoneId)
{
	schedule.clear();
	stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureZone/" << zoneId << "/schedule";
	json_object *j_week = json_tokener_parse(send_receive_data(url.str(), evoheader).c_str());
	json_object *j_days, *j_iwd, *j_swd;
	json_object_object_get_ex(j_week, "dailySchedules", &j_days);
	int l = json_object_array_length(j_days);
	int i;
	for (i = 0; i < l; i++)
	{
		j_iwd = json_object_array_get_idx(j_days, i);
		json_object_object_get_ex( j_iwd, "dayOfWeek", &j_swd);
		std::string key=json_object_get_string(j_swd);
		schedule[key] = json_object_get_string(j_iwd);
	}
}


std::string EvohomeClient::get_next_switchpoint(std::string zoneId)
{
	time_t now = time(0);
	struct tm ltime;
	localtime_r(&now,&ltime);
	int year = ltime.tm_year;
	int month = ltime.tm_mon;
	int day = ltime.tm_mday;
	get_schedule(zoneId);
	time_t ntime;
	std::string stime;
	int d;
	for (d = 0; d < 7; d++)
	{
		int wday = (ltime.tm_wday + d) % 7;
		std::string s_wday = (std::string)weekdays[wday];
		json_object *j_swd = json_tokener_parse(schedule[s_wday].c_str());
		json_object *j_list, *j_sp, *j_tim;
		json_object_object_get_ex( j_swd, "switchpoints", &j_list);
		int l = json_object_array_length(j_list);
		int i;
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
			ntime = mktime(&ltime);
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


bool EvohomeClient::schedules_backup(std::string filename)
{
	ofstream myfile (filename.c_str());
	if ( myfile.is_open() )
	{
		json_object *j_sched = json_object_new_object();

		unsigned int il;
		for (il = 0; il < locations.size(); il++)
		{
			json_object *j_loc = locations[il].installationInfo;
			json_object *j_locId, *j_locInfo, *j_locName;

			json_object_object_get_ex(j_loc, "locationInfo", &j_locInfo);
			json_object_object_get_ex(j_locInfo, "name", &j_locName);
			json_object_object_get_ex(j_locInfo, "locationId", &j_locId);
			std::string s_locId = json_object_get_string(j_locId);

			json_object *j_locsched = json_object_new_object();
			json_object_object_add(j_locsched,"locationId", j_locId);
			json_object_object_add(j_locsched,"name", j_locName);

			unsigned int igw;
			for (igw = 0; igw < locations[il].gateways.size(); igw++)
			{
				json_object *j_gw = locations[il].gateways[igw].installationInfo;
				json_object *j_gwId, *j_gwInfo;
		
				json_object_object_get_ex(j_gw, "gatewayInfo", &j_gwInfo);
				json_object_object_get_ex(j_gwInfo, "gatewayId", &j_gwId);
				std::string s_gwId = json_object_get_string(j_gwId);

				json_object *j_gwsched = json_object_new_object();
				json_object_object_add(j_gwsched,"gatewayId", j_gwId);

				unsigned int itcs;
				for (itcs = 0; itcs < locations[il].gateways[igw].temperatureControlSystems.size(); itcs++)
				{
					json_object *j_tcs = locations[il].gateways[igw].temperatureControlSystems[itcs].installationInfo;
					json_object *j_tcsId;
			
					json_object_object_get_ex(j_tcs, "systemId", &j_tcsId);
					std::string s_tcsId = json_object_get_string(j_tcsId);

					json_object *j_tcssched = json_object_new_object();
					json_object_object_add(j_tcssched,"systemId", j_tcsId);

					json_object *j_list, *j_zone;
					json_object_object_get_ex(j_tcs, "zones", &j_list);
					int l = json_object_array_length(j_list);

					json_object *j_zoneId, *j_name;
					stringstream url;
					int i;
					for (i = 0; i < l; i++)
					{
						j_zone = json_object_array_get_idx(j_list, i);

						json_object_object_get_ex(j_zone, "name", &j_name);
						json_object_object_get_ex(j_zone, "zoneId", &j_zoneId);
						std::string s_zoneId = json_object_get_string(j_zoneId);

						url.str("");
						url << "/WebAPI/emea/api/v1/temperatureZone/" << s_zoneId << "/schedule";
						json_object *j_week = json_tokener_parse(send_receive_data(url.str(), evoheader).c_str());
						json_object *j_days;
						json_object_object_get_ex(j_week, "dailySchedules", &j_days);

						json_object *j_zonesched = json_object_new_object();
						json_object_object_add(j_zonesched,"zoneId", j_zoneId);
						json_object_object_add(j_zonesched,"name", j_name);
						json_object_object_add(j_zonesched,"dailySchedules", j_days);
						json_object_object_add(j_tcssched,s_zoneId.c_str(), j_zonesched);
					}
/* Hot Water
 * GB3: I hope I got this right - I have no way of checking this because my installation does not have such a device
 */
					json_object *j_dhw;
					if (json_object_object_get_ex(j_tcs, "dhw", &j_dhw))
					{
						json_object_object_get_ex(j_zone, "dhwId", &j_zoneId);
						std::string s_zoneId = json_object_get_string(j_zoneId);
						url.str("");
						url << "/WebAPI/emea/api/v1/domesticHotWater/" << s_zoneId << "/schedule";
						json_object *j_week = json_tokener_parse(send_receive_data(url.str(), evoheader).c_str());
						json_object *j_days;
						json_object_object_get_ex(j_week, "dailySchedules", &j_days);

						json_object *j_zonesched = json_object_new_object();
						json_object_object_add(j_zonesched,"dhwId", j_zoneId);
						json_object_object_add(j_zonesched,"dailySchedules", j_days);
						json_object_object_add(j_tcssched,s_zoneId.c_str(), j_zonesched);
					}

					json_object_object_add(j_gwsched, s_tcsId.c_str(), j_tcssched);
				}
				json_object_object_add(j_locsched, s_gwId.c_str(), j_gwsched);
			}
			json_object_object_add(j_sched, s_locId.c_str(), j_locsched);
		}

		myfile << json_object_to_json_string_ext(j_sched,JSON_C_TO_STRING_PRETTY);
		myfile.close();
		return true;
	}
	return false;
}

bool EvohomeClient::read_schedules_from_file(std::string filename)
{
	stringstream ss;
	ifstream myfile (filename.c_str());
	if ( myfile.is_open() )
	{
		std::string line;
		while ( getline (myfile,line) )
		{
			ss << line << '\n';
		}
		myfile.close();
	}
	std::string s_fcontent = ss.str();
	if (s_fcontent == "")
		return false;

	json_object *j_sched = json_tokener_parse(s_fcontent.c_str());
	int location, gateway, tcs, zone;


	json_object_object_foreach(j_sched, locationId, j_loc)
	{
		location = get_location_by_ID(locationId);
		if (location == -1)
			continue;
		json_object_object_foreach(j_loc, gatewayId, j_gw)
		{
			gateway = get_gateway_by_ID(location, gatewayId);
			if (gateway == -1)
				continue;

			json_object_object_foreach(j_gw, systemId, j_tcs)
			{
				tcs = get_temperatureControlSystem_by_ID(location, gateway, systemId);
				if (tcs == -1)
					continue;

				json_object_object_foreach(j_tcs, zoneId, j_zone)
				{
					zone = get_zone_by_ID(location, gateway, tcs, zoneId);
					if (zone == -1)
						continue;
					json_object_object_get_ex(j_zone, "dailySchedules", &locations[location].gateways[gateway].temperatureControlSystems[tcs].zones[zone].schedule);

				}
			}
		}
	}
	return true;
}



void EvohomeClient::set_schedule(std::string zoneId, std::string zoneType, json_object *schedule)
{
	stringstream url;
	url << "/WebAPI/emea/api/v1/" << zoneType << "/" << zoneId << "/schedule";
	std::string postdata = json_object_get_string(schedule);
	stringstream ss;
	unsigned int i;
	unsigned int j = 0;
	char c;
	char word[30];
	for (i = 0; i < postdata.length(); i++)
	{
		c = postdata[i];
		if ( ((c|0x20) <= 0x60) || ((c|0x20) >= 0x7b) )
		{
			if (j > 0)
			{
				word[j] = '\0';
				if (strcmp(word,"Temperature") == 0)
					ss << "TargetTemperature";
				else
					ss << word;
				j = 0;
			}
			ss << c;
		}
		else
		{
			if (j == 0)
				word[j] = toupper(c);
			else
				word[j] = c;
			j++;
		}
	}

	put_receive_data(url.str(), ss.str(), evoheader);
}

void EvohomeClient::set_zone_schedule(std::string zoneId, json_object *schedule)
{
	set_schedule(zoneId, "temperatureZone", schedule);
}

void EvohomeClient::set_dhw_schedule(std::string zoneId, json_object *schedule)
{
	set_schedule(zoneId, "domesticHotWater", schedule);
}


bool EvohomeClient::schedules_restore(std::string filename)
{
	if ( ! read_schedules_from_file(filename) )
		return false;

	cout << "Restoring schedules from file " << filename << endl;
	unsigned int l,g,t,z;
	for (l = 0; l < locations.size(); l++)
	{
		cout << "  Location: " << locations[l].locationId << endl;

		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			cout << "    Gateway: " << locations[l].gateways[g].gatewayId << endl;

			for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				cout << "      System: " << locations[l].gateways[g].temperatureControlSystems[t].systemId << endl;
	
				for (z = 0; z < locations[l].gateways[g].temperatureControlSystems[t].zones.size(); z++)
				{
					cout << "        Zone: " << json_get_val(locations[l].gateways[g].temperatureControlSystems[t].zones[z].status, "name") << endl;
					json_object *j_sched = json_object_new_object();
					json_object_object_add(j_sched,"DailySchedules", locations[l].gateways[g].temperatureControlSystems[t].zones[z].schedule);

					set_zone_schedule( locations[l].gateways[g].temperatureControlSystems[t].zones[z].zoneId, j_sched);
				}
			}
		}
	}
	return true;
}








/************************************************************************
 *									*
 *									*
 *									*
 ************************************************************************/

int EvohomeClient::get_location_by_ID(std::string locationId)
{
	if (locations.size() == 0)
		full_installation();
	int i;
	for (i = 0; i < (int)locations.size(); i++)
	{
		if (locations[i].locationId == locationId)
			return i;
	}
	return -1;
}


int EvohomeClient::get_gateway_by_ID(int location, std::string gatewayId)
{
	if (locations.size() == 0)
		full_installation();
	int i;
	for (i = 0; i < (int)locations[location].gateways.size(); i++)
	{
		if (locations[location].gateways[i].gatewayId == gatewayId)
			return i;
	}
	return -1;
}


int EvohomeClient::get_temperatureControlSystem_by_ID(int location, int gateway, std::string systemId)
{
	if (locations.size() == 0)
		full_installation();
	int i;
	for (i = 0; i < (int)locations[location].gateways[gateway].temperatureControlSystems.size(); i++)
	{
		if (locations[location].gateways[gateway].temperatureControlSystems[i].systemId == systemId)
			return i;
	}
	return -1;
}

int EvohomeClient::get_zone_by_ID(int location, int gateway, int temperatureControlSystem, std::string zoneId)
{
	if (locations.size() == 0)
		full_installation();
	int i;
	for (i = 0; i < (int)locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones.size(); i++)
	{
		if (locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].zoneId == zoneId)
			return i;
	}
	return -1;
}


bool EvohomeClient::has_dhw(int location, int gateway, int temperatureControlSystem)
{
	evo_temperatureControlSystem *tcs = &locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];
	return has_dhw(tcs);
}

bool EvohomeClient::has_dhw(evo_temperatureControlSystem *tcs)
{
	json_object *j_dhw;
	return json_object_object_get_ex(tcs->status, "dhw", &j_dhw);
}

/************************************************************************
 *									*
 *									*
 *									*
 ************************************************************************/

std::string EvohomeClient::get_locationId(unsigned int location)
{
	if (locations.size() == 0)
		full_installation();

	if (location >= locations.size())
		return "";

	return json_get_val(locations[location].installationInfo, "locationInfo", "locationId");
}


std::string EvohomeClient::json_get_val(std::string s_json, std::string key)
{
	return json_get_val(json_tokener_parse(s_json.c_str()), key);
}


std::string EvohomeClient::json_get_val(json_object *j_json, std::string key)
{
	json_object *j_res;

	if (json_object_object_get_ex(j_json, key.c_str(), &j_res))
		return json_object_get_string(j_res);
	return "";
}


std::string EvohomeClient::json_get_val(std::string s_json, std::string key1, std::string key2)
{
	return json_get_val(json_tokener_parse(s_json.c_str()), key1, key2);
}


std::string EvohomeClient::json_get_val(json_object *j_json, std::string key1, std::string key2)
{
	json_object *j_tmp, *j_res;

	if ( ( json_object_object_get_ex( j_json, key1.c_str(), &j_tmp )) && ( json_object_object_get_ex( j_tmp, key2.c_str(), &j_res )) )
		return json_object_get_string(j_res);
	return "";
}

