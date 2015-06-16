
#include <iostream>

// 客服结构
struct service_t {
    std::string service_id;
    std::string service_title;
    std::string service_type;
    std::string service_nickname;
    std::string token;
    bool is_agent;
};

int get_customer_list(std::string url, std::string business_id, std::string service_type, std::vector<service_t>& services);
