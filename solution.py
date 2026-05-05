import config
import c_eclat 

def solve(min_support, min_confidence, verbose=False):
    datapath = config.datapath
    
    if verbose: print(f"Datapath: {datapath}")
    return c_eclat.solve_cpp(datapath, min_support, min_confidence, verbose)

if __name__ == "__main__":
    solve(config.min_support, config.min_confidence, verbose=True)
