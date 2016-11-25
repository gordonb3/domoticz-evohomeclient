#include <string>
#include <curl/curl.h>
#include <map>



std::string web_send_receive_data(std::string connection, std::string url, curl_slist *header);
std::string web_send_receive_data(std::string connection, std::string url, std::string postdata, curl_slist *header);

void web_connection_init(std::string connection);
void web_connection_cleanup(std::string connection);

std::string urlencode(std::string str);

