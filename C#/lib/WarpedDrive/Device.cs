using System;
using System.IO;

namespace WarpedDrive
{
    public class Device : Stream
    {
        protected Stream inner;
        protected byte[] buffer;
        protected int position;
        protected int capacity;

        public byte[] Buffer => buffer[position..capacity];

        public override long Position { get => Seek(0, SeekOrigin.Current); set => Seek(value, SeekOrigin.Begin); }
        public override long Length => inner.Length;
        public override bool CanWrite => false;
        public override bool CanRead => inner.CanRead;
        public override bool CanSeek => inner.CanSeek;
        public override bool CanTimeout => inner.CanTimeout;
        public override int ReadTimeout { get => inner.ReadTimeout; set => inner.ReadTimeout = value; }

        protected Device(Stream inner, uint blockSize) : base()
        {
            if (!inner.CanRead) throw new ArgumentException($"{nameof(inner)} must be readable");
            if (blockSize < 1) throw new ArgumentOutOfRangeException($"{nameof(blockSize)} must be greater than 0");

            this.inner = inner;
            this.buffer = new byte[blockSize];
            this.position = 0;
            this.capacity = 0;
        }

        public static Device Wrap(Stream inner, uint blockSize) => new Device(inner, blockSize);
        public static Device Open(string path, uint blockSize) =>
            Wrap(File.Open(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite), blockSize);

        public uint GetBlockSize() => (uint)buffer.Length;

        protected void fillBuffer()
        {
            if (position >= capacity)
            {
                capacity = inner.Read(buffer, 0, buffer.Length);
                position = 0;
            }
        }

        protected void consume(int amount)
        {
            position = Math.Min(position + amount, capacity);
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            if (position == capacity && count > this.buffer.Length)
            {
                int aligned = count - (count % this.buffer.Length);
                Flush();
                return inner.Read(buffer, offset, aligned);
            }

            fillBuffer();
            int read = Math.Min(capacity - position, count);
            int end = position + read;
            this.buffer[position..end].CopyTo(buffer, offset);
            consume(read);
            return read;
        }

        public override long Seek(long offset, SeekOrigin origin)
        {
            if (!inner.CanSeek) throw new NotSupportedException("seeking is not supported");

            long maximum = inner.Position;
            long minimum = maximum - capacity;
            long current = minimum + position;

            long target = -1;
            switch (origin)
            {
                case SeekOrigin.Current:
                    target = current + offset;
                    break;
                case SeekOrigin.Begin:
                    target = offset;
                    break;
                case SeekOrigin.End:
                    long length = inner.Seek(0, SeekOrigin.End);
                    inner.Seek(maximum, SeekOrigin.Begin);
                    target = length + offset;
                    break;
            }
            if (target < 0)
                throw new ArgumentOutOfRangeException("cannot seek beyond the bounds of the stream");

            if (target == current) return target;

            if (target < minimum || target > maximum)
            {
                long alignment = target % buffer.Length;
                minimum = target - alignment;

                Flush();

                if (minimum != maximum)
                {
                    inner.Seek(minimum, SeekOrigin.Begin);
                }
                if (target > minimum)
                {
                    fillBuffer();
                }
            }

            position = (int)(target - minimum);
            return target;
        }

        public override void SetLength(long value) =>
            throw new NotSupportedException("truncation is not supported");
        public override void Write(byte[] buffer, int offset, int count) =>
            throw new NotSupportedException("writing is not supported");

        public override void Flush()
        {
            position = 0;
            capacity = 0;
        }

        public override void Close() => inner.Close();

        protected override void Dispose(bool disposing)
        {
            if (disposing) inner.Dispose();
            else inner.Close();
        }
    }
}
