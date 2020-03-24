using System;
using System.IO;

namespace WarpedDrive.Main
{
    class Program
    {
        static void PrintUsage(string program, bool error)
        {
            TextWriter stream = Console.Out;
            if (error) stream = Console.Error;
            stream.WriteLine($"usage: {program} [-h] device");
        }

        static void PrintHelp(string program)
        {
            PrintUsage(program, false);
            Console.Out.WriteLine("");
            Console.Out.WriteLine("filesystem reader");
            Console.Out.WriteLine("");
            Console.Out.WriteLine("positional arguments:");
            Console.Out.WriteLine("  device      device or file to open");
            Console.Out.WriteLine("");
            Console.Out.WriteLine("optional arguments:");
            Console.Out.WriteLine("  -h, --help  show this help message and exit");
        }

        static int Main(string[] args)
        {
            string program = Environment.GetCommandLineArgs()[0];

            foreach (string arg in args)
            {
                if (arg == "-h" || arg == "--help")
                {
                    PrintHelp(program);
                    return 1;
                }
            }
            if (args.Length < 1)
            {
                PrintUsage(program, true);
                Console.Error.WriteLine($"{program}: error: the following arguments are required: device");
                return 1;
            }
            if (args.Length > 1)
            {
                PrintUsage(program, true);
                Console.Error.WriteLine($"{program}: error: unrecognized arguments: {string.Join(' ', args[1..])}");
                return 1;
            }

            string path = args[0];
            Console.Out.WriteLine($"Opening: {path}");
            try
            {
                using (Device device = Device.Open(path, 4096))
                {
                    // FIXME: XXX: ...
                }
            }
            catch (Exception ex) when (
                typeof(ArgumentException).IsInstanceOfType(ex) ||   // + ArgumentNullException
                typeof(IOException).IsInstanceOfType(ex) ||         // + PathTooLongException, DirectoryNotFoundException, FileNotFoundException
                typeof(UnauthorizedAccessException).IsInstanceOfType(ex) ||
                typeof(NotSupportedException).IsInstanceOfType(ex)
            )
            {
                Console.Error.WriteLine($"{program}: error: failed to open {path}: {ex.Message}");
                return 2;
            }
            return 0;
        }
    }
}
