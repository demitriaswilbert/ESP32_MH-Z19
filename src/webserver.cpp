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

        .state {
            font-size: 1.5rem;
            color: #8c8c8c;
            font-weight: bold;
        }

        .slider {
            width: 80%;
            height: 30px;
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
                <div id="prg" class="state"></div>
                <br />
                <div id="prgbar">
                    <div id="bar"></div>
                </div>
                <br />
            </form>
        </div>
        <div class="card">
            <h2>Temperature</h2>
            <p class="state"><span id="temp-state">%STATE%</span> &deg;C</p>
            <h2>Humidity</h2>
            <p class="state"><span id="humid-state">%STATE%</span> %</p>
        </div>
        <div class="card">
            <h2>CO2 ppm</h2>
            <p class="state"><span id="co2_ppm-state">%STATE%</span>ppm</p>
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

            if (myObj.hasOwnProperty("temp"))
                elemId('temp-state').innerText = myObj["temp"];

            if (myObj.hasOwnProperty("humid"))
                elemId('humid-state').innerText = myObj["humid"];

            if (myObj.hasOwnProperty("co2_ppm"))
                elemId('co2_ppm-state').innerText = myObj["co2_ppm"];

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
        Serial.write(data, len);
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
            Serial.printf(
                "WebSocket client #%u connected from %s. Total clients %lu\n",
                client->id(), client->remoteIP().toString().c_str(),
                server->getClients().length());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf(
                "WebSocket client #%u disconnected. Total clients %lu\n",
                client->id(), server->getClients().length());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

String processor(const String& var) {
    if (var == "MACADDR") {
        return WiFi.macAddress();
    } else if (var == "SSID") {
        return WiFi.SSID();
    } else if (var == "RSSI") {
        return String(WiFi.RSSI());
    } else if (var == "TXPWR") {
        return String(WiFi.getTxPower());
    }
    return String();
}

String getHTML(const char* data) {
    String html = "";
    char keyword[50];
    int _start = -1;
    for (int i = 0; index_html[i] != 0; i++) {
        if (index_html[i] == '%') {
            if (_start == -1)
                _start = i + 1;
            else {
                html += processor(keyword);
                _start = -1;
                keyword[0] = 0;
            }
        } else if (_start != -1) {
            keyword[i - _start] = index_html[i];
            keyword[i - _start + 1] = 0;
            if (!(index_html[i] >= 'A' && index_html[i] <= 'Z')) {
                // non keyword
                _start = -1;
                html += (char)'%';
                html += keyword;
                keyword[0] = 0;
            }
        } else
            html += index_html[i];
    }
    if (keyword[0] != 0) {
        html += (char)'%';
        html += keyword;
        keyword[0] = 0;
    }
    return html;
}

void server_handle_task(void* param) {

    static bool textfile = false;

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", getHTML(index_html));
    });

    // Handle firmware file upload
    
    int64_t cleanup_client_tmr = esp_timer_get_time();
    int64_t notify_client_tmr = esp_timer_get_time();

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
                    // for (size_t i = 0; i < len; i++) {
                    //     process_char(Serial, data[i]);
                    // }
                    Serial.write(data, len);
                } else {
                    if (Update.write(data, len) != len) {
                        Update.printError(Serial);
                    }
                }
            }
            if (final) {
                if (textfile) {
                    // process_char(Serial, '\033');
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

        while (xQueueReceive(websocket_report_queue, &recv_report, 500) == pdTRUE) {
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
