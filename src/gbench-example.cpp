#include <benchmark/benchmark.h>

#include <array>
#include <cstring>
#include <format>
#include <sstream>

namespace {

enum class LogColor : uint8_t {
    DEFAULT,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    WHITE
};

const char* GetAnsiColorCode(LogColor color)
{
    switch (color) {
    case LogColor::RED:
        return "1";
    case LogColor::GREEN:
        return "2";
    case LogColor::YELLOW:
        return "3";
    case LogColor::BLUE:
        return "4";
    case LogColor::MAGENTA:
        return "5";
    case LogColor::CYAN:
        return "6";
    case LogColor::WHITE:
        return "7";
    default:
        return nullptr;
    };
}

std::string GetBigOString(benchmark::BigO complexity)
{
    switch (complexity) {
    case benchmark::oN:
        return "N";
    case benchmark::oNSquared:
        return "N^2";
    case benchmark::oNCubed:
        return "N^3";
    case benchmark::oLogN:
        return "lgN";
    case benchmark::oNLogN:
        return "NlgN";
    case benchmark::o1:
        return "(1)";
    default:
        return "f(N)";
    }
}

std::string FormatTime(double time)
{
    // For the time columns of the console printer 13 digits are reserved. One of
    // them is a space and max two of them are the time unit (e.g ns). That puts
    // us at 10 digits usable for the number.
    // Align decimal places...
    if (time < 1.0) {
        return std::format("{:10.3f}", time);
    }
    if (time < 10.0) {
        return std::format("{:10.2f}", time);
    }
    if (time < 100.0) {
        return std::format("{:10.1f}", time);
    }
    // Assuming the time is at max 9.9999e+99 and we have 10 digits for the
    // number, we get 10-1(.)-1(e)-1(sign)-2(exponent) = 5 digits to print.
    if (time > 9999999999 /*max 10 digit number*/) {
        return std::format("{:1.4e}", time);
    }
    return std::format("{:10.0f}", time);
}
// kilo, Mega, Giga, Tera, Peta, Exa, Zetta, Yotta.
const char* const kBigSIUnits[] = { "k", "M", "G", "T", "P", "E", "Z", "Y" };
// Kibi, Mebi, Gibi, Tebi, Pebi, Exbi, Zebi, Yobi.
const char* const kBigIECUnits[] = { "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };
// milli, micro, nano, pico, femto, atto, zepto, yocto.
const char* const kSmallSIUnits[] = { "m", "u", "n", "p", "f", "a", "z", "y" };

constexpr int64_t kUnitsSize = std::size(kBigSIUnits);

void ToExponentAndMantissa(
    double val,
    int precision,
    double one_k,
    std::string* mantissa,
    int64_t* exponent)
{
    std::stringstream mantissa_stream;

    if (val < 0) {
        mantissa_stream << "-";
        val = -val;
    }

    // Adjust threshold so that it never excludes things which can't be rendered
    // in 'precision' digits.
    const double adjusted_threshold = std::max(1.0, 1.0 / std::pow(10.0, precision));
    const double big_threshold = (adjusted_threshold * one_k) - 1;
    const double small_threshold = adjusted_threshold;
    // Values in ]simple_threshold,small_threshold[ will be printed as-is
    const double simple_threshold = 0.01;

    if (val > big_threshold) {
        // Positive powers
        double scaled = val;
        for (size_t i = 0; i < std::size(kBigSIUnits); ++i) {
            scaled /= one_k;
            if (scaled <= big_threshold) {
                mantissa_stream << scaled;
                *exponent = static_cast<int64_t>(i + 1);
                *mantissa = mantissa_stream.str();
                return;
            }
        }
        mantissa_stream << val;
        *exponent = 0;
    } else if (val < small_threshold) {
        // Negative powers
        if (val < simple_threshold) {
            double scaled = val;
            for (size_t i = 0; i < std::size(kSmallSIUnits); ++i) {
                scaled *= one_k;
                if (scaled >= small_threshold) {
                    mantissa_stream << scaled;
                    *exponent = -static_cast<int64_t>(i + 1);
                    *mantissa = mantissa_stream.str();
                    return;
                }
            }
        }
        mantissa_stream << val;
        *exponent = 0;
    } else {
        mantissa_stream << val;
        *exponent = 0;
    }
    *mantissa = mantissa_stream.str();
}

std::string ExponentToPrefix(int64_t exponent, bool iec)
{
    if (exponent == 0) {
        return {};
    }

    const int64_t index = (exponent > 0 ? exponent - 1 : -exponent - 1);
    if (index >= kUnitsSize) {
        return {};
    }

    const char* const* array = (exponent > 0 ? (iec ? kBigIECUnits : kBigSIUnits) : kSmallSIUnits);

    return { array[index] };
}

std::string ToBinaryStringFullySpecified(
    double value,
    int precision,
    benchmark::Counter::OneK one_k)
{
    std::string mantissa;
    int64_t exponent = 0;
    ToExponentAndMantissa(
        value,
        precision,
        one_k == benchmark::Counter::kIs1024 ? 1024.0 : 1000.0,
        &mantissa,
        &exponent);
    return mantissa + ExponentToPrefix(exponent, one_k == benchmark::Counter::kIs1024);
}

std::string HumanReadableNumber(double n, benchmark::Counter::OneK one_k)
{
    return ToBinaryStringFullySpecified(n, 1, one_k);
}

template<typename... Args>
void IgnoreColorPrint(
    std::ostream& out,
    LogColor /*unused*/,
    std::format_string<Args...> fmt,
    Args&&... args)
{
    out << std::format(std::move(fmt), std::forward<Args>(args)...);
}

template<typename... Args>
void ColorPrint(std::ostream& out, LogColor color, std::format_string<Args...> fmt, Args&&... args)
{
    const char* color_code = GetAnsiColorCode(color);
    if (color_code != nullptr) {
        out << std::format("\033[0;3{}m", color_code);
    }
    out << std::format(fmt, std::forward<Args>(args)...) << "\033[m";
}

// Custom reporter that outputs benchmark data to the console.
class ConsoleReporter : public benchmark::ConsoleReporter {
    void PrintRunData(const Run& result) override
    {
        auto& Out = GetOutputStream();

        auto printer = [this]<typename... Args>(
                           std::ostream& out,
                           LogColor color,
                           std::format_string<Args...> fmt,
                           Args&&... args) {
            if ((output_options_ & OO_Color) != 0) {
                ColorPrint(out, color, fmt, std::forward<decltype(args)>(args)...);
            } else {
                IgnoreColorPrint(out, color, fmt, std::forward<decltype(args)>(args)...);
            }
        };

        auto name_color
            = (result.report_big_o || result.report_rms) ? LogColor::BLUE : LogColor::GREEN;

        printer(Out, name_color, "{:<{}}", result.benchmark_name(), name_field_width_ + 1);

        if (benchmark::internal::SkippedWithError == result.skipped) {
            printer(Out, LogColor::RED, "ERROR OCCURRED: \'{}\'", result.skip_message);
            printer(Out, LogColor::DEFAULT, "\n");
            return;
        }
        if (benchmark::internal::SkippedWithMessage == result.skipped) {
            printer(Out, LogColor::WHITE, "SKIPPED: \'{}\'", result.skip_message);
            printer(Out, LogColor::DEFAULT, "\n");
            return;
        }

        const double real_time = result.GetAdjustedRealTime();
        const double cpu_time = result.GetAdjustedCPUTime();
        const std::string real_time_str = FormatTime(real_time);
        const std::string cpu_time_str = FormatTime(cpu_time);

        if (result.report_big_o) {
            std::string big_o = GetBigOString(result.complexity);
            printer(
                Out,
                LogColor::YELLOW,
                "{:10.2} {:<4} {:10.2} {:<4} ",
                real_time,
                big_o,
                cpu_time,
                big_o);
        } else if (result.report_rms) {
            printer(
                Out,
                LogColor::YELLOW,
                "{:10.0} {:<4} {:10.0} {:<4} ",
                real_time * 100,
                "%",
                cpu_time * 100,
                "%");
        } else if (
            result.run_type != Run::RT_Aggregate
            || result.aggregate_unit == benchmark::StatisticUnit::kTime) {
            const char* timeLabel = GetTimeUnitString(result.time_unit);
            printer(
                Out,
                LogColor::YELLOW,
                "{} {:<4} {} {:<4} ",
                real_time_str,
                timeLabel,
                cpu_time_str,
                timeLabel);
        } else {
            assert(result.aggregate_unit == benchmark::StatisticUnit::kPercentage);
            printer(
                Out,
                LogColor::YELLOW,
                "{:10.2} {:<4} {:10.2} {:<4} ",
                (100. * result.real_accumulated_time),
                "%",
                (100. * result.cpu_accumulated_time),
                "%");
        }

        if (!result.report_big_o && !result.report_rms) {
            printer(Out, LogColor::CYAN, "{:>10d}", result.iterations);
        }

        for (const auto& counter : result.counters) {
            const std::size_t counterNameLen
                = std::max(static_cast<std::size_t>(10), counter.first.length());
            std::string s;
            const char* unit = "";
            if (result.run_type == Run::RT_Aggregate
                && result.aggregate_unit == benchmark::StatisticUnit::kPercentage) {
                s = std::format("{:.2}", 100. * counter.second.value);
                unit = "%";
            } else {
                s = HumanReadableNumber(counter.second.value, counter.second.oneK);
                if ((counter.second.flags & benchmark::Counter::kIsRate) != 0) {
                    unit = (counter.second.flags & benchmark::Counter::kInvert) != 0 ? "s" : "/s";
                }
            }
            if ((output_options_ & OO_Tabular) != 0) {
                printer(
                    Out,
                    LogColor::DEFAULT,
                    " {:>{}}{}",
                    s,
                    counterNameLen - strlen(unit),
                    unit);
            } else {
                printer(Out, LogColor::DEFAULT, " {}={}{}", counter.first, s, unit);
            }
        }

        if (!result.report_label.empty()) {
            printer(Out, LogColor::DEFAULT, " {}", result.report_label);
        }

        printer(Out, LogColor::DEFAULT, "\n");
    }

    // void PrintHeader(const Run& run) override
    // {
    // }
};

} // namespace

const char* hello = "hello";

static void StringCreation(benchmark::State& state)
{
    // Code inside this loop is measured repeatedly
    for (auto _ : state) {
        std::string created_string(hello);
        // Make sure the variable is not optimized away by compiler
        benchmark::DoNotOptimize(created_string);
    }
    state.SetBytesProcessed(strlen(hello));
    state.SetItemsProcessed(state.iterations());
}
// Register the function as a benchmark
BENCHMARK(StringCreation);

static void StringCopy(benchmark::State& state)
{
    // Code before the loop is not measured
    std::string x = hello;
    for (auto _ : state) {
        std::string copy(x);
    }
    state.SetBytesProcessed(strlen(hello));
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(StringCopy);

int main(int argc, char** argv)
{
    benchmark::MaybeReenterWithoutASLR(argc, argv);
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    auto console_reporter = std::make_unique<ConsoleReporter>();
    benchmark::RunSpecifiedBenchmarks(console_reporter.get());
    benchmark::Shutdown();
    return 0;
}