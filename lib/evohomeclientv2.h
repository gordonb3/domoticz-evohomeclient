#ifndef _EvohomeClientv2
#define _EvohomeClientv2

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <json-c/json.h>
#include <map>


class EvohomeClientV2
{
	private:
	std::map<std::string,std::string> auth_info;
	std::map<std::string,std::string> account_info;
	struct curl_slist *evoheader = NULL;

	void init();


	public:
	std::map<int,evo_location> locations;
	std::map<std::string,std::string> sys_info;
	std::map<std::string,std::string> schedule;




	EvohomeClientV2(std::string user, std::string password);
	void cleanup();




	std::string send_receive_data(std::string url, curl_slist *header);
	std::string send_receive_data(std::string url, std::string postdata, curl_slist *header);

	void login(std::string user, std::string password);
//	void user_account();


	void full_installation();

/*
	void get_gateways(int location);
	void get_temperatureControlSystems(int location, int gateway);
	void get_zones(int location, int gateway, int temperatureControlSystem);

	bool get_status(int location);

	std::string json_get_val(std::string s_json, std::string key);
	std::string get_locationId(unsigned int location);

	void get_schedule(std::string zoneId);
	std::string get_next_switchpoint(std::string zoneId);
*/

};


#endif
