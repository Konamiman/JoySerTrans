using System;
using System.Globalization;

namespace Konamiman.JoySerTrans;

internal class Program
{
    static long fileLength;
    static long bytesSent;
    static string filenameSent;

    static int Main(string[] args)
    {
        CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;

        Console.WriteLine(
@"Simple serial file sender v1.0
By Konamiman, 2024
");

        if(args.Length < 3) {
            Console.WriteLine(
@"Usage: jst <file> <port> <bauds> [<filename to send>]
");
            return 0;
        }

        try {
            var sender = new Sender(args[1], int.Parse(args[2]));
            sender.HeaderSent += Sender_HeaderSent;
            sender.DataSent += Sender_DataSent;
            Console.CursorVisible = false;

            sender.Send(args[0], args.Length > 3 ? args[3] : null);

            Console.WriteLine("100%  ");
            Console.WriteLine("Done!");
            return 0;
        }
        catch(Exception ex) {
            if(fileLength > 0) Console.WriteLine("");
            Console.WriteLine("*** " + ex.Message);
            return 1;
        }
        finally {
            Console.CursorVisible = true;
        }
    }

    private static void Sender_DataSent(object sender, int e)
    {
        bytesSent += e - 2;
        var pos = Console.CursorLeft;
        Console.Write($"{((decimal)bytesSent / fileLength) * 100:0.0}%");
        Console.CursorLeft = pos;
    }

    private static void Sender_HeaderSent(object sender, (long, string) e)
    {
        fileLength = e.Item1;
        filenameSent = e.Item2;
        Console.Write($"Sending file as {filenameSent}, size is {(decimal)fileLength / 1024:0}K... ");
    }
}
