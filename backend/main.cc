#include <drogon/drogon.h>
#include <string>

using namespace drogon;

int main() {

  app().setLogLevel(trantor::Logger::kDebug);

  using Callback = std::function<void(const HttpResponsePtr &)>;

  app().registerHandler("/",
                        [](const HttpRequestPtr &req, Callback &&callback) {
                          auto resp = HttpResponse::newHttpResponse();
                          resp->setBody("Welcome to distann backend server :)");
                          resp->setStatusCode(k200OK);
                          callback(resp);
                        });

  // TODO: response with static images
  app().registerHandler("/images/{image_name}",
                        [](const HttpRequestPtr &req, Callback &&callback,
                           const std::string image_name) {
                          auto resp = HttpResponse::newHttpResponse();

                          resp->setBody(image_name);
                          
                          resp->setStatusCode(k200OK);
                          callback(resp);
                        });

  app().registerHandler("/api/search", [](const HttpRequestPtr &req,
                                          Callback            &&callback) {
    auto resp   = HttpResponse::newHttpResponse();
    auto prompt = req->getOptionalParameter<std::string>("prompt");

    // handle empty prompt
    if (!prompt.has_value()) {
      resp->setStatusCode(k400BadRequest);
      resp->setBody("{\"error\": \"prompt unspecified\"}");
      resp->setContentTypeCode(CT_APPLICATION_JSON);
      callback(resp);
      return;
    }

    // TODO: convert prompt to an embedding e
    // TODO: do similarity search using e over the dataset
    // TODO: return top 5 similar results

    std::string response_content = "{\"prompt\": \"" + prompt.value() + "\"}";

    resp->setBody(response_content);
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setStatusCode(k200OK);
    callback(resp);
  });

  // run the backend server in the specified host/interface and port.
  std::string host = "0.0.0.0";
  int         port = 9000;
  LOG_INFO << "Running distann backend server on " << host << ":" << port;
  app().addListener(host, port).run();

  return 0;
}
