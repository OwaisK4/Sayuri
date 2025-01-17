#pragma once

#include <vector>
#include <algorithm>
#include <type_traits>
#include <cmath>

#define LOGIT_EPS  (1e-8f)
#define LOGIT_ZERO (-1e6f)

template<
    typename T,
    typename = std::enable_if_t<
                   std::is_floating_point<T>::value
               >
>
std::vector<T> Softmax(std::vector<T> &logits, double temp) {
    auto output = std::vector<T>{};
    output.reserve(logits.size());

    const auto alpha = *std::max_element(
                           std::begin(logits), std::end(logits));
    auto denom = 0.0f;

    for (const auto logit : logits) {
        auto val = std::exp((logit - alpha) / temp);
        denom += val;
        output.emplace_back(val);
    }

    for (auto &out : output) {
        out /= denom;
    }
    return output;
}

template<
    typename T,
    typename = std::enable_if_t<
                   std::is_floating_point<T>::value
               >
>
inline std::vector<T> GetZeroLogits(size_t size) {
    return std::vector<T>(size, LOGIT_ZERO);
}

template<
    typename T,
    typename = std::enable_if_t<
                   std::is_floating_point<T>::value
               >
>
inline T SafeLog(T v) {
    return std::log(static_cast<double>(v) + LOGIT_EPS);
}
