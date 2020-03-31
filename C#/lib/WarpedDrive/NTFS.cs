using System;
using System.IO;
using System.Text;

namespace WarpedDrive
{
    public class NTFSFile
    {
        protected long offset;

        protected NTFSFile(Stream stream)
        {
            offset = stream.Position;

            // MULTI_SECTOR_HEADER
            byte[] signature = new byte[4];
            stream.Read(signature, 0, 4);
            if (!IsHeaderValid(signature)) throw new NotSupportedException("not an NTFS file");
            // ushort UpdateSequenceArrayOffset, ushort UpdateSequenceArraySize
            stream.Seek(2 + 2, SeekOrigin.Current);

            // FILE_RECORD_SEGMENT_HEADER
            // ulong LogSequenceNumber, ushort SequenceNumber, ushort ReferenceCount
            stream.Seek(8 + 2 + 2, SeekOrigin.Current);
            ushort firstAttributeOffset = 0;
            ushort flags = 0;
            uint realSize = 0;
            using(BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
            {
                firstAttributeOffset = reader.ReadUInt16();
                flags = reader.ReadUInt16();
                realSize = reader.ReadUInt32();
            }

            Console.WriteLine($"FirstAttributeOffset: {firstAttributeOffset}");
            Console.WriteLine($"Flags: {flags}");
            Console.WriteLine($"RealSize: {realSize}");
        }

        public static bool IsHeaderValid(byte[] header) =>
            (header.Length > 3 && header[0] == 0x46 && header[1] == 0x49 && header[2] == 0x4C && header[3] == 0x45);

        public static NTFSFile Parse(Stream stream) => new NTFSFile(stream);
    }

    public class NTFS : Volume
    {
        protected NTFSFile mft;

        protected NTFS(Stream stream) : base(stream)
        {
            // byte[3] Jump, byte[8] OemId ('NTFS    ')
            byte[] header = new byte[11];
            stream.Read(header, 0, header.Length);
            if (!IsHeaderValid(header)) throw new NotSupportedException("not an NTFS volume");

            // BIOS_PARAMETER_BLOCK
            ushort bytesPerSector = 0;
            byte sectorsPerCluster = 0;
            using(BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
            {
                bytesPerSector = reader.ReadUInt16();
                sectorsPerCluster = reader.ReadByte();
            }
            // ushort ReservedSectors, byte NumberOfFATs (0), ushort RootDirectories (0), ushort TotalSectors (0),
            // byte MediaDescriptor, ushort SectorsPerFAT (0), ushort SectorsPerTrack, ushort NumberOfHeads,
            // uint HiddenSectors32, uint TotalSectors32
            stream.Seek(2 + 1 + 2 + 2 + 1 + 2 + 2 + 2 + 4 + 4, SeekOrigin.Current);

            // NTFS_EXTENDED_BIOS_PARAMETER_BLOCK
            // uint SectorsPerFAT32, ulong TotalSectors64
            stream.Seek(4 + 8, SeekOrigin.Current);
            ulong mftLocation = 0;
            ulong backupMftLocation = 0;
            sbyte clustersPerMftRecord = 0;
            using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
            {
                mftLocation = reader.ReadUInt64();
                backupMftLocation = reader.ReadUInt64();
                clustersPerMftRecord = reader.ReadSByte();
            }

            // calculate actual offsets and size
            uint clusterSize = (uint)bytesPerSector * (uint)sectorsPerCluster;
            ulong mftOffset = mftLocation * (ulong)clusterSize;
            ulong backupOffset = backupMftLocation * (ulong)clusterSize;
            uint recordSize = 0;
            if (clustersPerMftRecord > 0)
                recordSize = (uint)clustersPerMftRecord * clusterSize;
            else
                recordSize = (uint)Math.Pow(2, -clustersPerMftRecord);

            // parse the MFT file
            inner = Device.Wrap(stream, clusterSize);
            try
            {
                inner.Seek(offset + (long)mftOffset, SeekOrigin.Begin);
                mft = NTFSFile.Parse(inner);
            }
            catch (NotSupportedException)
            {
                Console.Error.WriteLine("WARNING: Primary MFT is bad. Parsing backup...");
                inner.Seek(offset + (long)backupOffset, SeekOrigin.Begin);
                mft = NTFSFile.Parse(inner);
            }
        }

        public static new bool IsHeaderValid(byte[] header) =>
            (header.Length > 7 && header[3] == 0x4e && header[4] == 0x54 && header[5] == 0x46 && header[6] == 0x53);

        public static new NTFS Parse(Stream stream) => new NTFS(stream);
    }
}