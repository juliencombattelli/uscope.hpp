#include "uscope.hpp"

#include <thread>

namespace {

using namespace std::chrono_literals;

void test_sleep_1ms(uscope::BenchmarkState& state)
{
    while (state.keep_running()) {
        std::this_thread::sleep_for(1ms);
    }
}

} // namespace

int main()
{
    uscope::BenchmarkRunner runner(
        uscope::Config {
            .iteration_count = 10,
        });
    runner.add_benchmark("test_sleep_1ms", &test_sleep_1ms);
    runner.run_all_benchmarks();
}
