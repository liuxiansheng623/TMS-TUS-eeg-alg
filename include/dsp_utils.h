/*共享 DSP 工具
提供 FFT / Welch 功率谱 / 带通滤波 / 希尔伯特变换 / CoG 等原语，
供 absolute_power、sham_power、phase_locking、pac_asymmetry 四个算法复用。
基于 FFTW3（双精度）与 Eigen 实现。*/
/*
 * 典型使用流程（供四个算法内部复用，也可独立使用）：
 * @code
 *   Eigen::VectorXd x = ...;                          // 单通道时域信号
 *   auto spec = dsp::welch_psd(x, 256.0);             // 功率谱 (freqs, psd)
 *   double alpha_pow = dsp::band_power(spec.first, spec.second, 8.0, 13.0);
 *   double apf = dsp::compute_apf_cog(spec.first, spec.second);   // Alpha 峰值频率
 *   auto filt = dsp::bandpass_filter(x, 256.0, 8.0, 13.0);        // 带通
 *   auto hb = dsp::hilbert_transform(filt);           // (瞬时相位, 瞬时振幅)
 * @endcode
 */
#ifndef DSP_UTILS_H
#define DSP_UTILS_H

#include <Eigen/Dense>
#include <vector>
#include <utility>
#include "eeg_alg_export.h"

namespace dsp {

/**
 * 汉宁窗（Hann window，对称型）
 * @param n 窗长度
 * @return 长度为 n 的窗系数向量
 */
EEG_ALG_API Eigen::VectorXd hann_window(int n);

/**
 * Welch 功率谱密度估计（单边 PSD）
 * 每段先去均值和最小二乘线性趋势，再采用汉宁窗、50% 重叠分段，
 * 并零填充至下一个 2 的幂。
 * @param signal   输入时域信号（单通道）
 * @param fs       采样率 (Hz)，要求 fs > 0
 * @param nperseg  每段采样数（窗长），默认 256；会自动夹紧到信号长度
 * @return (freqs, psd)：频率向量(Hz) 与对应单边 PSD（单位 V²/Hz）
 */
EEG_ALG_API std::pair<Eigen::VectorXd, Eigen::VectorXd> welch_psd(
    const Eigen::VectorXd& signal, double fs, int nperseg = 256);

/**
 * 频带功率（对 PSD 在 [f_low, f_high] 内做梯形积分）
 * @param freqs  频率向量
 * @param psd    PSD 向量
 * @param f_low  频带下界 (Hz)
 * @param f_high 频带上界 (Hz)
 * @return 频带内功率（V²）
 */
EEG_ALG_API double band_power(const Eigen::VectorXd& freqs, const Eigen::VectorXd& psd,
                  double f_low, double f_high);

/**
 * FFT 带通滤波（频域掩码 + 余弦过渡带）
 * @param signal     输入时域信号
 * @param fs         采样率 (Hz)
 * @param f_low      通带下界 (Hz)
 * @param f_high     通带上界 (Hz)
 * @param transition 过渡带宽度 (Hz)，默认 0.5
 * @return 滤波后的时域信号（与输入等长）
 */
EEG_ALG_API Eigen::VectorXd bandpass_filter(const Eigen::VectorXd& signal, double fs,
                                double f_low, double f_high,
                                double transition = 0.5);

/**
 * 希尔伯特变换 → 解析信号（瞬时相位 + 瞬时振幅）
 * @param signal 输入时域信号（建议先经带通滤波）
 * @return (inst_phase, inst_amplitude)：瞬时相位[-π,π] 与瞬时振幅
 */
EEG_ALG_API std::pair<Eigen::VectorXd, Eigen::VectorXd> hilbert_transform(
    const Eigen::VectorXd& signal);

/**
 * 带通 + 希尔伯特合并：直接求带通信号的解析信号（瞬时相位 + 瞬时振幅）
 * 等价于先 bandpass_filter() 再 hilbert_transform()，但只做一次正变换 + 一次逆变换，
 * FFT 次数减半；适合 PAC、相位锁定等大量“带通→相位/振幅”的场景复用。
 * @param signal     输入时域信号
 * @param fs         采样率 (Hz)，要求 fs > 0
 * @param f_low      通带下界 (Hz)
 * @param f_high     通带上界 (Hz)
 * @param transition 过渡带宽度 (Hz)，默认 0.5
 * @return (inst_phase, inst_amplitude)：带通后信号的瞬时相位[-π,π] 与瞬时振幅
 */
EEG_ALG_API std::pair<Eigen::VectorXd, Eigen::VectorXd> bandpass_analytic(
    const Eigen::VectorXd& signal, double fs,
    double f_low, double f_high, double transition = 0.5);

/**
 * 预创建并缓存指定尺寸 n 的实→复(r2c) FFTW plan（线程安全）。
 * 在并行调用 welch_psd / bandpass_filter / bandpass_analytic 等之前预热，
 * 可避免并行区内首次创建 plan（FFTW 的 planner 非线程安全，不能与 execute 并发）。
 * @param n 输入：FFT 尺寸（实数信号长度）。
 */
EEG_ALG_API void warmup_r2c(int n);

/**
 * 预创建并缓存指定尺寸 n 的复→复逆向(c2c backward) FFTW plan（线程安全）。
 * @param n 输入：FFT 尺寸（复数序列长度）。
 */
EEG_ALG_API void warmup_c2c_backward(int n);

/**
 * 重心法计算 Alpha 峰值频率（Center of Gravity）
 * APF = Σ(f·PSD) / Σ(PSD)，在 [cog_low, cog_high] 内
 * @param freqs    频率向量
 * @param psd      PSD 向量
 * @param cog_low  积分下界，默认 8.0 Hz
 * @param cog_high 积分上界，默认 13.0 Hz
 * @return APF (Hz)；若频带内总功率≈0 则返回 NaN
 */
EEG_ALG_API double compute_apf_cog(const Eigen::VectorXd& freqs, const Eigen::VectorXd& psd,
                       double cog_low = 8.0, double cog_high = 13.0);

/**
 * @brief 自动确定 Welch 窗长，并夹紧到不超过信号长度。
 * @param signal_length 输入：信号总采样点数。
 * @param requested 输入：期望的 Welch 分段长度，默认 256。
 * @return 输出：实际使用的分段长度；空信号返回 0。
 */
EEG_ALG_API int auto_nperseg(int signal_length, int requested = 256);

/**
 * @brief 计算不小于 n 的最小 2 的幂，用于 FFT 零填充。
 * @param n 输入：目标整数，正常调用要求 n>=1。
 * @return 输出：满足 p>=n 的最小 2 的幂 p。
 */
EEG_ALG_API int next_power_of_2(int n);

} // namespace dsp

#endif // DSP_UTILS_H
