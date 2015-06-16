
#include <iostream> 
#include <set>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp> 

#include "string.h"
#include "stdio.h"

#include "util-str.h"

void split(std::vector<int32_t>& dst, const std::string& src, const std::string& seprators){
    /*
    std::vector<std::string> tmp;
    boost::split(tmp, src, boost::is_any_of(seprators));
    for(int i=0; i<tmp.size(); i++){
        std::cout<<"vector["<<i<<"]="<<tmp[i]<<std::endl;
        dst.push_back(boost::lexical_cast<int32_t>(tmp[i]));
    }
    */
    char buff[256];
    char* pBuff = buff;
    const char* srcStr = src.c_str();
    int srcStrLen = strlen(srcStr);
    const char* filter = "0123456789,";
    int filterLen = strlen(filter);

    memset(buff, 0, 256);
    for(int i=0; i<srcStrLen; i++){
        for(int j=0; j<filterLen; j++){
            if(srcStr[i]==filter[j]){
                *(pBuff++)=srcStr[i];
            }
        }
    }
    std::cout<<"string after filted: "<<buff<<std::endl;
    char* tmp = strtok(buff, ",");
    while(tmp){
        int32_t record = atoi(tmp);
        dst.push_back(record);
        tmp = strtok(NULL,",");
    }
    for(int i; i<dst.size(); i++){
        std::cout<<"split result["<<i<<"]"<<dst[i]<<std::endl;
    }
}

void intSetToStr(const std::set<int32_t>& src, std::string& dst){
    std::set<int32_t>::iterator iterSetInt;
    std::stringstream tmp;
    bool start = true;
    for(iterSetInt=src.begin(); iterSetInt!=src.end(); iterSetInt++){
        if(start){
            start = false;
            tmp<<(*iterSetInt);
        }else{
            tmp<<","<<(*iterSetInt);
        }
    }
    std::cout<<"convert std::set<int32_t> to std::string: "<<tmp.str()<<std::endl;
    tmp>>dst;
}
