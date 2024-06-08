#include "TestCtrl.h"
#include <fstream>

void TestCtrl::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // write your application logic here

  // get the path of our image
  std::string imagePath = "/Users/samiahmed/Downloads/val2017/000000388258.jpg";

  // read our image from our path and store it in image
  std::ifstream imageData(imagePath, std::ios::binary);

  // throw error if image not found
  if (!imageData) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("Image not found");
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k404NotFound);
    callback(resp);
  }

  // store our image as string data
  std::ostringstream out;
  out << imageData.rdbuf();
  std::string image = out.str();

  // create our response to out put the image
  auto resp = HttpResponse::newHttpResponse();
  resp->setBody(std::move(image));
  resp->setContentTypeCode(CT_IMAGE_JPG);
  resp->setStatusCode(k200OK);

  callback(resp);
}
