#include "server.h"
//to start backend do: ./backend (port) (mode) (optional:forwarding_Mode) (optional:num_of_requests)
std::string mode = "backend";  // Default mode
std::string forwardingMode = "forward_one";  // Default forwarding mechanism
int num_of_requests = 1;  // Default number of requests per server
std::vector<std::pair<int, std::vector<double>>> port_latencies;
int num_of_total_requests = 0;
int power = 1; // Default number of servers to forward to

using json = nlohmann::json;


int main(int argc, char *argv[]) {
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

void startBackend(int port) {
    using Callback = std::function<void(const HttpResponsePtr &)>;

    app().registerHandler("/",
                          [](const HttpRequestPtr &req, Callback &&callback) {
                              auto resp = HttpResponse::newHttpResponse();
                              resp->setBody("Welcome to distann backend server :)");
                              resp->setStatusCode(k200OK);
                              callback(resp);
                          });

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
                return;
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
        "/api/search", [port](const HttpRequestPtr &req, Callback &&callback) {
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

            // test hnswlib
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

            // test hnswlib end

            // TODO: return top 5 similar results

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
}

int request_left = 0;

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
             << "\nforwarding mode: " << forwardingMode << " and " << num_of_requests << " requests per server\n";

    app().addListener(host, port).run();
}

void sendSingleRequest(const std::string &url, int port, const HttpRequestPtr &req, int reqNum,
                       std::function<void(const HttpResponsePtr &)> callback,
                       std::mutex &mutex) {
    auto client = HttpClient::newHttpClient(url);

    auto start = std::chrono::high_resolution_clock::now();

    client->sendRequest(req, [url, port, reqNum, callback, start, &mutex](ReqResult result, const HttpResponsePtr &resp) {
        auto end = std::chrono::high_resolution_clock::now();
        double latency = std::chrono::duration<double, std::milli>(end - start).count();
        num_of_total_requests++;
        std::ostringstream oss;
        if (result == ReqResult::Ok) {
            LOG_INFO << "Successfully forwarded to " << url << " on request number: " + std::to_string(reqNum) << "\n";
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

// Global variable to keep track of the current backend index
std::atomic<int> currentBackendIndex(0);

void handleForwarding(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &path, const std::string &forwardingMode, int num_of_requests) {
    std::vector<std::pair<std::string, int>> backendUrls = {
        {"http://localhost:10000", 10000},
        {"http://localhost:11000", 11000},
        {"http://localhost:12000", 12000}
    };

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
        LOG_INFO << "Forwardning Round Robin" << "\n";
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