import subprocess
import requests
import time
import argparse
import numpy as np
import matplotlib.pyplot as plt 

#to start: python3 measure.py --ports (the port number you want to measure) --num_requests (number of requests you want to send) --serving_appro (the serving approach you want to use) --mode (backend or proxy)

def calculate_cdf(data):
    plt.figure()
    
    for port, latencies in data:
        sotr = np.sort(np.array(latencies))
        cdf = np.arange(1, len(sotr) + 1) / len(sotr)
        
        # Plot CDF
        plt.plot(sotr, cdf, marker='o', label=f'Port {port}')
    
    # Labeling
    plt.xlabel('Milliseconds')
    plt.ylabel('CDF')
    plt.title('CDF of Response Time')
    plt.legend()
    
    # Save and display the plot
    plt.savefig('CDF_plot.png')
    print("Image saved!")
    subprocess.Popen('imview CDF_plot.png', shell=True)
    
def send_requests(serving_appro, num_requests = 1, power = 1):
    processes = []
    latencies = []
    server_ports = [10000, 11000, 12000]

    #open the servers
    for port in server_ports:
        process = subprocess.Popen([f'./backend {port} backend'], cwd='build', shell=True)
        processes.append(process)
        time.sleep(1)

    #open the flask application
    try:
        flask_process = subprocess.Popen(['flask', '--app', 'app', 'run'], cwd='API')
        processes.append(flask_process)
        time.sleep(4)
    except Exception as e:
        print(f"Failed to start Flask application: {e}")

    #open the proxy server
    process = subprocess.Popen(['build/backend', str(9000), "proxy", serving_appro, str(num_requests), str(power)])
    time.sleep(1)
    response = requests.get(f'http://localhost:9000')
    latencies = response.json()['latencies']
    print(latencies)
    calculate_cdf(latencies)
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
    parser.add_argument("--num_requests", type=int, default=1, help="Number of requests to send")   

    # parser.add_argument("--throughput_levels", nargs='+', type=float, required=True, help="List of throughput levels (requests per second)")

    #set serving_appro to to required after implementing the serving approach
    parser.add_argument("--serving_appro", type=str, default=0, help="Serving approach")
    parser.add_argument("--power", type=int, default=1, help="Power of the server")
    parser.add_argument("--prompt", type=str, default=False, help="Prompt user for input")

    args = parser.parse_args()
    
    print("Running...")
    try:
        processes = send_requests(args.serving_appro, args.num_requests, args.power)
        print("Press Ctrl+C to stop the servers")
        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print("Stopping servers...")
        close_processes(processes)
        print("All servers stopped.")