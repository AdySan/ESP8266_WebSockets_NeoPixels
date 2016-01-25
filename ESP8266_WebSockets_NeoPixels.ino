#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>
#include <NeoPixelBus.h>
#include <FS.h>

#define pixelCount 16
#define colorSaturation 255
NeoPixelBus strip = NeoPixelBus(pixelCount, 8, NEO_GRB | NEO_KHZ800);
RgbColor red = RgbColor(colorSaturation, 0, 0);
RgbColor green = RgbColor(0, colorSaturation, 0);
RgbColor blue = RgbColor(0, 0, colorSaturation);
RgbColor white = RgbColor(colorSaturation);
RgbColor black = RgbColor(0);

#define USE_SERIAL Serial

const char* ssid     = "*****";
const char* password = "*****";
const char* mDNSid   = "WebsocketsTest";

ESP8266WiFiMulti WiFiMulti;

ESP8266WebServer server(80);
//holds the current upload
File fsUploadFile;
WebSocketsServer webSocket = WebSocketsServer(81);

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  USE_SERIAL.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    USE_SERIAL.print("handleFileUpload Name: "); USE_SERIAL.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //USE_SERIAL.print("handleFileUpload Data: "); USE_SERIAL.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    USE_SERIAL.print("handleFileUpload Size: "); USE_SERIAL.println(upload.totalSize);
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  USE_SERIAL.println("handleFileDelete: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  USE_SERIAL.println("handleFileCreate: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = server.arg("dir");
  USE_SERIAL.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  server.send(200, "text/json", output);
}

void setColor(RgbColor color, int pixel) {

  for (int i=0; i<pixelCount; i++) {
    strip.SetPixelColor(i, black);
  }
  strip.SetPixelColor(pixel, color);
  strip.Show();
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type) {
        case WStype_DISCONNECTED:
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message to client
            webSocket.sendTXT(num, "Connected");
        }
            break;
        case WStype_TEXT:
           
            USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);

            if(payload[0] == '#') {
                // we get RGB data
                // decode rgb data
                uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);

                // NeoPixels
                for (int i=0; i<pixelCount; i++) {
                  strip.SetPixelColor(i, RgbColor(((rgb >> 16) & 0xFF), ((rgb >> 8) & 0xFF),((rgb >> 0) & 0xFF) ));
                }
                strip.Show();
            }

            if(payload[0] == '*') {
                // we get Pixel number
                uint32_t PixelNumber = (uint32_t) strtol((const char *) &payload[1], NULL, 16);
                // NeoPixels
                for (int i=0; i<pixelCount; i++) {
                  strip.SetPixelColor(i, RgbColor(0x00, 0x00,0x00));
                }
                strip.SetPixelColor(PixelNumber, RgbColor(0xff, 0xff,0xff));
                strip.Show();
            }

            break;
    }

}

void setup() {
    USE_SERIAL.begin(115200);
    USE_SERIAL.println();
    USE_SERIAL.println();
    USE_SERIAL.println();

    for(uint8_t t = 4; t > 0; t--) {
        USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
        USE_SERIAL.flush();
        delay(1000);
    }

  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      USE_SERIAL.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    USE_SERIAL.printf("\n");
  }

  WiFiMulti.addAP(ssid, password);
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
  }

  USE_SERIAL.println("");
  USE_SERIAL.print("Connected! IP address: ");
  USE_SERIAL.println(WiFi.localIP());

  // start webSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Set up mDNS responder:
  if (!MDNS.begin(mDNSid)) {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }
    Serial.println("mDNS responder started");

  USE_SERIAL.print("Open http://");
  USE_SERIAL.print(mDNSid);
  USE_SERIAL.println(".local/edit to see the file browser");

  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.begin();
  USE_SERIAL.println("HTTP server started");

  // Add service to MDNS
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);

  // Initialize NeoPixel Strip
  strip.Begin();
  strip.Show();
}

void loop() {
    webSocket.loop();
    server.handleClient();
}

