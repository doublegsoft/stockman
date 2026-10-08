/*                                                                                                             
**        ██               ▄▄                          
** ▄█▀▀▀ ▀██▀▀ ▄███▄ ▄████ ██ ▄█▀ ███▄███▄  ▀▀█▄ ████▄ 
** ▀███▄  ██   ██ ██ ██    ████   ██ ██ ██ ▄█▀██ ██ ██ 
** ▄▄▄█▀  ██   ▀███▀ ▀████ ██ ▀█▄ ██ ██ ██ ▀█▄██ ██ ██    
** 
** Copyright 2026 doublegsoft.me
** 
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
** 
**     http://www.apache.org/licenses/LICENSE-2.0
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#pragma once

#include "kchart.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

/**
  * @brief K 线形态 AI 向量化编码器
  * 默认将任意 K 线片段编码为 128 维特征向量 (32 步长 × 4 个微观几何特征)
  */
class CandleVectorizer {
public:
  static constexpr int DEFAULT_TARGET_STEPS = 32; // 采样步长
  static constexpr int FEATURES_PER_STEP    = 4;  // 每步特征数 (趋势, 实体, 上影, 下影)
  static constexpr int VECTOR_DIM           = DEFAULT_TARGET_STEPS * FEATURES_PER_STEP; // 128 维

  /**
    * @brief 将一段连续 K 线向量化为固定 128 维的高维特征向量
    * @param slice 待编码的 K 线窗口序列
    * @return 经过 L2 归一化的 float 向量 (可直接存入 Milvus / FAISS)
    */
  static std::vector<float> Embed(const std::vector<Candle>& slice, int target_steps = DEFAULT_TARGET_STEPS) {
    if (slice.empty()) return std::vector<float>(target_steps * FEATURES_PER_STEP, 0.0f);

    // 1. 时序长度对齐：将原序列重采样为固定的 target_steps 步长
    std::vector<Candle> resampled = Resample(slice, target_steps);

    // 2. 计算全局极值，实现幅度无量纲化 (消除股价高低差异)
    float global_min = resampled[0].low;
    float global_max = resampled[0].high;
    for (const auto& c : resampled) {
      if (c.low < global_min) global_min = c.low;
      if (c.high > global_max) global_max = c.high;
    }
    float price_range = global_max - global_min;
    if (price_range < 1e-6f) price_range = 1.0f;

    std::vector<float> embedding;
    embedding.reserve(target_steps * FEATURES_PER_STEP);

    // 3. 多维度形态特征抽取
    for (int i = 0; i < target_steps; ++i) {
      const auto& c = resampled[i];

      // 特征 1: 趋势位置 (收盘价在全局波动区间中的百分位 0.0 ~ 1.0)
      float norm_close = (c.close - global_min) / price_range;

      // 单根蜡烛内几何微观特征 (消除单根 K 线振幅差异)
      float candle_range = c.high - c.low;
      if (candle_range < 1e-6f) candle_range = 1e-6f;

      float top_body = std::max(c.open, c.close);
      float bot_body = std::min(c.open, c.close);

      // 特征 2: 实体比例及方向 (-1.0 满阴线 ~ +1.0 满阳线)
      float body_ratio = (c.close - c.open) / candle_range;

      // 特征 3: 上影线比例 (压力位特征 0.0 ~ 1.0)
      float upper_shadow = (c.high - top_body) / candle_range;

      // 特征 4: 下影线比例 (支撑位特征 0.0 ~ 1.0)
      float lower_shadow = (bot_body - c.low) / candle_range;

      embedding.push_back(norm_close);
      embedding.push_back(body_ratio);
      embedding.push_back(upper_shadow);
      embedding.push_back(lower_shadow);
    }

    // 4. L2 模长归一化 (||v|| = 1)，以便后续使用点积直接计算余弦相似度
    float sum_sq = 0.0f;
    for (float v : embedding) sum_sq += v * v;
    float norm = std::sqrt(sum_sq);
    if (norm > 1e-6f) {
      for (float& v : embedding) v /= norm;
    }

    return embedding;
  }

  /**
    * @brief 余弦相似度比对 (Cosine Similarity)
    * @return 相似度得分 (-1.0 ~ 1.0)，越接近 1.0 形态越吻合
    */
  static float CosineSimilarity(const std::vector<float>& vec_a, const std::vector<float>& vec_b) {
    if (vec_a.size() != vec_b.size() || vec_a.empty()) return 0.0f;
    float dot = 0.0f;
    for (size_t i = 0; i < vec_a.size(); ++i) {
      dot += vec_a[i] * vec_b[i];
    }
    return dot; // 因已做 L2 归一化，点积即为余弦相似度
  }

private:
  /**
    * @brief 线性插值重采样，统一时序维度
    */
  static std::vector<Candle> Resample(const std::vector<Candle>& src, int target_len) {
    if ((int)src.size() == target_len) return src;

    std::vector<Candle> dst(target_len);
    float step = (float)(src.size() - 1) / (float)(target_len - 1);

    for (int i = 0; i < target_len; ++i) {
      float src_idx = i * step;
      int i0 = (int)src_idx;
      int i1 = std::min(i0 + 1, (int)src.size() - 1);
      float t = src_idx - i0;

      dst[i].open  = src[i0].open  + t * (src[i1].open  - src[i0].open);
      dst[i].high  = src[i0].high  + t * (src[i1].high  - src[i0].high);
      dst[i].low   = src[i0].low   + t * (src[i1].low   - src[i0].low);
      dst[i].close = src[i0].close + t * (src[i1].close - src[i0].close);
    }
    return dst;
  }
};