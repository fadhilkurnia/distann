#ifndef SERVER_H
#define SERVER_H

#include <unistd.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <map>
#include <algorithm>
#include <random>
#include <mutex>
#include <json/json.h>
#include <limits>
#include <optional>

#include <drogon/drogon.h>
#include <fstream>
// #include <hnswlib/hnswlib.h>
#include "libs/hnswlib/hnswlib.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// headers for the user embedding
#include <curl/curl.h>
#include <iostream>
#include <stdio.h>

using namespace hnswlib;
using json = nlohmann::json;
using namespace drogon;


void startBackend(int port);
void startProxy(int port);

size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *s);

void sendSingleRequest(const std::string &url,
                       std::mutex &mutex,
                       std::optional<std::string> &first_response, 
                       std::condition_variable &cv,
                       std::atomic<bool> &is_first_request_done);
                       
void handleForwarding(const std::string &forwardingMode, 
                      std::vector<double> &port_latencies,
                      std::vector<std::string> &backendUrls,
                      std::optional<std::string> &first_response,
                      std::mutex &mutex, 
                      std::atomic<int> &currentBackendIndex);


void addLatency(std::vector<std::pair<int, std::vector<double>>>& port_latencies, int port, double latency);

#endif // SERVER_H
