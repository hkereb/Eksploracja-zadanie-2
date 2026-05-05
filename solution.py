import config
import c_eclat 

def solve(min_support, min_confidence, verbose=False):
    with open('data/config.py', 'r') as f:
        datapath = [line.split('=')[1].strip().strip("'\"") for line in f if 'datapath' in line][0]
    
    if verbose: print(f"Datapath: {datapath}")
    return c_eclat.solve_cpp(datapath, min_support, min_confidence, verbose)

if __name__ == "__main__":
    solve(config.min_support, config.min_confidence, verbose=True)