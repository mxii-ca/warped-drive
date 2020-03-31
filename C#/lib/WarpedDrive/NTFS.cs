using System;
using System.IO;
using System.Text;

namespace WarpedDrive
{
    public class NTFS : Volume
    {
        protected NTFS(Stream stream, byte[] header) : base(stream, header)
        {
            // byte[3] Jump, byte[8] OemId ('NTFS    ')
            if (!IsHeaderValid(header)) throw new NotSupportedException("not an NTFS volume");
            stream.Seek(11, SeekOrigin.Begin);

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
            ulong mftLocation = 0;
            ulong backupMftLocation = 0;
            sbyte clustersPerMftRecord = 0;
            sbyte clustersPerIndexBuffer = 0;
            // uint SectorsPerFAT32, ulong TotalSectors64
            stream.Seek(4 + 8, SeekOrigin.Current);
            using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
            {
                mftLocation = reader.ReadUInt64();
                backupMftLocation = reader.ReadUInt64();
                clustersPerMftRecord = reader.ReadSByte();
                reader.ReadBytes(3); // unused
                clustersPerIndexBuffer = reader.ReadSByte();
            }

            int clusterSize = bytesPerSector * sectorsPerCluster;
            ulong mftOffset = mftLocation * (ulong)clusterSize;
            ulong backupOffset = backupMftLocation * (ulong)clusterSize;

            int recordSize = 0;
            if (clustersPerMftRecord > 0)
                recordSize = clustersPerMftRecord * clusterSize;
            else
                recordSize = (int)Math.Pow(2, -clustersPerMftRecord);

            int indexSize = 0;
            if (clustersPerIndexBuffer > 0)
                indexSize = clustersPerIndexBuffer * clusterSize;
            else
                indexSize = (int)Math.Pow(2, -clustersPerIndexBuffer);

            System.Console.WriteLine($"Cluster Size: {clusterSize}");
            System.Console.WriteLine($"Primary MFT - Offset: {mftOffset}");
            System.Console.WriteLine($"Backup  MFT - Offset: {backupOffset}");
            System.Console.WriteLine($"MFT Record Size: {recordSize}");
            System.Console.WriteLine($"Index Buffer Size: {indexSize}");


            // FIXME: XXX: ...
        }

        public static new bool IsHeaderValid(byte[] header) =>
            (header.Length > 8 && header[3] == 0x4e && header[4] == 0x54 && header[5] == 0x46 && header[6] == 0x53);

        public static new NTFS Parse(Stream stream)
        {
            byte[] header = new byte[512];
            stream.Read(header, 0, header.Length);
            return new NTFS(stream, header);
        }
    }
}