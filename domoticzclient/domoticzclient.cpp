/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Domoticz
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include "domoticzclient.h"
#include "base64.h"
#include "../evohomeclient/webclient.h"


/*
 * Class construct
 */
DomoticzClient::DomoticzClient(std::string host)
{
	domoticzhost = host;
	domoticzheader.clear();
	domoticzheader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	domoticzheader.push_back("content-type: application/json");
	domoticzheader.push_back("charsets: utf-8");
	init();
}

/************************************************************************
 *									*
 *	Curl helpers							*
 *									*
 ************************************************************************/

/*
 * Initialize curl web client
 */
void DomoticzClient::init()
{
	web_connection_init("domoticz");
}


/*
 * Cleanup curl web client
 */
void DomoticzClient::cleanup()
{
	web_connection_cleanup("domoticz");
}

/*
 * Execute web query
 */
std::string DomoticzClient::send_receive_data(std::string url)
{
	return send_receive_data(url, "");
}

std::string DomoticzClient::send_receive_data(std::string url, std::string postdata)
{

	std::stringstream ss_url;
	ss_url << domoticzhost << url;
	return web_send_receive_data("domoticz", ss_url.str(), postdata, domoticzheader);
}


/************************************************************************
 *									*
 *	Private tools							*
 *									*
 ************************************************************************/

std::string _int_to_string(int myint)
{
	std::stringstream ss;
	ss << myint;
	return ss.str();
}


/************************************************************************
 *									*
 *	Public tools							*
 *									*
 ************************************************************************/

int DomoticzClient::get_hwid(int hw_type)
{
	return get_hwid(_int_to_string(hw_type), "");
}
int DomoticzClient::get_hwid(std::string hw_type)
{
	return get_hwid(hw_type, "");
}
int DomoticzClient::get_hwid(int hw_type, std::string hw_name)
{
	return get_hwid(_int_to_string(hw_type), hw_name);
}
int DomoticzClient::get_hwid(std::string hw_type, std::string hw_name)
{
	std::string s_res = send_receive_data("/json.htm?type=hardware");
	Json::Value j_res;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_res) || !j_res.isMember("result") || !j_res["result"].isArray())
		return false;

	size_t l = j_res["result"].size();
	for (size_t i = 0; i < l; ++i)
	{
		if (j_res["result"][(int)(i)]["Type"].asString() == hw_type)
		{
			if ( (hw_name == "") || (j_res["result"][(int)(i)]["Name"].asString() == hw_name) )
				return std::atoi(j_res["result"][(int)(i)]["idx"].asString().c_str());
		}
	}
	return -1;
}


/*
 *  Create hardware 
 */

int DomoticzClient::create_hardware(int hwtype, std::string hwname)
{
	return create_hardware(_int_to_string(hwtype), hwname);
}
int DomoticzClient::create_hardware(std::string hwtype, std::string hwname)
{
	std::stringstream ss;
	ss << "/json.htm?type=command&param=addhardware&htype=" << hwtype << "&port=1&name=" << hwname << "&enabled=true&datatimeout=0";
	send_receive_data(ss.str());
	return get_hwid(hwtype, hwname);
}


/************************************************************************
 *									*
 *	Evohome specific tools						*
 *									*
 ************************************************************************/

bool DomoticzClient::get_devices(int hwid)
{
	return get_devices(_int_to_string(hwid));
}
bool DomoticzClient::get_devices(std::string hwid)
{
	devices.clear();
	std::string s_res = send_receive_data("/json.htm?type=devices&displayhidden=1&used=all");
	Json::Value j_res;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_res) || !j_res.isMember("result") || !j_res["result"].isArray())
		return false;

	int eid = 0;
	for (size_t i = 0; i < j_res["result"].size(); i++)
	{
		if (j_res["result"][(int)(i)]["HardwareID"].asString() == hwid)
		{
			std::string evohome_id;
			if (j_res["result"][(int)(i)]["Used"].asString() != "0")
				evohome_id = j_res["result"][(int)(i)]["ID"].asString();
			else
			{
				evohome_id = _int_to_string(eid);
				eid++;
			}
			devices[evohome_id].ID = evohome_id;
			devices[evohome_id].idx = j_res["result"][(int)(i)]["idx"].asString();
			devices[evohome_id].SubType = j_res["result"][(int)(i)]["SubType"].asString();
			devices[evohome_id].Name = j_res["result"][(int)(i)]["Name"].asString();
			devices[evohome_id].Temp = j_res["result"][(int)(i)]["Temp"].asString();
		}
	}
	return true;
}


/*
 * Create Evohome device
 */
void DomoticzClient::create_evohome_device(int hwid, std::string devicetype)
{
	create_evohome_device(hwid, std::atoi(devicetype.c_str()));
}
void DomoticzClient::create_evohome_device(std::string hwid, int devicetype)
{
	create_evohome_device(std::atoi(hwid.c_str()), devicetype);
}
void DomoticzClient::create_evohome_device(std::string hwid, std::string devicetype)
{
	create_evohome_device(std::atoi(hwid.c_str()), std::atoi(devicetype.c_str()));
}
void DomoticzClient::create_evohome_device(int hwid, int devicetype)
{
	std::stringstream ss;
	ss << "/json.htm?type=createevohomesensor&idx=" << hwid << "&sensortype=" << devicetype;
	send_receive_data(ss.str());
}



void DomoticzClient::update_system_dev(std::string idx, std::string systemId, std::string modelType, std::string setmode_script)
{
	int smslen = setmode_script.length();
	int encoded_data_length = Base64encode_len(smslen);
	char* base64_string = (char*)malloc(encoded_data_length);
	Base64encode(base64_string, setmode_script.c_str(), smslen);
	std::stringstream ss;
	ss << "/json.htm?type=setused&idx=" << idx << "&deviceid=" << systemId << "&used=true&name=" << modelType << "&strparam1=" << base64_string;
	send_receive_data(ss.str());
}


void DomoticzClient::update_system_mode(std::string idx, std::string currentmode)
{
	std::stringstream ss;
	ss << "/json.htm?type=command&param=switchmodal&idx=" << idx << "&status=" << currentmode << "&action=0&ooc=1";
	send_receive_data(ss.str());
}



void DomoticzClient::update_zone_dev(std::string idx, std::string zoneId, std::string dev_name, std::string settemp_script)
{
	int smslen = settemp_script.length();
	int encoded_data_length = Base64encode_len(smslen);
	char* base64_string = (char*)malloc(encoded_data_length);
	Base64encode(base64_string, settemp_script.c_str(), smslen);
	std::stringstream ss;
	ss << "/json.htm?type=setused&idx=" << idx << "&deviceid=" << zoneId << "&used=true&name=" << urlencode(dev_name) << "&strparam1=" << base64_string;
	send_receive_data(ss.str());
}


void DomoticzClient::update_zone_status(std::string idx, std::string temperature, std::string state, std::string zonemode, std::string until)
{
	std::stringstream ss;
	ss << "/json.htm?type=command&param=udevice&idx=" << idx << "&nvalue=0&svalue=" << temperature << ";" << state << ";" << zonemode << ";" << until; 
	send_receive_data(ss.str());
}



