/*共享 DSP 工具实现
基于 FFTW3（双精度）与 Eigen。*/
#include "dsp_utils.h"

#include <fftw3.h>
#include <cmath>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <algorithm>

namespace dsp {

namespace {
constexpr double PI = 3.14159265358979323846;

// ---- FFTW 资源的 RAII 封装（保证异常安全、无泄漏）----
struct RealDeleter {
    /**
     * @brief 释放 FFTW 分配的实数数组。
     * @param p 输入：由 fftw_alloc_real() 返回的指针；允许为空。
     * @return 输出：无；非空指针指向的内存被释放。
     */
    void operator()(double* p) const { if (p) fftw_free(p); }
};

struct CpxDeleter {
    /**
     * @brief 释放 FFTW 分配的复数数组。
     * @param p 输入：由 fftw_alloc_complex() 返回的指针；允许为空。
     * @return 输出：无；非空指针指向的内存被释放。
     */
    void operator()(fftw_complex* p) const { if (p) fftw_free(p); }
};

using RealPtr = std::unique_ptr<double, RealDeleter>;
using CpxPtr  = std::unique_ptr<fftw_complex, CpxDeleter>;

// FFTW 的 plan 创建/销毁非线程安全，用互斥量保护（execute 本身线程安全）
std::mutex g_fftw_mutex;

// ---- FFTW plan 缓存：同 (类型, 尺寸) 的变换复用 plan，避免反复创建/销毁 ----
enum class PlanKind { R2C, C2C_Backward };

struct PlanKey {
    PlanKind kind;
    int n;
    bool operator==(const PlanKey& o) const { return kind == o.kind && n == o.n; }
};

struct PlanKeyHash {
    size_t operator()(const PlanKey& k) const {
        return std::hash<long long>{}(static_cast<long long>(k.kind) * 1000003LL + k.n);
    }
};

// 缓存的 plan 在进程生命周期内复用（实际只会用到少数几种尺寸）。
std::unordered_map<PlanKey, fftw_plan, PlanKeyHash> g_plan_cache;

// 调用方必须已持有 g_fftw_mutex。返回缓存的 plan；不存在则创建并缓存。
// plan 以 FFTW_ESTIMATE 针对临时对齐缓冲区创建，之后经 new-array execute
// 复用于任意“同尺寸、同对齐”的缓冲区（fftw_alloc 保证 SIMD 对齐）。
fftw_plan get_plan_locked(PlanKind kind, int n) {
    const PlanKey key{kind, n};
    auto it = g_plan_cache.find(key);
    if (it != g_plan_cache.end()) return it->second;

    fftw_plan plan = nullptr;
    if (kind == PlanKind::R2C) {
        double* in = fftw_alloc_real(n);
        fftw_complex* out = fftw_alloc_complex(n / 2 + 1);
        plan = fftw_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE);
        fftw_free(in);
        fftw_free(out);
    } else { // PlanKind::C2C_Backward
        fftw_complex* in = fftw_alloc_complex(n);
        fftw_complex* out = fftw_alloc_complex(n);
        plan = fftw_plan_dft_1d(n, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
        fftw_free(in);
        fftw_free(out);
    }
    g_plan_cache[key] = plan;
    return plan;
}
} // namespace

/**
 * @brief 生成长度为 n 的对称 Hann 窗。
 * @param n 输入：窗长度；n<=0 返回空向量，n==1 返回单个 1。
 * @return 输出：长度为 n 的 Hann 窗系数向量。
 */
Eigen::VectorXd hann_window(int n) {
    Eigen::VectorXd w(n);
    if (n <= 0) return w;
    if (n == 1) { w[0] = 1.0; return w; }
    for (int i = 0; i < n; ++i)
        w[i] = 0.5 * (1.0 - std::cos(2.0 * PI * i / (n - 1)));
    return w;
}

/**
 * @brief 计算不小于 n 的最小 2 的整数次幂。
 * @param n 输入：目标整数，正常调用要求 n>=1。
 * @return 输出：满足 p>=n 的最小 2 的幂 p。
 */
int next_power_of_2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

/**
 * @brief 将 Welch 分段长度限制在有效信号长度以内。
 * @param signal_length 输入：信号采样点数。
 * @param requested 输入：用户请求的分段长度。
 * @return 输出：min(requested, signal_length)；空信号返回 0。
 */
int auto_nperseg(int signal_length, int requested) {
    if (signal_length <= 0) return 0;
    return std::min(requested, signal_length);
}

/**
 * @brief 使用 Welch 方法估计单通道信号的单边功率谱密度。
 * @param signal 输入：单通道时域采样向量。
 * @param fs 输入：采样率，单位 Hz，要求 fs>0。
 * @param nperseg 输入：每个 Welch 分段的采样点数。
 * @return 输出：二元组 (freqs, psd)，分别为频率坐标 Hz 和单边 PSD。
 * @details 每段先去均值和线性趋势，再应用 Hann 窗、50% 重叠并平均周期图。
 */
std::pair<Eigen::VectorXd, Eigen::VectorXd> welch_psd(
    const Eigen::VectorXd& signal, double fs, int nperseg) {

    const int L = static_cast<int>(signal.size());
    nperseg = auto_nperseg(L, nperseg);
    if (nperseg < 2 || fs <= 0.0) {
        // 退化情况：返回单点零谱，避免崩溃
        return {Eigen::VectorXd::Zero(1), Eigen::VectorXd::Zero(1)};
    }

    const int nfft    = next_power_of_2(nperseg);
    const int overlap = nperseg / 2;                 // 50% 重叠
    const int step    = nperseg - overlap;
    const int n_segs  = (L - nperseg) / step + 1;    // 完整段数（>=1）
    const int n_freqs = nfft / 2 + 1;                // 单边谱长度

    const Eigen::VectorXd w = hann_window(nperseg);
    const double S2 = w.squaredNorm();               // Σ w[i]²（窗功率）
    const double center = 0.5 * static_cast<double>(nperseg - 1);
    double trend_den = 0.0;
    for (int i = 0; i < nperseg; ++i) {
        const double u = static_cast<double>(i) - center;
        trend_den += u * u;
    }

    Eigen::VectorXd psd = Eigen::VectorXd::Zero(n_freqs);

    fftw_plan plan;
    {
        std::lock_guard<std::mutex> lock(g_fftw_mutex);
        plan = get_plan_locked(PlanKind::R2C, nfft);   // 仅 plan 查询/创建在锁内
    }
    {
        RealPtr in(fftw_alloc_real(nfft));
        CpxPtr  out(fftw_alloc_complex(n_freqs));

        for (int s = 0; s < n_segs; ++s) {
            const int off = s * step;
            // Welch 每个分段先去均值和最小二乘线性趋势，再乘 Hann 窗。
            double mean = 0.0;
            for (int i = 0; i < nperseg; ++i) mean += signal[off + i];
            mean /= static_cast<double>(nperseg);

            double trend_num = 0.0;
            for (int i = 0; i < nperseg; ++i) {
                const double u = static_cast<double>(i) - center;
                trend_num += u * (signal[off + i] - mean);
            }
            const double slope = (trend_den > 0.0) ? trend_num / trend_den : 0.0;

            for (int i = 0; i < nperseg; ++i) {
                const double u = static_cast<double>(i) - center;
                const double detrended = signal[off + i] - mean - slope * u;
                in.get()[i] = detrended * w[i];
            }
            for (int i = nperseg; i < nfft; ++i) in.get()[i] = 0.0;  // 零填充

            fftw_execute_dft_r2c(plan, in.get(), out.get());

            // 周期图 |X|² / (fs · Σw²)
            for (int k = 0; k < n_freqs; ++k) {
                const double re = out.get()[k][0];
                const double im = out.get()[k][1];
                psd[k] += (re * re + im * im) / (fs * S2);
            }
        }
    } // plan/内存随 RAII 释放

    psd /= static_cast<double>(n_segs);              // 段间平均

    // 单边化：仅对非 DC、非 Nyquist 的频率点乘 2
    for (int k = 1; k < n_freqs - 1; ++k) psd[k] *= 2.0;

    Eigen::VectorXd freqs(n_freqs);
    for (int k = 0; k < n_freqs; ++k) freqs[k] = static_cast<double>(k) * fs / nfft;

    return {freqs, psd};
}

/**
 * @brief 对 PSD 在指定频率区间内进行梯形积分，得到绝对频带功率。
 * @param freqs 输入：单调递增的频率坐标向量，单位 Hz。
 * @param psd 输入：与 freqs 等长的功率谱密度向量。
 * @param f_low 输入：积分频带下界，单位 Hz。
 * @param f_high 输入：积分频带上界，单位 Hz。
 * @return 输出：频带 [f_low,f_high] 内的积分功率；非法或空区间返回 0。
 */
double band_power(const Eigen::VectorXd& freqs, const Eigen::VectorXd& psd,
                  double f_low, double f_high) {
    if (f_high <= f_low) return 0.0;
    const int n = static_cast<int>(freqs.size());
    if (n < 2) return 0.0;

    double power = 0.0;
    for (int k = 0; k + 1 < n; ++k) {
        const double f0 = freqs[k];
        const double f1 = freqs[k + 1];
        // 当前 bin 区间 [f0,f1] 与目标频带 [f_low,f_high] 的交集
        const double a = std::max(f0, f_low);
        const double b = std::min(f1, f_high);
        if (b > a && f1 > f0) {
            const double t_a = (a - f0) / (f1 - f0);
            const double t_b = (b - f0) / (f1 - f0);
            const double p_a = psd[k] + t_a * (psd[k + 1] - psd[k]);
            const double p_b = psd[k] + t_b * (psd[k + 1] - psd[k]);
            power += 0.5 * (p_a + p_b) * (b - a);    // 梯形积分
        }
    }
    return power;
}

/**
 * @brief 通过 FFT 频域掩码对单通道信号执行零相位带通滤波。
 * @param signal 输入：待滤波的单通道时域信号。
 * @param fs 输入：采样率，单位 Hz。
 * @param f_low 输入：通带下界，单位 Hz。
 * @param f_high 输入：通带上界，单位 Hz。
 * @param transition 输入：上下截止频率外侧的余弦过渡带宽度，单位 Hz。
 * @return 输出：与输入等长的带通滤波信号；空信号或非法采样率返回零向量。
 */
Eigen::VectorXd bandpass_filter(const Eigen::VectorXd& signal, double fs,
                                double f_low, double f_high, double transition) {
    const int n = static_cast<int>(signal.size());
    Eigen::VectorXd result = Eigen::VectorXd::Zero(n);
    if (n == 0 || fs <= 0.0) return result;

    const int nfft    = next_power_of_2(n);
    const int n_freqs = nfft / 2 + 1;

    // 正向 r2c FFT（plan 查询/创建在锁内，执行在锁外以支持多线程并行）
    fftw_plan fwd;
    {
        std::lock_guard<std::mutex> lock(g_fftw_mutex);
        fwd = get_plan_locked(PlanKind::R2C, nfft);
    }
    RealPtr r_in(fftw_alloc_real(nfft));
    CpxPtr  c_out(fftw_alloc_complex(n_freqs));
    for (int i = 0; i < n; ++i) r_in.get()[i] = signal[i];
    for (int i = n; i < nfft; ++i) r_in.get()[i] = 0.0;
    fftw_execute_dft_r2c(fwd, r_in.get(), c_out.get());

    // 频域掩码（通带=1，余弦过渡带，其余=0）
    for (int k = 0; k < n_freqs; ++k) {
        const double f = static_cast<double>(k) * fs / nfft;
        double mask = 0.0;
        if (f >= f_low && f <= f_high) {
            mask = 1.0;
        } else if (transition > 0.0 && f >= f_low - transition && f < f_low) {
            mask = 0.5 * (1.0 - std::cos(PI * (f - f_low + transition) / transition));
        } else if (transition > 0.0 && f > f_high && f <= f_high + transition) {
            mask = 0.5 * (1.0 + std::cos(PI * (f - f_high) / transition));
        }
        c_out.get()[k][0] *= mask;
        c_out.get()[k][1] *= mask;
    }

    // 共轭镜像构造全长谱，供 c2c 逆变换
    CpxPtr c_full(fftw_alloc_complex(nfft));
    for (int k = 0; k < n_freqs; ++k) {
        c_full.get()[k][0] = c_out.get()[k][0];
        c_full.get()[k][1] = c_out.get()[k][1];
    }
    for (int k = n_freqs; k < nfft; ++k) {
        c_full.get()[k][0] =  c_out.get()[nfft - k][0];
        c_full.get()[k][1] = -c_out.get()[nfft - k][1];   // 共轭
    }

    fftw_plan inv;
    {
        std::lock_guard<std::mutex> lock(g_fftw_mutex);
        inv = get_plan_locked(PlanKind::C2C_Backward, nfft);
    }
    CpxPtr  c_res(fftw_alloc_complex(nfft));
    fftw_execute_dft(inv, c_full.get(), c_res.get());

    for (int i = 0; i < n; ++i) result[i] = c_res.get()[i][0] / nfft;  // 逆变换取实部并归一
    return result;
}

/**
 * @brief 通过 FFT 构造解析信号并提取瞬时相位与瞬时振幅。
 * @param signal 输入：单通道实数信号，通常应先完成目标频带滤波。
 * @return 输出：二元组 (phase, amplitude)；相位单位为弧度，振幅为解析信号模值。
 */
std::pair<Eigen::VectorXd, Eigen::VectorXd> hilbert_transform(const Eigen::VectorXd& signal) {
    const int n = static_cast<int>(signal.size());
    Eigen::VectorXd phase = Eigen::VectorXd::Zero(n);
    Eigen::VectorXd amp   = Eigen::VectorXd::Zero(n);
    if (n == 0) return {phase, amp};

    const int m = n;                    // 不做零填充，保持输出长度 = n
    const int n_freqs = m / 2 + 1;

    // plan 查询/创建在锁内，执行在锁外以支持多线程并行
    fftw_plan fwd;
    {
        std::lock_guard<std::mutex> lock(g_fftw_mutex);
        fwd = get_plan_locked(PlanKind::R2C, m);
    }
    RealPtr r_in(fftw_alloc_real(m));
    CpxPtr  c_out(fftw_alloc_complex(n_freqs));
    for (int i = 0; i < m; ++i) r_in.get()[i] = signal[i];
    fftw_execute_dft_r2c(fwd, r_in.get(), c_out.get());

    // 构造解析信号频谱：正频率×2，DC/Nyquist 不变，负频率置 0
    CpxPtr c_full(fftw_alloc_complex(m));
    for (int k = 0; k < m; ++k) { c_full.get()[k][0] = 0.0; c_full.get()[k][1] = 0.0; }

    c_full.get()[0][0] = c_out.get()[0][0];                 // DC
    c_full.get()[0][1] = 0.0;
    for (int k = 1; k < n_freqs - 1; ++k) {                 // 正频率 ×2
        c_full.get()[k][0] = 2.0 * c_out.get()[k][0];
        c_full.get()[k][1] = 2.0 * c_out.get()[k][1];
    }
    if (m % 2 == 0) {                                        // 偶数：Nyquist 不变
        c_full.get()[m / 2][0] = c_out.get()[n_freqs - 1][0];
        c_full.get()[m / 2][1] = 0.0;
    } else {                                                 // 奇数：最高正频率 ×2
        c_full.get()[n_freqs - 1][0] = 2.0 * c_out.get()[n_freqs - 1][0];
        c_full.get()[n_freqs - 1][1] = 2.0 * c_out.get()[n_freqs - 1][1];
    }

    fftw_plan inv;
    {
        std::lock_guard<std::mutex> lock(g_fftw_mutex);
        inv = get_plan_locked(PlanKind::C2C_Backward, m);
    }
    CpxPtr  c_res(fftw_alloc_complex(m));
    fftw_execute_dft(inv, c_full.get(), c_res.get());

    for (int i = 0; i < n; ++i) {
        const double re = c_res.get()[i][0] / m;
        const double im = c_res.get()[i][1] / m;
        phase[i] = std::atan2(im, re);              // 瞬时相位
        amp[i]   = std::sqrt(re * re + im * im);    // 瞬时振幅
    }
    return {phase, amp};
}

/**
 * @brief 带通滤波与希尔伯特变换合并：一次正/逆 FFT 直接得到带通信号的解析信号。
 * @param signal 输入：待处理的单通道时域信号。
 * @param fs 输入：采样率，单位 Hz。
 * @param f_low 输入：通带下界，单位 Hz。
 * @param f_high 输入：通带上界，单位 Hz。
 * @param transition 输入：上下截止频率外侧的余弦过渡带宽度，单位 Hz。
 * @return 输出：二元组 (phase, amplitude)，即带通后信号的瞬时相位与瞬时振幅。
 * @details 在单边谱上同时施加带通掩码并构造解析谱（正频率×2、负频率置 0），
 *          再做一次逆变换，等价于 bandpass_filter()+hilbert_transform() 但 FFT 次数减半。
 */
std::pair<Eigen::VectorXd, Eigen::VectorXd> bandpass_analytic(
    const Eigen::VectorXd& signal, double fs,
    double f_low, double f_high, double transition) {
    const int n = static_cast<int>(signal.size());
    Eigen::VectorXd phase = Eigen::VectorXd::Zero(n);
    Eigen::VectorXd amp   = Eigen::VectorXd::Zero(n);
    if (n == 0 || fs <= 0.0) return {phase, amp};

    const int nfft    = next_power_of_2(n);
    const int n_freqs = nfft / 2 + 1;

    // 正向 r2c FFT（零填充到 nfft；plan 查询/创建在锁内，执行在锁外以支持并行）
    fftw_plan fwd;
    {
        std::lock_guard<std::mutex> lock(g_fftw_mutex);
        fwd = get_plan_locked(PlanKind::R2C, nfft);
    }
    RealPtr r_in(fftw_alloc_real(nfft));
    CpxPtr  c_out(fftw_alloc_complex(n_freqs));
    for (int i = 0; i < n; ++i) r_in.get()[i] = signal[i];
    for (int i = n; i < nfft; ++i) r_in.get()[i] = 0.0;
    fftw_execute_dft_r2c(fwd, r_in.get(), c_out.get());

    // 频率点 k 处的带通掩码（通带=1，余弦过渡带，其余=0）
    auto mask_at = [&](int k) -> double {
        const double f = static_cast<double>(k) * fs / nfft;
        if (f >= f_low && f <= f_high) return 1.0;
        if (transition > 0.0 && f >= f_low - transition && f < f_low)
            return 0.5 * (1.0 - std::cos(PI * (f - f_low + transition) / transition));
        if (transition > 0.0 && f > f_high && f <= f_high + transition)
            return 0.5 * (1.0 + std::cos(PI * (f - f_high) / transition));
        return 0.0;
    };

    // 直接在单边谱上构造“带通解析信号”频谱：正频率×2 并施加掩码，
    // DC/Nyquist 不×2，负频率置 0
    CpxPtr c_full(fftw_alloc_complex(nfft));
    for (int k = 0; k < nfft; ++k) { c_full.get()[k][0] = 0.0; c_full.get()[k][1] = 0.0; }

    const double m0 = mask_at(0);                         // DC
    c_full.get()[0][0] = m0 * c_out.get()[0][0];
    c_full.get()[0][1] = m0 * c_out.get()[0][1];
    for (int k = 1; k < n_freqs - 1; ++k) {               // 正频率 ×2
        const double mk = mask_at(k);
        c_full.get()[k][0] = 2.0 * mk * c_out.get()[k][0];
        c_full.get()[k][1] = 2.0 * mk * c_out.get()[k][1];
    }
    if (nfft % 2 == 0) {                                  // Nyquist（nfft 为 2 的幂，恒为偶数）
        const double mn = mask_at(nfft / 2);
        c_full.get()[nfft / 2][0] = mn * c_out.get()[n_freqs - 1][0];
        c_full.get()[nfft / 2][1] = mn * c_out.get()[n_freqs - 1][1];
    }
    // 负频率保持 0

    fftw_plan inv;
    {
        std::lock_guard<std::mutex> lock(g_fftw_mutex);
        inv = get_plan_locked(PlanKind::C2C_Backward, nfft);
    }
    CpxPtr  c_res(fftw_alloc_complex(nfft));
    fftw_execute_dft(inv, c_full.get(), c_res.get());

    for (int i = 0; i < n; ++i) {
        const double re = c_res.get()[i][0] / nfft;
        const double im = c_res.get()[i][1] / nfft;
        phase[i] = std::atan2(im, re);              // 瞬时相位
        amp[i]   = std::sqrt(re * re + im * im);    // 瞬时振幅
    }
    return {phase, amp};
}

/**
 * @brief 用功率谱重心法计算指定 Alpha 区间内的 APF。
 * @param freqs 输入：频率坐标向量，单位 Hz。
 * @param psd 输入：与 freqs 等长的功率谱密度向量。
 * @param cog_low 输入：CoG 计算区间下界，单位 Hz。
 * @param cog_high 输入：CoG 计算区间上界，单位 Hz。
 * @return 输出：APF=Σ(f·PSD)/ΣPSD，区间内总功率近零时返回 NaN。
 */
double compute_apf_cog(const Eigen::VectorXd& freqs, const Eigen::VectorXd& psd,
                       double cog_low, double cog_high) {
    double num = 0.0, den = 0.0;
    const int n = static_cast<int>(freqs.size());
    for (int k = 0; k < n; ++k) {
        const double f = freqs[k];
        if (f >= cog_low && f <= cog_high) {
            num += f * psd[k];
            den += psd[k];
        }
    }
    if (den < 1e-30) return std::nan("");
    return num / den;
}

/**
 * @brief 预创建并缓存指定尺寸的 r2c plan（线程安全），供并行前预热。
 * @param n 输入：FFT 尺寸（实数信号长度）。
 * @return 输出：无；plan 被创建（若不存在）并写入缓存。
 */
void warmup_r2c(int n) {
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    get_plan_locked(PlanKind::R2C, n);
}

/**
 * @brief 预创建并缓存指定尺寸的 c2c 逆向 plan（线程安全），供并行前预热。
 * @param n 输入：FFT 尺寸（复数序列长度）。
 * @return 输出：无；plan 被创建（若不存在）并写入缓存。
 */
void warmup_c2c_backward(int n) {
    std::lock_guard<std::mutex> lock(g_fftw_mutex);
    get_plan_locked(PlanKind::C2C_Backward, n);
}

} // namespace dsp
