//previous headers 
#include <drogon/drogon.h>
#include <fstream>
// #include <hnswlib/hnswlib.h>
#include "libs/hnswlib/hnswlib.h"
#include <iostream>
// #include <nlohmann/json.hpp>
#include "../../json.hpp"
#include <string>
#include <vector>

//headers for the user embedding 
#include <curl/curl.h>
#include <stdio.h>
#include <string>
#include <iostream>


using json = nlohmann::json;
using namespace drogon;

// Callback function to write response data into a string, for user embedding
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
  size_t totalSize = size * nmemb;
  s->append(static_cast<char*>(contents), totalSize);
  return totalSize;
}

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
            "/Linux/Ubuntu/home/adithya/URV/distann/backend/images/cocoSubset";
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

        //prompt either exists or it does not, it is std::optional<std::string>, as such
        //we should use prompt.value() in our curl code when sending post request 

        // handle empty prompt
        if (!prompt.has_value()) {
          resp->setStatusCode(k400BadRequest);
          resp->setBody("{\"error\": \"prompt unspecified\"}");
          resp->setContentTypeCode(CT_APPLICATION_JSON);
          callback(resp);
          return;
        }
        //I am too send a post request to my API, I want to eventually start the API 
        //directlley through C++ code but dont want to use system, looking into windows APIS for that

        //following code form curl documentation for simple HTTP-POST request 

        /*
        ****NOTES****
        Make sure to start running the API server before testing code, will eventually add 
        windows API C++ functionality which will run code directlly from here

        */

        //Start of user embedding code

        CURL* curl;
        CURLcode res;
        std::string responseString; // Variable to store the response

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();

        if(curl){
          curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/embed");

          // Prepare the user query to be sent to API
          json payload = {{"query", prompt.value()}};
          std::string payloadStr = payload.dump();

          // Set HTTP headers, as required by API
          struct curl_slist *headers = NULL;
          headers = curl_slist_append(headers, "Content-Type: application/json");
          curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

          // Set POST fields of call
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
          curl_easy_setopt(curl, CURLOPT_POST, 1L);

          // Set the callback function to capture response, converting CURL object to string to parse JSON
          curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
          curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

          // Perform the request
          res = curl_easy_perform(curl);

          if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
          } else {
            // Parse the JSON response
            try {
              auto data = json::parse(responseString);
              std::cout << data["embedding"][0];
            } catch (json::parse_error &e) {
              std::cerr << "JSON parse error: " << e.what() << std::endl;
            }
          }
          curl_easy_cleanup(curl);
          curl_slist_free_all(headers);
        }

        curl_global_cleanup();

        //End of user embedding code
      

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
