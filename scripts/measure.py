import subprocess 

import psutil #pip install psutil

#to start: python3 measure.py --ports (the port number you want to measure) --num_requests (number of requests you want to send) --serving_appro (the serving approach you want to use) --mode (backend or proxy)
import requests 
import time
import argparse
import numpy as np
import matplotlib.pyplot as plt 
import socket

#to start: python3 measure.py --ports (the port number you want to measure) --num_requests (number of requests you want to send) --serving_appro (the serving approach you want to use) --mode (backend or proxy)


#send the latency of the proxy 

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
    server_ports = [5000, 9000, 10000, 11000, 12000, 13000, 14000, 15000]
    for port in server_ports:
        kill_process_by_port(port)
    print("All processes terminated")

def calculate_cdf(serving_appro, num_requests = 1, power = 1, prompt = ""):

    processes = start_serv()

    if is_port_in_use(9000):
        kill_process_by_port(9000)
        print(f"Killed processes on port 9000")
    else:
        print(f"No processes found on port port_to_kill")

    proxy_process = subprocess.Popen([f'./backend 9000 proxy {serving_appro} {num_requests} {power} {prompt}'], cwd='build', shell=True)
    time.sleep(num_requests/100) # Wait for the requests to be processed

    # Fetch latencies
    try:
        response = requests.get(f'http://localhost:9000')
        data = response.json().get('latencies', [])
        print("Total request: " + str(response.json().get('total_requests')))

    except Exception as e:
        close_all_processes()
        print(f"Failed to get latencies: {e}")

    plt.figure()
    
    for port, latencies in data:
        sotr = np.sort(np.array(latencies))
        cdf = np.arange(1, len(sotr) + 1) / len(sotr)
        
        # Plot CDF
        plt.plot(sotr, cdf, marker='o', label=f'Port {port}')
    
    # Labeling
    plt.xlabel('Milliseconds')
    plt.ylabel('CDF')
    # Main title
    plt.suptitle('CDF of Response Time', fontsize=16, ha='center')

    # Subtitle between the title and the graph
    plt.title(f'Serving: {serving_appro}, Requests: {num_requests}, Prompt: {prompt} prompt', fontsize=10, ha='center', va='center')

    plt.legend(title='Port', bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True)
    plt.tight_layout()

    # Save and display the plot
    plt.savefig('CDF_plot.png')
    print("Image saved!")
    subprocess.Popen('imview CDF_plot.png', shell=True)
    close_all_processes()    

def measure_load_vs_latency(serving_appro, load_levels, power=1, prompt=""):
    latencies_by_load = []
    processes = start_serv()    
    
    for load in load_levels:
        print(f"Testing load level: {load}")

        # Open the proxy server
        if is_port_in_use(9000):
            kill_process_by_port(9000)
            print(f"Killed processes on port 9000")
        else:
            print(f"No processes found on port port_to_kill")

        proxy_process = subprocess.Popen([f'./backend 9000 proxy {serving_appro} {load} {power} {prompt}'], cwd='build', shell=True)
        time.sleep(load/10) # Wait for the requests to be processed

        # Fetch latencies
        try:
            response = requests.get(f'http://localhost:9000')
            
            data = response.json().get('latencies', [])
            
        except Exception as e:
            close_all_processes()
            print(f"Failed to get latencies: {e}")
    

        # print(f"Latencies: {data}")
        
        flattened_data = [item for sublist in data for item in sublist]
        # print(f"Flattened data: {flattened_data}")
        latencies_by_load.append((load, flattened_data))
        
        # print(f"Latencies by load: {latencies_by_load}")
    
        
        #make sure all the processes are closed
        print(f"Closing all servers in load {load}")
        kill_process_by_port(9000)
    close_all_processes()

    # Initialize the plot
    plt.figure()
    # plt.figure(figsize=(10, 6))
    port_data = {}

    # Loop through the data to collect latencies for each port at different loads
    for load, port_latencies in latencies_by_load:
        for i in range(0, len(port_latencies), 2):
            port = port_latencies[i]
            latencies = port_latencies[i+1]
            avg_latency = np.mean(latencies)  # Calculate average latency for each port

            if port not in port_data:
                port_data[port] = {"loads": [], "latencies": []}
            
            port_data[port]["loads"].append(load)
            port_data[port]["latencies"].append(avg_latency)

    # Plot the data for each port
    for port, data in port_data.items():
        plt.plot(data["loads"], data["latencies"], marker='o', label=f'Port {port}')

    # Labeling and other details
    plt.xlabel('Load (Requests per Second)')
    plt.ylabel('Average Latency (ms)')

    plt.suptitle('Load vs Average Latency for Different Ports', fontsize=16, ha='center')

    # Subtitle between the title and the graph
    plt.title(f'Serving: {serving_appro}, Load_Levels: {load_levels}, Prompt: {prompt} prompt',fontsize=10, ha='center', va='center')

    plt.legend(title='Port', bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True)
    plt.tight_layout()

    # Save and display the plot
    plt.savefig('Load_vs_Latency.png')
    print("Image saved!")
    subprocess.Popen('imview Load_vs_Latency.png', shell=True)


def start_serv():
    processes = []
    server_ports = [10000, 11000, 12000, 13000, 14000, 15000]

    # Open the backend servers
    for port in server_ports:
        if is_port_in_use(port):
            print(f"Port {port} is in use. Skipping...")
            continue
        
        try:
            process = subprocess.Popen([f'./backend {port} backend'], cwd='build', shell=True)
            processes.append(process)
            time.sleep(0.5)
        except Exception as e:
            print(f"Failed to start server on port {port}: {e}")

    # Open the Flask application
    try:
        flask_process = subprocess.Popen(['flask', '--app', 'app', 'run'], cwd='API')
        processes.append(flask_process)
        time.sleep(4)
    except Exception as e:
        close_all_processes()
        print(f"Failed to start Flask application: {e}")

    print("All servers started")
    
    return processes

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Send multiple requests to a backend")
    parser.add_argument("--num_requests", type=int, default=1, help="Number of requests to send")   
    parser.add_argument("--serving_appro", type=str, default="", help="Serving approach")
    parser.add_argument("--power", type=int, default=1, help="Power of the server")
    parser.add_argument("--load_levels", nargs='+', type=int, help="List of load levels (requests per second)")
    parser.add_argument("--prompt", type=str, default="", help="prompt")

    args = parser.parse_args()
    
    print("Running...")

    if(args.load_levels):
        measure_load_vs_latency(args.serving_appro, args.load_levels, args.power, args.prompt)
        print("Press Ctrl+C to stop the servers")
        while True:
            time.sleep(1)

    else:
        calculate_cdf(args.serving_appro, args.num_requests, args.power, args.prompt)
        print("Press Ctrl+C to stop the servers")
        while True:
            time.sleep(1)
