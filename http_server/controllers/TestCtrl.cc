#include "TestCtrl.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void TestCtrl::asyncHandleHttpRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto resp = HttpResponse::newHttpResponse();

  std::string imageDirectory = "/Users/samiahmed/Projects/URV/distann/images/";
  std::string localHost = "http://localhost";

  if (req->path() == "/search" && req->method() == drogon::Get) {
    // store the text prompt
    auto prompt = req->getQuery();
    // request for images will return url of images
    std::string imageURL = localHost + "/images/image1.jpg";

    // Create our JSON response
    json response;
    response["prompt"] = prompt;
    response["results"] = json::array();
    json result;
    result["id"] = 1;
    result["url"] = imageURL;
    result["alt"] = "Bike and Plane";
    response["results"].push_back(result);

    resp->setBody(response.dump());
    resp->setContentTypeCode(CT_APPLICATION_JSON);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else if (req->path() == "/images/{image_name}" &&
             req->method() == drogon::Get) {
    // get last part of URL (string) if it does not exist return error
    std::string imageName = req->getParameter("image_name");
    std::string imagePath = imageDirectory + imageName;

    // store our image as string data
    std::ifstream imageData(imagePath, std::ios::binary);

    // check that our image exists
    if (!imageData) {
      resp->setBody("Image not found");
      resp->setContentTypeCode(CT_TEXT_HTML);
      resp->setStatusCode(k404NotFound);
      callback(resp);
    }

    std::ostringstream oss;
    oss << imageData.rdbuf();
    std::string image = oss.str();

    resp->setBody(std::move(image));
    resp->setContentTypeCode(CT_IMAGE_JPG);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else if (req->path() == "/") {
    // post message to backend
    std::string message = "Welcome to backend server";

    resp->setBody(message);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k200OK);
    callback(resp);
  } else {
    // throw error if endpoint not found
    resp->setBody("Endpoint not found");
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setStatusCode(k404NotFound);
    callback(resp);
  }
}
