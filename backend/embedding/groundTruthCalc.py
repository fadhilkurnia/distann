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
        imgs = calcClosestImages(vectors, vector, 12)
        new_imgs = [img[0] for img in imgs]
        out.update({query_fix: new_imgs})
        count += 1

out_json = json.dumps(out)

with open('../data/groundTruth.json', 'w') as js:
    js.truncate(0)
    js.writelines(out_json)

