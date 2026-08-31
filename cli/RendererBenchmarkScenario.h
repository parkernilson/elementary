#pragma once
#include <chrono>
#include <string>
#include <vector>

#include "choc/javascript/choc_javascript.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"
#include "elem/lib/NodeUtils.h"

namespace benchmark {
    struct BuildAudioGraphStats {
        long long timeToBuildAudioGraph;
    };

    struct RenderAudioGraphStats {
        long long timeToRenderAudioGraph;
    };

    template <typename FloatType>
    class RendererBenchmarkScenario {
    public:
        RendererBenchmarkScenario(std::string name);
        virtual ~RendererBenchmarkScenario() = default;

        void runBenchmark();
    private:
        virtual BuildAudioGraphStats buildNextAudioGraph(size_t i) = 0;
        virtual RenderAudioGraphStats renderNextAudioGraph(size_t i) = 0;

        std::string mName;
    protected:
        std::shared_ptr<elem::Runtime<FloatType>> mRuntime;
    };

    template <typename FloatType>
    class JSRendererScenario : public RendererBenchmarkScenario<FloatType> {
    public:
        JSRendererScenario(std::string name, std::string jsFileName);
    private:
        BuildAudioGraphStats buildNextAudioGraph(size_t i) override;
        RenderAudioGraphStats renderNextAudioGraph(size_t i) override;

        choc::javascript::Context mJSCtx;
    };

    template <typename FloatType>
    class NativeRendererScenario : public RendererBenchmarkScenario<FloatType> {
    public:
        explicit NativeRendererScenario(std::string name);
    private:
        BuildAudioGraphStats buildNextAudioGraph(size_t i) override;
        RenderAudioGraphStats renderNextAudioGraph(size_t i) override;

        virtual elem::lib::NodeRepr buildNextAudioGraph_(size_t i) = 0;

        elem::Renderer<FloatType> mRenderer;
        elem::lib::NodeRepr mCurGraph;
    };
}

