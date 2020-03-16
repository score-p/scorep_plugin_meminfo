#pragma once

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <regex>
#include <sstream>

#include <cstdint>

namespace memInfo {

class result_t;
std::map<std::int64_t, result_t> parse(const std::vector<std::string>& search);
std::string printSize(std::int64_t size);

class result_t {
public:
    result_t()
    {
    }
    result_t(std::string name, std::int64_t size, std::int64_t id, bool use_byte = true)
        : name(name), size(size), id_(id), use_byte_(use_byte)
    {
    }

    std::string name;
    std::int64_t size;

    std::int64_t id() const
    {
        return id_;
    }

    bool using_byte() const
    {
        return use_byte_;
    }

    friend std::ostream& operator<<(std::ostream& os, const result_t& r);

private:
    std::int64_t id_;
    bool use_byte_ = true;
};

std::ostream& operator<<(std::ostream& os, const result_t& r)
{
    if (r.use_byte_) {
        os << r.name << ": " << printSize(r.size);
    }
    else {
        os << r.name << ": " << r.size;
    }

    return os;
}

/*
 *
 * parse
 *
 */

std::map<std::int64_t, result_t> parse(const std::vector<std::string>& search)
{
    std::string line;
    std::ifstream meminfo("/proc/meminfo");
    std::map<std::int64_t, result_t> results;

    std::string regex_custom_str;
    std::string regex_str = "(";

    if (search.empty()) {
        regex_str += "[a-zA-Z0-9_]+";
    }
    else {
        bool first = true;
        for (const auto& s : search) {
            if (first)
                first = false;
            else {
                regex_str += '|';
            }

            regex_str += s;
        }
    }
    regex_str += ")";
    regex_custom_str = regex_str;
    regex_str += ":[^a-zA-Z0-9]*([0-9]+).?([kKmMgGtT][bB])?[^a-zA-Z0-9]*";

    std::regex regex(regex_str);

    int mem_total;
    int mem_free;
    int buffers;
    int cache;
    int swap_total;
    int swap_free;
    int swap_cached;

    std::int64_t line_nr = 0;
    while (std::getline(meminfo, line)) {
        std::smatch match;

        bool run = false;
        bool save = false;

        if (std::regex_match(line, match, regex)) {
            run = true;
            save = true;
        }
        else if (std::regex_match(line, match, std::regex("(MemTotal|MemFree|SwapTotal|SwapFree|SwapCached|Cache|Buffers):[^a-zA-Z0-9]*([0-9]+).?([kKmMgGtT][bB])?[^a-zA-Z0-9]*"))) {
            run = true;
        }

        if (run) {
            std::string name = match[1].str();
            std::int64_t size = std::stoll(match[2].str());
            std::string unit = match[3].str();

            if (name == "MemTotal") {
                mem_total = size;
            }
            if (name == "MemFree") {
                mem_free = size;
            }
            if (name == "Buffers") {
                buffers = size;
            }
            if (name == "Cache") {
                cache = size;
            }
            if (name == "SwapTotal") {
                swap_total = size;
            }
            if (name == "SwapFree") {
                swap_free = size;
            }
            if (name == "SwapCached") {
                swap_cached = size;
            }

            if (unit.size() == 2 && std::tolower(unit[1]) == 'b') {
                switch (std::tolower(unit[0])) {
                case 't':
                    size *= 1024;
                case 'g':
                    size *= 1024;
                case 'm':
                    size *= 1024;
                case 'k':
                    size *= 1024;
                default:
                    break;
                }

                if (save) {
                    results.emplace(line_nr, result_t{std::move(name), size, line_nr});
                }
            }
            else if (unit.size() == 1 && std::tolower(unit[0]) == 'b' && save) {
                results.emplace(line_nr, result_t{std::move(name), size, line_nr});
            }
            else if (save) {
                results.emplace(line_nr, result_t{std::move(name), size, line_nr, false});
            }
        }
        ++line_nr;
    }

    // CUSTOM

    std::regex regex_custom(regex_custom_str);
    std::smatch match_custom;
    std::string s;

    {
        s = "MemUsed";
        if (std::regex_match(s, match_custom, regex_custom)) {
            results.emplace(
                line_nr, result_t{s, mem_total - mem_free - buffers - cache, line_nr});
        }
        ++line_nr;
    }

    {
        s = "SwapUsed";
        if (std::regex_match(s, match_custom, regex_custom)) {
            results.emplace(
                line_nr, result_t{s, swap_total - swap_free - swap_cached, line_nr});
        }
        ++line_nr;
    }

    return results;
}

} // namespace memInfo