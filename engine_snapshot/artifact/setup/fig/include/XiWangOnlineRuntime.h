#ifndef XIWANGONLINERUNTIME_H
#define XIWANGONLINERUNTIME_H

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fig
{

struct XiWangOnlineObservation
{
    std::string cluster_id;
    size_t level;
    size_t population;
    size_t successes;
    size_t trials;
};

class XiWangOnlineRuntime
{
public:
    XiWangOnlineRuntime() : batch_index_(0ul) {}

    bool enabled() const
    {
        const char* raw = std::getenv("BATS_XIWANG_ONLINE");
        return raw != nullptr && std::string(raw) == "1";
    }

    void reset()
    {
        plan_.clear();
        observations_.clear();
        batch_index_ = 0ul;
        batch_id_.clear();
        last_summary_.clear();
    }

    size_t batch_index() const { return batch_index_; }
    const std::string& last_summary() const { return last_summary_; }

    size_t population(const size_t level, const size_t fallback) const
    {
        const auto found = plan_.find(level);
        return found == plan_.end() ? fallback : found->second;
    }

    void begin_formal_batch()
    {
        if (!enabled()) return;
        observations_.clear();
        batch_id_ = "formal-" + std::to_string(batch_index_ + 1ul);
    }

    void observe(const std::string& cluster_id,
                 const size_t level,
                 const size_t population,
                 const size_t successes,
                 const size_t trials)
    {
        if (!enabled()) return;
        if (batch_id_.empty())
            throw std::logic_error("XiWang online batch was not started");
        if (cluster_id.empty() || population < 24ul || trials != population ||
            successes > trials)
            throw std::invalid_argument("invalid XiWang online observation");
        observations_.push_back(
            XiWangOnlineObservation{cluster_id, level, population, successes, trials});
    }

    void end_formal_batch()
    {
        if (!enabled()) return;
        if (observations_.empty())
            throw std::runtime_error("XiWang online formal batch has no observations");
        const std::string workdir = required_env("BATS_XIWANG_ONLINE_WORKDIR");
        const std::string config = required_env("BATS_XIWANG_ONLINE_CONFIG");
        const std::string pythonpath = required_env("BATS_XIWANG_ONLINE_PYTHONPATH");
        const std::string python = required_env("BATS_XIWANG_ONLINE_PYTHON");
        const std::string prefix = workdir + "/batch_" +
                                   std::to_string(batch_index_ + 1ul);
        const std::string observations = prefix + "_observations.jsonl";
        const std::string plan = prefix + "_plan.txt";
        const std::string summary = prefix + "_summary.json";
        const std::string state = workdir + "/state.json";
        write_observations(observations);
        set_pythonpath(pythonpath);
        std::ostringstream command;
        command << command_quote(python)
                << " -m xiwang_online.planner_cli"
                << " --config " << command_quote(config)
                << " --state " << command_quote(state)
                << " --observations " << command_quote(observations)
                << " --plan-output " << command_quote(plan)
                << " --summary-output " << command_quote(summary);
        const int code = std::system(command.str().c_str());
        if (code != 0)
            throw std::runtime_error("XiWang online planner process failed");
        read_plan(plan);
        std::ifstream result(summary.c_str());
        std::ostringstream text;
        text << result.rdbuf();
        last_summary_ = text.str();
        if (last_summary_.find("\"status\": \"PLAN_UPDATED\"") ==
            std::string::npos)
            throw std::runtime_error("XiWang online planner returned no update");
        ++batch_index_;
        batch_id_.clear();
    }

private:
    std::map<size_t,size_t> plan_;
    std::vector<XiWangOnlineObservation> observations_;
    size_t batch_index_;
    std::string batch_id_;
    std::string last_summary_;

    static std::string required_env(const char* name)
    {
        const char* raw = std::getenv(name);
        if (raw == nullptr || raw[0] == '\0')
            throw std::invalid_argument(std::string(name) + " is required");
        return std::string(raw);
    }

    static std::string json_string(const std::string& value)
    {
        std::ostringstream out;
        out << '"';
        for (const char ch: value) {
            if (ch == '"' || ch == '\\') out << '\\';
            out << ch;
        }
        out << '"';
        return out.str();
    }

    static std::string command_quote(const std::string& value)
    {
#ifdef _WIN32
        if (value.find_first_of(" \t\"") == std::string::npos)
            return value;
        std::string result("\"");
        for (const char ch: value) {
            if (ch == '"') result += '\\';
            result += ch;
        }
        result += '"';
        return result;
#else
        std::string result("'");
        for (const char ch: value) {
            if (ch == '\'') result += "'\\''";
            else result += ch;
        }
        result += '\'';
        return result;
#endif
    }

    static void set_pythonpath(const std::string& value)
    {
#ifdef _WIN32
        if (_putenv_s("PYTHONPATH", value.c_str()) != 0)
            throw std::runtime_error("cannot set XiWang online PYTHONPATH");
#else
        if (setenv("PYTHONPATH", value.c_str(), 1) != 0)
            throw std::runtime_error("cannot set XiWang online PYTHONPATH");
#endif
    }

    void write_observations(const std::string& path) const
    {
        std::ofstream out(path.c_str(), std::ios::trunc);
        if (!out)
            throw std::runtime_error("cannot write XiWang online observations");
        for (const auto& row: observations_) {
            out << "{\"batch_id\":" << json_string(batch_id_)
                << ",\"cluster_id\":" << json_string(row.cluster_id)
                << ",\"level\":" << row.level
                << ",\"population\":" << row.population
                << ",\"successes\":" << row.successes
                << ",\"trials\":" << row.trials << "}\n";
        }
        if (!out)
            throw std::runtime_error("failed writing XiWang online observations");
    }

    void read_plan(const std::string& path)
    {
        std::ifstream in(path.c_str());
        std::string text;
        std::getline(in, text);
        if (!in && text.empty())
            throw std::runtime_error("cannot read XiWang online plan");
        std::map<size_t,size_t> parsed;
        std::istringstream tokens(text);
        std::string token;
        while (std::getline(tokens, token, ',')) {
            if (token.empty()) continue;
            const size_t colon = token.find(':');
            if (colon == std::string::npos)
                throw std::runtime_error("invalid XiWang online plan text");
            const size_t level = static_cast<size_t>(
                std::stoull(token.substr(0ul, colon)));
            const size_t population = static_cast<size_t>(
                std::stoull(token.substr(colon + 1ul)));
            if (population < 24ul || population > 48ul ||
                !parsed.emplace(level, population).second)
                throw std::runtime_error("invalid XiWang online plan population");
        }
        if (parsed.empty())
            throw std::runtime_error("XiWang online planner returned an empty plan");
        plan_.swap(parsed);
    }
};

} // namespace fig

#endif // XIWANGONLINERUNTIME_H
