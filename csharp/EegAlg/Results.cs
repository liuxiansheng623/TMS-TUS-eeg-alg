namespace EegAlg;

public sealed class WelchPsdResult
{
    public double[] Frequencies { get; }
    public double[] Power { get; }

    internal WelchPsdResult(double[] frequencies, double[] power)
    {
        Frequencies = frequencies;
        Power = power;
    }
}

public abstract class FeatureResult
{
    private readonly double[,] _features;

    public int FeatureCount { get; }
    public int ChannelCount { get; }

    protected FeatureResult(double[,] features)
    {
        _features = features;
        FeatureCount = features.GetLength(0);
        ChannelCount = features.GetLength(1);
    }

    public double this[int feature, int channel] => _features[feature, channel];

    public double[,] AsMatrix() => _features;
}

public sealed class AbsolutePowerResult : FeatureResult
{
    internal AbsolutePowerResult(double[,] features) : base(features) { }

    public double this[AbsolutePowerFeature feature, int channel] => this[(int)feature, channel];

    public double GetApf(int channel) => this[(int)AbsolutePowerFeature.Apf, channel];
}

public sealed class ShamPowerResult : FeatureResult
{
    internal ShamPowerResult(double[,] features) : base(features) { }

    public double this[ShamPowerFeature feature, int channel] => this[(int)feature, channel];

    public double GetRdnFreqPnt(int channel) => this[(int)ShamPowerFeature.RdnFreqPnt, channel];
}

public sealed class PhaseLockingResult : FeatureResult
{
    internal PhaseLockingResult(double[,] features) : base(features) { }

    public double this[PhaseLockingBand band, int channel] => this[(int)band, channel];
}

public sealed class PacResult : FeatureResult
{
    internal PacResult(double[,] features) : base(features) { }

    public double this[PacMode mode, int channel] => this[(int)mode, channel];
}
