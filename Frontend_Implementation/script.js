var promt; // stores the prompt to convert to text description

function send(){
    //stores the promt in the global variable to be converted later on 
    //sends the a request to the backend
    //waits for the response from the backend
    promt = document.getElementById("input").value;
    
    //received the response from the backend and set the img1 - img10 to the images that we get from the backend 
    //if there is text: set the text to the description that we get from the backend
}
