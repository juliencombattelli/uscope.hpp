#ifndef USCOPE_HPP_
#define USCOPE_HPP_

#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace uscope {

using Iteration = int64_t;

class BenchmarkState {
public:
    explicit BenchmarkState(Iteration iteration_count)
        : total_iterations_(iteration_count)
        , remaining_iterations_(iteration_count)
    {
        iterations_time_.reserve(total_iterations_);
    }

    [[nodiscard]] bool keep_running()
    {
        switch (state_) {
        case State::Finished:
        case State::Skipped: {
            return false;
        }
        case State::NotStarted: {
            state_ = State::Started;
        } break;
        case State::Started: {
            end_ = std::chrono::steady_clock::now();
            int64_t elapsed
                = std::chrono::duration_cast<std::chrono::nanoseconds>(end_ - begin_).count();
            iterations_time_.push_back(elapsed);
        } break;
        }

        bool should_run = remaining_iterations_-- > 0;
        if (!should_run) {
            state_ = State::Finished;
        }
        begin_ = std::chrono::steady_clock::now();
        return should_run;
    }

    [[nodiscard]] Iteration remaining_iterations() const
    {
        return remaining_iterations_;
    }

private:
    enum class State : uint8_t {
        NotStarted,
        Started,
        Finished,
        Skipped,
    };

    Iteration total_iterations_;
    Iteration remaining_iterations_;
    State state_ { State::NotStarted };
    std::chrono::steady_clock::time_point begin_;
    std::chrono::steady_clock::time_point end_;
    std::vector<int64_t> iterations_time_;
};

template<typename Fn, typename... Args>
concept BenchmarkFunction = std::invocable<Fn, BenchmarkState&>
    && std::is_void_v<typename std::invoke_result_t<Fn, BenchmarkState&>>;

class Benchmark {
public:
    template<BenchmarkFunction Fn>
    Benchmark(const std::string& name, Fn&& function)
        : name_(name)
        , function_(std::forward<Fn>(function))
    {
    }

    void execute(BenchmarkState& state)
    {
        function_(state);
    }

private:
    std::string name_;
    std::function<void(BenchmarkState&)> function_;
};

struct Config {
    Iteration iteration_count;
};

template<std::integral Integer>
static inline size_t count_digits(Integer n)
{
    size_t result = 1;
    if (n < 0) {
        n = (n == std::numeric_limits<Integer>::min()) ? std::numeric_limits<Integer>::max() : -n;
    }
    while (n > 9) {
        n /= 10;
        result++;
    }
    return result;
}

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(const Config& config)
        : config_(config)
    {
    }

    template<BenchmarkFunction Fn>
    void add_benchmark(const std::string& name, Fn&& function)
    {
        benchmarks_.emplace_back(Benchmark { name, std::forward<Fn>(function) });
    }

    void run_all_benchmarks()
    {
        for (auto& benchmark : benchmarks_) {
            BenchmarkState state(config_.iteration_count);
            benchmark.execute(state);
        }
    }

private:
    Config config_;
    std::vector<Benchmark> benchmarks_;
};

} // namespace uscope

#endif // USCOPE_HPP_
