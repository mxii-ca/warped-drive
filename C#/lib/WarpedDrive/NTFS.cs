using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
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

    public class Attribute : Stream, IBlock
    {
        protected Stream inner;
        protected long offset;
        protected uint clusterSize;

        public ATTRIBUTE_TYPE_CODE Type { get; protected set; }
        public string Name { get; protected set; }
        internal uint recordLength;
        protected ushort flags;
        protected long dataSize;
        protected (long?, long)[] dataRuns;
        protected long currentPosition;
        protected int currentRun;
        protected int currentOffset;

        public override long Position { get => Seek(0, SeekOrigin.Current); set => Seek(value, SeekOrigin.Begin); }
        public override long Length => dataSize;
        public override bool CanWrite => false;
        public override bool CanRead => inner.CanRead;
        public override bool CanSeek => inner.CanSeek;
        public override bool CanTimeout => inner.CanTimeout;
        public override int ReadTimeout { get => inner.ReadTimeout; set => inner.ReadTimeout = value; }

        protected Attribute(Stream stream, uint clusterSize)
        {
            dataRuns = new (long?, long)[0];
            Name = "";

            inner = stream;
            offset = stream.Position;
            this.clusterSize = clusterSize;

            // ATTRIBUTE_RECORD_HEADER
            byte formCode = 0;
            byte nameLength = 0;
            ushort nameOffset = 0;
            using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
            {
                Type = (ATTRIBUTE_TYPE_CODE)reader.ReadUInt32();
                if (Type == ATTRIBUTE_TYPE_CODE.END) return;
                recordLength = reader.ReadUInt32();
                formCode = reader.ReadByte();
                nameLength = reader.ReadByte();
                nameOffset = reader.ReadUInt16();
                flags = reader.ReadUInt16();
            }
            stream.Seek(2, SeekOrigin.Current); // ushort Instance

            // resident header
            if (formCode == 0)
            {
                using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
                {
                    dataSize = (long)reader.ReadUInt32();           // ValueLength
                    long dataOffset = offset + reader.ReadUInt16(); // ValueOffset
                    dataRuns = new (long?, long)[1] { (dataOffset, (long)dataSize) };
                }
            }

            // non-resident header
            else
            {
                // ulong LowestVcn, ulong HighestVcn
                stream.Seek(8 + 8, SeekOrigin.Current);
                ushort mappingPairOffset = 0;
                using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
                    mappingPairOffset = reader.ReadUInt16();
                // ushort CompressionUnitSize, uint Padding, ulong AllocatedSize
                stream.Seek(2 + 4 + 8, SeekOrigin.Current);
                using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
                    dataSize = (long)reader.ReadUInt64();

                // parse the data runs
                // data runs are sized by cluster, not bytes
                List<(long?, long)> runs = new List<(long?, long)>();
                stream.Seek(offset + mappingPairOffset, SeekOrigin.Begin);
                long previousOffset = 0;
                while (stream.Position < offset + recordLength)
                {
                    int header = stream.ReadByte();
                    if (header == 0) break;

                    byte[] buffer = new byte[8];
                    int sizeLen = header & 0x0F;
                    stream.Read(buffer, 0, sizeLen);
                    long size = BitConverter.ToInt64(buffer, 0) * clusterSize;

                    // offset length of 0 for a sparse block
                    // offset is the offset from the previous non-sparse run offset
                    int offsetLen = header >> 4;
                    long? offset = null;
                    if (offsetLen > 0)
                    {
                        buffer.Initialize();
                        stream.Read(buffer, 0, offsetLen);
                        // extend the highest bit to support negative offsets
                        if ((buffer[offsetLen - 1] & 0x80) != 0)
                            foreach (int i in Enumerable.Range(offsetLen, 8 - offsetLen))
                                buffer[i] = 0xFF;
                        offset = BitConverter.ToInt64(buffer, 0) * clusterSize + previousOffset;
                        previousOffset = offset.Value;
                    }

                    runs.Add((offset, size));
                }
                dataRuns = runs.ToArray();
            }

            if (nameLength > 0)
            {
                stream.Seek(offset + nameOffset, SeekOrigin.Begin);
                using (BinaryReader reader = new BinaryReader(stream, Encoding.Unicode, true))
                {
                    Name = new string(reader.ReadChars(nameLength));
                }
            }
        }

        public static Attribute Parse(Stream stream, uint clusterSize) => new Attribute(stream, clusterSize);

        public static Attribute Parse<T>(T stream) where T : Stream, IBlock => Parse(stream, stream.GetBlockSize());

        public override int Read(byte[] buffer, int offset, int count)
        {
            if (count <= 0) return 0;

            (long? runOffset, long runSize) = dataRuns[currentRun];
            int read = Math.Min(count, (int)(runSize - currentOffset));
            if (read == 0) throw new EndOfStreamException();

            // sparse file
            if (runOffset == null)
            {
                Array.Clear(buffer, offset, read);
            }

            // regular data
            // TODO: support compression / encryption
            else
            {
                inner.Seek(runOffset.Value + currentOffset, SeekOrigin.Begin);
                read = inner.Read(buffer, offset, read);
            }

            // update position trackers
            currentOffset += read;
            currentPosition += read;
            if (currentOffset >= runSize && currentRun < dataRuns.Length - 1)
            {
                currentOffset = 0;
                currentRun += 1;
            }
            return read;
        }

        public override long Seek(long offset, SeekOrigin origin)
        {
            if (!inner.CanSeek) throw new NotSupportedException("seeking is not supported");

            long target = -1;
            switch (origin)
            {
                case SeekOrigin.Current:
                    target = currentPosition + offset;
                    break;
                case SeekOrigin.Begin:
                    target = offset;
                    break;
                case SeekOrigin.End:
                    target = dataSize + offset;
                    break;
            }
            if (target < 0 || target > dataSize)
                throw new ArgumentOutOfRangeException("cannot seek beyond the bounds of the stream");

            while (target != currentPosition)
            {
                (long? runOffset, long runSize) = dataRuns[currentRun];
                long minimum = currentPosition - currentOffset;
                long maximum = minimum + runSize;

                if (target >= maximum && currentRun < dataRuns.Length - 1)
                {
                    currentRun += 1;
                    currentOffset = 0;
                    currentPosition = maximum;
                }
                else if (target < minimum)
                {
                    currentRun -= 1;
                    currentOffset = 0;
                    currentPosition = minimum;
                }
                else if (target <= maximum)
                {
                    currentOffset = (int)(Math.Min(target, maximum) - minimum);
                    currentPosition = minimum + currentOffset;
                }
                else
                {
                    throw new EndOfStreamException("unexpected end of the stream");
                }
            }
            return target;
        }

        public override void Flush() => inner.Flush();

        public override void SetLength(long value) =>
            throw new NotSupportedException("truncation is not supported");
        public override void Write(byte[] buffer, int offset, int count) =>
            throw new NotSupportedException("writing is not supported");

        public uint GetBlockSize() => clusterSize;
    }

    public class File
    {
        protected long offset;
        protected List<Attribute> attributes;

        protected File(Stream stream, uint clusterSize)
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
                Attribute attr = Attribute.Parse(stream, clusterSize);
                if (attr.Type == ATTRIBUTE_TYPE_CODE.END) break;
                attributeOffset += attr.recordLength;
                attributes.Add(attr);
            }
        }

        public static bool IsHeaderValid(byte[] header) =>
            (header.Length > 3 && header[0] == 0x46 && header[1] == 0x49 && header[2] == 0x4C && header[3] == 0x45);

        public static File Parse(Stream stream, uint clusterSize) => new File(stream, clusterSize);
        public static File Parse<T>(T stream) where T : Stream, IBlock => Parse(stream, stream.GetBlockSize());
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
                mft = File.Parse(inner, clusterSize);
            }
            catch (NotSupportedException)
            {
                Console.Error.WriteLine("WARNING: Primary MFT is bad. Parsing backup...");
                inner.Seek(offset + (long)backupOffset, SeekOrigin.Begin);
                mft = File.Parse(inner, clusterSize);
            }
        }

        public static new bool IsHeaderValid(byte[] header) =>
            (header.Length > 7 && header[3] == 0x4e && header[4] == 0x54 && header[5] == 0x46 && header[6] == 0x53);

        public static new NTFS Parse(Stream stream) => new NTFS(stream);
    }
}