import time
import statistics
import pandas as pd
from tabulate import tabulate 
import slow_solution_apriori
import slow_solution_fpgrowth
import solution


def benchmark_solution(solution_module, name, iterations=5, support=0.03, confidence=0.5):
    print(f"Rozpoczynam testy dla: {name}...")
    times = []
    
    for i in range(1, iterations + 1):
        start_time = time.perf_counter()
        result = solution_module.solve(support, confidence, verbose=False)
        end_time = time.perf_counter()
        
        duration = end_time - start_time
        times.append(duration)
        print(f"  Przebieg {i}/{iterations}: {duration:.4f}s (Znaleziono {len(result)} reguł)")
        
    return {
        "Algorytm": name,
        "Średnia [s]": round(statistics.mean(times), 4),
        "Min [s]": round(min(times), 4),
        "Max [s]": round(max(times), 4),
        "Odchylenie std.": round(statistics.stdev(times), 4) if len(times) > 1 else 0
    }

def run_comparison():
    import config
    MIN_SUPPORT = config.min_support 
    MIN_CONFIDENCE = config.min_confidence
    ITERATIONS = config.test_iters

    print("="*50)
    print(f"BENCHMARK: Apriori vs FP-Growth (n={ITERATIONS})")
    print(f"Parametry: Support={MIN_SUPPORT}, Confidence={MIN_CONFIDENCE}")
    print("="*50)

    results = []

    # Test Apriori
    try:
        apriori_stats = benchmark_solution(slow_solution_apriori, "Apriori (Slow)", ITERATIONS, MIN_SUPPORT, MIN_CONFIDENCE)
        results.append(apriori_stats)
    except Exception as e:
        print(f"Błąd podczas testu Apriori: {e}")

    print("-" * 30)

    # Test FP-Growth
    try:
        fpgrowth_stats = benchmark_solution(slow_solution_fpgrowth, "FP-Growth (Slow)", ITERATIONS, MIN_SUPPORT, MIN_CONFIDENCE)
        results.append(fpgrowth_stats)
    except Exception as e:
        print(f"Błąd podczas testu FP-Growth: {e}")

    # Test Eclat (C++)
    try:
        fast_stats = benchmark_solution(solution, "Eclat", ITERATIONS, MIN_SUPPORT, MIN_CONFIDENCE)
        results.append(fast_stats)
    except Exception as e:
        print(f"Błąd podczas testu Eclat: {e}")

    # Wyświetlenie wyników
    print("\n" + "="*60)
    print("PODSUMOWANIE PORÓWNANIA")
    print("="*60)
    
    df_results = pd.DataFrame(results)
    print(tabulate(df_results, headers='keys', tablefmt='grid', showindex=False))

if __name__ == "__main__":
    run_comparison()