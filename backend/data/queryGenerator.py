from wonderwords import RandomWord
import os

word = RandomWord()

queries = int(input("Number of queries: "))

with open("queries.txt", "w") as f:
    f.truncate(0)
    for i in range(0, queries):
        f.writelines(word.word(include_parts_of_speech=["nouns"]) + '\n')
