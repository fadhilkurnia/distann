#include "TestCtrl.h"
#include <fstream>

void TestCtrl::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  std::string homeDirectory = "/Users/samiahmed/Projects/URV/distann";

  if (req->path() == "/search" && req->method() == drogon::Get) {
    // store the text prompt
    auto prompt = req->getQuery();

    // request for images will return relative paths of images
    std::string imagePath = "/images/image1.jpg";

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(imagePath);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else if (req->path() == "/images" && req->method() == drogon::Get) {
    std::string imagePath = homeDirectory + "/images/image1.jpg";

    // store our image as string data
    std::ifstream imageData(imagePath, std::ios::binary);
    std::ostringstream oss;
    oss << imageData.rdbuf();
    std::string image = oss.str();

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(std::move(image));
    resp->setContentTypeCode(CT_IMAGE_JPG);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else if (req->path() == "/") {
    // request for the homepage will return the path to the homepage which gets
    // generated in front-end
    std::string homePage = "/Frontend_Implementation/web.html";

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(homePage);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else {
    // throw error if endpoint not found
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("Endpoint not found");
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k404NotFound);
    callback(resp);
  }
}
