#ifndef _DomoticzClient
#define _DomoticzClient

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <json-c/json.h>
#include <map>

class c_device
{
	public:
	std::string SubType;
	std::string idx;
	std::string Name;
	std::string ID;
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
	std::map<std::string,c_device> devices;


	DomoticzClient(std::string host);
	void cleanup();


	std::string get_hwid(int hw_type);
	std::string get_hwid(std::string hw_type);


	bool get_evo_devices();


};

#endif
