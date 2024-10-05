# Lora2.4-testbed

Custom gateway with 4 channels, each assigned to one transceiver: 3 for receiving (Rx) and 1 for transmitting (Tx). \\
The testbed uses LILYGO TTGO v1.2 and Heltec LoRa v2 boards.


## Setup

Configure the router's DHCP to assign static IPs to both the gateway and the end devices.
Use assets.txt to provide specific settings for each device.
Initiate experiments using the script on a PC with the devices listed in assets.txt.
Port 8080: Used for experiment initialization and statistics.


## Usage

- To start a new experiment, use the following command:
  ```bash
  python3 init_exp_f.py <experiment_folder>
