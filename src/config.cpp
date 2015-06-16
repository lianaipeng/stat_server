
#include "config.h"

#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

using namespace boost;
using boost::property_tree::ptree;

static ptree pt;

void initConfig(const std::string& path){
    read_ini(path, pt);
}

std::string getConfig(const std::string& key){
    return pt.get<std::string>(key);
}
