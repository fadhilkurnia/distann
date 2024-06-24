# distann
Distributed ANN Serving

## Running the frontend
```
cd frontend
python3 -m http.server
```
Then go to your browser to access http://localhost:8000/

## Compiling and running the backend

Prerequisite:
- Install cmake and g++ in your machine
- Install drogon in your machine, ensuring we can access `<drogon/drogon.h>`.
  Follow its official documentation to see the instruction based on your OS.

1. Compile the backend
    ```
    cd backend/build
    cmake ..
    make
    ```
    The last command will generate an executable called `backend` (or `backend.exe` in Windows).
2. 
TBD