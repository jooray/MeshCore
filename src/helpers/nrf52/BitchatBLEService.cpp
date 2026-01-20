#include "BitchatBLEService.h"

#ifdef NRF52_PLATFORM

#include <Arduino.h>

// HardFault handler - catches crashes and prints debug info
extern "C" {
  void HardFault_Handler(void) {
    Serial.println("\n\n!!! HARDFAULT DETECTED !!!");
    Serial.println("Crash - likely stack overflow or memory corruption");
    Serial.flush();

    // Blink LED rapidly to indicate crash
    #ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    #endif

    while(1) {
      #ifdef LED_BUILTIN
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      #else
      delay(200);
      #endif
    }
  }
}

// Debug output - Adafruit nRF52 core supports Serial.printf
#if BITCHAT_DEBUG
  #define BITCHAT_DEBUG_PRINTLN(...) do { Serial.printf("BITCHAT_BLE: "); Serial.printf(__VA_ARGS__); Serial.println(); } while(0)
#else
  #define BITCHAT_DEBUG_PRINTLN(...) {}
#endif

// Bitchat service UUID: F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C
// Bluefruit uses little-endian byte order for UUIDs
static const uint8_t BITCHAT_SERVICE_UUID_BYTES[] = {
    0x5C, 0x4B, 0x3A, 0x2C, 0x1D, 0x8E, 0x3F, 0x9B,
    0x5A, 0x4C, 0x9E, 0x4A, 0x2D, 0x5E, 0x7B, 0xF4
};

// Bitchat characteristic UUID: A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D
// (Same as ESP32 BITCHAT_CHARACTERISTIC_UUID in BitchatProtocol.h)
// Bluefruit uses little-endian byte order for UUIDs
static const uint8_t BITCHAT_CHARACTERISTIC_UUID_BYTES[] = {
    0x5D, 0x4C, 0x3B, 0x2A, 0x1F, 0x0E, 0x9D, 0x8C,
    0x5B, 0x4A, 0xF6, 0xE5, 0xD4, 0xC3, 0xB2, 0xA1
};

// Singleton instance for static callback access
BitchatBLEService* BitchatBLEService::_instance = nullptr;

BitchatBLEService::BitchatBLEService()
    : _service(BITCHAT_SERVICE_UUID_BYTES)
    , _characteristic(BITCHAT_CHARACTERISTIC_UUID_BYTES)
    , _callback(nullptr)
    , _serviceActive(false)
    , _bitchatClientCount(0)
    , _clientSubscribed(false)
    , _pendingConnect(false)
    , _pendingData(false)
    , _writeBufferOffset(0)
    , _lastWriteTime(0)
    , _queueHead(0)
    , _queueTail(0)
    , _hasPendingOutgoing(false)
{
    memset(_writeBuffer, 0, sizeof(_writeBuffer));
    memset(_deviceName, 0, sizeof(_deviceName));
    strcpy(_deviceName, "Bitchat");
    for (size_t i = 0; i < MESSAGE_QUEUE_SIZE; i++) {
        _messageQueue[i].valid = false;
    }
    memset(&_pendingOutgoing, 0, sizeof(_pendingOutgoing));
    _instance = this;
}

bool BitchatBLEService::beginStandalone(const char* deviceName, BitchatBLECallback* callback) {
    Serial.println("NRF52_BLE: beginStandalone entry");
    Serial.flush();

    if (callback == nullptr) {
        Serial.println("NRF52_BLE: ERROR - callback is nullptr");
        return false;
    }

    _callback = callback;
    strncpy(_deviceName, deviceName, sizeof(_deviceName) - 1);
    _deviceName[sizeof(_deviceName) - 1] = '\0';

    Serial.println("NRF52_BLE: Configuring connection params");
    Serial.flush();

    // Configure connection parameters BEFORE begin()
    // MTU 517 is standard max BLE MTU - allows writes up to 514 bytes
    // setMaxLen(512) is the characteristic buffer size
    // Parameters: mtu_max, event_len, hvn_qsize, wrcmd_qsize
    Bluefruit.configPrphConn(517, BLE_GAP_EVENT_LENGTH_DEFAULT, BLE_GATTS_HVN_TX_QUEUE_SIZE_DEFAULT, BLE_GATTC_WRITE_CMD_TX_QUEUE_SIZE_DEFAULT);

    Serial.println("NRF52_BLE: Calling Bluefruit.begin()");
    Serial.flush();

    // Initialize Bluefruit
    Bluefruit.begin();

    Serial.println("NRF52_BLE: Bluefruit.begin() returned");
    Serial.flush();

    Bluefruit.setTxPower(8);  // Max power (+8 dBm) for better range

    // Set up connection callbacks
    Bluefruit.Periph.setConnectCallback(onConnect);
    Bluefruit.Periph.setDisconnectCallback(onDisconnect);

    // Bitchat uses open security (no PIN required)
    Bluefruit.Security.setMITM(false);
    Bluefruit.Security.setIOCaps(false, false, false);

    Serial.println("NRF52_BLE: Setting device name");
    Serial.flush();

    // Set device name (filter out non-ASCII characters for BLE)
    char safeName[32];
    size_t j = 0;
    for (size_t i = 0; deviceName[i] != '\0' && j < sizeof(safeName) - 1; i++) {
        if (deviceName[i] >= 0x20 && deviceName[i] <= 0x7E) {
            safeName[j++] = deviceName[i];
        }
    }
    safeName[j] = '\0';
    if (j == 0) strcpy(safeName, "Bitchat");

    Bluefruit.setName(safeName);

    Serial.println("NRF52_BLE: Starting service");
    Serial.flush();

    // Configure the Bitchat service
    _service.begin();

    Serial.println("NRF52_BLE: Configuring characteristic");
    Serial.flush();

    // Configure the characteristic with READ, WRITE, WRITE_NR, NOTIFY properties
    _characteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_NOTIFY | CHR_PROPS_INDICATE);
    _characteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);  // Open security
    // CRITICAL: Must be ≤512 or BLE service discovery breaks on NRF52
    _characteristic.setMaxLen(512);
    _characteristic.setWriteCallback(onCharacteristicWrite);
    _characteristic.setCccdWriteCallback(onCharacteristicCccdWrite);
    _characteristic.begin();

    Serial.println("NRF52_BLE: Service fully initialized");
    Serial.flush();

    _serviceActive = true;
    BITCHAT_DEBUG_PRINTLN("Bitchat BLE service initialized: %s", safeName);

    return true;
}

void BitchatBLEService::startAdvertising() {
    // Clear any previous advertising data
    Bluefruit.Advertising.clearData();
    Bluefruit.ScanResponse.clearData();

    // Set Bitchat UUID in MAIN advertisement (required for Bitchat app discovery)
    // The Bitchat Android app filters on service UUID in main advertisement packet
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addService(_service);

    // Put device name in scan response (not main adv - no room with 128-bit UUID)
    Bluefruit.ScanResponse.addName();

    // Configure advertising parameters
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);    // in units of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);      // seconds in fast mode
    Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising

    BITCHAT_DEBUG_PRINTLN("BLE advertising started");
}

void BitchatBLEService::onServerDisconnect() {
    if (_bitchatClientCount > 0) {
        _bitchatClientCount--;
    }

    if (_bitchatClientCount == 0) {
        _clientSubscribed = false;
        clearWriteBuffer();
        if (_callback != nullptr) {
            _callback->onBitchatClientDisconnect();
        }
    }
}

void BitchatBLEService::clearWriteBuffer() {
    _writeBufferOffset = 0;
    memset(_writeBuffer, 0, sizeof(_writeBuffer));
}

bool BitchatBLEService::queueMessage(const BitchatMessage& msg) {
    size_t nextTail = (_queueTail + 1) % MESSAGE_QUEUE_SIZE;

    if (nextTail == _queueHead) {
        BITCHAT_DEBUG_PRINTLN("Message queue full, dropping message");
        return false;
    }

    _messageQueue[_queueTail].msg = msg;
    _messageQueue[_queueTail].valid = true;
    _queueTail = nextTail;

    return true;
}

void BitchatBLEService::processQueue() {
    while (_queueHead != _queueTail) {
        if (_messageQueue[_queueHead].valid) {
            _messageQueue[_queueHead].valid = false;

            if (_callback != nullptr) {
                Serial.println("BLE_SERVICE: processQueue() calling callback...");
                _callback->onBitchatMessageReceived(_messageQueue[_queueHead].msg);
                Serial.println("BLE_SERVICE: processQueue() callback returned");
            }
        }
        _queueHead = (_queueHead + 1) % MESSAGE_QUEUE_SIZE;
    }
}

void BitchatBLEService::loop() {
    uint32_t now = millis();

    // Handle deferred connect callback
    if (_pendingConnect) {
        _pendingConnect = false;
        if (_callback != nullptr) {
            _callback->onBitchatClientConnect();
        }
    }

    // Handle deferred data processing
    // Wait 100ms after last write before processing to allow multi-chunk messages to arrive
    if (_pendingData && (now - _lastWriteTime >= 100)) {
        _pendingData = false;
        Serial.printf("BLE_LOOP: Processing %u bytes, flags byte=%02X\n",
            (unsigned)_writeBufferOffset,
            _writeBufferOffset > 11 ? _writeBuffer[11] : 0);  // flags at offset 11
        Serial.flush();

        // Full buffer dump before parsing
        Serial.println("REDUNDANT_DEBUG: ====== FULL BUFFER BEFORE PARSE ======");
        for (size_t i = 0; i < _writeBufferOffset; i++) {
            Serial.printf("%02X", _writeBuffer[i]);
        }
        Serial.println();
        Serial.println("REDUNDANT_DEBUG: ====== END BUFFER ======");
        Serial.flush();

        // Parse header manually for debug output
        if (_writeBufferOffset >= 14) {
            uint8_t version = _writeBuffer[0];
            uint8_t type = _writeBuffer[1];
            uint8_t ttl = _writeBuffer[2];
            uint8_t flags = _writeBuffer[11];
            uint16_t payloadLen = (_writeBuffer[12] << 8) | _writeBuffer[13];
            Serial.printf("REDUNDANT_DEBUG: HEADER: ver=%u type=0x%02X ttl=%u flags=0x%02X payloadLen=%u\n",
                          version, type, ttl, flags, payloadLen);
            Serial.flush();
        }

        BitchatMessage msg;
        Serial.println("BLE_LOOP: Calling parseMessage...");
        Serial.flush();
        bool parseOk = BitchatProtocol::parseMessage(_writeBuffer, _writeBufferOffset, msg);
        Serial.printf("BLE_LOOP: parseMessage returned %d\n", parseOk);
        Serial.flush();

        if (parseOk) {
            if (BitchatProtocol::validateMessage(msg)) {
                Serial.printf("BLE_LOOP: Valid msg type=0x%02X, len=%d\n", msg.type, msg.payloadLength);
                Serial.flush();
                queueMessage(msg);
            } else {
                Serial.println("BLE_LOOP: Validation failed");
            }
            clearWriteBuffer();
        } else {
            Serial.printf("BLE_LOOP: Parse failed, have %u bytes\n", (unsigned)_writeBufferOffset);
            if (_writeBufferOffset >= BITCHAT_HEADER_SIZE) {
                size_t expectedMin = BitchatProtocol::getMessageSize(msg);
                if (_writeBufferOffset > expectedMin + 100) {
                    Serial.println("BLE_LOOP: Clearing unparseable data");
                    clearWriteBuffer();
                }
            }
        }
    }

    // Check for write buffer timeout
    if (_writeBufferOffset > 0) {
        if (now - _lastWriteTime > WRITE_TIMEOUT_MS) {
            BITCHAT_DEBUG_PRINTLN("Write buffer timeout, clearing");
            clearWriteBuffer();
        }
    }

    // Process queued messages
    processQueue();

    // Debug: mark end of loop iteration if we processed something
    static uint32_t lastLoopPrint = 0;
    if (now - lastLoopPrint > 5000) {
        Serial.println("BLE_SERVICE: loop() heartbeat");
        lastLoopPrint = now;
    }
}

bool BitchatBLEService::broadcastMessage(const BitchatMessage& msg) {
    Serial.println("BLE_SERVICE: >>> broadcastMessage() ENTRY <<<");
    BITCHAT_DEBUG_PRINTLN("broadcastMessage: type=0x%02X, active=%d, subscribed=%d, clients=%d",
                          msg.type, _serviceActive, _clientSubscribed, _bitchatClientCount);

    if (!_serviceActive) {
        BITCHAT_DEBUG_PRINTLN("broadcastMessage: service not active");
        return false;
    }

    // Serialize message - use static buffer to avoid stack overflow
    static uint8_t buffer[BITCHAT_MAX_MESSAGE_SIZE];
    size_t len = BitchatProtocol::serializeMessage(msg, buffer, sizeof(buffer));
    if (len == 0) {
        BITCHAT_DEBUG_PRINTLN("broadcastMessage: serialize failed");
        return false;
    }

    // Always set the characteristic value so it can be read
    _characteristic.write(buffer, len);
    BITCHAT_DEBUG_PRINTLN("broadcastMessage: set characteristic value (%u bytes)", (unsigned)len);

    // If client connected but not yet subscribed, queue for notification later
    if (_bitchatClientCount > 0 && !_clientSubscribed) {
        BITCHAT_DEBUG_PRINTLN("broadcastMessage: client not subscribed, queuing notify for later");
        _pendingOutgoing = msg;
        _hasPendingOutgoing = true;
        return true;  // Value is set for reading, notify will happen when they subscribe
    }

    // Try to send via notify if we have subscribers
    if (_clientSubscribed) {
        Serial.println("BLE_SERVICE: Client subscribed, sending notify");
        uint16_t result = _characteristic.notify(buffer, len);
        BITCHAT_DEBUG_PRINTLN("broadcastMessage: notify returned %u", result);
        if (result) {
            BITCHAT_DEBUG_PRINTLN("TX: type=0x%02X, len=%u", msg.type, (unsigned)len);
        } else {
            Serial.println("BLE_SERVICE: WARNING - notify returned 0 (failed)");
        }
    } else {
        Serial.println("BLE_SERVICE: Client NOT subscribed, only set characteristic value");
    }

    return true;
}

void BitchatBLEService::sendPendingOutgoing() {
    if (!_hasPendingOutgoing || !_clientSubscribed) {
        return;
    }

    BITCHAT_DEBUG_PRINTLN("Sending pending outgoing message");
    _hasPendingOutgoing = false;

    // Serialize and send the pending message - use static buffer to avoid stack overflow
    static uint8_t buffer[BITCHAT_MAX_MESSAGE_SIZE];
    size_t len = BitchatProtocol::serializeMessage(_pendingOutgoing, buffer, sizeof(buffer));
    if (len > 0) {
        uint16_t result = _characteristic.notify(buffer, len);
        BITCHAT_DEBUG_PRINTLN("Pending message notify returned %u", result);
    }
}

// Static callbacks

void BitchatBLEService::onConnect(uint16_t conn_handle) {
    if (_instance != nullptr) {
        _instance->_bitchatClientCount++;
        _instance->_pendingConnect = true;

        // Log connection parameters for debugging
        BLEConnection* conn = Bluefruit.Connection(conn_handle);
        if (conn) {
            BITCHAT_DEBUG_PRINTLN("=== Connection established ===");
            BITCHAT_DEBUG_PRINTLN("Connection interval: %.1f ms", conn->getConnectionInterval() * 1.25);
            BITCHAT_DEBUG_PRINTLN("Slave latency: %d", conn->getSlaveLatency());
            BITCHAT_DEBUG_PRINTLN("Supervision timeout: %d ms", conn->getSupervisionTimeout() * 10);
            BITCHAT_DEBUG_PRINTLN("MTU: %d", conn->getMtu());
        }

        BITCHAT_DEBUG_PRINTLN("BLE client connected");
    }
}

void BitchatBLEService::onDisconnect(uint16_t conn_handle, uint8_t reason) {
    if (_instance != nullptr) {
        _instance->onServerDisconnect();
        const char* reasonStr;
        switch(reason) {
            case 0x08: reasonStr = "Connection timeout"; break;
            case 0x13: reasonStr = "Remote user terminated"; break;
            case 0x16: reasonStr = "Local host terminated"; break;
            case 0x22: reasonStr = "LL Response timeout"; break;
            case 0x3E: reasonStr = "Connection failed to establish"; break;
            default: reasonStr = "Unknown"; break;
        }
        BITCHAT_DEBUG_PRINTLN("BLE client disconnected, reason=0x%02X (%s)", reason, reasonStr);
    }
}

void BitchatBLEService::onCharacteristicWrite(uint16_t conn_handle, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    // REDUNDANT_DEBUG: Entry point - if we don't see this, crash is in BLE stack
    Serial.println("REDUNDANT_DEBUG: >>> onCharacteristicWrite ENTRY <<<");
    Serial.flush();

    Serial.printf("REDUNDANT_DEBUG: conn=%u, chr=%p, data=%p, len=%u\n",
                  conn_handle, (void*)chr, (void*)data, len);
    Serial.flush();

    // Dump first 32 bytes of incoming data (hex)
    Serial.print("REDUNDANT_DEBUG: data[0..31]: ");
    for (uint16_t i = 0; i < len && i < 32; i++) {
        Serial.printf("%02X ", data[i]);
    }
    Serial.println();
    Serial.flush();

    // Full packet hex dump for reconstruction
    Serial.println("REDUNDANT_DEBUG: ====== FULL INCOMING PACKET ======");
    for (uint16_t i = 0; i < len; i++) {
        Serial.printf("%02X", data[i]);
    }
    Serial.println();
    Serial.println("REDUNDANT_DEBUG: ====== END PACKET ======");
    Serial.flush();

    Serial.print("BLE_WRITE_CB: len=");
    Serial.println(len);
    Serial.flush();

    if (_instance == nullptr) {
        Serial.println("REDUNDANT_DEBUG: _instance is NULL!");
        Serial.flush();
        return;
    }
    if (len == 0) {
        Serial.println("REDUNDANT_DEBUG: len is 0!");
        Serial.flush();
        return;
    }

    Serial.printf("REDUNDANT_DEBUG: _instance=%p, _writeBufferOffset=%u, bufSize=%u\n",
                  (void*)_instance, (unsigned)_instance->_writeBufferOffset,
                  (unsigned)sizeof(_instance->_writeBuffer));
    Serial.flush();

    _instance->_lastWriteTime = millis();
    _instance->_pendingData = true;
    Serial.println("REDUNDANT_DEBUG: updated lastWriteTime and pendingData");
    Serial.flush();

    // Append to write buffer
    size_t copyLen = len;
    if (_instance->_writeBufferOffset + copyLen > sizeof(_instance->_writeBuffer)) {
        Serial.printf("REDUNDANT_DEBUG: OVERFLOW! offset=%u + len=%u > bufSize=%u\n",
                      (unsigned)_instance->_writeBufferOffset, (unsigned)copyLen,
                      (unsigned)sizeof(_instance->_writeBuffer));
        Serial.flush();
        _instance->clearWriteBuffer();
        copyLen = (len > sizeof(_instance->_writeBuffer)) ? sizeof(_instance->_writeBuffer) : len;
        Serial.printf("REDUNDANT_DEBUG: after clear, copyLen=%u\n", (unsigned)copyLen);
        Serial.flush();
    }

    Serial.printf("REDUNDANT_DEBUG: memcpy %u bytes to _writeBuffer[%u]\n",
                  (unsigned)copyLen, (unsigned)_instance->_writeBufferOffset);
    Serial.flush();

    memcpy(&_instance->_writeBuffer[_instance->_writeBufferOffset], data, copyLen);

    Serial.println("REDUNDANT_DEBUG: memcpy done");
    Serial.flush();

    _instance->_writeBufferOffset += copyLen;
    Serial.print("BLE_WRITE_CB: buffer now ");
    Serial.println(_instance->_writeBufferOffset);
    Serial.flush();

    // Dump full buffer content (first 64 bytes) for debugging
    Serial.print("REDUNDANT_DEBUG: buffer[0..63]: ");
    for (size_t i = 0; i < _instance->_writeBufferOffset && i < 64; i++) {
        Serial.printf("%02X ", _instance->_writeBuffer[i]);
    }
    Serial.println();
    Serial.flush();

    Serial.println("REDUNDANT_DEBUG: >>> onCharacteristicWrite EXIT <<<");
    Serial.flush();
}

void BitchatBLEService::onCharacteristicCccdWrite(uint16_t conn_handle, BLECharacteristic* chr, uint16_t cccd_value) {
    Serial.print("BLE_SERVICE: CCCD write callback, cccd_value=0x");
    Serial.println(cccd_value, HEX);

    if (_instance != nullptr) {
        bool wasSubscribed = _instance->_clientSubscribed;
        _instance->_clientSubscribed = (cccd_value & BLE_GATT_HVX_NOTIFICATION) != 0;
        BITCHAT_DEBUG_PRINTLN("CCCD write: notifications %s (was %s)",
                              _instance->_clientSubscribed ? "enabled" : "disabled",
                              wasSubscribed ? "enabled" : "disabled");
        Serial.print("BLE_SERVICE: _clientSubscribed now = ");
        Serial.println(_instance->_clientSubscribed ? "true" : "false");

        // If client just subscribed and we have pending messages, send them
        if (!wasSubscribed && _instance->_clientSubscribed) {
            Serial.println("BLE_SERVICE: Client just subscribed, sending pending");
            _instance->sendPendingOutgoing();
        }
    }
}

#endif // NRF52_PLATFORM
