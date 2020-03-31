using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace WarpedDrive.NTFS
{
    public enum ATTRIBUTE_TYPE_CODE : uint
    {
        STANDARD_INFORMATION = 0x10,
        ATTRIBUTE_LIST = 0x20,
        FILE_NAME = 0x30,
        OBJECT_ID = 0x40,
        VOLUME_NAME = 0x60,
        VOLUME_INFORMATION = 0x70,
        DATA = 0x80,
        INDEX_ROOT = 0x90,
        INDEX_ALLOCATION = 0xA0,
        BITMAP = 0xB0,
        REPARSE_POINT = 0xC0,
        END = 0xFFFFFFFF,
    }

    public class Attribute
    {
        protected long offset;
        internal uint recordLength;
        internal ATTRIBUTE_TYPE_CODE type;

        protected Attribute(Stream stream)
        {
            offset = stream.Position;

            // ATTRIBUTE_RECORD_HEADER
            type = ATTRIBUTE_TYPE_CODE.END;
            byte formCode = 0;
            byte nameLength = 0;
            ushort nameOffset = 0;
            ushort flags = 0;
            using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
            {
                type = (ATTRIBUTE_TYPE_CODE)reader.ReadUInt32();
                recordLength = reader.ReadUInt32();

                Console.WriteLine($"Attribute Type: {type}");
                Console.WriteLine($"Attribute Length: {recordLength}");
                if (type == ATTRIBUTE_TYPE_CODE.END) return;

                formCode = reader.ReadByte();
                nameLength = reader.ReadByte();
                nameOffset = reader.ReadUInt16();
                flags = reader.ReadUInt16();

                Console.WriteLine($"Attribute Form: {formCode}");
                Console.WriteLine($"Attribute Flags: {flags}");
            }

            if (nameLength > 0)
            {
                stream.Seek(offset + nameOffset, SeekOrigin.Begin);
                using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
                {
                    string name = new string(reader.ReadChars(nameLength));
                    Console.WriteLine($"Attribute Name: {name}");
                }
            }
        }

        public static Attribute Parse(Stream stream) => new Attribute(stream);
    }

    public class File
    {
        protected long offset;
        protected List<Attribute> attributes;

        protected File(Stream stream)
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

            // parse file attributes
            attributes = new List<Attribute>();
            long attributeOffset = offset + firstAttributeOffset;
            while (attributeOffset < offset + realSize)
            {
                stream.Seek(attributeOffset, SeekOrigin.Begin);
                Attribute attr = Attribute.Parse(stream);
                if (attr.type == ATTRIBUTE_TYPE_CODE.END) break;
                attributeOffset += attr.recordLength;
                attributes.Add(attr);
            }
        }

        public static bool IsHeaderValid(byte[] header) =>
            (header.Length > 3 && header[0] == 0x46 && header[1] == 0x49 && header[2] == 0x4C && header[3] == 0x45);

        public static File Parse(Stream stream) => new File(stream);
    }

    public class NTFS : Volume
    {
        protected File mft;

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
                mft = File.Parse(inner);
            }
            catch (NotSupportedException)
            {
                Console.Error.WriteLine("WARNING: Primary MFT is bad. Parsing backup...");
                inner.Seek(offset + (long)backupOffset, SeekOrigin.Begin);
                mft = File.Parse(inner);
            }
        }

        public static new bool IsHeaderValid(byte[] header) =>
            (header.Length > 7 && header[3] == 0x4e && header[4] == 0x54 && header[5] == 0x46 && header[6] == 0x53);

        public static new NTFS Parse(Stream stream) => new NTFS(stream);
    }
}