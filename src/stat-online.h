
#include <iostream>
#include <map>
#include <vector>

struct online_t{
    int32_t businessId;
    float hours;
    float hoursPerDay;
    std::string businessName;
    std::string workingHours;
};

void onlineToJsonStr(std::vector<online_t>& online, std::string& _return);
void getOnlineStat(const std::string& business_id_str_list, const std::string& date, const int32_t& days, std::vector<online_t>& _return);

