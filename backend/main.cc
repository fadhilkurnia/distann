#include "server.h"

    std::string mode = "backend";  // Default mode
    std::string forwardingMode = "forward_random";  // Default forwarding mechanism
    std::string promptType = "word"; // Default prompt type

using json = nlohmann::json;
using namespace drogon;

#include "server.h"
//to start backend do: ./backend (port) (mode) (optional:forwarding_Mode) (optional: promptType)
int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[1] << " <port> <mode> <forwarding_mode> <prompt>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    mode = argv[2];

    if (argc > 3) {
        forwardingMode = argv[3];
    }

    if(argc > 4){
        promptType = argv[4];
    }

    if (mode == "backend") {
        startBackend(port);
    } else if (mode == "proxy") {
        startProxy(port);
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

  app().registerHandler("/api/search", [&alg_hnsw, port](const HttpRequestPtr &req,
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
                            // Set the URL to send the request to the API server running on port/2
                            // 
                            std::string url = "http://127.0.0.1:"+ std::to_string(port/2)+"/embed";
                            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

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
                            std::string port_str = std::to_string(port);
                            img["url"] = "http://localhost:" + port_str + "/images/" + label + ".jpg";
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


void startProxy(int port) {
    std::vector<double> port_latencies = {};
    
    std::vector<std::string> backendUrls = {
        {"http://localhost:10000"},
        {"http://localhost:11000"},
        {"http://localhost:12000"},
        {"http://localhost:13000"},
        {"http://localhost:14000"},
        {"http://localhost:15000"},
    };

    std::vector<std::string> word = {
     "cat",
     "dog",
     "car",
     "plane",
     "bird",
     "flower"
     };
    std::vector<std::string> sentence  = {
        "The cat is on the mat",
        "The dog is in the house",
        "The car is on the road",
        "The plane is in the sky",
        "The bird is in the tree",
        "The flower is in the garden"
    };
    std::vector<std::string> mixed = {
        "The cat is on the mat",
        "bird",
        "The car is on the road",
        "plane",
        "The plane is in the sky",
        "car"
    };

    for (int i = 0; i < backendUrls.size(); i++) {
        if (promptType == "word") {
            backendUrls[i] += "/api/search?prompt=" + word[i];
        } else if (promptType == "sentence") {
            backendUrls[i] += "/api/search?prompt=" + sentence[i];
        } else if (promptType == "mixed") {
            backendUrls[i] += "/api/search?prompt=" + mixed[i];
        }
    }
    
    std::optional<std::string> first_response = std::nullopt;
    std::mutex mutex;
    std::atomic<int> currentBackendIndex(0); // Variable to keep track of the current backend index for round-robin forwarding

    app().registerHandler("/", [&currentBackendIndex, &port_latencies, &backendUrls, &first_response, &mutex](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
        handleForwarding(forwardingMode, port_latencies, backendUrls, first_response, mutex, currentBackendIndex);

        // loop until all requests are done
        LOG_INFO << "All requests are done. Setting upp the JSON...";
                json json_latencies;
                json_latencies["Forwarding_mode"] = forwardingMode;
                json_latencies["Latency"] = port_latencies;
                auto resp = HttpResponse::newHttpResponse();
                resp->setBody(json_latencies.dump());
                resp->setContentTypeCode(CT_APPLICATION_JSON);
                resp->setStatusCode(k200OK);
                callback(resp);
    });

    // New handler to respond to /call which shows the callback response of the latest/fastest request from the backend 
    app().registerHandler("/call", [&first_response, &mutex](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
        std::string response_body;

        // Lock the mutex to access the first_response
        mutex.lock();
        {
            if (first_response.has_value()) {
                response_body = first_response.value();
            } else {
                response_body = "No response available yet.";
            }
        }
        mutex.unlock();

        // Create the response object and send it back
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody(response_body);
        resp->setContentTypeCode(CT_TEXT_PLAIN);
        resp->setStatusCode(k200OK);
        callback(resp);
    });

    std::string host = "0.0.0.0";
    LOG_INFO << "Running distann proxy server on " << host << ":" << port
             << "\nforwarding mode: " << forwardingMode << "\n";

    app().addListener(host, port).run();
}

void sendSingleRequest(const std::string &url,
                       std::mutex &mutex,
                       std::optional<std::string> &first_response, 
                       std::condition_variable &cv,
                       std::atomic<bool> &is_first_request_done) {
    CURL* curl;
    CURLcode res;
    std::string responseString; // Variable to store the response

    curl = curl_easy_init();
    if (curl) {
        // Set the URL to send the request 
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        // Set the callback function to capture response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        // Perform the request
        // std::cout << "Sending request to " << url << std::endl;
        res = curl_easy_perform(curl);

        if (CURLE_OK == res) {

                // Lock the mutex to update shared variables
                std::lock_guard<std::mutex> lock(mutex);

                // Capture the first response
                if (!first_response.has_value()) {
                    LOG_INFO << "First response received: " << url;
                    first_response.emplace(responseString);
                }  
        } else {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
    } else {
        std::cerr << "Failed to initialize curl" << std::endl;
    }

    // Notify that the first response has been received
    is_first_request_done.store(true);
    cv.notify_all();
}

void handleForwarding(const std::string &forwardingMode, 
                      std::vector<double> &port_latencies,
                      std::vector<std::string> &backendUrls,
                      std::optional<std::string> &first_response,
                      std::mutex &mutex, 
                      std::atomic<int> &currentBackendIndex) {

    std::vector<std::thread> threads;
    std::condition_variable cv;
    std::atomic<bool> is_first_request_done(false);

    // Initialize the temporary current fastest response
    first_response = std::nullopt;

    if (forwardingMode == "forward_all") {
        LOG_INFO << "Forwarding All" << "\n";

        // Send requests to all backends and starts the timer
        auto start_time = std::chrono::high_resolution_clock::now();
        for (const auto &url : backendUrls) {
            threads.emplace_back(sendSingleRequest, 
                                 url,
                                 std::ref(mutex), 
                                 std::ref(first_response),
                                 std::ref(cv),
                                 std::ref(is_first_request_done));
        }
            // Wait for the first response to be set
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&] { return is_first_request_done.load(); });
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        // Calculate the elapsed time
        std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
        port_latencies.push_back(elapsed_time.count());

    } else if (forwardingMode == "forward_round") {
        LOG_INFO << "Forwarding Round Robin" << "\n";
        
        int backendCount = backendUrls.size();
        int currentIndex = currentBackendIndex.fetch_add(1) % backendCount;
        auto& selectedBackend = backendUrls[currentIndex];

        auto start_time = std::chrono::high_resolution_clock::now();
        threads.emplace_back(sendSingleRequest, 
                             selectedBackend, 
                             std::ref(mutex), 
                             std::ref(first_response),
                             std::ref(cv),
                             std::ref(is_first_request_done));

        // Wait for the first response to be set
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&] { return is_first_request_done.load(); });
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        // Calculate the elapsed time
        std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
        port_latencies.push_back(elapsed_time.count());

    } else if (forwardingMode == "forward_random") {
        LOG_INFO << "Forwarding Random" << "\n";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, backendUrls.size() - 1);
        int randomIndex = dis(gen);

        auto start_time = std::chrono::high_resolution_clock::now();
        threads.emplace_back(sendSingleRequest, 
                             backendUrls[randomIndex], 
                             std::ref(mutex), 
                             std::ref(first_response),
                             std::ref(cv),
                             std::ref(is_first_request_done));
        // Wait for the first response to be set
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&] { return is_first_request_done.load(); });
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        // Calculate the elapsed time
        std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
        port_latencies.push_back(elapsed_time.count());

    } else if (forwardingMode == "forward_two") {
        LOG_INFO << "Forwarding Two" << "\n";

        // Randomly select two backends that is different
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, backendUrls.size() - 1);
        int randomIndex1 = dis(gen);
        int randomIndex2;
        do {
            randomIndex2 = dis(gen);
        } while (randomIndex2 == randomIndex1);

        // Send requests to the two randomly selected backends
        auto start_time = std::chrono::high_resolution_clock::now();
        threads.emplace_back(sendSingleRequest, 
                            backendUrls[randomIndex1], 
                            std::ref(mutex), 
                            std::ref(first_response),
                            std::ref(cv),
                            std::ref(is_first_request_done));
        
        threads.emplace_back(sendSingleRequest, 
                            backendUrls[randomIndex2], 
                            std::ref(mutex), 
                            std::ref(first_response),
                            std::ref(cv),
                            std::ref(is_first_request_done));    
        // Wait for the first response to be set
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&] { return is_first_request_done.load(); });
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        // Calculate the elapsed time
        std::chrono::duration<double, std::milli> elapsed_time = end_time - start_time;
        port_latencies.push_back(elapsed_time.count());
    }


    // Ensure all threads are joined
    for (auto &thread : threads) {
        std::ostringstream id_stream;
        id_stream << thread.get_id();
        std::string thread_id_str = id_stream.str();
        thread.join();
    }
    
    LOG_INFO << "All threads joined";

    // In case no response was set during request processing
    if (!first_response.has_value()) {
        // Set the first response to the last response
        first_response.emplace("No response received");
    }
}

