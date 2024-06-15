#include "TestCtrl.h"
#include <fstream>

void TestCtrl::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // write your application logic here

  // get the path of our image
  // NOTE: Will need to update path to match your local path
  std::string filePath;

  if (req->path() == "/test" && req->method() == drogon::Get) {
    // read our image from our path and store it in image
    filePath = "/Users/samiahmed/Downloads/val2017/000000388258.jpg";
  } else if (req->path() == "/") {
    filePath = "/Users/samiahmed/Projects/URV/distann/Frontend_Implementation/"
               "web.html";
  } else {
    // throw error if image not found
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("Image not found");
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k404NotFound);
    callback(resp);
  }

  std::ifstream fileData(filePath, std::ios::binary);

  // store our image as string data
  std::ostringstream out;
  out << fileData.rdbuf();
  std::string file = out.str();

  // create our response to out put the image
  auto resp = HttpResponse::newHttpResponse();
  resp->setBody(std::move(file));

  if (req->path() == "/test" && req->method() == drogon::Get) {
    resp->setContentTypeCode(CT_IMAGE_JPG);
  } else if (req->path() == "/") {
    resp->setContentTypeCode(CT_TEXT_HTML);
  }

  resp->setStatusCode(k200OK);
  callback(resp);
}
