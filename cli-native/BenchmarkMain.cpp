#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "HelloSine.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"


template <typename FloatType>
void runBenchmark(std::string const& name) {
    auto runtime = std::make_shared<elem::Runtime<FloatType>>(44100.0, 512);
    elem::Renderer<FloatType> renderer(runtime);

    // Build and render the native graph — no JS/QuickJS involved
    auto renderStats = renderer.renderGraph(elem::lib::buildHelloSineGraph());
    std::cout << "Render result: " << renderStats.result << std::endl;

    std::vector<std::vector<FloatType>> scratchBuffers;
    std::vector<FloatType*> scratchPointers;

    for (int i = 0; i < 2; ++i) {
        scratchBuffers.push_back(std::vector<FloatType>(512));
        scratchPointers.push_back(scratchBuffers[i].data());
    }

    // Run the first block to process the events
    runtime->process(
        nullptr,
        0,
        scratchPointers.data(),
        2,
        512,
        0
    );

    // Now we can measure the static render process. We have this sleep
    // here to clearly demarcate, in the profiling timeline, which work is
    // related to the event processing above and which work is related to this
    // work loop here
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<double> deltas;

    for (size_t i = 0; i < 10'000; ++i) {
        auto t0 = std::chrono::steady_clock::now();

        runtime->process(
            nullptr,
            0,
            scratchPointers.data(),
            2,
            512,
            0
        );

        auto t1 = std::chrono::steady_clock::now();
        auto diffms = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        deltas.push_back(static_cast<double>(diffms));
    }

    // Reporting
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto const sum = std::accumulate(deltas.begin(), deltas.end(), 0.0);
    auto const avg = sum / (double) deltas.size();

    std::cout << "[Running " << name << "]:" << std::endl;
    std::cout << "Total run time: " << sum << "us " << "(" << (sum / 1000000) << "s)" << std::endl;
    std::cout << "Average iteration time: " << avg << "us" << std::endl;
    std::cout << "Done" << std::endl << std::endl;
}

int main()
{
    runBenchmark<float>("Float");
    runBenchmark<double>("Double");

    return 0;
}
