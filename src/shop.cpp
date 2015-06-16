
#include <iostream>
#include <sstream>

#include <json/json.h>

#include "shop.h"
#include "http-request.h"


int verify_data(const std::string& data,Json::Value& jdata)
{
  Json::Reader reader;
  Json::Value value;
  if(!reader.parse(data, value))
    return -1;

  Json::Value jresponse=value["response"];
  if(!jresponse)
    return -2;
  Json::Value jcode=jresponse["code"];
  std::string code=jcode.asString();
  if(code.empty() or code!="0"){
    return -4;
  }

  jdata=jresponse["data"];
  if(!jdata)
    return -3;
  return 0;
}

int parse_service_resp(std::string& data, std::vector<service_t>& services) 
{
  Json::Value jdata;
  if(verify_data(data,jdata)<0)
    return -1;

  for(int i=0;i<jdata.size();i++){
    Json::Value jservice=jdata[i];
    if(!jservice){
      break;
    }

    Json::Value jserviceid=jservice["service_id"];
    Json::Value jservice_title=jservice["service_title"];
    Json::Value jservice_type=jservice["service_type"];
    Json::Value jservice_nickname=jservice["service_nikename"];
    Json::Value jtoken=jservice["token"];

    service_t service;

    service.service_id=jserviceid.asString();
    service.service_title=jservice_title.asString();
    service.service_type=jservice_type.asString();
    service.service_nickname=jservice_nickname.asString();
    service.token=jtoken.asString();
    if (jservice.isMember("is_agent")) {
        Json::Value jisagent=jservice["is_agent"];
        service.is_agent=jisagent.asBool();
    } else {
        service.is_agent=false;
    }
    services.push_back(service);
  }
    return 0;
}

int get_customer_list(std::string url, std::string business_id, std::string service_type, std::vector<service_t>& services)
{
    std::string post_data = std::string("data={\"type\":\"getkefu\", \"business_id\":") + business_id  + ",\"service_type\":" + service_type + std::string("}");
    std::string page;
    int r = HttpRequest::Post(url, post_data, page);
    //std::cout<<r<<std::endl;
    if(r < 0) return -1;
    //std::cout<<page<<std::endl;
    return parse_service_resp(page, services);
}

