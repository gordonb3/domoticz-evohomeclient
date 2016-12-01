/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Evohome
 *
 *
 *
 */

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
	std::string zoneId;
	json_object *installationInfo;
	json_object *status;
	json_object *schedule;
};

class evo_temperatureControlSystem
{
	public:
	std::string systemId;
	json_object *installationInfo;
	json_object *status;
	std::map<int,evo_zone> zones;
};

class evo_gateway
{
	public:
	std::string gatewayId;
	json_object *installationInfo;
	json_object *status;
	std::map<int,evo_temperatureControlSystem> temperatureControlSystems;
};


class evo_location
{
	public:
	std::string locationId;
	json_object *installationInfo;
	json_object *status;
	std::map<int,evo_gateway> gateways;
};




class EvohomeClient
{
	private:
	std::map<std::string,std::string> auth_info;
	std::map<std::string,std::string> account_info;
	struct curl_slist *evoheader;

	std::string send_receive_data(std::string url, curl_slist *header);
	std::string send_receive_data(std::string url, std::string postdata, curl_slist *header);
	std::string put_receive_data(std::string url, std::string postdata, curl_slist *header);

	void init();
	void login(std::string user, std::string password);
	void user_account();

	void get_gateways(int location);
	void get_temperatureControlSystems(int location, int gateway);
	void get_zones(int location, int gateway, int temperatureControlSystem);

	void set_schedule(std::string zoneId, std::string zoneType, json_object *schedule);

	public:
	std::map<int,evo_location> locations;
	std::map<std::string,std::string> sys_info;
	std::map<std::string,std::string> schedule;


	EvohomeClient(std::string user, std::string password);
	void cleanup();

	void full_installation();
	bool get_status(int location);


	int get_location_by_ID(std::string locationId);
	int get_gateway_by_ID(int location, std::string gatewayId);
	int get_temperatureControlSystem_by_ID(int location, int gateway, std::string systemId);
	int get_zone_by_ID(int location, int gateway, int temperatureControlSystem, std::string systemId);


	bool has_dhw(int location, int gateway, int temperatureControlSystem);
	bool has_dhw(evo_temperatureControlSystem *tcs);


	bool schedules_backup(std::string filename);
	bool schedules_restore(std::string filename);
	bool read_schedules_from_file(std::string filename);
	void get_schedule(std::string zoneId);
	void set_zone_schedule(std::string zoneId, json_object *schedule);
	void set_dhw_schedule(std::string zoneId, json_object *schedule);
	std::string get_next_switchpoint(std::string zoneId);


	bool set_system_mode(std::string systemId, int mode, std::string date_until);
	bool set_system_mode(std::string systemId, std::string mode, std::string date_until);
	bool set_system_mode(std::string systemId, std::string mode);


	std::string json_get_val(std::string s_json, std::string key);
	std::string json_get_val(json_object *j_json, std::string key);
	std::string json_get_val(std::string s_json, std::string key1, std::string key2);
	std::string json_get_val(json_object *j_json, std::string key1, std::string key2);

};

#endif
