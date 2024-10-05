# python fair_calculation.py ../results_exp/linear/0.9/a.txt ../results_exp/linear/0.9/b.txt ../results_exp/linear/0.9/c.txt    
# python fair_calculation.py ../results_exp/linear/0.1/a.txt ../results_exp/linear/0.1/b.txt ../results_exp/linear/0.1/c.txt    
import numpy as np
import sys
import re
import glob

#--------------------------------CALCULATE EXPERIMENTS--------------------------------
def count_experiment_starts_and_invalids(file_path):
    exp_start_count = 0
    invalid_exp_count = 0  
    # print(f"Processing file: {file_path}")
    with open(file_path, 'r') as file:
        for line in file:
            if "Experiment is started!" in line:
                exp_start_count += 1
            if "Processing message: [ERR] ED TIMEOUT" in line:
                invalid_exp_count += 1
    
    return exp_start_count, invalid_exp_count

#--------------------------------PARSE EACH LOG FILE--------------------------------
def parse_log_file(file_path):
    experiments = list()
    current_experiment = {i: 0 for i in range(16)} 
    is_exp_valid = True
    with open(file_path, "r") as file_to_read:
        # print(f"Processing file: {file_path}")
        # print(line)
        for line in file_to_read:
            if "Experiment is started!" in line: 
                if any(current_experiment.values()) and is_exp_valid:
                    experiments.append(current_experiment)
                current_experiment = {i: 0 for i in range(16)}
                is_exp_valid = True
                
            elif "Processing message: [ERR] ED TIMEOUT" in line:
                is_exp_valid = False
            match = re.search(r'id,(\d+)', line) 
            if match:
                end_device_id = int(match.group(1))  
                if 0 <= end_device_id <= 15:
                    current_experiment[end_device_id] += 1
    if any(value > 0 for value in current_experiment.values()):
        experiments.append(current_experiment)  
    # print()
    # print(current_experiment)

    # print("\n")
    return experiments


def pars_config(file_path):
    map_of_letters = {'A': [], 'B': [], 'C': []}
    with open(file_path, 'r') as file:
        for _ in range(3):
            next(file)
        for line in file:
            line = line.strip()
            if line:  
                index_str, letter = line.split()
                index = int(index_str)
                map_of_letters[letter].append(index)
    # print(map_of_letters, file_path)

    return map_of_letters


#--------------------------------CALCULATE PRR AND ST DEV WITH FAIRNESS--------------------------------
def calculate_prr_and_st_dev(experiments):
    i = 0 # for specifying specifying algorithm; first [0; 24] is BT
    all_prr_values = []
    std_deviations = []
    for experiment in experiments:
        prr_values = [count / 100 for count in experiment.values()]
        all_prr_values.extend(prr_values)
        if len(prr_values) > 1:  
            std_dev = np.std(prr_values, ddof=1)  
            std_deviations.append(std_dev)
    if all_prr_values:
        avg_prr = np.mean(all_prr_values)
        print(f"Average PRR across all experiments: {avg_prr:.4f}")
    else:
        print("No PRR values to calculate an average.")
    if len(std_deviations) >= 1:
        avg_std_dev = np.mean(std_deviations)  
        print(f"Average Standard Deviation of PRR across all experiments: {avg_std_dev:.4f}")
    else:
        print(f"Less than 1 experiments found. Only {len(std_deviations)} experiments were processed.")
    # print(all_prr_values)

#--------------------------------MAIN CODE--------------------------------
def main():
    if len(sys.argv) != 4:
        print("usage: python prr_calc.py <file_name_1> <file_name_2> <file_name_3>")
        sys.exit(1)  
    file_paths = sys.argv[1:]
    # pars_config(file_path="../exp_16EDs/sim_configLP_2_1_0.1.txt")
    total_exp_start_count, total_invalid_exp_count = count_experiment_starts_and_invalids(file_path=file_paths[0])
    experiments_tuple = tuple()
    config_file_paths = sorted(glob.glob("../exp_16EDs/sim_configLP_2_*_0.9.txt"))
    
    
    config_mappings = []
    for config_file_path in config_file_paths:
        mapping = pars_config(file_path=config_file_path)
        config_mappings.append(mapping)
    
    num_runs = 25  # Total number of runs
    num_configs = len(config_mappings)
    runs_per_config = num_runs // num_configs
    remainder = num_runs % num_configs
    
    # Associate configurations with runs
    config_mappings_for_runs = []
    for i, mapping in enumerate(config_mappings):
        count = runs_per_config + (1 if i < remainder else 0)
        config_mappings_for_runs.extend([mapping] * count)
    
    experiments_for_all_A = []
    experiments_for_all_B = []
    experiments_for_all_C = []

    for file_path in file_paths:
        if "a.txt" == file_path.split("/")[-1]:
            # print("Gateway A")
            experiments_for_all_A = parse_log_file(file_path=file_path)
        
        if "b.txt" == file_path.split("/")[-1]:
            # print("Gateway B")
            experiments_for_all_B = parse_log_file(file_path=file_path)
        
        if "c.txt" == file_path.split("/")[-1]:
            # print("Gateway C")
            experiments_for_all_C = parse_log_file(file_path=file_path)

    fairness = []
    prr = []

    for i in range(25):
        if i >= len(experiments_for_all_A) or i >= len(experiments_for_all_B) or i >= len(experiments_for_all_C):
            print(f"Skipping RUN {i} due to insufficient experiments.")
            continue
        sumRcvPkt_A = sum(experiments_for_all_A[i].values())
        sumRcvPkt_B = sum(experiments_for_all_B[i].values())
        sumRcvPkt_C = sum(experiments_for_all_C[i].values())

        map_of_letters = config_mappings_for_runs[i]
        print(map_of_letters)
        print(f"RUN {i}")
        print("A: ", experiments_for_all_A[i])
        print("B: ", experiments_for_all_B[i])
        print("C: ", experiments_for_all_C[i])
        print(f"SUM RCV       A: {sumRcvPkt_A}  B: {sumRcvPkt_B}    C: {sumRcvPkt_C}, Total: {sumRcvPkt_A + sumRcvPkt_B + sumRcvPkt_C}")
        print(f"SUM SND       A: {len(map_of_letters['A'])*100}  B: {len(map_of_letters['B'])*100}    C: {len(map_of_letters['C'])*100}, Total: {len(map_of_letters['A'])*100 + len(map_of_letters['B'])*100 + len(map_of_letters['C'])*100}")
        
        summed_dict = {}
        for key in experiments_for_all_A[i]:
            summed_dict[key] = experiments_for_all_A[i][key] + experiments_for_all_B[i][key] + experiments_for_all_C[i][key]
        
        print("SUM: ", summed_dict)
        prr_run = (sumRcvPkt_A + sumRcvPkt_B + sumRcvPkt_C) / (len(map_of_letters['A']) * 100 + len(map_of_letters['B']) * 100 + len(map_of_letters['C']) * 100) * 100
        prr.append(prr_run)
        print(f"PRR RUN: {prr_run:.3f}%")
        
        # Calculate PRR for A, B, and C
        prr_A = sumRcvPkt_A / (len(map_of_letters['A']) * 100) * 100
        prr_B = sumRcvPkt_B / (len(map_of_letters['B']) * 100) * 100
        prr_C = sumRcvPkt_C / (len(map_of_letters['C']) * 100) * 100
        
        print(f"PRR A: {prr_A:.2f}  B: {prr_B:.2f}  C: {prr_C:.2f}")
        
        prr_A_values = [experiments_for_all_A[i].get(key, 0) / 100 for key in experiments_for_all_A[i]]
        prr_B_values = [experiments_for_all_B[i].get(key, 0) / 100 for key in experiments_for_all_B[i]]
        prr_C_values = [experiments_for_all_C[i].get(key, 0) / 100 for key in experiments_for_all_C[i]]

        std_prr_A = np.std(prr_A_values)
        std_prr_B = np.std(prr_B_values)
        std_prr_C = np.std(prr_C_values)
        
        fair = max(std_prr_A, std_prr_B, std_prr_C) - min(std_prr_A, std_prr_B, std_prr_C)
        print(f"Standard Deviation of PRR for RUN {i} - A: {std_prr_A:.2f}, B: {std_prr_B:.2f}, C: {std_prr_C:.2f}")
        print(f"Fairness: {fair:.3f}")
        fairness.append(fair)
        print("\n")
    


    print("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n")
    print(file_paths)
    print(f"Average Fairness: {np.mean(fairness):.3f}")
    print(f"Average PRR: {np.mean(prr)/100:.3f}")
    # 
    
    
if __name__ == "__main__":
    main()