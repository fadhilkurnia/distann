function displayResults(results) {
    //this hides the images box 
    $("#result-area").hide();

    //removes all the html code in the section with id "result-area"
    $("#result-area").html("");
    let resultRaw = "";

    
    // for (let i = 0; i < results.length; ++i) {
    //     imgID = results[i].id;
    //     imgUrl = results[i].url;
    //     imgAlt = results[i].alt;
    //     resultRaw += `
    //       <div class="col s4">
    //         <img
    //           id="${imgID}"
    //           class="materialboxed"
    //           width="100%"
    //           src="${imgUrl}"
    //           alt="${imgAlt}">
    //       </div>`;
    // }

    //I dont think i++ vs ++i matters here
    for (let i = 0; i < results.length; i++){
        imgID = `result-img-${i}`;
        imgURL = results[i];
        imgALT = "sucks to suck";

        resultRaw += `
          <div class="col s4">
            <img
              id="${imgID}"
              class="materialboxed"
              width="100%"
              src="${imgURL}"
              alt="${imgALT}">
          </div>`;
    }
    $("#result-area").html(resultRaw);

    $("#result-area").show();
}

function retrieveImages(prompt) {
    $("#loading").show();

    console.log(`printing of prompt from retriveImages ${prompt}`);


    //the second paramter is a callback function, that only runs after a get request is made to the endpoint. Meaning the request must work, so the else statement only runs if there was an error in handaling the request at that enpoint, not if there is an error sending and recived back a response.
    //If we have an error when sending a reciving back a response, the callback will never run as it only runs after a valid request (with it sending a reciving back the response)
    $.get(`http://localhost:9000/api/search?prompt=${prompt}`, (data, status) =>{
        if (status == "success"){
            // console.log(data.results);
            urls = data.results.map(x => x.url);
            console.log(urls[3]);
            displayResults(urls)
            $("#loading").hide();
            
        }
        else{
            throw new Error(`request failed with status: ${status}`)
        }
    })

    // TODO: contact the actual distann backend
    // $.ajax({
    //     url: '/example-result.json?prompt=' + prompt,
    //     type: 'GET',
    //     success: function (data) {
    //         displayResults(data.results);
    //         console.log("success");
    //         console.log(data);
    //         $("#loading").hide();
    //     },
    //     error: function (data) {
    //         $("#loading").hide();
    //         alert("failed to retrieve data from backend :( " + data);
    //     }
    // });


}

function handlePrompt() {
    var prompt = $('#prompt-input').val();
    if (prompt == "") {
        alert("prompt cannot be empty");
        return;
    }
    console.log(`printing of prompt from handelPrompt ${prompt}`);

    retrieveImages(prompt);
}

// event: user press enter after typing prompt
$('#prompt-input').on('keypress', function (e) {
    // is 13 the enter key
    if (e.which == 13) {
        handlePrompt();
    }
})

// event: user click search button
$('#prompt-send-btn').on('click', function (e) {
    handlePrompt();
})