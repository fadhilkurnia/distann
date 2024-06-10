var promt; // stores the prompt to conver to text description
var time; // stores the time


fetch('https://cocodataset.org/#explore')
.then(res => (res.ok ? res.text() : Promise.reject(new Error(res.statusText))))
.then(data => {
    console.log(data);
  });

function send(){
    //stores the promt in the global variable to be converted later on 
    promt = document.getElementById("input").value;


}