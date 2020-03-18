#include <scorep/plugin/plugin.hpp>

#include <atomic>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <cstdint>

struct meminfo_t {
    std::int64_t line_nr;
    std::string name;
    std::string unit;
};

class meminfo_plugin : public scorep::plugin::base<meminfo_plugin,
                                                   scorep::plugin::policy::async,
                                                   scorep::plugin::policy::post_mortem,
                                                   scorep::plugin::policy::scorep_clock,
                                                   scorep::plugin::policy::once> {
public:
    meminfo_plugin()
    {
        auto interval_str =
            scorep::environment_variable::get("INTERVAL", "10ms");

        std::regex r("([0-9]+)([mun]?s)");
        std::smatch s;

        if (std::regex_match(interval_str, s, r)) {
            std::string time = s[1];
            std::string unit = s[2];

            switch (unit[0]) {
            case 's':
                intervall_ = std::chrono::seconds(std::stol(time));
                break;
            case 'm':
                intervall_ = std::chrono::milliseconds(std::stol(time));
                break;
            case 'u':
                intervall_ = std::chrono::microseconds(std::stol(time));
                break;
            case 'n':
                intervall_ = std::chrono::nanoseconds(std::stol(time));
                break;

            default:
                break;
            }
        }
        else {
            intervall_ = std::chrono::milliseconds(10);
        }
    }

    std::vector<scorep::plugin::metric_property> get_metric_properties(const std::string& pattern /* = "mem.*"*/)
    {
        std::vector<scorep::plugin::metric_property> result;

        for (const auto& match : init({pattern})) {
            if (subscribed_.find(match.name) == subscribed_.end()) {
                result.push_back(
                    scorep::plugin::metric_property{match.name, "", match.unit}
                        .absolute_point()
                        .value_int());
                subscribed_.emplace(match.name, last_id);
                id_by_line_.emplace(match.line_nr, last_id);
                values_by_id_.emplace(last_id, std::vector<int64_t>());

                ++last_id;
            }
        }

        if (subscribed_.find("MemTotal") == subscribed_.end()) {
            throw new std::exception();
        }
        mem_total_pos = subscribed_.find("MemTotal")->second;

        if (subscribed_.find("MemFree") == subscribed_.end()) {
            throw new std::exception();
        }
        mem_free_pos = subscribed_.find("MemFree")->second;

        if (subscribed_.find("Buffers") == subscribed_.end()) {
            throw new std::exception();
        }
        buffers_pos = subscribed_.find("Buffers")->second;

        if (subscribed_.find("Cache") == subscribed_.end()) {
            throw new std::exception();
        }
        cache_pos = subscribed_.find("Cache")->second;

        if (subscribed_.find("SwapTotal") == subscribed_.end()) {
            throw new std::exception();
        }
        swap_total_pos = subscribed_.find("SwapTotal")->second;

        if (subscribed_.find("SwapFree") == subscribed_.end()) {
            throw new std::exception();
        }
        swap_free_pos = subscribed_.find("SwapFree")->second;

        if (subscribed_.find("SwapCached") == subscribed_.end()) {
            throw new std::exception();
        }
        swap_cached_pos = subscribed_.find("SwapCached")->second;

        if (subscribed_.find("MemUsed") == subscribed_.end()) {
            throw new std::exception();
        }
        mem_used_pos = subscribed_.find("MemUsed")->second;

        if (subscribed_.find("SwapUsed") == subscribed_.end()) {
            throw new std::exception();
        }
        swap_used_pos = subscribed_.find("SwapUsed")->second;

        return result;
    }

    std::int64_t add_metric(const std::string& match_name)
    {
        return subscribed_.find(match_name)->second;
    }

    void start()
    {
        if (running) {
            return;
        }

        running = true;
        last_measurement_ = std::chrono::system_clock::now();
        thread_ = std::thread([&]() { this->exec(); });
    }

    void stop()
    {
        if (!running) {
            return;
        }

        running = false;

        thread_.join();
    }

    void exec()
    {
        while (running) {
            parse(values_by_id_);
            times_.push_back(scorep::chrono::measurement_clock::now());

            while (last_measurement_ < std::chrono::system_clock::now()) {
                last_measurement_ += intervall_;
            }

            std::this_thread::sleep_until(last_measurement_);
        }
    }

    template <typename Cursor>
    void get_all_values(const int& id, Cursor& c)
    {
        const auto& values = values_by_id_.find(id)->second;

        for (int i = 0; i < values.size(); ++i) {
            c.write(times_[i], values[i]);
        }
    }

private:
    std::map<std::string, std::int64_t> subscribed_;
    std::atomic<bool> running = false;
    std::thread thread_;
    std::map<std::int64_t, std::int64_t> id_by_line_;
    std::map<std::int64_t, std::vector<std::int64_t>> values_by_id_;
    std::vector<scorep::chrono::ticks> times_;
    std::chrono::nanoseconds intervall_;
    std::chrono::system_clock::time_point last_measurement_ =
        std::chrono::system_clock::now();

    std::int64_t last_id = 0;

    int mem_total_pos;
    int mem_free_pos;
    int buffers_pos;
    int cache_pos;
    int swap_total_pos;
    int swap_free_pos;
    int swap_cached_pos;
    int mem_used_pos;
    int swap_used_pos;

    std::regex regex_parse =
        std::regex(".*:[^a-zA-Z0-9]*([0-9]+).?([kKmMgGtT][bB])?.*");

    std::vector<meminfo_t> init(const std::vector<std::string>& search)
    {
        std::string line;
        std::ifstream meminfo("/proc/meminfo");
        std::vector<meminfo_t> results;

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

            bool save = false;

            if (std::regex_match(line, match, regex)) {
                save = true;
            }
            else if (std::regex_match(line, match, std::regex("(MemTotal|MemFree|SwapTotal|SwapFree|SwapCached|Cache|Buffers):[^a-zA-Z0-9]*([0-9]+).?[kKmMgGtT]?([bB])?[^a-zA-Z0-9]*"))) {
                save = true;
            }

            if (save) {
                results.push_back(meminfo_t{line_nr, match[1].str(), match[3].str()});
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
                results.push_back(meminfo_t{line_nr, s, "B"});
            }
            ++line_nr;
        }

        {
            s = "SwapUsed";
            if (std::regex_match(s, match_custom, regex_custom)) {
                results.push_back(meminfo_t{line_nr, s, "B"});
            }
            ++line_nr;
        }

        return results;
    }

    void parse(std::map<std::int64_t, std::vector<std::int64_t>>& data)
    {
        std::string line;
        std::ifstream meminfo("/proc/meminfo");

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

            if (auto it = data.find(id_by_line_.find(line_nr)->second); it != data.end()) {
                std::regex_match(line, match, regex_parse);

                std::int64_t value = std::stoll(match[1].str());
                std::string unit = match[2].str();

                if (unit.size() == 2 && std::tolower(unit[1]) == 'b') {
                    switch (std::tolower(unit[0])) {
                    case 't':
                        value *= 1024;
                    case 'g':
                        value *= 1024;
                    case 'm':
                        value *= 1024;
                    case 'k':
                        value *= 1024;
                    default:
                        break;
                    }
                }

                if (mem_total_pos == line_nr) {
                    mem_total = value;
                }
                if (mem_free_pos == line_nr) {
                    mem_free = value;
                }
                if (buffers_pos == line_nr) {
                    buffers = value;
                }
                if (cache_pos == line_nr) {
                    cache = value;
                }
                if (swap_total_pos == line_nr) {
                    swap_total = value;
                }
                if (swap_free_pos == line_nr) {
                    swap_free = value;
                }
                if (swap_cached_pos == line_nr) {
                    swap_cached = value;
                }

                it->second.push_back(value);
            }
            ++line_nr;
        }

        // MemUsed

        if (auto it = data.find(mem_used_pos); it != data.end()) {
            it->second.push_back(mem_total - mem_free - buffers - cache);
        }

        ++line_nr;

        // SwapUsed

        if (auto it = data.find(swap_used_pos); it != data.end()) {
            it->second.push_back(swap_total - swap_free - swap_cached);
        }
    }
};