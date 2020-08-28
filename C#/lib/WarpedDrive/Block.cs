using System;
using System.IO;
using System.Runtime.InteropServices;

namespace WarpedDrive
{
    public interface IBlock
    {
        uint GetBlockSize();
    }

    public static class BlockExtension
    {

        // --- windows-specific code ---

        // #define FILE_ACCESS_ANY              0
        // #define METHOD_BUFFERED              0
        //
        // #define CTL_CODE(DeviceType, Function, Method, Access)   \
        //      ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method)
        //
        // #define FILE_DEVICE_MASS_STORAGE     0x0000002d
        // #define IOCTL_STORAGE_BASE           FILE_DEVICE_MASS_STORAGE
        // #define IOCTL_STORAGE_QUERY_PROPERTY CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)
        //                                      \-> CTL_CODE(0x2d, 0x0500, 0, 0)
        //                                      \-> (0x2d << 16) | (0 << 14) | (0x0500 << 2) | 0
        internal const uint IOCTL_STORAGE_QUERY_PROPERTY = 0x002d1400;
        internal const uint StorageAccessAlignmentProperty = 6;
        internal const uint PropertyStandardQuery = 0;

        [StructLayout(LayoutKind.Sequential)]
        internal struct STORAGE_PROPERTY_QUERY
        {
            public uint PropertyId;
            public uint QueryType;
            public byte AdditionalParameters;
        }

        [StructLayout(LayoutKind.Sequential)]
        internal struct STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR
        {
            public uint Version;
            public uint Size;
            public uint BytesPerCacheLine;
            public uint BytesOffsetForCacheAlignment;
            public uint BytesPerLogicalSector;
            public uint BytesPerPhysicalSector;
            public uint BytesOffsetForSectorAlignment;
        }

        [DllImport("kernel32", EntryPoint = "DeviceIoControl", SetLastError = true)]
        internal static extern bool DeviceIoControl(
            SafeHandle handle,
            uint code,
            ref STORAGE_PROPERTY_QUERY inbuffer, int insize,
            out STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR outbuffer, int outsize, out int retsize,
            IntPtr overlapped
        );

        // #define FILE_DEVICE_DISK                 0x00000007
        // #define IOCTL_DISK_BASE                  FILE_DEVICE_DISK
        // #define IOCTL_DISK_GET_DRIVE_GEOMETRY    CTL_CODE(IOCTL_DISK_BASE, 0x0000, METHOD_BUFFERED, FILE_ANY_ACCESS)
        //                                          \-> CTL_CODE(0x07, 0x0000, 0, 0)
        //                                          \-> (0x07 << 16) | (0 << 14) | (0 << 2) | 0
        internal const uint IOCTL_DISK_GET_DRIVE_GEOMETRY = 0x00070000;

        [StructLayout(LayoutKind.Sequential)]
        internal struct DISK_GEOMETRY
        {
            public long Cylinders;
            public uint MediaType;
            public uint TracksPerCylinder;
            public uint SectorsPerTrack;
            public uint BytesPerSector;
        }

        [DllImport("kernel32", EntryPoint = "DeviceIoControl", SetLastError = true)]
        internal static extern bool DeviceIoControl(
            SafeHandle handle,
            uint code,
            IntPtr inbuffer, int insize,
            out DISK_GEOMETRY outbuffer, int outsize, out int retsize,
            IntPtr overlapped
        );

        internal static uint? getBlockSizeWindows(SafeHandle handle)
        {
            int size = 0;
            var query = new STORAGE_PROPERTY_QUERY()
            {
                PropertyId = StorageAccessAlignmentProperty,
                QueryType = PropertyStandardQuery,
                AdditionalParameters = 0
            };
            var alignment = new STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR();

            bool result = DeviceIoControl(
                handle,
                IOCTL_STORAGE_QUERY_PROPERTY,
                ref query, Marshal.SizeOf(query),
                out alignment, Marshal.SizeOf(alignment), out size,
                IntPtr.Zero
            );
            if (result) return alignment.BytesPerPhysicalSector;

            // IOCTL_STORAGE_QUERY_PROPERTY doesn't work for external drives
            // fallback to the logical sector size.
            // if an Advanced Format drive is in emulation mode,
            // this will always report 512, even when the physical is 4096
            DISK_GEOMETRY geometry = new DISK_GEOMETRY();
            result = DeviceIoControl(
                handle,
                IOCTL_DISK_GET_DRIVE_GEOMETRY,
                IntPtr.Zero, 0,
                out geometry, Marshal.SizeOf(geometry), out size,
                IntPtr.Zero
            );
            if (result) return geometry.BytesPerSector;
            return null;
        }

        // --- unix-specific code ---

        // #define _IOC_NRBITS              8
        // #define _IOC_TYPEBITS            8
        // #define _IOC_SIZEBITS            14
        // #define _IOC_DIRBITS             2
        // #define _IOC_NRSHIFT	            0
        //
        // #define _IOC_TYPESHIFT	        (_IOC_NRSHIFT+_IOC_NRBITS)
        //                                  \-> (0 + 8) --> 8
        // #define _IOC_SIZESHIFT	        (_IOC_TYPESHIFT+_IOC_TYPEBITS)
        //                                  \-> (8 + 8) --> 16
        // #define _IOC_DIRSHIFT	        (_IOC_SIZESHIFT+_IOC_SIZEBITS)
        //                                  \-> (16 + 14) --> 30
        //
        // #define _IOC(dir,type,nr,size)   (((dir)  << _IOC_DIRSHIFT) | ((type) << _IOC_TYPESHIFT) | \
        //                                      ((nr)   << _IOC_NRSHIFT) | ((size) << _IOC_SIZESHIFT))
        //                                  \-> (((dir) << 30) | ((type) << 8) | ((nr) << 0) | ((size) << 16))
        //
        // #define _IOC_READ                2U
        // #define _IOC_TYPECHECK(t)        (sizeof(t))
        // #define _IOR(type,nr,size)       _IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
        //                                  \-> _IOC(2U,(type),(nr),(sizeof(size)))
        //                                  \-> ((2U << 30) | ((type) << 8) | ((nr) << 0) | (sizeof(size) << 16))
        //
        // #define BLKBSZGET                _IOR(0x12, 112, size_t)
        //                                  \-> ((2U << 30) | (0x12 << 8) | (112 << 0) | (sizeof(size_t) << 16))
        internal static uint BLKBSZGET = 0x80001270 | ((uint)(UIntPtr.Size) << 16);

        [DllImport("libc", EntryPoint = "ioctl", SetLastError = true)]
        internal static extern int Ioctl(SafeHandle handle, uint request, ref UIntPtr size);

        internal static uint? getBlockSizeUnix(SafeHandle handle)
        {
            UIntPtr size = UIntPtr.Zero;
            int result = Ioctl(handle, BLKBSZGET, ref size);
            if (result != -1) return size.ToUInt32();
            return null;
        }

        // --- FileStream extension method ---

        public static uint GetBlockSize(this FileStream file)
        {
            uint? size = null;
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                size = getBlockSizeWindows(file.SafeFileHandle);
            }
            else
            {
                size = getBlockSizeUnix(file.SafeFileHandle);
            }

            // --- default to common hard drive sector size ---
            // by the late 1980s, 512 byte sectors became industry standard for hard drives.
            // by the end of 2007, manufacturers started shipping "Advanced Format" drives
            // that used 4096 byte sectors; by January 2011 all manufacturers had switched over.
            // optical drives tend to use sector sizes of 2048 bytes.
            // solid state drives don't have tracks and sectors like hard drives,
            // they typically have RAM pages of 4096 bytes which combine into a 512k block.
            // for backwards compatibility with older software, many newer drives
            // have emulation modes that report the sector size as 512 bytes.
            return size ?? 4096;
        }
    }
}