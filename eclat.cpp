#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <chrono>

namespace py = pybind11;

using BitMap = std::vector<uint64_t>;

struct Rule {
    std::vector<int> A;
    std::vector<int> B;
    double supp;
    double conf;
};

BitMap intersect_bitmaps(const BitMap& b1, const BitMap& b2) {
    size_t size = std::min(b1.size(), b2.size());
    BitMap result(size);
    for (size_t i = 0; i < size; ++i) {
        result[i] = b1[i] & b2[i];
    }
    return result;
}

int count_bits(const BitMap& b) {
    int count = 0;
    for (uint64_t block : b) {
#if defined(__GNUC__) || defined(__clang__)
        count += __builtin_popcountll(block); 
#elif defined(_MSC_VER)
        count += __popcnt64(block);
#else
        while (block) {
            block &= (block - 1);
            count++;
        }
#endif
    }
    return count;
}

// eclat
void eclat_dfs(
    const std::vector<int>& prefix,
    const std::vector<std::pair<int, BitMap>>& items,
    int min_supp_count,
    std::map<std::vector<int>, int>& all_frequent) 
{
    for (size_t i = 0; i < items.size(); ++i) {
        std::vector<int> new_prefix = prefix;
        new_prefix.push_back(items[i].first);
        std::sort(new_prefix.begin(), new_prefix.end());
        
        all_frequent[new_prefix] = count_bits(items[i].second);

        std::vector<std::pair<int, BitMap>> next_items;
        for (size_t j = i + 1; j < items.size(); ++j) {
            BitMap intersection = intersect_bitmaps(items[i].second, items[j].second);
            if (count_bits(intersection) >= min_supp_count) {
                next_items.push_back({items[j].first, intersection});
            }
        }

        if (!next_items.empty()) {
            eclat_dfs(new_prefix, next_items, min_supp_count, all_frequent);
        }
    }
}

// main func
py::list solve_cpp(std::string path, double min_support, double min_confidence, bool verbose) {
    // string <-> int
    std::unordered_map<std::string, int> string_to_id;
    std::vector<std::string> id_to_string;

    // data
    std::map<std::string, std::vector<int>> trans_dict;
    std::ifstream file(path);
    std::string line, invoice, stock_code;

    if (file.is_open()) {
        std::getline(file, line);
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            if (std::getline(ss, invoice, ',') && std::getline(ss, stock_code, ',')) {
                if (!invoice.empty() && isdigit(invoice[0]) && !stock_code.empty()) {
                    // add id
                    if (string_to_id.find(stock_code) == string_to_id.end()) {
                        string_to_id[stock_code] = id_to_string.size();
                        id_to_string.push_back(stock_code);
                    }
                    trans_dict[invoice].push_back(string_to_id[stock_code]);
                }
            }
        }
    }

    std::vector<std::vector<int>> transactions;
    for (auto const& [_, items] : trans_dict) {
        transactions.push_back(items);
    }

    int n_trans = transactions.size();
    if (n_trans == 0) return py::list();
    
    int min_supp_count = std::max(1, (int)(min_support * n_trans));
    int num_unique_items = id_to_string.size();

    // transposition
    std::vector<BitMap> vertical_data(num_unique_items);
    for (int tid = 0; tid < n_trans; ++tid) {
        for (int item_id : std::set<int>(transactions[tid].begin(), transactions[tid].end())) {
            int block_idx = tid / 64;
            if (block_idx >= vertical_data[item_id].size()) {
                vertical_data[item_id].resize(block_idx + 1, 0);
            }
            vertical_data[item_id][block_idx] |= (1ULL << (tid % 64));
        }
    }

    // filtering
    std::vector<std::pair<int, BitMap>> frequent_items;
    for (int i = 0; i < num_unique_items; ++i) {
        if (count_bits(vertical_data[i]) >= min_supp_count) {
            frequent_items.push_back({i, vertical_data[i]});
        }
    }

    std::sort(frequent_items.begin(), frequent_items.end(), 
              [](const auto& a, const auto& b) { return count_bits(a.second) < count_bits(b.second); });

    // time start
    auto start_time = std::chrono::high_resolution_clock::now();

    // eclat on ints
    std::map<std::vector<int>, int> all_frequent;
    eclat_dfs({}, frequent_items, min_supp_count, all_frequent);

    // generating rules
    std::vector<Rule> rules;
    for (const auto& [itemset, count] : all_frequent) {
        if (itemset.size() < 2) continue;
        
        int num_subsets = (1 << itemset.size()) - 1; 
        for (int mask = 1; mask < num_subsets; ++mask) {
            std::vector<int> A, B;
            for (size_t i = 0; i < itemset.size(); ++i) {
                if (mask & (1 << i)) A.push_back(itemset[i]);
                else B.push_back(itemset[i]);
            }
            
            if (all_frequent.count(A)) {
                double conf = (double)count / all_frequent[A];
                if (conf >= min_confidence) {
                    rules.push_back({A, B, (double)count / n_trans, conf});
                }
            }
        }
    }

    // time end
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end_time - start_time;

    std::cout << duration.count() << std::endl;

    // int string
    std::ofstream outfile("rules_output.txt");
    if (outfile.is_open()) {
        for (const auto& r : rules) {
            outfile << "{";
            for (size_t i = 0; i < r.A.size(); ++i) outfile << id_to_string[r.A[i]] << (i < r.A.size() - 1 ? ", " : "");
            outfile << "} => {";
            for (size_t i = 0; i < r.B.size(); ++i) outfile << id_to_string[r.B[i]] << (i < r.B.size() - 1 ? ", " : "");
            outfile << "} | supp: " << r.supp << " conf: " << r.conf << "\n";
        }
    } else if (verbose) {
        std::cerr << "Error: Nie udalo sie zapisac do rules_output.txt\n";
    }

    if (verbose) {
        std::cerr << "[C++] Time: " << duration.count() << "s | Rules: " << rules.size() << "\n";
    }

    // int back to string (python)
    py::list py_rules;
    for (const auto& r : rules) {
        py::dict d;
        py::list py_A, py_B;
        for (int id : r.A) py_A.append(id_to_string[id]);
        for (int id : r.B) py_B.append(id_to_string[id]);
        
        d["A"] = py_A;
        d["B"] = py_B;
        d["supp"] = r.supp;
        d["conf"] = r.conf;
        py_rules.append(d);
    }
    return py_rules;
}

PYBIND11_MODULE(c_eclat, m) {
    m.def("solve_cpp", &solve_cpp);
}