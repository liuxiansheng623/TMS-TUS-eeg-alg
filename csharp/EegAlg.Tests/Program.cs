using EegAlg;

const double Pi = Math.PI;
int failures = 0;

void Check(bool cond, string name)
{
    Console.WriteLine($"  [{(cond ? "PASS" : "FAIL")}] {name}");
    if (!cond) failures++;
}

double[] MakeSine(double freq, double amp, double fs, int n, double phase0 = 0.0)
{
    var x = new double[n];
    for (int i = 0; i < n; i++) x[i] = amp * Math.Sin(2.0 * Pi * freq * (i / fs) + phase0);
    return x;
}

Console.WriteLine("== C# API wrapper test ==");

// 1) version
string version = EegAlgClient.Version;
Console.WriteLine($"  version = {version}");
Check(!string.IsNullOrEmpty(version), "Version returns non-empty");

// 2) WelchPsd + BandPower
{
    const double fs = 256.0, A = 3.0, f0 = 2.0;
    const int n = 2560;
    var sig = MakeSine(f0, A, fs, n);
    var spec = EegAlgClient.WelchPsd(sig, fs, 256);
    Check(spec.Frequencies.Length == spec.Power.Length && spec.Frequencies.Length > 0, "WelchPsd output dims");
    double power = EegAlgClient.BandPower(spec.Frequencies, spec.Power, 1.0, 4.0);
    double theory = A * A / 2.0;
    Check(Math.Abs(power - theory) <= 0.25 * theory, $"BandPower ≈ A²/2 (got {power:F4}, theory {theory:F4})");
}

// 3) AbsolutePower
{
    const double fs = 1024.0, apf = 10.0;
    const int n = 10240, ch = 4;
    var data = new double[n, ch];
    var sig = MakeSine(9.0, 2.0, fs, n);
    for (int s = 0; s < n; s++) for (int c = 0; c < ch; c++) data[s, c] = sig[s];

    var res = EegAlgClient.AbsolutePower(data, fs, apf, 1024);
    Check(res.ChannelCount == ch && res.FeatureCount == FeatureCounts.AbsolutePower, "AbsolutePower dims");
    Check(res[AbsolutePowerFeature.LoAlpha, 0] > res[AbsolutePowerFeature.Delta, 0], "LO_ALPHA > DELTA");
    Check(Math.Abs(res.GetApf(0) - apf) < 1e-9, "APF backfilled");
}

// 4) AbsolutePowerIndiv
{
    const double fs = 1024.0;
    const int n = 10240;
    var data = new double[n, 2];
    var a = MakeSine(9.0, 2.0, fs, n);
    var b = MakeSine(11.0, 2.0, fs, n);
    for (int s = 0; s < n; s++) { data[s, 0] = a[s]; data[s, 1] = b[s]; }

    var res = EegAlgClient.AbsolutePowerIndiv(data, fs, 1024);
    Check(Math.Abs(res.GetApf(0) - 9.0) < 0.7, $"indiv APF ch0 ≈ 9 (got {res.GetApf(0):F2})");
    Check(Math.Abs(res.GetApf(1) - 11.0) < 0.7, $"indiv APF ch1 ≈ 11 (got {res.GetApf(1):F2})");
}

// 5) ShamPower
{
    const double fs = 256.0, rdn = 20.0;
    const int n = 2560, ch = 2;
    var data = new double[n, ch];
    var sig = MakeSine(22.0, 2.0, fs, n); // BAND_1 = 21..25
    for (int s = 0; s < n; s++) for (int c = 0; c < ch; c++) data[s, c] = sig[s];

    var res = EegAlgClient.ShamPower(data, fs, rdn);
    Check(Math.Abs(res.GetRdnFreqPnt(0) - rdn) < 1e-12, "Sham rdn backfilled");
    Check(res[ShamPowerFeature.ShamBand1, 0] > res[ShamPowerFeature.ShamBand0, 0], "Sham BAND_1 > BAND_0");
}

// 6) PhaseLocking
{
    const double fs = 256.0, apf = 10.0;
    const int n = 512, ch = 2, T = 20;
    var epochs = new double[T, n, ch];
    for (int t = 0; t < T; t++)
    {
        var sig = MakeSine(10.0, 1.0, fs, n, 0.0);
        for (int s = 0; s < n; s++) for (int c = 0; c < ch; c++) epochs[t, s, c] = sig[s];
    }

    var res = EegAlgClient.PhaseLocking(epochs, fs, apf);
    Check(res.ChannelCount == ch && res.FeatureCount == FeatureCounts.PhaseLocking, "PhaseLocking dims");
    Check(res[PhaseLockingBand.Alpha, 0] > 0.9, $"Alpha ITPC high (got {res[PhaseLockingBand.Alpha, 0]:F3})");
}

// 7) Pac
{
    const double fs = 256.0, apf = 10.0;
    const int n = 2560, ch = 1;
    var data = new double[n, ch];
    var rng = new Random(7);
    for (int s = 0; s < n; s++) data[s, 0] = (rng.NextDouble() * 2.0 - 1.0);

    var res = EegAlgClient.Pac(data, fs, apf);
    bool allFinite = true;
    for (int f = 0; f < FeatureCounts.Pac; f++)
        if (!double.IsFinite(res[f, 0]) || res[f, 0] < 0.0 || res[f, 0] > 1.0) allFinite = false;
    Check(allFinite, "Pac MI finite and in [0,1]");
}

// 8) exception mapping
{
    bool threw = false;
    try { EegAlgClient.AbsolutePower(new double[512, 1], 256.0, 10.0); }
    catch (EegAlgException ex) { threw = ex.ErrorCode == -2; }
    Check(threw, "fs<600 throws EegAlgException with code -2");
}

Console.WriteLine(failures == 0 ? "ALL PASSED" : $"{failures} FAILED");
return failures == 0 ? 0 : 1;
