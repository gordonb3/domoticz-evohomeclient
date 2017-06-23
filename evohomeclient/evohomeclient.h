/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for UK/EMEA Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeClient
#define _EvohomeClient

#include "jsoncpp/json.h"
#include <map>
#include <string>

#include <curl/curl.h>


class EvohomeClient
{
	private:
	std::map<std::string,std::string> account_info;

	struct curl_slist *evoheader;
	std::string send_receive_data(std::string url, curl_slist *header);
	std::string send_receive_data(std::string url, std::string postdata, curl_slist *header);
	std::string put_receive_data(std::string url, std::string postdata, curl_slist *header);

	void init();
	bool user_account();

	void get_gateways(int location);
	void get_temperatureControlSystems(int location, int gateway);
	void get_zones(int location, int gateway, int temperatureControlSystem);


	public:
	struct zone
	{
		std::string locationId;
		std::string gatewayId;
		std::string systemId;
		std::string zoneId;
		Json::Value *installationInfo;
		Json::Value *status;
		Json::Value schedule;
		std::string hdtemp;
	};

	struct temperatureControlSystem
	{
		std::string locationId;
		std::string gatewayId;
		std::string systemId;
		Json::Value *installationInfo;
		Json::Value *status;
		std::map<int, zone> zones;
	};

	struct gateway
	{
		std::string locationId;
		std::string gatewayId;
		Json::Value *installationInfo;
		Json::Value *status;
		std::map<int, temperatureControlSystem> temperatureControlSystems;
	};


	struct location
	{
		std::string locationId;
		Json::Value *installationInfo;
		Json::Value *status;
		std::map<int, gateway> gateways;
	};

	std::map<int, location> locations;
	Json::Value j_fi;
	Json::Value j_stat;


	EvohomeClient(std::string user, std::string password);
	void cleanup();

	bool login(std::string user, std::string password);
	bool full_installation();
	bool get_status(int location);
	bool get_status(std::string locationId);

	location* get_location_by_ID(std::string locationId);
	gateway* get_gateway_by_ID(std::string gatewayId);
	temperatureControlSystem* get_temperatureControlSystem_by_ID(std::string systemId);
	zone* get_zone_by_ID(std::string zoneId);
	temperatureControlSystem* get_zone_temperatureControlSystem(zone* zone);

	bool has_dhw(int location, int gateway, int temperatureControlSystem);
	bool has_dhw(temperatureControlSystem *tcs);
	bool is_single_heating_system();

	bool schedules_backup(std::string filename);
	bool schedules_restore(std::string filename);
	bool read_schedules_from_file(std::string filename);
	bool get_schedule(std::string zoneId);
	bool set_schedule(std::string zoneId, std::string zoneType, Json::Value *schedule);
	bool set_zone_schedule(std::string zoneId, Json::Value *schedule);
	bool set_dhw_schedule(std::string zoneId, Json::Value *schedule);

	std::string get_next_switchpoint(temperatureControlSystem* tcs, int zone);
	std::string get_next_switchpoint(zone* hz);
	std::string get_next_switchpoint(std::string zoneId);
	std::string get_next_switchpoint(Json::Value &schedule);
	std::string get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint);
	std::string get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint, int force_weekday);

	bool set_system_mode(std::string systemId, int mode, std::string date_until);
	bool set_system_mode(std::string systemId, int mode);

	bool set_system_mode(std::string systemId, std::string mode, std::string date_until);
	bool set_system_mode(std::string systemId, std::string mode);

	bool set_temperature(std::string zoneId, std::string temperature, std::string time_until);
	bool set_temperature(std::string zoneId, std::string temperature);
	bool cancel_temperature_override(std::string zoneId);

	bool set_dhw_mode(std::string dhwId, std::string mode, std::string time_until);
	bool set_dhw_mode(std::string systemId, std::string mode);

	std::string request_next_switchpoint(std::string zoneId);
};

#endif
