
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
	
static float getThedayBusinessFirstResAveHours(int32_t endTime,std::vector<oplog_t>& vecOplog,std::map<int32_t,oneResponse_t> & mapUserIdToFirstRes){
	
	std::map<int32_t,oneResponse_t>::iterator iterMapUserIdToFirstRes;
	
	std::vector<oplog_t>::iterator iterVecOplog;
	iterVecOplog = vecOplog.begin();
	while(iterVecOplog != vecOplog.end()){
        if(iterVecOplog->src_role==0){
			//  针对初次操作
        	iterMapUserIdToFirstRes = mapUserIdToFirstRes.find(iterVecOplog->src_role_id);
			if(iterMapUserIdToFirstRes == mapUserIdToFirstRes.end()){  // 如果没有就添加 有了不作处理
				oneResponse_t oneRep;
				oneRep.userId = iterVecOplog->src_role_id;
				oneRep.startTime = iterVecOplog->timestamp;
				oneRep.endTime = endTime;
				oneRep.repState = false;
				mapUserIdToFirstRes[iterVecOplog->src_role_id] = oneRep;		
			}
		}else if(iterVecOplog->src_role==1){
        	iterMapUserIdToFirstRes = mapUserIdToFirstRes.find(iterVecOplog->dst_role_id);
			if(iterMapUserIdToFirstRes != mapUserIdToFirstRes.end()){
				if(iterMapUserIdToFirstRes->second.repState == false){
					iterMapUserIdToFirstRes->second.endTime = iterVecOplog->timestamp;
					iterMapUserIdToFirstRes->second.repState = true;
				}
			}
		}
		//std::cout <<iterMapUserIdToFirstRes->second.userId <<" "<< iterMapUserIdToFirstRes->second.startTime << " "<< iterMapUserIdToFirstRes->second.endTime<< "##"; 
		iterVecOplog++;
	}
	/*	
	// 针对一直不响应的状态 累加天数
	iterMapUserIdToFirstRes = mapUserIdToFirstRes.begin();
	while(iterMapUserIdToFirstRes != mapUserIdToFirstRes.end()){
		if(iterMapUserIdToFirstRes->second.repState == false){
		    iterMapUserIdToFirstRes->second.endTime = endTime;
		}
		iterMapUserIdToFirstRes++;
	}
	*/
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
static float getThedayBusinessResAveHours(int32_t endTime,std::vector<oplog_t>& vecOplog,std::map<int32_t,std::vector<oneResponse_t>> & mapUserIdToVecRes){
	std::map<int32_t,std::vector<oneResponse_t>>::iterator  iterMapUserIdToVecRes;
	
	std::vector<oplog_t>::iterator iterVecOplog;
	iterVecOplog = vecOplog.begin();
	while(iterVecOplog != vecOplog.end()){
        if(iterVecOplog->src_role==0){
			//  针对初次操作
        	iterMapUserIdToVecRes = mapUserIdToVecRes.find(iterVecOplog->src_role_id);
			if(iterMapUserIdToVecRes == mapUserIdToVecRes.end() ){  // 如果没有就添加 有了不作处理
				oneResponse_t oneRep;
				oneRep.userId = iterVecOplog->src_role_id;
				oneRep.startTime = iterVecOplog->timestamp;
				oneRep.endTime = endTime;
				oneRep.repState = false;

				std::vector<oneResponse_t> tempVec;
				tempVec.push_back(oneRep);
				mapUserIdToVecRes[iterVecOplog->src_role_id] = tempVec;		
			}else{
				if(iterMapUserIdToVecRes->second.back().repState == true){
					oneResponse_t oneRep;
					oneRep.userId = iterVecOplog->src_role_id;
					oneRep.startTime = iterVecOplog->timestamp;
					oneRep.endTime = endTime;
					oneRep.repState = false;
					
					mapUserIdToVecRes[iterVecOplog->src_role_id].push_back(oneRep);
				}
			}
		}else if(iterVecOplog->src_role==1){
        	iterMapUserIdToVecRes = mapUserIdToVecRes.find(iterVecOplog->dst_role_id);
			if(iterMapUserIdToVecRes != mapUserIdToVecRes.end()){
				if(iterMapUserIdToVecRes->second.back().repState == false){
					iterMapUserIdToVecRes->second.back().endTime = iterVecOplog->timestamp;
					iterMapUserIdToVecRes->second.back().repState = true;
					std::cout << "hello " << iterMapUserIdToVecRes->second.back().endTime << " " << iterVecOplog->timestamp << " " << iterMapUserIdToVecRes->second.back().repState << std::endl;  
				}
			}
		}
		iterVecOplog++;
	}
	/*
	// 针对一直不响应的状态 累加天数
	iterMapUserIdToVecRes = mapUserIdToVecRes.begin();
	while(iterMapUserIdToVecRes != mapUserIdToVecRes.end()){
		if(iterMapUserIdToVecRes->second.back().repState == false){
			iterMapUserIdToVecRes->second.back().endTime = endTime;
			std::cout << "nihao " << iterMapUserIdToVecRes->second.back().endTime << " " << iterVecOplog->timestamp << " " << iterMapUserIdToVecRes->second.back().repState << std::endl;
		}
		iterMapUserIdToVecRes++;
	}
	*/
}

static void getThedayBusinessResHours(std::vector<int32_t> vecBusinessId,std::vector<int32_t> vecBusinessIdNoOplog,const std::string business_id_str_list,
		std::set<int32_t> setCustomerId,const int32_t theday, sql::Statement* stmt,
		std::map<int32_t,std::map<int32_t,oneResponse_t>> & mapBusinessIdToMapUserIdToFirstRes,
		std::map<int32_t,std::map<int32_t,std::vector<oneResponse_t>>> &  mapBusinessIdToMapUserIdToVecRes){
	
	int32_t startTime = theday;
	int32_t endTime   = theday+86400;
    std::string strCustomerId;
    intSetToStr(setCustomerId, strCustomerId);

    sql::ResultSet*  res = NULL;
    try{
        std::stringstream cmd;
        //cmd<<"select op, src_role, src_role_id, dst_role, dst_role_id, business_id, UNIX_TIMESTAMP(ts) from oplog where (((src_role=0) and business_id in ("<<business_id_str_list<<")) or ((src_role=1) and (src_role_id in ("<<strCustomerId<<")))) and (op=3) and (ts>FROM_UNIXTIME("<<startTime<<") and ts<FROM_UNIXTIME("<<endTime<<")) order by ts;";
        cmd<<"select op, src_role, src_role_id, dst_role, dst_role_id, business_id, UNIX_TIMESTAMP(ts) from oplog where business_id in ("<<business_id_str_list<<") and (op=3) and (ts>FROM_UNIXTIME("<<startTime<<") and ts<FROM_UNIXTIME("<<endTime<<")) order by ts;";
        std::cout<<__func__<<" sql cmd: "<<cmd.str()<<std::endl;

		
		std::map<int32_t, std::vector<oplog_t> > mapBusinessIdToVecOplog;
		std::map<int32_t, std::vector<oplog_t> >::iterator iterMapBusinessIdToVecOplog;

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
			
            mapBusinessIdToVecOplog[oplog.business_id].push_back(oplog);
        }
	
		std::map<int32_t,std::map<int32_t,oneResponse_t>>::iterator iterMapBusinessIdToMapUserIdToFirstRes;
		std::map<int32_t,std::map<int32_t,std::vector<oneResponse_t>>>::iterator iterMapBusinessIdToMapUserIdToVecRes;
		//循环遍历使用
		iterMapBusinessIdToVecOplog = mapBusinessIdToVecOplog.begin();
		while(iterMapBusinessIdToVecOplog!=mapBusinessIdToVecOplog.end()){
			std::cout << "############## businessId ################## " << iterMapBusinessIdToVecOplog->first << std::endl;
			iterMapBusinessIdToMapUserIdToFirstRes = mapBusinessIdToMapUserIdToFirstRes.find(iterMapBusinessIdToVecOplog->first);
			if(iterMapBusinessIdToMapUserIdToFirstRes == mapBusinessIdToMapUserIdToFirstRes.end()){
				std::map<int32_t,oneResponse_t> tempMapUserIdToFirstRes;
				mapBusinessIdToMapUserIdToFirstRes[iterMapBusinessIdToVecOplog->first] = tempMapUserIdToFirstRes;

				std::cout << " no BusinessId  MapUserIdToFirstRes " << std::endl;
				iterMapBusinessIdToMapUserIdToFirstRes = mapBusinessIdToMapUserIdToFirstRes.find(iterMapBusinessIdToVecOplog->first);
			}
			getThedayBusinessFirstResAveHours(endTime,iterMapBusinessIdToVecOplog->second,iterMapBusinessIdToMapUserIdToFirstRes->second);
			
			iterMapBusinessIdToMapUserIdToVecRes = mapBusinessIdToMapUserIdToVecRes.find(iterMapBusinessIdToVecOplog->first);
			if(iterMapBusinessIdToMapUserIdToVecRes == mapBusinessIdToMapUserIdToVecRes.end()){
				std::map<int32_t,std::vector<oneResponse_t>> tempMapUserIdToVecRes;
				mapBusinessIdToMapUserIdToVecRes[iterMapBusinessIdToVecOplog->first] = tempMapUserIdToVecRes;
				
				std::cout << " no BusinessId  MapUserIdToVecRes " << std::endl;
				iterMapBusinessIdToMapUserIdToVecRes = mapBusinessIdToMapUserIdToVecRes.find(iterMapBusinessIdToVecOplog->first);
			}
			getThedayBusinessResAveHours(endTime,iterMapBusinessIdToVecOplog->second,iterMapBusinessIdToMapUserIdToVecRes->second);
			
        	iterMapBusinessIdToVecOplog++;
		}

		/*
		iterMapBusinessIdToMapUserIdToFirstRes = mapBusinessIdToMapUserIdToFirstRes.begin();
		while( iterMapBusinessIdToMapUserIdToFirstRes != mapBusinessIdToMapUserIdToFirstRes.end()){
			std::cout << " $$$$$$$$$$$$$$$$$$$$$ BusinessIdToUserId $$$$$$$$$$$$$$$$$$$$$$ " << iterMapBusinessIdToMapUserIdToFirstRes->first << std::endl;
			std::map<int32_t,oneResponse_t>::iterator iterMapUserIdToFirstRes = iterMapBusinessIdToMapUserIdToFirstRes->second.begin();
			while(iterMapUserIdToFirstRes != iterMapBusinessIdToMapUserIdToFirstRes->second.end()){
				std::cout << iterMapUserIdToFirstRes->first << "," << iterMapUserIdToFirstRes->second.userId << "," <<
				          iterMapUserIdToFirstRes->second.startTime << "," << iterMapUserIdToFirstRes->second.endTime << std::endl;
				iterMapUserIdToFirstRes++;
			}
			std::cout << std::endl;

			iterMapBusinessIdToMapUserIdToFirstRes++;
		}*/
		
		//std::map<int32_t,std::map<int32_t,std::vector<oneResponse_t>>>::iterator iterMapBusinessIdToMapUserIdToVecRes;
		/*
		iterMapBusinessIdToMapUserIdToVecRes = mapBusinessIdToMapUserIdToVecRes.begin();
		while(iterMapBusinessIdToMapUserIdToVecRes != mapBusinessIdToMapUserIdToVecRes.end()){
			std::cout << " $$$$$$$$$$$$$$$$$$$$$ BusinessIdToUserId $$$$$$$$$$$$$$$$$$$$$$ " << iterMapBusinessIdToMapUserIdToVecRes->first << std::endl;
			std::map<int32_t,std::vector<oneResponse_t>>::iterator iterMapUserIdToVecRes = iterMapBusinessIdToMapUserIdToVecRes->second.begin();
			while(iterMapUserIdToVecRes != iterMapBusinessIdToMapUserIdToVecRes->second.end()){
				std::cout << iterMapUserIdToVecRes->first << "," << iterMapUserIdToVecRes->second.size() << std::endl;
				
				std::vector<oneResponse_t>::iterator iterVecRes;
				iterVecRes = iterMapUserIdToVecRes->second.begin();
				while(iterVecRes != iterMapUserIdToVecRes->second.end()){
					std::cout << iterVecRes->userId << " " << iterVecRes->startTime << " " << iterVecRes->endTime << " " << iterVecRes->repState << std::endl;
					iterVecRes++;
				}

				iterMapUserIdToVecRes++;
			}
			std::cout << std::endl;
			iterMapBusinessIdToMapUserIdToVecRes++;
		}
		*/
        delete res;
    }catch(sql::SQLException &e){
        std::cout<<__func__<<" sql exception: "<<e.what();
    }
}

void getResponseStat(const std::string& business_id_str_list, const std::string& date, const int32_t& days, std::vector<response_t>& _return){
    std::vector<int32_t> vecBusinessId;
	std::vector<int32_t>::iterator iterVecBusinessId;
    std::vector<int32_t> vecBusinessIdNoOplog;
    std::set<int32_t> setCustomerId;
    split(vecBusinessId, business_id_str_list, ",");
    std::copy(vecBusinessId.begin(), vecBusinessId.end(), std::back_inserter(vecBusinessIdNoOplog));
	
    //std::map<int32_t, std::set<int32_t>> mapBusinessIdToSetCustomerId;
    for(uint32_t i=0; i<vecBusinessId.size(); i++){
        std::vector<service_t> tmp;
        get_customer_list(getConfig("shop.url"), boost::lexical_cast<std::string>(vecBusinessId[i]), "0", tmp);
        for(uint32_t j=0; j<tmp.size(); j++){
            setCustomerId.insert(boost::lexical_cast<int32_t>(tmp[j].service_id));
           // mapBusinessIdToSetCustomerId[vecBusinessId[i]].insert(boost::lexical_cast<int32_t>(tmp[j].service_id));
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

	std::cout << "days " << days << std::endl;

    int32_t startTime;
    int32_t endTime;
	const int32_t tempDays = days; 
    getTsRangeBackward(date, tempDays, startTime, endTime);

    sql::Driver*     driver;
    sql::Connection* conn;
    sql::Statement*  stmt;
    try{
        driver = get_driver_instance();
        conn = driver->connect(getConfig("mysql.uri"), getConfig("mysql.username"), getConfig("mysql.password"));
        conn->setSchema(getConfig("mysql.schema"));
        sql::PreparedStatement *prep_stmt = conn->prepareStatement("set names utf8mb4");
        prep_stmt->executeQuery();
        stmt = conn->createStatement();

		std::vector<std::map<int32_t,response_t>> vecMapBusinessToResponse;
		int32_t  tempStartTime;
		int32_t  tempEndTime;
		std::map<int32_t,std::map<int32_t,oneResponse_t>> mapBusinessIdToMapUserIdToFirstRes;  // 初次回复的时间
		std::map<int32_t,std::map<int32_t,oneResponse_t>>::iterator iterMapBusinessIdToMapUserIdToFirstRes;
		std::map<int32_t,oneResponse_t>::iterator iterMapUserIdToFirstRes;
		std::map<int32_t,oneResponse_t>::iterator iterMapUserIdToFirstResEnd;
		std::map<int32_t,std::map<int32_t,std::vector<oneResponse_t>>> mapBusinessIdToMapUserIdToVecRes;  // 每次回复的时间
		std::map<int32_t,std::map<int32_t,std::vector<oneResponse_t>>>::iterator iterMapBusinessIdToMapUserIdToVecRes;
		std::map<int32_t,std::vector<oneResponse_t>>::iterator  iterMapUserIdToVecRes;
		std::map<int32_t,std::vector<oneResponse_t>>::iterator  iterMapUserIdToVecResEnd;
		for(int i=0; i<tempDays; i++){
			tempStartTime = startTime+i*86400;
			tempEndTime = tempStartTime+86400;
			if( tempEndTime-endTime > 0 && tempEndTime-endTime < 86400)
				tempEndTime = endTime;
			
			std::cout << "################## getThedayBusinessResHours ################ " << i << std::endl;
			getThedayBusinessResHours(vecBusinessId,vecBusinessIdNoOplog,business_id_str_list,setCustomerId,tempStartTime,stmt,
										mapBusinessIdToMapUserIdToFirstRes, 
										mapBusinessIdToMapUserIdToVecRes);

			iterVecBusinessId = vecBusinessId.begin();
			while(iterVecBusinessId != vecBusinessId.end()){
				iterMapBusinessIdToMapUserIdToFirstRes = mapBusinessIdToMapUserIdToFirstRes.find(*iterVecBusinessId);
				if(iterMapBusinessIdToMapUserIdToFirstRes != mapBusinessIdToMapUserIdToFirstRes.end()){
					// 针对一直不响应的状态 累加天数
					iterMapUserIdToFirstRes = iterMapBusinessIdToMapUserIdToFirstRes->second.begin();
					iterMapUserIdToFirstResEnd = iterMapBusinessIdToMapUserIdToFirstRes->second.end();
					while(iterMapUserIdToFirstRes != iterMapUserIdToFirstResEnd){
						if(iterMapUserIdToFirstRes->second.repState == false){
			    			iterMapUserIdToFirstRes->second.endTime = tempEndTime;
						}
						iterMapUserIdToFirstRes++;
					}
				}

				iterMapBusinessIdToMapUserIdToVecRes = mapBusinessIdToMapUserIdToVecRes.find(*iterVecBusinessId);
				if(iterMapBusinessIdToMapUserIdToVecRes != mapBusinessIdToMapUserIdToVecRes.end()){			
					// 针对一直不响应的状态 累加天数
					iterMapUserIdToVecRes = iterMapBusinessIdToMapUserIdToVecRes->second.begin();
					iterMapUserIdToVecResEnd = iterMapBusinessIdToMapUserIdToVecRes->second.end();
					while(iterMapUserIdToVecRes != iterMapUserIdToVecResEnd){
						if(iterMapUserIdToVecRes->second.back().repState == false){
							iterMapUserIdToVecRes->second.back().endTime = tempEndTime;
							//std::cout << "nihao " << iterMapUserIdToVecRes->second.back().endTime << " " << tempEndTime << " " << endTime << " " << iterMapUserIdToVecRes->second.back().repState << std::endl;
						}
						iterMapUserIdToVecRes++;
					}
				}
				
    	    	iterVecBusinessId++;
			}
		}

		int userCount = 0;
		float firstHours = 0;
		float aveHours = 0;
		//std::map<int32_t,std::map<int32_t,oneResponse_t>>::iterator iterMapBusinessIdToMapUserIdToFirstRes;
		//std::map<int32_t,oneResponse_t>::iterator iterMapUserIdToFirstRes;
		//std::map<int32_t,oneResponse_t>::iterator iterMapUserIdToFirstResEnd;
		//std::map<int32_t,std::map<int32_t,std::vector<oneResponse_t>>>::iterator iterMapBusinessIdToMapUserIdToVecRes;
		//std::map<int32_t,std::vector<oneResponse_t>>::iterator iterMapUserIdToVecRes;
		//std::map<int32_t,std::vector<oneResponse_t>>::iterator iterMapUserIdToVecResEnd;
		std::vector<oneResponse_t>::iterator iterVecRes;
		std::vector<oneResponse_t>::iterator iterVecResEnd;
		
		iterVecBusinessId = vecBusinessId.begin();
		while(iterVecBusinessId != vecBusinessId.end()){
			//std::cout << "8888888888888 businessId 8888888888888  " << *iterVecBusinessId << std::endl;
			userCount = 0;
			firstHours = 0;
			aveHours = 0;
			iterMapBusinessIdToMapUserIdToFirstRes = mapBusinessIdToMapUserIdToFirstRes.find(*iterVecBusinessId);
			if(iterMapBusinessIdToMapUserIdToFirstRes != mapBusinessIdToMapUserIdToFirstRes.end()){
				//std::cout << iterMapBusinessIdToMapUserIdToFirstRes->first << " FirstRes Size " << iterMapBusinessIdToMapUserIdToFirstRes->second.size() << std::endl;
				userCount = iterMapBusinessIdToMapUserIdToFirstRes->second.size();
				iterMapUserIdToFirstRes	= iterMapBusinessIdToMapUserIdToFirstRes->second.begin();
				iterMapUserIdToFirstResEnd	= iterMapBusinessIdToMapUserIdToFirstRes->second.end();
				while(iterMapUserIdToFirstRes != iterMapUserIdToFirstResEnd){
					firstHours += (float)(iterMapUserIdToFirstRes->second.endTime - iterMapUserIdToFirstRes->second.startTime)/(3600);
					iterMapUserIdToFirstRes++;
				}
			}
			iterMapBusinessIdToMapUserIdToVecRes = mapBusinessIdToMapUserIdToVecRes.find(*iterVecBusinessId);
			if(iterMapBusinessIdToMapUserIdToVecRes != mapBusinessIdToMapUserIdToVecRes.end()){
				std::cout << iterMapBusinessIdToMapUserIdToVecRes->first  << " VecRes Size " << iterMapBusinessIdToMapUserIdToVecRes->second.size() << std::endl;	
				iterMapUserIdToVecRes	= iterMapBusinessIdToMapUserIdToVecRes->second.begin();
				iterMapUserIdToVecResEnd	= iterMapBusinessIdToMapUserIdToVecRes->second.end();
				while(iterMapUserIdToVecRes != iterMapUserIdToVecResEnd){
					iterVecRes = iterMapUserIdToVecRes->second.begin();
					iterVecResEnd = iterMapUserIdToVecRes->second.end();
					std::cout << "##### userId ##### " << iterMapUserIdToVecRes->first << std::endl;  
					while(iterVecRes != iterVecResEnd){
						aveHours += (float)(iterVecRes->endTime - iterVecRes->startTime)/(3600);
						std::cout << iterVecRes->endTime << " " << iterVecRes->startTime << " " << aveHours << " " << iterVecRes->repState <<std::endl;
						iterVecRes++;
					}
					iterMapUserIdToVecRes++;
				}
			}

            response_t response;
            response.businessId = *iterVecBusinessId;
            response.userCount = userCount;
			if( firstHours == 0 || userCount == 0)
            	response.firstResHours = 0;
			else
            	response.firstResHours = (firstHours/userCount);

			if(aveHours == 0 || userCount == 0)
            	response.aveResHours = 0;
			else
            	response.aveResHours = (aveHours/userCount);	
            _return.push_back(response);

        	iterVecBusinessId++;
		}
        delete conn;
        delete stmt;
        delete prep_stmt;

    }catch(sql::SQLException &e){
        std::cout<<__func__<<" sql exception: "<<e.what();
    }
}


/*
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
*/
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
