#include "server.h"

std::string mode = "backend";  // Default mode
std::string forwardingMode = "forward_one";  // Default forwarding mechanism

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <port> <mode> [forwarding_mode]" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    mode = argv[2];

    if (argc > 3) {
        forwardingMode = argv[3];
    }

    app().setLogLevel(trantor::Logger::kDebug);

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

void startProxy(int port) {
    app().registerHandler("/{path}", [](const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, const std::string &path) {
        handleForwarding(req, std::move(callback), path, forwardingMode);
    });

    // Register backend routes
    // startBackend(10000);
    // startBackend(11000);
    // startBackend(12000);

    std::string host = "0.0.0.0";
    LOG_INFO << "Running distann proxy server on " << host << ":" << port;

    app().addListener(host, port).run();
}

void handleForwarding(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, const std::string &path, const std::string &forwardingMode) {
    std::vector<std::string> backendUrls = {
        "http://localhost:10000",
        "http://localhost:11000",
        "http://localhost:12000"
    };

    LOG_INFO << "Forwarding request for path: " << path << " with forwarding mode: " << forwardingMode;

    if (forwardingMode == "forward_all") {
        std::shared_ptr<int> responsesLeft(new int(backendUrls.size()));
        std::shared_ptr<std::vector<std::string>> responses(new std::vector<std::string>());
        std::mutex mutex;

        for (const auto &url : backendUrls) {
            LOG_INFO << "Forwarding to " << url;
            auto client = HttpClient::newHttpClient(url);
            auto newReq = HttpRequest::newHttpRequest();
            newReq->setMethod(req->method());
            newReq->setPath(req->getPath());
            newReq->setBody(std::string(req->getBody()));

            // Forward headers
            for (const auto &header : req->getHeaders()) {
                newReq->addHeader(header.first, header.second);
            }

            client->sendRequest(newReq, [callback, url, responses, responsesLeft, &mutex](ReqResult result, const HttpResponsePtr &resp) {
                std::ostringstream oss;
                if (result == ReqResult::Ok) {
                    LOG_INFO << "Successfully forwarded to " << url;
                    oss << "Response from " << url << ": " << resp->getBody() << "\n";
                } else {
                    LOG_ERROR << "Failed to forward to " << url;
                    oss << "Failed to connect to backend server: " << url << "\n";
                }

                {
                    // Use a scoped lock to ensure thread safety when accessing shared data
                    std::lock_guard<std::mutex> lock(mutex);
                    responses->push_back(oss.str());
                    (*responsesLeft)--;
                }

                if (*responsesLeft == 0) {
                    auto aggregatedResponse = HttpResponse::newHttpResponse();
                    std::ostringstream aggregatedBody;
                    for (const auto &res : *responses) {
                        aggregatedBody << res;
                    }
                    aggregatedResponse->setBody(aggregatedBody.str());
                    aggregatedResponse->setStatusCode(k200OK);
                    callback(aggregatedResponse);
                }
            });
        }
    } else if (forwardingMode == "forward_one") {
        auto client = HttpClient::newHttpClient(backendUrls[0]);
        
        LOG_INFO << "Forwarding to " << backendUrls[0];

        auto newReq = HttpRequest::newHttpRequest();
        newReq->setMethod(req->method());
        newReq->setPath(req->getPath());
        newReq->setBody(std::string(req->getBody()));

        // Forward headers
        for (const auto &header : req->getHeaders()) {
            newReq->addHeader(header.first, header.second);
        }

        client->sendRequest(newReq, [callback](ReqResult result, const HttpResponsePtr &resp) {
            if (result == ReqResult::Ok) {
                LOG_INFO << "Successfully forwarded request";
                callback(resp);
            } else {
                LOG_ERROR << "Failed to forward request";
                auto errorResp = HttpResponse::newHttpResponse();
                errorResp->setStatusCode(k500InternalServerError);
                errorResp->setBody("Failed to connect to backend server");
                callback(errorResp);
            }
        });
    }
}
