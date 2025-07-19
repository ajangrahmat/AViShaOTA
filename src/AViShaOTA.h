#ifndef AVISHA_OTA_H
#define AVISHA_OTA_H

// Platform detection and includes
#if defined(ESP32)
    #include <Arduino.h>
    #include <WiFi.h>
    #include <ArduinoOTA.h>
    #include <WebServer.h>
    #include <ESPmDNS.h>
    #include <Update.h>
    #include <WiFiClient.h>
    #include <esp_wifi.h>
    #define AVISHA_PLATFORM "ESP32"
    #define AVISHA_WEBSERVER WebServer
    #define AVISHA_WIFI_EVENT WiFiEvent_t
#elif defined(ESP8266)
    #include <Arduino.h>
    #include <ESP8266WiFi.h>
    #include <ArduinoOTA.h>
    #include <ESP8266WebServer.h>
    #include <ESP8266mDNS.h>
    #include <Updater.h>
    #include <WiFiClient.h>
    #define AVISHA_PLATFORM "ESP8266"
    #define AVISHA_WEBSERVER ESP8266WebServer
    #define AVISHA_WIFI_EVENT WiFiEventStationModeGotIP
#else
    #error "Platform not supported. This library supports ESP32 and ESP8266 only."
#endif

// Version information
#define AVISHA_OTA_VERSION "1.2.1"

// Default configuration
#define AVISHA_OTA_DEFAULT_PORT 80
#define AVISHA_OTA_DEFAULT_HOSTNAME "AViShaOTA_Device"
#define AVISHA_OTA_WIFI_TIMEOUT 30000
#define AVISHA_OTA_UPLOAD_TIMEOUT 300000

class AViShaOTA {
private:
    // Core components
    AVISHA_WEBSERVER* server;
    String hostname;
    String otaPassword;
    int serverPort;
    
    // Feature flags
    bool mdnsEnabled;
    bool serialDebug;
    bool autoReconnect;
    
    // Status tracking
    bool isInitialized;
    bool otaInProgress;
    bool webUpdateInProgress;
    
    // Connection tracking
    unsigned long lastWiFiCheck;
    unsigned long wifiCheckInterval;
    
    // Callback function pointers
    void (*onStartCallback)();
    void (*onEndCallback)();
    void (*onProgressCallback)(unsigned int progress, unsigned int total);
    #if defined(ESP32)
        void (*onErrorCallback)(ota_error_t error);
    #elif defined(ESP8266)
        void (*onErrorCallback)(ota_error_t error);
    #endif
    void (*onWiFiConnectedCallback)();
    void (*onWiFiDisconnectedCallback)();
    void (*onWebUpdateStartCallback)();
    void (*onWebUpdateEndCallback)(bool success);
    
    // Platform-specific event handlers
    #if defined(ESP32)
        WiFiEventId_t wifiEventId;
    #elif defined(ESP8266)
        WiFiEventHandler wifiConnectHandler;
        WiFiEventHandler wifiDisconnectHandler;
    #endif
    
    // Internal setup methods
    void setupWebServer();
    void setupArduinoOTA();
    bool setupWiFi(const char* ssid, const char* password);
    bool setupMDNS();
    
    // HTTP request handlers
    void handleRoot();
    void handleUpdate();
    void handleUpdateFinish();
    void handleNotFound();
    void handleInfo();
    void handleRestart();
    
    // WiFi event handling (platform-specific implementations)
    #if defined(ESP32)
        static void wifiEventHandler(WiFiEvent_t event);
        void handleWiFiEvent(WiFiEvent_t event);
    #elif defined(ESP8266)
        void onWiFiConnected(const WiFiEventStationModeGotIP& event);
        void onWiFiDisconnected(const WiFiEventStationModeDisconnected& event);
    #endif
    void checkWiFiConnection();
    
    // Utility methods
    bool validatePassword(const String& password);
    bool validateBinaryFile(const String& filename);
    void logMessage(const String& message);
    void logError(const String& error);
    
    // Static instance for event handling
    static AViShaOTA* instance;
    
    // HTML templates
    const char* getUploadHTML();
    const char* getInfoHTML();
    String getSystemInfo();

public:
    // Constructors
    AViShaOTA(const String& hostname = AVISHA_OTA_DEFAULT_HOSTNAME, 
              int port = AVISHA_OTA_DEFAULT_PORT);
    
    // Destructor
    ~AViShaOTA();
    
    // Prevent copy constructor and assignment operator
    AViShaOTA(const AViShaOTA&) = delete;
    AViShaOTA& operator=(const AViShaOTA&) = delete;
    
    // Configuration methods
    void setOTAPassword(const String& password);
    void setHostname(const String& name);
    void setPort(int port);
    void enableMDNS(bool enable = true);
    void enableSerialDebug(bool enable = true);
    void enableAutoReconnect(bool enable = true);
    void setWiFiCheckInterval(unsigned long interval = 10000);
    
    // Callback registration methods
    void onStart(void (*callback)());
    void onEnd(void (*callback)());
    void onProgress(void (*callback)(unsigned int progress, unsigned int total));
    #if defined(ESP32)
        void onError(void (*callback)(ota_error_t error));
    #elif defined(ESP8266)
        void onError(void (*callback)(ota_error_t error));
    #endif
    void onWiFiConnected(void (*callback)());
    void onWiFiDisconnected(void (*callback)());
    void onWebUpdateStart(void (*callback)());
    void onWebUpdateEnd(void (*callback)(bool success));
    
    // Main lifecycle methods
    bool begin(const char* ssid, const char* password = nullptr);
    bool beginAP(const char* ssid, const char* password = nullptr);
    void handle();
    void end();
    
    // Network utility methods
    String getLocalIP();
    String getUploadURL();
    String getInfoURL();
    bool isConnected();
    bool isOTAInProgress();
    bool isWebUpdateInProgress();
    #if defined(ESP32)
        WiFiMode_t getWiFiMode();
    #elif defined(ESP8266)
        WiFiMode getWiFiMode();
    #endif
    
    // System utility methods
    void restart();
    void factoryReset();
    String getChipID();
    String getMACAddress();
    uint32_t getFreeHeap();
    uint32_t getFlashChipSize();
    String getSketchMD5();
    
    // Platform-specific system info
    #if defined(ESP32)
        String getCPUFreqMHz();
        uint32_t getFlashChipSpeed();
        String getSDKVersion();
    #elif defined(ESP8266)
        String getCPUFreqMHz();
        uint32_t getFlashChipSpeed();
        String getCoreVersion();
        String getBootVersion();
    #endif
    
    // Status and information methods
    bool getInitializationStatus();
    String getHostname();
    int getPort();
    bool isMDNSEnabled();
    bool isSerialDebugEnabled();
    String getLastError();
    String getPlatform();
    
    // Static utility methods
    static const char* getVersion();
    static String getLibraryInfo();
    static bool isValidHostname(const String& hostname);
    static bool isValidPassword(const String& password);
    
    // Advanced configuration
    struct Config {
        String hostname;
        String otaPassword;
        int serverPort;
        bool mdnsEnabled;
        bool serialDebug;
        bool autoReconnect;
        unsigned long wifiTimeout;
        unsigned long uploadTimeout;
        
        Config() : 
            hostname(AVISHA_OTA_DEFAULT_HOSTNAME),
            otaPassword(""),
            serverPort(AVISHA_OTA_DEFAULT_PORT),
            mdnsEnabled(true),
            serialDebug(true),
            autoReconnect(true),
            wifiTimeout(AVISHA_OTA_WIFI_TIMEOUT),
            uploadTimeout(AVISHA_OTA_UPLOAD_TIMEOUT) {}
    };
    
    // Advanced configuration methods
    void setConfig(const Config& config);
    Config getConfig();
    #if defined(ESP32)
        bool loadConfig(); // Load from Preferences
        bool saveConfig(); // Save to Preferences
    #elif defined(ESP8266)
        bool loadConfig(); // Load from EEPROM
        bool saveConfig(); // Save to EEPROM
    #endif
    
private:
    // Internal state
    Config currentConfig;
    String lastError;
    unsigned long startTime;
    
    // Internal helper methods
    void initializeDefaults();
    void cleanup();
    bool isValidConfiguration();
    void updateInternalState();
    
    // Platform-specific helper methods
    #if defined(ESP32)
        String getResetReason();
        void initPreferences();
    #elif defined(ESP8266)
        String getResetReason();
        void initEEPROM();
    #endif
};

// Global helper functions
namespace AViShaOTAUtils {
    String formatBytes(size_t bytes);
    String getResetReason();
    String getBootMode();
    bool isValidIPAddress(const String& ip);
    String generateRandomPassword(int length = 8);
    
    // Platform detection utilities
    bool isESP32();
    bool isESP8266();
    String getPlatformName();
    
    // Platform-specific utilities
    #if defined(ESP32)
        String getChipModel();
        uint8_t getChipRevision();
        uint32_t getApbFrequency();
    #elif defined(ESP8266)
        uint32_t getChipId();
        uint32_t getCycleCount();
        String getFlashChipMode();
    #endif
}

// Platform compatibility macros
#if defined(ESP8266)
    #define WiFiMode_t WiFiMode
#endif

#endif // AVISHA_OTA_H
