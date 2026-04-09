"use strict";

let currentRequest = null;
let cnt_disconn = 0;

function req_rt_data() {
    if (currentRequest && currentRequest.readyState !== 4) {
        currentRequest.abort();
        cnt_disconn = cnt_disconn + 1;
        if (cnt_disconn > 4) {
            cnt_disconn = 4;
            document.getElementById('labelOffline').style.display = 'block';
        }
    }

    currentRequest = new XMLHttpRequest();
    currentRequest.onreadystatechange = function () {
        if (currentRequest.readyState == 4 && currentRequest.status == 200) {
            rt_data_cb(currentRequest.responseText);
            cnt_disconn = cnt_disconn - 1;
            if (cnt_disconn < -4) {
                cnt_disconn = -4;
                document.getElementById('labelOffline').style.display = 'none';
            }
        }
    }
    currentRequest.open("GET", "/api/rtd", true);// true for asynchronous 
    currentRequest.send(null);
}

function logAppend(m) {
    var newlineIndex = m.indexOf('\n');
    if (newlineIndex === -1) {
        appendSingleLine(m);
    } else {
        var firstLine = m.substring(0, newlineIndex);
        var rest = m.substring(newlineIndex + 1);
        appendSingleLine(firstLine);
        logAppend(rest);
    }
}

function appendSingleLine(line) {
    if (line === "") return;
    const line_mod = line.replace(/ /g, '&nbsp;');

    var d = new Date();
    document.getElementById('messageLog').innerHTML += "<a style=\"color:blue;font-size:12px\">" +
        d.toLocaleTimeString('en-US', { hour12: false }) + "." + ('00' + d.getMilliseconds()).slice(-3) +
        ": </a><code style=\"font-size:12pxwhite-space:pre\">" +
        line_mod +
        "</code><br/>";

    var elem = document.getElementById('messageLog');
    elem.scrollTop = elem.scrollHeight;
}

function rt_data_cb(resp) {
    const obj = JSON.parse(resp);

    if (obj.hasOwnProperty("console")) {
        logAppend(obj.console);
    }
}

function consoleSend() {
    var xmlHttp = new XMLHttpRequest();
    xmlHttp.open("POST", "/console/" + document.getElementById("console").value, true); // true for asynchronous 
    xmlHttp.send(null);
    logAppend("send: " + document.getElementById("console").value);
}

function consoleSendEvent(event) {
    if (event.key === "Enter") {
        consoleSend()
    }
}

function startUpload() {
    document.getElementById("status").innerHTML = "Downloading...";
    var otafile = document.getElementById("otafile").files;
    var file = otafile[0];
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function () {
        if (xhr.readyState == 4) {
            if (xhr.status == 200) {
                document.getElementById("status").innerHTML = xhr.responseText;
            } else if (xhr.status == 0) {
                alert("Server closed the connection");
                location.reload();
            } else {
                alert(xhr.status + " Error!\n" + xhr.responseText);
                location.reload();
            }
        }
    };
    xhr.upload.onprogress = function (e) { document.getElementById("status").innerHTML = ((e.loaded / e.total * 100).toFixed(0)) + "%"; };
    xhr.open("POST", "/update", true);
    xhr.send(file);
}

window.onload = function () {
    setInterval(req_rt_data, 250);
}

window.startUpload = startUpload;
window.consoleSend = consoleSend;
window.consoleSendEvent = consoleSendEvent;