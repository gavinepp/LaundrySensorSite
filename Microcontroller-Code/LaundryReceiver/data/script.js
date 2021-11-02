/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-mpu-6050-web-server/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
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
        let obj = JSON.parse(event.data);
        let machineName = obj.machineName;
        document.getElementById(machineName).innerHTML = obj.accX;
        document.getElementById("accY").innerHTML = obj.accY;
        document.getElementById("accZ").innerHTML = obj.accZ;
    }, false);
}
function resetPosition(element) {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", "/" + element.id, true);
    console.log(element.id);
    xhr.send();
}
