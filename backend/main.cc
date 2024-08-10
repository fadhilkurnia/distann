#include "server.h"

using json = nlohmann::json;
using namespace drogon;

#include "server.h"
//to start backend do: ./backend (port) (mode) (optional:forwarding_Mode) (optional:num_of_requests)
std::string mode = "backend";  // Default mode
std::string forwardingMode = "forward_random";  // Default forwarding mechanism
int num_of_requests = 1;  // Default number of requests per server
std::vector<std::pair<int, std::vector<double>>> port_latencies;
int num_of_total_requests = 0;
int power = 1; // Default number of servers to forward to
int request_left = 0; // Number of requests left to be processed
std::atomic<int> currentBackendIndex(0); // Global variable to keep track of the current backend index
std::string prompt = ""; 

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[1] << " <port> <mode> <forwarding_mode> <num_of_requests> <power> <prompt>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    mode = argv[2];

    if (argc > 3) {
        forwardingMode = argv[3];
    }

    if(argc > 4){
        num_of_requests = std::stoi(argv[4]);
    }

    if(argc > 5){
        power = std::stoi(argv[5]);
    }

    if(argc > 6){
        prompt = argv[6];
    }

    app().setLogLevel(trantor::Logger::kDebug);

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

void startBackend(int port){
      // Open file with our embedded image vectors
  const char *embeddingsFile = "../imageEmbeddings.txt";
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
  int max_elements = 1000;
  int M = 16;
  int ef_construction = 200;

  // Create HNSW index
  hnswlib::L2Space space(dim);
  hnswlib::HierarchicalNSW<float> *alg_hnsw =
      new hnswlib::HierarchicalNSW<float>(&space, max_elements, M,
                                          ef_construction);

  // Create vector to hold data we read from input file
  std::vector<float> currEmbedd(dim);
  int imageLabel = 0;

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

  app().registerHandler("/api/search", [&alg_hnsw](const HttpRequestPtr &req,
                                                   Callback &&callback) {
                          auto resp = HttpResponse::newHttpResponse();
                          auto prompt = req->getOptionalParameter<std::string>("prompt");

                          // prompt either exists or it does not, it is std::optional<std::string>, as
                          // such we should use prompt.value() in our curl code when sending post
                          // request

                          // handle empty prompt
                          if (!prompt.has_value()) {
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

                          CURL *curl;
                          CURLcode res;
                          std::string responseString; // Variable to store the response
                          std::vector<hnswlib::labeltype>
                              similarImages; // vector to store the labels returned from searchKnn

                          curl_global_init(CURL_GLOBAL_DEFAULT);
                          curl = curl_easy_init();

                          if (curl) {
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
                                  auto result = alg_hnsw->searchKnn(embedding.data(), 5);

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
                          response["prompt"] = prompt.value();
                          response["results"] = json::array();
                          for (size_t i = 0; i < similarImages.size(); i++) {
                            json img;
                            img["id"] = i + 1;
                            std::string label = std::to_string(similarImages[i]);
                            while (label.length() < 4) {
                              label.insert(0, 1, '0');
                            }
                            img["url"] = "http://localhost:9000/images/" + label + ".jpg";
                            img["alt"] = "Image: " + label + ".jpg";
                            response["results"].push_back(img);
                          }

                          resp->setBody(response.dump());
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
    app().registerHandler("/{path}", [num_of_requests](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, const std::string &path) {
        handleForwarding(req, std::move(callback), path, forwardingMode, num_of_requests);

        //loop until all requests are done
        while(request_left > -1){
            if(request_left == 0){
                json json_latencies;
                json_latencies["latencies"] = port_latencies;
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(json_latencies.dump());
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setStatusCode(k200OK);
                callback(resp);
                printLatencies();
                fastestResp();
                break;
            } 
            LOG_INFO << "Requests left: " << request_left << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    std::string host = "0.0.0.0";
    LOG_INFO << "Running distann proxy server on " << host << ":" << port
             << "\nforwarding mode: " << forwardingMode << " and " << num_of_requests << " requests\n";

    app().addListener(host, port).run();
}

void sendSingleRequest(const std::string &url, int port, const HttpRequestPtr &req, int reqNum,
                       std::function<void(const HttpResponsePtr &)> callback,
                       std::mutex &mutex) {
    auto client = HttpClient::newHttpClient(url);
    auto newReq = HttpRequest::newHttpRequest();
    newReq->setMethod(drogon::HttpMethod::Get);
    newReq->addHeader("Accept", "application/json");

    auto start = std::chrono::high_resolution_clock::now();

    client->sendRequest(newReq, [url, port, reqNum, callback, start, &mutex](ReqResult result, const HttpResponsePtr &resp) {
        auto end = std::chrono::high_resolution_clock::now();
        double latency = std::chrono::duration<double, std::milli>(end - start).count();
        num_of_total_requests++;
        std::ostringstream oss;
        if (result == ReqResult::Ok) {
            LOG_INFO << "Successfully forwarded to " << url << " on request number: " + std::to_string(reqNum) << "\n";

            std::string contentType = resp->getHeader("Content-Type");
            LOG_INFO << "Content-Type: " << contentType;

            std::string responseBody(resp->getBody().data(), resp->getBody().size());
            LOG_INFO << "Response from " << url << ": " << responseBody;

            oss << "Response from " << url << ": " << resp->getBody() << "\n";
        } else {
            LOG_INFO << "Failed to forward to " << url << "\n";
            oss << "Failed to connect to backend server: " << url << "\n";
        }

        std::lock_guard<std::mutex> lock(mutex);

        // Add latency to port_latencies
        auto it = std::find_if(port_latencies.begin(), port_latencies.end(),
                                [port](const std::pair<int, std::vector<double>> &p) { return p.first == port; });
        if (it != port_latencies.end()) {
            it->second.push_back(latency);
        } else {
            port_latencies.push_back(std::make_pair(port, std::vector<double>{latency}));
        }

        request_left--;
        LOG_INFO << "Request left in thread: " << request_left;
    });
}

void sendGetRequestsToServer(const std::string &url, int port, const HttpRequestPtr &req, int num_of_requests,
                            std::function<void(const HttpResponsePtr &)> callback,
                             std::mutex &mutex) {
    std::vector<std::thread> threads;
    for (int reqNum = 0; reqNum < num_of_requests; reqNum++) {
        LOG_INFO << "Thread request number: " << reqNum << " to " << url;
        threads.emplace_back(std::thread(sendSingleRequest, url, port, req, reqNum, callback, std::ref(mutex)));
    }

    for (auto &thread : threads) {
        thread.join();
    }
}

void handleForwarding(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &path, const std::string &forwardingMode, int num_of_requests) {
    std::vector<std::pair<std::string, int>> backendUrls = {
        {"http://localhost:10000", 10000},
        {"http://localhost:11000", 11000},
        {"http://localhost:12000", 12000}
    };

    //append /api/search?prompt= to each backend url
        for (auto &url : backendUrls) {
            url.first += "/api/search?prompt=" + prompt;
        }

    std::mutex mutex;

    if (forwardingMode == "forward_all") {
        LOG_INFO << "Forwarding All" << "\n";
        for (const auto &url : backendUrls) {
            LOG_INFO << "Thread Forwarding to " + url.first;
            request_left += num_of_requests;
            std::thread(sendGetRequestsToServer, url.first, url.second, req, num_of_requests, callback, std::ref(mutex)).detach();
        }
    } else if (forwardingMode == "forward_round") {
        // Round-robin fashion
        LOG_INFO << "Forwardning Round Robin";
        request_left += num_of_requests;
        for (int i = 0; i < num_of_requests; ++i) {
            int backendCount = backendUrls.size();
            int currentIndex = currentBackendIndex.fetch_add(1) % backendCount;
            auto& selectedBackend = backendUrls[currentIndex];
            LOG_INFO << "Forwarding to " << selectedBackend.first << " in round-robin fashion";
            LOG_INFO << "request number: " << i << " to " << selectedBackend.first;
            // Call the function to send the request to the server sequentially
            sendSingleRequest(selectedBackend.first, selectedBackend.second, req, i, callback, mutex);
        }
    } else if (forwardingMode == "forward_random") {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, backendUrls.size() - 1);
        int randomIndex = dis(gen);
        LOG_INFO << "Forwarding Random" << "\n";
        request_left += num_of_requests;
        LOG_INFO << "Forwarding to " << backendUrls[randomIndex].first << "\n";

        std::thread(sendGetRequestsToServer, backendUrls[randomIndex].first, backendUrls[randomIndex].second, req, num_of_requests, callback, std::ref(mutex)).detach();
    } else if (forwardingMode == "forward_fastest"){
        LOG_INFO << "Forwarding Power to " << power << " servers"<< "\n";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(backendUrls.begin(), backendUrls.end(), gen);
        
        for(int i = 0; i < power; i++){
            request_left += num_of_requests;
            LOG_INFO << "Forwarding to " << backendUrls[i].first << "\n";
            std::thread(sendGetRequestsToServer, backendUrls[i].first, backendUrls[i].second, req, num_of_requests, callback, std::ref(mutex)).detach();
        }

    }
}


void printLatencies() {
    for (const auto &port_latency : port_latencies) {
        std::cout << "Port: " << port_latency.first << std::endl;
        int count = 0;
        for (const auto &latency : port_latency.second) {
            count++;
            std::cout << latency << ", ";
        }
        std::cout << "\nnum of latency: "<< count << std::endl;
    }
    std::cout << "Total number of requests: " << num_of_total_requests << std::endl;
}

int fastestResp(){
    double fastest_time = std::numeric_limits<double>::max();
    int fastest_port = -1;

    // Iterate over each pair in the vector
    for (const auto& port_latency_pair : port_latencies) {
        int port = port_latency_pair.first;
        const std::vector<double>& latencies = port_latency_pair.second;

        auto min_latency_it = std::min_element(latencies.begin(), latencies.end());
        double min_latency = *min_latency_it;

        if (min_latency < fastest_time) {
            fastest_time = min_latency;
            fastest_port = port;
        }
    }

    std::cout << "The fastest response time is " << fastest_time << " ms on port " << fastest_port << "." << std::endl;
    return fastest_port;
}
