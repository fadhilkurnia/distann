// test.cc - demonstration code for forward_all approach.
//
// To compile   : g++ -o test test.cc -std=c++20
// Then run     : ./test

#include <atomic>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <vector>
#include <optional>

// A dummy process that "sending" request to a backend server.
void sendRequest(const std::string& client_request,
                 std::atomic<bool>& is_request_done,
                 std::mutex& response_lock,
                 std::optional<std::string>& first_response) {
    std::thread::id thread_id = std::this_thread::get_id();
    
    // Does dummy processing by sleeping, mimicking sending and receiving
    // response from backend.
    std::cout << "thread-" << thread_id 
        << ": curl forwarding request: '" << client_request << "' ....\n";
    std::this_thread::sleep_for(
        std::chrono::milliseconds(2 + std::rand()%5));
    std::cout << "thread-" << thread_id 
        << ": curl receiving a response.\n";
    std::stringstream ss; ss << thread_id;
    std::string response = "a response from " + ss.str();

    // Set the first response. We use lock to ensure only one thread
    // updating the response at any given time. We check whether
    // `first_response` already has value or not to ensure only the first
    // thread successfully set the response value.
    response_lock.lock();
    {
        if (!first_response.has_value()) {
            first_response.emplace(response);
        }
    }
    response_lock.unlock();
    
    // Notify the main() that we have a response received already.
    // It is fine to have multiple threads set the value to `true`.
    is_request_done = true;
    is_request_done.notify_all();
}

int main() {
    std::cout << "Prototype for forward_all" << std::endl;

    // Assume we are receiving an http request, with callback.
    std::string client_request = "a request";
    
    // Prepare important variables for synhronization and storing
    // the first response.
    std::atomic<bool> is_first_request_done = false;
    std::optional<std::string> first_response = std::nullopt;
    std::mutex response_lock;   // a lock to ensure no concurrent update to 
                                // `first_response`.
    
    // Send concurrent requests to all 5 backends, using 5 threads.
    const int num_backends = 5;
    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_backends; ++i) {
        threads.push_back(std::thread(sendRequest,
                                      std::ref(client_request),
                                      std::ref(is_first_request_done),
                                      std::ref(response_lock),
                                      std::ref(first_response)));
    }
    
    // Wait for the fastest response. That is until the value is not `false`.
    is_first_request_done.wait(false);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = end_time - start_time;

    // Copy the response so we can forward it back to the client.
    // Note that we need to copy the response because we can not directly
    // use `first_response` that require acquiring lock for access.
    std::string copy_response;
    response_lock.lock();
    copy_response = first_response.value();
    response_lock.unlock();
    
    // In the actual implementation, we need to forward `copy_response` to the
    // client using callback that is provided when receiving an HTTP request.
    //   `callback(copy_response)`
    // Here, we simply print out the response.
    std::cout << "Get the first response in " << duration.count() 
              << "ns. Response: '" << copy_response << "'" << std::endl;
    
    // Cleaning up all the threads, ensuring all of them terminate.
    for (std::thread& t : threads) {
        t.join();
    }
    return 0;
}