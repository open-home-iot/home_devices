/*
   Basic LED, the first HOME-compliant device. Its just a stupid blink-sketch,
   but it's still exciting!
*/

#include <bluefruit.h>

// TODO: create HOME-specific header file.
#define REQUEST_TYPE        0
// Ignored since only 1 state group.
#define REQUEST_STATE_GROUP 1
#define REQUEST_NEW_STATE   2

#define REQUEST_CAPABILITY    48 // ASCII 0
#define REQUEST_ACTION        49 // ASCII 1
#define REQUEST_ACTION_STATES 50 // ASCII 2

#define MSG_START 94 // ASCII ^
#define MSG_END   36 // ASCII $

#define CAPABILITIES "^0{\"uuid\":\"b1585196-e856-4218-9130-c90d8e482c99\",\"name\":\"Basic LED\",\"category\":1,\"type\":1,\"states\":[{\"id\":0,\"control\":[{\"on\":1},{\"off\":0}]}]}$"

#define STATE_LED_OFF 48
#define STATE_LED_ON  49

/*
BLE Services

 Nordic Semiconductor Nordic UART Service (NUS)
 128 bit UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/include/bluetooth/services/nus.html
 RX characteristic: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 TX characteristic: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
*/
BLEUart bleuart;

uint8_t request[32];
uint8_t cursor = 0;

uint8_t led_state = LOW;
// Response indicating which stateful action state: ^100$ | ^101$
// 0x5e | 94   = ^ - message start
// 0x31 | 49   = 1 - message type, STATEFUL ACTION
// 0x30 | 48   = 0 - group ID
// 0x30 | 48   = 0 - LED state
// 0x24 | 36   = $ - message end
uint8_t response[5] = { 0x5e, 0x31, 0x30, 0x30, 0x24 };

void setup() {
#if CFG_DEBUG
  // Blocking wait for connection when debug mode is enabled via IDE
  while ( !Serial ) yield();
#endif

  // Setup the BLE LED to be enabled on CONNECT
  // Note: This is actually the default behavior, but provided
  // here in case you want to control this LED manually via PIN 19
  Bluefruit.autoConnLed(true);

  // Config the peripheral connection with maximum bandwidth 
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

  Bluefruit.begin();
  Bluefruit.setName("Basic LED");
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Set up and start advertising
  startAdv();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, led_state);
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);

  // Secondary Scan Response packet (optional)
  // Since there is no room for 'Name' in Advertising packet
  Bluefruit.ScanResponse.addName();

  // SVC 16 bit, meaning [0] and [1] constitutes the SVC UUID, with [1] preceding [0].
  // HOME SVC UUID: a1b2
  // HOME SVC value: 1337
  const uint8_t sData[] = { 0xb2, 0xa1, 0x13, 0x37 };
  Bluefruit.ScanResponse.addData(BLE_GAP_AD_TYPE_SERVICE_DATA, &sData, 4);
  
  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds  
}

void loop() {
  while ( bleuart.available() )
  {
    /*
       Iterate over read bytes and put together a full request body,
       append to request array where cursor left off, if multiple
       read-iterations were necessary.

       If encountering MSG_START:
       - clear request array
       - reset cursor
       - start appending request bytes to the request array

       If encountering MSG_END:
       - execute request
       - continue parsing input bytes

    */
    uint8_t ch;
    ch = (uint8_t) bleuart.read();

    if (ch == MSG_START)
    {
      memset(request, 0, sizeof(request));
      cursor = 0;
    }
    else if (ch == MSG_END)
    {
      execute_request();
    }
    else
    {
      request[cursor] = ch;
      cursor++;
    }
  }
}

void execute_request() {
  /*
     1. Find which request was received
     2. Execute the corresponding action
  */
  if (request[REQUEST_TYPE] == REQUEST_CAPABILITY)
  {
    bleuart.write(CAPABILITIES);
  }
  else if (request[REQUEST_TYPE] == REQUEST_ACTION_STATES) 
  {
    if (led_state == LOW) 
    {
      response[3] = 0x30;  
    }
    else
    {
      response[3] = 0x31;
    }
    bleuart.write(response, 5);

    write_some_weird_stuff();
  }
  else if (request[REQUEST_TYPE] == REQUEST_ACTION)
  {
    // Ignore checking state group since only one exists.
    
    if (request[REQUEST_NEW_STATE] == STATE_LED_ON)
    {
      led_state = HIGH;
      digitalWrite(LED_BUILTIN, led_state);
      response[3] = 0x31;
      bleuart.write(response, 5);
    }
    else if (request[REQUEST_NEW_STATE] == STATE_LED_OFF)
    {
      led_state = LOW;
      digitalWrite(LED_BUILTIN, led_state);
      response[3] = 0x30;
      bleuart.write(response, 5);
    }
  }
}

void write_some_weird_stuff()
{
  uint8_t stuff[5] = { 0x5e, 0x2, 0x3, 0x4, 0x24 };
  uint8_t more[5] = { 0x5e, 0x30, 0x31, 0x01, 0x24 };
  char chars[6] = "^bc1$";

  bleuart.write(stuff, 5);
  bleuart.write(more, 5);
  bleuart.write(chars);

  chars[3] = '!';
  bleuart.write(chars);

  more[2] = '1';
  bleuart.write(more, 5);
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
}
