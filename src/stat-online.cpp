
#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h> 
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <boost/lexical_cast.hpp>

#include <map>
#include <set>
#include <vector>
#include <sstream>
#include <iostream>

#include <json/json.h>

#include "stat-online.h"
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

/* 获取客服初始状态, 在统计区间内，视第一次操作是登出的客服为在线客服
 * true:  在线;
 * false: 离线;
 * */
static bool getCustomerInitState(const int32_t& customerId, std::vector<oplog_t>& vecOplog){
    bool result = false;
    for(std::vector<oplog_t>::iterator iterVecOplog=vecOplog.begin(); iterVecOplog!=vecOplog.end(); iterVecOplog++){
        //displayOplog(*iterVecOplog);
        if((iterVecOplog->src_role==1) && (iterVecOplog->src_role_id == customerId)){
            if(iterVecOplog->op == 1){
                result = false;
                break;
            }else if(iterVecOplog->op == 2){
                result = true;
                break;
            }
        }
    }
    std::cout<<"customerId initial state: "<<(result?"online":"offline")<<std::endl;
    return result;
}

/* 获取在统计区间内,商家起始在线客服人数
 * */
static void getCustomerStateForOneBusiness(std::map<int32_t, bool>& mapCustomerToInitState, std::vector<int32_t>& vecCustomerId, std::vector<oplog_t>& vecOplog){
    for(std::vector<int32_t>::iterator iterVecCustomerId=vecCustomerId.begin(); iterVecCustomerId!=vecCustomerId.end(); iterVecCustomerId++){
        bool state = getCustomerInitState(*iterVecCustomerId, vecOplog);
        mapCustomerToInitState[*iterVecCustomerId] = state;
    }
}

/* 获取在统计区间内,商家起始在线客服人数
 * */
static void getCustomerStateForOneBusiness(std::map<int32_t, bool>& mapCustomerToInitState, const std::set<int32_t>& setCustomerId, std::vector<oplog_t>& vecOplog){
    for(std::set<int32_t>::iterator iterSetCustomerId=setCustomerId.begin(); iterSetCustomerId!=setCustomerId.end();iterSetCustomerId++){
        bool state = getCustomerInitState(*iterSetCustomerId, vecOplog);
        mapCustomerToInitState[*iterSetCustomerId] = state;
    }
}

/* 获取商家在线总时长
 * */
static float getBusinessOnlineHours(int32_t startTimestamp, std::map<int32_t, bool>& mapCustomerToInitState, std::vector<oplog_t>& oplog, online_t& online){

    float workingSeconds = 0;
    int32_t onlineCustomerNumber = 0;
    int32_t start = startTimestamp;
    int32_t end = startTimestamp;

    for(std::map<int32_t, bool>::iterator iterMapCustomerToInitState=mapCustomerToInitState.begin(); iterMapCustomerToInitState!=mapCustomerToInitState.end(); iterMapCustomerToInitState++){
        if(iterMapCustomerToInitState->second == true){
            onlineCustomerNumber++;
        } 
		std::cout << iterMapCustomerToInitState->first << " " << iterMapCustomerToInitState->second << ",";
    }
	std::cout <<std::endl;
	std::cout << "iterMapCustomerToInitState End ^^^^^^^^^ onlineCustomerNumber " << onlineCustomerNumber  <<std::endl;

    std::stringstream log;
    for(std::vector<oplog_t>::iterator iterVecOplog=oplog.begin(); iterVecOplog!=oplog.end(); iterVecOplog++){
        //displayOplog(*iterVecOplog);
        if(iterVecOplog->op == 1){
            if(mapCustomerToInitState[iterVecOplog->src_role_id] == false){ // false 代表登录
                mapCustomerToInitState[iterVecOplog->src_role_id] = true;
                if(onlineCustomerNumber == 0){
                    start = iterVecOplog->timestamp;
                }
                onlineCustomerNumber++;
            }
        } else if (iterVecOplog->op == 2){
            if(mapCustomerToInitState[iterVecOplog->src_role_id] == true){
                mapCustomerToInitState[iterVecOplog->src_role_id] = false;
                onlineCustomerNumber--;
                if(onlineCustomerNumber == 0){
        			end = iterVecOplog->timestamp;  // 记录最新的登出时间
                    workingSeconds += (float)(end - start);
					std::cout << "onlineCustomerNumber " << onlineCustomerNumber << " " << start << " " << end << std::endl;
                    start = end;
                }
            }
        }
        std::string dateStr;
        getDateStrFromTs(iterVecOplog->timestamp, dateStr);
        log<<"[customer:"<<iterVecOplog->src_role_id<<(iterVecOplog->op==1?" login":" logout")<<" "<<dateStr<<"] ";
    }
	std::cout << std::endl;
	/*
    for(std::vector<oplog_t>::iterator iterVecOplog=oplog.begin(); iterVecOplog!=oplog.end(); iterVecOplog++){
        //displayOplog(*iterVecOplog);
        if(iterVecOplog->op == 1){
            if(mapCustomerToInitState[iterVecOplog->src_role_id] == false){
                mapCustomerToInitState[iterVecOplog->src_role_id] = true;
                if(onlineCustomerNumber == 0){
                    start = iterVecOplog->timestamp;
                }
                onlineCustomerNumber++;
            }
        } else if (iterVecOplog->op == 2){
			std::cout << "onlineCustomerNumber " << onlineCustomerNumber << " " << start << std::endl;
            if(mapCustomerToInitState[iterVecOplog->src_role_id] == true){
                mapCustomerToInitState[iterVecOplog->src_role_id] = false;
                onlineCustomerNumber--;
                if(onlineCustomerNumber == 0){
                    workingSeconds += (float)(end - start);
					std::cout << "onlineCustomerNumber " << onlineCustomerNumber << " " << start << " " << end << std::endl;
                    start = end;
                }
            }
        }
        std::string dateStr;
        getDateStrFromTs(iterVecOplog->timestamp, dateStr);
        log<<"[customer:"<<iterVecOplog->src_role_id<<(iterVecOplog->op==1?" login":" logout")<<" "<<dateStr<<"] ";
        	end = iterVecOplog->timestamp;  // 记录最新的登出时间
    }*/
    online.workingHours = log.str(); 
    log.clear();
	std::cout << "@@@@@@ return hours " << (workingSeconds/3600) << std::endl;
    return (workingSeconds/3600);
}

static void getThedayBusinessOnlineHours(std::map<int32_t,std::set<int32_t> > & mapBusinessIdToSetCustomerId,std::vector<int32_t> vecBusinessIdNoOplog,
				std::set<int32_t> setCustomerId,const int32_t theday, sql::Statement* stmt, std::map<int32_t,online_t> & mapBusinessToOnline){
    int32_t startTime = theday;
    int32_t endTime   = theday+86400;
    std::string strCustomerId;
    intSetToStr(setCustomerId, strCustomerId);

    sql::ResultSet*         res       = NULL;
	try{
        std::stringstream cmd;
        cmd<<"select op, src_role, src_role_id, dst_role, dst_role_id, business_id, UNIX_TIMESTAMP(ts) from oplog where (op in (1,2)) and (ts>FROM_UNIXTIME("<<startTime<<") and ts<FROM_UNIXTIME("<<endTime<<")) and (src_role=1 and src_role_id in ("<<strCustomerId<<")) order by ts;";
        std::cout<<__func__<<" sql cmd: "<<cmd.str()<<std::endl;

		std::map<int32_t, std::set<int32_t> >::iterator iterMapBusinessIdToSetCustomerId;
		std::map<int32_t, std::set<int32_t> >::iterator iterMapBusinessIdToSetCustomerIdEnd;
		iterMapBusinessIdToSetCustomerIdEnd = mapBusinessIdToSetCustomerId.end();
		
        std::map<int32_t, std::set<int32_t> > mapBusinessIdToSetOnCustomerId;
        std::map<int32_t, std::set<int32_t> >::iterator iterMapBusinessIdToSetOnCustomerId;
        std::map<int32_t, std::vector<oplog_t> > mapBusinessIdToVecOplog;
        std::map<int32_t, std::vector<oplog_t> >::iterator iterMapBusinessToVecOplog;
        res = stmt->executeQuery(cmd.str());
        while(res->next()){
            oplog_t oplog;
            oplog.op = res->getInt(1);
            oplog.src_role = res->getInt(2);
            oplog.src_role_id = res->getInt(3);
            oplog.dst_role = res->getInt(4);
            oplog.dst_role_id = res->getInt(5);
            oplog.business_id = res->getInt(6);
            oplog.timestamp = res->getInt(7);

			iterMapBusinessIdToSetCustomerId = mapBusinessIdToSetCustomerId.begin();
			while(iterMapBusinessIdToSetCustomerId != iterMapBusinessIdToSetCustomerIdEnd){
				if(iterMapBusinessIdToSetCustomerId->second.find(oplog.src_role_id) != iterMapBusinessIdToSetCustomerId->second.end() ){
					mapBusinessIdToSetOnCustomerId[iterMapBusinessIdToSetCustomerId->first].insert(oplog.src_role_id);
					mapBusinessIdToVecOplog[iterMapBusinessIdToSetCustomerId->first].push_back(oplog);
				}
				iterMapBusinessIdToSetCustomerId++;
			}
        }

		std::cout << "$$$$$$$$$$$$$$$$$$$$$R mapBusinessIdToSetOnCustomerId $$$$$$$$$$$$$$$" << std::endl;
		std::map<int32_t, std::set<int32_t>>::iterator iterBtCSet = mapBusinessIdToSetOnCustomerId.begin();
		while(iterBtCSet != mapBusinessIdToSetOnCustomerId.end()){
			std::cout << "My business Id " << iterBtCSet->first << std::endl;
			std::set<int32_t>::iterator iterCSet = iterBtCSet->second.begin();
			while(iterCSet != iterBtCSet->second.end() ){
				std::cout << *iterCSet << "," ;
				iterCSet++;
			} 
			std::cout << std::endl;
			iterBtCSet++;
		}
		std::cout << "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$" << std::endl;

        for(iterMapBusinessToVecOplog=mapBusinessIdToVecOplog.begin(); iterMapBusinessToVecOplog!=mapBusinessIdToVecOplog.end(); iterMapBusinessToVecOplog++){
            iterMapBusinessIdToSetOnCustomerId = mapBusinessIdToSetOnCustomerId.find(iterMapBusinessToVecOplog->first);
            if(iterMapBusinessIdToSetOnCustomerId != mapBusinessIdToSetOnCustomerId.end()){
                online_t online;
                online.businessId = iterMapBusinessToVecOplog->first;
                std::map<int32_t, bool> mapCustomerToInitState;
                getCustomerStateForOneBusiness(mapCustomerToInitState, iterMapBusinessIdToSetOnCustomerId->second, iterMapBusinessToVecOplog->second);
                float workingHours = getBusinessOnlineHours(startTime, mapCustomerToInitState, iterMapBusinessToVecOplog->second, online);
                online.hours = workingHours;
                online.hoursPerDay = workingHours;
                mapBusinessToOnline[iterMapBusinessToVecOplog->first] = online;

                std::vector<int32_t>::iterator iterVecBusinessNoOplog = std::find(vecBusinessIdNoOplog.begin(), vecBusinessIdNoOplog.end(), iterMapBusinessIdToSetOnCustomerId->first);
                if(iterVecBusinessNoOplog!=vecBusinessIdNoOplog.end()){
                    vecBusinessIdNoOplog.erase(iterVecBusinessNoOplog);
                }
			}	
		}
		
        for(std::vector<int32_t>::iterator iterVecBusinessNoOplog=vecBusinessIdNoOplog.begin(); iterVecBusinessNoOplog!=vecBusinessIdNoOplog.end(); iterVecBusinessNoOplog++){
            online_t online;
            online.businessId = *iterVecBusinessNoOplog;
            online.businessName = "";
            online.hours = 0;
            online.hoursPerDay = 0;
            online.workingHours = "";
            mapBusinessToOnline[*iterVecBusinessNoOplog] = online;
        }
        DELETE_IF_NOT_NULL(res);

    }catch(sql::SQLException &e){
        std::cout<<__func__<<" sql exception: "<<e.what();
    }
    DELETE_IF_NOT_NULL(res);
}

void getOnlineStat(const std::string& business_id_str_list, const std::string& date, const int32_t& days, std::vector<online_t>& _return){
    std::vector<int32_t> vecBusinessId;
    std::vector<int32_t> vecBusinessIdNoOplog;
    std::set<int32_t> setCustomerId; // select in database use it 
    split(vecBusinessId, business_id_str_list, ",");
    std::copy(vecBusinessId.begin(), vecBusinessId.end(), std::back_inserter(vecBusinessIdNoOplog));

    std::map<int32_t, std::set<int32_t>> mapBusinessIdToSetCustomerId;
    for(uint32_t i=0; i < vecBusinessId.size(); i++){
        std::vector<service_t> tmp;
        get_customer_list(getConfig("shop.url"), boost::lexical_cast<std::string>(vecBusinessId[i]), "0", tmp);
        for(uint32_t j=0; j<tmp.size(); j++){
            setCustomerId.insert(boost::lexical_cast<int32_t>(tmp[j].service_id));
            mapBusinessIdToSetCustomerId[vecBusinessId[i]].insert(boost::lexical_cast<int32_t>(tmp[j].service_id));
        }
    }
/*
	std::cout << "########################### mapBusinessIdToSetCustomerId ############" << std::endl;
	std::map<int32_t, std::set<int32_t>>::iterator iterBtCSet = mapBusinessIdToSetCustomerId.begin();
	while(iterBtCSet != mapBusinessIdToSetCustomerId.end()){
		std::cout << "My business Id " << iterBtCSet->first << std::endl;
		std::set<int32_t>::iterator iterCSet = iterBtCSet->second.begin();
		while(iterCSet != iterBtCSet->second.end() ){
			std::cout << *iterCSet << "," ;
			iterCSet++;
		} 
		std::cout << std::endl;
		iterBtCSet++;
	}
	std::cout << "#####################################################################" << std::endl;
*/
    int32_t startTime = 0;
    int32_t endTime   = 0;
	//const int32_t tempDays = 6;
	const int32_t tempDays = days;
    getTsRangeBackward(date, tempDays, startTime, endTime);
	//startTime = 1434643199;
	
    sql::Driver*            driver    = NULL;
    sql::Connection*        conn      = NULL;
    sql::Statement*         stmt      = NULL;
    sql::PreparedStatement* prep_stmt = NULL;
	try{
        driver = get_driver_instance();
        conn = driver->connect(getConfig("mysql.uri"), getConfig("mysql.username"), getConfig("mysql.password"));
        conn->setSchema(getConfig("mysql.schema"));
        sql::PreparedStatement *prep_stmt = conn->prepareStatement("set names utf8mb4");
        prep_stmt->executeQuery();
        stmt = conn->createStatement();

		//   record the day all business online hours
		//   记录该天  所有商家在线时间
		std::vector<std::map<int32_t,online_t>> vecMapBusinessToOnline;
		int32_t  tempStartTime;
		for(int i=0; i < tempDays ; i++){
		//for(int i=0; i < 2 ; i++){
			std::cout << "################### getThedayBusinessOnlineHOurs ################### " << i << std::endl;
			tempStartTime = startTime+i*86400;
			std::map<int32_t,online_t> mapBusinessToOnline;
			getThedayBusinessOnlineHours(mapBusinessIdToSetCustomerId,vecBusinessIdNoOplog,setCustomerId,tempStartTime,stmt,mapBusinessToOnline);
			
			vecMapBusinessToOnline.push_back(mapBusinessToOnline);
			/*   // return every business online hours per day
			std::map<int32_t,online_t>::iterator iterMapBusinessToOnline;
			iterMapBusinessToOnline = mapBusinessToOnline.begin();
			while(iterMapBusinessToOnline != mapBusinessToOnline.end()){
				_return.push_back(iterMapBusinessToOnline->second);
				iterMapBusinessToOnline++;
			}
			*/
		}

		// 获取最终的返回时间
        online_t online;
        online_t tempOnline;
    	std::vector<int32_t>::iterator iterVecBusinessId = vecBusinessId.begin();
		std::vector<std::map<int32_t,online_t>>::iterator iterVecMapBusinessToOnline;
		while(iterVecBusinessId != vecBusinessId.end()){
			iterVecMapBusinessToOnline = vecMapBusinessToOnline.begin();
        	online.hours = 0;
	        online.hoursPerDay = 0;
    	    online.workingHours = "";
			while(iterVecMapBusinessToOnline != vecMapBusinessToOnline.end()){
				tempOnline = (*iterVecMapBusinessToOnline)[*iterVecBusinessId];
				online.businessId = tempOnline.businessId;
				online.businessName = tempOnline.businessName;
            	online.hours += tempOnline.hours;
     	        online.hoursPerDay += tempOnline.hoursPerDay;
        	    online.workingHours += tempOnline.workingHours;

				iterVecMapBusinessToOnline++;
			}
			online.hoursPerDay = online.hoursPerDay/tempDays;
			_return.push_back(online);
			iterVecBusinessId++;
		}
		
    	DELETE_IF_NOT_NULL(conn);
    	DELETE_IF_NOT_NULL(stmt);
    	DELETE_IF_NOT_NULL(prep_stmt);
    }catch(sql::SQLException &e){
        std::cout<<__func__<<" sql exception: "<<e.what();
    }
    DELETE_IF_NOT_NULL(conn);
    DELETE_IF_NOT_NULL(stmt);
    DELETE_IF_NOT_NULL(prep_stmt);
}
// end set


//  old vesion 
/*
void getOnlineStat(const std::string& business_id_str_list, const std::string& date, const int32_t& days, std::vector<online_t>& _return){

    sql::Driver*            driver    = NULL;
    sql::Connection*        conn      = NULL;
    sql::Statement*         stmt      = NULL;
    sql::ResultSet*         res       = NULL;
    sql::PreparedStatement* prep_stmt = NULL;

    int32_t startTime = 0;
    int32_t endTime   = 0;

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

    try{
        driver = get_driver_instance();
        conn = driver->connect(getConfig("mysql.uri"), getConfig("mysql.username"), getConfig("mysql.password"));
        conn->setSchema(getConfig("mysql.schema"));

        sql::PreparedStatement *prep_stmt = conn->prepareStatement("set names utf8mb4");
        prep_stmt->executeQuery();
        stmt = conn->createStatement();

        std::stringstream cmd;
        cmd<<"select op, src_role, src_role_id, dst_role, dst_role_id, business_id, UNIX_TIMESTAMP(ts) from oplog where (op in (1,2)) and (ts>FROM_UNIXTIME("<<startTime<<") and ts<FROM_UNIXTIME("<<endTime<<")) and (src_role=1 and src_role_id in ("<<strCustomerId<<")) order by ts;";
        std::cout<<__func__<<" sql cmd: "<<cmd.str()<<std::endl;

        std::map<int32_t, std::set<int32_t>> mapBusinessIdToRealSetCustomerId;
        std::map<int32_t, std::set<int32_t>>::iterator iterMapBusinessIdToRealSetCustomerId;
        std::map<int32_t, std::vector<oplog_t>> mapBusinessIdToOplog;
        std::map<int32_t, std::vector<oplog_t>>::iterator iterMapBusinessToOplog;

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
            mapBusinessIdToRealSetCustomerId[mapCustomerIdToBusinessId[oplog.src_role_id]].insert(oplog.src_role_id);
            mapBusinessIdToOplog[mapCustomerIdToBusinessId[oplog.src_role_id]].push_back(oplog);
        }

        for(iterMapBusinessToOplog=mapBusinessIdToOplog.begin(); iterMapBusinessToOplog!=mapBusinessIdToOplog.end(); iterMapBusinessToOplog++){
            iterMapBusinessIdToRealSetCustomerId = mapBusinessIdToRealSetCustomerId.find(iterMapBusinessToOplog->first);
            if(iterMapBusinessIdToRealSetCustomerId != mapBusinessIdToRealSetCustomerId.end()){
                online_t online;
                online.businessId = iterMapBusinessToOplog->first;
                std::map<int32_t, bool> mapCustomerToInitState;
                getCustomerStateForOneBusiness(mapCustomerToInitState, iterMapBusinessIdToRealSetCustomerId->second, iterMapBusinessToOplog->second);
                float workingHours = getBusinessOnlineHours(startTime, mapCustomerToInitState, iterMapBusinessToOplog->second, online);
                online.hours = workingHours;
                online.hoursPerDay = workingHours/days;
                _return.push_back(online);
                std::cout<<"online businessId: "<<online.businessId<<std::endl;
                std::cout<<"online hours: "<<online.hours<<std::endl;
                std::cout<<"online hoursPerDay: "<<online.hoursPerDay<<std::endl;
                std::cout<<"online workingHours: "<<online.workingHours<<std::endl;
                std::vector<int32_t>::iterator iterVecBusinessNoOplog = std::find(vecBusinessIdNoOplog.begin(), vecBusinessIdNoOplog.end(), iterMapBusinessIdToRealSetCustomerId->first);
                if(iterVecBusinessNoOplog!=vecBusinessIdNoOplog.end()){
                    vecBusinessIdNoOplog.erase(iterVecBusinessNoOplog);
                }

            }
        }

        for(std::vector<int32_t>::iterator iterVecBusinessNoOplog=vecBusinessIdNoOplog.begin(); iterVecBusinessNoOplog!=vecBusinessIdNoOplog.end(); iterVecBusinessNoOplog++){
            online_t online;
            online.businessId = *iterVecBusinessNoOplog;
            online.businessName = "";
            online.hours = 0;
            online.hoursPerDay = 0;
            online.workingHours = "";
            _return.push_back(online);
        }

        DELETE_IF_NOT_NULL(conn);
        DELETE_IF_NOT_NULL(stmt);
        DELETE_IF_NOT_NULL(res);
        DELETE_IF_NOT_NULL(prep_stmt);

    }catch(sql::SQLException &e){
        std::cout<<__func__<<" sql exception: "<<e.what();
    }

    DELETE_IF_NOT_NULL(conn);
    DELETE_IF_NOT_NULL(stmt);
    DELETE_IF_NOT_NULL(res);
    DELETE_IF_NOT_NULL(prep_stmt);
}
*/



void onlineToJsonStr(std::vector<online_t>& online, std::string& _return){
    Json::Value root;
    //std::cout<<"online.size() = "<<online.size()<<std::endl;
    for(uint32_t i=0; i<online.size(); i++){
        Json::Value item;
        item["business_id"] = boost::lexical_cast<std::string>(online[i].businessId);
        item["business_name"] = online[i].businessName;
        item["total_hours"] = boost::lexical_cast<std::string>(online[i].hours);
        item["hours_per_day"] = boost::lexical_cast<std::string>(online[i].hoursPerDay);
        item["duration"] = online[i].workingHours;
        root.append(item);
    }
    _return = root.toStyledString();
    //std::cout<<"data = "<<_return<<std::endl;
}

