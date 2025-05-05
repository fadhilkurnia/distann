from app import vectorize_text
import heapq

def loadVectorsFromFile(path):
    with open(imageEmbeddingsPath, 'r') as f:
        lines = f.readlines()
        assert len(lines) > 0
        
        # read the first line to get the dimension
        num_dimension = 0
        num_dimension = int(lines[0].strip())
        
        # read the remaining lines, iterate through all the lines
        num_lines = len(lines)
        print("number of lines: ", num_lines)
        vectors = []
        curr_line_idx = 1
        while curr_line_idx < num_lines:
            curr_line = lines[curr_line_idx]
            assert curr_line[0] == '[', f"Incorrect start of the line"
            
            # iterate until the end of the current list (vector),
            # generating a string for the current list.
            is_curr_list_end = False
            curr_list_str = ""
            while not is_curr_list_end:
                if curr_line.strip()[-1] == ']':
                    is_curr_list_end = True
                    break
                curr_line = lines[curr_line_idx]
                curr_list_str += curr_line
                curr_line_idx += 1
            
            # convert the vector from string into a list of float
            vector_str = curr_list_str.strip()[1:-1].strip()        # remove the '[' and ']'
            vector_raw = vector_str.split()
            vector = []
            for raw in vector_raw:
                vector.append(float(raw))
            vectors.append(vector)
        return vectors

# vectors[] holds all the embeddings from the file

# Calculate distances
def calcClosestImages(vectors, input_vector, n):
    out_heap = []
    for i in range(0, len(vectors) - 1):
        dist = 0
        print(f'\rCalculating image {i+1}/{len(vectors)}', end = "\r")
        for j in range(0, len(vectors[i]) - 1):
            dist += (float(vectors[i][j]) - float(input_vector[0][j]))**2
        out_heap.append((dist, i))

    smallest = heapq.nsmallest(n, out_heap)
    smallest = [(key, value) for value, key in smallest]
    return smallest

if __name__ == "__main__":
    imageEmbeddingsPath = "../data/convertedImageEmbeddings.txt"
    user_input = input("Prompt: ")
    n = int(input("Number of results: "))
    input_vector = vectorize_text(user_input)

    vectors = loadVectorsFromFile(imageEmbeddingsPath)
    print(calcClosestImages(vectors, input_vector, n))