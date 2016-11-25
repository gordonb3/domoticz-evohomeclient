#ifndef _EvohomeClient
#define _EvohomeClient

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <json-c/json.h>
#include <map>


class evo_zone
{
	public:
	std::string content;
	std::string status;
};

class evo_temperatureControlSystem
{
	public:
	std::string content;
	std::string status;
	std::map<int,evo_zone> zones;
};

class evo_gateway
{
	public:
	std::string content;
	std::string status;
	std::map<int,evo_temperatureControlSystem> temperatureControlSystems;
};


class evo_location
{
	public:
	std::string content;
	std::string status;
	std::map<int,evo_gateway> gateways;
};




class EvohomeClient
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




	EvohomeClient(std::string user, std::string password);
	void cleanup();




	std::string send_receive_data(std::string url, curl_slist *header);
	std::string send_receive_data(std::string url, std::string postdata, curl_slist *header);

	void login(std::string user, std::string password);
	void user_account();


	void full_installation();
	void get_gateways(int location);
	void get_temperatureControlSystems(int location, int gateway);
	void get_zones(int location, int gateway, int temperatureControlSystem);

	bool get_status(int location);

	std::string json_get_val(std::string s_json, std::string key);
	std::string get_locationId(unsigned int location);

	void get_schedule(std::string zoneId);
	std::string get_next_switchpoint(std::string zoneId);
};


#endif
