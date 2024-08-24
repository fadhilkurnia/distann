// distann - backend & backend-proxy

#include <atomic>
#include <curl/curl.h>
#include <drogon/drogon.h>
#include <iostream>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "hnswlib/hnswlib.h"
#include "nlohmann/json/json.hpp"

using json = nlohmann::json;
using namespace drogon;
using Callback = std::function<void(const HttpResponsePtr &)>;

const std::string default_server_mode = "backend"; // Default mode
const std::string default_serving_mode =
    "forward_random"; // Default forwarding mechanism
int power = 1; // Default number of servers to forward to for fastest n number
               // of servers

// prepare the backend hosts, and a random generator to chose the backend.
const std::vector<std::pair<std::string, int>> backend_hosts = {
    {"127.0.0.1", 10000}, {"127.0.0.1", 11000}, {"127.0.0.1", 12000},
    {"127.0.0.1", 13000}, {"127.0.0.1", 14000}, {"127.0.0.1", 15000},
};
const int num_backend_hosts = backend_hosts.size();

// ========================================================================== //
//                          Begin Function Declarations                       //
// ========================================================================== //
void startBackend(const int port);
void startProxy(const int port, const std::string &forward_mode);
void forwardRequest(const HttpRequestPtr &req, const std::string &forward_mode,
                    Callback &&callback);
void forwardRequestToOne(const HttpRequestPtr &req, Callback &&callback);
void forwardRequestToTwo(const HttpRequestPtr &req, Callback &&callback);
void forwardRequestToAll(const HttpRequestPtr &req, Callback &&callback);
void forwardRequestToN(const HttpRequestPtr &req, Callback &&callback,
                       const int n);
void forwardRequestWithRoundRobin(const HttpRequestPtr &req,
                                  Callback            &&callback);

size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s);

int getRandomInt(const int &min, const int &max) {
  static thread_local std::mt19937   generator;
  std::uniform_int_distribution<int> distribution(min, max);
  return distribution(generator);
}

// ========================================================================== //
//                           End Function Declarations                        //
// ========================================================================== //

// to start backend do: ./backend (port) (mode) (optional:forwarding_Mode)
// (optional:num_of_requests)
int main(int argc, char *argv[]) {
  const std::string binary_name = argv[0];

  if (argc < 4) {
    std::cerr << "Usage: " << std::endl
              << "    " << binary_name << " <port> <mode> <forward_mode>\n";
    return 1;
  }

  const int         port         = std::stoi(argv[1]);
  const std::string server_mode  = argv[2];
  const std::string forward_mode = argv[3];

  // validate the server mode
  if (server_mode != "backend" && server_mode != "proxy") {
    std::cerr << "Invalid server mode:" << server_mode << std::endl;
    std::cerr << "Valid server mode are 'backend' or 'proxy'" << std::endl;
    return 1;
  }

  // validate the forwarding mode
  std::vector<std::string> valid_forward_mode    = {"all", "random_one",
                                                    "random_two", "round_robin"};
  bool                     is_forward_mode_valid = false;
  for (auto &mode : valid_forward_mode) {
    if (forward_mode == mode) {
      is_forward_mode_valid = true;
      break;
    }
  }
  if (!is_forward_mode_valid) {
    std::cerr << "Invalid forwarding mode:" << forward_mode << std::endl;
    std::cerr << "Valid modes are ";
    for (auto &mode : valid_forward_mode) {
      std::cerr << "'" << mode << "' ";
    }
    std::cerr << std::endl;
    return 1;
  }

  std::cout << "Running distann server with the following config:\n"
            << " - port         : " << port << std::endl
            << " - server mode  : " << server_mode << std::endl
            << " - forward mode : " << forward_mode << std::endl
            << std::endl;

  if (server_mode == "backend") {
    startBackend(port);
  } else if (server_mode == "proxy") {
    startProxy(port, forward_mode);
  } else {
    std::cerr << "Invalid mode. Use 'backend' or 'proxy'." << std::endl;
    return 1;
  }

  return 0;
}

// Callback function to write response data into a string, for user embedding
size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *s) {
  size_t totalSize = size * nmemb;
  s->append(static_cast<char *>(contents), totalSize);
  return totalSize;
}

void startBackend(int port) {
  // Open file with our embedded image vectors
  const char   *embeddingsFile = "../data/imageEmbeddings.txt";
  std::ifstream in(embeddingsFile, std::ios::binary);
  if (!in) {
    std::cerr << "Error opening file" << std::endl;
    return;
  }

  // Read the dimension of the embeddings
  int dim;
  in.read(reinterpret_cast<char *>(&dim), sizeof(int));
  if (!in) {
    std::cerr << "Error reading dimension" << std::endl;
    return;
  }

  // Params for HNSW alg
  int max_elements    = 1000;
  int M               = 16;
  int ef_construction = 200;

  // Create HNSW index
  hnswlib::L2Space                 space(dim);
  hnswlib::HierarchicalNSW<float> *alg_hnsw =
      new hnswlib::HierarchicalNSW<float>(&space, max_elements, M,
                                          ef_construction);

  // Create vector to hold data we read from input file
  std::vector<float> currEmbedd(dim);
  int                imageLabel = 0;

  // While there is data to read, go to front of each embedding and read data
  // into vector
  while (in.read(reinterpret_cast<char *>(currEmbedd.data()),
                 dim * sizeof(float))) {
    if (!in) {
      std::cerr << "Error reading embedding values" << std::endl;
      return;
    }

    // Add vector to index
    alg_hnsw->addPoint(currEmbedd.data(), imageLabel++);
  }
  in.close();

  app().setLogLevel(trantor::Logger::kDebug);
  app().registerHandler("/",
                        [](const HttpRequestPtr &req, Callback &&callback) {
                          auto resp = HttpResponse::newHttpResponse();
                          resp->setBody("Welcome to distann backend server :)");
                          resp->setStatusCode(k200OK);
                          callback(resp);
                        });

  app().registerHandler("/images/{image_name}",
                        [](const HttpRequestPtr &req, Callback &&callback,
                           const std::string image_name) {
                          auto resp = HttpResponse::newHttpResponse();

                          std::string imageDirectory = "../images/cocoSubset/";
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
                          resp->setContentTypeCode(CT_IMAGE_JPG);
                          resp->setStatusCode(k200OK);
                          callback(resp);
                        });

  app().registerHandler("/api/search", [&alg_hnsw,
                                        port](const HttpRequestPtr &req,
                                              Callback            &&callback) {
    auto resp   = HttpResponse::newHttpResponse();
    auto prompt = req->getOptionalParameter<std::string>("prompt");

    // prompt either exists or it does not, it is std::optional<std::string>, as
    // such we should use prompt.value() in our curl code when sending post
    // request

    // handle empty prompt
    if (!prompt.has_value() || prompt.value() == "") {
      resp->setStatusCode(k400BadRequest);
      resp->setBody("{\"error\": \"prompt unspecified\"}");
      resp->setContentTypeCode(CT_APPLICATION_JSON);
      callback(resp);
      return;
    }
    // I am too send a post request to my API, I want to eventually start the
    // API directlley through C++ code but dont want to use system, looking into
    // windows APIS for that

    // following code form curl documentation for simple HTTP-POST request

    /*
    ****NOTES****
    Make sure to start running the API server before testing code, will
    eventually add windows API C++ functionality which will run code directlly
    from here

    */

    // Start of user embedding code

    CURL       *curl;
    CURLcode    res;
    std::string responseString; // Variable to store the response
    std::vector<hnswlib::labeltype>
        similarImages; // vector to store the labels returned from searchKnn

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/embed");

      // Prepare the user query to be sent to API
      json        payload    = {{"query", prompt.value()}};
      std::string payloadStr = payload.dump();

      // Set HTTP headers, as required by API
      struct curl_slist *headers = NULL;
      headers = curl_slist_append(headers, "Content-Type: application/json");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

      // Set POST fields of call
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
      curl_easy_setopt(curl, CURLOPT_POST, 1L);

      // Set the callback function to capture response, converting CURL object
      // to string to parse JSON
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

      // Perform the request
      res = curl_easy_perform(curl);

      if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
      } else {
        try {
          // Parse the JSON response
          auto data = json::parse(responseString);
          if (data.contains("embedding")) {
            std::vector<float> embedding =
                data["embedding"].get<std::vector<float>>();
            auto result = alg_hnsw->searchKnn(embedding.data(), 12);

            // Convert the priority queue to a vector with our labels
            while (!result.empty()) {
              similarImages.push_back(result.top().second);
              result.pop();
            }
          } else {
            std::cerr << "No embedding found" << std::endl;
          }
        } catch (json::parse_error &e) {
          std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
      }
      curl_easy_cleanup(curl);
      curl_slist_free_all(headers);
    }
    curl_global_cleanup();

    // Create JSON response
    json response;
    response["prompt"]  = prompt.value();
    response["results"] = json::array();
    for (size_t i = 0; i < similarImages.size(); i++) {
      json img;
      img["id"]         = i + 1;
      std::string label = std::to_string(similarImages[i]);
      while (label.length() < 4) {
        label.insert(0, 1, '0');
      }
      std::string port_str = std::to_string(port);
      img["url"] = "http://localhost:" + port_str + "/images/" + label + ".jpg";
      img["alt"] = "Image: " + label + ".jpg";
      response["results"].push_back(img);
    }

    resp->setBody(response.dump());
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setStatusCode(k200OK);
    callback(resp);
  });

  // run the backend server in the specified host/interface and port.
  std::string host = "0.0.0.0";
  LOG_INFO << "Running distann backend server on " << host << ":" << port;
  app().addListener(host, port).run();
}

void startProxy(const int port, const std::string &forward_mode) {
  const std::string default_forward_mode = "random_one";
  app().registerHandler(
      "/api/search",
      [&default_forward_mode](const HttpRequestPtr &req, Callback &&callback) {
        LOG_INFO << "Receiving request ...";
        auto forward_mode = req->getOptionalParameter<std::string>("forward");
        if (!forward_mode.has_value()) {
          forward_mode.emplace(default_forward_mode);
        }

        forwardRequest(req, forward_mode.value(), std::move(callback));
        return;
      });

  const std::string host = "0.0.0.0";
  LOG_INFO << "Running distann proxy server on " << host << ":" << port << ". "
           << "Forwarding mode '" << forward_mode << "'.";
  app().addListener(host, port).run();
}

void forwardRequest(const HttpRequestPtr &req, const std::string &forward_mode,
                    Callback &&callback) {
  // TODO: use persistent client for each backend, for better performance.

  LOG_INFO << "Forwarding request with mode=" << forward_mode;

  if (forward_mode == "random_one") {
    forwardRequestToOne(req, std::move(callback));
    return;

  } else if (forward_mode == "random_two") {
    forwardRequestToTwo(req, std::move(callback));
    return;

  } else if (forward_mode == "all") {
    forwardRequestToAll(req, std::move(callback));
    return;

  } else {
    auto response = HttpResponse::newHttpResponse();
    response->setBody("Unknown forwarding mode.");
    response->setStatusCode(k400BadRequest);
    callback(response);
    return;
  }
}

void forwardRequestToOne(const HttpRequestPtr &req, Callback &&callback) {
  // prepare the target backend server to forward the request to
  int         random_backend_id        = getRandomInt(0, num_backend_hosts - 1);
  auto        target_backend_host_pair = backend_hosts[random_backend_id];
  std::string target_backend_ip        = target_backend_host_pair.first;
  int         target_backend_port      = target_backend_host_pair.second;

  // forward the request to the target backend, synchronously.
  // TODO: use persistent client for each backend, for better performance.
  HttpClientPtr client =
      HttpClient::newHttpClient(target_backend_ip, target_backend_port);
  auto req_result = client->sendRequest(req);

  // move the response for the proxy's client.
  auto response = HttpResponse::newHttpResponse();
  response      = std::move(req_result.second);

  callback(response);
  return;
}

void forwardRequestToTwo(const HttpRequestPtr &req, Callback &&callback) {
  std::vector<std::thread>       threads;
  std::atomic<bool>              is_first_request_done = false;
  std::mutex                     response_lock;
  std::optional<HttpResponsePtr> first_response;

  // a lambda function that actually send request to backend, and record the
  // first response among other threads.
  auto sendRequestSetFastestResponse =
      [&req, &is_first_request_done, &response_lock,
       &first_response](HttpClientPtr &client) {
        LOG_INFO << "Sending request to " << client->getPort();
        auto req_result = client->sendRequest(req);
        if (req_result.first != ReqResult::Ok) {
          return;
        }

        response_lock.lock();
        if (!first_response.has_value()) {
          first_response.emplace(req_result.second);
        }
        response_lock.unlock();

        // Notify that we have a response received already.
        // It is fine to have multiple threads set the value to `true`.
        is_first_request_done = true;
        is_first_request_done.notify_all();
      };

  std::vector<HttpClientPtr> http_clients;
  std::unordered_set<int>    used_backend_ids;
  for (int i = 0; i < 3; ++i) {
    int random_backend_id = getRandomInt(0, num_backend_hosts - 1);
    while (used_backend_ids.contains(random_backend_id)) {
      random_backend_id = getRandomInt(0, num_backend_hosts - 1);
    }
    used_backend_ids.insert(random_backend_id);

    auto        target_backend_host_pair = backend_hosts[random_backend_id];
    std::string target_backend_ip        = target_backend_host_pair.first;
    int         target_backend_port      = target_backend_host_pair.second;
    http_clients.push_back(
        HttpClient::newHttpClient(target_backend_ip, target_backend_port));
  }

  // use threads to send requests
  for (auto &client : http_clients) {
    threads.push_back(
        std::thread(sendRequestSetFastestResponse, std::ref(client)));
  }

  // Wait for the fastest response. That is until the value is not `false`.
  is_first_request_done.wait(false);

  // Copy the response so we can forward it back to the client.
  HttpResponsePtr copy_response;
  response_lock.lock();
  copy_response = std::move(first_response.value());
  response_lock.unlock();

  // Call back the client, sending the response.
  callback(copy_response);

  // Cleaning up all the threads, ensuring all of them terminate.
  for (std::thread &t : threads) {
    t.join();
  }

  return;
}

void forwardRequestToN(const HttpRequestPtr &req, Callback &&callback,
                       const int n) {
  auto response = HttpResponse::newHttpResponse();
  response->setBody("Forward to N is unimplemented :(");
  response->setStatusCode(k501NotImplemented);
  callback(response);
  return;
}

void forwardRequestToAll(const HttpRequestPtr &req, Callback &&callback) {
  auto response = HttpResponse::newHttpResponse();
  response->setBody("Forward to all is unimplemented :(");
  response->setStatusCode(k501NotImplemented);
  callback(response);
  return;
}

void forwardRequestWithRoundRobin(const HttpRequestPtr &req,
                                  Callback            &&callback) {
  auto response = HttpResponse::newHttpResponse();
  response->setBody("Forward with round robin is unimplemented :(");
  response->setStatusCode(k501NotImplemented);
  callback(response);
  return;
}
