/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for 'Old' US Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeOldClient
#define _EvohomeOldClient

#include <map>
#include <vector>
#include <string>
#include "jsoncpp/json.h"


class EvohomeOldClient
{
	private:
	void init();

	std::string send_receive_data(std::string url, std::vector<std::string> &header);
	std::string send_receive_data(std::string url, std::string postdata, std::vector<std::string> &header);

	std::string v1uid;
	std::vector<std::string> evoheader;

	public:
	struct location
	{
		std::string locationId;
		Json::Value *installationInfo;
		Json::Value *status;
	};

	EvohomeOldClient();
	EvohomeOldClient(std::string user, std::string password);
	void cleanup();

	bool login(std::string user, std::string password);
	bool full_installation();
	std::string get_zone_temperature(int location, std::string zoneId, int decimals);

	Json::Value j_fi;
	std::map<int, location> locations;
};

#endif
