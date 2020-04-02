using System.IO;


namespace WarpedDrive
{
    public static class StreamExtension
    {
        public static int ReadExact(this Stream stream, byte[] buffer, int offset, int count)
        {
            int total = 0;
            while (count > 0)
            {
                int read = stream.Read(buffer, offset, count);
                if (read < 1) throw new EndOfStreamException();
                total += read;
                offset += read;
                count -= read;
            }
            return total;
        }
    }
}