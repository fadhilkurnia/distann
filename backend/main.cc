// distann - backend & backend-proxy

#include <curl/curl.h>
#include <drogon/drogon.h>
#include <iostream>
#include <string>
#include <vector>

#include "hnswlib/hnswlib.h"
#include "nlohmann/json/json.hpp"

using json = nlohmann::json;
using namespace drogon;

std::string mode            = "backend";        // Default mode
std::string forwardingMode  = "forward_random"; // Default forwarding mechanism
int         num_of_requests = 1; // Default number of requests per server
int power = 1; // Default number of servers to forward to for fastest n number
               // of servers
std::string promptType = "word"; // Default prompt type

// ========================================================================== //
//                           Begin Function Declaration                       //
// ========================================================================== //
void   startBackend(int port);
void   startProxy(int port, int num_of_requests);
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *s);
void   sendSingleRequest(
      const std::string &url, int port, const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> callback, std::mutex &mutex,
      std::vector<std::pair<int, std::vector<double>>> &port_latencies,
      int &request_left, int &num_of_total_requests);
void handleForwarding(
    const HttpRequestPtr                          &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &forwardingMode, int num_of_requests,
    std::vector<std::pair<int, std::vector<double>>> &port_latencies,
    std::vector<std::pair<std::string, int>> &backendUrls, int &request_left,
    int &num_of_total_requests);
std::pair<int, double> fastestResp(
    const std::vector<std::pair<int, std::vector<double>>> &port_latencies);
void addLatency(
    std::vector<std::pair<int, std::vector<double>>> &port_latencies, int port,
    double latency);

// ========================================================================== //
//                            End Function Declaration                        //
// ========================================================================== //

// to start backend do: ./backend (port) (mode) (optional:forwarding_Mode)
// (optional:num_of_requests)
int main(int argc, char *argv[]) {
  std::string binary_name = argv[0];

  if (argc < 2) {
    std::cerr << "Usage: " << std::endl
              << "    " << binary_name
              << " <port> <mode> <forwarding_mode> <num_of_requests> <power> "
                 "<prompt>\n";
    return 1;
  }

  int port = std::stoi(argv[1]);
  mode     = argv[2];

  if (argc > 3) {
    forwardingMode = argv[3];
  }

  if (argc > 4) {
    num_of_requests = std::stoi(argv[4]);
  }

  if (argc > 5) {
    power = std::stoi(argv[5]);
  }

  if (argc > 6) {
    promptType = argv[6];
  }

  std::cout << "Running distann server with the following config:\n"
            << " - port         : " << port << std::endl
            << " - server mode  : " << mode << std::endl
            << " - serving mode : " << forwardingMode << std::endl
            << std::endl;

  if (mode == "backend") {
    startBackend(port);
  } else if (mode == "proxy") {
    startProxy(port, num_of_requests);
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

// thread pool

class ThreadPool {
public:
  ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
      workers.emplace_back([this]() {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this]() { return !tasks.empty() || stop; });
            if (stop && tasks.empty())
              return;
            task = std::move(tasks.front());
            tasks.pop();
          }
          task();
        }
      });
    }
  }

  template <class F> void enqueue(F &&f) {
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      if (stop)
        throw std::runtime_error("enqueue on stopped ThreadPool");
      tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers) {
      worker.join();
    }
  }

private:
  std::vector<std::thread>          workers;
  std::queue<std::function<void()>> tasks;
  std::mutex                        queueMutex;
  std::condition_variable           condition;
  bool                              stop = false;
};

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

  using Callback = std::function<void(const HttpResponsePtr &)>;

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

void startProxy(int port, int num_of_requests) {
  std::vector<std::pair<int, std::vector<double>>> port_latencies = {
      {10000, {}}, {11000, {}}, {12000, {}},
      {13000, {}}, {14000, {}}, {15000, {}},
  };

  std::vector<std::pair<std::string, int>> backendUrls = {
      {"http://localhost:10000", 10000}, {"http://localhost:11000", 11000},
      {"http://localhost:12000", 12000}, {"http://localhost:13000", 13000},
      {"http://localhost:14000", 14000}, {"http://localhost:15000", 15000},
  };

  std::vector<std::string> word     = {"cat",   "dog",  "car",
                                       "plane", "bird", "flower"};
  std::vector<std::string> sentence = {
      "The cat is on the mat",   "The dog is in the house",
      "The car is on the road",  "The plane is in the sky",
      "The bird is in the tree", "The flower is in the garden"};
  std::vector<std::string> mixed = {"The cat is on the mat",   "bird",
                                    "The car is on the road",  "plane",
                                    "The plane is in the sky", "car"};

  int request_left          = 0; // Number of requests left to be processed
  int num_of_total_requests = 0; // Total number of requests processed

  for (int i = 0; i < backendUrls.size(); i++) {
    if (promptType == "word") {
      backendUrls[i].first += "/api/search?prompt=" + word[i];
    } else if (promptType == "sentence") {
      backendUrls[i].first += "/api/search?prompt=" + sentence[i];
    } else if (promptType == "mixed") {
      backendUrls[i].first += "/api/search?prompt=" + mixed[i];
    }
  }

  app().registerHandler(
      "/",
      [&num_of_requests, &port_latencies, &backendUrls, &request_left,
       &num_of_total_requests](
          const HttpRequestPtr                          &req,
          std::function<void(const HttpResponsePtr &)> &&callback) mutable {
        // try not to use global function
        // define counter in lambda function
        // 1. store the callback function
        // 2. spawn requests, each sending a request to a different backend
        // server . response; . response.push_back(response); .count = 5
        // 3. get the fastest response time and return it to the user
        // concurrent vector / hashmap

        handleForwarding(req, std::move(callback), forwardingMode,
                         num_of_requests, port_latencies, backendUrls,
                         request_left, num_of_total_requests);

        // loop until all requests are done
        while (request_left > -1) {
          if (request_left == 0) {
            json json_latencies;
            json_latencies["total_requests"]   = num_of_total_requests;
            json_latencies["fastest_response"] = fastestResp(port_latencies);
            json_latencies["latencies"]        = port_latencies;
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(json_latencies.dump());
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            resp->setStatusCode(k200OK);
            callback(resp);
            // printLatencies(port_latencies);
            break;
          }
          LOG_INFO << "Requests left: " << request_left << "\n";
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      });

  std::string host = "0.0.0.0";
  LOG_INFO << "Running distann proxy server on " << host << ":" << port
           << "\nforwarding mode: " << forwardingMode << " and "
           << num_of_requests << " requests\n";

  app().addListener(host, port).run();
}

void sendSingleRequest(
    const std::string &url, int port, const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> callback, std::mutex &mutex,
    std::vector<std::pair<int, std::vector<double>>> &port_latencies,
    int &request_left, int &num_of_total_requests) {

  auto start = std::chrono::high_resolution_clock::now();

  CURL       *curl;
  CURLcode    res;
  std::string readBuffer;
  LOG_INFO << "Sending request to " << url;
  curl = curl_easy_init();
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    res = curl_easy_perform(curl);

    auto   end = std::chrono::high_resolution_clock::now();
    double latency =
        std::chrono::duration<double, std::milli>(end - start).count();

    if (res != CURLE_OK) {
      std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res)
                << std::endl;
    } else {
      LOG_INFO << "Success response from " << url;
      num_of_total_requests++;
    }

    std::unique_lock<std::mutex> lock(mutex);

    addLatency(port_latencies, port, latency);
    request_left--;
    LOG_INFO << "Request left in thread: " << request_left;

    curl_easy_cleanup(curl);
  } else {
    std::cerr << "Failed to initialize curl" << std::endl;
  }
}

void sendGetRequestsToServer(
    const std::string &url, int port, const HttpRequestPtr &req,
    int num_of_requests, std::function<void(const HttpResponsePtr &)> callback,
    std::mutex                                       &mutex,
    std::vector<std::pair<int, std::vector<double>>> &port_latencies,
    int &request_left, int &num_of_total_requests) {

  ThreadPool pool(50); // Use a thread pool with # of worker threads

  for (int reqNum = 0; reqNum < num_of_requests; ++reqNum) {
    pool.enqueue([&, url, port]() {
      sendSingleRequest(url, port, req, callback, std::ref(mutex),
                        std::ref(port_latencies), std::ref(request_left),
                        std::ref(num_of_total_requests));
    });
  }
}

void handleForwarding(
    const HttpRequestPtr                          &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &forwardingMode, int num_of_requests,
    std::vector<std::pair<int, std::vector<double>>> &port_latencies,
    std::vector<std::pair<std::string, int>> &backendUrls, int &request_left,
    int &num_of_total_requests) {

  std::atomic<int> currentBackendIndex(
      0); // variable to keep track of the current backend index for round-robin
          // forwarding
  std::mutex               mutex;
  std::vector<std::thread> threads;

  if (forwardingMode == "forward_all") {
    LOG_INFO << "Forwarding All" << "\n";
    for (const auto &url : backendUrls) {
      request_left += num_of_requests;
      threads.emplace_back(sendGetRequestsToServer, url.first, url.second, req,
                           num_of_requests, callback, std::ref(mutex),
                           std::ref(port_latencies), std::ref(request_left),
                           std::ref(num_of_total_requests));
    }

  } else if (forwardingMode == "forward_round") {
    LOG_INFO << "Forwarding Round Robin";
    request_left += num_of_requests;
    for (int i = 0; i < num_of_requests; ++i) {
      int   backendCount    = backendUrls.size();
      int   currentIndex    = currentBackendIndex.fetch_add(1) % backendCount;
      auto &selectedBackend = backendUrls[currentIndex];
      sendSingleRequest(selectedBackend.first, selectedBackend.second, req,
                        callback, mutex, port_latencies, request_left,
                        num_of_total_requests);
    }

  } else if (forwardingMode == "forward_random") {
    std::random_device              rd;
    std::mt19937                    gen(rd());
    std::uniform_int_distribution<> dis(0, backendUrls.size() - 1);
    int                             randomIndex = dis(gen);
    request_left += num_of_requests;
    threads.emplace_back(
        sendGetRequestsToServer, backendUrls[randomIndex].first,
        backendUrls[randomIndex].second, req, num_of_requests, callback,
        std::ref(mutex), std::ref(port_latencies), std::ref(request_left),
        std::ref(num_of_total_requests));

  } else if (forwardingMode == "forward_fastest") {
    LOG_INFO << "Forwarding Power to " << power << " servers" << "\n";
    for (int i = 0; i < power; i++) {
      request_left += num_of_requests;
      threads.emplace_back(sendGetRequestsToServer, backendUrls[i].first,
                           backendUrls[i].second, req, num_of_requests,
                           callback, std::ref(mutex), std::ref(port_latencies),
                           std::ref(request_left),
                           std::ref(num_of_total_requests));
    }
  }

  for (auto &thread : threads) {
    thread.join();
  }
}

std::pair<int, double> fastestResp(
    const std::vector<std::pair<int, std::vector<double>>> &port_latencies) {
  std::pair<int, double> fastest_port = {-1, -1};

  double min_latency = std::numeric_limits<double>::max();

  for (const auto &entry : port_latencies) {
    if (!entry.second.empty()) {
      for (double latency : entry.second) {
        if (latency < min_latency) {
          min_latency  = latency;
          fastest_port = {entry.first, latency};
        }
      }
    }
  }

  return fastest_port; // Returns an optional containing the port and the
                       // fastest latency or std::nullopt if no latency found.
}

void addLatency(
    std::vector<std::pair<int, std::vector<double>>> &port_latencies, int port,
    double latency) {
  for (auto &p : port_latencies) {
    if (p.first == port) {
      p.second.push_back(latency);
      return;
    }
  }
  std::cerr << "Port " << port << " not found.\n";
}
