#pragma once

#include <string>
#include <atomic>

#include "examples/crtc/net/http.h"
#include "examples/crtc/tools/uuid4.hpp"

namespace net {

    Http::HTTPERROR httpExecRequest(
                                 const std::string& method,
                                 const std::string& url,
                                 std::string& responsebody,
                                 const std::string& requestBody = "",
                                 const Http::ContentType& type = Http::ContentType::Json);
    std::string httpErrorString(Http::HTTPERROR errcode);
}

class Tools {
public:
    std::string RandString(UInt32 len);

private:
    void Init();
    std::string GetUUID();

private:
   bool isInit = false;


};