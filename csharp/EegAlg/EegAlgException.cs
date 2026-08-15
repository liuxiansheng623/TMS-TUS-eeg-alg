namespace EegAlg;

public sealed class EegAlgException : Exception
{
    public int ErrorCode { get; }

    public EegAlgException(int errorCode, string message)
        : base(message)
    {
        ErrorCode = errorCode;
    }
}
