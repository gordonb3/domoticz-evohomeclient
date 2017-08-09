/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Helper to keep track of multiple Curl connections
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <cstring>
#include <map>
#include "webclient.h"
#include <curl/curl.h>
#include <stdexcept>


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
	if (curl_connections.find(connection) != curl_connections.end()) // connection already exists
		return;
	CURL *conn;
	if (curl_connections.size() == 0)
	{
#ifdef _WIN32
		curl_global_init(CURL_GLOBAL_WIN32);
#else
		curl_global_init(CURL_GLOBAL_SSL);
#endif
	}
	conn = curl_easy_init();
	if ( ! conn )
		throw std::runtime_error( "Failed to instantiate libcurl !" );
	curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
	std::stringstream ss_ua;
	ss_ua << "libcurl-agent/" << info->version;
	userAgent = ss_ua.str();
	curl_connections[connection] = conn;
}


/*
 * Cleanup curl web client
 */
void web_connection_cleanup(std::string connection)
{
	if (curl_connections.find(connection) == curl_connections.end()) // connection does not exist
		return;
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
std::string web_send_receive_data(std::string connection, std::string url, std::vector<std::string> &header)
{
	return web_send_receive_data(connection, url, "", header, "POST");
}
std::string web_send_receive_data(std::string connection, std::string url, std::string postdata, std::vector<std::string> &header)
{
	return web_send_receive_data(connection, url, postdata, header, "POST");
}
std::string web_send_receive_data(std::string connection, std::string url, std::string postdata, std::vector<std::string> &header, std::string method)
{
	if (curl_connections.find(connection) == curl_connections.end()) // connection does not exist
	{
		throw std::invalid_argument( "curl connection is not initialized" );
	}

	struct curl_slist *httpheader = NULL;
	if (header.size() > 0)
	{
		std::vector<std::string>::const_iterator itt;
		for (itt = header.begin(); itt != header.end(); ++itt)
		{
			httpheader = curl_slist_append(httpheader, (*itt).c_str());
		}
	}

	CURL *conn = curl_connections[connection];
	curl_easy_reset(conn);
	curl_easy_setopt(conn, CURLOPT_USERAGENT, userAgent.c_str());
	curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, curl_write_cb);
	res = curl_easy_setopt(conn, CURLOPT_HTTPHEADER, httpheader);
	struct curl_data_st result;
	result.payload = (char*)malloc(1);
	result.size = 0;
	curl_easy_setopt(conn, CURLOPT_WRITEDATA, (void *)&result);
	if (postdata.length()>0)
	{
		curl_easy_setopt(conn, CURLOPT_POSTFIELDS,postdata.c_str());
		if (method != "POST")
			curl_easy_setopt(conn, CURLOPT_CUSTOMREQUEST, method.c_str());
	}
	curl_easy_setopt(conn, CURLOPT_URL, url.c_str());
	res = curl_easy_perform(conn);
	curl_slist_free_all(httpheader);

	if (res != CURLE_OK)
	{
		free(result.payload);
		std::stringstream ss_err;
		ss_err << "Failed to connect to " << connection << " server, client responds: " << curl_easy_strerror(res);
		throw std::invalid_argument( ss_err.str() );
	}

	// need to free curl result before returning
	std::stringstream rdata;
	rdata << result.payload;
	free(result.payload);
	return rdata.str();
}


std::string urlencode(std::string str)
{
	char c;
	unsigned int i;
	std::stringstream ss;
	ss << std::hex;
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
			ss << std::uppercase << "%0" << int((unsigned char) c) << std::nouppercase;
		else
			ss << std::uppercase << '%' << int((unsigned char) c) << std::nouppercase;
	}
	return ss.str();
}

