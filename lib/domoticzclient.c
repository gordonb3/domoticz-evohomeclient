/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Domoticz
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <malloc.h>
#include <cstring>
#include <ctime>
#include "webclient.h"
#include "domoticzclient.h"
#include "base64.h"

using namespace std;



/*
 * Class construct
 */
DomoticzClient::DomoticzClient(std::string host)
{
	domoticzhost = host;
	domoticzheader = NULL;
	domoticzheader = curl_slist_append(domoticzheader,"Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	domoticzheader = curl_slist_append(domoticzheader,"content-type: application/json");
	domoticzheader = curl_slist_append(domoticzheader,"charsets: utf-8");
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

	stringstream ss_url;
	ss_url << domoticzhost << url;
	return web_send_receive_data("domoticz", ss_url.str(), postdata, domoticzheader);
}


/************************************************************************
 *									*
 *	Private tools							*
 *									*
 ************************************************************************/

int _json_find_object_index;
std::string _json_find_object_key;
std::string _json_find_object_needle;


std::string _int_to_string(int myint)
{
	stringstream ss;
	ss << myint;
	return ss.str();
}


bool _json_cmp_val(json_object *input, std::string key, std::string val)
{
	json_object *j_res;
	return ( (json_object_object_get_ex(input, key.c_str(), &j_res)) && (strcmp(json_object_get_string(j_res),val.c_str())==0) );
}


std::string _json_get_val(json_object *input, std::string key)
{
	json_object *j_res;
	if (json_object_object_get_ex(input, key.c_str(), &j_res))
		return json_object_get_string(j_res);
	return "";
}

bool _json_find_object(json_object *array, json_object **result, std::string key, std::string needle)
{
	json_object *j_val;
	_json_find_object_key = key;
	_json_find_object_needle = needle;
	int l = json_object_array_length(array);
	int i = 0;
	while (i < l)
	{
		*result = json_object_array_get_idx(array, i);
		i++;
		if ( (json_object_object_get_ex(*result, key.c_str(), &j_val)) &&
		     (strcmp(json_object_get_string(j_val),needle.c_str())==0) )
		{
			_json_find_object_index = i;
			return true;
		}
	}
	return false;
}

bool _json_find_next_object(json_object *array, json_object **result)
{
	json_object *j_val;
	std::string key = _json_find_object_key;
	std::string needle = _json_find_object_needle;
	int l = json_object_array_length(array);
	int i = _json_find_object_index;
	while (i < l)
	{
		*result = json_object_array_get_idx(array, i);
		i++;
		if ( (json_object_object_get_ex(*result, key.c_str(), &j_val)) &&
		     (strcmp(json_object_get_string(j_val),needle.c_str())==0) )
		{
			_json_find_object_index = i;
			return true;
		}
	}
	return false;

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
	json_object *j_res = json_tokener_parse(send_receive_data("/json.htm?type=hardware").c_str());
	json_object *j_list, *j_hw;
	if ( ! json_object_object_get_ex(j_res, "result", &j_list) )
		return -1;
	int l = json_object_array_length(j_list);
	int i;
	for (i = 0; i < l; i++)
	{
		j_hw = json_object_array_get_idx(j_list, i);
		if (_json_get_val(j_hw, "Type") == hw_type.c_str())
			if ( (hw_name == "") || (_json_get_val(j_hw, "Name") == hw_name.c_str()) )
				return atoi( _json_get_val(j_hw, "idx").c_str() );
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
	stringstream ss;
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
	json_object *j_res = json_tokener_parse(send_receive_data("/json.htm?type=devices&displayhidden=1&used=all").c_str());
	json_object *j_list, *j_dev;
	if ( ! json_object_object_get_ex(j_res, "result", &j_list) )
		return false;

	std::string evohome_id, used, index;
	int i = 0;

	if ( _json_find_object(j_list, &j_dev, "HardwareID", hwid.c_str()) )
	{
		used = _json_get_val(j_dev, "Used");
		if (used != "0")
			evohome_id = _json_get_val(j_dev, "ID");
		else
		{
			evohome_id = _int_to_string(i);
			i++;
		}
		devices[evohome_id].ID = evohome_id;
		devices[evohome_id].idx = _json_get_val(j_dev, "idx");
		devices[evohome_id].SubType = _json_get_val(j_dev, "SubType");
		devices[evohome_id].Name = _json_get_val(j_dev, "Name");
		devices[evohome_id].Temp = _json_get_val(j_dev, "Temp");
	}


	while ( _json_find_next_object(j_list, &j_dev) )
	{
		used = _json_get_val(j_dev, "Used");
		if (used != "0")
			evohome_id = _json_get_val(j_dev, "ID");
		else
		{
			evohome_id = _int_to_string(i);
			i++;
		}
		devices[evohome_id].ID = evohome_id;
		devices[evohome_id].idx = _json_get_val(j_dev, "idx");
		devices[evohome_id].SubType = _json_get_val(j_dev, "SubType");
		devices[evohome_id].Name = _json_get_val(j_dev, "Name");
		devices[evohome_id].Temp = _json_get_val(j_dev, "Temp");
	}
	return true;
}


/*
 * Create Evohome device
 */
void DomoticzClient::create_evohome_device(int hwid, std::string devicetype)
{
	create_evohome_device(hwid, atoi(devicetype.c_str()));
}
void DomoticzClient::create_evohome_device(std::string hwid, int devicetype)
{
	create_evohome_device(atoi(hwid.c_str()), devicetype);
}
void DomoticzClient::create_evohome_device(std::string hwid, std::string devicetype)
{
	create_evohome_device(atoi(hwid.c_str()), atoi(devicetype.c_str()));
}
void DomoticzClient::create_evohome_device(int hwid, int devicetype)
{
	stringstream ss;
	ss << "/json.htm?type=createevohomesensor&idx=" << hwid << "&sensortype=" << devicetype;
	send_receive_data(ss.str());
}



void DomoticzClient::update_system_dev(std::string idx, std::string systemId, std::string modelType, std::string setmode_script)
{
	int smslen = setmode_script.length();
	int encoded_data_length = Base64encode_len(smslen);
	char* base64_string = (char*)malloc(encoded_data_length);
	Base64encode(base64_string, setmode_script.c_str(), smslen);
	stringstream ss;
	ss << "/json.htm?type=setused&idx=" << idx << "&deviceid=" << systemId << "&used=true&name=" << modelType << "&strparam1=" << base64_string;
	send_receive_data(ss.str());
}


void DomoticzClient::update_system_mode(std::string idx, std::string currentmode)
{
	stringstream ss;
	ss << "/json.htm?type=command&param=switchmodal&idx=" << idx << "&status=" << currentmode << "&action=0&ooc=1";
	send_receive_data(ss.str());
}



void DomoticzClient::update_zone_dev(std::string idx, std::string dhwId, std::string dev_name, std::string setdhw_script)
{
	int smslen = setdhw_script.length();
	int encoded_data_length = Base64encode_len(smslen);
	char* base64_string = (char*)malloc(encoded_data_length);
	Base64encode(base64_string, setdhw_script.c_str(), smslen);
	stringstream ss;
	ss << "/json.htm?type=setused&idx=" << idx << "&deviceid=" << dhwId << "&used=true&name=" << dev_name << "&strparam1=" << base64_string;
	send_receive_data(ss.str());
}


void DomoticzClient::update_zone_status(std::string idx, std::string temperature, std::string state, std::string zonemode, std::string until)
{
	stringstream ss;
	ss << "/json.htm?type=command&param=udevice&idx=" << idx << "&nvalue=0&svalue=" << temperature << ";" << state << ";" << zonemode << ";" << until; 
	send_receive_data(ss.str());
}



