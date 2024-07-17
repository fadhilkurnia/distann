#include <drogon/drogon.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include <hnswlib/hnswlib.h>

using namespace hnswlib;

using json = nlohmann::json;

using namespace drogon;

int port = 0;

int main(int argc, char *argv[]) {

    port = std::stoi(argv[1]);
  
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

        // TODO: convert prompt to an embedding e
        // TODO: do similarity search using e over the dataset

        //test hnswlib
    int dim = 16;               // Dimension of the elements
    int max_elements = 10000;   // Maximum number of elements, should be known beforehand
    int M = 16;                 // Tightly connected with internal dimensionality of the data
                                // strongly affects the memory consumption
    int ef_construction = 200;  // Controls index search speed/build speed tradeoff

        // Initing index
    hnswlib::L2Space space(dim);
    hnswlib::HierarchicalNSW<float>* alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, max_elements, M, ef_construction);

        // Generate random data
    std::mt19937 rng;
    rng.seed(47);
    std::uniform_real_distribution<> distrib_real;
    float* data = new float[dim * max_elements];
    for (int i = 0; i < dim * max_elements; i++) {
        data[i] = distrib_real(rng);
    }

    // Add data to index
    for (int i = 0; i < max_elements; i++) {
        alg_hnsw->addPoint(data + i * dim, i);
    }

    // Query the elements for themselves and measure recall
    float correct = 0;
    for (int i = 0; i < max_elements; i++) {
        std::priority_queue<std::pair<float, hnswlib::labeltype>> result = alg_hnsw->searchKnn(data + i * dim, 1);
        hnswlib::labeltype label = result.top().second;
        if (label == i) correct++;
    }
    float recall1 = correct / max_elements;
    std::cout << "Recall: " << recall1 << "\n";

    // Serialize index
    std::string hnsw_path = "hnsw.bin";
    alg_hnsw->saveIndex(hnsw_path);
    delete alg_hnsw;

    // Deserialize index and check recall
    alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, hnsw_path);
    correct = 0;
    for (int i = 0; i < max_elements; i++) {
        std::priority_queue<std::pair<float, hnswlib::labeltype>> result = alg_hnsw->searchKnn(data + i * dim, 1);
        hnswlib::labeltype label = result.top().second;
        if (label == i) correct++;
    }
    float recall2 = (float)correct / max_elements;
    std::cout << "Recall of deserialized index: " << recall2 << "\n";

        //test hnswlib end

        // TODO: return top 5 similar results

        // std::string response_content = "{\"prompt\": \"" + prompt.value() +
        // "\"}";
        std::string localHost = "http://localhost:" + std::to_string(port);
        std::string imageURL = localHost + "/images/cat.png";

        // Create our JSON response
        json response;
        response["prompt"] = prompt.value();
        response["results"] = json::array();
        json result;
        result["id"] = 1;
        result["url"] = imageURL;
        result["alt"] = "Cat";
        result["Recall"] = recall1;
        result["Recall2"] = recall2;
        response["results"].push_back(result);

        resp->setBody(response.dump());
        resp->setContentTypeCode(CT_APPLICATION_JSON);
        resp->setStatusCode(k200OK);
        callback(resp);
      });

  // run the backend server in the specified host/interface and port.
  
  std::string host = "0.0.0.0";
  LOG_INFO << "Running distann backend server on " << host << ":" << port;

  app().addListener(host, port).run();

  return 0;
}
