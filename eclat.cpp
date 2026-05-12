#if defined(__GNUC__) || defined(__clang__)
#pragma GCC optimize("O3,unroll-loops")
#pragma GCC target("avx2,bmi,bmi2,lzcnt,popcnt")
#endif

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <algorithm>
#include <chrono>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace py = pybind11;

using BitMap = std::vector<uint64_t>;

struct Rule {
    std::vector<int> A;
    std::vector<int> B;
    double supp;
    double conf;
};

struct VectorHash {
    size_t operator()(const std::vector<int>& v) const {
        size_t seed = v.size();
        for (int i : v) {
            seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

inline BitMap intersect_bitmaps(const BitMap& b1, const BitMap& b2) {
    size_t size = b1.size();
    BitMap result(size);
    for (size_t i = 0; i < size; ++i) {
        result[i] = b1[i] & b2[i];
    }
    return result;
}

inline int count_bits(const BitMap& b) {
    int count = 0;
    for (uint64_t block : b) {
#if defined(__GNUC__) || defined(__clang__)
        count += __builtin_popcountll(block); 
#elif defined(_MSC_VER)
        count += (int)__popcnt64(block); 
#else
        while (block) {
            block &= (block - 1);
            count++;
        }
#endif
    }
    return count;
}

inline int intersect_and_count(const BitMap& b1, const BitMap& b2, BitMap& result) {
    size_t size = b1.size();
    int count = 0;
    for (size_t i = 0; i < size; ++i) {
        uint64_t val = b1[i] & b2[i];
        result[i] = val;
#if defined(__GNUC__) || defined(__clang__)
        count += __builtin_popcountll(val); 
#elif defined(_MSC_VER)
        count += (int)__popcnt64(val);
#else
        uint64_t v = val;
        while (v) { v &= (v - 1); count++; }
#endif
    }
    return count;
}

void eclat_dfs(
    const std::vector<int>& prefix,
    const std::vector<std::pair<int, BitMap>>& items,
    int min_supp_count,
    std::unordered_map<std::vector<int>, int, VectorHash>& all_frequent) 
{
    for (size_t i = 0; i < items.size(); ++i) {
        std::vector<int> new_prefix = prefix;
        new_prefix.push_back(items[i].first);
        
        all_frequent[new_prefix] = count_bits(items[i].second);

        std::vector<std::pair<int, BitMap>> next_items;
        next_items.reserve(items.size() - i - 1);

        for (size_t j = i + 1; j < items.size(); ++j) {
            BitMap intersection(items[i].second.size()); 
            int count = intersect_and_count(items[i].second, items[j].second, intersection);
            
            if (count >= min_supp_count) {
                next_items.emplace_back(items[j].first, std::move(intersection));
            }
        }

        if (!next_items.empty()) {
            eclat_dfs(new_prefix, next_items, min_supp_count, all_frequent);
        }
    }
}

py::list solve_cpp(std::string path, double min_support, double min_confidence, bool verbose) {
    auto start_time_total = std::chrono::high_resolution_clock::now();

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return py::list();
    
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<char> buffer(file_size + 1);
    fread(buffer.data(), 1, file_size, f);
    fclose(f);
    buffer[file_size] = '\0';

    std::unordered_map<std::string_view, int> string_to_id;
    std::vector<std::string> id_to_string;
    std::unordered_map<std::string_view, std::vector<int>> trans_dict;
    
    char* ptr = buffer.data();
    
    while (*ptr && *ptr != '\n') ptr++;
    if (*ptr == '\n') ptr++;

    while (*ptr) {
        char* invoice_start = ptr;
        while (*ptr && *ptr != ',') ptr++;
        if (!*ptr) break;
        std::string_view invoice(invoice_start, ptr - invoice_start);
        ptr++;

        char* code_start = ptr;
        while (*ptr && *ptr != ',') ptr++;
        std::string_view code(code_start, ptr - code_start);

        while (*ptr && *ptr != '\n') ptr++;
        if (*ptr == '\n') ptr++;

        if (!invoice.empty() && isdigit(invoice[0]) && !code.empty()) {
            auto it = string_to_id.find(code);
            int code_id;
            if (it == string_to_id.end()) {
                code_id = (int)id_to_string.size();
                string_to_id[code] = code_id;
                id_to_string.emplace_back(code);
            } else {
                code_id = it->second;
            }
            trans_dict[invoice].push_back(code_id);
        }
    }

    std::vector<std::vector<int>> transactions;
    transactions.reserve(trans_dict.size());
    for (auto& pair : trans_dict) {
        auto& items = pair.second;
        std::sort(items.begin(), items.end());
        items.erase(std::unique(items.begin(), items.end()), items.end());
        transactions.push_back(std::move(items));
    }

    int n_trans = (int)transactions.size();
    if (n_trans == 0) return py::list();
    
    int min_supp_count = std::max(1, (int)(min_support * n_trans));
    int num_unique_items = (int)id_to_string.size();

    int num_blocks = (n_trans + 63) / 64;
    std::vector<BitMap> vertical_data(num_unique_items, BitMap(num_blocks, 0));
    
    for (int tid = 0; tid < n_trans; ++tid) {
        for (int item_id : transactions[tid]) {
            int block_idx = tid / 64;
            vertical_data[item_id][block_idx] |= (1ULL << (tid % 64));
        }
    }

    std::vector<std::pair<int, BitMap>> frequent_items;
    for (int i = 0; i < num_unique_items; ++i) {
        if (count_bits(vertical_data[i]) >= min_supp_count) {
            frequent_items.emplace_back(i, std::move(vertical_data[i]));
        }
    }

    std::sort(frequent_items.begin(), frequent_items.end(), 
              [](const auto& a, const auto& b) { return count_bits(a.second) < count_bits(b.second); });

    auto start_time_algo = std::chrono::high_resolution_clock::now();

    std::unordered_map<std::vector<int>, int, VectorHash> all_frequent;
    all_frequent.reserve(100000);
    eclat_dfs({}, frequent_items, min_supp_count, all_frequent);

    std::vector<Rule> rules;
    rules.reserve(100000);
    for (const auto& pair : all_frequent) {
        const auto& itemset = pair.first;
        int count = pair.second;
        
        if (itemset.size() < 2) continue;
        
        int num_subsets = (1 << itemset.size()) - 1; 
        for (int mask = 1; mask < num_subsets; ++mask) {
            std::vector<int> A, B;
            for (size_t i = 0; i < itemset.size(); ++i) {
                if (mask & (1 << i)) A.push_back(itemset[i]);
                else B.push_back(itemset[i]);
            }
            
            auto it_A = all_frequent.find(A);
            if (it_A != all_frequent.end()) {
                double conf = (double)count / it_A->second;
                if (conf >= min_confidence) {
                    rules.push_back({std::move(A), std::move(B), (double)count / n_trans, conf});
                }
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_total = end_time - start_time_total;
    std::chrono::duration<double> duration_algo = end_time - start_time_algo;

    if (!verbose) { 
        std::cout << "Wygenerowano " << rules.size() << " regul.\n";
        
        std::string buffer;
        buffer.reserve(1024 * 1024); 
        char num_buf[128]; 

        for (const auto& r : rules) {
            buffer += "{";
            for (size_t i = 0; i < r.A.size(); ++i) {
                buffer += id_to_string[r.A[i]];
                if (i < r.A.size() - 1) buffer += ", ";
            }
            buffer += "} => {";
            for (size_t i = 0; i < r.B.size(); ++i) {
                buffer += id_to_string[r.B[i]];
                if (i < r.B.size() - 1) buffer += ", ";
            }
            
            snprintf(num_buf, sizeof(num_buf), "} Support: %.4f, Confidence: %.4f\n", r.supp, r.conf);
            buffer += num_buf;

            if (buffer.size() > 1000000) {
                fwrite(buffer.data(), 1, buffer.size(), stdout);
                buffer.clear();
            }
        }
        
        if (!buffer.empty()) {
            fwrite(buffer.data(), 1, buffer.size(), stdout);
        }
        fflush(stdout); 
    }
    
    if (!verbose) {
        std::cerr << "Total Time: " << duration_total.count() << "s | Core Algo Time: " << duration_algo.count() << "s | Rules: " << rules.size() << "\n";
    }

    std::vector<py::str> py_id_to_string;
    py_id_to_string.reserve(id_to_string.size());
    for (const auto& s : id_to_string) {
        py_id_to_string.push_back(py::str(s)); 
    }

    py::str key_A("A"), key_B("B"), key_supp("supp"), key_conf("conf");

    py::list py_rules;
    
    for (const auto& r : rules) {
        py::dict d;
        
        py::list py_A(r.A.size());
        for (size_t i = 0; i < r.A.size(); ++i) {
            py_A[i] = py_id_to_string[r.A[i]]; 
        }
        
        py::list py_B(r.B.size());
        for (size_t i = 0; i < r.B.size(); ++i) {
            py_B[i] = py_id_to_string[r.B[i]];
        }
        
        d[key_A] = py_A;
        d[key_B] = py_B;
        d[key_supp] = r.supp;
        d[key_conf] = r.conf;
        
        py_rules.append(d);
    }
    return py_rules;
}

PYBIND11_MODULE(c_eclat, m) {
    m.def("solve_cpp", &solve_cpp);
}