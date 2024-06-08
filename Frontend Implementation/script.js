var promt; // stores the prompt to conver to text description
var time; // stores the time

const http = new XMLHttpRequest();
const url = 'https://cocodataset.org/#explore'; // the url of the server
http.open("GET", url);
http.send();



//testing
http.onreadystatechange = (e) => {
    console.log(http.responseText)
  }
  
function send(){
    //stores the promt in the global variable to be converted later on 
    promt = document.getElementById("input").value;


}