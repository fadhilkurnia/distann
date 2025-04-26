from app import vectorize_text
from linNNS import loadVectorsFromFile, calcClosestImages
import json

vectors = loadVectorsFromFile("../data/convertedImagedEmbeddings.txt")
out = {}

with open('../data/queries.txt', 'r') as f:
    queries = f.readlines()
    count = 1

    for query in queries:
        query_fix = query.rstrip()
        vector = vectorize_text(query_fix)
        print(f'\nCalculating query {query_fix} ({count}/{len(queries)})')
        out.update({query_fix: calcClosestImages(vectors, vector, 10)})
        count += 1

out_json = json.dumps(out)

with open('../data/groundTruth.json', 'w') as js:
    js.truncate(0)
    js.writelines(out_json)

