import subprocess
import requests
import time
import argparse
import numpy as np
import matplotlib.pyplot as plt 


def calculate_cdf(latencies): 
    sotr = np.sort(latencies)
    cdf = np.linspace(0, 1, len(latencies))

    #plot 
    plt.xlabel('time') 
    plt.ylabel('cdf') 
    plt.title('CDF of response time')
    plt.plot(sotr, cdf, marker = 'o')
    plt.savefig('plot.png')
    print("image saved!")
    subprocess.Popen('imview plot.png', shell = True)
    return 
    
# def calculate_latency_increasing_thoughput(time, bytes):
#     throughput = bytes/time
#     print(f"Total Time Taken: {time:.2f} seconds")
#     print(f"Throughput: {throughput / (1024 * 1024):.2f} MBps")
#     return throughput

def send_requests(ports, serving_appro, num_requests = 1, throughput_levels = [1], duration = 10):
    processes = []
    latencies = {port: [] for port in ports}

    for port in ports:
        
        print(f"Starting server on port: {port}, using serving approach: {serving_appro}")
        process = subprocess.Popen(['build/backend', str(port)]) 
        #process = subprocess.Popen(['build/backend', str(port), str(serving_appro)])
        time.sleep(1)
        for _ in range(num_requests):
            try:
                start_time = time.time()
                response = requests.get(f'http://localhost:{port}')
                end_time = time.time()
                latency = end_time - start_time
                latencies[port].append(latency)

                print(f"Port:{port}, Status Code: {response.status_code}, Response Time: {response.elapsed.total_seconds()} seconds")
            except requests.RequestException as e:
                print(f"Request failed for port {port}: {e}")

        calculate_cdf(latencies[port])
        processes.append(process) 

    print("All servers started")   
    return processes

def close_processes(processes):
    for process in processes:
        process.terminate()
        process.wait()
    print("All servers stopped")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Send multiple requests to a backend")
    parser.add_argument("--ports", nargs = '+', type = int, required = True, help = "List of ports for the servers")
    parser.add_argument("--num_requests", type=int, default=1, help="Number of requests to send")   

    # parser.add_argument("--throughput_levels", nargs='+', type=float, required=True, help="List of throughput levels (requests per second)")
    # parser.add_argument("--duration", type=int, default=10, help="Duration to test each throughput level (seconds)")
 
    #set serving_appro to to required after implementing the serving approach
    parser.add_argument("--serving_appro", type=int, default=0, help="Serving approach")
    parser.add_argument("--mode", type=str, default="backend", help="Mode to run the server")

    args = parser.parse_args()
    
    if(args.mode == "backend"):
        #backend mode
        print("Running in backend mode")
        try:
            processes = send_requests(args.ports, args.serving_appro, args.num_requests)
            print("Press Ctrl+C to stop the servers")
            while True:
                time.sleep(1)

        except KeyboardInterrupt:
            print("Stopping servers...")
            close_processes(processes)
            print("All servers stopped.")
    else:
        #proxy mode
        print("Running in proxy mode")
        try:
            processes = send_requests(args.ports, args.serving_appro, args.num_requests)
            print("Press Ctrl+C to stop the servers")
            while True:
                time.sleep(1)

        except KeyboardInterrupt:
            print("Stopping servers...")
            close_processes(processes)
            print("All servers stopped.")
