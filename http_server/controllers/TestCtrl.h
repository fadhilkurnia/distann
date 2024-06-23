#pragma once

#include <drogon/HttpSimpleController.h>

using namespace drogon;

class TestCtrl : public drogon::HttpSimpleController<TestCtrl> {
public:
  void asyncHandleHttpRequest(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback) override;
  PATH_LIST_BEGIN
  // list path definitions here;
  PATH_ADD("/", Get, Post);
  PATH_ADD("/search", Get, Post);
  PATH_ADD("/images/{image_name}", Get);

  PATH_LIST_END
};
