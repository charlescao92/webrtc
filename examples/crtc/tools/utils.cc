#include "utils.h"

using HttpClient = Http::HttpClient;

namespace net {

Http::HTTPERROR httpExecRequest(const std::string& method,
                                const std::string& url,
                                std::string& responsebody,
                                const std::string& requestBody,
                                const Http::ContentType& type) {
  Http::HTTPERROR errcode = Http::HTTPERROR_SUCCESS;
  Http::HttpResponse response;
  HttpClient client;
  Http::HttpRequest request(method, url);
  std::string contentType = "application/json";
  if (type == Http::ContentType::Xml) {
    request.addHeaderField("Content-Type", "application/xml");
  }
  request.setBody(requestBody);
  if (!client.execute(&request, &response)) {
    errcode = client.getErrorCode();
    client.killSelf();
    return errcode;
  }
  responsebody = response.getBody();
  client.killSelf();
  return Http::HTTPERROR_SUCCESS;
}

std::string httpErrorString(Http::HTTPERROR errcode) {
  switch (errcode) {
    case Http::HTTPERROR_SUCCESS:
      return "success";
      break;
    case Http::HTTPERROR_INVALID:
      return "http invalid";
      break;
    case Http::HTTPERROR_CONNECT:
      return "connect error";
      break;
    case Http::HTTPERROR_TRANSPORT:
      return "transport error";
      break;
    case Http::HTTPERROR_IO:
      return "IO error";
      break;
    case Http::HTTPERROR_PARAMETER:
      break;
      return "param error";
    default:
      return "";
      break;
  }
  return "";
}

}

#define CHECK_TOOLS_INIT \
  if (!isInit) {         \
    Init();              \
  }

void Tools::Init() {
  srand(time(nullptr));
  auto ret = uuid4_init();
  assert(ret == UUID4_ESUCCESS);
  isInit = true;
}

std::string Tools::GetUUID() {
  CHECK_TOOLS_INIT
  char uuid[UUID4_LEN] = {0};
  uuid4_generate(uuid);
  return uuid;
}

std::string Tools::RandString(UInt32 len) {
  std::string uuid = GetUUID();
  for (auto& u : uuid) {
    if (u == '-') {
      u = 'a';
    }
  }
  if (len < uuid.size()) {
    return uuid.substr(0, len);
  } else {
    std::string str;
    for (UInt32 i = 0; i < len - uuid.size(); i++) {
      uuid += rand() % 26 + 'a';
    } 
  }
  return uuid;
}
