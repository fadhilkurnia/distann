import requests
import time
import numpy as np

# the API endpoint
url = "http://localhost:4000/api/search?prompt=cat&forward=random_two"

if __name__ == "__main__":
    num_request = 100
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
