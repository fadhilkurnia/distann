#include <"/Users/samiahmed/Projects/URV/hnswlib/hnswlib.h">
#include <drogon/drogon.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
using namespace drogon;

int main() {

  // Creat our hnsw index that will be queried on when we search for images
  // using our prompt hnsw alg params
  int dim = 16;
  int max_elements = 100;
  int M = 16;
  int ef_construction = 200;

  // create index
  hnswlib::L2Space space(dim);
  hnswlib::HierarchicalNSW<float> *alg_hnsw =
      new hnswlib::HierarchicalNSW<float>(&space, max_elements, M,
                                          ef_construction);

  // Add images to index
  for (int i = 0; i < max_elements; i++) {
    alg_hnsw->addPoint(/*image*/ +i * dim, i);
  }

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
  app().registerHandler(
      "/images/{image_name}", [](const HttpRequestPtr &req, Callback &&callback,
                                 const std::string image_name) {
        auto resp = HttpResponse::newHttpResponse();

        std::string imageDirectory =
            "/Users/samiahmed/Projects/URV/distann/backend/images/";
        std::string imagePath = imageDirectory + image_name;

        // store our image as string data
        std::ifstream imageData(imagePath, std::ios::binary);

        // check that our image exists
        if (!imageData) {
          resp->setBody("Image not found");
          resp->setContentTypeCode(CT_TEXT_HTML);
          resp->setStatusCode(k404NotFound);
          callback(resp);
        }

        std::ostringstream oss;
        oss << imageData.rdbuf();
        std::string image = oss.str();

        resp->setBody(std::move(image));
        resp->setContentTypeCode(CT_IMAGE_PNG);
        resp->setStatusCode(k200OK);
        callback(resp);
      });

  app().registerHandler(
      "/api/search", [](const HttpRequestPtr &req, Callback &&callback) {
        auto resp = HttpResponse::newHttpResponse();
        auto prompt = req->getOptionalParameter<std::string>("prompt");

        // handle empty prompt
        if (!prompt.has_value()) {
          resp->setStatusCode(k400BadRequest);
          resp->setBody("{\"error\": \"prompt unspecified\"}");
          resp->setContentTypeCode(CT_APPLICATION_JSON);
          callback(resp);
          return;
        }

        std::string localHost = "http://localhost:9000";
        std::string imageURL = localHost + "/images/cat.png";

        // TODO: convert prompt to an embedding e
        // TODO: do similarity search using searchKnn over the index with e
        // TODO: return top 5 similar results
        // TODO: convert the embeddings to image id's

        // create json response
        json response;
        response["prompt"] = prompt.value();
        response["results"] = json::array();
        json result;
        result["id"] = 1;
        result["url"] = imageURL;
        result["alt"] = "Cat";
        response["results"].push_back(result);

        resp->setBody(response.dump());
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setStatusCode(k200OK);
        callback(resp);
      });

  // run the backend server in the specified host/interface and port.
  std::string host = "0.0.0.0";
  int port = 9000;
  LOG_INFO << "Running distann backend server on " << host << ":" << port;
  app().addListener(host, port).run();

  return 0;
}
