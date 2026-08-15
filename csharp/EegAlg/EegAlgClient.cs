using System.Runtime.InteropServices;

namespace EegAlg;

public static class EegAlgClient
{
    public static string Version
    {
        get
        {
            IntPtr ptr = Native.eeg_alg_abi_version();
            return Marshal.PtrToStringUTF8(ptr) ?? string.Empty;
        }
    }

    public static WelchPsdResult WelchPsd(double[] signal, double fs, int nperseg = 256)
    {
        ArgumentNullException.ThrowIfNull(signal);
        if (signal.Length == 0) throw new ArgumentException("signal is empty", nameof(signal));

        var freqs = new double[signal.Length + 1];
        var psd = new double[signal.Length + 1];
        int rc = Native.eeg_alg_welch_psd(signal, signal.Length, fs, nperseg, freqs, psd, out int nOut);
        ThrowIfError(rc);
        Array.Resize(ref freqs, nOut);
        Array.Resize(ref psd, nOut);
        return new WelchPsdResult(freqs, psd);
    }

    public static double BandPower(double[] freqs, double[] psd, double fLow, double fHigh)
    {
        ArgumentNullException.ThrowIfNull(freqs);
        ArgumentNullException.ThrowIfNull(psd);
        if (freqs.Length != psd.Length) throw new ArgumentException("freqs and psd length mismatch");
        return Native.eeg_alg_band_power(freqs, psd, freqs.Length, fLow, fHigh);
    }

    public static AbsolutePowerResult AbsolutePower(double[,] data, double fs, double apf, int nperseg = 256)
    {
        var (flat, samples, channels) = Flatten(data);
        var features = new double[FeatureCounts.AbsolutePower * channels];
        int rc = Native.eeg_alg_absolute_power(flat, samples, channels, fs, apf, nperseg, features);
        ThrowIfError(rc);
        return new AbsolutePowerResult(Reshape(features, FeatureCounts.AbsolutePower, channels));
    }

    public static AbsolutePowerResult AbsolutePowerIndiv(double[,] data, double fs, int nperseg = 256)
    {
        var (flat, samples, channels) = Flatten(data);
        var features = new double[FeatureCounts.AbsolutePower * channels];
        int rc = Native.eeg_alg_absolute_power_indiv(flat, samples, channels, fs, nperseg, features);
        ThrowIfError(rc);
        return new AbsolutePowerResult(Reshape(features, FeatureCounts.AbsolutePower, channels));
    }

    public static ShamPowerResult ShamPower(double[,] data, double fs, double rdnFreqPnt = -1.0, int nperseg = 256)
    {
        var (flat, samples, channels) = Flatten(data);
        var features = new double[FeatureCounts.ShamPower * channels];
        int rc = Native.eeg_alg_sham_power(flat, samples, channels, fs, rdnFreqPnt, nperseg, features);
        ThrowIfError(rc);
        return new ShamPowerResult(Reshape(features, FeatureCounts.ShamPower, channels));
    }

    public static PhaseLockingResult PhaseLocking(double[,,] epochs, double fs, double apf)
    {
        ArgumentNullException.ThrowIfNull(epochs);
        int trials = epochs.GetLength(0);
        int samples = epochs.GetLength(1);
        int channels = epochs.GetLength(2);
        if (trials == 0 || samples == 0 || channels == 0) throw new ArgumentException("epochs has zero dimension", nameof(epochs));

        var flat = new double[trials * samples * channels];
        for (int t = 0; t < trials; t++)
            for (int s = 0; s < samples; s++)
                for (int c = 0; c < channels; c++)
                    flat[(t * samples + s) * channels + c] = epochs[t, s, c];

        var features = new double[FeatureCounts.PhaseLocking * channels];
        int rc = Native.eeg_alg_phase_locking(flat, trials, samples, channels, fs, apf, features);
        ThrowIfError(rc);
        return new PhaseLockingResult(Reshape(features, FeatureCounts.PhaseLocking, channels));
    }

    public static PacResult Pac(double[,] data, double fs, double apf, int numBins = 18)
    {
        var (flat, samples, channels) = Flatten(data);
        var features = new double[FeatureCounts.Pac * channels];
        int rc = Native.eeg_alg_pac(flat, samples, channels, fs, apf, numBins, features);
        ThrowIfError(rc);
        return new PacResult(Reshape(features, FeatureCounts.Pac, channels));
    }

    private static (double[] flat, int samples, int channels) Flatten(double[,] data)
    {
        ArgumentNullException.ThrowIfNull(data);
        int samples = data.GetLength(0);
        int channels = data.GetLength(1);
        if (samples == 0 || channels == 0) throw new ArgumentException("data has zero dimension", nameof(data));

        var flat = new double[samples * channels];
        for (int s = 0; s < samples; s++)
            for (int c = 0; c < channels; c++)
                flat[s * channels + c] = data[s, c];
        return (flat, samples, channels);
    }

    private static double[,] Reshape(double[] flat, int featureCount, int channels)
    {
        var matrix = new double[featureCount, channels];
        for (int f = 0; f < featureCount; f++)
            for (int c = 0; c < channels; c++)
                matrix[f, c] = flat[f * channels + c];
        return matrix;
    }

    private static void ThrowIfError(int rc)
    {
        if (rc == Native.Ok) return;
        throw rc switch
        {
            Native.ErrInvalidSamplingRate => new EegAlgException(rc, "invalid sampling rate"),
            Native.ErrInvalidArg => new EegAlgException(rc, "invalid argument"),
            _ => new EegAlgException(rc, $"native error {rc}"),
        };
    }
}
