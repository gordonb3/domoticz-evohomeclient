/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Helper to keep track of multiple Curl connections
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _WebClient
#define _WebClient

#include <string>
#include <vector>

std::string web_send_receive_data(std::string connection, std::string url, std::vector<std::string> &header);
std::string web_send_receive_data(std::string connection, std::string url, std::string postdata, std::vector<std::string> &header);
std::string web_send_receive_data(std::string connection, std::string url, std::string postdata, std::vector<std::string> &header, std::string method);

void web_connection_init(std::string connection);
void web_connection_cleanup(std::string connection);

std::string urlencode(std::string str);

#endif
