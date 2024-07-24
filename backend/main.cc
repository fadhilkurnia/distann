#include <drogon/drogon.h>
#include <fstream>
#include <hnswlib/hnswlib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace drogon;

int main() {

  // Open file with our embedded image vectors
  const char *embeddingsFile = "../imageEmbeddings.txt";
  std::ifstream in(embeddingsFile, std::ios::binary);
  if (!in) {
    std::cerr << "Error opening file" << std::endl;
    return 1;
  }

  // Read the dimension of the embeddings
  int dim;
  in.read(reinterpret_cast<char *>(&dim), sizeof(int));
  if (!in) {
    std::cerr << "Error reading dimension" << std::endl;
    return 1;
  }

  // Params for HNSW alg
  int max_elements = 100;
  int M = 16;
  int ef_construction = 200;

  // Create HNSW index
  hnswlib::L2Space space(dim);
  hnswlib::HierarchicalNSW<float> *alg_hnsw =
      new hnswlib::HierarchicalNSW<float>(&space, max_elements, M,
                                          ef_construction);

  // Create vector to hold data we read from input file
  std::vector<float> currEmbedd(dim);
  int embeddCount = 0;

  // While there is data to read, go to front of each embedding and read data
  // into vector
  while (in.read(reinterpret_cast<char *>(currEmbedd.data()),
                 dim * sizeof(float))) {
    if (!in) {
      std::cerr << "Error reading embedding values" << std::endl;
      return 1;
    }

    // Add vector to index
    alg_hnsw->addPoint(currEmbedd.data(), embeddCount++);
  }

  in.close();
  std::cout << "Embeddings have been successfully added to the HNSW index"
            << std::endl;

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
