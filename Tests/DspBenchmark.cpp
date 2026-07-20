#include "DSP/CutlineEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

int main()
{
    constexpr auto sampleRate = 192000.0;
    constexpr auto blockSize = 512;
    constexpr auto blocks = 4000;
    cutline::dsp::CutlineEngine<double> engine;
    engine.prepare (sampleRate, blockSize);

    cutline::dsp::Parameters parameters;
    parameters.highPassEnabled = true;
    parameters.highPassPoles = 8;
    parameters.highPassFrequency = 20.0f;
    parameters.lowPassEnabled = true;
    parameters.lowPassPoles = 8;
    parameters.lowPassFrequency = 20000.0f;
    engine.setTargets (parameters);

    std::vector<double> left (blockSize);
    std::vector<double> right (blockSize);
    double phase = 0.0;
    double* channels[] { left.data(), right.data() };
    const auto start = std::chrono::steady_clock::now();

    for (int block = 0; block < blocks; ++block)
    {
        parameters.highPassPoles = (block & 1) == 0 ? 7 : 8;
        parameters.lowPassPoles = (block & 1) == 0 ? 8 : 7;
        engine.setTargets (parameters);

        for (int sample = 0; sample < blockSize; ++sample)
        {
            left[static_cast<std::size_t> (sample)] = std::sin (phase);
            right[static_cast<std::size_t> (sample)] = std::cos (phase);
            phase += 0.01;
        }

        engine.process (channels, 2, blockSize);
    }

    const auto elapsed = std::chrono::duration<double> (
        std::chrono::steady_clock::now() - start).count();
    const auto audioDuration = static_cast<double> (blocks * blockSize) / sampleRate;
    std::cout << "Worst-case double precision, 192 kHz, parallel pole transitions: "
              << elapsed << " s for " << audioDuration << " s audio ("
              << audioDuration / elapsed << "x realtime)\n";
    return std::isfinite (left.back()) && std::isfinite (right.back()) ? 0 : 1;
}
