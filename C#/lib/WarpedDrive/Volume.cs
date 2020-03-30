using System;
using System.IO;
using System.Linq;
using System.Reflection;

namespace WarpedDrive
{
    public class Volume
    {
        protected Stream inner;

        protected Volume(Stream stream, byte[] header){
            inner = stream;
        }

        public static bool IsHeaderValid(byte[] header) => false;

        public static Volume Parse(Stream stream)
        {
            byte[] header = new byte[512];
            stream.Read(header, 0, header.Length);
            header.PrintXxd();

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
                        new object[] { stream, header },
                        null);
                }
            }

            throw new NotSupportedException("unrecognized volume type");
        }
    }
}