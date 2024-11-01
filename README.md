# Lora2.4-testbed

One gateway with 4 channels, each assigned to one transceiver: 3 for receiving (Rx) and 1 for transmitting (Tx). \
The testbed uses LILYGO TTGO v1.2 and Heltec LoRa v2 boards.


## Setup

1. Configure the router's DHCP to assign static IPs to both the gateway and the end devices. 
2. Use assets.txt to provide specific settings for each device. 
3. Initiate experiments using the script on a PC with the devices listed in assets.txt. 


## Usage

- To start a new experiment, use the following command:
  ```bash
  python3 init_exp_f.py <experiment_folder>

## License
This project is licensed under the GNU General Public License - see the LICENSE.md file for details.

## Credits

The lora2.4-testbed project is developed and maintained by the NU IoT Lab.
