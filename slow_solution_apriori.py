import time
from collections import defaultdict
from itertools import combinations
import config


def load_data(path):
    """
    Wczytywanie danych
    """
    transactions = defaultdict(set)
    try:
        with open(path, 'r', encoding='ISO-8859-1') as f:
            next(f)
            for line in f:
                parts = line.strip().split(',')
                if len(parts) < 2: continue
                invoice, stock_code = parts[0], parts[1]
                if invoice.isdigit() and stock_code:
                    transactions[invoice].add(stock_code)
    except FileNotFoundError:
        return []
    return list(transactions.values())

def get_frequent_1_itemsets(transactions, min_support):
    """
    Generuje zbiory częste o liczności 1
    """
    counts = defaultdict(int)
    for transaction in transactions:
        for item in transaction:
            counts[item] += 1
    n = len(transactions)
    return {frozenset([item]): count / n for item, count in counts.items() if count / n >= min_support}

def generate_candidates(prev_frequent_itemsets, k):
    """
    Generuje kandydujące zbiory częste
    """
    candidates = set()
    list_frequent = list(prev_frequent_itemsets)
    for i in range(len(list_frequent)):
        for j in range(i + 1, len(list_frequent)):
            union = list_frequent[i] | list_frequent[j]
            if len(union) == k:
                candidates.add(union)
    return candidates

def generate_rules(frequent_itemsets, min_confidence):
    """
    Generuje reguły asocjacyjne na podstawie zbiorów częstych.
    frequent_itemsets: słownik {frozenset: support}
    """
    rules = []
    for itemset, support in frequent_itemsets.items():
        if len(itemset) > 1:
            # Dla każdego zbioru (np. {A, B, C}) generujemy pary podzbiorów
            # Na podstawie podzbiorów generujemy reguły, antecedent -> consequent (tylko jeżeli confidence > zadany)
            for i in range(1, len(itemset)):
                for antecedent in combinations(itemset, i):
                    antecedent = frozenset(antecedent)
                    consequent = itemset - antecedent
                    
                    # obliczamy Confidence = Support(A U B) / Support(A)
                    support_a = frequent_itemsets.get(antecedent)
                    if support_a:
                        confidence = support / support_a
                        if confidence >= min_confidence:
                            rules.append({
                                'A': set(antecedent),
                                'B': set(consequent),
                                'supp': support,
                                'conf': confidence,
                            })
    return rules

def solve(min_support, min_confidence, verbose = True):
    MIN_SUPPORT = min_support
    MIN_CONFIDENCE = min_confidence
    transactions = load_data(config.datapath)
    n_trans = len(transactions)
    # 1. Szukanie zbiorów częstych (Apriori)
    l1 = get_frequent_1_itemsets(transactions, MIN_SUPPORT)
    all_frequent = {**l1}
    current_frequent = l1
    
    k = 2
    while current_frequent:
        if not verbose:
            print(f'Przetwarzam poziom {k}')
        candidates = generate_candidates(current_frequent.keys(), k)
        if not candidates: break
        
        counts = defaultdict(int)
        if verbose:
            for trans in transactions:
                for cand in candidates:
                    if cand.issubset(trans):
                        counts[cand] += 1
        else:
            from tqdm import tqdm
            for trans in tqdm(transactions):
                for cand in candidates:
                    if cand.issubset(trans):
                        counts[cand] += 1
        current_frequent = {itemset: c / n_trans for itemset, c in counts.items() if c / n_trans >= MIN_SUPPORT}
        all_frequent.update(current_frequent)
        k += 1
    
    # 2. Generowanie reguł
    if not verbose:
        print(f"Generowanie reguł z {len(all_frequent)} zbiorów częstych...")
    rules = generate_rules(all_frequent, MIN_CONFIDENCE)
    if not verbose:
        print(f'Wygenerowano {len(rules)} reguł.')
        for rule in rules:
            print(f'{rule['A']}=>{rule['B']} Support: {rule['supp']}, Confidence: {rule['conf']}')
    return rules
if __name__ == "__main__":
    solve(config.min_support, config.min_confidence, verbose=False)