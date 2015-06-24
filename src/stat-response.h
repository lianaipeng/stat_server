
#include<iostream>
#include<map>
#include<vector>

struct response_t{
    int32_t userCount;
    int32_t businessId;
    std::string businessName;
    float firstResHours;
    float aveResHours;
};

struct oneResponse_t{
    int32_t userId;
	int32_t startTime;
	int32_t endTime;
    bool  repState;
};

void getResponseStat(const std::string& business_id_str_list, const std::string& date, const int32_t& days, std::vector<response_t>& _return);
void responseToJsonStr(std::vector<response_t>& response, std::string& _return);
