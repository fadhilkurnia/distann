import subprocess
import requests
import time
import argparse
import numpy as np
import matplotlib.pyplot as plt 

#to start: python3 measure.py --ports (the port number you want to measure) --num_requests (number of requests you want to send) --serving_appro (the serving approach you want to use) --mode (backend or proxy)

def calculate_cdf(latencies): 
    sotr = np.sort(latencies)
    cdf = np.linspace(0, 1, len(latencies))

    #plot 
    plt.xlabel('time') 
    plt.ylabel('cdf') 
    plt.title('CDF of response time')
    plt.plot(sotr, cdf, marker = 'o')
    plt.savefig('CDF_plot.png')
    print("image saved!")
    subprocess.Popen('imview CDF_plot.png', shell = True)
    return 
    
def send_requests(ports, serving_appro, num_requests = 1, throughput_levels = [1]):
    processes = []
    latencies = {port: [] for port in ports}

    for port in ports:
        
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