# distann - backend

Project structure:
```
backend/
├─ build/               // compilation directory, omitted in git.
├─ images/              // dataset of images, the filename is the image ID.
│  ├─ 0000.png
│  ├─ 0001.png
│  ├─ 0002.png
│  ├─ 0003.png
│  ├─ ...
│  ├─ 9999.png
├─ libs/                // the libraries used by this backend.
│  ├─ clip-cpp/         // model that convert text/image into embeddings.
│  ├─ hnswlib/          // an implementation of HNSW: indexing & search.
│  ├─ usearch/          // another implementation of HNSW: indexing & search.
├─ CMakeLists.txt       // cmake file to generate the Makefile in the build/.
├─ main.cc              // the main function of this backend.
├─ README.md
```

## Getting the dependencies
TBD

## Preparing the embeddings dataset
TBD

## Compiling and running the backend
1. Go to the `build` directory, please make the directory if not exist.
   ```
   cd build
   ```
2. From the `build` directory, generate the `Makefile` with `cmake`:
   ```
   cmake ..
   ```
3. Compile the backend to generate the `backend` binary:
   ```
   make
   ```
   The command above should produce `backend` (or `backend.exe`) in Windows.
4. Finally, run the `backend`:
   ```
   ./backend
   ```
   The command above will run the backend in the default port of `9000`.
   You can access the backend in your browser by visiting 
   `http://localhost:9000`.
