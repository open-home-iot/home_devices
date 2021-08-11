/*
   Basic LED, the first HOME-compliant device. Its just a stupid blink-sketch,
   but it's still exciting!
*/

// TODO: create HOME-specific header file.
#define REQUEST_TYPE       0
#define REQUEST_CAPABILITY 48 // ASCII 0

#define MSG_START 94 // ASCII ^
#define MSG_END   36 // ASCII $

#define CAPABILITIES "^0{\"uuid\":\"0ededfd1-65aa-42ba-866e-fb5c5ad607f2\",\"category\":1,\"type\":1,\"states\":{\"control\":[\"on\",\"off\"]}}$"

uint8_t request[64];
uint8_t cursor = 0;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Native USB only
  }
}

void loop() {
  while ( Serial.available() )
  {
    // Delay to await input
    delay(2);

    uint8_t buffer[64];
    int count = Serial.readBytes(buffer, sizeof(buffer));

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

    for (int i = 0; i < count; i++)
    {
      uint8_t ch = buffer[i];

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
        request[cursor] = buffer[i];
        cursor++;
      }
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
}
