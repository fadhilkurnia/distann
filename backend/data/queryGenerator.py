from wonderwords import RandomWord
import os

word = RandomWord()

queries = int(input("Number of queries: "))
all_queries = []
processed = 0

with open("queries.txt", "w") as f:
    f.truncate(0)
    while(processed < queries):
        w = word.word(include_parts_of_speech=["nouns"])
        if w not in all_queries:
            f.writelines(w + '\n')
            all_queries.append(w)
            processed += 1
