import requests
import time
import numpy as np

# the API endpoint
url = "http://localhost:4000/api/search?prompt=cat&forward=random_one"

if __name__ == "__main__":
    num_request = 1000
    latencies = []

    measurement_start_time = time.time()
    for i in range(num_request):
        start_time = time.time()
        response = requests.get(url)
        end_time = time.time()
        latency = end_time - start_time
        latencies.append(latency)
    measurement_end_time = time.time()
    
    print("Avg. latency: ", np.mean(latencies), "seconds")
    print("Avg. latency: ", 
          (measurement_end_time-measurement_start_time)/num_request, "seconds")
    print("p05 latency: ", np.percentile(latencies, 5), "seconds")
    print("p25 latency: ", np.percentile(latencies, 25), "seconds")
    print("p50 latency: ", np.percentile(latencies, 50), "seconds")
    print("p75 latency: ", np.percentile(latencies, 75), "seconds")
    print("p90 latency: ", np.percentile(latencies, 90), "seconds")
    print("p95 latency: ", np.percentile(latencies, 95), "seconds")
    print("p99 latency: ", np.percentile(latencies, 99), "seconds")
    print("p99.9 latency: ", np.percentile(latencies, 99.9), "seconds")
