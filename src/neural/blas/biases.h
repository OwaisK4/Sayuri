#pragma once

#include <vector>
#include <cstddef>

class AddSpatialBiases {
public:
    AddSpatialBiases() = delete;
    static void Forward(const size_t board_size,
                        const size_t channels,
                        std::vector<float> &input,
                        const std::vector<float> &biases,
                        bool ReLU);

    static void Forward(const size_t board_size,
                        const size_t channels,
                        std::vector<float> &input,
                        const std::vector<float> &biases,
                        std::vector<float> &residual,
                        bool ReLU);
};

class AddVectorBiases {
public:
    AddVectorBiases() = delete;
    static void Forward(const size_t size,
                        std::vector<float> &input,
                        const std::vector<float> &biases,
                        bool ReLU = false);
};
