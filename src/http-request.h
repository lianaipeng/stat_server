
#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_

#include <vector>
#include <iostream>

class HttpRequest
{
public:
    /*************************************************
      GET/POST http page from @url, save @page
      @return 0: success  <0: fail
    *************************************************/
    static int Get(std::string url, std::string& page);
    static int Post(std::string url, std::string& form_data, std::string& page);
    static int Post(std::string url, std::vector<std::string>& headers, std::string& form_data, std::string& page);
};

#endif

