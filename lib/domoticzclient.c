#include <malloc.h>
#include <cstring>
#include <ctime>
#include "webclient.h"
#include "domoticzclient.h"

using namespace std;

/************************************************************************
 *									*
 *	Json client for Domoticz	 				*
 *									*
 *	Original version 2016-11-25 by gordon@bosvangennip.nl		*
 *									*
 ************************************************************************/

/*
 * Class construct
 */
DomoticzClient::DomoticzClient(std::string host)
{
	domoticzhost = host;
	domoticzheader = NULL;
	domoticzheader = curl_slist_append(domoticzheader,"Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
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
			_json_find_object_index = i + 1;
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

std::string DomoticzClient::get_hwid(int hw_type)
{
	stringstream ss;
	ss << hw_type;
	return get_hwid(ss.str());
}
std::string DomoticzClient::get_hwid(std::string hw_type)
{
	json_object *j_res = json_tokener_parse(send_receive_data("/json.htm?type=hardware").c_str());
	json_object *j_list, *j_hw;
	if ( ! json_object_object_get_ex(j_res, "result", &j_list) )
		return "";
	int l = json_object_array_length(j_list);
	int i;
	for (i = 0; i < l; i++)
	{
		if ( _json_find_object(j_list, &j_hw, "Type", hw_type.c_str()) )
			return _json_get_val(j_hw, "idx");
	}
	return "";
}



/************************************************************************
 *									*
 *	Evohome specific tools						*
 *									*
 ************************************************************************/

bool DomoticzClient::get_evo_devices()
{
	std::string evohome_id;
	json_object *j_res = json_tokener_parse(send_receive_data("/json.htm?type=devices&displayhidden=1&used=all").c_str());
	json_object *j_list, *j_dev;
	if ( ! json_object_object_get_ex(j_res, "result", &j_list) )
		return false;

	if ( _json_find_object(j_list, &j_dev, "HardwareID", get_hwid("40").c_str()) )
	{
		evohome_id = _json_get_val(j_dev, "ID");
		devices[evohome_id].ID = evohome_id;
		devices[evohome_id].idx = _json_get_val(j_dev, "idx");
		devices[evohome_id].SubType = _json_get_val(j_dev, "SubType");
		devices[evohome_id].Name = _json_get_val(j_dev, "Name");
	}


	while ( _json_find_next_object(j_list, &j_dev) )
	{
		evohome_id = _json_get_val(j_dev, "ID");
		devices[evohome_id].ID = evohome_id;
		devices[evohome_id].idx = _json_get_val(j_dev, "idx");
		devices[evohome_id].SubType = _json_get_val(j_dev, "SubType");
		devices[evohome_id].Name = _json_get_val(j_dev, "Name");
	}
	return true;
}

