import subprocess 

import psutil #pip install psutil

# python3 measure.py --num_requests <number_of_requests> --measure <cdf / lead> --load_levels <load_level1> <load_level2> ...

import requests 
import time 
import argparse
import numpy as np
import matplotlib.pyplot as plt #pip install matplotlib
from matplotlib.gridspec import GridSpec
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

    # Create a plot with two subplots for the broken y-axis side by side
    fig = plt.figure(figsize=(14, 6))

    # Use subplot2grid to place subplots
    ax1 = plt.subplot2grid((1, 4), (0, 0), colspan=2)  # Left plot
    ax2 = plt.subplot2grid((1, 4), (0, 2), colspan=2, sharey=ax1)  # Right plot, sharing y-axis with ax1

    for serving_approach, latencies in data.items():
        sorted_latencies = np.sort(np.array(latencies))
        cdf = np.arange(1, len(sorted_latencies) + 1) / len(sorted_latencies)

        # Plot the data on both axes with different y-limits
        ax1.plot(sorted_latencies, cdf, marker=None, label=f'{serving_approach}')
        ax2.plot(sorted_latencies, cdf, marker=None, label=f'{serving_approach}')

    # Set x-limits to focus on the desired ranges and create the break
    ax2.set_xlim(100, 4800)  # Adjust based on the specific range needed before the gap
    ax1.set_xlim(0, 50)     # Adjust based on the specific range needed after the gap

    # Define custom y-ticks for each axis
    xticks1 = np.concatenate([
        np.arange(0, 50, 5),     # 20.2, 20.4, ..., 30
    ])

    xticks2 = np.concatenate([
        np.arange(100, 4900, 400)
    ])

    ax2.set_xticks(xticks2)
    ax1.set_xticks(xticks1)


    # Adjust positions of subplots to connect them
    ax1_pos = ax1.get_position()  # Get the original position of ax1
    ax2_pos = ax2.get_position()  # Get the original position of ax2

    # Overlap ax2 with ax1 by setting ax2's x0 to ax1's x1
    ax2.set_position([ax1_pos.x1, ax2_pos.y0, ax2_pos.width, ax2_pos.height])
    ax1.set_position([ax1_pos.x0, ax1_pos.y0, ax1_pos.width, ax1_pos.height])
    ax2.yaxis.set_tick_params(labelleft=False)

    # Add diagonal lines to indicate the axis break
    d = 0.015  # Size of diagonal lines in axes coordinates

    # Diagonal lines for the axis break on the left plot (ax1)
    kwargs = dict(transform=ax1.transAxes, color='k', clip_on=False)
    ax1.plot((-d, +d), (1 - d, 1 + d), **kwargs)  # Bottom-right diagonal
    ax1.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)  # Bottom-left diagonal

    # Diagonal lines for the axis break on the right plot (ax2)
    kwargs.update(transform=ax2.transAxes)
    ax2.plot((-d, +d), (-d, +d), **kwargs)  # Top-right diagonal
    ax2.plot((1 - d, 1 + d), (-d, +d), **kwargs)  # Top-left diagonal

    # Labeling
    ax1.set_xlabel('Milliseconds')
    ax2.set_xlabel('Milliseconds')
    ax1.set_ylabel('CDF')
    ax2.set_ylabel('')  
    # Hide y-axis tick labels but keep the axis line
    ax2.yaxis.set_tick_params(labelleft=False, labelright=False)  # Hide tick labels
    ax2.spines['left'].set_color('black')  # Keep the left spine (y-axis line) visible
    # Hide the spines between ax1 and ax2 to connect the plots seamlessly
    ax1.spines['right'].set_visible(False)
    ax2.legend(title='Serving Approach', bbox_to_anchor=(1, 1), loc='upper left')

    fig.suptitle(f'CDF of Response Time\n Serving Approaches, Requests: {num_requests}, Prompt: {prompt}', fontsize=16, ha='center')
    ax1.grid(True)
    ax2.grid(True)

    # Adjust layout to overlap 

    # Save and display the plot
    plt.savefig('CDF_plot.png')
    print("Image saved!")

    close_all_processes()
    
def measure_load_vs_latency(load_levels, prompt=""):
    # comment this to remove forward_all from the measurement 
    serving_approaches = [
                        "forward_all",
                        "forward_random",
                        "forward_round",
                        "forward_two"]

    # Dictionary to store latencies for each serving approach
    latencies_by_approach = {approach: [] for approach in serving_approaches}

    # Start the backend and api servers
    start_serv()

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

    # Create a plot with two subplots stacked vertically to represent the broken axis
    # Create a plot with a tight layout and no space between subplots
    fig = plt.figure(figsize=(6, 7))
    gs = GridSpec(2, 1, height_ratios=[1, 1])  # 2 rows, 1 column

    # Use subplot2grid to place subplots
    ax1 = fig.add_subplot(gs[0, 0])  # Top plot
    ax2 = fig.add_subplot(gs[1, 0], sharex=ax1)  # Bottom plot, sharing x-axis with ax1

    for serving_approach, latencies in latencies_by_approach.items():
        loads, avg_latencies = zip(*sorted(latencies))

        # Plot the data on both axes, with different y-limits
        ax1.plot(loads, avg_latencies, marker=None, label=f'{serving_approach}')
        ax2.plot(loads, avg_latencies, marker=None, label=f'{serving_approach}')

    # Set y-limits to focus on the desired ranges and create the break
    ax1.set_ylim(100, 4800)  # Adjust based on the specific range needed before the gap
    ax2.set_ylim(0, 50)     # Adjust based on the specific range needed after the gap

    # Define custom y-ticks for each axis
    yticks1 = np.concatenate([
        np.arange(0, 50, 5),     # 20.2, 20.4, ..., 30
    ])

    yticks2 = np.concatenate([
        np.arange(100, 4900, 400)
    ])

    # Apply custom y-ticks to each subplot
    ax1.set_yticks(yticks2)
    ax2.set_yticks(yticks1)

    # Manually adjust positions to overlap subplots
    ax1_pos = ax1.get_position()  # Get the original position of ax1
    ax2_pos = ax2.get_position()  # Get the original position of ax2

    # Overlap ax2 with ax1 by setting ax2's y0 to ax1's y1
    ax2.set_position([ax2_pos.x0, ax2_pos.y0 - 0.022, ax2_pos.width, ax2_pos.height])
    ax1.set_position([ax1_pos.x0, ax1_pos.y0 + 0.01, ax1_pos.width, ax1_pos.height])

    # Add diagonal lines to indicate the axis break
    d = 0.015  # Size of diagonal lines in axes coordinates
    kwargs = dict(transform=ax1.transAxes, color='k', clip_on=False)
    ax1.plot((-d, +d), (1 - d, 1 + d), **kwargs)  # Bottom-right diagonal
    ax1.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)  # Bottom-left diagonal

    kwargs.update(transform=ax2.transAxes)  # Switch to the right axis
    ax2.plot((-d, +d), (-d, +d), **kwargs)  # Top-right diagonal
    ax2.plot((1 - d, 1 + d), (-d, +d), **kwargs)  # Top-left diagonal

    load_levels_str = ', '.join(map(str, load_levels))

    # Labeling
    ax2.set_xlabel('Load (Requests per Second)')
    ax1.set_ylabel('Average Latency (ms)')
    ax2.set_ylabel('Average Latency (ms)')

    # Using LaTeX-like formatting for title
    fig.suptitle(
        f'Load vs Average Latency for Different Serving Approaches\n'
        f'Load: {load_levels_str}\n'
        f'Prompt: {prompt}',
    )
    ax1.legend(title='Serving Approach', bbox_to_anchor=(1.05, 1), loc='upper left')
    ax1.grid(True)
    ax2.grid(True)

    # Remove x-tick labels on the top plot
    ax1.xaxis.set_tick_params(labelbottom=False)

    plt.tight_layout()

    # Save and display the plot
    plt.savefig('Load_vs_Latency.png')
    print("Image saved!")
            
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

    elif(args.measure == "cdf"):
        calculate_cdf(args.num_requests, args.prompt)
