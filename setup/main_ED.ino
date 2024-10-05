/*******************************************************************************************************
  Programs for Arduino - Copyright of the author Stuart Robinson - 02/03/20

  This program is supplied as is, it is up to the user of the program to decide if the program is
  suitable for the intended purpose and free from errors.
*******************************************************************************************************/
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>                                               //the lora device is SPI based so load the SPI library                                         
#include <SX128XLT.h>                                          //include the appropriate library  
#include <string.h>
#include <cstdlib> // For rand() and srand()
#include <ctime>
#include <Adafruit_NeoPixel.h>
#define Program_Version "V1.1"

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
#define NEO_DATA_PIN 0                           // GPIO0 for NeoPixel data
#define NEO_POWER_PIN 32                         // GPIO32 for controlling NeoPixel power
#define NUM_PIXELS 1                             

//*******  Setup LoRa Parameters Here ! ***************
//LoRa Modem Parameters
const uint32_t Frequency = 2445000000;           //frequency of transmissions
const int32_t Offset = 0;                        //offset frequency for calibration purposes
const uint8_t Bandwidth = LORA_BW_0800;          //LoRa bandwidth
const uint8_t SpreadingFactor = LORA_SF7;        //LoRa spreading factor
const uint8_t CodeRate = LORA_CR_4_5;            //LoRa coding rate
const int8_t TXpower = 10;                       //LoRa transmit power in dBm
const uint16_t packet_delay = 1000;             //mS delay between packets
uint8_t TXPacketL;
uint32_t TXPacketCount, startmS, endmS;
SX128XLT LT;                                     //create a library class instance called LT

//*******  Setup WiFi and Socket params here! ***************
const char* ssid = "IoTlab_Public";
const char* password = "iotlabwifi";
const int udpPort = 12345;
WiFiUDP udp;
WiFiServer server(80);
WiFiClient pcClient;
IPAddress pcIP;
uint16_t targetPort = 8080;
uint8_t *buff = nullptr;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEO_DATA_PIN, NEO_GRB + NEO_KHZ800);


void processMessage(String message);
void packet_is_OK();
void packet_is_Error();
void setup();
void setNeoPixelColor(uint8_t red, uint8_t green, uint8_t blue);
void printElapsedTime();
bool SetUpLoRA(int SF, int frame_size, double payload);

const String init_com = "START";
bool exp_init = false;
bool setup_init = false;
char incomingUDPPacket[255];
int id, sf, frame_size, pack_num;
double payload;
char gateway_label;
const int maxDelayTime = 5000; // 5 seconds // Random delay max time (in milliseconds)
const int wifiMaxDelayTime = 120000;
unsigned long lastTransmissionTime = 0;
int currentPacketNumber = 0;
int initialDelay;
int wifiDelay;

//***************************************************************************************************

void setup() {
  //***************************************************************************************************
  //Setup WiFi device
  //***************************************************************************************************
  Serial.begin(115200);

  pinMode(NEO_POWER_PIN, OUTPUT);
  digitalWrite(NEO_POWER_PIN, HIGH); 
  
  strip.begin();
  strip.setBrightness(50);               // Set brightness to 50%
  strip.show();

  wifiDelay = rand() % wifiMaxDelayTime;
  delay(wifiDelay);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    setNeoPixelColor(255, 0, 0);         // RED
    delay(1000);
  }

   // Start the client
  server.begin();
  Serial.println("started");

  udp.begin(udpPort);

  Serial.print("Connected to Wi-Fi SSID: ");
  Serial.println(ssid);
  setNeoPixelColor(0, 255, 0);   //GREEN
  Serial.print("Connected to Wi-Fi. IP address: ");
  Serial.println(WiFi.localIP());
  String macAddress = WiFi.macAddress();
  Serial.print("MAC Address: ");
  Serial.println(macAddress);

  
  
  
  //***************************************************************************************************
  //Setup LoRa device
  //***************************************************************************************************
  Serial.println();
  Serial.println(F("104_LoRa_Receiver_Detailed_Setup_ESP32 Starting"));
  Serial.println();
  
  //SPI.begin();                                          //default setup can be used be used
  SPI.begin(SCK, MISO, MOSI, NSS);                        //alternative format for SPI3, VSPI; SPI.begin(SCK,MISO,MOSI,SS)
  // Initilizing SPI for LoRa

  //SPI beginTranscation is normally part of library routines, but if it is disabled in the library
  //a single instance is needed here, so uncomment the program line below
  //SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

  //setup hardware pins used by device, then check if device is found
  if (LT.begin(NSS, NRESET, RFBUSY, DIO1, LORA_DEVICE))
  {
    Serial.println(F("LoRa Device found"));
    setNeoPixelColor(0, 255, 0);
    delay(1000);
  }
  else
  {
    Serial.println(F("No device responding"));
    while (1)
    {
      setNeoPixelColor(0, 0, 255);  // BLUE
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
  LT.printModemSettings();                               //reads and prints the configured LoRa settings, useful check
  Serial.println();
  LT.printOperatingSettings();                           //reads and prints the configured operating settings, useful check
  Serial.println();
  Serial.println();
  LT.printRegisters(0x900, 0x9FF);                       //print contents of device registers, normally 0x900 to 0x9FF
  Serial.println();
  Serial.println();

  Serial.print(F("Transmitter ready"));
  Serial.println();

  // Initialize random seed
  srand(time(NULL)); // Seed the random number generator
}

void loop() {
  //***************************************************************************************************
  //GET PARAMS
  //***************************************************************************************************
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost, restarting...");

    delay(1000);  // Optionally wait before resetting
    ESP.restart();  // Reset the device to trigger reconnection in setup()
    setNeoPixelColor(255, 0, 0);  // Red if disconnected
  } else {
    setNeoPixelColor(0, 255, 0);  // Green if connected
  }

  WiFiClient client = server.available();
  if (client) {
    setNeoPixelColor(0, 255, 0);   //GREEN
    Serial.println("New client connected");
    IPAddress pcIP = client.remoteIP(); // Get the IP address of the client

    // Print the client's IP address
    Serial.print("Client IP address: ");
    Serial.println(pcIP);
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

    // Unpack and process the message
    processMessage(message);
    
    // Send response to the client
    client.println("message_received");
    
    // Close the connection
    client.stop();
    Serial.println("\nClient disconnected");
    Serial.println("Completed task, now disconnecting WiFi.");
    //WiFi.disconnect(); 
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
      setup_init = false;

      // Initialize random delay for the first packet
      initialDelay = rand() % maxDelayTime;
      lastTransmissionTime = millis() - initialDelay; // Adjust time to simulate initial delay
      currentPacketNumber = 0;
      TXPacketCount = 0; 
    } else {
      Serial.println("Wrong Command");
    }
  }

  //***************************************************************************************************
  //EXP IS STARTED
  //***************************************************************************************************
  if (exp_init){
    while (currentPacketNumber < 100) { 
      setNeoPixelColor(0, 255, 0);
      unsigned long currentTime = millis(); // Get current time
      if (currentTime - lastTransmissionTime >= initialDelay) {
        delay(initialDelay);
        startmS =  millis();         //start transmit timer
        
        Serial.println("=========================================");

        TXPacketL = pack_num;                          
        if (LT.transmit(buff, TXPacketL, 20000, TXpower, WAIT_TX))  //will return packet length sent if OK, otherwise 0 if transmit error
        {
          endmS = millis();                                          //packet sent, note end time
          TXPacketCount++;
          packet_is_OK();
        } else {
          packet_is_Error();                                 //transmit packet returned 0, there was an error
        }
        // Update last transmission time and packet number
        lastTransmissionTime = currentTime;
        initialDelay = rand() % maxDelayTime; // Randomize delay for the next packet
        currentPacketNumber++;
      }
    }

    // connect to pc via ip address 
    pcIP = IPAddress(192, 168, 0, 106);
    if ( pcClient.connect(pcIP, targetPort) ) {
      Serial.println("Connected to PC or server.");
      // Generate a random delay between 1 and 60 seconds (1000 to 60000 milliseconds)
      unsigned long randomDelay = random(1000, 20000);
      Serial.print("Waiting for ");
      Serial.print(randomDelay / 1000); // Convert milliseconds to seconds for display
      Serial.println(" seconds...");

      delay(randomDelay); // Wait for the random delay

      pcClient.println("READY:" + String(id) + ":" + String(TXPacketCount));

      Serial.println("Sent completion message to server");

    } else {
      Serial.println("Connection to PC or server failed.");
    }
    setNeoPixelColor(255, 0, 255);
    pcClient.stop(); 
    Serial.println("\nClient disconnected");
    exp_init = false;
    
  }
}

void processMessage(String message) {
  // Trim the message
  message.trim(); 
  Serial.println("Received message: " + message);  // 9 7 48 A
  
  // Parse the message
  int space1 = message.indexOf(' ');
  int space2 = message.indexOf(' ', space1 + 1);
  int space3 = message.indexOf(' ', space2 + 1);

  if (space1 > 0 && space2 > 0 && space3 > 0) {
    id = message.substring(0, space1).toInt(); // Get the first character as the ID 9
    sf = message.substring(space1 + 1, space2).toInt();
    pack_num = (message.substring(space2 + 1, space3).toInt()) - 4; // ==packet_size 
    if (pack_num > 255) {
      pack_num = 255;
    }
    gateway_label = message.charAt(message.length()-1); // Get the last character
    
    Serial.println("===================FREQUENCY===================");
    Serial.println("Gateway label: " + String(gateway_label));
    // Set frequency based on gateway label
    switch (gateway_label) {
      case 'A':
          LT.setRfFrequency(2405000000, Offset); 
          break;
      case 'B':
          LT.setRfFrequency(2410000000, Offset);
          break;
      case 'C':
          LT.setRfFrequency(2415000000, Offset);
          break;
      default:
          Serial.println("Invalid gateway label!");
          return;
    }
    Serial.print("Frequency set for gateway ");
    Serial.print(gateway_label);
    Serial.print(" with offset ");
    Serial.println(Offset);
    Serial.println("=========================================");

    Serial.print("ID: ");
    Serial.println(id);
    Serial.print("Spreading Factor: ");
    Serial.println(sf);
    Serial.print("Packet Number: ");
    Serial.println(pack_num);

    if (buff != nullptr) {
        delete[] buff;
    }

    // Allocate new memory for the buffer
    buff = new uint8_t[pack_num];

    if (buff != nullptr) {
        buff[0] = pack_num; // First byte is the packet size
        buff[1] = id; // id 
        for (size_t index = 2; index < pack_num; ++index) {
            buff[index] = 'a'; // Hexadecimal value of 'a' * (pack_num -2)
        }
    } else {
        Serial.println("Memory allocation failed");
    }
    LT.printASCIIPacket(buff, pack_num);
    // Example actions based on the parsed data
    if (id >= 0 && id <= 100) { // Assuming ID ranges from 0 to 11
      setup_init = SetUpLoRA(sf); 
    } else {
      Serial.println("Invalid message format");
    }
  } else {
    Serial.println("Space index below or equal to zero");
  }
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

void packet_is_OK(){ //if here packet has been sent OK
  uint16_t localCRC;
  Serial.print(F("  BytesSent,"));
  Serial.print(TXPacketL);                             //print transmitted packet length
  localCRC = LT.CRCCCITT(buff, TXPacketL, 0xFFFF);
  Serial.print(F("  CRC,"));
  Serial.print(localCRC, HEX);                         //print CRC of transmitted packet
  Serial.print(F("  TransmitTime,"));
  Serial.print(endmS - startmS);                       //print transmit time of packet
  Serial.print(F("mS"));
  Serial.print(F("  PacketsSent,"));
  Serial.println(TXPacketCount);                         //print total of packets sent OK
}


void packet_is_Error(){ //if here there was an error transmitting packet
  uint16_t IRQStatus;
  IRQStatus = LT.readIrqStatus();                      //read the the interrupt register
  Serial.print(F(" SendError,"));
  Serial.print(F("Length,"));
  Serial.print(TXPacketL);                             //print transmitted packet length
  Serial.print(F(",IRQreg,"));
  Serial.print(IRQStatus, HEX);                        //print IRQ status
  LT.printIrqStatus();                                 //prints the text of which IRQs set
}


void printElapsedTime(){
  float seconds;
  seconds = millis() / 1000;
  Serial.print(seconds, 0);
  Serial.print(F("s"));
}

void setNeoPixelColor(uint8_t red, uint8_t green, uint8_t blue) {
  strip.setPixelColor(0, strip.Color(red, green, blue)); 
  strip.show();                                          
}