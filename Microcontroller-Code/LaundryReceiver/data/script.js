// Create events for the sensor readings
if (!!window.EventSource) {
    let source = new EventSource('/events');
    source.addEventListener('open', () => {
        console.log("Events Connected");
    }, false);
    source.addEventListener('error', (event) => {
        if (event.target.readyState != EventSource.OPEN) {
            console.log("Events Disconnected");
        }
    }, false);
    source.addEventListener('machine_status', (event) => {
        console.log("Machine Status: ", event.data);
        let receivedJson = JSON.parse(event.data);
        let machineHtmlElement = document.getElementById(receivedJson.id);
        if (receivedJson.status) {
            machineHtmlElement.className = "machine_on";
            machineHtmlElement.children[1].innerHTML = "Occupied";
        }
        else {
            machineHtmlElement.className = "machine_off";
            machineHtmlElement.children[1].innerHTML = "Available";
        }
    }, false);
}
