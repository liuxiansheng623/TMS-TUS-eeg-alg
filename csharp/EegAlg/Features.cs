namespace EegAlg;

public enum AbsolutePowerFeature
{
    Delta = 0,
    Theta = 1,
    LoAlpha = 2,
    HiAlpha = 3,
    LoBeta = 4,
    HiBeta = 5,
    LoGamma = 6,
    HiGamma = 7,
    VhiGamma = 8,
    VvhiFreq = 9,
    Muscle = 10,
    PowerSupply50Hz = 11,
    PowerSupply100Hz = 12,
    Apf = 13,
}

public enum ShamPowerFeature
{
    ShamBand0 = 0,
    ShamBand1 = 1,
    ShamBand2 = 2,
    ShamBand3 = 3,
    ShamBand4 = 4,
    ShamBand5 = 5,
    ShamBand6 = 6,
    ShamBand7 = 7,
    ShamBand8 = 8,
    ShamBand9 = 9,
    ShamBand10 = 10,
    PowerSupply50Hz = 11,
    PowerSupply100Hz = 12,
    RdnFreqPnt = 13,
}

public enum PhaseLockingBand
{
    Delta = 0,
    Theta = 1,
    LoAlpha = 2,
    HiAlpha = 3,
    Alpha = 4,
    LoBeta = 5,
    Beta = 6,
    Gamma = 7,
}

public enum PacMode
{
    DeltaLoGamma = 0,
    DeltaHiGamma = 1,
    ThetaLoGamma = 2,
    ThetaHiGamma = 3,
    AlphaLoGamma = 4,
    AlphaHiGamma = 5,
    BetaLoGamma = 6,
    BetaHiGamma = 7,
}

public static class FeatureCounts
{
    public const int AbsolutePower = 14;
    public const int ShamPower = 14;
    public const int PhaseLocking = 8;
    public const int Pac = 8;
}
