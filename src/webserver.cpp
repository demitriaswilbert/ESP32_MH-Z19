#include "main.h"

/**
 * @brief main HTML
 *
 */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
    <title>ESP Web Server</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
        html {
            font-family: Arial, Helvetica, sans-serif;
            text-align: center;
        }

        h1 {
            font-size: 1.8rem;
            color: white;
        }

        h2 {
            font-size: 1.5rem;
            font-weight: bold;
            color: #143642;
        }

        .topnav {
            overflow: hidden;
            background-color: #143642;
        }

        body {
            margin: 0;
        }

        .content {
            padding: 30px;
            max-width: 600px;
            margin: 0 auto;
        }

        .card {
            background-color: #F8F7F9;
            box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);
            padding-top: 10px;
            padding-bottom: 20px;
        }

        .button {
            display: inline-block;
            padding: 15px 50px;
            font-size: 24px;
            text-align: center;
            font-style: consolas;
            outline: none;
            color: #fff;
            background-color: #0f8b8d;
            border: none;
            border-radius: 5px;
            margin: 1%;
            cursor: pointer;
            -webkit-touch-callout: none;
            -webkit-user-select: none;
            -khtml-user-select: none;
            -moz-user-select: none;
            -ms-user-select: none;
            user-select: none;
            -webkit-tap-highlight-color: rgba(0, 0, 0, 0);
        }

        .button:active {
            background-color: #0f8b8d;
            box-shadow: 2 2px #CDCDCD;
            transform: translateY(2px);
        }

        .button:hover {
            background-color: #5bb9ba;
        }

        .download-state {
            font-size: 1.5rem;
            color: #8c8c8c;
            font-weight: bold;
            align-items: center;
        }

        .state {
            font-size: 1rem;
            color: #8c8c8c;
            font-weight: bold;
            display: flex;
            align-items: center;
        }

        .slider {
            width: 80%;
            height: 30px;
        }

        .row-left {
            flex: 0 25%; /* Set a fixed width for alignment */
            text-align: left;
            padding-left: 20%; /* Add some space after the text */
        }

        .row-center {
            flex: 0 5%; /* Set a fixed width for alignment */
        }

        .row-right {
            flex: 1;
            text-align: left;
        }

    </style>
    <title>ESP Web Server</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
</head>

<body>
    <div class="topnav">
        <h1>ESP WebSocket Server</h1>
    </div>
    <div class="content">
        <div class="card">
            <h2>Firmware Upload</h2>
            <form method="POST" action="#" enctype="multipart/form-data" id="upload_form">
                <div>
                    <input type="file" name="update" class="button" id="file" onchange="sub(this)"
                        style="display:none;" />
                    <label for="file" class="button button-file" id="filename">Upload File</label>
                </div>
                <input type="submit" class="button button-submit" value="Update" disabled="" />
                <div id="prg" class="download-state"></div>
                <br />
                <div id="prgbar">
                    <div id="bar"></div>
                </div>
                <br />
            </form>
        </div>
        <div class="card">
            <h2>Sensors</h2>
            <p class="state">
                <span class="row-left">CPU Temp</span>
                <span class="row-center"> : </span>
                <span class="row-right"><span id="cputemp-state"></span> &deg;C</span>
            </p>
            <p class="state">
                <span class="row-left">Air Temp</span>
                <span class="row-center"> : </span>
                <span class="row-right"><span id="temp-state"></span> &deg;C</span>
            </p>
            <p class="state">
                <span class="row-left">Humidity</span>
                <span class="row-center"> : </span>
                <span class="row-right"><span id="humid-state"></span> %</span>
            </p>
            <p class="state">
                <span class="row-left">CO2 Conc.</span>
                <span class="row-center"> : </span>
                <span class="row-right"><span id="co2_ppm-state"></span> ppm</span>
            </p>
        </div>
        <div class="card">
            <h2>LED period</h2>
            <input type="button" class="button" id="led" value="" onclick="ledbutton()">
            <p class="state">
                <span class="row-left">RGB Period</span>
                <span class="row-center"> : </span>
                <span class="row-right"><span id="period-state"></span> ms</span>
            </p>
            <p><input id="period" type="range" class="slider" min="100" max="10000" step="100" oninput="inputDelay(this)"
                    onchange="updateDelay(this)"></p>
        </div>
        <div class="card">
            <h2>Network Info</h2>
            <p class="state state-row">
                <span class="row-left">RSSI</span>
                <span class="row-center"> : </span>
                <span class="row-right" id="rssi-state"></span>
            </p>
            <p class="state state-row">
                <span class="row-left">SSID</span> 
                <span class="row-center"> : </span>
                <span class="row-right" id="ssid"></span>
            </p>
            <p class="state state-row">
                <span class="row-left">Gateway</span> 
                <span class="row-center"> : </span>
                <span class="row-right" id="gw"></span>
            </p>
            <p class="state state-row">
                <span class="row-left">Subnet Mask</span> 
                <span class="row-center"> : </span>
                <span class="row-right" id="sm"></span>
            </p>
            <p class="state state-row">
                <span class="row-left">Clients</span> 
                <span class="row-center"> : </span>
                <span class="row-right" id="clients"></span>
            </p>
        </div>
        
    </div>
    <script>
        function sub(obj) {
            var fileName = obj.value.split("\\");
            document.getElementById("filename").innerText = "File: " + fileName[fileName.length - 1];
            if (fileName[fileName.length - 1].length > 0) {
                $("input:submit").attr("disabled", false);
            } else {
                $("input:submit").attr("disabled", true);
                document.getElementById("filename").innerText = "Upload File";
            }
        }
        $("form").submit(function (e) {
            e.preventDefault();
            var form = $("#upload_form")[0];
            var data = new FormData(form);
            $.ajax({
                url: "/update",
                type: "POST",
                data: data,
                contentType: false,
                processData: false,
                xhr: function () {
                    var xhr = new window.XMLHttpRequest();
                    xhr.upload.addEventListener(
                        "progress",
                        function (evt) {
                            if (evt.lengthComputable) {
                                var per = evt.loaded / evt.total;
                                $("#prg").html("Progress: " + Math.round(per * 100) + "%");
                                $("#bar").css("width", Math.round(per * 100) + "%");
                            }
                        },
                        false
                    );
                    return xhr;
                },
                success: function (d, s) {
                    console.log("success!");
                },
                error: function (a, b, c) { },
            });
        });
        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;
        var ignores = {};
        var myObj = {};
        let elemId = (x) => {return document.getElementById(x);}
        window.addEventListener('load', onLoad);
        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen = onOpen;
            websocket.onclose = onClose;
            websocket.onmessage = onMessage; // <-- add this line
        }
        function onOpen(event) {
            console.log('Connection opened');
        }
        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        }
        function onMessage(event) {
            myObj = JSON.parse(event.data);
            console.log(myObj);

            if (myObj.hasOwnProperty("cputemp"))
                elemId('cputemp-state').innerText = myObj["cputemp"];

            if (myObj.hasOwnProperty("temp"))
                elemId('temp-state').innerText = myObj["temp"];

            if (myObj.hasOwnProperty("humid"))
                elemId('humid-state').innerText = myObj["humid"];

            if (myObj.hasOwnProperty("co2_ppm"))
                elemId('co2_ppm-state').innerText = myObj["co2_ppm"];

            if (myObj.hasOwnProperty("period") && (ignores['period'] != true)) {
                elemId('period').value = myObj["period"];
                elemId('period-state').innerText = myObj["period"];
            }
            if (myObj.hasOwnProperty("ssid")) 
                elemId('ssid').innerText = myObj["ssid"];

            if (myObj.hasOwnProperty("gw")) 
                elemId('gw').innerText = myObj["gw"];

            if (myObj.hasOwnProperty("sm")) 
                elemId('sm').innerText = myObj["sm"];

            if (myObj.hasOwnProperty("clients")) 
                elemId('clients').innerText = myObj["clients"];

            if (myObj.hasOwnProperty("led")) 
                elemId('led').value = (myObj["led"] == "ON"? "Turn Led OFF" : "Turn Led ON");
                
            if (myObj.hasOwnProperty("rssi"))
                elemId('rssi-state').innerText = myObj["rssi"];

        }

        function ledbutton(elem) {
            websocket.send('\033KB toggle\033');
        }
        function inputDelay(elem) {
            // ignores[elem.id] = true;
            let sliderValue = elem.value;
            if (sliderValue >= 100 && sliderValue <= 1000)
                elem.step = 10
            else if (sliderValue > 1000 && sliderValue < 10000)
                elem.step = 100
            elemId('period-state').innerText = sliderValue;
        }
        function updateDelay(elem) {
            let sliderValue = elem.value;
            elemId('period-state').innerText = sliderValue;
            websocket.send('\033KB delay ' + sliderValue + '\033');
            // ignores[elem.id] = false;
        }
        function onLoad(event) {
            initWebSocket();
        }
        function restart(elem) {
            websocket.send('KB restart\n');
        }
    </script>
</body>
</html>
)rawliteral";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
bool send_update = false;

/**
 * @brief send json message to client
 * 
 */
void notifyClients(String str) {
    if (WiFi.isConnected())
        ws.textAll(str);
}

/**
 * @brief handle incoming websocket message
 * 
 * @param arg AwsFrameInfo
 * @param data 
 * @param len 
 */
static void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len &&
        info->opcode == WS_TEXT) {
        data[len] = 0;
        process_string(Serial, (const char*)data);
    }
}

/**
 * @brief websocket event callback
 * 
 * @param server websocket server
 * @param client websocket server
 * @param type event type
 * @param arg 
 * @param data 
 * @param len 
 */
static void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                    AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT: 
        {
            size_t client_length = server->getClients().length();
            Serial.printf(
                "WebSocket client #%u connected from %s. Total clients %lu\n",
                client->id(), client->remoteIP().toString().c_str(),
                client_length);
            websocket_queue_send("ssid", WiFi.SSID());
            websocket_queue_send("gw", WiFi.gatewayIP().toString());
            websocket_queue_send("sm", WiFi.subnetMask().toString());
            websocket_queue_send("clients", String(client_length));
            websocket_queue_send("period", String(rgb_period));
            websocket_queue_send("led", String(rgb_led_on? "ON" : "OFF"));
            break;
        }

        case WS_EVT_DISCONNECT:
        {
            size_t client_length = server->getClients().length();
            Serial.printf(
                "WebSocket client #%u disconnected. Total clients %lu\n",
                client->id(), server->getClients().length());
            websocket_queue_send("clients", String(client_length));
            break;
        }

        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void server_handle_task(void* param) 
{
    log_d("Task %s running on core %d", "server_handle_task", xPortGetCoreID());
    static bool textfile = false;

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        // request->send(200, "text/html", getHTML(index_html));
        request->send_P(200, "text/html", index_html);
    });


    server.on(
        "/update", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            bool success = !Update.hasError();
            request->send(200, "text/plain", success ? "OK" : "FAIL");
            if (success && !textfile) {
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest* request, const String& filename, size_t index,
           uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                Serial.printf("Update Start: %s\n", filename.c_str());
                textfile = !filename.endsWith(".bin");
                Serial.printf("Received %s\n", textfile ? "Text" : "bin");
                if (!textfile) {
                    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                        Update.printError(Serial);
                    }
                }
            }
            if (len) {
                if (textfile) {
                    for (size_t i = 0; i < len; i++) {
                        process_char(Serial, data[i]);
                    }
                } else {
                    if (Update.write(data, len) != len) {
                        Update.printError(Serial);
                    }
                }
            }
            if (final) {
                if (textfile) {
                    process_char(Serial, '\033');
                } else {
                    if (Update.end(true)) {
                    } else {
                        Update.printError(Serial);
                    }
                }
            }
        });

    // init web socket    
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    json_pair_t* recv_report;

    while (!WiFi.isConnected()) {
        vTaskDelay(1000);
        Serial.printf("[SERVER] WiFi not connected\n");
    }
    Serial.printf("[SERVER] Starting server\n");
    server.begin();

    while (true) {

        JSONVar json_report;
        bool new_report = false;

        while (xQueueReceive(websocket_report_queue, &recv_report, 0) == pdTRUE) {
            json_report[recv_report->key] = recv_report->value;
            delete recv_report;
            new_report = true;
        } 
        
        if (new_report) {
            // Serial.println(JSON.stringify(json_report));
            notifyClients(JSON.stringify(json_report));
        }

        ws.cleanupClients();
        vTaskDelay(100);
    }
    server.end();
    vTaskDelete(NULL);
}
