#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

const char* ssid = "Galaxy andres";
const char* password = "moqa0193";
const int ledPin = 2;  // pin que se esta utilizando

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // Sin ajuste de zona horaria aquí
AsyncWebServer server(80);

// Variables para almacenar el tiempo en que el LED fue encendido/apagado
String lastLedOnTime;
String lastLedOffTime;

// Variables de simulación para CPU y espacio disponible
int cpuUsage;
int availableSpace;

// Ajuste de zona horaria para Bogotá (UTC -5)
const long utcOffsetInSeconds = -5 * 3600;  // UTC-5 sin horario de verano

// Función para obtener la hora ajustada a la zona horaria de Bogotá
String getFormattedTime() {
  unsigned long rawTime = timeClient.getEpochTime() + utcOffsetInSeconds;
  time_t adjustedTime = rawTime;
  struct tm *timeinfo = localtime(&adjustedTime);
  
  char timeStringBuff[50];  // Buffer para almacenar la hora formateada
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(timeStringBuff);
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Simulador de Compuertas Lógicas con LED Físico</title>
    <style>
        *{
            box-sizing: border-box;
        }
        body {
            font-family: Arial, sans-serif;
            display: flex;
            flex-direction: column;
            align-items: center;
            background-color: #f0f0f0;
            margin: 0;
            padding: 20px;
        }
        h1, h2 {
            color: #333;
            text-align: center;
        }
        .container {
            background-color: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            margin-bottom: 20px;
            width: 100%;
            max-width: 600px;
        }
        .controls {
            display: flex;
            flex-wrap: wrap;
            justify-content: space-around;
            align-items: center;
            margin-bottom: 20px;
        }
        .switch-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            margin: 10px 0;
        }
        .switch {
            width: 60px;
            height: 34px;
            position: relative;
            display: inline-block;
        }
        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 34px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background-color: #2196F3;
        }
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        .led {
            width: 50px;
            height: 50px;
            border-radius: 50%;
            background-color: #ccc;
            margin: 20px auto;
        }
        .led.on {
            background-color: #00ff00;
            box-shadow: 0 0 20px #00ff00;
        }
        .gate-buttons {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 10px;
            margin-top: 10px;
        }
        .gate-button {
            padding: 10px 15px;
            background-color: #f0f0f0;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        .gate-button.selected {
            background-color: #2196F3;
            color: white;
        }
        .physical-led-controls {
            display: flex;
            justify-content: center;
            gap: 10px;
            margin-top: 20px;
        }
        .status-container {
            margin-top: 20px;
            padding: 10px;
            background-color: #f0f0f0;
            border-radius: 5px;
        }
        .server-status {
            text-align: center;
            font-weight: bold;
            margin-top: 10px;
            padding: 10px;
            border-radius: 5px;
        }
        .server-status.connected {
            background-color: #d4edda;
            color: #155724;
        }
        .server-status.disconnected {
            background-color: #f8d7da;
            color: #721c24;
        }
        #status{
          text-align: center;
        }
        footer {
            margin-top: 20px;
            text-align: center;
            color: #666;
        }
        .disabled-element {
            cursor: not-allowed;
        }
        .disabled-element * {
            pointer-events: none;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Simulador de Compuertas Lógicas con LED Físico</h1>
        <div id="serverStatus" class="server-status disconnected">
            Verificando conexión con el servidor...
        </div>
        <div class="controls">
            <div class="switch-container">
                <h2>Interruptor 1</h2>
                <label class="switch">
                    <input type="checkbox" id="switch1">
                    <span class="slider"></span>
                </label>
            </div>
            <div class="switch-container">
                <h2>Interruptor 2</h2>
                <label class="switch">
                    <input type="checkbox" id="switch2">
                    <span class="slider"></span>
                </label>
            </div>
        </div>
        <div>
            <h2>Compuerta Lógica</h2>
            <div class="gate-buttons">
                <button class="gate-button selected" data-gate="AND">AND</button>
                <button class="gate-button" data-gate="OR">OR</button>
                <button class="gate-button" data-gate="XOR">XOR</button>
                <button class="gate-button" data-gate="XNOR">XNOR</button>
                <button class="gate-button" data-gate="NAND">NAND</button>
                <button class="gate-button" data-gate="NOR">NOR</button>
            </div>
        </div>
        <div>
            <h2>Resultado Virtual</h2>
            <div id="led" class="led"></div>
        </div>
        <div class="status-container">
            <h2>Estado del Sistema</h2>
            <div id="status"></div>
        </div>
    </div>
    <footer>
        <p>Universidad: Politécnico Grancolombiano</p>
        <p>Integrantes: Andrés Laguilavo y Dayana Calderón</p>
    </footer>

    <script>
        const switch1 = document.getElementById('switch1');
        const switch2 = document.getElementById('switch2');
        const led = document.getElementById('led');
        const gateButtons = document.querySelectorAll('.gate-button');
        const serverStatus = document.getElementById('serverStatus');
        let selectedGate = 'AND';
        let isServerConnected = false;

        function updateLED() {
            if (!isServerConnected) return;

            const input1 = switch1.checked;
            const input2 = switch2.checked;
            let output;

            switch(selectedGate) {
                case 'AND':
                    output = input1 && input2;
                    break;
                case 'OR':
                    output = input1 || input2;
                    break;
                case 'XOR':
                    output = input1 !== input2;
                    break;
                case 'XNOR':
                    output = input1 === input2;
                    break;
                case 'NAND':
                    output = !(input1 && input2);
                    break;
                case 'NOR':
                    output = !(input1 || input2);
                    break;
            }

            led.classList.toggle('on', output);
            toggleLed(output ? 'on' : 'off');
        }

        function selectGate(gate) {
            if (selectedGate !== gate) {
                selectedGate = gate;
                gateButtons.forEach(btn => {
                    btn.classList.toggle('selected', btn.dataset.gate === gate);
                });
                updateLED();
            }
        }

        function toggleLed(state) {
            if (!isServerConnected) return;

            fetch(/led/${state}, { method: 'POST' })
            .then(response => response.text())
            .catch(error => {
                console.error('Error:', error);
                setServerStatus(false);
            });
        }

        function getStatus() {
            fetch('/status')
            .then(response => response.json())
            .then(data => {
                setServerStatus(true);
                console.log(data)
                document.getElementById('status').innerHTML = 
                    <b>Uso de CPU:</b> ${data.cpuUsage}%<br>
                    <b>Espacio disponible:</b> ${data.availableSpace} bytes<br>
                    <b>Última vez que se encendió el LED:</b> ${data.lastLedOn}<br>
                    <b>Última vez que se apagó el LED:</b> ${data.lastLedOff}<br>
                    <b>Hora actual NTP:</b> ${data.ntpTime}
                ;
            })
            .catch(error => {
                console.error('Error:', error);
                setServerStatus(false);
            });
        }

        function setServerStatus(connected) {
            isServerConnected = connected;
            serverStatus.textContent = connected ? 'Servidor conectado' : 'Servidor desconectado';
            serverStatus.className = server-status ${connected ? 'connected' : 'disconnected'};
            
            // Habilitar o deshabilitar los interruptores y el LED virtual
            switch1.disabled = !connected;
            switch2.disabled = !connected;
            led.style.opacity = connected ? '1' : '0.5';
            
            // Habilitar o deshabilitar los botones de compuertas lógicas
            gateButtons.forEach(btn => {
                btn.disabled = !connected;
                btn.classList.toggle('disabled-element', !connected);
            });
            
            // Añadir o quitar la clase 'disabled-element' a los contenedores de los switches
            document.querySelectorAll('.switch-container').forEach(container => {
                container.classList.toggle('disabled-element', !connected);
            });
            
            // Actualizar el estado del LED si el servidor está conectado
            if (connected) {
                updateLED();
            } else {
                document.getElementById('status').innerHTML = 'No hay información disponible';
            }
        }

        function initializeServerConnection() {
            getStatus();
        }

        switch1.addEventListener('change', updateLED);
        switch2.addEventListener('change', updateLED);

        gateButtons.forEach(button => {
            button.addEventListener('click', () => selectGate(button.dataset.gate));
        });

        // Inicializar la conexión del servidor al cargar la página
        initializeServerConnection();

        // Actualizar el estado cada segundo después de la inicialización
        setInterval(getStatus, 1000);
    </script>
</body>
</html>
)rawliteral";


void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);

  // Conexión a la red WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");
  Serial.println(WiFi.localIP());
  // Inicializar el cliente NTP
  timeClient.begin();

  // Inicializar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
    return;
  }


  // Rutas del servidor
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/led/on", HTTP_POST, [](AsyncWebServerRequest *request){
    digitalWrite(ledPin, HIGH);
    lastLedOnTime = getFormattedTime();
    request->send(200, "text/plain", "LED Encendido");
  });

  server.on("/led/off", HTTP_POST, [](AsyncWebServerRequest *request){
    digitalWrite(ledPin, LOW);
    lastLedOffTime = getFormattedTime();
    request->send(200, "text/plain", "LED Apagado");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<200> json;
    json["cpuUsage"] = cpuUsage;
    json["availableSpace"] = availableSpace;
    json["lastLedOn"] = lastLedOnTime;
    json["lastLedOff"] = lastLedOffTime;
    json["ntpTime"] = getFormattedTime();
    
    String jsonString;
    serializeJson(json, jsonString);
    request->send(200, "application/json", jsonString);
  });

  server.begin();
}

void loop() {
  // Actualiza la hora desde el servidor NTP
  timeClient.update();

  // Simulación de uso de CPU y espacio disponible
  cpuUsage = random(0, 100);  // Ejemplo de uso de CPU
  availableSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
}
