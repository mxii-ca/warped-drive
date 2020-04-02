using System;
using System.IO;
using System.Linq;
using System.Reflection;

namespace WarpedDrive
{
    public class Volume
    {
        protected long offset;
        protected Stream inner;

        protected Volume(Stream stream){
            inner = stream;
            offset = stream.Position;
        }

        public static bool IsHeaderValid(byte[] header) => false;

        public static Volume Parse(Stream stream)
        {
            byte[] header = new byte[512];
            stream.ReadExact(header, 0, header.Length);
            stream.Seek(-512, SeekOrigin.Current);

            var query = typeof(Volume).Assembly.GetTypes()
                .Where(t => !t.IsAbstract && t.IsSubclassOf(typeof(Volume)));
            foreach (var subclass in query)
            {
                if ((bool)subclass.GetMethod("IsHeaderValid").Invoke(null, new object[] { header }))
                {
                    Console.WriteLine($"Volume Type: {subclass.Name}");
                    return (Volume)Activator.CreateInstance(
                        subclass,
                        BindingFlags.NonPublic | BindingFlags.CreateInstance | BindingFlags.Instance,
                        null,
                        new object[] { stream },
                        null);
                }
            }

            throw new NotSupportedException("unrecognized volume type");
        }
    }
}