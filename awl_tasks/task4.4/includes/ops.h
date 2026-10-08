#ifndef OPTS_H
#define OPTS_H

#include <vector>
#include <algorithm>
#include <cmath>

inline std::vector<float> softmax(const std::vector<float>& logits) {
    std::vector<float> exp_logits(logits.size());
    float sum_exp = 0.0f;

    for (size_t i = 0; i < logits.size(); ++i) {
        exp_logits[i] = std::exp(logits[i]);
        sum_exp += exp_logits[i];
    }

    for (size_t i = 0; i < exp_logits.size(); ++i) {
        exp_logits[i] /= sum_exp;
    }

    return exp_logits;
}

inline unsigned int argmax(const std::vector<float>& vec) {
    return (unsigned int)std::distance(vec.begin(), std::max_element(vec.begin(), vec.end()));
}

#endif