#ifndef USCOPE_HPP_
#define USCOPE_HPP_

#include <concepts>
#include <cstdint>
#include <functional>
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
    }

    [[nodiscard]] inline bool keep_running();

    [[nodiscard]] inline Iteration remaining_iterations() const;

private:
    void start_keep_running()
    {
        flags_.started = 1;
        total_iterations_ = skipped() ? 0 : max_iterations;
        if (BENCHMARK_BUILTIN_EXPECT(profiler_manager_ != nullptr, false)) {
            profiler_manager_->AfterSetupStart();
        }
        manager_->StartStopBarrier();
        if (!skipped()) {
            ResumeTiming();
        }
    }

    struct Flags {
        uint8_t started : 1;
        uint8_t finished : 1;
        uint8_t skipped : 1;
    };

    Iteration total_iterations_;
    Iteration remaining_iterations_;
    Flags flags_ {};
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

[[nodiscard]] bool BenchmarkState::keep_running()
{
    if (!flags_.started) {
        flags_.started = true;
    }
    if (remaining_iterations_-- > 0) { }
}

[[nodiscard]] Iteration BenchmarkState::remaining_iterations() const
{
    return remaining_iterations_;
}

} // namespace uscope

#endif // USCOPE_HPP_
