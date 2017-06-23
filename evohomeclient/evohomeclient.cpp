/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for UK/EMEA Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <cstring>
#include <string>
#include <ctime>
#include "webclient.h"
#include "evohomeclient.h"
#include "jsoncpp/json.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

#define EVOHOME_HOST "https://tccna.honeywell.com"


#ifdef _WIN32
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#endif

//using namespace std;

const std::string weekdays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const std::string evo_modes[7] = {"Auto", "HeatingOff", "AutoWithEco", "Away", "DayOff", "", "Custom"};

/*
 * Class construct
 */
EvohomeClient::EvohomeClient(std::string user, std::string password)
{
	evoheader.clear();
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
	web_connection_cleanup("evohome");
}

/*
 * Execute web query
 */
std::string EvohomeClient::send_receive_data(std::string url, std::vector<std::string> &header)
{
	return send_receive_data(url, "", header);
}

std::string EvohomeClient::send_receive_data(std::string url, std::string postdata, std::vector<std::string> &header)
{

	std::stringstream ss_url;
	ss_url << EVOHOME_HOST << url;
	return web_send_receive_data("evohome", ss_url.str(), postdata, header);
}

std::string EvohomeClient::put_receive_data(std::string url, std::string putdata, std::vector<std::string> &header)
{

	std::stringstream ss_url;
	ss_url << EVOHOME_HOST << url;
	return web_send_receive_data("evohome", ss_url.str(), putdata, header, "PUT");
}

/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/

bool EvohomeClient::login(std::string user, std::string password)
{
	std::vector<std::string> lheader;
	lheader.push_back("Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	lheader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	lheader.push_back("charsets: utf-8");

	std::stringstream pdata;

	pdata << "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8";
	pdata << "&Host=rs.alarmnet.com%2F";
	pdata << "&Cache-Control=no-store%20no-cache";
	pdata << "&Pragma=no-cache";
	pdata << "&grant_type=password";
	pdata << "&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous%20EMEA-V1-Get-Current-User-Account";
	pdata << "&Username=" << urlencode(user);
	pdata << "&Password=" << urlencode(password);
	pdata << "&Connection=Keep-Alive";


	std::string s_res = send_receive_data("/Auth/OAuth/Token", pdata.str(), lheader);

	if (s_res.find("<title>") != std::string::npos) // got an HTML page
	{
		std::cout << "Login to Evohome server failed - server responds: ";
		int i = s_res.find("<title>");
		char* html = &s_res[i];
		i = 7;
		char c = html[i];
		while (c != '<')
		{
			std::cout << c;
			i++;
			c = html[i];
		}
		std::cout << "\n";
		web_connection_cleanup("evohome");
		return false;
	}

	if (s_res[0] == '[') // got unnamed array as reply
	{
		s_res[0] = ' ';
		size_t len = s_res.size();
		len--;
		s_res[len] = ' ';
	}

	Json::Value j_login;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_login))
		return false;

	std::string szError = "";
	if (j_login.isMember("error"))
		szError = j_login["error"].asString();
	if (j_login.isMember("message"))
		szError = j_login["message"].asString();
	if (!szError.empty())
		return false;


	std::stringstream atoken;
	atoken << "Authorization: bearer " << j_login["access_token"].asString();

	evoheader.clear();
	evoheader.push_back(atoken.str());
	evoheader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	evoheader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	evoheader.push_back("content-type: application/json");
	evoheader.push_back("charsets: utf-8");

	return user_account();
}


/* 
 * Retrieve evohome user info
 */
bool EvohomeClient::user_account()
{
	account_info.clear();
	std::string s_res = send_receive_data("/WebAPI/emea/api/v1/userAccount", evoheader);
	if (s_res[0] == '[') // got unnamed array as reply
	{
		s_res[0] = ' ';
		size_t len = s_res.size();
		len--;
		s_res[len] = ' ';
	}
	Json::Value j_account;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_account) || !j_account.isMember("userId"))
		return false;

	account_info["userId"] = j_account["userId"].asString();
	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations retrieval				*
 *									*
 ************************************************************************/

void EvohomeClient::get_zones(int location, int gateway, int temperatureControlSystem)
{
	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones.clear();
	Json::Value *j_tcs = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].installationInfo;

	if (!(*j_tcs)["zones"].isArray())
		return;

	size_t l = (*j_tcs)["zones"].size();
	for (size_t i = 0; i < l; ++i)
	{
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].installationInfo = &(*j_tcs)["zones"][(int)(i)];
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].zoneId = (*j_tcs)["zones"][(int)(i)]["zoneId"].asString();


		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].systemId = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].systemId;
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].gatewayId = locations[location].gateways[gateway].gatewayId;
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].locationId = locations[location].locationId;
	}
}


void EvohomeClient::get_temperatureControlSystems(int location, int gateway)
{
	locations[location].gateways[gateway].temperatureControlSystems.clear();
	Json::Value *j_gw = locations[location].gateways[gateway].installationInfo;

	if (!(*j_gw)["temperatureControlSystems"].isArray())
		return;

	size_t l = (*j_gw)["temperatureControlSystems"].size();
	for (size_t i = 0; i < l; ++i)
	{
		locations[location].gateways[gateway].temperatureControlSystems[i].installationInfo = &(*j_gw)["temperatureControlSystems"][(int)(i)];
		locations[location].gateways[gateway].temperatureControlSystems[i].systemId = (*j_gw)["temperatureControlSystems"][(int)(i)]["systemId"].asString();
		locations[location].gateways[gateway].temperatureControlSystems[i].gatewayId = locations[location].gateways[gateway].gatewayId;
		locations[location].gateways[gateway].temperatureControlSystems[i].locationId = locations[location].locationId;

		get_zones(location, gateway, i);
	}
}


void EvohomeClient::get_gateways(int location)
{
	locations[location].gateways.clear();
	Json::Value *j_loc = locations[location].installationInfo;

	if (!(*j_loc)["gateways"].isArray())
		return;

	size_t l = (*j_loc)["gateways"].size();
	for (size_t i = 0; i < l; ++i)
	{
		locations[location].gateways[i].installationInfo = &(*j_loc)["gateways"][(int)(i)];
		locations[location].gateways[i].gatewayId = (*j_loc)["gateways"][(int)(i)]["gatewayInfo"]["gatewayId"].asString();
		locations[location].gateways[i].locationId = locations[location].locationId;

		get_temperatureControlSystems(location, i);
	}
}


bool EvohomeClient::full_installation()
{
	locations.clear();
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/location/installationInfo?userId=" << account_info["userId"] << "&includeTemperatureControlSystems=True";

	// evohome v1 interface does not correctly format the json output
	std::stringstream ss_jdata;
	ss_jdata << "{\"locations\": " << send_receive_data(url.str(), evoheader) << "}";
	Json::Reader jReader;
	j_fi.clear();
	if (!jReader.parse(ss_jdata.str(), j_fi) || !j_fi["locations"].isArray())
		return false; // invalid return

	size_t l = j_fi["locations"].size();
	for (size_t i = 0; i < l; ++i)
	{
		locations[i].installationInfo = &j_fi["locations"][(int)(i)];
		locations[i].locationId = j_fi["locations"][(int)(i)]["locationInfo"]["locationId"].asString();

		get_gateways(i);
	}
	return true;
}


/************************************************************************
 *									*
 *	Evohome system status retrieval					*
 *									*
 ************************************************************************/

bool EvohomeClient::get_status(std::string locationId)
{
	if (locations.size() == 0)
		return false;
	for (size_t i = 0; i < locations.size(); i++)
	{
		if (locations[i].locationId == locationId)
			return get_status(i);
	}
	return false;
}
bool EvohomeClient::get_status(int location)
{
	Json::Value *j_loc, *j_gw, *j_tcs;
	if ((locations.size() == 0) || locations[location].locationId.empty())
		return false;

	bool valid_json = true;
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/location/" << locations[location].locationId << "/status?includeTemperatureControlSystems=True";
	std::string s_res = send_receive_data(url.str(), evoheader);

	Json::Reader jReader;
	j_stat.clear();
	if (!jReader.parse(s_res, j_stat))
		return false;
	locations[location].status = &j_stat;
	j_loc = locations[location].status;

	// get gateway status
	if ((*j_loc)["gateways"].isArray())
	{
		size_t lgw = (*j_loc)["gateways"].size();
		for (size_t igw = 0; igw < lgw; ++igw)
		{
			locations[location].gateways[igw].status = &(*j_loc)["gateways"][(int)(igw)];
			j_gw = locations[location].gateways[igw].status;

			// get temperatureControlSystem status
			if ((*j_gw)["temperatureControlSystems"].isArray())
			{
				size_t ltcs = (*j_gw)["temperatureControlSystems"].size();
				for (size_t itcs = 0; itcs < ltcs; itcs++)
				{
					locations[location].gateways[igw].temperatureControlSystems[itcs].status = &(*j_gw)["temperatureControlSystems"][(int)(itcs)];
					j_tcs = locations[location].gateways[igw].temperatureControlSystems[itcs].status;

					// get zone status
					if ((*j_tcs)["zones"].isArray())
					{
						size_t lz = (*j_tcs)["zones"].size();
						for (size_t iz = 0; iz < lz; iz++)
						{
							locations[location].gateways[igw].temperatureControlSystems[itcs].zones[iz].status = &(*j_tcs)["zones"][(int)(iz)];
							locations[location].gateways[igw].temperatureControlSystems[itcs].zones[iz].hdtemp="";
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
 *	Schedule handlers						*
 *									*
 ************************************************************************/


EvohomeClient::location* EvohomeClient::get_location_by_ID(std::string locationId)
{
	if (locations.size() == 0)
		full_installation();
	unsigned int l;
	for (l = 0; l < locations.size(); l++)
	{
		if (locations[l].locationId == locationId)
			return &locations[l];
	}
	return NULL;
}


EvohomeClient::gateway* EvohomeClient::get_gateway_by_ID(std::string gatewayId)
{
	if (locations.size() == 0)
		full_installation();
	unsigned int l,g;
	for (l = 0; l < locations.size(); l++)
	{
		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			if (locations[l].gateways[g].gatewayId == gatewayId)
				return &locations[l].gateways[g];
		}
	}
	return NULL;
}


EvohomeClient::temperatureControlSystem* EvohomeClient::get_temperatureControlSystem_by_ID(std::string systemId)
{
	if (locations.size() == 0)
		full_installation();
	unsigned int l,g,t;
	for (l = 0; l < locations.size(); l++)
	{
		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				if (locations[l].gateways[g].temperatureControlSystems[t].systemId == systemId)
					return &locations[l].gateways[g].temperatureControlSystems[t];
			}
		}
	}
	return NULL;
}


EvohomeClient::zone* EvohomeClient::get_zone_by_ID(std::string zoneId)
{
	if (locations.size() == 0)
		full_installation();
	unsigned int l,g,t,z;
	for (l = 0; l < locations.size(); l++)
	{
		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				for (z = 0; z < locations[l].gateways[g].temperatureControlSystems[t].zones.size(); z++)
				{
					if (locations[l].gateways[g].temperatureControlSystems[t].zones[z].zoneId == zoneId)
						return &locations[l].gateways[g].temperatureControlSystems[t].zones[z];
				}
			}
		}
	}
	return NULL;
}


EvohomeClient::temperatureControlSystem* EvohomeClient::get_zone_temperatureControlSystem(EvohomeClient::zone* zone)
{
	unsigned int l,g,t,z;
	for (l = 0; l < locations.size(); l++)
	{
		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				for (z = 0; z < locations[l].gateways[g].temperatureControlSystems[t].zones.size(); z++)
				{
					if (locations[l].gateways[g].temperatureControlSystems[t].zones[z].zoneId == zone->zoneId)
						return &locations[l].gateways[g].temperatureControlSystems[t];
				}
			}
		}
	}
	return NULL;
}


std::string EvohomeClient::request_next_switchpoint(std::string zoneId)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureZone/" << zoneId << "/schedule/upcommingSwitchpoints?count=1";
	std::string s_res = send_receive_data(url.str(), evoheader);
	if (s_res[0] == '[') // got unnamed array as reply
	{
		s_res[0] = ' ';
		size_t len = s_res.size();
		len--;
		s_res[len] = ' ';
	}

	Json::Value j_sp;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_sp) || !j_sp.isMember("time"))
		return "";

	std::stringstream ss;
	ss << j_sp["time"].asString() << 'Z';
	return ss.str();
}


bool EvohomeClient::get_schedule(std::string zoneId)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureZone/" << zoneId << "/schedule";
	std::string s_res = send_receive_data(url.str(), evoheader);
	if (!s_res.find("\"id\""))
		return false;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), get_zone_by_ID(zoneId)->schedule))
		return false;
	return true;
}


std::string EvohomeClient::get_next_switchpoint(EvohomeClient::temperatureControlSystem* tcs, int zone)
{
	if ((tcs->zones[zone].schedule.isNull()) && !get_schedule(tcs->zones[zone].zoneId))
		return "";
	return get_next_switchpoint(tcs->zones[zone].schedule);
}
std::string EvohomeClient::get_next_switchpoint(zone* hz)
{
	if ((hz->schedule.isNull()) && !get_schedule(hz->zoneId))
		return "";
	return get_next_switchpoint(hz->schedule);
}
std::string EvohomeClient::get_next_switchpoint(std::string zoneId)
{
	return get_next_switchpoint(get_zone_by_ID(zoneId));
}
std::string EvohomeClient::get_next_switchpoint(Json::Value &schedule)
{
	std::string current_setpoint;
	return get_next_switchpoint_ex(schedule, current_setpoint);
}
std::string EvohomeClient::get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint)
{
	return get_next_switchpoint_ex(schedule, current_setpoint, -1);
}
std::string EvohomeClient::get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint, int force_weekday)
{
	if (schedule.isNull())
		return "";

	struct tm ltime;
	time_t now = time(0);
	localtime_r(&now, &ltime);
	int year = ltime.tm_year;
	int month = ltime.tm_mon;
	int day = ltime.tm_mday;
	int wday = (force_weekday >= 0) ? (force_weekday % 7) : ltime.tm_wday;
	char rdata[30];
	sprintf(rdata, "%04d-%02d-%02dT%02d:%02d:%02dZ", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	std::string szdatetime = std::string(rdata);
	if (szdatetime <= schedule["nextSwitchpoint"].asString()) // our current cached values are still valid
	{
		current_setpoint = schedule["currentSetpoint"].asString();
		return schedule["nextSwitchpoint"].asString();
	}

	std::string sztime;
	bool found = false;
	current_setpoint = "";
	for (uint8_t d = 0; ((d < 7) && !found); d++)
	{
		int tryday = (wday + d) % 7;
		std::string s_tryday = (std::string)weekdays[tryday];
		Json::Value* j_day;
		// find day
		for (size_t i = 0; ((i < schedule["dailySchedules"].size()) && !found); i++)
		{
			j_day = &schedule["dailySchedules"][(int)(i)];
			if (((*j_day).isMember("dayOfWeek")) && ((*j_day)["dayOfWeek"] == s_tryday))
				found = true;
		}
		if (!found)
			continue;

		found = false;
		for (size_t i = 0; ((i < (*j_day)["switchpoints"].size()) && !found); ++i)
		{
			sztime = (*j_day)["switchpoints"][(int)(i)]["timeOfDay"].asString();
			ltime.tm_isdst = -1;
			ltime.tm_year = year;
			ltime.tm_mon = month;
			ltime.tm_mday = day + d;
			ltime.tm_hour = std::atoi(sztime.substr(0, 2).c_str());
			ltime.tm_min = std::atoi(sztime.substr(3, 2).c_str());
			ltime.tm_sec = std::atoi(sztime.substr(6, 2).c_str());
			time_t ntime = mktime(&ltime);
			if (ntime > now)
				found = true;
			else
				current_setpoint = (*j_day)["switchpoints"][(int)(i)]["temperature"].asString();
		}
	}

	if (current_setpoint.empty()) // got a direct match for the next switchpoint, need to go back in time to find the current setpoint
	{
		found = false;
		for (uint8_t d = 1; ((d < 7) && !found); d++)
		{
			int tryday = (wday - d + 7) % 7;
			std::string s_tryday = (std::string)weekdays[tryday];
			Json::Value* j_day;
			// find day
			for (size_t i = 0; ((i < schedule["dailySchedules"].size()) && !found); i++)
			{
				j_day = &schedule["dailySchedules"][(int)(i)];
				if (((*j_day).isMember("dayOfWeek")) && ((*j_day)["dayOfWeek"] == s_tryday))
					found = true;
			}
			if (!found)
				continue;

			found = false;
			size_t l = (*j_day)["switchpoints"].size();
			if (l > 0)
			{
				l--;
				current_setpoint = (*j_day)["switchpoints"][(int)(l)]["temperature"].asString();
				found = true;
			}
		}
	}

	if (!found)
		return "";

	sprintf(rdata, "%04d-%02d-%02dT%sA", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, sztime.c_str()); // localtime => use CET to indicate that it is not UTC
	szdatetime = std::string(rdata);
	schedule["currentSetpoint"] = current_setpoint;
	schedule["nextSwitchpoint"] = szdatetime;
	return szdatetime;
}





bool EvohomeClient::schedules_backup(std::string filename)
{
	std::ofstream myfile (filename.c_str());
	if ( myfile.is_open() )
	{
		Json::Value j_sched;

		size_t il;
		for (il = 0; il < locations.size(); il++)
		{
			Json::Value *j_loc = locations[il].installationInfo;
			std::string s_locId = (*j_loc)["locationInfo"]["locationId"].asString();
			if (s_locId.empty())
				continue;

			Json::Value j_locsched;
			j_locsched["locationId"] = s_locId;
			j_locsched["name"] = (*j_loc)["locationInfo"]["name"].asString();

			size_t igw;
			for (igw = 0; igw < locations[il].gateways.size(); igw++)
			{
				Json::Value *j_gw = locations[il].gateways[igw].installationInfo;
				std::string s_gwId = (*j_gw)["gatewayInfo"]["gatewayId"].asString();
				if (s_gwId.empty())
					continue;
		
				Json::Value j_gwsched;
				j_gwsched["gatewayId"] = s_gwId;

				size_t itcs;
				for (itcs = 0; itcs < locations[il].gateways[igw].temperatureControlSystems.size(); itcs++)
				{
					Json::Value *j_tcs = locations[il].gateways[igw].temperatureControlSystems[itcs].installationInfo;
					std::string s_tcsId = (*j_tcs)["systemId"].asString();
		
					if (s_tcsId.empty())
						continue;

					Json::Value j_tcssched;
					j_tcssched["systemId"] = s_tcsId;
					if (!(*j_tcs)["zones"].isArray())
						continue;

					size_t iz;
					for (iz = 0; iz < (*j_tcs)["zones"].size(); iz++)
					{
						std::stringstream url;
						std::string s_zoneId = (*j_tcs)["zones"][(int)(iz)]["zoneId"].asString();
						if (s_zoneId.empty())
							continue;

						url << "/WebAPI/emea/api/v1/temperatureZone/" << s_zoneId << "/schedule";
						std::string s_res = send_receive_data(url.str(), evoheader);
						if (!s_res.find("\"id\""))
							continue;

						Json::Reader jReader;
						Json::Value j_week;
						if (!jReader.parse(s_res, j_week))
							continue;

						Json::Value j_zonesched;
						j_zonesched["zoneId"] = s_zoneId;
						j_zonesched["name"] = (*j_tcs)["zones"][(int)(iz)]["name"].asString();
						if (j_week["dailySchedules"].isArray())
							j_zonesched["dailySchedules"] = j_week["dailySchedules"];
						else
							j_zonesched["dailySchedules"] = Json::arrayValue;
						j_tcssched[s_zoneId] = j_zonesched;
					}
// Hot Water
					if (has_dhw(il, igw, itcs))
					{
						std::string s_dhwId = (*j_tcs)["dhw"]["dhwId"].asString();
						if (s_dhwId.empty())
							continue;

						std::stringstream url;
						url << "/WebAPI/emea/api/v1/domesticHotWater/" << s_dhwId << "/schedule";
						std::string s_res = send_receive_data(url.str(), evoheader);
						if ( ! s_res.find("\"id\""))
							return false;

						Json::Reader jReader;
						Json::Value j_week;
						if (!jReader.parse(s_res, j_week))
							continue;

						Json::Value j_dhwsched;
						j_dhwsched["dhwId"] = s_dhwId;
						if (j_week["dailySchedules"].isArray())
							j_dhwsched["dailySchedules"] = j_week["dailySchedules"];
						else
							j_dhwsched["dailySchedules"] = Json::arrayValue;
						j_tcssched[s_dhwId] = j_dhwsched;
					}
					j_gwsched[s_tcsId] = j_tcssched;
				}
				j_locsched[s_gwId] = j_gwsched;
			}
			j_sched[s_locId] = j_locsched;
		}

		myfile << j_sched.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


bool EvohomeClient::read_schedules_from_file(std::string filename)
{
	std::stringstream ss;
	std::ifstream myfile (filename.c_str());
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

	Json::Reader jReader;
	Json::Value j_sched;
	if (!jReader.parse(s_fcontent, j_sched))
		return false;


	Json::Value::Members locations = j_sched.getMemberNames();
	for (size_t l = 0; l < locations.size(); l++)
	{
		if (j_sched[locations[l]].isString())
			continue;
		Json::Value::Members gateways = j_sched[locations[l]].getMemberNames();
		for (size_t g = 0; g < gateways.size(); g++)
		{
			if (j_sched[locations[l]][gateways[g]].isString())
				continue;
			Json::Value::Members temperatureControlSystems = j_sched[locations[l]][gateways[g]].getMemberNames();
			for (size_t t = 0; t < temperatureControlSystems.size(); t++)
			{
				if (j_sched[locations[l]][gateways[g]][temperatureControlSystems[t]].isString())
					continue;
				Json::Value::Members zones = j_sched[locations[l]][gateways[g]][temperatureControlSystems[t]].getMemberNames();
				for (size_t z = 0; z < zones.size(); z++)
				{
					if (j_sched[locations[l]][gateways[g]][temperatureControlSystems[t]][zones[z]].isString())
						continue;
					EvohomeClient::zone* zone = get_zone_by_ID(zones[z]);
					zone->schedule = j_sched[locations[l]][gateways[g]][temperatureControlSystems[t]][zones[z]];
				}
			}
		}
	}
	return true;
}


bool EvohomeClient::set_schedule(std::string zoneId, std::string zoneType, Json::Value *schedule)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/" << zoneType << "/" << zoneId << "/schedule";
	std::string postdata = (*schedule).toStyledString();
	std::stringstream ss;
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

	std::string s_res = put_receive_data(url.str(), ss.str(), evoheader);
	if (s_res.find("\"id\""))
		return true;
	return false;
}


bool EvohomeClient::set_zone_schedule(std::string zoneId, Json::Value *schedule)
{
	return set_schedule(zoneId, "temperatureZone", schedule);
}


bool EvohomeClient::set_dhw_schedule(std::string zoneId, Json::Value *schedule)
{
	return set_schedule(zoneId, "domesticHotWater", schedule);
}


bool EvohomeClient::schedules_restore(std::string filename)
{
	if ( ! read_schedules_from_file(filename) )
		return false;

	std::cout << "Restoring schedules from file " << filename << "\n";
	unsigned int l,g,t,z;
	for (l = 0; l < locations.size(); l++)
	{
		std::cout << "  Location: " << locations[l].locationId << "\n";

		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			std::cout << "    Gateway: " << locations[l].gateways[g].gatewayId << "\n";

			for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				std::cout << "      System: " << locations[l].gateways[g].temperatureControlSystems[t].systemId << "\n";
	
				for (z = 0; z < locations[l].gateways[g].temperatureControlSystems[t].zones.size(); z++)
				{
					std::cout << "        Zone: " << (*locations[l].gateways[g].temperatureControlSystems[t].zones[z].installationInfo)["name"].asString() << "\n";
					set_zone_schedule(locations[l].gateways[g].temperatureControlSystems[t].zones[z].zoneId, &locations[l].gateways[g].temperatureControlSystems[t].zones[z].schedule);
				}
			}
		}
	}
	return true;
}


/************************************************************************
 *									*
 *	Evohome overrides						*
 *									*
 ************************************************************************/

bool verify_date(std::string date)
{
	if (date.length() < 10)
		return false;
	std:: string s_date = date.substr(0,10);
	struct tm mtime;
	mtime.tm_isdst = -1;
	mtime.tm_year = atoi(date.substr(0, 4).c_str()) - 1900;
	mtime.tm_mon = atoi(date.substr(5, 2).c_str()) - 1;
	mtime.tm_mday = atoi(date.substr(8, 2).c_str());
	mtime.tm_hour = 12; // midday time - prevent date shift because of DST
	mtime.tm_min = 0;
	mtime.tm_sec = 0;
	time_t ntime = mktime(&mtime);
	if ( ntime == -1)
		return false;
	char rdata[12];
	sprintf(rdata,"%04d-%02d-%02d",mtime.tm_year+1900,mtime.tm_mon+1,mtime.tm_mday);
	return (s_date == std::string(rdata));
}

	
bool verify_datetime(std::string datetime)
{
	if (datetime.length() < 19)
		return false;
	std:: string s_date = datetime.substr(0,10);
	std:: string s_time = datetime.substr(11,8);
	struct tm mtime;
	mtime.tm_isdst = -1;
	mtime.tm_year = atoi(datetime.substr(0, 4).c_str()) - 1900;
	mtime.tm_mon = atoi(datetime.substr(5, 2).c_str()) - 1;
	mtime.tm_mday = atoi(datetime.substr(8, 2).c_str());
	mtime.tm_hour = atoi(datetime.substr(11, 2).c_str());
	mtime.tm_min = atoi(datetime.substr(14, 2).c_str());
	mtime.tm_sec = atoi(datetime.substr(17, 2).c_str());
	time_t ntime = mktime(&mtime);
	if ( ntime == -1)
		return false;
	char c_date[12];
	sprintf(c_date,"%04d-%02d-%02d",mtime.tm_year+1900,mtime.tm_mon+1,mtime.tm_mday);
	char c_time[12];
	sprintf(c_time,"%02d:%02d:%02d",mtime.tm_hour,mtime.tm_min,mtime.tm_sec);
	return ( (s_date == std::string(c_date)) && (s_time == std::string(c_time)) );
}


bool EvohomeClient::set_system_mode(std::string systemId, int mode, std::string date_until)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureControlSystem/" << systemId << "/mode";
	std::stringstream data;
	data << "{\"SystemMode\":" << mode;
	if (date_until == "")
		data << ",\"TimeUntil\":null,\"Permanent\":true}";
	else
	{
		if ( ! verify_date(date_until) )
			return false;
		data << ",\"TimeUntil\":\"" << date_until.substr(0,10) << "T00:00:00Z\",\"Permanent\":false}";
	}
	std::string s_res = put_receive_data(url.str(), data.str(), evoheader);
	if (s_res.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient::set_system_mode(std::string systemId, int mode)
{
	return set_system_mode(systemId, mode, "");
}
bool EvohomeClient::set_system_mode(std::string systemId, std::string mode, std::string date_until)
{
	int i = 0;
	int s = sizeof(evo_modes);
	while (s > 0)
	{
		if (evo_modes[i] == mode)
			return set_system_mode(systemId, i, date_until);
		s -= sizeof(evo_modes[i]);
		i++;
	}
	return false;
}
bool EvohomeClient::set_system_mode(std::string systemId, std::string mode)
{
	return set_system_mode(systemId, mode, "");
}



bool EvohomeClient::set_temperature(std::string zoneId, std::string temperature, std::string time_until)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureZone/" << zoneId << "/heatSetpoint";
	std::stringstream data;
	data << "{\"HeatSetpointValue\":" << temperature;
	if (time_until == "")
		data << ",\"SetpointMode\":1,\"TimeUntil\":null}";
	else
	{
		if ( ! verify_datetime(time_until) )
			return false;
		data << ",\"SetpointMode\":2,\"TimeUntil\":\"" << time_until.substr(0,10) << "T" << time_until.substr(11,8) << "Z\"}";
	}
	std::string s_res = put_receive_data(url.str(), data.str(), evoheader);
	if (s_res.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient::set_temperature(std::string zoneId, std::string temperature)
{
	return set_temperature(zoneId, temperature, "");
}


bool EvohomeClient::cancel_temperature_override(std::string zoneId)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureZone/" << zoneId << "/heatSetpoint";
	std::string s_data = "{\"HeatSetpointValue\":0.0,\"SetpointMode\":0,\"TimeUntil\":null}";
	std::string s_res = put_receive_data(url.str(), s_data, evoheader);
	if (s_res.find("\"id\""))
		return true;
	return false;
}


bool EvohomeClient::has_dhw(int location, int gateway, int temperatureControlSystem)
{
	EvohomeClient::temperatureControlSystem *tcs = &locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];
	return has_dhw(tcs);
}
bool EvohomeClient::has_dhw(EvohomeClient::temperatureControlSystem *tcs)
{
	return (*tcs->status).isMember("dhw");
}


bool EvohomeClient::is_single_heating_system()
{
	if (locations.size() == 0)
		full_installation();
	return ( (locations.size() == 1) &&
		 (locations[0].gateways.size() == 1) &&
		 (locations[0].gateways[0].temperatureControlSystems.size() == 1) );
}


bool EvohomeClient::set_dhw_mode(std::string dhwId, std::string mode, std::string time_until)
{
	std::stringstream data;
	if (mode == "auto")
		data << "{\"State\":0,\"Mode\":0,\"UntilTime\":null}";
	else
	{
		data << "{\"State\":";
		if (mode == "on")
			data << 1;
		else
			data << 0;
		if (time_until == "")
			data << ",\"Mode\":1,\"UntilTime\":null}";
		else
		{
			if ( ! verify_datetime(time_until) )
				return false;
			data << ",\"Mode\":2,\"UntilTime\":\"" << time_until.substr(0,10) << "T" << time_until.substr(11,8) << "Z\"}";
		}
	}
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/domesticHotWater/" << dhwId << "/state";
	std::string s_res = put_receive_data(url.str(), data.str(), evoheader);
	if (s_res.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient::set_dhw_mode(std::string systemId, std::string mode)
{
	return set_dhw_mode(systemId, mode, "");
}

