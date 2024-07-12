# distann
Distributed ANN Serving

## Running the frontend
```
cd frontend
python3 -m http.server (for linux and mac)
```
Then go to your browser to access http://localhost:8000/

## Compiling and running the backend

Prerequisite:
- Install cmake and g++ in your machine
- Install drogon in your machine, ensuring we can access `<drogon/drogon.h>`.
  Follow its official documentation to see the instruction based on your OS using this link `https://github.com/drogonframework/drogon/wiki/ENG-02-Installation`

If you can't install drogon on windows: 

- On Windows if you have problems installing drogon in your machine, you can use install and run the server on WSL by following the steps in this website: `https://learn.microsoft.com/en-us/windows/wsl/install` 

- After you have installed WSL you would follow the Install by source in Linux guide in the installation website. 

- You would also want to install git to access your repository
  ```
  sudo apt install git
  ```

Before cloning repo make sure you have followed all drogon installation steps found here. `https://github.com/drogonframework/drogon/wiki/ENG-02-Installation`

- To clone a repository on git follow the steps here: `https://docs.github.com/en/repositories/creating-and-managing-repositories/cloning-a-repository#cloning-a-repository`. 
You would need your Github username and create a token (settings -> Developer settings -> Personal Access Tokens -> Tokens -> Generate new token).

- When generating token make sure you toggle 'repo' on the select scopes section to allow you to have acess to the repo as it is private.  

1. Compile the backend
    ```
    cd backend/build
    cmake ..
    make
    ```
    The last command will generate an executable called `backend` (or `backend.exe` in Windows). On linux or Mac can test backend by running ./backend in terminal, then go to http://localhost:9000
2. 
TBD
