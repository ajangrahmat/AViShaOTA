// AViShaOTA.cpp - ESP32 & ESP8266 Support with Fixed Password Validation
#include "AViShaOTA.h"

// Platform-specific includes for storage
#if defined(ESP32)
    #include <Preferences.h>
#elif defined(ESP8266)
    #include <EEPROM.h>
    #include <FS.h>
#endif

// Static instance pointer
AViShaOTA* AViShaOTA::instance = nullptr;

// Constructor
AViShaOTA::AViShaOTA(const String& hostname, int port) {
  this->hostname = hostname;
  this->serverPort = port;
  this->server = nullptr;
  this->otaPassword = "";
  this->mdnsEnabled = true;
  this->serialDebug = true;
  this->autoReconnect = true;
  this->isInitialized = false;
  this->otaInProgress = false;
  this->webUpdateInProgress = false;
  this->lastWiFiCheck = 0;
  this->wifiCheckInterval = 10000;
  
  // Initialize callbacks to nullptr
  this->onStartCallback = nullptr;
  this->onEndCallback = nullptr;
  this->onProgressCallback = nullptr;
  this->onErrorCallback = nullptr;
  this->onWiFiConnectedCallback = nullptr;
  this->onWiFiDisconnectedCallback = nullptr;
  this->onWebUpdateStartCallback = nullptr;
  this->onWebUpdateEndCallback = nullptr;

  // Set static instance
  instance = this;
  
  // Initialize defaults
  initializeDefaults();
}

// Destructor
AViShaOTA::~AViShaOTA() {
  cleanup();
}

// Initialize defaults
void AViShaOTA::initializeDefaults() {
  currentConfig = Config();
  currentConfig.hostname = hostname;
  currentConfig.serverPort = serverPort;
  lastError = "";
  startTime = millis();
}

// Cleanup resources
void AViShaOTA::cleanup() {
  end();
  if (server) {
    delete server;
    server = nullptr;
  }
  instance = nullptr;
}

// Configuration methods
void AViShaOTA::setOTAPassword(const String& password) {
  this->otaPassword = password;
  this->currentConfig.otaPassword = password;
}

void AViShaOTA::setHostname(const String& name) {
  this->hostname = name;
  this->currentConfig.hostname = name;
}

void AViShaOTA::setPort(int port) {
  this->serverPort = port;
  this->currentConfig.serverPort = port;
}

void AViShaOTA::enableMDNS(bool enable) {
  this->mdnsEnabled = enable;
  this->currentConfig.mdnsEnabled = enable;
}

void AViShaOTA::enableSerialDebug(bool enable) {
  this->serialDebug = enable;
  this->currentConfig.serialDebug = enable;
}

void AViShaOTA::enableAutoReconnect(bool enable) {
  this->autoReconnect = enable;
  this->currentConfig.autoReconnect = enable;
}

void AViShaOTA::setWiFiCheckInterval(unsigned long interval) {
  this->wifiCheckInterval = interval;
}

// Callback setters
void AViShaOTA::onStart(void (*callback)()) {
  this->onStartCallback = callback;
}

void AViShaOTA::onEnd(void (*callback)()) {
  this->onEndCallback = callback;
}

void AViShaOTA::onProgress(void (*callback)(unsigned int progress, unsigned int total)) {
  this->onProgressCallback = callback;
}

void AViShaOTA::onError(void (*callback)(ota_error_t error)) {
  this->onErrorCallback = callback;
}

void AViShaOTA::onWiFiConnected(void (*callback)()) {
  this->onWiFiConnectedCallback = callback;
}

void AViShaOTA::onWiFiDisconnected(void (*callback)()) {
  this->onWiFiDisconnectedCallback = callback;
}

void AViShaOTA::onWebUpdateStart(void (*callback)()) {
  this->onWebUpdateStartCallback = callback;
}

void AViShaOTA::onWebUpdateEnd(void (*callback)(bool success)) {
  this->onWebUpdateEndCallback = callback;
}

// End method
void AViShaOTA::end() {
  if (server) {
    server->stop();
  }
  
  #if defined(ESP32)
    if (mdnsEnabled) {
      MDNS.end();
    }
    // Remove WiFi event handler
    WiFi.removeEvent(wifiEventId);
  #elif defined(ESP8266)
    if (mdnsEnabled) {
      MDNS.end();
    }
    // Event handlers will be automatically removed
  #endif
  
  isInitialized = false;
  otaInProgress = false;
  webUpdateInProgress = false;
}

// Status methods
bool AViShaOTA::isOTAInProgress() {
  return otaInProgress;
}

bool AViShaOTA::isWebUpdateInProgress() {
  return webUpdateInProgress;
}

bool AViShaOTA::getInitializationStatus() {
  return isInitialized;
}

String AViShaOTA::getHostname() {
  return hostname;
}

int AViShaOTA::getPort() {
  return serverPort;
}

bool AViShaOTA::isMDNSEnabled() {
  return mdnsEnabled;
}

bool AViShaOTA::isSerialDebugEnabled() {
  return serialDebug;
}

String AViShaOTA::getLastError() {
  return lastError;
}

String AViShaOTA::getPlatform() {
  return AVISHA_PLATFORM;
}

// Static methods
const char* AViShaOTA::getVersion() {
  return AVISHA_OTA_VERSION;
}

String AViShaOTA::getLibraryInfo() {
  return String("AViShaOTA v") + AVISHA_OTA_VERSION + " for " + AVISHA_PLATFORM;
}

// Main begin method
bool AViShaOTA::begin(const char* ssid, const char* password) {
  if (isInitialized) {
    if (serialDebug) {
      Serial.println("AViShaOTA already initialized!");
    }
    return true;
  }

  if (!ssid || strlen(ssid) == 0) {
    lastError = "SSID cannot be empty";
    if (serialDebug) {
      Serial.println("Error: SSID cannot be empty!");
    }
    return false;
  }

  if (serialDebug) {
    Serial.println("Starting AViShaOTA...");
    Serial.println("Platform: " + getPlatform());
  }

  // Create server instance
  if (server) {
    delete server;
  }
  server = new AVISHA_WEBSERVER(serverPort);

  // Setup WiFi - platform specific
  if (!setupWiFi(ssid, password)) {
    lastError = "Failed to connect to WiFi";
    return false;
  }

  // Setup OTA and Web Server
  setupArduinoOTA();
  setupWebServer();

  // Start MDNS if enabled
  if (!setupMDNS()) {
    logMessage("Warning: MDNS setup failed");
  }

  // Start services
  ArduinoOTA.begin();
  server->begin();

  if (serialDebug) {
    Serial.println("AViShaOTA started successfully!");
    Serial.print("Upload URL: ");
    Serial.println(getUploadURL());
  }

  isInitialized = true;
  return true;
}

// Begin AP mode
bool AViShaOTA::beginAP(const char* ssid, const char* password) {
  if (isInitialized) {
    if (serialDebug) {
      Serial.println("AViShaOTA already initialized!");
    }
    return true;
  }

  if (!ssid || strlen(ssid) == 0) {
    lastError = "AP SSID cannot be empty";
    if (serialDebug) {
      Serial.println("Error: AP SSID cannot be empty!");
    }
    return false;
  }

  if (serialDebug) {
    Serial.println("Starting AViShaOTA in AP mode...");
  }

  // Create server instance
  if (server) {
    delete server;
  }
  server = new AVISHA_WEBSERVER(serverPort);

  // Setup WiFi AP mode
  #if defined(ESP32)
    WiFi.mode(WIFI_AP);
    bool apResult = WiFi.softAP(ssid, password);
  #elif defined(ESP8266)
    WiFi.mode(WIFI_AP);
    bool apResult = WiFi.softAP(ssid, password);
  #endif

  if (!apResult) {
    lastError = "Failed to start AP mode";
    if (serialDebug) {
      Serial.println("Error: Failed to start AP mode!");
    }
    return false;
  }

  if (serialDebug) {
    Serial.println("AP Mode started!");
    Serial.print("AP IP Address: ");
    #if defined(ESP32)
      Serial.println(WiFi.softAPIP());
    #elif defined(ESP8266)
      Serial.println(WiFi.softAPIP());
    #endif
  }

  // Setup services
  setupArduinoOTA();
  setupWebServer();
  setupMDNS();

  ArduinoOTA.begin();
  server->begin();

  isInitialized = true;
  return true;
}

// Handle method - call this in loop()
void AViShaOTA::handle() {
  if (!isInitialized) {
    return;
  }

  ArduinoOTA.handle();
  if (server) {
    server->handleClient();
  }

  // Check WiFi connection periodically if auto-reconnect is enabled
  if (autoReconnect && millis() - lastWiFiCheck > wifiCheckInterval) {
    checkWiFiConnection();
    lastWiFiCheck = millis();
  }

  #if defined(ESP8266)
    // For ESP8266, we need to call MDNS.update() in the loop
    if (mdnsEnabled) {
      MDNS.update();
    }
  #endif
}

// Setup WiFi - platform specific
bool AViShaOTA::setupWiFi(const char* ssid, const char* password) {
  #if defined(ESP32)
    WiFi.mode(WIFI_STA);
    wifiEventId = WiFi.onEvent(wifiEventHandler);
    WiFi.setAutoReconnect(autoReconnect);
    WiFi.persistent(true);
  #elif defined(ESP8266)
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(autoReconnect);
    WiFi.persistent(true);
    
    // Setup event handlers for ESP8266
    wifiConnectHandler = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP& event) {
      if (serialDebug) {
        Serial.println("WiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Upload URL: ");
        Serial.println(getUploadURL());
      }
      if (onWiFiConnectedCallback) {
        onWiFiConnectedCallback();
      }
    });
    
    wifiDisconnectHandler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& event) {
      if (serialDebug) {
        Serial.println("WiFi disconnected, attempting to reconnect...");
      }
      if (onWiFiDisconnectedCallback) {
        onWiFiDisconnectedCallback();
      }
    });
  #endif

  WiFi.begin(ssid, password);

  if (serialDebug) {
    Serial.print("Connecting to WiFi");
  }

  // Wait for connection with timeout
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < currentConfig.wifiTimeout) {
    delay(500);
    if (serialDebug) {
      Serial.print(".");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (serialDebug) {
      Serial.println("\nFailed to connect to WiFi!");
    }
    return false;
  }

  if (serialDebug) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  return true;
}

// Setup MDNS
bool AViShaOTA::setupMDNS() {
  if (!mdnsEnabled) {
    return true;
  }

  if (MDNS.begin(hostname.c_str())) {
    if (serialDebug) {
      Serial.println("MDNS responder started");
    }
    return true;
  } else {
    if (serialDebug) {
      Serial.println("Error starting MDNS responder!");
    }
    return false;
  }
}

// Check WiFi connection
void AViShaOTA::checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED && autoReconnect) {
    if (serialDebug) {
      Serial.println("WiFi connection lost, reconnecting...");
    }
    WiFi.reconnect();
  }
}

// Setup ArduinoOTA
void AViShaOTA::setupArduinoOTA() {
  ArduinoOTA.setHostname(hostname.c_str());
  
  if (otaPassword.length() > 0) {
    ArduinoOTA.setPassword(otaPassword.c_str());
  }

  ArduinoOTA.onStart([this]() {
    otaInProgress = true;
    if (serialDebug) {
      Serial.println("OTA Update started...");
    }
    if (onStartCallback) {
      onStartCallback();
    }
  });

  ArduinoOTA.onEnd([this]() {
    otaInProgress = false;
    if (serialDebug) {
      Serial.println("\nOTA Update completed!");
    }
    if (onEndCallback) {
      onEndCallback();
    }
  });

  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    if (serialDebug) {
      Serial.printf("OTA Progress: %u%%\r", (progress * 100) / total);
    }
    if (onProgressCallback) {
      onProgressCallback(progress, total);
    }
  });

  ArduinoOTA.onError([this](ota_error_t error) {
    otaInProgress = false;
    if (serialDebug) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    }
    if (onErrorCallback) {
      onErrorCallback(error);
    }
  });
}

// Setup Web Server
void AViShaOTA::setupWebServer() {
  server->on("/", HTTP_GET, [this]() {
    handleRoot();
  });

  server->on("/update", HTTP_POST, [this]() {
    handleUpdateFinish();
  }, [this]() {
    handleUpdate();
  });

  server->on("/info", HTTP_GET, [this]() {
    handleInfo();
  });

  server->on("/restart", HTTP_POST, [this]() {
    handleRestart();
  });

  server->onNotFound([this]() {
    handleNotFound();
  });
}

// Handle root request
void AViShaOTA::handleRoot() {
  server->send(200, "text/html", getUploadHTML());
}

// Handle info request
void AViShaOTA::handleInfo() {
  server->send(200, "application/json", getSystemInfo());
}

// Handle restart request
void AViShaOTA::handleRestart() {
  server->send(200, "text/plain", "Restarting device...");
  delay(1000);
  restart();
}

// Handle not found
void AViShaOTA::handleNotFound() {
  server->send(404, "text/plain", "Not Found");
}

// Handle update finish
void AViShaOTA::handleUpdateFinish() {
  webUpdateInProgress = false;
  
  #if defined(ESP32)
    if (Update.hasError()) {
      if (serialDebug) {
        Serial.println("Web Update failed!");
        Update.printError(Serial);
      }
      server->send(500, "text/plain", "Update failed");
      if (onWebUpdateEndCallback) {
        onWebUpdateEndCallback(false);
      }
    } else {
      if (serialDebug) {
        Serial.println("Web Update successful!");
      }
      server->send(200, "text/plain", "Update successful! Device will restart...");
      if (onWebUpdateEndCallback) {
        onWebUpdateEndCallback(true);
      }
      delay(1000);
      restart();
    }
  #elif defined(ESP8266)
    if (Update.hasError()) {
      if (serialDebug) {
        Serial.println("Web Update failed!");
        Update.printError(Serial);
      }
      server->send(500, "text/plain", "Update failed");
      if (onWebUpdateEndCallback) {
        onWebUpdateEndCallback(false);
      }
    } else {
      if (serialDebug) {
        Serial.println("Web Update successful!");
      }
      server->send(200, "text/plain", "Update successful! Device will restart...");
      if (onWebUpdateEndCallback) {
        onWebUpdateEndCallback(true);
      }
      delay(1000);
      restart();
    }
  #endif
}

// Handle update request with proper password validation
void AViShaOTA::handleUpdate() {
  HTTPUpload& upload = server->upload();
  static bool passwordChecked = false;
  static bool passwordValid = false;

  if (upload.status == UPLOAD_FILE_START) {
    passwordChecked = false;
    passwordValid = false;
    
    // Check password if set
    if (otaPassword.length() > 0) {
      String receivedPassword = "";
      
      if (server->hasArg("password")) {
        receivedPassword = server->arg("password");
      }
      
      if (receivedPassword.length() == 0) {
        for (int i = 0; i < server->args(); i++) {
          if (server->argName(i) == "password") {
            receivedPassword = server->arg(i);
            break;
          }
        }
      }
      
      if (serialDebug) {
        Serial.println("Checking OTA password...");
      }
      
      if (receivedPassword != otaPassword) {
        if (serialDebug) {
          Serial.println("OTA: Password mismatch - access denied");
        }
        server->send(401, "text/plain", "Unauthorized: Invalid password");
        return;
      }
      
      passwordValid = true;
    } else {
      passwordValid = true;
    }
    
    passwordChecked = true;
    webUpdateInProgress = true;
    
    if (serialDebug) {
      Serial.printf("Web Update Start: %s\n", upload.filename.c_str());
    }
    
    if (onWebUpdateStartCallback) {
      onWebUpdateStartCallback();
    }

    #if defined(ESP32)
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        if (serialDebug) {
          Serial.println("Update.begin() failed:");
          Update.printError(Serial);
        }
        webUpdateInProgress = false;
        return;
      }
    #elif defined(ESP8266)
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) {
        if (serialDebug) {
          Serial.println("Update.begin() failed:");
          Update.printError(Serial);
        }
        webUpdateInProgress = false;
        return;
      }
    #endif
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!passwordChecked || !passwordValid) {
      return;
    }
    
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      if (serialDebug) {
        Serial.println("Update.write() failed:");
        Update.printError(Serial);
      }
      webUpdateInProgress = false;
      return;
    }

    if (serialDebug) {
      Serial.printf("Web Update Progress: %d%%\r", (Update.progress() * 100) / Update.size());
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (!passwordChecked || !passwordValid) {
      return;
    }
    
    if (Update.end(true)) {
      if (serialDebug) {
        Serial.printf("\nWeb Update Success: %u bytes\n", upload.totalSize);
      }
    } else {
      if (serialDebug) {
        Serial.println("Update.end() failed:");
        Update.printError(Serial);
      }
      webUpdateInProgress = false;
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    webUpdateInProgress = false;
    if (serialDebug) {
      Serial.println("Web Update was aborted");
    }
  }
}

// Platform-specific WiFi event handlers
#if defined(ESP32)
void AViShaOTA::wifiEventHandler(WiFiEvent_t event) {
  if (instance) {
    instance->handleWiFiEvent(event);
  }
}

void AViShaOTA::handleWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_STA_DISCONNECTED:
      if (serialDebug) {
        Serial.println("WiFi disconnected, attempting to reconnect...");
      }
      if (onWiFiDisconnectedCallback) {
        onWiFiDisconnectedCallback();
      }
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      if (serialDebug) {
        Serial.println("WiFi connected!");
      }
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      if (serialDebug) {
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("Upload URL: ");
        Serial.println(getUploadURL());
      }
      if (onWiFiConnectedCallback) {
        onWiFiConnectedCallback();
      }
      break;
    default:
      break;
  }
}
#endif

// Utility methods
String AViShaOTA::getLocalIP() {
  #if defined(ESP32)
    if (WiFi.getMode() == WIFI_AP) {
      return WiFi.softAPIP().toString();
    } else {
      return WiFi.localIP().toString();
    }
  #elif defined(ESP8266)
    if (WiFi.getMode() == WIFI_AP) {
      return WiFi.softAPIP().toString();
    } else {
      return WiFi.localIP().toString();
    }
  #endif
}

String AViShaOTA::getUploadURL() {
  return "http://" + getLocalIP() + ":" + String(serverPort) + "/";
}

String AViShaOTA::getInfoURL() {
  return "http://" + getLocalIP() + ":" + String(serverPort) + "/info";
}

bool AViShaOTA::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

#if defined(ESP32)
WiFiMode_t AViShaOTA::getWiFiMode() {
  return WiFi.getMode();
}
#elif defined(ESP8266)
WiFiMode AViShaOTA::getWiFiMode() {
  return WiFi.getMode();
}
#endif

// System utility methods
void AViShaOTA::restart() {
  #if defined(ESP32)
    ESP.restart();
  #elif defined(ESP8266)
    ESP.restart();
  #endif
}

void AViShaOTA::factoryReset() {
  #if defined(ESP32)
    // Clear preferences and restart
  #elif defined(ESP8266)
    // Clear EEPROM and restart
    EEPROM.begin(512);
    for (int i = 0; i < 512; i++) {
      EEPROM.write(i, 0);
    }
    EEPROM.commit();
    EEPROM.end();
  #endif
  delay(1000);
  restart();
}

String AViShaOTA::getChipID() {
  #if defined(ESP32)
    return String((uint32_t)ESP.getEfuseMac(), HEX);
  #elif defined(ESP8266)
    return String(ESP.getChipId(), HEX);
  #endif
}

String AViShaOTA::getMACAddress() {
  return WiFi.macAddress();
}

uint32_t AViShaOTA::getFreeHeap() {
  return ESP.getFreeHeap();
}

uint32_t AViShaOTA::getFlashChipSize() {
  #if defined(ESP32)
    return ESP.getFlashChipSize();
  #elif defined(ESP8266)
    return ESP.getFlashChipRealSize();
  #endif
}

String AViShaOTA::getSketchMD5() {
  return ESP.getSketchMD5();
}

// Platform-specific system info methods
#if defined(ESP32)
String AViShaOTA::getCPUFreqMHz() {
  return String(ESP.getCpuFreqMHz());
}

uint32_t AViShaOTA::getFlashChipSpeed() {
  return ESP.getFlashChipSpeed();
}

String AViShaOTA::getSDKVersion() {
  return ESP.getSdkVersion();
}
#elif defined(ESP8266)
String AViShaOTA::getCPUFreqMHz() {
  return String(ESP.getCpuFreqMHz());
}

uint32_t AViShaOTA::getFlashChipSpeed() {
  return ESP.getFlashChipSpeed();
}

String AViShaOTA::getCoreVersion() {
  return ESP.getCoreVersion();
}

String AViShaOTA::getBootVersion() {
  return String(ESP.getBootVersion());
}
#endif

// Configuration methods
void AViShaOTA::setConfig(const Config& config) {
  currentConfig = config;
  hostname = config.hostname;
  otaPassword = config.otaPassword;
  serverPort = config.serverPort;
  mdnsEnabled = config.mdnsEnabled;
  serialDebug = config.serialDebug;
  autoReconnect = config.autoReconnect;
}

AViShaOTA::Config AViShaOTA::getConfig() {
  return currentConfig;
}

// Logging methods
void AViShaOTA::logMessage(const String& message) {
  if (serialDebug) {
    Serial.println("[AViShaOTA] " + message);
  }
}

void AViShaOTA::logError(const String& error) {
  lastError = error;
  if (serialDebug) {
    Serial.println("[AViShaOTA ERROR] " + error);
  }
}

// Get system info as JSON
String AViShaOTA::getSystemInfo() {
  String json = "{";
  json += "\"platform\":\"" + getPlatform() + "\",";
  json += "\"version\":\"" + String(getVersion()) + "\",";
  json += "\"chipId\":\"" + getChipID() + "\",";
  json += "\"macAddress\":\"" + getMACAddress() + "\",";
  json += "\"freeHeap\":" + String(getFreeHeap()) + ",";
  json += "\"flashSize\":" + String(getFlashChipSize()) + ",";
  json += "\"cpuFreq\":\"" + getCPUFreqMHz() + "MHz\",";
  json += "\"sketchMD5\":\"" + getSketchMD5() + "\",";
  
  #if defined(ESP32)
    json += "\"sdkVersion\":\"" + getSDKVersion() + "\",";
  #elif defined(ESP8266)
    json += "\"coreVersion\":\"" + getCoreVersion() + "\",";
    json += "\"bootVersion\":\"" + getBootVersion() + "\",";
  #endif
  
  json += "\"wifiStatus\":\"" + String(WiFi.status()) + "\",";
  json += "\"localIP\":\"" + getLocalIP() + "\",";
  json += "\"hostname\":\"" + getHostname() + "\"";
  json += "}";
  
  return json;
}

// FIXED: Updated HTML with better password handling
const char* AViShaOTA::getUploadHTML() {
  static const char* uploadHTML = R"(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>AViSha OTA Update</title>
  <style>
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Oxygen,
    Ubuntu, Cantarell, "Open Sans", "Helvetica Neue", sans-serif;
  margin: 0;
  padding: 0;
  background: #f2f2f7;
}

.container {
  max-width: 400px;
  margin: 80px auto;
  background: #fff;
  border-radius: 20px;
  box-shadow: 0 8px 20px rgba(0, 0, 0, 0.08);
  padding: 30px;
  text-align: center;
}

h1 {
  font-size: 24px;
  margin-bottom: 10px;
  color: #111;
}

p {
  color: #555;
  font-size: 14px;
  margin-bottom: 20px;
}

.upload-area {
  border: 2px dashed #1abc9c; /* tosca border */
  border-radius: 12px;
  padding: 30px 10px;
  background-color: #fafafa;
  transition: background 0.3s ease;
}

.upload-area:hover {
  background: #e0f7f4; /* lebih soft tosca */
}

input[type="password"], input[type="file"] {
  margin-top: 15px;
  padding: 10px;
  border: 1px solid #1abc9c;
  border-radius: 8px;
  font-size: 14px;
  width: 80%;
  max-width: 250px;
}

input[type="file"] {
  cursor: pointer;
  padding: 8px;
}

button {
  margin-top: 20px;
  background-color: #1abc9c; /* tosca utama */
  color: white;
  border: none;
  padding: 12px 24px;
  font-size: 16px;
  border-radius: 12px;
  cursor: pointer;
  transition: background 0.3s ease;
  min-width: 150px;
}

button:hover:not(:disabled) {
  background-color: #159c88; /* tosca lebih gelap */
}

button:disabled {
  background-color: #8e8e93;
  cursor: not-allowed;
}

.progress {
  width: 100%;
  background-color: #e5e5ea;
  border-radius: 12px;
  margin-top: 25px;
  height: 12px;
  overflow: hidden;
  display: none;
}

.progress-bar {
  height: 100%;
  width: 0%;
  background-color: #1abc9c; /* progress bar tosca */
  transition: width 0.3s ease;
}

#status {
  margin-top: 25px;
  font-size: 14px;
  color: #333;
}

.status-success {
  color: #1abc9c; /* hijau diganti tosca */
}

.status-error {
  color: #ff3b30;
}

.file-info {
  margin-top: 10px;
  font-size: 12px;
  color: #666;
}

  </style>
</head>
<body>
  <div class="container">
    <h1>AViSha OTA Update</h1>
    <p>Select a .bin file to update your device's firmware.</p>

    <div class="upload-area">
      <form id="uploadForm" enctype="multipart/form-data">
        <input type="password" name="password" id="passwordInput" placeholder="Enter OTA Password" />
        <br />
        <input type="file" name="update" id="fileInput" accept=".bin" required />
        <div class="file-info" id="fileInfo"></div>
        <br />
        <button type="submit" id="uploadBtn">Upload and Update</button>
      </form>
    </div>

    <div class="progress" id="progressContainer">
      <div class="progress-bar" id="progressBar"></div>
    </div>

    <div id="status"></div>
  </div>

  <script>
    const uploadForm = document.getElementById("uploadForm");
    const fileInput = document.getElementById("fileInput");
    const passwordInput = document.getElementById("passwordInput");
    const progressContainer = document.getElementById("progressContainer");
    const progressBar = document.getElementById("progressBar");
    const status = document.getElementById("status");
    const uploadBtn = document.getElementById("uploadBtn");
    const fileInfo = document.getElementById("fileInfo");

    fileInput.addEventListener("change", function() {
      const file = this.files[0];
      if (file) {
        const sizeMB = (file.size / (1024 * 1024)).toFixed(2);
        fileInfo.textContent = `File: ${file.name} (${sizeMB} MB)`;
      } else {
        fileInfo.textContent = "";
      }
    });

    uploadForm.addEventListener("submit", function (e) {
      e.preventDefault();

      const file = fileInput.files[0];
      const password = passwordInput.value.trim();

      if (!file || !file.name.endsWith(".bin")) {
        alert("Please select a valid .bin file.");
        return;
      }

      // Create FormData and append fields in correct order
      const formData = new FormData();
      
      // Add password first (if provided)
      if (password) {
        formData.append("password", password);
      }
      
      // Then add the file
      formData.append("update", file);

      const xhr = new XMLHttpRequest();
      
      // Disable upload button
      uploadBtn.disabled = true;
      uploadBtn.textContent = "Uploading...";
      progressContainer.style.display = "block";
      status.innerHTML = "";

      xhr.upload.addEventListener("progress", function (e) {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          progressBar.style.width = percent + "%";
          status.innerHTML = `<p>Uploading: ${percent}%</p>`;
        }
      });

      xhr.addEventListener("load", function () {
        uploadBtn.disabled = false;
        uploadBtn.textContent = "Upload and Update";
        
        if (xhr.status === 200) {
          progressBar.style.width = "100%";
          status.innerHTML = `<p class="status-success">Update successful! Restarting device...</p>`;
          setTimeout(() => {
            status.innerHTML = `<p class="status-success">Restarting... Page will reload.</p>`;
            setTimeout(() => location.reload(), 5000);
          }, 2000);
        } else if (xhr.status === 401) {
          progressContainer.style.display = "none";
          status.innerHTML = `<p class="status-error">Incorrect password! Please check and try again.</p>`;
          passwordInput.focus();
        } else {
          progressContainer.style.display = "none";
          status.innerHTML = `<p class="status-error">Update failed: ${xhr.responseText}</p>`;
        }
      });

      xhr.addEventListener("error", function () {
        uploadBtn.disabled = false;
        uploadBtn.textContent = "Upload and Update";
        progressContainer.style.display = "none";
        status.innerHTML = `<p class="status-error">Network error occurred while uploading.</p>`;
      });

      xhr.addEventListener("timeout", function () {
        uploadBtn.disabled = false;
        uploadBtn.textContent = "Upload and Update";
        progressContainer.style.display = "none";
        status.innerHTML = `<p class="status-error">Upload timeout - please try again.</p>`;
      });

      xhr.timeout = 300000; // 5 minutes timeout
      xhr.open("POST", "/update");
      xhr.send(formData);
    });

    // Focus password field on load if needed
    window.addEventListener('load', function() {
      if (passwordInput.placeholder) {
        passwordInput.focus();
      }
    });
  </script>
</body>
</html>
)";
  return uploadHTML;
}
