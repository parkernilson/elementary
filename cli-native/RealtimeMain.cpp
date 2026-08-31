#include <array>
#include <iostream>
#include <memory>
#include <vector>

#include "HelloSine.h"
#include "elem/Renderer.h"
#include "elem/Runtime.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"


// A simple struct to proxy between the audio device and the Elementary engine
struct DeviceProxy {
    DeviceProxy(double sampleRate, size_t blockSize)
        : scratchData(2 * blockSize), runtime(std::make_shared<elem::Runtime<float>>(sampleRate, blockSize))
    {}

    void process(float* outputData, size_t numChannels, size_t numFrames)
    {
        // We might hit this the first time around, but after that should be fine
        if (scratchData.size() < (numChannels * numFrames))
            scratchData.resize(numChannels * numFrames);

        auto* deinterleaved = scratchData.data();
        std::array<float*, 2> ptrs {deinterleaved, deinterleaved + numFrames};

        runtime->process(
            nullptr,
            0,
            ptrs.data(),
            numChannels,
            numFrames,
            nullptr
        );

        for (size_t i = 0; i < numChannels; ++i)
        {
            for (size_t j = 0; j < numFrames; ++j)
            {
                outputData[i + numChannels * j] = deinterleaved[i * numFrames + j];
            }
        }
    }

    std::vector<float> scratchData;
    std::shared_ptr<elem::Runtime<float>> runtime;
};

void audioCallback(ma_device* pDevice, void* pOutput, const void* /* pInput */, ma_uint32 frameCount)
{
    auto* proxy = static_cast<DeviceProxy*>(pDevice->pUserData);

    const auto numChannels = static_cast<size_t>(pDevice->playback.channels);
    const auto numFrames = static_cast<size_t>(frameCount);

    proxy->process(static_cast<float*>(pOutput), numChannels, numFrames);
}

int main()
{
    ma_result result;

    ma_device_config deviceConfig;
    ma_device device;

    // XXX: I don't see a way to ask miniaudio for a specific block size. Let's just allocate
    // here for 1024 and resize in the first callback if we need to.
    std::unique_ptr<DeviceProxy> proxy = std::make_unique<DeviceProxy>(44100.0, 1024);

    deviceConfig = ma_device_config_init(ma_device_type_playback);

    deviceConfig.playback.pDeviceID = nullptr;
    deviceConfig.playback.format    = ma_format_f32;
    deviceConfig.playback.channels  = 2;
    deviceConfig.sampleRate         = 44100;
    deviceConfig.dataCallback       = audioCallback;
    deviceConfig.pUserData          = proxy.get();

    result = ma_device_init(nullptr, &deviceConfig, &device);

    if (result != MA_SUCCESS) {
        std::cout << "Failed to start the audio device! Exiting..." << std::endl;
        return 1;
    }

    // Build and render the graph before audio starts
    elem::Renderer<float> renderer(proxy->runtime);
    auto stats = renderer.renderGraph(elem::lib::buildHelloSineGraph());

    std::cout << "Render result: " << stats.result << std::endl;
    std::cout << "Nodes added: " << stats.nodesAdded << std::endl;
    std::cout << "Edges added: " << stats.edgesAdded << std::endl;
    std::cout << "Props written: " << stats.propsWritten << std::endl;

    if (stats.result != elem::ReturnCode::Ok()) {
        std::cerr << "Failed to render graph: " << elem::ReturnCode::describe(stats.result) << std::endl;
        ma_device_uninit(&device);
        return 1;
    }

    result = ma_device_start(&device);

    if (result != MA_SUCCESS) {
        std::cout << "Failed to start the audio device! Exiting..." << std::endl;
        ma_device_uninit(&device);
        return 1;
    }

    std::cout << "Press Enter to exit..." << std::endl;
    getchar();

    ma_device_uninit(&device);
    return 0;
}
