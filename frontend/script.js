let distannServer = "localhost:9000";

function displayResults(results) {
    // this hides the images box 
    $("#result-area").hide();

    // removes all the html code in the section with id "result-area"
    $("#result-area").html("");
    let resultRaw = "";

    for (let i = 0; i < results.length; i++){
        imgID = `result-img-${i}`;
        imgURL = results[i];;

        resultRaw += `
          <div class="col s4">
            <img
              id="${imgID}"
              class="materialboxed"
              width="100%"
              src="${imgURL}">
          </div>`;
    }
    $("#result-area").html(resultRaw);

    $("#result-area").show();
}

function retrieveImages(prompt) {
    $("#loading").show();

    // The second parameter is a callback function that only runs after a get
    // request is made to the endpoint. Meaning the request must work, so the
    // else statement only runs if there was an error in handaling the request
    // at that enpoint, not if there is an error sending and received back a
    // response. If we have an error when sending a receiving back a response,
    // the callback will never run as it only runs after a valid request
    // (with it sending a reciving back the response).
    $.get(`http://${distannServer}/api/search?prompt=${prompt}`,
        (data, status) => {
            if (status == "success"){
                urls = data.results.map(x => x.url);
                console.log(urls[3]);
                displayResults(urls)
                $("#loading").hide();
            } else{
                alert("failed to retrieve data from backend: " + status);
                $("#loading").hide();
            }
    });
}

function handlePrompt() {
    var prompt = $('#prompt-input').val();
    if (prompt == "") {
        alert("prompt cannot be empty");
        return;
    }

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