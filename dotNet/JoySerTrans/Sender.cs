//#define SIMULATE

using System;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text;

namespace Konamiman.JoySerTrans
{
    internal class Sender(string portName, int bauds)
    {
        const int CHUNK_SIZE = 1024;

        SerialPort port = null;
        FileStream fileStream = null;

        public event EventHandler<(long, string)> HeaderSent;
        public event EventHandler<int> DataSent;

        public void Send(string filePath, string fileNameToSend = null)
        {
            try {
                SendCore(filePath, fileNameToSend);
            }
            finally {
                if(port != null) {
                    port.Close();
                    port = null;
                }

                if(fileStream != null) {
                    fileStream.Close();
                    fileStream = null;
                }
            }
        }

        private void SendCore(string filePath, string fileNameToSend = null)
        {
            // Check/process arguments

            fileNameToSend ??= Path.GetFileName(filePath).ToUpper();
            if(Path.GetFileNameWithoutExtension(fileNameToSend).Length > 8) {
                throw new ArgumentException("File name is too long, maximum length is 8");
            }
            if(Path.GetExtension(fileNameToSend).Length > 4) { //4, not 3, since it includes the dot!
                throw new ArgumentException("File extension is too long, maximum length is 3");
            }

            // Open file and serial port

            var fileInfo = new FileInfo(filePath);
            var fileStream = fileInfo.OpenRead();
            var fileLength = fileInfo.Length;

#if !SIMULATE
            var port = new SerialPort(portName, bauds, Parity.None, dataBits: 8) { Handshake = Handshake.None, StopBits = StopBits.One, WriteTimeout = 5000, ReadTimeout = 5000 };
            port.Open();
#endif

            // Define a local function for sending a chunk of data.
            // After sending the chunk it expects the peer to send one byte:
            // 0 = Ok, 1 = CRC error, >1 = Other error.

            void sendData(byte[] data)
            {
                var retries = 0;
                while(true) {
#if SIMULATE
                    System.Threading.Thread.Sleep(10);
                    var result = data.Length > 19 ? 1 : 0;
#else
                    port.Write(data, 0, data.Length);
                    var result = port.ReadByte();
#endif
                    if(result == 0) {
                        return;
                    }
                    else if(result == 1) {
                        retries++;
                        if(retries > 5) {
                            throw new Exception("Too many CRC errors");
                        }
                        continue;
                    }
                    else {
                        throw new Exception($"Peer closed connection with code {result}");
                    }
                }
            }

            // Compose and send header

            var header = new byte[12 + 1 + 4 + 2]; //File name, 0, file length, CRC
            var encodedLength = Encoding.ASCII.GetBytes(fileNameToSend, 0, fileNameToSend.Length, header, 0);
            header[encodedLength] = 0;

            var lengthBytes = BitConverter.IsLittleEndian ? BitConverter.GetBytes(fileLength) : BitConverter.GetBytes(fileLength).Reverse().ToArray();
            Array.Copy(lengthBytes, 0, header, 12 + 1, 4);

            var crc = CalculateCrc(header, header.Length - 2);
            header[^2] = (byte)(crc & 0xFF);
            header[^1] = (byte)(crc >> 8);

            sendData(header);
            HeaderSent?.Invoke(this, (fileLength, fileNameToSend));

            // Send file in chunks

            var buffer = new byte[CHUNK_SIZE + 2];
            int actualChunkSize;

            while((actualChunkSize = fileStream.Read(buffer, 0, CHUNK_SIZE)) > 0) {
                crc = CalculateCrc(buffer, actualChunkSize);
                buffer[actualChunkSize] = (byte)(crc & 0xFF);
                buffer[actualChunkSize + 1] = (byte)(crc >> 8);

                sendData(buffer.Take(actualChunkSize + 2).ToArray());
                DataSent?.Invoke(this, actualChunkSize + 2);
            }
        }


        /*
        // https://stjarnhimlen.se/snippets/crc-16.c
        //                                      16   12   5
        // this is the CCITT CRC 16 polynomial X  + X  + X  + 1.
        // This works out to be 0x1021, but the way the algorithm works
        // lets us use 0x8408 (the reverse of the bit pattern).  The high
        // bit is always assumed to be set, thus we only use 16 bits to
        // represent the 17 bit value.
        */
        private static ushort CalculateCrc(byte[] data, int? length = null)
        {
            const ushort POLY = 0x8408;

            int i;
            uint dataByte;
            int dataIndex = 0;
            uint crc = 0xffff;
            length ??= data.Length;

            if(length == 0) {
                return ((ushort)~crc);
            }

            do {
                for(i = 0, dataByte = (uint)0xff & data[dataIndex++]; i < 8; i++, dataByte >>= 1)
                {
                    if(((crc & 0x0001) ^ (dataByte & 0x0001)) != 0) {
                        crc = (crc >> 1) ^ POLY;
                    }
                    else {
                        crc >>= 1;
                    }
                }
            } while(--length > 0);

            crc = ~crc;
            dataByte = crc;
            crc = (crc << 8) | (dataByte >> 8 & 0xff);

            return (ushort)crc;
        }
    }
}
