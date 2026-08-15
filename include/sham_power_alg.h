/*伪刺激(Sham)功率算法声明
适用：sham_power_index*/
#ifndef SHAM_POWER_ALG_H
#define SHAM_POWER_ALG_H

#include <Eigen/Dense>
#include "sham_power.h"       // struct/ 下的输出结构体
#include "eeg_alg_export.h"

namespace sham_power {

/**
 * 计算伪刺激(Sham)功率
 *
 * 在随机起点频率 RDN_FREQ_PNT 附近计算 11 个随机频带的功率，用于对照实验；
 * 另计算 50/100 Hz 工频功率，并将实际使用的随机起点回填到 RDN_FREQ_PNT 特征。
 *
 * @param data          EEG 数据矩阵，形状 (采样点 × 通道)
 * @param fs            采样率 (Hz)，要求 fs > 0
 * @param rdn_freq_pnt  随机起点频率 (Hz)；默认 -1.0 表示内部随机生成
 *                      （保证 RDN_FREQ_PNT + 41 < fs/2）
 * @param nperseg       Welch 窗长，默认 256
 * @return Sham_Power_SoA（14 特征 × 通道数）
 *
 * 频带定义（相对 RDN_FREQ_PNT 偏移）：
 *   BAND_0  +0–+1 (1Hz) | BAND_1..10  +1–+5, +5–+9, … , +37–+41 (各 4Hz)
 *   50HZ 49–51 | 100HZ 99–101 | RDN_FREQ_PNT 存实际起点值
 */
EEG_ALG_API Sham_Power_SoA compute(const Eigen::MatrixXd& data, double fs,
                                   double rdn_freq_pnt = -1.0, int nperseg = 256);

/**
 * ──────────────── 输入 / 输出 / 使用方法 ────────────────
 * 输入：
 *   - data         : Eigen::MatrixXd，形状 (采样点 × 通道)
 *   - fs           : 采样率 (Hz)
 *   - rdn_freq_pnt : 随机起点频率 (Hz)；<0 表示内部随机生成（默认 -1.0）
 *   - nperseg      : Welch 窗长，默认 256（可选）
 * 输出：
 *   - Sham_Power_SoA：14 特征 × 通道数；
 *     RDN_FREQ_PNT 特征存实际使用的随机起点（便于复现）。
 * 使用方法：
 * @code
 *   auto res  = sham_power::compute(data, 256.0);          // 自动随机起点
 *   auto res2 = sham_power::compute(data, 256.0, 20.0);    // 指定起点 20Hz（可复现）
 *   double used_rdn = res2.features[sham_power::RDN_FREQ_PNT][0];
 *   double band1    = res2.features[sham_power::SHAM_BAND_1][0];
 * @endcode
 */

} // namespace sham_power

#endif // SHAM_POWER_ALG_H
