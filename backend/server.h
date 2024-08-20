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
void startProxy(int port, int numRequests);

size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     std::string *s);

void sendSingleRequest(const std::string &url, int port, const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> callback,
                       std::mutex &mutex,
                       std::vector<std::pair<int, std::vector<double>>> &port_latencies,
                       int &request_left, int &num_of_total_requests);
                       
void sendGetRequestsToServer(const std::string &url, int port, const HttpRequestPtr &req, int num_of_requests,
                            std::function<void(const HttpResponsePtr &)> callback,
                             std::mutex &mutex, std::vector<std::pair<int, 
                             std::vector<double>>> &port_latencies, int &request_left, int &num_of_total_requests); 

void handleForwarding(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &forwardingMode, 
                      int num_of_requests, 
                      std::vector<std::pair<int, std::vector<double>>> &port_latencies,
                      std::vector<std::pair<std::string, int>> &backendUrls,
                      int &request_left, int &num_of_total_requests);

void calculateCDF(const std::vector<double>& latencies, Json::Value& cdfJson);

// void printLatencies();
std::pair<int, double> fastestResp(const std::vector<std::pair<int, std::vector<double>>> &port_latencies);
void addLatency(std::vector<std::pair<int, std::vector<double>>>& port_latencies, int port, double latency);

#endif // SERVER_H
