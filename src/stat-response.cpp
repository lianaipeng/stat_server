
#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h> 
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <map>
#include <vector>
#include <sstream>

#include <json/json.h>

#include "config.h"
#include "stat-response.h"
#include "stat-oplog.h"
#include "util-date.h"
#include "util-mem.h"
#include "util-str.h"
#include "shop.h"
#include "config.h"

static void displayOplog(oplog_t& oplog){
    std::cout<<__func__<<" oplog.op = "<<oplog.op<<std::endl;
    std::cout<<__func__<<" oplog.src_role = "<<oplog.src_role<<std::endl;
    std::cout<<__func__<<" oplog.src_role_id = "<<oplog.src_role_id<<std::endl;
    std::cout<<__func__<<" oplog.dst_role = "<<oplog.dst_role<<std::endl;
    std::cout<<__func__<<" oplog.dst_role_id = "<<oplog.dst_role_id<<std::endl;
    std::cout<<__func__<<" oplog.business_id = "<<oplog.business_id<<std::endl;
    std::cout<<__func__<<" oplog.timestamp = "<<oplog.timestamp<<std::endl;
}

static float getFirstTimeAveResHoursForOneBusiness(std::set<int32_t>& setUserId, std::vector<oplog_t>& vecOplog){
    
    if(setUserId.size()==0 || vecOplog.size()==0){
        return 0;
    }
    float totalSeconds = 0;
    int32_t count = setUserId.size();
    for(std::set<int32_t>::iterator iterSetUserId=setUserId.begin(); iterSetUserId!=setUserId.end(); iterSetUserId++){
        bool gotReq = false;
        bool gotRep = false;
        int32_t req = 0;
        int32_t rep = 0;
        for(std::vector<oplog_t>::iterator iterVecOplog=vecOplog.begin(); iterVecOplog!=vecOplog.end(); iterVecOplog++){
            if(!gotReq){
                if(iterVecOplog->src_role==0 && iterVecOplog->src_role_id==(*iterSetUserId)){
                    req = iterVecOplog->timestamp;
                    rep = iterVecOplog->timestamp;
                    //std::cout<<__func__<<" req timestamp:"<<req<<std::endl;
                    gotReq = true;
                }
            }else{
                if(iterVecOplog->src_role==1 && iterVecOplog->dst_role_id==(*iterSetUserId)){
                    rep = iterVecOplog->timestamp;
                    //std::cout<<__func__<<" rep timestamp:"<<rep<<std::endl;
                    gotRep=true;
                    break;
                }
            }
        }
        if(gotRep==false){
            rep = vecOplog[vecOplog.size()-1].timestamp;
        }
        totalSeconds += (float)(rep-req);
        //std::cout<<__func__<<" totalSeconds:"<<totalSeconds<<std::endl;
    }
    //std::cout<<__func__<<" (totalSeconds/3600)/count:"<<((totalSeconds/3600)/count)<<std::endl;
    return (totalSeconds/3600)/count;
}

static float getAveResHoursForOneBusiness(std::set<int32_t>& setUserId, std::vector<oplog_t>& vecOplog){
    if(setUserId.size()==0 || vecOplog.size()==0){
        return 0;
    }
    float totalSeconds = 0;
    int32_t count = setUserId.size();
    for(std::set<int32_t>::iterator iterSetUserId=setUserId.begin(); iterSetUserId!=setUserId.end(); iterSetUserId++){
        bool gotReq = false;
        bool gotRep = false;
        int32_t req = 0;
        int32_t rep = 0;
        int32_t times = 0;
        for(std::vector<oplog_t>::iterator iterVecOplog=vecOplog.begin(); iterVecOplog!=vecOplog.end(); iterVecOplog++){
            if(!gotReq){
                if(iterVecOplog->src_role==0 && iterVecOplog->src_role_id==(*iterSetUserId)){
                    req = iterVecOplog->timestamp;
                    rep = iterVecOplog->timestamp;
                    //std::cout<<__func__<<" req timestamp:"<<req<<std::endl;
                    gotReq = true;
                }
            }else{
                if(iterVecOplog->src_role==1 && iterVecOplog->dst_role_id==(*iterSetUserId)){
                    //std::cout<<__func__<<" rep timestamp:"<<rep<<std::endl;
                    rep = iterVecOplog->timestamp;
                    times++;
                    gotRep=true;
                }
            }
        }
        if(gotRep==false){
            rep = vecOplog[vecOplog.size()-1].timestamp;
            totalSeconds += (float)(rep-req);
            //std::cout<<__func__<<" totalSeconds:"<<totalSeconds<<std::endl;
        }else{
            totalSeconds += ((float)(rep-req))/times;
            //std::cout<<__func__<<" totalSeconds:"<<totalSeconds<<std::endl;
        }
        times = 0;
    }
    //std::cout<<__func__<<" totalSeconds/3600/count:"<<((totalSeconds/3600)/count)<<std::endl;
    return (totalSeconds/3600)/count;
}

void getResponseStat(const std::string& business_id_str_list, const std::string& date, const int32_t& days, std::vector<response_t>& _return){

    sql::Driver*     driver;
    sql::Connection* conn;
    sql::Statement*  stmt;
    sql::ResultSet*  res;

    int32_t startTime;
    int32_t endTime;

    getTsRangeBackward(date, days, startTime, endTime);

    std::vector<int32_t> vecBusinessId;
    std::vector<int32_t> vecBusinessIdNoOplog;
    std::set<int32_t> setCustomerId;
    split(vecBusinessId, business_id_str_list, ",");
    std::map<int32_t, int32_t> mapCustomerIdToBusinessId;

    std::copy(vecBusinessId.begin(), vecBusinessId.end(), std::back_inserter(vecBusinessIdNoOplog));

    for(uint32_t i=0; i<vecBusinessId.size(); i++){
        std::vector<service_t> tmp;
        get_customer_list(getConfig("shop.url"), boost::lexical_cast<std::string>(vecBusinessId[i]), "0", tmp);
        for(uint32_t j=0; j<tmp.size(); j++){
            setCustomerId.insert(boost::lexical_cast<int32_t>(tmp[j].service_id));
            mapCustomerIdToBusinessId[boost::lexical_cast<int32_t>(tmp[j].service_id)] = vecBusinessId[i];
        }
    }

    std::string strCustomerId;
    intSetToStr(setCustomerId, strCustomerId);
    std::map<int32_t, int32_t> mapUserIdToBusinessId;

    std::map<int32_t, std::vector<oplog_t>> mapBusinessIdToOplog;
    std::map<int32_t, std::vector<oplog_t>>::iterator iterMapBusinessToOplog;

    try{

        driver = get_driver_instance();
        conn = driver->connect(getConfig("mysql.uri"), getConfig("mysql.username"), getConfig("mysql.password"));
        conn->setSchema(getConfig("mysql.schema"));

        sql::PreparedStatement *prep_stmt = conn->prepareStatement("set names utf8mb4");
        prep_stmt->executeQuery();
        stmt = conn->createStatement();

        std::stringstream cmd;
        cmd<<"select op, src_role, src_role_id, dst_role, dst_role_id, business_id, UNIX_TIMESTAMP(ts) from oplog where (((src_role=0) and business_id in ("<<business_id_str_list<<")) or ((src_role=1) and (src_role_id in ("<<strCustomerId<<")))) and (op=3) and (ts>FROM_UNIXTIME("<<startTime<<") and ts<FROM_UNIXTIME("<<endTime<<")) order by ts;";
        std::cout<<__func__<<" sql cmd: "<<cmd.str()<<std::endl;

        res=stmt->executeQuery(cmd.str());
        while(res->next()){
            oplog_t oplog;
            oplog.op = res->getInt(1);
            oplog.src_role = res->getInt(2);
            oplog.src_role_id = res->getInt(3);
            oplog.dst_role = res->getInt(4);
            oplog.dst_role_id = res->getInt(5);
            oplog.business_id = res->getInt(6);
            oplog.timestamp = res->getInt(7);
            //displayOplog(oplog);
            if (oplog.src_role == 0){
                mapBusinessIdToOplog[oplog.business_id].push_back(oplog);
            }else if (oplog.src_role == 1){
                mapBusinessIdToOplog[mapCustomerIdToBusinessId[oplog.src_role_id]].push_back(oplog);
            }
        }
        for(iterMapBusinessToOplog=mapBusinessIdToOplog.begin(); iterMapBusinessToOplog!=mapBusinessIdToOplog.end(); iterMapBusinessToOplog++){
            //std::cout<<"business_id = "<<iterMapBusinessToOplog->first<<std::endl;
            std::set<int32_t> setUserId;
            for(std::vector<oplog_t>::iterator iterSetOplog=iterMapBusinessToOplog->second.begin(); iterSetOplog!=iterMapBusinessToOplog->second.end(); iterSetOplog++){
                //displayOplog(*iterSetOplog);
                if(iterSetOplog->src_role==0){
                    setUserId.insert(iterSetOplog->src_role_id);
                    //std::cout<<"user_id = "<<iterSetOplog->src_role_id<<std::endl;
                }
            }
            response_t response;
            response.businessId = iterMapBusinessToOplog->first;
            response.userCount = setUserId.size();
            response.firstResHours = getFirstTimeAveResHoursForOneBusiness(setUserId, iterMapBusinessToOplog->second);
            response.aveResHours = getAveResHoursForOneBusiness(setUserId, iterMapBusinessToOplog->second);
            _return.push_back(response);

            std::cout<<"business_id: "<<std::endl;
            std::vector<int32_t>::iterator iterVecBusinessNoOplog = std::find(vecBusinessIdNoOplog.begin(), vecBusinessIdNoOplog.end(), iterMapBusinessToOplog->first);
            if(iterVecBusinessNoOplog!=vecBusinessIdNoOplog.end()){
                std::cout<<"earsed business_id: "<<iterMapBusinessToOplog->first<<std::endl;
                vecBusinessIdNoOplog.erase(iterVecBusinessNoOplog);
            }
        }

        delete conn;
        delete stmt;
        delete res;
        delete prep_stmt;

    }catch(sql::SQLException &e){
        std::cout<<__func__<<" sql exception: "<<e.what();
    }

    for(std::vector<int32_t>::iterator iterVecBusinessNoOplog=vecBusinessIdNoOplog.begin(); iterVecBusinessNoOplog!=vecBusinessIdNoOplog.end(); iterVecBusinessNoOplog++){
        response_t response;
        response.businessId = *iterVecBusinessNoOplog;
        response.businessName = ""; 
        response.aveResHours = 0;
        response.firstResHours = 0;
        response.userCount = 0; 
        _return.push_back(response);
    } 
}

void responseToJsonStr(std::vector<response_t>& response, std::string& _return){
    Json::Value root;
    //std::cout<<"response.size() = "<<response.size()<<std::endl;
    for(uint32_t i=0; i<response.size(); i++){
        Json::Value item;
        item["business_id"] = boost::lexical_cast<std::string>(response[i].businessId);
        item["business_name"] = response[i].businessName;
        item["first_res_hours"] = boost::lexical_cast<std::string>(response[i].firstResHours);
        item["ave_res_hours"] = boost::lexical_cast<std::string>(response[i].aveResHours);
        item["user_count"] = boost::lexical_cast<std::string>(response[i].userCount);
        root.append(item);
    }
    _return = root.toStyledString();
    //std::cout<<"data = "<<_return<<std::endl;
}
