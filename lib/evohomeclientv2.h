/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Evohome version 2 API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

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
	void login(std::string user, std::string password);


	public:
	std::map<int,evo_location> locations;
	std::map<std::string,std::string> sys_info;
	std::map<std::string,std::string> schedule;

	EvohomeClientV2(std::string user, std::string password);
	void cleanup();

	std::string send_receive_data(std::string url, curl_slist *header);
	std::string send_receive_data(std::string url, std::string postdata, curl_slist *header);

	void full_installation();
};

#endif
