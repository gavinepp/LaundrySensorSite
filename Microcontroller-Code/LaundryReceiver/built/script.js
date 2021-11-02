/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-mpu-6050-web-server/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
window.addEventListener('resize', onWindowResize, false);
// Create events for the sensor readings
if (!!window.EventSource) {
    var source = new EventSource('/events');
    source.addEventListener('open', function (e) {
        console.log("Events Connected");
    }, false);
    source.addEventListener('error', function (e) {
        if (e.target.readyState != EventSource.OPEN) {
            console.log("Events Disconnected");
        }
    }, false);
    source.addEventListener('machine_status', function (e) {
        console.log("Machine Status: ", e.data);
        var obj = JSON.parse(e.data);
        document.getElementById("accX").innerHTML = obj.accX;
        document.getElementById("accY").innerHTML = obj.accY;
        document.getElementById("accZ").innerHTML = obj.accZ;
    }, false);
}
function resetPosition(element) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/" + element.id, true);
    console.log(element.id);
    xhr.send();
}
