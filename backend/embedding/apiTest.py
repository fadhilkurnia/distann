import requests

url = 'http://127.0.0.1:5000/embed'
data = {'query': 'Hello, World!'}

response = requests.post(url, json=data)

print(response.json())
