from collections import defaultdict
import config
class FPNode:
    def __init__(self, item, count, parent):
        self.item = item
        self.count = count
        self.parent = parent
        self.children = {}
        self.next_link = None

def solve(min_support, min_confidence, verbose=False):
    # 1. Ładowanie i przygotowanie danych
    raw_transactions = []
    try:
        with open(config.datapath, 'r', encoding='ISO-8859-1') as f:
            next(f)
            trans_dict = defaultdict(list)
            for line in f:
                parts = line.strip().split(',')
                if len(parts) >= 2 and parts[0].isdigit():
                    trans_dict[parts[0]].append(parts[1])
            raw_transactions = list(trans_dict.values())
    except Exception:
        raw_transactions = []

    n_trans = len(raw_transactions)
    min_supp_count = min_support * n_trans

    # 2. Budowa drzewa FP-Drzewa
    # Krok 1: Policz wystąpienia i odfiltruj te poniżej min_support
    item_counts = defaultdict(int)
    for trans in raw_transactions:
        for item in trans:
            item_counts[item] += 1
    
    frequent_items = {k: v for k, v in item_counts.items() if v >= min_supp_count}
    # Sortowanie elementów według popularności (ważne dla FP-Growth)
    sorted_items = sorted(frequent_items.keys(), key=lambda x: frequent_items[x], reverse=True)

    # Krok 2: Budowa drzewa
    root = FPNode(None, 0, None)
    header_table = {item: [frequent_items[item], None] for item in frequent_items}

    for trans in raw_transactions:
        # Tylko częste elementy, posortowane
        filtered_trans = [item for item in sorted_items if item in trans]
        if filtered_trans:
            insert_tree(filtered_trans, root, header_table)

    # 3. Tworzenie zbiorów częstych
    frequent_itemsets = {}
    mine_tree(header_table, min_supp_count, set(), frequent_itemsets)

    # 4. Generowanie reguł 
    from itertools import combinations
    rules = []
    for itemset, count in frequent_itemsets.items():
        if len(itemset) > 1:
            support = count / n_trans
            for i in range(1, len(itemset)):
                for antecedent in combinations(itemset, i):
                    antecedent = frozenset(antecedent)
                    consequent = itemset - antecedent
                    
                    supp_a = frequent_itemsets.get(antecedent)
                    if supp_a:
                        confidence = count / supp_a
                        if confidence >= min_confidence:
                            rules.append({
                                'A': list(antecedent),
                                'B': list(consequent),
                                'supp': support,
                                'conf': confidence,
                            })
    
    if not verbose:
        print(f"Znaleziono {len(rules)} reguł.")
    if not verbose:
        print(f'Wygenerowano {len(rules)} reguł.')
        for rule in rules:
            print(f'{rule['A']}=>{rule['B']} Support: {rule['supp']}, Confidence: {rule['conf']}')
    
    return rules

def insert_tree(items, node, header_table):
    if items[0] in node.children:
        node.children[items[0]].count += 1
    else:
        new_node = FPNode(items[0], 1, node)
        node.children[items[0]] = new_node
        # Aktualizacja powiązań w header_table
        if header_table[items[0]][1] is None:
            header_table[items[0]][1] = new_node
        else:
            current = header_table[items[0]][1]
            while current.next_link is not None:
                current = current.next_link
            current.next_link = new_node
            
    if len(items) > 1:
        insert_tree(items[1:], node.children[items[0]], header_table)

def mine_tree(header_table, min_supp_count, prefix, frequent_itemsets):
    # Sortowanie elementów w tablicy nagłówków (od najrzadszych)
    sorted_items = [v[0] for v in sorted(header_table.items(), key=lambda p: p[1][0])]
    
    for item in sorted_items:
        new_frequent_set = prefix.copy()
        new_frequent_set.add(item)
        frequent_itemsets[frozenset(new_frequent_set)] = header_table[item][0]
        
        # Budowa bazy wzorców warunkowych
        cond_patterns = []
        node = header_table[item][1]
        while node is not None:
            prefix_path = []
            parent = node.parent
            while parent.item is not None:
                prefix_path.append(parent.item)
                parent = parent.parent
            if prefix_path:
                cond_patterns.append((prefix_path, node.count))
            node = node.next_link
        
        # Budowa warunkowego drzewa FP
        cond_header = defaultdict(int)
        for path, count in cond_patterns:
            for p_item in path:
                cond_header[p_item] += count
        
        cond_header = {k: [v, None] for k, v in cond_header.items() if v >= min_supp_count}
        
        if cond_header:
            cond_root = FPNode(None, 0, None)
            for path, count in cond_patterns:
                filtered_path = [i for i in path if i in cond_header]
                if filtered_path:
                    # Wstawianie do drzewa warunkowego (uproszczone)
                    curr = cond_root
                    for p_item in reversed(filtered_path): # Odwracamy ścieżkę do roota
                        if p_item in curr.children:
                            curr.children[p_item].count += count
                        else:
                            new_node = FPNode(p_item, count, curr)
                            curr.children[p_item] = new_node
                            if cond_header[p_item][1] is None:
                                cond_header[p_item][1] = new_node
                            else:
                                tmp = cond_header[p_item][1]
                                while tmp.next_link is not None: tmp = tmp.next_link
                                tmp.next_link = new_node
                        curr = curr.children[p_item]
            
            mine_tree(cond_header, min_supp_count, new_frequent_set, frequent_itemsets)

if __name__ == "__main__":
    solve(config.min_support, config.min_confidence, verbose=False)