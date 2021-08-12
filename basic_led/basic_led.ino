/*
   Basic LED, the first HOME-compliant device. Its just a stupid blink-sketch,
   but it's still exciting!
*/

// TODO: create HOME-specific header file.
#define REQUEST_TYPE        0
#define REQUEST_STATE_GROUP 1
#define REQUEST_NEW_STATE   2

#define REQUEST_CAPABILITY  48 // ASCII 0
#define REQUEST_ACTION      49 // ASCII 1

#define MSG_START 94 // ASCII ^
#define MSG_END   36 // ASCII $

#define CAPABILITIES "^0{\"uuid\":\"0ededfd1-65aa-42ba-866e-fb5c5ad607f2\",\"name\":\"Basic LED\",\"category\":1,\"type\":1,\"states\":[{\"id\":0,\"control\":[{\"on\":1},{\"off\":0}]}]}$"

#define STATE_LED_OFF 48
#define STATE_LED_ON  49

#define LED_PIN 13

uint8_t request[32];
uint8_t cursor = 0;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  while ( Serial.available() )
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
    uint8_t ch = Serial.read();

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
    Serial.write(CAPABILITIES);
  }
  else if (request[REQUEST_TYPE] == REQUEST_ACTION)
  {
    // Ignore checking state group since only one exists.
    
    if (request[REQUEST_NEW_STATE] == STATE_LED_ON)
    {
      digitalWrite(LED_PIN, HIGH);
    }
    else if (request[REQUEST_NEW_STATE] == STATE_LED_OFF)
    {
      digitalWrite(LED_PIN, LOW);
    }
  }
}
