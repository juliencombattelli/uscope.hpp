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
    class Iterations;

    explicit BenchmarkState(Iteration iteration_count)
        : remaining_iterations_(iteration_count)
    {
    }

    [[nodiscard]] inline Iterations iterate() const;

    [[nodiscard]] inline Iteration remaining_iterations() const;

private:
    Iteration remaining_iterations_;
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

class BenchmarkState::Iterations {
public:
    struct [[nodiscard]] Iterator {
        Iteration remaining_iterations;

        friend Iteration operator*(Iterator& self)
        {
            return self.remaining_iterations;
        }

        friend Iterator& operator++(Iterator& self)
        {
            --self.remaining_iterations;
            return self;
        }

        friend bool operator!=(Iterator const& lhs, Iterator const& /*rhs*/)
        {
            if (lhs.remaining_iterations != 0) [[likely]] {
                return true;
            }
            return false;
        }
    };

    explicit Iterations(Iteration iteration_count)
        : iteration_count_(iteration_count)
    {
    }

    Iterator begin() const
    {
        return Iterator(iteration_count_);
    }

    static Iterator end()
    {
        return Iterator(0);
    }

private:
    Iteration iteration_count_;
};

[[nodiscard]] BenchmarkState::Iterations BenchmarkState::iterate() const
{
    return BenchmarkState::Iterations(remaining_iterations_);
}

[[nodiscard]] Iteration BenchmarkState::remaining_iterations() const
{
    return remaining_iterations_;
}

} // namespace uscope

#endif // USCOPE_HPP_
