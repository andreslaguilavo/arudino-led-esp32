#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <WebSocketsServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

const char* ssid = "Galaxy andres";
const char* password = "moqa0193";
// Variables de simulación
int cpuUsage = 30;
int availableSpace = 5000;
const int maxHistorialSize = 50;
String historial[maxHistorialSize];
int historialIndex = 0;

// Pines y WebSocket
const int pin1 = 2;
const int pin2 = 4;
const int pin3 = 5;
const int pin4 = 18;
const int entryPin = 21;
int lastStatePin21 = LOW;  // Variable para almacenar el estado anterior del pin
unsigned long lastTimeSent = 0;
const long interval = 10000;  // Intervalo de 10 segundos

//Google sheet
const char* serverName = "https://script.google.com/macros/s/AKfycbwzuO5VG14rzrk3BmGbTEPy48zmI3rpUUi_DnBS3dmJLm3WhfTnyxVxzAnhvfDA3IMu/exec";


AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Ajuste de zona horaria para Bogotá (UTC -5)
const long utcOffsetInSeconds = -5 * 3600;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.nist.gov", utcOffsetInSeconds);

// Obtener la hora ajustada
String getFormattedTime() {
  timeClient.update();
  unsigned long rawTime = timeClient.getEpochTime();  // No se necesita sumar utcOffsetInSeconds
  time_t adjustedTime = rawTime;
  struct tm* timeinfo = localtime(&adjustedTime);

  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", timeinfo);
  return String(timeStringBuff);
}


// Función para guardar el historial en memoria
void guardarHistorial(String evento) {
  historial[historialIndex] = evento;
  historialIndex = (historialIndex + 1) % maxHistorialSize;
  EEPROM.put(historialIndex, evento);
  EEPROM.commit();
}

// Función para enviar historial al cliente WebSocket
void enviarHistorial(uint8_t num) {
  String allEvents = "";
  for (int i = 0; i < maxHistorialSize; i++) {
    if (historial[i] != "") {
      allEvents += historial[i] + "\n";  // Concatenar todos los eventos
    }
  }
  if (allEvents != "") {
    webSocket.sendTXT(num, allEvents);  // Enviar todos los eventos en un solo mensaje
  }
}
// Funcion para enviar los datos a google Sheets
void sendToGoogleSheet() {
  const String time = getFormattedTime();
  String estadoLed = digitalRead(entryPin) ? "Encendida" : "Apagado";

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    int valuePin1 = digitalRead(pin1);
    int valuePin2 = digitalRead(pin2);
    int valuePin3 = digitalRead(pin3);
    int valuePin4 = digitalRead(pin4);

    String jsonData = "{";
    jsonData += "\"pins\": [";
    jsonData += String(valuePin1) + ",";
    jsonData += String(valuePin2) + ",";
    jsonData += String(valuePin3) + ",";
    jsonData += String(valuePin4);
    jsonData += "],";
    jsonData += "\"ledStatus\": \"" + estadoLed + "\",";
    jsonData += "\"time\": \"" + time + "\"";
    jsonData += "}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Wrong request POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
}

// Función para manejar los estados de los pines
void controlarPin(int pin, bool estado, const char* accion) {
  Serial.print(estado);
  digitalWrite(pin, estado ? HIGH : LOW);

  const String time = getFormattedTime();
  String evento = String(accion) + " a las " + time;

  guardarHistorial(evento);
  String allEvents = "";
  for (int i = 0; i < maxHistorialSize; i++) {
    if (historial[i] != "") {
      allEvents += historial[i] + "\n";  // Concatenar todo el historial
    }
  }
  webSocket.broadcastTXT(allEvents);
}

// Manejador de WebSocket
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload);

    if (msg == "historial") {
      enviarHistorial(num);
    } else if (msg == "hora") {
      String currentTime = getFormattedTime();
      webSocket.sendTXT(num, currentTime);  // Enviar la hora al cliente
    } else if (msg.startsWith("pin")) {
      int pin = msg.charAt(3) - '0';
      bool estado = msg.endsWith("on");

      switch (pin) {
        case 1: controlarPin(pin1, estado, estado ? "Activa entrada 1" : "Desactivada entrada 1"); break;
        case 2: controlarPin(pin2, estado, estado ? "Activa entrada 2" : "Desactivada entrada 2"); break;
        case 3: controlarPin(pin3, estado, estado ? "Activa entrada 3" : "Desactivada entrada 3"); break;
        case 4: controlarPin(pin4, estado, estado ? "Activa entrada 4" : "Desactivada entrada 4"); break;
      }
    }
  }
}



const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Simulador de Compuertas Lógicas con LED Físico</title>
    <style>
      /* Estilos mantenidos igual que en el código original */
      * {
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
      h1,
      h2 {
        color: #333;
        text-align: center;
      }
      .container {
        background-color: white;
        border-radius: 8px;
        padding: 20px;
        box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        margin-bottom: 20px;
        width: 100%;
        max-width: 600px;
      }
      .controls {
        display: grid;
        grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
        flex-wrap: wrap;
        justify-content: space-around;
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
        transition: 0.4s;
        border-radius: 34px;
      }
      .slider:before {
        position: absolute;
        content: '';
        height: 26px;
        width: 26px;
        left: 4px;
        bottom: 4px;
        background-color: white;
        transition: 0.4s;
        border-radius: 50%;
      }
      input:checked + .slider {
        background-color: #2196f3;
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
      #status {
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
      .hora {
        text-align: center;
        font-weight: bold;
        font-size: larger;
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
          <h2>S1 / A</h2>
          <label class="switch">
            <input type="checkbox" id="switch1" />
            <span class="slider"></span>
          </label>
        </div>
        <div class="switch-container">
          <h2>S2 / B</h2>
          <label class="switch">
            <input type="checkbox" id="switch2" />
            <span class="slider"></span>
          </label>
        </div>
        <div class="switch-container">
          <h2>S3 / C</h2>
          <label class="switch">
            <input type="checkbox" id="switch3" />
            <span class="slider"></span>
          </label>
        </div>
        <div class="switch-container">
          <h2>S4 / D</h2>
          <label class="switch">
            <input type="checkbox" id="switch4" />
            <span class="slider"></span>
          </label>
        </div>
      </div>
      <div>
        <h2>Resultado</h2>
        <div id="led" class="led"></div>
      </div>
      <div id="horaContainer">
        <h2>Hora actual:</h2>
        <p id="horaActual" class="hora"></p>
      </div>
      <div id="historialContainer">
        <h2>Historial de Eventos</h2>
        <ul id="historialList"></ul>
      </div>
    </div>
    <footer>
      <p>Universidad: Politécnico Grancolombiano</p>
      <p>Integrantes: Andrés Laguilavo y Dayana Calderón</p>
    </footer>

    <script>
      let socket
      const switch1 = document.getElementById('switch1')
      const switch2 = document.getElementById('switch2')
      const switch3 = document.getElementById('switch3')
      const switch4 = document.getElementById('switch4')
      const serverStatus = document.getElementById('serverStatus')
      const led = document.getElementById('led')
      const historialList = document.getElementById('historialList')

      // Función para inicializar WebSocket
      function initWebSocket() {
        
        socket = new WebSocket('ws://' + window.location.hostname + ':81/');

        socket.onopen = function () {
          console.log('Conectado al servidor')
          serverStatus.textContent = 'Conectado al servidor'
          serverStatus.classList.remove('disconnected')
          serverStatus.classList.add('connected')

          // Solicitar historial y la hora cuando se conecta
          // socket.send('hora')
          socket.send('historial')
        }

        socket.onclose = function () {
          serverStatus.textContent = 'Desconectado del servidor'
          serverStatus.classList.remove('connected')
          serverStatus.classList.add('disconnected')
          setTimeout(initWebSocket, 2000) // Intentar reconectar
        }

        socket.onmessage = function (event) {
          const data = event.data
          console.log('Evento:', data)
          if (data === 'HIGH') {
            led.classList.add('on')
          } else if (data === 'LOW') {
            led.classList.remove('on')
          } else if (
            data.startsWith('Activa') ||
            data.startsWith('Desactivada')
          ) {
            mostrarHistorial(data)
          } else {
            document.getElementById('horaActual').textContent = data
          }
        }
      }

      function mostrarHistorial(data) {
        const eventos = data.split('\n')
        historialList.innerHTML = '' // Limpiar lista actual
        eventos.reverse().forEach((evento) => {
          if (evento.trim() !== '') {
            const listItem = document.createElement('li')
            listItem.textContent = evento
            historialList.appendChild(listItem)
          }
        })
      }

      switch1.addEventListener('change', () =>
        togglePin(1, switch1.checked ? 'on' : 'off')
      )
      switch2.addEventListener('change', () =>
        togglePin(2, switch2.checked ? 'on' : 'off')
      )
      switch3.addEventListener('change', () =>
        togglePin(3, switch3.checked ? 'on' : 'off')
      )
      switch4.addEventListener('change', () =>
        togglePin(4, switch4.checked ? 'on' : 'off')
      )

      function togglePin(pin, action) {
        const message = `pin${pin}/${action}`
        if (socket.readyState === WebSocket.OPEN) {
          socket.send(message)
        } else {
          console.error('WebSocket no está conectado.')
        }
      }

      // Iniciar WebSocket al cargar la página

      window.onload = function () {
        // console.log('Iniciando WebSocket...');
        initWebSocket()
      }
    </script>
  </body>
</html>



)rawliteral";

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);  // Inicializamos EEPROM

  // Configuración de los pines
  pinMode(pin1, OUTPUT);
  pinMode(pin2, OUTPUT);
  pinMode(pin3, OUTPUT);
  pinMode(pin4, OUTPUT);
  pinMode(entryPin, INPUT);  // Configurar el pin 21 como entrada

  // Conexión a WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");
  Serial.println(WiFi.localIP());

  // Iniciar el servidor WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Iniciar el servidor NTP
  timeClient.begin();


  // Configurar rutas del servidor web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html);  // Tu HTML aquí
  });

  server.begin();
}

int lastStatePin1 = 0;
int lastStatePin2 = 0;
int lastStatePin3 = 0;
int lastStatePin4 = 0;

void loop() {
  webSocket.loop();     // Mantener WebSocket activo
  timeClient.update();  // Actualizar la hora NTP

  if (millis() - lastTimeSent > interval) {
    lastTimeSent = millis();
    String currentTime = getFormattedTime();
    webSocket.broadcastTXT(currentTime);  // Enviar hora por WebSocket
  }

  int currentStatePin21 = digitalRead(entryPin);

  // Flag para saber si ya se envió a Google Sheets en este ciclo
  bool hasSentToGoogleSheet = false;

  // Validar cambios en los pines 1, 2, 3, y 4
  if (lastStatePin1 != digitalRead(pin1) || 
      lastStatePin2 != digitalRead(pin2) || 
      lastStatePin3 != digitalRead(pin3) || 
      lastStatePin4 != digitalRead(pin4)) {
    
    // Actualizar estados previos
    lastStatePin1 = digitalRead(pin1);
    lastStatePin2 = digitalRead(pin2);
    lastStatePin3 = digitalRead(pin3);
    lastStatePin4 = digitalRead(pin4);
    
    // Enviar datos a Google Sheets si no se ha enviado ya
    if (!hasSentToGoogleSheet) {
      sendToGoogleSheet();
      hasSentToGoogleSheet = true;
    }
  }

  // Validar cambios en el switch (entryPin)
  if (currentStatePin21 != lastStatePin21) {
    Serial.println(currentStatePin21); 
    lastStatePin21 = currentStatePin21;  // Actualizar estado previo
    String mensaje = (currentStatePin21 == HIGH) ? "HIGH" : "LOW";
    
    // Enviar mensaje por WebSocket
    webSocket.broadcastTXT(mensaje);  
    Serial.println(mensaje);          // Mostrar en consola para depuración
    
    // Enviar datos a Google Sheets si no se ha enviado ya
    if (!hasSentToGoogleSheet) {
      sendToGoogleSheet();
      hasSentToGoogleSheet = true;
    }
  }
}
