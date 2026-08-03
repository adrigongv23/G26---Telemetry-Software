using System.IO.Ports;

namespace App_radio;

/// <summary>
/// Comunicación USB-serie con el ESP32 y protocolo de audio heredado del programa C++.
/// </summary>
internal sealed class RadioSerialService : IDisposable
{
    private const int BaudRate = 115200;
    private const int AudioFrameMax = 1600;
    private const int AudioChunkMax = 200;
    private const int AudioChunkTotalMax = 8;
    private const int HeaderSize = 11;
    private const byte ControlMagic1 = 0xE5;
    private const byte ControlMagic2 = 0x5F;
    private const byte ControlCommandBoxPtt = 0x01;

    private readonly object _portLock = new();
    private readonly object _writeLock = new();
    private readonly object _parserLock = new();
    private readonly AudioPacketParser _parser = new();

    private SerialPort? _port;
    private int _transmitMode;
    private byte _txFrameSequence;

    public event EventHandler<sbyte[]>? AudioFrameReceived;
    public event EventHandler<string>? Error;

    public bool IsOpen
    {
        get
        {
            lock (_portLock)
            {
                return _port?.IsOpen == true;
            }
        }
    }

    public string? PortName
    {
        get
        {
            lock (_portLock)
            {
                return _port?.PortName;
            }
        }
    }

    public void Open(string portName)
    {
        if (string.IsNullOrWhiteSpace(portName))
        {
            throw new ArgumentException("Debes seleccionar un puerto COM.", nameof(portName));
        }

        lock (_portLock)
        {
            if (_port?.IsOpen == true)
            {
                throw new InvalidOperationException("El puerto serie ya está abierto.");
            }

            var port = new SerialPort(portName, BaudRate, Parity.None, 8, StopBits.One)
            {
                Handshake = Handshake.None,
                DtrEnable = true,
                RtsEnable = true,
                ReadBufferSize = 8192,
                WriteBufferSize = 8192,
                ReadTimeout = 100,
                // Se escriben fragmentos de 200 bytes. Un timeout corto evita que
                // soltar PTT o cerrar la ventana deje bloqueada la interfaz.
                WriteTimeout = 150
            };

            port.DataReceived += Port_DataReceived;
            port.ErrorReceived += Port_ErrorReceived;

            try
            {
                port.Open();
                port.DiscardInBuffer();
                port.DiscardOutBuffer();

                lock (_parserLock)
                {
                    _parser.Reset();
                }

                Volatile.Write(ref _transmitMode, 0);
                _txFrameSequence = 0;
                _port = port;
            }
            catch
            {
                port.DataReceived -= Port_DataReceived;
                port.ErrorReceived -= Port_ErrorReceived;
                port.Dispose();
                throw;
            }
        }
    }

    public void Close()
    {
        SerialPort? port;

        // Primero apartamos el puerto para impedir escrituras nuevas.
        lock (_portLock)
        {
            port = _port;
            _port = null;

            if (port is not null)
            {
                port.DataReceived -= Port_DataReceived;
                port.ErrorReceived -= Port_ErrorReceived;
            }
        }

        Volatile.Write(ref _transmitMode, 0);

        lock (_parserLock)
        {
            _parser.Reset();
        }

        if (port is null)
        {
            return;
        }

        // Si había un fragmento de audio escribiéndose, solo esperamos a que
        // termine ese fragmento (máximo el WriteTimeout), no un frame completo.
        lock (_writeLock)
        {
            try
            {
                if (port.IsOpen)
                {
                    port.Close();
                }
            }
            catch (Exception ex) when (ex is IOException
                                       or InvalidOperationException
                                       or UnauthorizedAccessException)
            {
                Error?.Invoke(this, $"Error cerrando el puerto serie: {ex.Message}");
            }
            finally
            {
                try
                {
                    port.Dispose();
                }
                catch
                {
                    // El dispositivo ya no está disponible; no queda ningún recurso útil que cerrar.
                }
            }
        }
    }

    public void SetTransmitMode(bool transmitting)
    {
        Volatile.Write(ref _transmitMode, transmitting ? 1 : 0);
    }

    /// <summary>
    /// Abre una sesión de transmisión y envía START sin permitir que se cruce
    /// con el STOP de una pulsación anterior.
    /// </summary>
    public void BeginTransmitSession()
    {
        lock (_writeLock)
        {
            SerialPort port = GetOpenPort();
            Volatile.Write(ref _transmitMode, 1);
            WritePttControlLocked(port, true);
        }
    }

    /// <summary>
    /// Envía STOP después de que el bucle de transmisión haya vaciado todos los
    /// frames grabados. Solo entonces desactiva el envío de audio.
    /// </summary>
    public void EndTransmitSession()
    {
        lock (_writeLock)
        {
            SerialPort port = GetOpenPort();
            WritePttControlLocked(port, false);
            Volatile.Write(ref _transmitMode, 0);
        }
    }

    /// <summary>
    /// Envía un control corto y separado del audio. Formato: E5 5F 01 estado.
    /// </summary>
    public void SendPttControl(bool active)
    {
        lock (_writeLock)
        {
            SerialPort port = GetOpenPort();
            WritePttControlLocked(port, active);
        }
    }

    private SerialPort GetOpenPort()
    {
        lock (_portLock)
        {
            SerialPort port = _port
                ?? throw new InvalidOperationException("El puerto serie no está abierto.");

            if (!port.IsOpen)
            {
                throw new InvalidOperationException("El puerto serie no está abierto.");
            }

            return port;
        }
    }

    private static void WritePttControlLocked(SerialPort port, bool active)
    {
        byte[] packet =
        {
            ControlMagic1,
            ControlMagic2,
            ControlCommandBoxPtt,
            active ? (byte)1 : (byte)0
        };

        port.Write(packet, 0, packet.Length);
    }

    public void SendAudioFrame(sbyte[] samples)
    {
        ArgumentNullException.ThrowIfNull(samples);

        if (samples.Length == 0 || samples.Length > AudioFrameMax)
        {
            throw new ArgumentOutOfRangeException(
                nameof(samples),
                $"El frame debe contener entre 1 y {AudioFrameMax} muestras.");
        }

        if (Volatile.Read(ref _transmitMode) == 0)
        {
            return;
        }

        int frameLength = samples.Length;
        int chunkTotal = (frameLength + AudioChunkMax - 1) / AudioChunkMax;

        if (chunkTotal <= 0 || chunkTotal > AudioChunkTotalMax)
        {
            throw new InvalidOperationException("El frame genera un número de fragmentos no válido.");
        }

        lock (_writeLock)
        {
            SerialPort port;
            byte frameSequence;

            lock (_portLock)
            {
                port = _port
                    ?? throw new InvalidOperationException("El puerto serie no está abierto.");

                if (!port.IsOpen)
                {
                    throw new InvalidOperationException("El puerto serie no está abierto.");
                }

                // Una vez iniciado, el frame se envía completo. El STOP se coloca
                // detrás de la cola de audio y nunca corta un frame a la mitad.
                frameSequence = _txFrameSequence++;
            }

            int offset = 0;

            for (int chunkIndex = 0; chunkIndex < chunkTotal; chunkIndex++)
            {
                int chunkLength = Math.Min(AudioChunkMax, frameLength - offset);
                var packet = new byte[HeaderSize + chunkLength];

                packet[0] = 0xE5;
                packet[1] = 0x5E;
                packet[2] = frameSequence;
                packet[3] = (byte)chunkIndex;
                packet[4] = (byte)chunkTotal;
                packet[5] = (byte)(frameLength & 0xFF);
                packet[6] = (byte)((frameLength >> 8) & 0xFF);
                packet[7] = (byte)(offset & 0xFF);
                packet[8] = (byte)((offset >> 8) & 0xFF);
                packet[9] = (byte)(chunkLength & 0xFF);
                packet[10] = (byte)((chunkLength >> 8) & 0xFF);

                for (int i = 0; i < chunkLength; i++)
                {
                    packet[HeaderSize + i] = unchecked((byte)samples[offset + i]);
                }

                port.Write(packet, 0, packet.Length);
                offset += chunkLength;
            }
        }
    }

    private void Port_DataReceived(object sender, SerialDataReceivedEventArgs e)
    {
        if (sender is not SerialPort port)
        {
            return;
        }

        try
        {
            byte[] buffer;
            int bytesRead;

            // El lock del puerto solo se mantiene mientras se extraen los bytes.
            lock (_portLock)
            {
                if (!ReferenceEquals(port, _port) || !port.IsOpen)
                {
                    return;
                }

                int available = port.BytesToRead;
                if (available <= 0)
                {
                    return;
                }

                buffer = new byte[available];
                bytesRead = port.Read(buffer, 0, buffer.Length);
            }

            List<sbyte[]>? completedFrames = null;

            lock (_parserLock)
            {
                for (int i = 0; i < bytesRead; i++)
                {
                    _parser.ProcessByte(buffer[i], frame =>
                    {
                        completedFrames ??= new List<sbyte[]>();
                        completedFrames.Add(frame);
                    });
                }
            }

            if (completedFrames is not null)
            {
                foreach (sbyte[] frame in completedFrames)
                {
                    AudioFrameReceived?.Invoke(this, frame);
                }
            }
        }
        catch (Exception ex) when (ex is IOException
                                   or InvalidOperationException
                                   or UnauthorizedAccessException
                                   or TimeoutException)
        {
            Error?.Invoke(this, $"Error leyendo el puerto serie: {ex.Message}");
        }
    }

    private void Port_ErrorReceived(object sender, SerialErrorReceivedEventArgs e)
    {
        Error?.Invoke(this, $"Error del puerto serie: {e.EventType}");
    }

    public void Dispose()
    {
        Close();
    }

    private enum ReceiveState
    {
        WaitE5,
        Wait5E,
        ReadFrameSequence,
        ReadChunkIndex,
        ReadChunkTotal,
        ReadFrameLengthLow,
        ReadFrameLengthHigh,
        ReadOffsetLow,
        ReadOffsetHigh,
        ReadChunkLengthLow,
        ReadChunkLengthHigh,
        ReadAudio
    }

    private sealed class AudioPacketParser
    {
        private readonly sbyte[] _frameBuffer = new sbyte[AudioFrameMax];
        private readonly bool[] _receivedChunks = new bool[AudioChunkTotalMax];

        private ReceiveState _state = ReceiveState.WaitE5;
        private byte _frameSequence;
        private byte _chunkIndex;
        private byte _chunkTotal;
        private ushort _frameLength;
        private ushort _offset;
        private ushort _chunkLength;
        private byte[] _audio = Array.Empty<byte>();
        private int _audioIndex;

        private bool _frameActive;
        private byte _currentFrameSequence;
        private byte _currentChunkTotal;
        private ushort _currentFrameLength;
        private int _receivedChunkCount;

        public void ProcessByte(byte value, Action<sbyte[]> onFrameCompleted)
        {
            switch (_state)
            {
                case ReceiveState.WaitE5:
                    if (value == 0xE5)
                    {
                        _state = ReceiveState.Wait5E;
                    }
                    break;

                case ReceiveState.Wait5E:
                    if (value == 0x5E)
                    {
                        ResetPacket();
                        _state = ReceiveState.ReadFrameSequence;
                    }
                    else
                    {
                        _state = value == 0xE5
                            ? ReceiveState.Wait5E
                            : ReceiveState.WaitE5;
                    }
                    break;

                case ReceiveState.ReadFrameSequence:
                    _frameSequence = value;
                    _state = ReceiveState.ReadChunkIndex;
                    break;

                case ReceiveState.ReadChunkIndex:
                    _chunkIndex = value;
                    _state = ReceiveState.ReadChunkTotal;
                    break;

                case ReceiveState.ReadChunkTotal:
                    _chunkTotal = value;
                    _state = ReceiveState.ReadFrameLengthLow;
                    break;

                case ReceiveState.ReadFrameLengthLow:
                    _frameLength = value;
                    _state = ReceiveState.ReadFrameLengthHigh;
                    break;

                case ReceiveState.ReadFrameLengthHigh:
                    _frameLength |= (ushort)(value << 8);
                    _state = ReceiveState.ReadOffsetLow;
                    break;

                case ReceiveState.ReadOffsetLow:
                    _offset = value;
                    _state = ReceiveState.ReadOffsetHigh;
                    break;

                case ReceiveState.ReadOffsetHigh:
                    _offset |= (ushort)(value << 8);
                    _state = ReceiveState.ReadChunkLengthLow;
                    break;

                case ReceiveState.ReadChunkLengthLow:
                    _chunkLength = value;
                    _state = ReceiveState.ReadChunkLengthHigh;
                    break;

                case ReceiveState.ReadChunkLengthHigh:
                    _chunkLength |= (ushort)(value << 8);

                    if (!HeaderIsValid())
                    {
                        _state = ReceiveState.WaitE5;
                        break;
                    }

                    _audio = new byte[_chunkLength];
                    _audioIndex = 0;
                    _state = ReceiveState.ReadAudio;
                    break;

                case ReceiveState.ReadAudio:
                    _audio[_audioIndex++] = value;

                    if (_audioIndex >= _audio.Length)
                    {
                        CompletePacket(onFrameCompleted);
                        _state = ReceiveState.WaitE5;
                    }
                    break;
            }
        }

        public void Reset()
        {
            _state = ReceiveState.WaitE5;
            _frameActive = false;
            ResetPacket();
            Array.Clear(_frameBuffer, 0, _frameBuffer.Length);
            Array.Clear(_receivedChunks, 0, _receivedChunks.Length);
        }

        private bool HeaderIsValid()
        {
            if (_chunkTotal == 0 || _chunkTotal > AudioChunkTotalMax)
            {
                return false;
            }

            if (_chunkIndex >= _chunkTotal || _chunkIndex >= AudioChunkTotalMax)
            {
                return false;
            }

            if (_frameLength == 0 || _frameLength > AudioFrameMax)
            {
                return false;
            }

            if (_chunkLength == 0 || _chunkLength > AudioChunkMax)
            {
                return false;
            }

            return _offset + _chunkLength <= _frameLength
                   && _offset + _chunkLength <= AudioFrameMax;
        }

        private void CompletePacket(Action<sbyte[]> onFrameCompleted)
        {
            if (!_frameActive || _frameSequence != _currentFrameSequence)
            {
                StartFrame();
            }

            if (_frameLength != _currentFrameLength || _chunkTotal != _currentChunkTotal)
            {
                _frameActive = false;
                return;
            }

            for (int i = 0; i < _audio.Length; i++)
            {
                _frameBuffer[_offset + i] = unchecked((sbyte)_audio[i]);
            }

            if (!_receivedChunks[_chunkIndex])
            {
                _receivedChunks[_chunkIndex] = true;
                _receivedChunkCount++;
            }

            if (_receivedChunkCount < _currentChunkTotal)
            {
                return;
            }

            var completedFrame = new sbyte[_currentFrameLength];
            Array.Copy(_frameBuffer, completedFrame, _currentFrameLength);
            _frameActive = false;
            onFrameCompleted(completedFrame);
        }

        private void StartFrame()
        {
            Array.Clear(_frameBuffer, 0, _frameBuffer.Length);
            Array.Clear(_receivedChunks, 0, _receivedChunks.Length);

            _currentFrameSequence = _frameSequence;
            _currentChunkTotal = _chunkTotal;
            _currentFrameLength = _frameLength;
            _receivedChunkCount = 0;
            _frameActive = true;
        }

        private void ResetPacket()
        {
            _frameSequence = 0;
            _chunkIndex = 0;
            _chunkTotal = 0;
            _frameLength = 0;
            _offset = 0;
            _chunkLength = 0;
            _audio = Array.Empty<byte>();
            _audioIndex = 0;
        }
    }
}
