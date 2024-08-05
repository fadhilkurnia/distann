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

using namespace hnswlib;
using json = nlohmann::json;
using namespace drogon;


void startBackend(int port);
void startProxy(int port, int numRequests);

void sendSingleRequest(const std::string &url, 
                       const drogon::HttpRequestPtr &req, 
                        int reqNum,
                       std::shared_ptr<std::vector<std::string>> responses, 
                       std::shared_ptr<int> responsesLeft, 
                       std::function<void(const drogon::HttpResponsePtr &)> callback, 
                       std::mutex &mutex);

void sendGetRequestsToServer(const std::string &url, 
                             const drogon::HttpRequestPtr &req, 
                             int num_of_requests, 
                             std::shared_ptr<std::vector<std::string>> responses, 
                             std::shared_ptr<int> responsesLeft, 
                             std::function<void(const drogon::HttpResponsePtr &)> callback, 
                             std::mutex &mutex);

void handleForwarding(const HttpRequestPtr &req, 
                      std::function<void(const HttpResponsePtr &)> &&callback, 
                      const std::string &path, 
                      const std::string &forwardingMode, 
                      int num_of_requests);
#endif // SERVER_H
