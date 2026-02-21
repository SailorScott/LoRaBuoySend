// PYC Race Mark postion sending unit.
// Send position every minute
// Auto-sleep after 5 hours (reset to wake)

// Turn off the display after 5min from restart

// Normal sleep mode
// turn off the GPS.


#include "Arduino.h"
#include "GPS_Air530Z.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include "LoRaWan_APP.h"

SSD1306Wire displayBd(0x3c, 500000, SDA, SCL, GEOMETRY_128_64, GPIO10); // addr , freq , i2c group , resolution , rst
Air530ZClass GPS;

// Buoys can transmit every 30 seconds and send 2 positions
// 	GPS postion reads at 0 and 15
// 	Transmit at GPS Seconds Mod 0 (and 30) = time slot seconds 0  + y tenths.

#define DEVICE_ID 6 // Device ID in database

#define TXSECTIMETRIG 30      // Time triggerred sampling - transmit at 0 and 30 seconds for Buoys
#define GPSREADSECTIMETRIG 15 // time triggered reading of GPS at 15 second base (0, 15, 30 and 45) seconds
#define TXTENTHSSLOT 6        // Tx slot for tenths of second - see database for assigned time slot

#define RF_FREQUENCY 915000000 // Hz

#define TX_OUTPUT_POWER 14 // dBm

#define LORA_BANDWIDTH 2        // [0: 125 kHz,
                                //  1: 250 kHz,
                                //  2: 500 kHz,
                                //  3: Reserved]
#define LORA_SPREADING_FACTOR 8 // [SF7..SF12]
#define LORA_CODINGRATE 2       // [1: 4/5,
                                //  2: 4/6,
                                //  3: 4/7,
                                //  4: 4/8]
#define LORA_PREAMBLE_LENGTH 8  // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT 0   // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define RX_TIMEOUT_VALUE 1000
#define BUFFER_SIZE 200 // room for 4 messages

#define BOATAUTOSLEETIME 18000000 // 5 hours then go to sleep. Reset occurs to wake up.

#define DISPLAY_OFF_TIME 300000    // 5 minutes in ms - turn off display after this
#define NAV_FLASH_ON_TIME 500      // Nav light on duration (0.5 seconds)
#define NAV_FLASH_CYCLE_TIME 20000 // Nav light cycle (20 seconds)

const int MessageType = 2;     // Buoy Message
int TxCounter = 0;             // Incrument with each send, used for checking for dropped messages.
char transmitStr[BUFFER_SIZE]; // Used for holding the Tx message
char transmit2Str[BUFFER_SIZE];
int secondsCounter = -99; // Used for counting seconds for Tx slot

int startTran = 0; // Used for timing the Tx message
int gpsLat = 0;    // hold Lat and long from GPS
int gpsLong = 0;
bool successfulGPSMessage = false;

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);

void OnNavLightOn(void);
void OnNavLightOff(void);
void updateNavLightFlash(void);
void updateDisplayAutoOff(void);
bool isNightMode(void);
bool buildGPSTXMessage(void);

uint16_t voltage = 0;          // Reading battery voltage level
uint32_t bootTime = 0;         // Track boot time for display auto-off
bool displayActive = true;     // Display on/off state
typedef enum
{
  stWaitGPSBoot,
  stWaitGPS1PPS,
  st1SecPulseGrapGPSData,
  stTxString,
  stReadGPSSave2BufferAndTransmit,
  stReadGPSSave2Buffer
} States_t;

// intial state machine
States_t StateMachine = stWaitGPSBoot;
uint32_t starttime = 0;

int fracPart(double val, int n) // Cuts apart GPS lat/long
{
  val = abs(val);
  return (int)((val - (int)(val)) * pow(10, n));
}

// Vext controls external power to GPS and Display
void VextPowON(void)
{
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextPowOFF(void) // Vext default OFF
{
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
}

// GPS 1 second pulse interrupt handler. Set next state.
void GPS1SecPulse(void)
{

  StateMachine = st1SecPulseGrapGPSData;
}

void setup()
{
  VextPowON();
  delay(10);

  displayBd.init();
  displayBd.clear();
  displayBd.display();

  displayBd.setTextAlignment(TEXT_ALIGN_CENTER);
  displayBd.setFont(ArialMT_Plain_16);
  displayBd.drawString(64, 32 - 16 / 2, "GPS initing...");
  displayBd.display();

  displayBd.setTextAlignment(TEXT_ALIGN_LEFT);
  displayBd.setFont(ArialMT_Plain_10);

  Serial.begin(115200);
  Serial.print("Looking for GPS......");
  GPS.setmode(MODE_GPS_GLONASS); // US and Europen satalites.
  GPS.setNMEA(0);                // No preprogramed sentances being sent out.
  GPS.begin(57600);

  // Setup Pins usage
  pinMode(GPIO12, INPUT_PULLUP);
  pinMode(GPIO6, OUTPUT_PULLDOWN); // Nav light
  pinMode(ADC2, INPUT);            // Solar panel voltage

  // Setup LoRa
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);

  bootTime = millis();
  StateMachine = stWaitGPSBoot; // intial state
}

void loop()
{

  updateDisplayAutoOff();
  updateNavLightFlash();

  switch (StateMachine)
  {
  case stWaitGPSBoot: // Wait for GPS to boot up
    if (GPS.available() > 0)
    {
      attachInterrupt(digitalPinToInterrupt(GPIO12), GPS1SecPulse, RISING);
    }
    TxCounter = TxCounter + 1;
    if (displayActive)
    {
      displayBd.clear();
      displayBd.drawString(0, 0, "waiting..." + String(TxCounter));
      displayBd.display();
    }
    Serial.println("Wait GPS Boot.." + String(TxCounter));
    delay(1000);
    break;

  case st1SecPulseGrapGPSData:
    // Check if time to send or just wait.
    starttime = millis(); // time of 1 second pulse

    while (GPS.available() > 0)
    {
      GPS.encode(GPS.read());
      secondsCounter = GPS.time.second();
    }

    {
      bool txNow = false;
      if (isNightMode())
      {
        // Night mode (22:00-04:00): transmit every minute (at 0 and 30 seconds)
        txNow = (secondsCounter % TXSECTIMETRIG == 0);
      }
      else
      {
        // Day mode (04:01-21:59): transmit every 5 minutes (at second 0)
        txNow = (secondsCounter == 0 && GPS.time.minute() % 5 == 0);
      }

      if (txNow)
      {
        if (!isNightMode())
        {
          // Day mode: clear buffer, send single reading only
          transmitStr[0] = 0;
        }
        StateMachine = stReadGPSSave2BufferAndTransmit;
      }
      else if (isNightMode() && secondsCounter % GPSREADSECTIMETRIG == 0)
      {
        // Night mode only: buffer GPS readings between transmits
        Serial.print(String(GPS.time.hour()) + ":" + String(GPS.time.minute()) + ":" + String(secondsCounter));
        Serial.print(" Sat=" + String(GPS.satellites.value()));
        Serial.print(" Lat=" + String(GPS.location.lat(), 6));
        Serial.println(" Lng=" + String(GPS.location.lng(), 6));
        StateMachine = stReadGPSSave2Buffer;
      }
      else
      {
        StateMachine = stWaitGPS1PPS;
      }
    }

    break;

  case stTxString:
    while ((millis() - starttime) < 100 * TXTENTHSSLOT)
    { // wait till time send.
    }
    turnOnRGB(0x0000FF, 0); // Blue during TX
    if (displayActive)
    {
      displayBd.clear();
      displayBd.drawStringMaxWidth(20, 0, 128, transmitStr);
      displayBd.display();
    }

    startTran = millis();

    Serial.println("TX>> " + String(transmitStr));
    Radio.Send((uint8_t *)transmitStr, strlen(transmitStr));

    StateMachine = stWaitGPS1PPS; // Wait for 1 second pulse.

    break;

  case stReadGPSSave2BufferAndTransmit:
    // read GPS and save to buffer, then transmit if  GPS is good
    successfulGPSMessage = buildGPSTXMessage();

    if (successfulGPSMessage)
    {
      StateMachine = stTxString;
    }
    else
    {
      StateMachine = stWaitGPS1PPS;
    }

    break;
  case stReadGPSSave2Buffer:
    // read GPS and save to buffer, then wait for next 1 second pulse
    successfulGPSMessage = buildGPSTXMessage();
    StateMachine = stWaitGPS1PPS;
    break;

  } // end of switch
} // end of loop

bool buildGPSTXMessage(void)
{
  // Start building up transmit string
  // Get current battery voltage
  pinMode(VBAT_ADC_CTL, OUTPUT);
  digitalWrite(VBAT_ADC_CTL, LOW);
  voltage = analogRead(ADC);

  pinMode(VBAT_ADC_CTL, INPUT);

  gpsLat = GPS.location.lat();
  gpsLong = GPS.location.lng();

  if (gpsLat != 0 && gpsLong != 0)
  {
    // Serial.println("buildGPSTXMessage");

    // GPS is good, build up transmit string
    sprintf(transmit2Str,
            "|%d,%d,"       // Device/msg
            "%02d%02d%02d," // gps time
            "%d.%d,"        // Lat
            "%d.%d,"        // Long
            "%d",           // battery level //// fake RSSI
            MessageType, DEVICE_ID,
            GPS.time.hour(), GPS.time.minute(), GPS.time.second(),
            gpsLat, fracPart(GPS.location.lat(), 6), // (int)GPS.location.lat()
            gpsLong, fracPart(GPS.location.lng(), 6),
            voltage);

    strcat(transmitStr, transmit2Str);
    transmit2Str[0] = 0; // clear transmit2Str
    return true;         // good read
  }
  else
  {
    // GPS is not good, wait for next 1 second pulse
    // Serial.println("bad lat or long");
    return false; // bad read
  }
}
void OnTxDone(void)
{
  // Serial.println("TX done!");
  int endTran = millis();
  turnOffRGB();
  Serial.println("Tx time=" + String(endTran - startTran) );
  // Serial.println(transmitStr);
  transmitStr[0] = 0; // clear transmitStr
}

void OnTxTimeout(void)
{
  Radio.Sleep();
  Serial.println("TX Timeout......");
  StateMachine = stWaitGPS1PPS;
}

void OnNavLightOn(void)
{
  digitalWrite(GPIO6, HIGH);
}
void OnNavLightOff(void)
{
  digitalWrite(GPIO6, LOW);
}

// Check if in night mode: UTC 22:00 to 04:00
bool isNightMode(void)
{
  int hour = GPS.time.hour();
  return (hour >= 22 || hour < 4);
}

// Nav light flash: 0.5s on every 20s, active UTC 01:00 to 13:00
void updateNavLightFlash(void)
{
  int hour = GPS.time.hour();
  bool navLightActive = (hour >= 1 && hour < 13);

  if (navLightActive)
  {
    uint32_t cyclePos = millis() % NAV_FLASH_CYCLE_TIME;
    if (cyclePos < NAV_FLASH_ON_TIME)
    {
      OnNavLightOn();
    }
    else
    {
      OnNavLightOff();
    }
  }
  else
  {
    OnNavLightOff();
  }
}

// Turn off display after 5 minutes from boot
void updateDisplayAutoOff(void)
{
  if (displayActive && (millis() - bootTime) > DISPLAY_OFF_TIME)
  {
    displayBd.clear();
    displayBd.display();
    displayActive = false;
    Serial.println("Display auto-off");
  }
}

