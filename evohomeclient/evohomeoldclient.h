/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Old Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeOldClient
#define _EvohomeOldClient

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include "jsoncpp/json.h"
#include <map>


class EvohomeOldClient
{
	private:
	void init();

	std::string send_receive_data(std::string url, curl_slist *header);
	std::string send_receive_data(std::string url, std::string postdata, curl_slist *header);

	std::string v1uid;
	struct curl_slist *evoheader;

	public:
	struct location
	{
		std::string locationId;
		Json::Value *installationInfo;
		Json::Value *status;
	};

	EvohomeOldClient(std::string user, std::string password);
	void cleanup();

	bool login(std::string user, std::string password);
	bool full_installation();
	std::string get_zone_temperature(int location, std::string zoneId, int decimals);

	Json::Value j_fi;
	std::map<int, location> locations;
};

#endif
