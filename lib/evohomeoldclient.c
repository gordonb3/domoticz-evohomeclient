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
void EvohomeOldClient::login(std::string user, std::string password)
{
	auth_info.clear();
	struct curl_slist *lheader = NULL;
	lheader = curl_slist_append(lheader,"Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	lheader = curl_slist_append(lheader,"content-type: application/json");

	stringstream pdata;
	pdata << "{'Username': '" << user << "', 'Password': '" << password << "'";
	pdata << ", 'ApplicationId': '91db1612-73fd-4500-91b2-e63b069b185c'}";
	json_object *j_ret = json_tokener_parse(send_receive_data("/WebAPI/api/Session", pdata.str(), lheader).c_str());

	json_object_object_foreach(j_ret, key, val)
	{
		if (strcmp(key,"userInfo") == 0)
		{
			json_object_object_foreach(val, key2, val2)
			{
				account_info[key2] = json_object_get_string(val2);
			}
		}
		else
			auth_info[key] = json_object_get_string(val);
	}

	stringstream atoken;
	atoken << "sessionId: " << auth_info["sessionId"];
	evoheader = curl_slist_append(evoheader,atoken.str().c_str());
	evoheader = curl_slist_append(evoheader,"applicationId: 91db1612-73fd-4500-91b2-e63b069b185c");
	evoheader = curl_slist_append(evoheader,"Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	curl_slist_free_all(lheader);
}


/************************************************************************
 *									*
 *	Evohome heating installations					*
 *									*
 ************************************************************************/

void EvohomeOldClient::full_installation()
{
	locations.clear();
	stringstream url;

	url << "/WebAPI/api/locations/?userId=" << account_info["userID"] << "&allData=True";

	// evohome v2 interface does not correctly format the json output
	stringstream ss_jdata;
	ss_jdata << "{\"locations\": " << send_receive_data(url.str(), evoheader) << "}";

	json_object *j_fi = json_tokener_parse(ss_jdata.str().c_str());
	json_object *j_list;
	json_object_object_get_ex(j_fi, "locations", &j_list);
	int l = json_object_array_length(j_list);
	int i;
	for (i=0; i<l; i++)
	{
		locations[i].installationInfo = json_object_array_get_idx(j_list, i);
	}
}


