#include <scorep/plugin/plugin.hpp>

#include <memInfo.hpp>

#include <atomic>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct dataset {
    dataset(scorep::chrono::ticks timepoint, std::map<std::int64_t, memInfo::result_t> data)
        : timepoint(timepoint), data(std::move(data))
    {
    }

    scorep::chrono::ticks timepoint;
    std::map<std::int64_t, memInfo::result_t> data;
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

        for (auto match : memInfo::parse({pattern})) {
            if (subscribed_.find(match.second.name) == subscribed_.end()) {
                result.push_back(scorep::plugin::metric_property{
                    match.second.name, "", match.second.usingByte() ? "B" : ""}
                                     .absolute_point()
                                     .value_int());
                subscribed_.emplace(match.second.name, match.second.id());
                names_.emplace_back(match.second.name);
            }
        }

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
        lastMeasurement_ = std::chrono::system_clock::now();
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
            auto now = scorep::chrono::measurement_clock::now();
            data_.emplace_back(now, memInfo::parse(names_));

            while (lastMeasurement_ < std::chrono::system_clock::now())
                lastMeasurement_ += intervall_;

            std::this_thread::sleep_until(lastMeasurement_);
        }
    }

    template <typename Cursor>
    void get_all_values(const int& id, Cursor& c)
    {
        for (const auto& data_point : data_) {
            c.write(data_point.timepoint, data_point.data.find(id)->second.size);
        }
    }

private:
    std::map<std::string, std::int64_t> subscribed_;
    std::atomic<bool> running = false;
    std::thread thread_;
    std::vector<std::string> names_;
    std::vector<dataset> data_;
    std::chrono::nanoseconds intervall_;
    std::chrono::system_clock::time_point lastMeasurement_ =
        std::chrono::system_clock::now();
};