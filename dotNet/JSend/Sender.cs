//#define SIMULATE

using System;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading;

namespace Konamiman.JoySerTrans
{
    internal class Sender(string portName, int bauds)
    {
        const int CHUNK_SIZE = 1024;
        const int DATA_SEND_RCV_TIMEOUT_MS = 5000;

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
            var port = new SerialPort(portName, bauds, Parity.None, dataBits: 8) { Handshake = Handshake.RequestToSend, StopBits = StopBits.One, WriteTimeout = DATA_SEND_RCV_TIMEOUT_MS, ReadTimeout = DATA_SEND_RCV_TIMEOUT_MS };
            port.Open();
#endif

            int waitedTimes = 0;

            void Wait(string errorMessage)
            {
                if(waitedTimes > DATA_SEND_RCV_TIMEOUT_MS/10) {
                    throw new Exception(errorMessage);
                }

                Thread.Sleep(10);
                waitedTimes++;
            }

            // Define a local function for sending a chunk of data.
            // After sending the chunk it expects the peer to send one byte:
            // 0 = Ok, 1 = CRC error (will trigger a retry), 2 = Header CRC error, >2 = Other error.

            void sendData(byte[] data)
            {
                waitedTimes = 0;
                var retries = 0;
                while(true) {
#if SIMULATE
                    System.Threading.Thread.Sleep(10);
                    var result = data.Length > 19 ? 1 : 0;
#else
                    while(port.CtsHolding) Wait("CTS line timeout (peer is not ready to receive data)");
                    port.Write(data, 0, data.Length);
 
                    var resultBytes = new byte[4];
                    while(port.CtsHolding) Wait("RTS line timeout (peer is not ready to send status data)");

                    while(port.BytesToRead < resultBytes.Length) Wait("Status data reception timeout");
                    port.Read(resultBytes, 0, resultBytes.Length);

                    var resultBytesByCount = resultBytes.GroupBy(x => x).Select(x => new { Value = x.First(), Count = x.Count() } ).ToArray();
                    var result = resultBytesByCount.OrderByDescending(x => x.Count).ThenByDescending(x => x.Value).First().Value;
#endif

                    if(result == 0) {
                        return;
                    }
                    else if(result == 1) {
                        retries++;
                        if(retries > 4) {
                            throw new Exception("Too many CRC errors");
                        }
                        continue;
                    }
                    else if(result == 2) {
                        throw new Exception("CRC error in the header");
                    }
                    else {
                        throw new Exception($"Peer closed connection with code 0x{result:X2}");
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
         * XMODEM CRC calculation
         * https://mdfs.net/Info/Comp/Comms/CRC16.htm
         * "The XMODEM CRC is CRC-16 with a start value of &0000, the end value is not XORed, and uses a polynoimic of 0x1021."
        */
        private static ushort CalculateCrc(byte[] data, int? length = null)
        {
            const ushort POLY = 0x1021;

            length ??= data.Length;
            int crc = 0;

            for(var dataIndex=0; dataIndex < length; dataIndex++) {
                crc = crc ^ (data[dataIndex] << 8);      /* Fetch byte from memory, XOR into CRC top byte*/
                for(var i = 0; i < 8; i++) {       /* Prepare to rotate 8 bits */
                    crc = crc << 1;            /* rotate */
                    if((crc & 0x10000) != 0) {           /* bit 15 was set (now bit 16)... */
                        crc = (crc ^ POLY) & 0xFFFF;     /* XOR with XMODEM polynomic */
                    }                                    /* and ensure CRC remains 16-bit value */
                }                              /* Loop for 8 bits */
            }

            return (ushort)crc;                /* Return updated CRC */
        }
    }
}
