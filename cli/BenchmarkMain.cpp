#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Benchmark.h"
#include "RendererBenchmarkScenario.h"
#include "RendererBenchmarks.h"

namespace {
    void printUsage() {
        std::cout <<
            "Usage:\n"
            "  elembench runtime <file.js> [--float-type=float|double|both]\n"
            "  elembench renderer js <file.js> [--float-type=float|double|both]\n"
            "  elembench renderer native <scenario-name> [--float-type=float|double|both]\n"
            "  elembench renderer native --list\n";
    }

    enum class FloatTypeOption { Float, Double, Both };

    // Pulls --float-type=... out of args if present (defaulting to Both),
    // leaving the remaining positional arguments behind.
    FloatTypeOption parseFloatTypeOption(std::vector<std::string>& args) {
        constexpr auto prefix = "--float-type=";

        for (auto it = args.begin(); it != args.end(); ++it) {
            if (it->rfind(prefix, 0) == 0) {
                auto const value = it->substr(std::string(prefix).size());
                args.erase(it);

                if (value == "float") return FloatTypeOption::Float;
                if (value == "double") return FloatTypeOption::Double;
                if (value == "both") return FloatTypeOption::Both;

                throw std::invalid_argument("Unknown --float-type value: " + value);
            }
        }

        return FloatTypeOption::Both;
    }

    void runRuntimeBenchmark(std::string const& inputFileName, FloatTypeOption floatType) {
        if (floatType == FloatTypeOption::Float || floatType == FloatTypeOption::Both) {
            runBenchmark<float>("Float", inputFileName, [](auto&) {});
        }

        if (floatType == FloatTypeOption::Double || floatType == FloatTypeOption::Both) {
            runBenchmark<double>("Double", inputFileName, [](auto&) {});
        }
    }

    template <typename FloatType>
    void runJSRendererBenchmark(std::string const& name, std::string const& inputFileName) {
        auto runtime = std::make_shared<elem::Runtime<FloatType>>(44100.0, 512);
        auto [build, render] = benchmark::makeJSGraphFns<FloatType>(runtime, inputFileName);
        benchmark::RendererBenchmarkScenario<FloatType> scenario(name, std::move(build), std::move(render));
        scenario.runBenchmark();
    }

    void runRendererJSBenchmark(std::string const& inputFileName, FloatTypeOption floatType) {
        if (floatType == FloatTypeOption::Float || floatType == FloatTypeOption::Both) {
            runJSRendererBenchmark<float>("Float", inputFileName);
        }

        if (floatType == FloatTypeOption::Double || floatType == FloatTypeOption::Both) {
            runJSRendererBenchmark<double>("Double", inputFileName);
        }
    }

    template <typename FloatType>
    void runNativeRendererBenchmark(std::string const& name, std::string const& scenarioName) {
        auto const& scenarios = benchmark::nativeRendererScenarios<FloatType>();
        auto const it = scenarios.find(scenarioName);

        if (it == scenarios.end()) {
            throw std::invalid_argument("Unknown native renderer scenario: " + scenarioName);
        }

        auto runtime = std::make_shared<elem::Runtime<FloatType>>(44100.0, 512);
        auto [build, render] = it->second(runtime);
        benchmark::RendererBenchmarkScenario<FloatType> scenario(name, std::move(build), std::move(render));
        scenario.runBenchmark();
    }

    void runRendererNativeBenchmark(std::string const& scenarioName, FloatTypeOption floatType) {
        if (floatType == FloatTypeOption::Float || floatType == FloatTypeOption::Both) {
            runNativeRendererBenchmark<float>("Float", scenarioName);
        }

        if (floatType == FloatTypeOption::Double || floatType == FloatTypeOption::Both) {
            runNativeRendererBenchmark<double>("Double", scenarioName);
        }
    }

    void listNativeRendererScenarios() {
        std::cout << "Available native renderer scenarios:" << std::endl;

        for (auto const& [name, factory] : benchmark::nativeRendererScenarios<float>()) {
            (void) factory;
            std::cout << "  " << name << std::endl;
        }
    }
}

int main(int argc, char **argv)
{
#ifndef NDEBUG
    std::cerr << "warning: elembench was built without NDEBUG (a Debug build) -- "
                  "timings will include debug-logging overhead and unoptimized code, "
                  "and won't reflect real performance. Rebuild with -DCMAKE_BUILD_TYPE=Release."
              << std::endl << std::endl;
#endif

    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) {
        printUsage();
        return 1;
    }

    try {
        auto const floatType = parseFloatTypeOption(args);
        auto const subcommand = args.at(0);

        if (subcommand == "runtime") {
            if (args.size() < 2) throw std::invalid_argument("Missing argument: what file do you want to run?");
            runRuntimeBenchmark(args.at(1), floatType);
            return 0;
        }

        if (subcommand == "renderer") {
            if (args.size() < 2) throw std::invalid_argument("Missing argument: js or native?");
            auto const rendererKind = args.at(1);

            if (rendererKind == "js") {
                if (args.size() < 3) throw std::invalid_argument("Missing argument: what file do you want to run?");
                runRendererJSBenchmark(args.at(2), floatType);
                return 0;
            }

            if (rendererKind == "native") {
                if (args.size() >= 3 && args.at(2) == "--list") {
                    listNativeRendererScenarios();
                    return 0;
                }

                if (args.size() < 3) throw std::invalid_argument("Missing argument: which scenario do you want to run?");
                runRendererNativeBenchmark(args.at(2), floatType);
                return 0;
            }

            throw std::invalid_argument("Unknown renderer kind: " + rendererKind);
        }

        throw std::invalid_argument("Unknown subcommand: " + subcommand);
    } catch (std::exception const& e) {
        std::cout << e.what() << std::endl << std::endl;
        printUsage();
        return 1;
    }
}
