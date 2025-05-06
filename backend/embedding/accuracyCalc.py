import requests
import json

# Run queries against HNSW algorithm

data = dict()

print("Running queries against HNSW algorithm...")
with open("../data/queries.txt", "r") as f:
    queries = f.readlines()
    for query in queries:
        query = query.rstrip()
        url = f'http://localhost:9000/api/search?prompt={query}'
        data[query] = requests.get(url).json()["results"]

# Do checking

print("Calculating accuracy...")
groundTruthPath = "../data/groundTruth.json"
results = []
totalAcc = 0

with open(groundTruthPath, 'r') as j:
    groundTruth = json.loads(j.read())

    for query in groundTruth:
        queryAcc = 0
        print(f'\rCalculating query {query} ({len(results)}/{len(groundTruth)})', end = '\r')
        for gndImg in groundTruth[query]:
            # Make array of numbers
            nums = []
            for img in data[query]:
                nums.append(int(img['url'].split('/')[-1].split('.')[0]))
            if gndImg in nums:
                queryAcc += 1
        results.append(queryAcc / 12)

    for res in results:
        totalAcc += res
    totalAcc /= len(results)

    print('\n')
    print(f'Accuracy: {totalAcc * 100}%')
