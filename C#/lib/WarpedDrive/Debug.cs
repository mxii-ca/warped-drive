using System;
using System.Linq;


namespace WarpedDrive
{
    public static class Debug
    {
        public static void PrintXxd(this byte[] data, long address)
        {
            System.Console.Error.WriteLine("");
            for (int i = 0; i < data.Length; i += 16)
            {
                System.Console.Error.Write($"{address + i:X08}:");

                int j = 0;
                for (; j < 8 && i + j < data.Length; j++)
                    System.Console.Error.Write($" {data[i + j]:X02}");
                if (i + j < data.Length)
                    System.Console.Error.Write(" -");
                else
                    System.Console.Error.Write("  ");
                for (; j < 16 && i + j < data.Length; j++)
                    System.Console.Error.Write($" {data[i + j]:X02}");
                for (; j < 16; j++)
                    System.Console.Error.Write("   ");

                System.Console.Error.Write("  ");

                for (j = 0; j < 16 && i + j < data.Length; j++)
                    if (data[i + j] < 32 || data[i + j] > 126)
                        System.Console.Error.Write(".");
                    else
                        System.Console.Error.Write($"{char.ConvertFromUtf32(data[i + j])}");

                System.Console.Error.WriteLine("");
            }
            System.Console.Error.WriteLine("");
        }

        public static void PrintXxd(this byte[] data) => PrintXxd(data, 0);
    }
}