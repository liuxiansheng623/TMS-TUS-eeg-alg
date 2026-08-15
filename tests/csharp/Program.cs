using System.Runtime.InteropServices;

// C ABI 声明（与 include/eeg_alg_capi.h 保持一致）。
static class EegAlg
{
    private const string Dll = "eeg_alg";

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr eeg_alg_abi_version();

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int eeg_alg_welch_psd(
        double[] signal, int n, double fs, int nperseg,
        [Out] double[] freqs_out, [Out] double[] psd_out, out int n_out);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern double eeg_alg_band_power(
        double[] freqs, double[] psd, int n, double f_low, double f_high);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    public static extern int eeg_alg_absolute_power(
        double[] data, int num_samples, int num_channels,
        double fs, double apf, int nperseg, [Out] double[] features_out);
}

static class Program
{
    private const double Pi = Math.PI;
    private static int _failures = 0;

    private static void Check(bool cond, string name)
    {
        Console.WriteLine($"  [{(cond ? "PASS" : "FAIL")}] {name}");
        if (!cond) _failures++;
    }

    private static double[] MakeSine(double freq, double amp, double fs, int n, double phase0 = 0.0)
    {
        var x = new double[n];
        for (int i = 0; i < n; i++)
            x[i] = amp * Math.Sin(2.0 * Pi * freq * (i / fs) + phase0);
        return x;
    }

    private static int Main()
    {
        Console.WriteLine("== C# P/Invoke interop test ==");

        // 1) 版本符号
        var verPtr = EegAlg.eeg_alg_abi_version();
        var ver = Marshal.PtrToStringUTF8(verPtr);
        Console.WriteLine($"  version = {ver}");
        Check(!string.IsNullOrEmpty(ver), "eeg_alg_abi_version returns non-empty");

        // 2) welch_psd + band_power：2 Hz 正弦，频带功率应接近 A²/2
        const double fs = 256.0, A = 3.0, f0 = 2.0;
        const int n = 2560;
        var sig = MakeSine(f0, A, fs, n);
        var freqs = new double[n + 1];
        var psd = new double[n + 1];
        int rc = EegAlg.eeg_alg_welch_psd(sig, n, fs, 256, freqs, psd, out int nOut);
        Check(rc == 0, $"welch_psd rc=0 (got {rc})");
        Check(nOut > 0, $"welch_psd n_out>0 (got {nOut})");

        var f = new double[nOut];
        var p = new double[nOut];
        Array.Copy(freqs, f, nOut);
        Array.Copy(psd, p, nOut);
        double deltaPow = EegAlg.eeg_alg_band_power(f, p, nOut, 1.0, 4.0);
        double theory = A * A / 2.0;
        Check(Math.Abs(deltaPow - theory) <= 0.25 * theory,
            $"band_power ≈ A²/2 (got {deltaPow:F4}, theory {theory:F4})");

        // 3) absolute_power：9 Hz 正弦 → LO_ALPHA 占优，APF 回填正确
        const double fs2 = 1024.0, apf = 10.0;
        const int n2 = 10240, ch = 4;
        var chSig = MakeSine(9.0, 2.0, fs2, n2);
        var data = new double[n2 * ch]; // sample-major
        for (int s = 0; s < n2; s++)
            for (int c = 0; c < ch; c++)
                data[s * ch + c] = chSig[s];

        var feats = new double[14 * ch];
        int rc2 = EegAlg.eeg_alg_absolute_power(data, n2, ch, fs2, apf, 1024, feats);
        Check(rc2 == 0, $"absolute_power rc=0 (got {rc2})");
        // LO_ALPHA=2 索引；feature-major: feats[2*ch + c]
        double loAlpha = feats[2 * ch + 0];
        double delta = feats[0 * ch + 0];
        Check(loAlpha > delta, "LO_ALPHA > DELTA for 9 Hz tone");
        Check(Math.Abs(feats[13 * ch + 0] - apf) < 1e-9, "APF feature == input apf");

        Console.WriteLine(_failures == 0 ? "ALL PASSED" : $"{_failures} FAILED");
        return _failures == 0 ? 0 : 1;
    }
}
