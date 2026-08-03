using System.Threading.Channels;

namespace App_radio;

/// <summary>
/// Une la interfaz, el audio y el puerto serie.
/// </summary>
internal sealed class RadioController : IDisposable
{
    private const int MaxQueuedMicrophoneFrames = 2;

    private readonly RadioSerialService _serial = new();
    private readonly AudioEngine _audio = new();
    private readonly object _stateLock = new();
    private readonly object _pttSessionLock = new();

    private Channel<TransmitItem>? _transmitChannel;
    private CancellationTokenSource? _transmitCancellation;
    private Task? _transmitTask;
    private bool _connected;
    private int _pttActive;
    private int _pttSessionId;
    private int _queuedMicrophoneFrames;
    private bool _disposed;

    public event EventHandler<string>? Error;
    public event EventHandler? PilotAudioReceived;
    public event EventHandler? ConnectionFaulted;

    public RadioController()
    {
        _serial.AudioFrameReceived += Serial_AudioFrameReceived;
        _serial.Error += Serial_Error;
        _audio.MicrophoneFrameReady += Audio_MicrophoneFrameReady;
        _audio.Error += Audio_Error;
    }

    public bool IsConnected => Volatile.Read(ref _connected) && _serial.IsOpen;

    public bool IsPttActive => Volatile.Read(ref _pttActive) != 0;

    public void Connect(string portName)
    {
        ThrowIfDisposed();

        lock (_stateLock)
        {
            if (_connected)
            {
                return;
            }
        }

        var channel = Channel.CreateUnbounded<TransmitItem>(new UnboundedChannelOptions
        {
            SingleReader = true,
            SingleWriter = false,
            AllowSynchronousContinuations = false
        });

        var cancellation = new CancellationTokenSource();

        try
        {
            _serial.Open(portName);
            _audio.Start();

            lock (_stateLock)
            {
                _transmitChannel = channel;
                _transmitCancellation = cancellation;
                Volatile.Write(ref _connected, true);
                Volatile.Write(ref _pttActive, 0);
                Volatile.Write(ref _queuedMicrophoneFrames, 0);
                _pttSessionId = 0;
                _transmitTask = Task.Run(() => TransmitLoopAsync(channel.Reader, cancellation.Token), cancellation.Token);
            }
        }
        catch
        {
            cancellation.Cancel();
            cancellation.Dispose();
            _audio.Stop();
            _serial.Close();
            throw;
        }
    }

    public async Task DisconnectAsync()
    {
        Channel<TransmitItem>? channel;
        CancellationTokenSource? cancellation;
        Task? transmitTask;

        bool alreadyDisconnected;

        lock (_stateLock)
        {
            alreadyDisconnected = !_connected && _transmitTask is null;
            Volatile.Write(ref _connected, false);

            channel = _transmitChannel;
            cancellation = _transmitCancellation;
            transmitTask = _transmitTask;

            _transmitChannel = null;
            _transmitCancellation = null;
            _transmitTask = null;
        }

        if (alreadyDisconnected)
        {
            _audio.Stop();
            _serial.Close();
            return;
        }

        SetPtt(false);
        _audio.Stop();

        channel?.Writer.TryComplete();
        cancellation?.Cancel();

        if (transmitTask is not null)
        {
            try
            {
                await transmitTask.ConfigureAwait(false);
            }
            catch (OperationCanceledException)
            {
                // Cancelación normal al desconectar.
            }
        }

        cancellation?.Dispose();
        Volatile.Write(ref _queuedMicrophoneFrames, 0);
        _serial.Close();
    }

    public bool SetPtt(bool active)
    {
        if (active && !IsConnected)
        {
            return false;
        }

        lock (_pttSessionLock)
        {
            int requestedState = active ? 1 : 0;
            int previousState = Volatile.Read(ref _pttActive);

            if (previousState == requestedState)
            {
                return true;
            }

            Channel<TransmitItem>? channel;
            lock (_stateLock)
            {
                channel = _transmitChannel;
            }

            try
            {
                if (active)
                {
                    unchecked
                    {
                        ++_pttSessionId;
                    }

                    // START y el modo de transmisión se activan bajo el mismo lock
                    // de escritura. Así no puede cruzarse un STOP de la pulsación anterior.
                    _serial.BeginTransmitSession();
                    Volatile.Write(ref _pttActive, 1);
                    _audio.SetPtt(true);
                }
                else
                {
                    // Se deja de capturar inmediatamente, pero NO se vacía la cola ni se
                    // desactiva el puerto. El marcador END se procesa después de todos los
                    // frames ya grabados y solo entonces envía STOP al piloto.
                    Volatile.Write(ref _pttActive, 0);
                    _audio.SetPtt(false);

                    if (channel is not null)
                    {
                        channel.Writer.TryWrite(TransmitItem.EndPtt(_pttSessionId));
                    }
                    else
                    {
                        _serial.EndTransmitSession();
                    }
                }

                return true;
            }
            catch (Exception ex) when (ex is IOException
                                       or InvalidOperationException
                                       or UnauthorizedAccessException
                                       or TimeoutException)
            {
                _audio.SetPtt(false);
                _serial.SetTransmitMode(false);
                Volatile.Write(ref _pttActive, 0);
                Error?.Invoke(this, $"Error cambiando el PTT: {ex.Message}");
                ConnectionFaulted?.Invoke(this, EventArgs.Empty);
                return false;
            }
        }
    }

    public void SetBoxVolume(int percent, bool muted)
    {
        float gain = muted ? 0.0f : PercentToGain(percent);
        _audio.SetPlaybackGain(gain);
    }

    public void SetPilotVolume(int percent, bool muted)
    {
        float gain = muted ? 0.0f : PercentToGain(percent);
        _audio.SetMicrophoneGain(gain);
    }

    public static float PercentToGain(int percent)
    {
        // 0 -> 0.00x, 50 -> 1.00x y 100 -> 2.00x.
        // Así el control no multiplica directamente por 50 ni por 100.
        return Math.Clamp(percent, 0, 100) / 50.0f;
    }

    private async Task TransmitLoopAsync(ChannelReader<TransmitItem> reader, CancellationToken cancellationToken)
    {
        try
        {
            await foreach (TransmitItem item in reader.ReadAllAsync(cancellationToken))
            {
                if (item.Kind == TransmitItemKind.Audio)
                {
                    try
                    {
                        if (IsConnected && item.Audio is not null)
                        {
                            // Los frames que ya estaban en la cola se envían aunque el
                            // usuario haya soltado PTT. Eso conserva el final del mensaje.
                            _serial.SendAudioFrame(item.Audio);
                        }
                    }
                    catch (Exception ex) when (ex is IOException
                                               or InvalidOperationException
                                               or UnauthorizedAccessException
                                               or TimeoutException)
                    {
                        Error?.Invoke(this, $"Error enviando audio: {ex.Message}");
                        ConnectionFaulted?.Invoke(this, EventArgs.Empty);
                        return;
                    }
                    finally
                    {
                        Interlocked.Decrement(ref _queuedMicrophoneFrames);
                    }

                    continue;
                }

                lock (_pttSessionLock)
                {
                    // Si el usuario volvió a pulsar antes de llegar aquí, este END es
                    // antiguo y no debe mandar STOP ni cerrar la nueva transmisión.
                    if (!IsConnected || IsPttActive || item.SessionId != _pttSessionId)
                    {
                        continue;
                    }

                    try
                    {
                        _serial.EndTransmitSession();
                    }
                    catch (Exception ex) when (ex is IOException
                                               or InvalidOperationException
                                               or UnauthorizedAccessException
                                               or TimeoutException)
                    {
                        Error?.Invoke(this, $"Error terminando el envío de audio: {ex.Message}");
                        ConnectionFaulted?.Invoke(this, EventArgs.Empty);
                        return;
                    }
                }
            }
        }
        catch (OperationCanceledException)
        {
            // Cancelación normal al desconectar.
        }
    }

    private void Audio_MicrophoneFrameReady(object? sender, sbyte[] frame)
    {
        lock (_pttSessionLock)
        {
            Channel<TransmitItem>? channel;
            int sessionId;

            lock (_stateLock)
            {
                if (!_connected || !IsPttActive)
                {
                    return;
                }

                channel = _transmitChannel;
                sessionId = _pttSessionId;
            }

            if (channel is null)
            {
                return;
            }

            int queued = Interlocked.Increment(ref _queuedMicrophoneFrames);
            if (queued > MaxQueuedMicrophoneFrames)
            {
                Interlocked.Decrement(ref _queuedMicrophoneFrames);
                return;
            }

            if (!channel.Writer.TryWrite(TransmitItem.AudioFrame(frame, sessionId)))
            {
                Interlocked.Decrement(ref _queuedMicrophoneFrames);
            }
        }
    }

    private void Serial_AudioFrameReceived(object? sender, sbyte[] frame)
    {
        // La recepción no depende del PTT: el box puede transmitir y escuchar
        // al piloto simultáneamente.
        if (!IsConnected)
        {
            return;
        }

        _audio.PushReceivedFrame(frame);
        PilotAudioReceived?.Invoke(this, EventArgs.Empty);
    }

    private void Serial_Error(object? sender, string message)
    {
        Error?.Invoke(this, message);
    }

    private void Audio_Error(object? sender, string message)
    {
        Error?.Invoke(this, message);
        ConnectionFaulted?.Invoke(this, EventArgs.Empty);
    }

    private void ThrowIfDisposed()
    {
        if (_disposed)
        {
            throw new ObjectDisposedException(nameof(RadioController));
        }
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;

        try
        {
            DisconnectAsync().GetAwaiter().GetResult();
        }
        finally
        {
            _audio.MicrophoneFrameReady -= Audio_MicrophoneFrameReady;
            _audio.Error -= Audio_Error;
            _serial.AudioFrameReceived -= Serial_AudioFrameReceived;
            _serial.Error -= Serial_Error;
            _audio.Dispose();
            _serial.Dispose();
        }
    }

    private enum TransmitItemKind
    {
        Audio,
        EndPtt
    }

    private readonly record struct TransmitItem(TransmitItemKind Kind, sbyte[]? Audio, int SessionId)
    {
        public static TransmitItem AudioFrame(sbyte[] audio, int sessionId) => new(TransmitItemKind.Audio, audio, sessionId);
        public static TransmitItem EndPtt(int sessionId) => new(TransmitItemKind.EndPtt, null, sessionId);
    }
}
