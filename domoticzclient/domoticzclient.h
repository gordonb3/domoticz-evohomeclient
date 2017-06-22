/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Domoticz
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _DomoticzClient
#define _DomoticzClient

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <json-c/json.h>
#include <map>


struct domoticz_device
{
	public:
	std::string SubType;
	std::string idx;
	std::string Name;
	std::string ID;
	std::string Temp;
};

class DomoticzClient
{

	private:
	std::string domoticzhost;
	curl_slist *domoticzheader;

	void init();
	std::string send_receive_data(std::string url);
	std::string send_receive_data(std::string url, std::string postdata);


	public:
	std::map<std::string,domoticz_device> devices;

	DomoticzClient(std::string host);
	void cleanup();

	int get_hwid(int hw_type);
	int get_hwid(std::string hw_type);
	int get_hwid(int hw_type, std::string hw_name);
	int get_hwid(std::string hw_type, std::string hw_name);

	bool get_devices(int hwid);
	bool get_devices(std::string hwid);

	int create_hardware(int hwtype, std::string hwname);
	int create_hardware(std::string hwtype, std::string hwname);

	void create_evohome_device(int hwid, std::string devicetype);
	void create_evohome_device(std::string hwid, int devicetype);
	void create_evohome_device(std::string hwid, std::string devicetype);
	void create_evohome_device(int hwid, int devicetype);

	void update_system_dev(std::string idx, std::string systemId, std::string modelType, std::string setmode_script);
	void update_system_mode(std::string idx, std::string currentmode);

	void update_zone_dev(std::string idx, std::string zoneId, std::string dev_name, std::string settemp_script);
	void update_zone_status(std::string idx, std::string temperature, std::string state, std::string zonemode, std::string until);
};

#endif
