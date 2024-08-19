#include <stdio.h>   //difference between this and cstudio
#include <curl/curl.h>
#include <string>
#include <iostream>
#include "../../json.hpp" // Ensure this path is correct for your project

using json = nlohmann::json;

// Callback function to write response data into a string
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t totalSize = size * nmemb;
    s->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

int main() {
    std::string prompt = "Hello World";
    
    CURL* curl;
    CURLcode res;
    std::string responseString; // Variable to store the response

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5000/embed");

        // Prepare the user query to be sent to API
        json payload = {{"query", prompt}};
        std::string payloadStr = payload.dump();

        // Set HTTP headers, as required by API
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set POST fields of call
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // Set the callback function to capture response, converting CURL object to string to parse JSON
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseString);

        // Perform the request
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            // Parse the JSON response
            try {
                auto data = json::parse(responseString);
                std::cout << data["embedding"][0];
            } catch (json::parse_error& e) {
                std::cerr << "JSON parse error: " << e.what() << std::endl;
            }
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    curl_global_cleanup();

    return 0;
}
