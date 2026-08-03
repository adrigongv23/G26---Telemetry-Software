using NAudio.Wave;

namespace App_radio;

/// <summary>
/// Captura el micrófono del PC y reproduce el audio recibido por la radio.
/// Conserva el formato del programa C++ original: 8000 Hz, mono, PCM signed 8-bit por radio.
/// </summary>
internal sealed class AudioEngine : IDisposable
{
    private const int SampleRate = 8000;
    private const int Channels = 1;
    private const int MicrophoneFrameSamples = 1600;

    private readonly object _stateLock = new();
    private readonly object _captureLock = new();
    private readonly MicrophoneFilter _microphoneFilter = new();
    private readonly sbyte[] _microphoneFrame = new sbyte[MicrophoneFrameSamples];

    private WaveInEvent? _capture;
    private WaveOutEvent? _playback;
    private BufferedWaveProvider? _playbackBuffer;

    private int _microphoneFrameIndex;
    private bool _started;
    private bool _pttActive;
    private float _microphoneGain = 1.0f;
    private float _playbackGain = 1.0f;

    public event EventHandler<sbyte[]>? MicrophoneFrameReady;
    public event EventHandler<string>? Error;

    public bool IsStarted
    {
        get
        {
            lock (_stateLock)
            {
                return _started;
            }
        }
    }

    public bool PttActive
    {
        get
        {
            lock (_captureLock)
            {
                return _pttActive;
            }
        }
    }

    public void Start()
    {
        lock (_stateLock)
        {
            if (_started)
            {
                return;
            }

            var playbackBuffer = new BufferedWaveProvider(new WaveFormat(SampleRate, 16, Channels))
            {
                BufferDuration = TimeSpan.FromSeconds(2),
                DiscardOnBufferOverflow = true,
                ReadFully = true
            };

            var playback = new WaveOutEvent
            {
                DesiredLatency = 100,
                NumberOfBuffers = 3
            };

            var capture = new WaveInEvent
            {
                WaveFormat = new WaveFormat(SampleRate, 16, Channels),
                BufferMilliseconds = 20,
                NumberOfBuffers = 4
            };

            capture.DataAvailable += Capture_DataAvailable;
            capture.RecordingStopped += Capture_RecordingStopped;

            try
            {
                playback.Init(playbackBuffer);
                playback.Play();

                _playbackBuffer = playbackBuffer;
                _playback = playback;
                _capture = capture;
                _started = true;

                ResetMicrophoneState();
                //capture.StartRecording();
            }
            catch
            {
                capture.DataAvailable -= Capture_DataAvailable;
                capture.RecordingStopped -= Capture_RecordingStopped;
                capture.Dispose();

                try
                {
                    playback.Stop();
                }
                catch
                {
                    // No hay nada más que limpiar si el dispositivo no llegó a arrancar.
                }

                playback.Dispose();
                _playbackBuffer = null;
                _playback = null;
                _capture = null;
                _started = false;
                throw;
            }
        }
    }

    public void Stop()
    {
        WaveInEvent? capture;
        WaveOutEvent? playback;

        lock (_stateLock)
        {
            if (!_started && _capture is null && _playback is null)
            {
                return;
            }

            _started = false;
            capture = _capture;
            playback = _playback;
            _capture = null;
            _playback = null;
            _playbackBuffer = null;
        }

        SetPtt(false);

        if (capture is not null)
        {
            capture.DataAvailable -= Capture_DataAvailable;
            capture.RecordingStopped -= Capture_RecordingStopped;

            try
            {
                capture.StopRecording();
            }
            catch
            {
                // El dispositivo puede estar ya detenido si se desconectó físicamente.
            }

            capture.Dispose();
        }

        if (playback is not null)
        {
            try
            {
                playback.Stop();
            }
            catch
            {
                // El dispositivo puede estar ya detenido si se desconectó físicamente.
            }

            playback.Dispose();
        }

        ResetMicrophoneState();
    }

    public void SetPtt(bool active)
    {
        lock (_captureLock)
        {
            if (_pttActive == active)
            {
                return;
            }

            _pttActive = active;
            _microphoneFrameIndex = 0;
            _microphoneFilter.Reset();
        }

        // No se vacia ni se detiene la reproduccion al cambiar el PTT.
        // Captura y salida permanecen activas de forma simultanea.
    }

    public void SetMicrophoneGain(float gain)
    {
        lock (_captureLock)
        {
            _microphoneGain = Math.Clamp(gain, 0.0f, 2.0f);
        }
    }

    public void SetPlaybackGain(float gain)
    {
        lock (_stateLock)
        {
            _playbackGain = Math.Clamp(gain, 0.0f, 2.0f);
        }
    }

    public void PushReceivedFrame(sbyte[] frame)
    {
        if (frame.Length == 0)
        {
            return;
        }

        lock (_stateLock)
        {
            if (!_started || _playbackBuffer is null)
            {
                return;
            }

            // Si se ha acumulado demasiado audio, vaciamos para no escuchar voz atrasada.
            if (_playbackBuffer.BufferedDuration > TimeSpan.FromMilliseconds(900))
            {
                _playbackBuffer.ClearBuffer();
            }

            var pcm16 = new byte[frame.Length * 2];
            float gain = _playbackGain;

            for (int i = 0; i < frame.Length; i++)
            {
                int sample = (int)MathF.Round(frame[i] * 256.0f * gain);
                sample = Math.Clamp(sample, short.MinValue, short.MaxValue);

                short sample16 = (short)sample;
                pcm16[i * 2] = (byte)(sample16 & 0xFF);
                pcm16[i * 2 + 1] = (byte)((sample16 >> 8) & 0xFF);
            }

            _playbackBuffer.AddSamples(pcm16, 0, pcm16.Length);
        }
    }

    public void ClearPlaybackBuffer()
    {
        lock (_stateLock)
        {
            _playbackBuffer?.ClearBuffer();
        }
    }

    private void Capture_DataAvailable(object? sender, WaveInEventArgs e)
    {
        List<sbyte[]>? completedFrames = null;

        lock (_captureLock)
        {
            if (!_pttActive)
            {
                _microphoneFrameIndex = 0;
                return;
            }

            float gain = _microphoneGain;

            // El formato solicitado a NAudio es signed 16-bit, mono, little-endian.
            for (int i = 0; i + 1 < e.BytesRecorded; i += 2)
            {
                short inputSample = (short)(e.Buffer[i] | (e.Buffer[i + 1] << 8));
                short filteredSample = _microphoneFilter.Process(inputSample);

                int amplified = (int)MathF.Round(filteredSample * gain);
                amplified = Math.Clamp(amplified, short.MinValue, short.MaxValue);

                _microphoneFrame[_microphoneFrameIndex] = (sbyte)(amplified >> 8);
                _microphoneFrameIndex++;

                if (_microphoneFrameIndex >= MicrophoneFrameSamples)
                {
                    var frame = new sbyte[MicrophoneFrameSamples];
                    Array.Copy(_microphoneFrame, frame, MicrophoneFrameSamples);

                    completedFrames ??= new List<sbyte[]>();
                    completedFrames.Add(frame);
                    _microphoneFrameIndex = 0;
                }
            }
        }

        if (completedFrames is null)
        {
            return;
        }

        foreach (sbyte[] frame in completedFrames)
        {
            MicrophoneFrameReady?.Invoke(this, frame);
        }
    }

    private void Capture_RecordingStopped(object? sender, StoppedEventArgs e)
    {
        if (e.Exception is not null)
        {
            Error?.Invoke(this, $"El micrófono se detuvo: {e.Exception.Message}");
        }
    }

    private void ResetMicrophoneState()
    {
        lock (_captureLock)
        {
            _microphoneFrameIndex = 0;
            _microphoneFilter.Reset();
        }
    }

    public void Dispose()
    {
        Stop();
    }

    /// <summary>
    /// Filtro trasladado del programa C++: pasa-altos, pasa-bajos y noise gate.
    /// </summary>
    private sealed class MicrophoneFilter
    {
        private const int NoiseGateOpenThreshold = 800;
        private const int NoiseGateCloseThreshold = 400;
        private const int NoiseGateHoldSamples = 640;

        private const int HighPassRQ8 = 250;
        private const int LowPassAlphaQ8 = 170;
        private const int GateAttackQ8 = 24;
        private const int GateReleaseQ8 = 6;

        private int _highPassPreviousInput;
        private int _highPassPreviousOutput;
        private int _lowPassOutput;
        private int _gateHold;
        private int _gateLevelQ8;

        public short Process(short sample)
        {
            int input = sample;

            int highPass = input - _highPassPreviousInput
                           + ((_highPassPreviousOutput * HighPassRQ8) / 256);

            _highPassPreviousInput = input;
            _highPassPreviousOutput = highPass;
            highPass = Math.Clamp(highPass, short.MinValue, short.MaxValue);

            _lowPassOutput += ((highPass - _lowPassOutput) * LowPassAlphaQ8) / 256;
            _lowPassOutput = Math.Clamp(_lowPassOutput, short.MinValue, short.MaxValue);

            int amplitude = Math.Abs(_lowPassOutput);

            if (amplitude >= NoiseGateOpenThreshold)
            {
                _gateHold = NoiseGateHoldSamples;
            }
            else if (_gateHold > 0)
            {
                _gateHold--;
            }

            if (_gateHold > 0 || amplitude >= NoiseGateOpenThreshold)
            {
                _gateLevelQ8 = Math.Min(256, _gateLevelQ8 + GateAttackQ8);
            }
            else if (amplitude <= NoiseGateCloseThreshold)
            {
                _gateLevelQ8 = Math.Max(0, _gateLevelQ8 - GateReleaseQ8);
            }
            else
            {
                _gateLevelQ8 = Math.Max(0, _gateLevelQ8 - GateReleaseQ8);
            }

            int output = (_lowPassOutput * _gateLevelQ8) / 256;

            // Elimina los últimos residuos digitales cuando la puerta está casi cerrada.
            if (_gateLevelQ8 == 0 || Math.Abs(output) < 16)
            {
                output = 0;
            }

            output = Math.Clamp(output, short.MinValue, short.MaxValue);
            return (short)output;
        }

        public void Reset()
        {
            _highPassPreviousInput = 0;
            _highPassPreviousOutput = 0;
            _lowPassOutput = 0;
            _gateHold = 0;
            _gateLevelQ8 = 0;
        }
    }
}
