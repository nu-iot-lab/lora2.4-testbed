#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>                                               //the lora device is SPI based so load the SPI library                                         
#include <SX128XLT.h>                                          //include the appropriate library  
#include <string.h>
#include <Adafruit_NeoPixel.h>

#define Program_Version "V1.1"


/*******************************************************************************************************
  Programs for Arduino - Copyright of the author Stuart Robinson - 02/03/20

  This program is supplied as is, it is up to the user of the program to decide if the program is
  suitable for the intended purpose and free from errors.
*******************************************************************************************************/

//*******  Setup hardware pin definitions here ! ***************

//These are the pin definitions for one of my own boards, a ESP32 shield base with BBF board shield on
//top. Be sure to change the definitions to match your own setup. Some pins such as DIO2, DIO3, BUZZER
//may not be in used by this sketch so they do not need to be connected and should be included and be 
//set to -1.

#define NSS 33                                   //select pin on LoRa device
#define SCK 5                                  //SCK on SPI3
#define MISO 21                                 //MISO on SPI3 
#define MOSI 19                                 //MOSI on SPI3 

#define NRESET 27                               //reset pin on LoRa device
#define RFBUSY 25                               //busy line

#define LED1 2                                  //on board LED, high for on
#define DIO1 26                                 //DIO1 pin on LoRa device, used for RX and TX done 
#define DIO2 -1                                 //DIO2 pin on LoRa device, normally not used so set to -1 
#define DIO3 -1                                 //DIO3 pin on LoRa device, normally not used so set to -1
#define RX_EN -1                                //pin for RX enable, used on some SX128X devices, set to -1 if not used
#define TX_EN -1                                //pin for TX enable, used on some SX128X devices, set to -1 if not used 
#define BUZZER -1                               //pin for buzzer, set to -1 if not used 
#define VCCPOWER 14                             //pin controls power to external devices
#define LORA_DEVICE DEVICE_SX1280               //we need to define the device we are using

#define NEO_DATA_PIN 0  // Change to the correct pin for your NeoPixel data
#define NEO_POWER_PIN 32  // If using a power control pin for NeoPixel
#define NUM_PIXELS 1  // Number of pixels in your NeoPixel strip (assuming 1)

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEO_DATA_PIN, NEO_GRB + NEO_KHZ800);

void setNeoPixelColor(uint8_t red, uint8_t green, uint8_t blue) {
  strip.setPixelColor(0, strip.Color(red, green, blue));  // Set the NeoPixel color
  strip.show();  // Update the NeoPixel
}

//*******  Setup LoRa Parameters Here ! ***************

//LoRa Modem Parameters
// const uint32_t Frequency = 2405000000;           //frequency of transmissions for A
// const uint32_t Frequency = 2410000000;           //frequency of transmissions for B
const uint32_t Frequency = 2415000000;           //frequency of transmissions for C
const int32_t Offset = 0;                        //offset frequency for calibration purposes
const uint8_t Bandwidth = LORA_BW_0800;          //LoRa bandwidth
const uint8_t SpreadingFactor = LORA_SF7;        //LoRa spreading factor
const uint8_t CodeRate = LORA_CR_4_5;            //LoRa coding rate

const int8_t TXpower = 10;                       //LoRa transmit power in dBm

const uint16_t packet_delay = 1000;             //mS delay between packets

#define RXBUFFER_SIZE 255                        //RX buffer size (always same)


SX128XLT LT;                                     //create a library class instance called LT

uint32_t RXpacketCount;
uint32_t errors;

uint8_t RXBUFFER[RXBUFFER_SIZE];                 //create the buffer that received packets are copied into

uint8_t RXPacketL;                               //stores length of packet received
int16_t PacketRSSI;                              //stores RSSI of received packet
int8_t  PacketSNR;                               //stores signal to noise ratio (SNR) of received packet


//*******                          ***************

//*******  Setup WiFi and Socket params here! ***************
const String init_com = "START";
bool exp_init = false;
bool setup_init = false;
const char* ssid = "IoTlab_Public";
const char* password = "iotlabwifi";
const int udpPort = 12345;
WiFiUDP udp;
char incomingUDPPacket[255];

WiFiServer server(80); // Initialize the server on port 80


//*******                          ***************

void setup() {
  //***************************************************************************************************
  //Setup WiFi device
  //***************************************************************************************************
  Serial.begin(115200);
  delay(1000);
  WiFi.begin(ssid, password);

  // Initialize the NeoPixel
  pinMode(NEO_POWER_PIN, OUTPUT);  // Assuming you control power to the NeoPixel
  digitalWrite(NEO_POWER_PIN, HIGH);  // Turn on power to NeoPixel
  strip.begin();  // Initialize the NeoPixel
  strip.setBrightness(50);  // Adjust brightness to 50%
  strip.show();  // Make sure all pixels are off

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    setNeoPixelColor(255, 0, 0);  // Set NeoPixel to red while connecting
    delay(1000);
  }

  setNeoPixelColor(0, 255, 0);  // Set NeoPixel to green once connected
  Serial.println("Connected to Wi-Fi. IP address: ");
  Serial.println(WiFi.localIP());

   // Start the server
  server.begin();
  Serial.println("Server started");

  udp.begin(udpPort);
  
  //***************************************************************************************************
  //Setup LoRa device
  //***************************************************************************************************
  pinMode(LED1, OUTPUT);                        //setup pin as output for indicator LED
  led_Flash(2, 125);                            //two quick LED flashes to indicate program start

  Serial.println();
  Serial.println(F("104_LoRa_Receiver_Detailed_Setup_ESP32 Starting"));
  Serial.println();
  
  //SPI.begin();                                          //default setup can be used be used
  SPI.begin(SCK, MISO, MOSI, NSS);                        //alternative format for SPI3, VSPI; SPI.begin(SCK,MISO,MOSI,SS)


  //SPI beginTranscation is normally part of library routines, but if it is disabled in the library
  //a single instance is needed here, so uncomment the program line below
  //SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  //setup hardware pins used by device, then check if device is found
  if (LT.begin(NSS, NRESET, RFBUSY, DIO1, LORA_DEVICE))
  {
    Serial.println(F("LoRa Device found"));
    led_Flash(2, 125);
    delay(1000);
  }
  else
  {
    Serial.println(F("No device responding"));
    while (1)
    {
      led_Flash(50, 50);                                       //long fast speed LED flash indicates device error
    }
  }

  //The function call list below shows the complete setup for the LoRa device using the information defined in the
  //Settings.h file.
  //The 'Setup LoRa device' list below can be replaced with a single function call;
  //LT.setupLoRa(Frequency, Offset, SpreadingFactor, Bandwidth, CodeRate);

  //***************************************************************************************************
  //Setup LoRa device
  //***************************************************************************************************
  LT.setMode(MODE_STDBY_RC);
  LT.setRegulatorMode(USE_LDO);
  LT.setPacketType(PACKET_TYPE_LORA);
  LT.setRfFrequency(Frequency, Offset);
  LT.setBufferBaseAddress(0, 0);
  LT.setModulationParams(SpreadingFactor, Bandwidth, CodeRate);
  LT.setPacketParams(12, LORA_PACKET_VARIABLE_LENGTH, 255, LORA_CRC_ON, LORA_IQ_NORMAL, 0, 0);
  LT.setDioIrqParams(IRQ_RADIO_ALL, (IRQ_TX_DONE + IRQ_RX_TX_TIMEOUT), 0, 0);
  //***************************************************************************************************


  Serial.println();
  LT.printModemSettings();                                     //reads and prints the configured LoRa settings, useful check
  Serial.println();
  LT.printOperatingSettings();                                 //reads and prints the configured operting settings, useful check
  Serial.println();
  Serial.println();
  LT.printRegisters(0x900, 0x9FF);                             //print contents of device registers, normally 0x900 to 0x9FF
  Serial.println();
  Serial.println();

  Serial.print(F("Receiver ready - RXBUFFER_SIZE "));
  Serial.println(RXBUFFER_SIZE);
  Serial.println();
}

void loop() {
  //***************************************************************************************************
  //GET PARAMS
  //***************************************************************************************************
  // Check if a client has connected

  if (WiFi.status() != WL_CONNECTED) {
    setNeoPixelColor(255, 0, 0);  
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("WiFi reconnected.");
    setNeoPixelColor(0, 255, 0);  
  }

  WiFiClient client = server.available();
  if (client) {
    setNeoPixelColor(0, 255, 0); 
    Serial.println("New client connected");
    exp_init = false;
    String message = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        message += c;
        if (c == '\n') {
          break;
        }
      }
    }

    // Process the received message
    Serial.println("Processing message: " + message);
    if (message.startsWith("[ERR]")) {
      processErrorMessage(message);
      client.println("error_received");
    } else {
      processMessage(message);
      // Set NeoPixel to orange to indicate data is being processed
      setNeoPixelColor(255, 165, 0);  
      delay(1000);  
      setNeoPixelColor(0, 255, 0);  
      // Send response to the client
      client.println("message_received");
    }

    
    // // Send response to the client
    // client.println("message_received");
    
    // Close the connection
    client.stop();
    Serial.println("\nClient disconnected");

  }

  //***************************************************************************************************
  //GET START EXP COMMAND
  //***************************************************************************************************
  int packetSize = udp.parsePacket();
  if (!exp_init && packetSize && setup_init) {
    int len = udp.read(incomingUDPPacket, 255);
    if (len > 0) {
      incomingUDPPacket[len] = 0; // Null-terminate the string
    }
    Serial.printf("Received packet: %s\n", incomingUDPPacket);
    // Convert the String object to a char array
      if (strcmp(incomingUDPPacket, init_com.c_str()) == 0) {
        Serial.println("They are equal!");
        exp_init = true;
        RXpacketCount = 0;
        Serial.println("Experiment is started!");
        setup_init = false;
      } else {
          Serial.println("Wrong Command");
      }
  }

  //***************************************************************************************************
  //EXP IS STARTED
  //***************************************************************************************************
  if (exp_init){
    RXPacketL = LT.receive(RXBUFFER, RXBUFFER_SIZE, 20000, WAIT_RX); //wait for a packet to arrive with 60seconds (60000mS) timeout
    setNeoPixelColor(0, 0, 255);  // Blue for packet receiving
    digitalWrite(LED1, HIGH);                      //something has happened

    PacketRSSI = LT.readPacketRSSI();              //read the recived RSSI value
    PacketSNR = LT.readPacketSNR();                //read the received SNR value

    if (RXPacketL == 0)                            //if the LT.receive() function detects an error, RXpacketL is 0
    {
      packet_is_Error();
    }
    else
    {
      packet_is_OK(); // if packet is Ok increment rec packs num
    }

    digitalWrite(LED1, LOW);                       //LED off
    setNeoPixelColor(0, 255, 0);  

    Serial.println();                                //have a delay between packets
  }


}

void processMessage(String message) {
  // Trim the message
  message.trim();
  Serial.println("Received message: " + message);

  // Parse the message
  char id;
  int sf;
  int space1 = message.indexOf(' ');
  
  if (space1 > 0) {
    id = message.charAt(0); // Get the first character as the ID
    sf = message.substring(space1 + 1).toInt();
    
    Serial.print("ID: ");
    Serial.println(id);
    Serial.print("Spreading Factor: ");
    Serial.println(sf);
    
    // Example actions based on the parsed data
    if (id == 'A' || id == 'B' || id == 'C' ) {
      setup_init = SetUpLoRA(sf); // setUP and check if successful
    } else {
      Serial.println("Invalid message format");
    }
  }
}

void processErrorMessage(String message) {
  // Implement your error message processing logic here
  Serial.println(message);
}

bool SetUpLoRA(int SF) {
  uint8_t NewSpreadingFactor;

  if (SF == 7) { NewSpreadingFactor = LORA_SF7;} 
  else if (SF == 8) { NewSpreadingFactor = LORA_SF8;} 
  else if (SF == 9) { NewSpreadingFactor = LORA_SF9;} 
  else if (SF == 10) { NewSpreadingFactor = LORA_SF10;} 
  else if (SF == 11) { NewSpreadingFactor = LORA_SF11;} 
  else if (SF == 12) { NewSpreadingFactor = LORA_SF12;} 
  else {
    Serial.println("Invalid SF value");
    return false;
  }
  
  // update SF
  LT.setModulationParams(NewSpreadingFactor, Bandwidth, CodeRate);
  Serial.println("New SF set was successful");
  LT.printModemSettings(); 
  return true;
}

// Verify that all bytes in the buffer after the first byte are 'a'.
bool isPacketValid() {

  // The first byte of the buffer represents the length of the message
  uint8_t length = RXBUFFER[0];

  // // Check if the length is valid
  // // It should be less than or equal to the size of the buffer - 1
  // if (length > (RXBUFFER_SIZE - 1)) {
  //       return false;
  // }
  //LT.printASCIIPacket(RXBUFFER, length);                        //print the buffer (the sent packet) as ASCII

  // Check if all bytes after the first byte are 'a'
  for (size_t i = 2; i < length; ++i) {

      if (RXBUFFER[i] != 'a') {
          Serial.print(RXBUFFER[i]);
          return false;
      }
  }
  return true;
}

void packet_is_OK()
{
  uint16_t IRQStatus, localCRC;

  IRQStatus = LT.readIrqStatus();                 //read the LoRa device IRQ status register

  if (isPacketValid()) {  // Verify that all bytes in the buffer after the first byte are 'a'.
    RXpacketCount++;
  } else {
    Serial.print("Packet value is invalid");
  }

  printElapsedTime();                             //print elapsed time to Serial Monitor
  Serial.print(F("  "));
  //LT.printASCIIPacket(RXBUFFER, RXPacketL);       //print the packet as ASCII characters

  // print float number recv
  // receivePayload(RXBUFFER);
  Serial.print("Recv id,");
  Serial.print(RXBUFFER[1]);
  localCRC = LT.CRCCCITT(RXBUFFER, RXPacketL, 0xFFFF);  //calculate the CRC, this is the external CRC calculation of the RXBUFFER
  Serial.print(F(",CRC,"));                       //contents, not the LoRa device internal CRC
  Serial.print(localCRC, HEX);
  Serial.print(F(",RSSI,"));
  Serial.print(PacketRSSI);
  Serial.print(F("dBm,SNR,"));
  Serial.print(PacketSNR);
  Serial.print(F("dB,Length,"));
  Serial.print(RXPacketL);
  Serial.print(F(",Packets,"));
  Serial.print(RXpacketCount);
  Serial.print(F(",Errors,"));
  Serial.print(errors);
  Serial.print(F(",IRQreg,"));
  Serial.print(IRQStatus, HEX);
}


void packet_is_Error()
{
  uint16_t IRQStatus;
  IRQStatus = LT.readIrqStatus();                   //read the LoRa device IRQ status register

  printElapsedTime();                               //print elapsed time to Serial Monitor

  if (IRQStatus & IRQ_RX_TIMEOUT)                   //check for an RX timeout
  {
    Serial.print(F(" RXTimeout"));
  }
  else
  {
    errors++;
    Serial.print(F(" PacketError"));
    Serial.print(F(",RSSI,"));
    Serial.print(PacketRSSI);
    Serial.print(F("dBm,SNR,"));
    Serial.print(PacketSNR);
    Serial.print(F("dB,Length,"));
    Serial.print(LT.readRXPacketL());               //get the device packet length
    Serial.print(F(",Packets,"));
    Serial.print(RXpacketCount);
    Serial.print(F(",Errors,"));
    Serial.print(errors);
    Serial.print(F(",IRQreg,"));
    Serial.print(IRQStatus, HEX);
    LT.printIrqStatus();                            //print the names of the IRQ registers set
  }

  delay(250);                                       //gives a longer buzzer and LED flash for error

}


void printElapsedTime()
{
  float seconds;
  seconds = millis() / 1000;
  Serial.print(seconds, 0);
  Serial.print(F("s"));
}


void led_Flash(uint16_t flashes, uint16_t delaymS)
{
  uint16_t index;

  for (index = 1; index <= flashes; index++)
  {
    digitalWrite(LED1, HIGH);
    delay(delaymS);
    digitalWrite(LED1, LOW);
    delay(delaymS);
  }
}