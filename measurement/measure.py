import subprocess 

import psutil #pip install psutil

# python3 measure.py --num_requests <number_of_requests> --measure <cdf / lead> --load_levels <load_level1> <load_level2> ...

import requests 
import time 
import argparse
import numpy as np
import matplotlib.pyplot as plt #pip install matplotlib
import socket
import threading

# from test import start

def is_port_in_use(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(('localhost', port)) == 0

def kill_process_by_port(port):
    for proc in psutil.process_iter(['pid', 'name']):
        try:
            # Get the list of connections for the process
            connections = proc.net_connections()
            for conn in connections:
                if conn.laddr.port == port:
                    proc.kill()
                    print(f"Terminated process {proc.info['pid']} using port {port}")
                    return
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            continue
    print(f"No process found using port {port}")

def close_all_processes():
    server_ports = [9000, 10000, 11000, 12000, 13000, 14000, 15000, 5000, 5500, 6000, 6500, 7000, 7500]
    for port in server_ports:
        kill_process_by_port(port)
    print("All processes terminated")

def calculate_cdf(num_requests = 1, prompt = ""):
    data = {
    # comment this to remove forward_all to the measurement 
    "forward_all" : [],
    "forward_random" : [], 
    "forward_two" : [], 
    "forward_round" : []}

    start_serv()

    for serving_approach in data.keys():
        print(f"Testing serving approach: {serving_approach} **********")

        # Open the proxy server
        if(is_port_in_use(9000)):
            kill_process_by_port(9000)

        subprocess.Popen([f'./backend 9000 proxy {serving_approach} {prompt}'], cwd='../backend/build', shell=True)
        time.sleep(2) # Wait for the requests to be processed

        #loops through the number of requests and updates the latency data
        for i in range(num_requests):
            # Call start_serv and store the result in the corresponding list
            print(f"{i} requests served for {serving_approach}")
            data[serving_approach] = send_request()
            time.sleep(0.01) # Wait for the requests to be processed
        kill_process_by_port(9000)

    # Plot for all serving approaches except 'forward_all'
    plt.figure()

    for serving_approach, latencies in data.items():
        sotr = np.sort(np.array(latencies))
        cdf = np.arange(1, len(sotr) + 1) / len(sotr)

        # Plot CDF
        plt.plot(sotr, cdf, marker=None , label=f'Port {serving_approach}')

        # Set x-axis limits conditionally
        if serving_approach == "forward_all":
            plt.xlim(0, 500)

    # For other serving approaches, ensure the x-axis starts from 0
    plt.xlim(left=0)

    # Labeling
    plt.xlabel('Milliseconds')
    plt.ylabel('CDF')
    plt.suptitle('CDF of Response Time', fontsize=16, ha='center')
    plt.title(f'Serving Approaches, Requests: {num_requests}, Prompt: {prompt}', fontsize=10, ha='center', va='center')
    plt.legend(title='Port', bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True)
    plt.tight_layout()

    # Save and display the plot
    plt.savefig('CDF_plot.png')
    print("Image saved!")
    subprocess.Popen('imview CDF_plot.png', shell=True)

    close_all_processes()
    
def measure_load_vs_latency(load_levels, prompt=""):
    # comment this to remove forward_all from the measurement 
    serving_approaches = [
                        "forward_all",
                        "forward_random",
                        "forward_round",
                        "forward_two"]

    # Start the backend and api servers
    start_serv()

    # Dictionary to store latencies for each serving approach
    latencies_by_approach = {approach: [] for approach in serving_approaches}

    # Create and start threads for each combination of serving approach and load level
    for serving_approach in serving_approaches:
        #closes any port 9000 to let the proxy start clean
        if(is_port_in_use(9000)):
            kill_process_by_port(9000)

        #loops through the load levels 
        for load in load_levels:
            subprocess.Popen([f'./backend 9000 proxy {serving_approach} {prompt}'], cwd='../backend/build', shell=True)
            time.sleep(1) # Wait for the requests to be processed

            # Create a thread to similate concurrent requests
            threads = []
            for _ in range(load):
                thread = threading.Thread(target=send_request)
                thread.start()
                threads.append(thread)

            # Wait for all threads to complete
            for thread in threads:
                thread.join()
            
            #last request to get all the accumulated data that is stored in the proxy server
            data = send_request()

            latencies_by_approach[serving_approach].append((load, np.mean(data)))
            kill_process_by_port(9000)

    print(f"latencies_by_approach: {latencies_by_approach}")

    # Create a single plot for all serving approaches
    plt.figure()

    for serving_approach, latencies in latencies_by_approach.items():
        loads, avg_latencies = zip(*sorted(latencies))
        
        # Plot the data
        plt.plot(loads, avg_latencies, marker=None, label=f'{serving_approach}')
        
        # Set y-axis limits conditionally
        if serving_approach == "forward_all":
            plt.ylim(0, 1000)

    # For other serving approaches, ensure the y-axis starts from 0
    plt.ylim(bottom=0)

    # Labeling
    plt.xlabel('Load (Requests per Second)')
    plt.ylabel('Average Latency (ms)')
    plt.suptitle('Load vs Average Latency for Different Serving Approaches', fontsize=16, ha='center')
    plt.title(f'Load Levels: {load_levels}, Prompt: {prompt}', fontsize=10, ha='center', va='center')
    plt.legend(title='Serving Approach', bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True)
    plt.tight_layout()

    # Save and display the plot
    plt.savefig('Load_vs_Latency.png')
    print("Image saved!")
    subprocess.Popen('imview Load_vs_Latency.png', shell=True)

    close_all_processes()
    

def start_serv():
    server_ports = [10000, 11000, 12000, 13000, 14000, 15000]
    embedded_port = [5000, 5500, 6000, 6500, 7000, 7500]

    # Open the backend servers
    for port in server_ports:
        if is_port_in_use(port):
            print(f"Port {port} is in use. Skipping...")
            continue
        
        try:
            subprocess.Popen([f'./backend {port} backend'], cwd='../backend/build', shell=True)
            time.sleep(0.5)
        except Exception as e:
            print(f"Failed to start server on port {port}: {e}")

    # Open the Flask application
    for port in embedded_port:
        if is_port_in_use(port):
            print(f"Port {port} is in use. Skipping...")
            continue
        
        try:
            subprocess.Popen([f'python3 app.py --port {port}'], cwd='../backend/API', shell=True)
            time.sleep(4)
        except Exception as e:
            print(f"Failed to start server on port {port}: {e}")

    print("All servers started")
    

def send_request():
    try:
        response = requests.get("http://localhost:9000")

        data = response.json().get('Latency', [])
        return data

    except Exception as e:
        close_all_processes()
        print(f"Failed to get latencies: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Send multiple requests to a backend")
    parser.add_argument("--num_requests", type=int, default=1, help="Number of requests to send")   
    parser.add_argument("--measure", type=str, default="", help="Serving approach")
    parser.add_argument("--load_levels", nargs='+', type=int, help="List of load levels (requests per second)")
    parser.add_argument("--prompt", type=str, default="", help="prompt")

    args = parser.parse_args()
    
    print("Running...")
    ###for testing single request
    # start_serv()
    close_all_processes()
    # kill_process_by_port(9000)
    
    if(args.measure == "load"):
        measure_load_vs_latency(args.load_levels, args.prompt)
        print("Press Ctrl+C to stop the servers")
        while True:
            time.sleep(1)

    elif(args.measure == "cdf"):
        calculate_cdf(args.num_requests, args.prompt)
        print("Press Ctrl+C to stop the servers")
        while True:
            time.sleep(1)