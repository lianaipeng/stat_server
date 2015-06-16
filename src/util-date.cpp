
#include <iostream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>

#include "util-date.h"

using namespace boost::gregorian;
using namespace boost::posix_time;

void getTsRangeBackward(std::string strDate, const int32_t dayRange, int32_t& start, int32_t& end){

    //todo: check format with regular expression.
    
    std::cout<<"argv date = "<<strDate+" 23:59:59"<<std::endl;
    std::cout<<"argv dayRange = "<<dayRange<<std::endl;

    std::string st(strDate + " 23:59:59");
    ptime ptEnd(time_from_string(st));
    days ds(dayRange);
    ptime ptStart = ptEnd - ds;
    tm tmStart = to_tm(ptStart);
    tm tmEnd = to_tm(ptEnd);
    start = ::mktime(&tmStart);
    end = ::mktime(&tmEnd);
    std::cout<<"return start = "<<start<<std::endl;
    std::cout<<"return end = "<<end<<std::endl;
}

void getDateStrFromTs(const int32_t& ts, std::string& dateStr){
    std::time_t tt = ts;
    ptime p = from_time_t(ts);
    dateStr = to_simple_string(p);
}
