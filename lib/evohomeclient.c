#include <malloc.h>
#include <cstring>
#include <ctime>
#include "webclient.h"
#include "evohomeclient.h"


#define EVOHOME_HOST "https://tccna.honeywell.com"

using namespace std;


// Lookup for translating numeric tm_wday to a full weekday string
std::string weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Lookup for evohome numeric state to a full string
std::string evo_modes[] = {"Auto", "HeatingOff", "AutoWithEco", "Away", "DayOff", "", "Custom"};


/*
 * Class construct
 */
EvohomeClient::EvohomeClient(std::string user, std::string password)
{
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

	stringstream pdata;
	pdata << "Content-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8";
	pdata << "&Host=rs.alarmnet.com%2F";
	pdata << "&Cache-Control=no-store%20no-cache";
	pdata << "&Pragma=no-cache";
	pdata << "&grant_type=password";
	pdata << "&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous%20EMEA-V1-Get-Current-User-Account";
	pdata << "&Username=" << urlencode(user);
	pdata << "&Password=" << urlencode(password);
	pdata << "&Connection=Keep-Alive";

	json_object *j_ret = json_tokener_parse(send_receive_data("/Auth/OAuth/Token", pdata.str(), lheader).c_str());
	json_object_object_foreach(j_ret, key, val)
	{
		auth_info[key] = json_object_get_string(val);
	}

	stringstream atoken;
	atoken << "Authorization: bearer " << auth_info["access_token"];
	evoheader = curl_slist_append(evoheader,atoken.str().c_str());
	evoheader = curl_slist_append(evoheader,"applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	evoheader = curl_slist_append(evoheader,"Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	curl_slist_free_all(lheader);
	user_account();
}


/* 
 * Retrieve evohome user info
 */
void EvohomeClient::user_account()
{
	account_info.clear();
	json_object *j_ret = json_tokener_parse(send_receive_data("/WebAPI/emea/api/v1/userAccount", evoheader).c_str());
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
	json_object *j_tcs = json_tokener_parse(locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].content.c_str());
	json_object *j_list, *j_zone;
	json_object_object_get_ex(j_tcs, "zones", &j_list);
	int l = json_object_array_length(j_list);
	int i;
	for (i=0; i<l; i++)
	{
		j_zone = json_object_array_get_idx(j_list, i);
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].content = json_object_get_string(j_zone);
	}
}

void EvohomeClient::get_temperatureControlSystems(int location, int gateway)
{
	locations[location].gateways[gateway].temperatureControlSystems.clear();
	json_object *j_gw = json_tokener_parse(locations[location].gateways[gateway].content.c_str());

	json_object *j_list, *j_tcs;
	if ( json_object_object_get_ex(j_gw, "temperatureControlSystems", &j_list) )
	{
		int l = json_object_array_length(j_list);
		int i;
		for (i = 0; i < l; i++)
		{
			j_tcs = json_object_array_get_idx(j_list, i);
			locations[location].gateways[gateway].temperatureControlSystems[i].content = json_object_get_string(j_tcs);
		}
	}
}


void EvohomeClient::get_gateways(int location)
{
	locations[location].gateways.clear();
	json_object *j_loc = json_tokener_parse(locations[location].content.c_str());
	json_object *j_list;
	if ( json_object_object_get_ex(j_loc, "gateways", &j_list) )
	{
		int l = json_object_array_length(j_list);
		int i;
		for (i = 0; i < l; i++)
		{
			json_object *j_gw = json_object_array_get_idx(j_list, i);
			locations[location].gateways[i].content = json_object_get_string(j_gw);
			get_temperatureControlSystems(location,i);
		}
	}
}


void EvohomeClient::full_installation()
{
	locations.clear();
	stringstream url;
	url << "/WebAPI/emea/api/v1/location/installationInfo?userId=" << account_info["userId"] << "&includeTemperatureControlSystems=True";

	// evohome interface does not correctly format the json output
	stringstream ss_jdata;
	ss_jdata << "{\"locations\": " << send_receive_data(url.str(), evoheader) << "}";

	json_object *j_fi = json_tokener_parse(ss_jdata.str().c_str());
	json_object *j_list, *j_loc;
	json_object_object_get_ex(j_fi, "locations", &j_list);
	int l = json_object_array_length(j_list);
	int i;
	for (i=0; i<l; i++)
	{
		j_loc = json_object_array_get_idx(j_list, i);
		locations[i].content = json_object_get_string(j_loc);
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
	if ( (locations.size() == 0) || (locations[location].content.length() == 0) )
	{
		return false;
	}

	bool valid_json = true;
	stringstream url;
	url << "/WebAPI/emea/api/v1/location/" << get_locationId(location) << "/status?includeTemperatureControlSystems=True";
	locations[location].status = send_receive_data(url.str(), evoheader);

	// get gateway status
	json_object *j_stat = json_tokener_parse(locations[location].status.c_str());
	json_object *j_gwlist;
	if ( json_object_object_get_ex(j_stat, "gateways", &j_gwlist) )
	{
		int lgw = json_object_array_length(j_gwlist);
		int igw;
		for (igw = 0; igw < lgw; igw++)
		{
			json_object *j_gw = json_object_array_get_idx(j_gwlist, igw);
			locations[location].gateways[igw].status = json_object_get_string(j_gw);

			// get temperatureControlSystem status
			json_object *j_tcslist, *j_tcs;
			if ( json_object_object_get_ex(j_gw, "temperatureControlSystems", &j_tcslist) )
			{
				int ltcs = json_object_array_length(j_tcslist);
				int itcs;
				for (itcs = 0; itcs < ltcs; itcs++)
				{
					j_tcs = json_object_array_get_idx(j_tcslist, itcs);
					locations[location].gateways[igw].temperatureControlSystems[itcs].status = json_object_get_string(j_tcs);
					json_object *j_zlist, *j_zone;
					if ( json_object_object_get_ex(j_tcs, "zones", &j_zlist) )
					{
						int lz = json_object_array_length(j_zlist);
						int iz;
						for (iz = 0; iz < lz; iz++)
						{
							j_zone = json_object_array_get_idx(j_zlist, iz);
							locations[location].gateways[igw].temperatureControlSystems[itcs].zones[iz].status = json_object_get_string(j_zone);
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
	for (i=0; i<l; i++)
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
	for (d=0; d<7; d++)
	{
		int wday = (ltime.tm_wday + d) % 7;
		std::string s_wday = (std::string)weekdays[wday];
		json_object *j_swd = json_tokener_parse(schedule[s_wday].c_str());
		json_object *j_list, *j_sp, *j_tim;
		json_object_object_get_ex( j_swd, "switchpoints", &j_list);
		int l = json_object_array_length(j_list);
		int i;
		for (i=0; i<l; i++)
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

	json_object *j_loc = json_tokener_parse(locations[location].content.c_str());
	json_object *j_inf, *j_lid;
	if ( ( json_object_object_get_ex( j_loc, "locationInfo", &j_inf )) &&
	     ( json_object_object_get_ex( j_inf, "locationId", &j_lid )) )
	{
		return json_object_get_string(j_lid);
	}
	return "";
}


std::string EvohomeClient::json_get_val(std::string s_json, std::string key)
{
	json_object *j_json = json_tokener_parse(s_json.c_str());
	json_object *j_res;

	if (json_object_object_get_ex(j_json, key.c_str(), &j_res))
		return json_object_get_string(j_res);
	return "";
}



