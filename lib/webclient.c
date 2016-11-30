#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <malloc.h>
#include <cstring>

#include "webclient.h"


using namespace std;

/************************************************************************
 *									*
 *	Keep track of multiple Curl connections				*
 *									*
 *	Original version 2016-11-25 by gordon@bosvangennip.nl		*
 *									*
 ************************************************************************/

CURLcode res;
std::string userAgent;
std::map<std::string, CURL*> curl_connections;

struct curl_data_st
{
	char *payload;
	size_t size;
};


/*
 * Curl write back function
 */
size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t realsize = size * nmemb;
	struct curl_data_st *data = (struct curl_data_st*)userdata;
	data->payload = (char*)realloc(data->payload, data->size + realsize + 1);
	if (data->payload == NULL)
		return 0;
	memcpy(&(data->payload[data->size]), ptr, realsize);
	data->size += realsize;
	data->payload[data->size] = '\0';
	return realsize;
}


/*
 * Initialize curl web client and do a curl global initialize if it is the first connection
 */
void web_connection_init(std::string connection)
{
	CURL *conn;
	if (curl_connections.size() == 0)
	{
		curl_global_init(CURL_GLOBAL_SSL);
	}
	conn = curl_easy_init();
	if ( ! conn )
	{
		cerr << "Unable to initiate libcurl !";
		exit(1);
	}
	curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
	stringstream ss_ua;
	ss_ua << "libcurl-agent/" << info->version;
	userAgent = ss_ua.str();
	curl_connections[connection] = conn;
}


/*
 * Cleanup curl web client
 */
void web_connection_cleanup(std::string connection)
{
	CURL *conn = curl_connections[connection];
	curl_easy_cleanup(conn);
	curl_connections.erase(connection);
	if (curl_connections.size() == 0)
	{
		curl_global_cleanup();
	}
}


/*
 * Panic action: cleanup all curl web clients and do a global cleanup
 */
void web_kill_all()
{
	for (std::map<std::string, CURL*>::iterator it=curl_connections.begin(); it!=curl_connections.end(); ++it)
		curl_easy_cleanup(it->second);
	curl_connections.clear();
	curl_global_cleanup();
}


/*
 * Execute web query
 */
std::string web_send_receive_data(std::string connection, std::string url, curl_slist *header)
{
	return web_send_receive_data(connection, url, "", header);
}
std::string web_send_receive_data(std::string connection, std::string url, std::string postdata, curl_slist *header)
{
	CURL *conn = curl_connections[connection];
	curl_easy_reset(conn);
	curl_easy_setopt(conn, CURLOPT_USERAGENT, userAgent.c_str());
	curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, curl_write_cb);
	res = curl_easy_setopt(conn, CURLOPT_HTTPHEADER, header);
	struct curl_data_st result;
	result.payload = (char*)malloc(1);
	result.size = 0;
	curl_easy_setopt(conn, CURLOPT_WRITEDATA, (void *)&result);
	if (postdata.length()>0)
		curl_easy_setopt(conn, CURLOPT_POSTFIELDS,postdata.c_str());

	curl_easy_setopt(conn, CURLOPT_URL, url.c_str());
	res = curl_easy_perform(conn);

	if(res != CURLE_OK)
	{
		cerr << "ERROR: Could not connect to " << connection << " server, client responds: " << curl_easy_strerror(res) << endl;
		free(result.payload);
		web_kill_all();
		exit(1);
	}

	// need to free curl result before returning
	stringstream rdata;
	rdata << result.payload;
	free(result.payload);
	return rdata.str();
}


std::string urlencode(std::string str)
{
	char c;
	unsigned int i;
	stringstream ss;
	ss << hex;
	for (i=0; i<str.length(); i++)
	{
		c = str[i];
		if (c == '-' || c == '_' || c == '.' || c == '~')
		{
			ss << c;
			continue;
		}
		if ( (c >= 0x30) && (c < 0x3A) )
		{
			ss << c;
			continue;
		}
		if ( ((c|0x20) > 0x60) && ((c|0x20) < 0x7b) )
		{
			ss << c;
			continue;
		}
		if (c < 0x10)
			ss << uppercase << "%0" <<  int((unsigned char) c)<< nouppercase;
		else
			ss << uppercase << '%' <<  int((unsigned char) c)<< nouppercase;
	}
	return ss.str();
}

