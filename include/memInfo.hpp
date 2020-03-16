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
    result_t(std::string name, std::int64_t size, std::int64_t id, bool useByte = true)
        : name(name), size(size), id_(id), useByte_(useByte)
    {
    }

    std::string name;
    std::int64_t size;

    std::int64_t id() const
    {
        return id_;
    }

    bool usingByte() const
    {
        return useByte_;
    }

    friend std::ostream& operator<<(std::ostream& os, const result_t& r);

private:
    std::int64_t id_;
    bool useByte_ = true;
};

std::ostream& operator<<(std::ostream& os, const result_t& r)
{
    if (r.useByte_)
        os << r.name << ": " << printSize(r.size);
    else
        os << r.name << ": " << r.size;

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

    std::string str = "(";
    if (search.empty()) {
        str += "[a-zA-Z0-9_]+";
    }
    else {
        bool first = true;
        for (const auto& s : search) {
            if (first)
                first = false;
            else
                str += '|';

            str += s;
        }
    }
    str += "):[^a-zA-Z0-9]*([0-9]+).?([kKmMgGtT][bB])?[^a-zA-Z0-9]*";

    std::regex regEx(str);

    int MemTotal;
    int MemFree;
    int Buffers;
    int Cache;
    int SwapTotal;
    int SwapCached;
    int SwapFree;

    std::int64_t lineNr = 0;
    while (std::getline(meminfo, line)) {
        std::smatch match;

        bool run = false;

        if (std::regex_match(line, match, regEx))
            run = true;
        else if (std::regex_match(line, match, std::regex("(MemTotal|MemFree|SwapTotal|SwapFree|SwapCached|Cache|Buffers):[^a-zA-Z0-9]*([0-9]+).?([kKmMgGtT][bB])?[^a-zA-Z0-9]*")))
            run = true;

        if (run) {
            std::string name = match[1].str();
            std::int64_t size = std::stoll(match[2].str());
            std::string sizeType = match[3].str();

            if (!sizeType.empty() && std::tolower(sizeType[1]) == 'b') {
                switch (std::tolower(sizeType[0])) {
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

                if (name == "MemTotal")
                    MemTotal = size;
                if (name == "MemFree")
                    MemFree = size;
                if (name == "Buffers")
                    Buffers = size;
                if (name == "Cache")
                    Cache = size;
                if (name == "SwapTotal")
                    SwapTotal = size;
                if (name == "SwapFree")
                    SwapFree = size;
                if (name == "SwapCached")
                    SwapCached = size;

                if (std::regex_match(line, match, regEx))
                    results.emplace(lineNr, result_t{std::move(name), size, lineNr});
            }
            else {
                if (std::regex_match(line, match, regEx))
                    results.emplace(lineNr, result_t{std::move(name), size, lineNr, false});
            }
        }
        ++lineNr;
    }

    // CUSTOM

    std::string str_custom = "(";
    if (search.empty()) {
        str_custom += "[a-zA-Z0-9_]+";
    }
    else {
        bool first = true;
        for (const auto& s : search) {
            if (first)
                first = false;
            else
                str_custom += '|';

            str_custom += s;
        }
    }
    str_custom += ")";

    std::regex regEx_custom(str_custom);
    std::smatch match_custom;
    std::string s;

    {
        s = "MemUsed";
        if (std::regex_match(s, match_custom, regEx_custom)) {
            results.emplace(
                lineNr, result_t{s, MemTotal - MemFree - Buffers - Cache, lineNr});
        }
        ++lineNr;
    }

    {
        s = "SwapUsed";
        if (std::regex_match(s, match_custom, regEx_custom)) {
            results.emplace(lineNr, result_t{s, SwapTotal - SwapFree - SwapCached, lineNr});
        }
        ++lineNr;
    }

    return results;
}

std::string printSize(std::int64_t size)
{
    const std::array<const char* const, 5> sizeTypes = {" B", " kB", " MB",
                                                        " GB"};

    std::stringstream out;
    double res = static_cast<double>(size);

    for (const auto& s : sizeTypes) {
        if (res < 800) {
            out << std::fixed << std::setprecision(2) << res << s;
            return out.str();
        }
        res /= 1024;
    }

    out << std::fixed << std::setprecision(2) << res << " TB";
    return out.str();
}

} // namespace memInfo