/*
   Basic LED, the first HOME-compliant device. Its just a stupid blink-sketch,
   but it's still exciting!
*/

#include <bluefruit.h>

// Message start/end chars
#define MSG_START_CHAR '^'
#define MSG_END_CHAR   '$'

// Message indices
#define I_MSG_TYPE        0
#define I_MSG_STATE_GROUP 1
#define I_MSG_NEW_STATE   2

// Message types
#define MSG_CAPABILITY      '0'
#define MSG_ACTION_STATEFUL '1'
#define MSG_ACTION_STATES   '2'

// State groups
#define STATE_GROUP_RED   '0'
#define STATE_GROUP_GREEN '1'
#define STATE_GROUP_BLUE  '2'

// New state
#define NEW_STATE_LED_OFF '0'
#define NEW_STATE_LED_ON  '1'

// Device capability
#define CAPABILITIES "^0{\"uuid\":\"b1585196-e856-4218-9130-c90d8e482c99\",\"name\":\"Basic LED\",\"category\":1,\"type\":1,\"states\":[{\"id\":0,\"red\":[{\"on\":1},{\"off\":0}]},{\"id\":1,\"green\":[{\"on\":1},{\"off\":0}]},{\"id\":2,\"blue\":[{\"on\":1},{\"off\":0}]}]}$"

// LED pins
#define PIN_RED   16
#define PIN_GREEN 15
#define PIN_BLUE  7

/*
BLE Services

 Nordic Semiconductor Nordic UART Service (NUS)
 128 bit UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/include/bluetooth/services/nus.html
 RX characteristic: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
 TX characteristic: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
*/
BLEUart bleuart;

byte request[32];
byte cursor = 0;

// Response indicating stateful action state: ^100$ | ^101$ | ^110$ | ^111$ | ^120$ | ^121$
byte response[5] = { '^', '1', '0', '0', '$' };

// State tracking, all are LOW by default
byte states[3] = { LOW, LOW, LOW };

void setup() {
#if CFG_DEBUG
  // Blocking wait for connection when debug mode is enabled via IDE
  while ( !Serial ) yield();
#endif

  // pinmodes
  pinMode(PIN_RED, OUTPUT);
  digitalWrite(PIN_RED, LOW);
  pinMode(PIN_GREEN, OUTPUT);
  digitalWrite(PIN_GREEN, LOW);
  pinMode(PIN_BLUE, OUTPUT);
  digitalWrite(PIN_BLUE, LOW);

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

  // Configure and Start BLE Uart Service
  bleuart.begin();

  // Set up and start advertising
  startAdv();
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

  // b1585196-e856-4218-9130-c90d8e482c99
  const uint8_t sData[] = { 0x99, 0x2c, 0x48, 0x8e, 0x0d, 0xc9, 0x30, 0x91, 0x18, 0x42, 0x56, 0xe8, 0x96, 0x51, 0x58, 0xb1, 0xff, 0xff };
  Bluefruit.ScanResponse.addData(BLE_GAP_AD_TYPE_SERVICE_DATA_128BIT_UUID, &sData, 18);
  
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

       If encountering MSG_START_CHAR:
       - clear request array
       - reset cursor
       - start appending request bytes to the request array

       If encountering MSG_END_CHAR:
       - execute request
       - continue parsing input bytes

    */
    uint8_t ch;
    ch = (uint8_t) bleuart.read();

    if (ch == MSG_START_CHAR)
    {
      memset(request, 0, sizeof(request));
      cursor = 0;
    }
    else if (ch == MSG_END_CHAR)
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
  switch (request[I_MSG_TYPE]) 
  {
    case MSG_CAPABILITY:
      bleuart.write(CAPABILITIES);
      break;
    case MSG_ACTION_STATES:
      report_action_states();
      break;
    case MSG_ACTION_STATEFUL:
      change_led_state();
      break;
    default:
      break;
  }
}

void report_action_states()
{
  response[2] = STATE_GROUP_RED; // already ASCII formatted
  response[3] = numberToAscii(states[0]);
  bleuart.write(response, 5);
  
  response[2] = STATE_GROUP_GREEN; // already ASCII formatted
  response[3] = numberToAscii(states[1]);
  bleuart.write(response, 5);
  
  response[2] = STATE_GROUP_BLUE; // already ASCII formatted
  response[3] = numberToAscii(states[2]);
  bleuart.write(response, 5);
}

void change_led_state()
{
  byte new_state = asciiToNumber(request[I_MSG_NEW_STATE]);
  switch (request[I_MSG_STATE_GROUP])
  {
    case STATE_GROUP_RED:
      digitalWrite(PIN_RED, new_state);
      break;
    case STATE_GROUP_GREEN:
      digitalWrite(PIN_GREEN, new_state);
      break;
    case STATE_GROUP_BLUE:
      digitalWrite(PIN_BLUE, new_state);
      break;
    default:
      break;
  }
  // Update state array
  states[asciiToNumber(request[I_MSG_STATE_GROUP])] = new_state;

  // Update and send the response
  response[2] = request[I_MSG_STATE_GROUP];
  response[3] = request[I_MSG_NEW_STATE];
  bleuart.write(response, 5);
}

byte asciiToNumber(char c) 
{
  return c - '0';
}

char numberToAscii(byte b)
{
  return b + '0';
}
