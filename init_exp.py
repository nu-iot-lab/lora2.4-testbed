import socket
import random, time
import os
import re
import threading
import telebot
import sys
from datetime import datetime

BOT_TOKEN = os.environ.get('BOT_TOKEN')
bot = telebot.TeleBot(BOT_TOKEN)

PORT = 80
EXP_FOLDER="/home/hanush/Desktop/lora2.4-gwconf/experiments/16EDs"
LOG_CHAT_IDS = []
EXP_ITERS_NUM = 5

BROADCAST_IP = "255.255.255.255"
BROADCAST_PORT = 12345
BROADCAST_MESSAGE = "START"

ED_END_MESSAGE = "READY"
MAX_END_MESSAGES = 16

SERVER_PORT = 8080
TIMEOUT = 1200
ERRORS = {0:"[ERR] ED TIMEOUT\n"}

MAX_RETRIES = 5
RETRY_DELAY = 1

#--------------------------------ED and GW parsing-------------------------------
def pars_config(file_path):
    gw_sf_mapping = {}
    ed_sf_mapping = {}
    with open(file_path, 'r') as config_file:
        lines = config_file.readlines()
        for line in lines:
            key, value = line.strip().split()
            if key.isalpha():
                gw_sf_mapping[key] = int(value)
            else:
                ed_sf_mapping[int(key)] = gw_sf_mapping[value]
    return gw_sf_mapping, ed_sf_mapping

#--------------------------------Payload parsing-------------------------------
def parse_payload(file_path):
    ed_payloads_length = {}
    with open(file_path, 'r') as payload_file:
        lines = payload_file.readlines()[1:] 
        for line in lines:
            parts = line.split()
            ed_index = int(parts[0])
            length = parts[-1]
            ed_payloads_length[ed_index] = length
    return ed_payloads_length

#--------------------------------GET GW LABEL-------------------------------
def get_GW_label(ED_SF, gw_sf_mapping):
    our_id = None
    for id in gw_sf_mapping.keys():
        if ED_SF == gw_sf_mapping[id]:
            our_id = id
    return our_id

#--------------------------------ERROR MESSAGE-------------------------------
def send_error(err_num, IP):
    message = ERRORS[err_num]
    retry_attempts = 0
    connected = False
    while retry_attempts < MAX_RETRIES and not connected:
        try:
            client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client_socket.connect((IP, PORT))
            connected = True
            print(f"Connected to {IP}:{PORT}")
    
            client_socket.sendall(message.encode())
            print(f"Sent message: {message.strip()}")
            
            response = client_socket.recv(1024)
            print(f"Received response: {response.decode().strip()}")  
        except Exception as e:
            retry_attempts += 1
            print(f"An error occurred: {e}")
            print(f"Retrying in {RETRY_DELAY} seconds...")
            time.sleep(RETRY_DELAY) 
        finally:
            client_socket.close()
            print("Connection closed")

#--------------------------------SEND MESSAGE TO ED AND GW-------------------------------
def send_message(IP, id, sf, pack_length=None, GW_label=None):
    response = ""
    message = f"{id} {sf}\n"
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)    
    try:
        # Connect to the ESP32 server
        client_socket.connect((IP, PORT))
        print(f"Connected to {IP}:{PORT}")
        
        # Send the message
        if not pack_length:
            client_socket.sendall(message.encode())
            print(f"Sent message: {message.strip()}")
            
            # Receive the response from the server
            response = client_socket.recv(1024)
            print(f"Received response: {response.decode().strip()}")
        else:
            message = f"{id} {sf} {pack_length} {GW_label}\n"
            client_socket.sendall(message.encode())
            print(f"Sent message: {message.strip()}")
            
            # Receive the response from the server
            response = client_socket.recv(1024)
            print(f"Received response: {response.decode().strip()}")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        client_socket.close()
        print("Connection closed")
        return response
    
#--------------------------------PARAMETERS-------------------------------    
def send_params(assets, gw_sf_mapping, ed_sf_mapping, ed_payloads_length):
    init = random.randint(1, 65535)
    print("\nNew experiment with id:", init)
    for asset in assets:
        print(asset)
        items = asset.split(" ")
        IP = items[3]
        ID = items[1]
        
        if (items[0] == 'GW'):
            SF = gw_sf_mapping[ID] 
            resp = send_message(IP, ID, SF)
            
        elif (items[0] == 'ED'):
            SF = ed_sf_mapping[int(ID)]
            pack_length = ed_payloads_length[int(ID)]
            GW_label = get_GW_label(SF, gw_sf_mapping) 
            resp = send_message(IP, ID, SF, pack_length=pack_length, GW_label=GW_label)
        else:
            continue
      
#--------------------------------GET FILE PATHS PAYLOAD AND CONFIG*-------------------------------    
def get_param_pathes(experiments_folder):
    # Define patterns for configED, configBT, and payload (test) files
    configED_pattern = re.compile(r'sim_configED_(\d+_\d)\-0.1.txt')
    configBT_pattern = re.compile(r'sim_configBT_(\d+_\d)\-0.1.txt')
    configST_pattern = re.compile(r'sim_configST_(\d+_\d)\.txt')
    payload_pattern = re.compile(r'test(\d+_\d)\.txt')
    
    # Store files in dictionaries with logical numbers as keys
    configED_files = {}
    configBT_files = {}
    configST_files = {}
    payload_files = {}

# Iterate over the files in the experiments folder
    for root, dirs, files in os.walk(experiments_folder):
        for file in files:
            configED_match = configED_pattern.match(file)
            configBT_match = configBT_pattern.match(file)
            configST_match = configST_pattern.match(file)
            payload_match = payload_pattern.match(file)
            
            if configED_match:
                logical_number = int(configED_match.group(1))
                if logical_number not in configED_files:
                    configED_files[logical_number] = []
                    configED_files[logical_number].append(os.path.join(root, file))
            
            if configBT_match:
                logical_number = int(configBT_match.group(1))
                if logical_number not in configBT_files:
                    configBT_files[logical_number] = []
                    configBT_files[logical_number].append(os.path.join(root, file))
            
            if configST_match:
                logical_number = int(configST_match.group(1))
                if logical_number not in configST_files:
                    configST_files[logical_number] = []
                    configST_files[logical_number].append(os.path.join(root, file))
                
            if payload_match:
                logical_number = int(payload_match.group(1))
                if logical_number not in payload_files:
                    payload_files[logical_number] = []
                payload_files[logical_number].append(os.path.join(root, file))

    
    # # Yield pairs of configBT and payload files with the same logical identifier
    # for logical_identifier in sorted(configBT_files.keys()):
    #     if logical_identifier in payload_files:
    #         yield configBT_files[logical_identifier][0], payload_files[logical_identifier][0]
    
    # Yield pairs of configED and payload files with the same logical identifier
    for logical_identifier in sorted(configED_files.keys()):
        if logical_identifier in payload_files:
            yield configED_files[logical_identifier][0], payload_files[logical_identifier][0]

    #     # Yield pairs of configED and payload files with the same logical identifier
    # for logical_identifier in sorted(configST_files.keys()):
    #     if logical_identifier in payload_files:
    #         yield configST_files[logical_identifier][0], payload_files[logical_identifier][0]

def get_ip_adresses(assets):
    end_device_ips = []
    for asset in assets:
        items = asset.split(" ")
        if items[0] == "ED":
            end_device_ips.append(items[3])
    return end_device_ips

####################################################################
# TELEGRAM BOT
####################################################################
def bot_response(log_entry):
    for chat_id in LOG_CHAT_IDS:
        bot.send_message(chat_id, get_wt(log_entry))

# Define a simple handler for the /start command
@bot.message_handler(commands=['start'])
def send_welcome(message):
    bot.reply_to(message, "Welcome! Send /setlog to start receiving log messages.")

# Define a handler for the /setlog command to set the chat ID for logging
@bot.message_handler(commands=['setlog'])
def set_log_chat(message):
    global LOG_CHAT_IDS
    chat_id = message.chat.id
    if chat_id not in LOG_CHAT_IDS:
        LOG_CHAT_IDS.append(chat_id)
        bot.reply_to(message, "You will now receive log messages in this chat.")
    else:
        bot.reply_to(message, "This chat is already receiving log messages.")

# Function to start infinity_polling
def start_polling():
    bot.infinity_polling()

def get_wt(msg):
    ''' 
    get message with current date and time 
    '''
    current_date_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    return f'[{current_date_time}][SERVER] {msg}'

####################################################################
# MAIN
####################################################################
if __name__ == "__main__":
    
    if len(sys.argv) != 2:
        print("usage: python init.py <file_name>")
        sys.exit(1)
        
    EXP_FOLDER = sys.argv[1]
    print("Experiment folder is ", EXP_FOLDER)
    assets = None
    polling_thread = threading.Thread(target=start_polling)
    polling_thread.start()

    print("30 secs to connect to bot")
    
    with open("assets.txt") as file:
        next(file)
        assets = [l.rstrip() for l in file]
        end_device_ips = get_ip_adresses(assets)

    # Create a UDP socket
    broadcast_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Enable the option to broadcast messages
    broadcast_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    for config_file_path, payload_file_path in get_param_pathes(experiments_folder=EXP_FOLDER):
        i = 0
        while i != EXP_ITERS_NUM:
            print(config_file_path)
            gw_sf_mapping, ed_sf_mapping = pars_config(config_file_path)
            ed_payloads_length = parse_payload(payload_file_path)
            send_params(assets, gw_sf_mapping, ed_sf_mapping, ed_payloads_length)
            #print(payload_file_path)
            bot_response(f"Experiment for {os.path.basename(config_file_path)} is started, #{i}")

            # Send the broadcast message
            broadcast_sock.sendto(BROADCAST_MESSAGE.encode(), (BROADCAST_IP, BROADCAST_PORT))
            print(f"broadcast message: {BROADCAST_MESSAGE}")
            bot_response("\nWaiting for statistics from EACH ED\n")
            # Create a socket object with IPv4 and TCP
            server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            # Bind the socket to the specified host and port
            server_socket.bind(('192.168.0.106', SERVER_PORT))

            # Listen for incoming connections (backlog of 5)
            server_socket.listen(12)
            # Set a timeout for the socket
            server_socket.settimeout(TIMEOUT)
            end_messages = 0
            TotalTXPacketCount = 0

            while end_messages != MAX_END_MESSAGES:
                try:
                    # Accept a new client connection
                    client_socket, client_address = server_socket.accept()
                    bot_response(f"Connection from {client_address}")

                    # Receive data from the client
                    data = client_socket.recv(1024).decode('utf-8')
                    status, id, count = data.split(":")
                    bot_response(f"Received message: {status} from dev:{id}, TXpack number:{count}")

                    if status == "READY":
                        end_messages += 1
                        TXPacketCount = int(count)
                        TotalTXPacketCount += TXPacketCount
                        print(f"Dev_id: {id}, TXPacketCount: {TXPacketCount}")

                    #bot_response(f"Received message: {data}")
                    client_socket.close()
                except socket.timeout:
                    bot_response(f"Timeout occurred. No connection received in time {TIMEOUT}s.")
                    i = i - 1
                    for asset in assets:
                        items = asset.split(" ")
                        IP = items[3]
                        if (items[0] == 'GW'):
                            send_error(0, IP)
                    break
                except Exception as e:
                    bot_response(f"Error: {e}")
                    break
            # Close the server socket when done
            server_socket.close()
            i = i + 1
            print(f"TotalTXPacketCount:{TotalTXPacketCount}")
            bot_response(f"TotalTXPacketCount:{TotalTXPacketCount}")