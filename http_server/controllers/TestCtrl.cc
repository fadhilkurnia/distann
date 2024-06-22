#include "TestCtrl.h"
#include <fstream>

void TestCtrl::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  // Set the homeDirectory for our project to the GitHub Repo main branch
  std::string homeDirectory =
      "https://raw.githubusercontent.com/fadhilkurnia/distann/main/";
  // Set the image directory where we will get our images from
  std::string imageDirectory =
      "https://raw.githubusercontent.com/fadhilkurnia/distann/main/images/";

  if (req->path() == "/test" && req->method() == drogon::Get) {
    // store the text prompt
    auto prompt = req->getParameters();
    std::string text = prompt["message"];
    std::cout << text;

    // request for images will return relative paths of images
    std::string image = imageDirectory + "image1.jpg";

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(image);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else if (req->path() == "/") {
    // request for the homepage will return the path to the homepage which gets
    // generated in front-end
    std::string homePage = homeDirectory + "Frontend_Implementation/web.html";

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(homePage);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else {
    // throw error if response not found
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("Response not found");
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k404NotFound);
    callback(resp);
  }
}
