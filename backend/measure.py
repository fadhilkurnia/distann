import subprocess
import requests
import time
import argparse

def send_requests(ports, num_requests = 1, delay = 1):
    processes = []
    for port in ports:
        print(f"Starting server on port {port}")
        process = subprocess.Popen(['build/backend', str(port)]) 
        time.sleep(delay)
        for _ in range(num_requests):
            try:
                response = requests.get(f'http://localhost:{port}')
                print(f"Port:{port}, Status Code: {response.status_code}, Response Time: {response.elapsed.total_seconds()} seconds")
            except requests.RequestException as e:
                print(f"Request failed for port {port}: {e}")
            time.sleep(1)

        processes.append(process)    
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
    parser.add_argument("--delay", type=float, default=1, help="Delay between requests")

    args = parser.parse_args()
    
    try:
        processes = send_requests(args.ports, args.num_requests, args.delay)
        print("Press Ctrl+C to stop the servers")
        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print("Stopping servers...")
        close_processes(processes)
        print("All servers stopped.")
