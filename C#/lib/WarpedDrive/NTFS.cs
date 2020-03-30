using System;
using System.IO;


namespace WarpedDrive
{
    public class NTFS : Volume
    {
        protected NTFS(Stream stream, byte[] header) : base(stream, header)
        {
            // FIXME: XXX: ...
        }

        public static new bool IsHeaderValid(byte[] header) =>
            (header.Length > 8 && header[3] == 0x4e && header[4] == 0x54 && header[5] == 0x46 && header[6] == 0x53);

        public static new NTFS Parse(Stream stream)
        {
            byte[] header = new byte[512];
            stream.Read(header, 0, header.Length);
            if (!IsHeaderValid(header)) throw new NotSupportedException("not an NTFS volume");
            return new NTFS(stream, header);
        }
    }
}