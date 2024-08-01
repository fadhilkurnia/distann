#ifndef SERVER_H
#define SERVER_H

#include <drogon/drogon.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <hnswlib/hnswlib.h>
#include <iostream>

using namespace hnswlib;
using json = nlohmann::json;
using namespace drogon;


void startBackend(int port);
void startProxy(int port);
void handleForwarding(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback, const std::string &path, const std::string &forwardingMode);

#endif // SERVER_H
