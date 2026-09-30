// Minimal XML plist parser (dict/array/string/integer/real/true/false).
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace ogd {

struct PValue {
    enum Type { None, Dict, Array, String, Int, Real, Bool } type = None;
    std::string s;
    double n = 0;
    bool b = false;
    std::vector<PValue> arr;
    std::vector<std::pair<std::string, PValue>> dict;

    const PValue* get(const std::string& key) const {
        for (auto& kv : dict)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    bool isDict() const { return type == Dict; }
    std::string str() const { return type == String ? s : std::string(); }
    double num() const { return (type == Int || type == Real) ? n : (type == Bool ? (b ? 1 : 0) : 0); }
    bool boolean() const { return type == Bool ? b : (num() != 0); }
};

// Returns a value of type None on parse failure.
PValue parsePlist(const std::string& xml);

// Extracts every number found in strings like "{{1,2},{3.5,-4}}".
std::vector<double> parseNumbers(const std::string& s);

} // namespace ogd
