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
#include <json-c/json.h>
#include <map>


class EvohomeOldClient
{
	private:
	std::map<std::string,std::string> auth_info;
	std::map<std::string,std::string> account_info;
	struct curl_slist *evoheader = NULL;

	void init();
	void login(std::string user, std::string password);


	public:
	struct location
	{
		std::string locationId;
		json_object *installationInfo;
		json_object *status;
	};

	std::map<int, location> locations;
	std::map<std::string,std::string> sys_info;
	std::map<std::string,std::string> schedule;

	EvohomeOldClient(std::string user, std::string password);
	void cleanup();

	std::string send_receive_data(std::string url, curl_slist *header);
	std::string send_receive_data(std::string url, std::string postdata, curl_slist *header);

	void full_installation();
};

#endif
