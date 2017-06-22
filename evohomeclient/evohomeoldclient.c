/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Old Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <malloc.h>
#include <cstring>
#include <ctime>
#include "webclient.h"
#include "evohomeoldclient.h"


#define EVOHOME_HOST "https://tccna.honeywell.com"

using namespace std;


/*
 * Class construct
 */
EvohomeOldClient::EvohomeOldClient(std::string user, std::string password)
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
void EvohomeOldClient::init()
{
	web_connection_init("old_evohome");
}


/*
 * Cleanup curl web client
 */
void EvohomeOldClient::cleanup()
{
	web_connection_cleanup("old_evohome");
}

/*
 * Execute web query
 */
std::string EvohomeOldClient::send_receive_data(std::string url, curl_slist *header)
{
	return send_receive_data(url, "", header);
}

std::string EvohomeOldClient::send_receive_data(std::string url, std::string postdata, curl_slist *header)
{

	stringstream ss_url;
	ss_url << EVOHOME_HOST << url;
	return web_send_receive_data("old_evohome", ss_url.str(), postdata, header);
}


/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/


/* 
 * login to evohome web server
 */
bool EvohomeOldClient::login(std::string user, std::string password)
{
	struct curl_slist *lheader = NULL;
	lheader = curl_slist_append(lheader,"Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	lheader = curl_slist_append(lheader,"content-type: application/json");

	stringstream pdata;
	pdata << "{'Username': '" << user << "', 'Password': '" << password << "'";
	pdata << ", 'ApplicationId': '91db1612-73fd-4500-91b2-e63b069b185c'}";
	std::string s_res = send_receive_data("/WebAPI/api/Session", pdata.str(), lheader);

	if (s_res.find("<title>") != std::string::npos) // got an HTML page
	{
		std::cerr << "Login to Evohome server failed - server responds: ";
		int i = s_res.find("<title>");
		char* html = &s_res[i];
		i = 7;
		char c = html[i];
		std::stringstream edata;
		while (c != '<')
		{
			std::cout << c;
			i++;
			c = html[i];
		}
		std::cout << "\n";
		return false;
	}

	if (s_res[0] == '[') // got unnamed array as reply
	{
		s_res[0] = ' ';
		size_t len = s_res.size();
		len--;
		s_res[len] = ' ';
	}

	Json::Value j_login;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_login))
		return false;

	std::string szError = "";
	if (j_login.isMember("error"))
		szError = j_login["error"].asString();
	if (j_login.isMember("message"))
		szError = j_login["message"].asString();
	if (!szError.empty())
	{
		std::cerr << "Login to Evohome server failed with message: " << szError << "\n";
		return false;
	}

	if (!j_login.isMember("sessionId") || !j_login.isMember("userInfo") || !j_login["userInfo"].isObject() || !j_login["userInfo"].isMember("userID"))
	{
		std::cerr << "Login to Evohome server returned unknown data\n";
		return false;
	}

	std::string sessionId = j_login["sessionId"].asString();
	std::stringstream atoken;
	atoken << "sessionId: " << sessionId;
	v1uid = j_login["userInfo"]["userID"].asString();

	evoheader = NULL;
	evoheader = curl_slist_append(evoheader,atoken.str().c_str());
	evoheader = curl_slist_append(evoheader,"applicationId: 91db1612-73fd-4500-91b2-e63b069b185c");
	evoheader = curl_slist_append(evoheader,"Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	curl_slist_free_all(lheader);
	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations					*
 *									*
 ************************************************************************/

bool EvohomeOldClient::full_installation()
{
	locations.clear();
	stringstream url;

	url << "/WebAPI/api/locations/?userId=" << v1uid << "&allData=True";

	// evohome 'old' interface does not correctly format the json output
	stringstream ss_jdata;
	ss_jdata << "{\"locations\": " << send_receive_data(url.str(), evoheader) << "}";
	Json::Reader jReader;
	if (!jReader.parse(ss_jdata.str(), j_fi))
		return false;

	size_t l = 0;
	if (j_fi["locations"].isArray())
		l = j_fi["locations"].size();
	for (size_t i = 0; i < l; ++i)
		locations[i].installationInfo = &j_fi["locations"][(int)(i)];
	return true;
}


