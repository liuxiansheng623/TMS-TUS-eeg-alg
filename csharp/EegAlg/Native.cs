using System.Runtime.InteropServices;

namespace EegAlg;

internal static class Native
{
    private const string Dll = "eeg_alg";

    internal const int Ok = 0;
    internal const int ErrInvalidArg = -1;
    internal const int ErrInvalidSamplingRate = -2;

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr eeg_alg_abi_version();

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int eeg_alg_welch_psd(
        double[] signal, int n, double fs, int nperseg,
        [Out] double[] freqs_out, [Out] double[] psd_out, out int n_out);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    internal static extern double eeg_alg_band_power(
        double[] freqs, double[] psd, int n, double f_low, double f_high);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int eeg_alg_absolute_power(
        double[] data, int num_samples, int num_channels,
        double fs, double apf, int nperseg, [Out] double[] features_out);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int eeg_alg_absolute_power_indiv(
        double[] data, int num_samples, int num_channels,
        double fs, int nperseg, [Out] double[] features_out);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int eeg_alg_sham_power(
        double[] data, int num_samples, int num_channels,
        double fs, double rdn_freq_pnt, int nperseg, [Out] double[] features_out);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int eeg_alg_phase_locking(
        double[] epochs, int num_trials, int num_samples,
        int num_channels, double fs, double apf, [Out] double[] features_out);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int eeg_alg_pac(
        double[] data, int num_samples, int num_channels,
        double fs, double apf, int num_bins, [Out] double[] features_out);
}
