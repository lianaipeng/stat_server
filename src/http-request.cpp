
#include <iostream>
#include <curl/curl.h>
#include "http-request.h"

using namespace std;

static char errorBuffer[CURL_ERROR_SIZE];
static int writer(char *, size_t, size_t, std::string*);
static bool init(CURL *&, const char*, std::string*);

int HttpRequest::Get(std::string url, std::string& page)
{
    CURL *conn = NULL;
    CURLcode code;
 
    curl_global_init(CURL_GLOBAL_DEFAULT);
    if (!init(conn, url.c_str(), &page))
    {
        fprintf(stderr, "Connection initializion failed\n");
        return -1;
    }
    code = curl_easy_perform(conn);
   
    if (code != CURLE_OK)
    {
        fprintf(stderr, "Failed to get '%s' [%s]\n", url.c_str(), errorBuffer);
        return -2;
    }
    curl_easy_cleanup(conn);
    return 0;
}

static bool init(CURL *&conn, const char *url, std::string *p_buffer)
{
    CURLcode code;
    conn = curl_easy_init();
    if (conn == NULL)
    {
        fprintf(stderr, "Failed to create CURL connection\n");
        return false;
    }
    code=curl_easy_setopt(conn, CURLOPT_NOSIGNAL, 1L);
    if (code != CURLE_OK)
    {
        fprintf(stderr, "Failed to set nosignal  [%s]\n", errorBuffer);
        return false;
    }
    code = curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, errorBuffer);
    if (code != CURLE_OK)
    {
        fprintf(stderr, "Failed to set error buffer [%d]\n", code);
        return false;
    }
    code = curl_easy_setopt(conn, CURLOPT_URL, url);
    if (code != CURLE_OK)
    {
        fprintf(stderr, "Failed to set URL [%s]\n", errorBuffer);
        return false;
    }
    code = curl_easy_setopt(conn, CURLOPT_FOLLOWLOCATION, 1);
    if (code != CURLE_OK)
    {
        fprintf(stderr, "Failed to set redirect option [%s]\n", errorBuffer);
        return false;
    }
    code = curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, writer);
    if (code != CURLE_OK)
    {
        fprintf(stderr, "Failed to set writer [%s]\n", errorBuffer);
        return false;
    }
    code = curl_easy_setopt(conn, CURLOPT_WRITEDATA, p_buffer);
    if (code != CURLE_OK)
    {
        fprintf(stderr, "Failed to set write data [%s]\n", errorBuffer);
        return false;
    }
    return true;
}

static int writer(char *data, size_t size, size_t nmemb, std::string *writerData)
{
    unsigned long sizes = size * nmemb;
    if (writerData == NULL) return 0;
    writerData->append(data, sizes);
    return sizes;
}

int HttpRequest::Post(std::string url, std::string& form_data, std::string& page) {
    //static const char *postthis="Field=1&Field=2&Field=3";
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();
    if(curl) {
      curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    
        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &page);
    
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data.c_str());
    
        /* if we don't provide POSTFIELDSIZE, libcurl will strlen() by
           itself */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)form_data.size());
    
        /* Perform the request, res will get the return code */
        CURLcode res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
          fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        /* always cleanup */
        curl_easy_cleanup(curl);
    
        /* we're done with libcurl, so clean it up */
        curl_global_cleanup();

        return res==CURLE_OK?0:-1;
    }

    return -2;
}

int HttpRequest::Post(std::string url, std::vector<std::string>& headers, std::string& form_data, std::string& page) {
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();
    if(curl) {
      curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    
        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &page);
        
        //std::string stoken = std::string("Cookie: PHPSESSID=") + token;
        struct curl_slist *chunk = NULL;
        for(int i=0; i<headers.size(); i++) {
            std::string& header = headers[i];
            chunk = curl_slist_append(chunk, header.c_str());
        }
        if(chunk)
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, form_data.c_str());
    
        /* if we don't provide POSTFIELDSIZE, libcurl will strlen() by
           itself */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)form_data.size());
    
        /* Perform the request, res will get the return code */
        CURLcode res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
          fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_slist_free_all(chunk);
        /* always cleanup */
        curl_easy_cleanup(curl);
    
        /* we're done with libcurl, so clean it up */
        curl_global_cleanup();

        return res==CURLE_OK?0:-1;
    }

    return -2;


}
