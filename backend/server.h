#ifndef SERVER_H
#define SERVER_H

#include <drogon/drogon.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <hnswlib/hnswlib.h>
#include <iostream>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <sstream>
#include <map>
#include <algorithm>
#include <random>
#include <mutex>
#include <json/json.h>
#include <limits>




using namespace hnswlib;
using json = nlohmann::json;
using namespace drogon;


void startBackend(int port);
void startProxy(int port, int numRequests);

void sendSingleRequest(const std::string &url, int port, const HttpRequestPtr &req, int reqNum,
                       std::function<void(const HttpResponsePtr &)> callback,
                       std::mutex &mutex);
                       
void sendGetRequestsToServer(const std::string &url, int port, const HttpRequestPtr &req, int num_of_requests,
                             std::function<void(const HttpResponsePtr &)> callback,
                             std::mutex &mutex);
void handleForwarding(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &path, const std::string &forwardingMode, int num_of_requests);

void calculateCDF(const std::vector<double>& latencies, Json::Value& cdfJson);

void printLatencies();
int fastestResp();

#endif // SERVER_H
