using System.IO.Ports;

namespace App_radio;

public partial class RadioMain : Form
{
    private readonly RadioController _radioController = new();

    private bool _pilotConfirmed;
    private bool _disconnectingAfterFault;
    private bool _closing;
    private bool _closeReady;
    private bool _pttTransitionInProgress;

    public RadioMain()
    {
        InitializeComponent();
        ConfigureControls();
        SubscribeEvents();
        LoadPorts();
        ApplyVolumes();
        ShowDisconnectedState();
        AddLog("Aplicación iniciada.");
    }

    private void ConfigureControls()
    {
        PuertoBox.DropDownStyle = ComboBoxStyle.DropDownList;

        VolumenBoxUpdown.Minimum = 0;
        VolumenBoxUpdown.Maximum = 100;
        VolumenBoxUpdown.Increment = 1;

        VolumenPilotoUpdown.Minimum = 0;
        VolumenPilotoUpdown.Maximum = 100;
        VolumenPilotoUpdown.Increment = 1;

        PttButton.Enabled = false;
        PttButton.Cursor = Cursors.Hand;
    }

    private void SubscribeEvents()
    {
        ActualizarButton.Click += ActualizarButton_Click;
        ConectarButton.Click += ConectarButton_Click;

        PttButton.MouseDown += PttButton_MouseDown;
        PttButton.MouseUp += PttButton_MouseUp;
        PttButton.MouseCaptureChanged += PttButton_MouseCaptureChanged;

        VolumenBoxUpdown.ValueChanged += VolumeControl_Changed;
        VolumenPilotoUpdown.ValueChanged += VolumeControl_Changed;
        SilenciarBoxCheck.CheckedChanged += VolumeControl_Changed;
        SilenciarPilotoCheck.CheckedChanged += VolumeControl_Changed;

        Deactivate += RadioMain_Deactivate;
        FormClosing += RadioMain_FormClosing;

        _radioController.Error += RadioController_Error;
        _radioController.PilotAudioReceived += RadioController_PilotAudioReceived;
        _radioController.ConnectionFaulted += RadioController_ConnectionFaulted;
    }

    private void LoadPorts()
    {
        string? previousSelection = PuertoBox.SelectedItem?.ToString();

        PuertoBox.Items.Clear();
        string[] availablePorts = SerialPort.GetPortNames();
        Array.Sort(availablePorts, ComparePortNames);
        PuertoBox.Items.AddRange(availablePorts);

        if (previousSelection is not null && PuertoBox.Items.Contains(previousSelection))
        {
            PuertoBox.SelectedItem = previousSelection;
        }
        else if (PuertoBox.Items.Count > 0)
        {
            PuertoBox.SelectedIndex = 0;
        }

        if (PuertoBox.Items.Count == 0)
        {
            AddLog("No se ha encontrado ningún puerto COM.");
        }
    }

    private static int ComparePortNames(string? left, string? right)
    {
        int leftNumber = ExtractPortNumber(left);
        int rightNumber = ExtractPortNumber(right);

        int numberComparison = leftNumber.CompareTo(rightNumber);
        return numberComparison != 0
            ? numberComparison
            : string.Compare(left, right, StringComparison.OrdinalIgnoreCase);
    }

    private static int ExtractPortNumber(string? portName)
    {
        if (portName is not null
            && portName.StartsWith("COM", StringComparison.OrdinalIgnoreCase)
            && int.TryParse(portName.AsSpan(3), out int number))
        {
            return number;
        }

        return int.MaxValue;
    }

    private void ActualizarButton_Click(object? sender, EventArgs e)
    {
        LoadPorts();
        AddLog($"Puertos actualizados: {PuertoBox.Items.Count} encontrado(s).");
    }

    private async void ConectarButton_Click(object? sender, EventArgs e)
    {
        if (_radioController.IsConnected)
        {
            await DisconnectRadioAsync("Desconectado por el usuario.");
            return;
        }

        string? selectedPort = PuertoBox.SelectedItem?.ToString();
        if (string.IsNullOrWhiteSpace(selectedPort))
        {
            MessageBox.Show(
                this,
                "Selecciona un puerto COM antes de conectar.",
                "Puerto no seleccionado",
                MessageBoxButtons.OK,
                MessageBoxIcon.Warning);
            return;
        }

        SetConnectionControlsEnabled(false);
        SetStatus(ConectadoBoxLabel, "● Conectando...", Color.DarkOrange);
        SetGeneralRadioStatus("● Conectando...", Color.DarkOrange);
        AddLog($"Abriendo {selectedPort} a 115200 baudios...");

        try
        {
            await Task.Run(() => _radioController.Connect(selectedPort));

            _pilotConfirmed = false;
            SetStatus(ConectadoBoxLabel, "● Conectado", Color.Green);
            SetStatus(ConectadoPilotoLabel, "● Sin confirmar", Color.Gray);
            SetGeneralRadioStatus("● Escuchando", Color.Green);

            ConectarButton.Text = "Desconectar";
            PttButton.Enabled = true;
            PuertoBox.Enabled = false;
            ActualizarButton.Enabled = false;
            ConectarButton.Enabled = true;

            ApplyVolumes();
            AddLog($"Radio del box conectada en {selectedPort}.");
            AddLog("Mantén pulsado el botón PTT para hablar.");
        }
        catch (Exception ex)
        {
            ShowDisconnectedState();
            AddLog($"No se pudo conectar: {ex.Message}");

            MessageBox.Show(
                this,
                $"No se pudo iniciar la radio.\n\n{ex.Message}",
                "Error de conexión",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }

    private void PttButton_MouseDown(object? sender, MouseEventArgs e)
    {
        if (e.Button != MouseButtons.Left || !_radioController.IsConnected)
        {
            return;
        }

        PttButton.Capture = true;

        if (_radioController.SetPtt(true))
        {
            PttButton.Text = "HABLANDO...\nEscucha simultánea activa";
            PttButton.UseVisualStyleBackColor = false;
            PttButton.BackColor = Color.IndianRed;
            SetGeneralRadioStatus("● Transmitiendo y escuchando", Color.Red);
            AddLog("PTT activado: transmitiendo y manteniendo activa la escucha.");
        }
    }

    private void PttButton_MouseUp(object? sender, MouseEventArgs e)
    {
        if (e.Button == MouseButtons.Left)
        {
            StopPtt();
        }
    }

    private void PttButton_MouseCaptureChanged(object? sender, EventArgs e)
    {
        if (!PttButton.Capture && _radioController.IsPttActive)
        {
            StopPtt();
        }
    }

    private void RadioMain_Deactivate(object? sender, EventArgs e)
    {
        StopPtt();
    }

    private void StopPtt()
    {
        if (_pttTransitionInProgress)
        {
            return;
        }

        _pttTransitionInProgress = true;

        try
        {
            bool wasActive = _radioController.IsPttActive;

            // El controlador corta la transmisión de forma inmediata y el
            // servicio serie deja de enviar al terminar como máximo el fragmento
            // actual de 200 bytes.
            _radioController.SetPtt(false);

            PttButton.Capture = false;
            PttButton.Text = "Pulsar para hablar";
            PttButton.UseVisualStyleBackColor = true;
            PttButton.BackColor = SystemColors.Control;

            if (_radioController.IsConnected)
            {
                SetGeneralRadioStatus("● Escuchando", Color.Green);
            }
            else
            {
                SetGeneralRadioStatus("● Desconectada", Color.Gray);
            }

            // Fuerza el repintado antes de añadir el registro, para que el botón
            // nunca parezca quedarse pulsado mientras se actualiza el RichTextBox.
            PttButton.Invalidate();
            PttButton.Update();

            if (wasActive)
            {
                AddLog("PTT desactivado: escuchando la radio del piloto.");
            }
        }
        finally
        {
            _pttTransitionInProgress = false;
        }
    }

    private void VolumeControl_Changed(object? sender, EventArgs e)
    {
        ApplyVolumes();
    }

    private void ApplyVolumes()
    {
        int boxPercent = Decimal.ToInt32(VolumenBoxUpdown.Value);
        int pilotPercent = Decimal.ToInt32(VolumenPilotoUpdown.Value);

        _radioController.SetBoxVolume(boxPercent, SilenciarBoxCheck.Checked);
        _radioController.SetPilotVolume(pilotPercent, SilenciarPilotoCheck.Checked);
    }

    private void RadioController_PilotAudioReceived(object? sender, EventArgs e)
    {
        RunOnUiThread(() =>
        {
            if (_pilotConfirmed)
            {
                return;
            }

            _pilotConfirmed = true;
            SetStatus(ConectadoPilotoLabel, "● Conectado", Color.Green);
            AddLog("Se ha recibido un frame de audio válido de la radio del piloto.");
        });
    }

    private void RadioController_Error(object? sender, string message)
    {
        AddLog(message);
    }

    private void RadioController_ConnectionFaulted(object? sender, EventArgs e)
    {
        RunOnUiThread(async () =>
        {
            if (_disconnectingAfterFault || !_radioController.IsConnected)
            {
                return;
            }

            _disconnectingAfterFault = true;
            try
            {
                await DisconnectRadioAsync("La conexión se ha cerrado por un error.");
            }
            finally
            {
                _disconnectingAfterFault = false;
            }
        });
    }

    private async Task DisconnectRadioAsync(string logMessage)
    {
        StopPtt();
        SetConnectionControlsEnabled(false);

        try
        {
            await _radioController.DisconnectAsync();
        }
        catch (Exception ex)
        {
            AddLog($"Error al cerrar la conexión: {ex.Message}");
        }
        finally
        {
            ShowDisconnectedState();
            AddLog(logMessage);
        }
    }

    private void ShowDisconnectedState()
    {
        _pilotConfirmed = false;
        SetStatus(ConectadoBoxLabel, "● Desconectado", Color.Gray);
        SetStatus(ConectadoPilotoLabel, "● Desconectado", Color.Gray);
        SetGeneralRadioStatus("● Desconectada", Color.Gray);

        ConectarButton.Text = "Conectar";
        ConectarButton.Enabled = true;
        PuertoBox.Enabled = true;
        ActualizarButton.Enabled = true;
        PttButton.Enabled = false;
        PttButton.Text = "Pulsar para hablar";
        PttButton.UseVisualStyleBackColor = true;
        PttButton.BackColor = SystemColors.Control;
    }

    private void SetConnectionControlsEnabled(bool enabled)
    {
        ConectarButton.Enabled = enabled;
        PuertoBox.Enabled = enabled;
        ActualizarButton.Enabled = enabled;
        PttButton.Enabled = enabled && _radioController.IsConnected;
    }

    private static void SetStatus(Label label, string text, Color color)
    {
        label.Text = text;
        label.ForeColor = color;
    }

    private void SetGeneralRadioStatus(string text, Color color)
    {
        label2.Text = text;
        label2.ForeColor = color;
    }

    private void AddLog(string message)
    {
        RunOnUiThread(() =>
        {
            if (RegistrosTextBox.TextLength > 100_000)
            {
                RegistrosTextBox.Select(0, 20_000);
                RegistrosTextBox.SelectedText = string.Empty;
            }

            RegistrosTextBox.AppendText(
                $"[{DateTime.Now:HH:mm:ss}] {message}{Environment.NewLine}");
            RegistrosTextBox.SelectionStart = RegistrosTextBox.TextLength;
            RegistrosTextBox.ScrollToCaret();
        });
    }

    private void RunOnUiThread(Action action)
    {
        if (IsDisposed || Disposing)
        {
            return;
        }

        if (InvokeRequired)
        {
            try
            {
                BeginInvoke(action);
            }
            catch (InvalidOperationException)
            {
                // La ventana se está cerrando.
            }
        }
        else
        {
            action();
        }
    }

    private async void RadioMain_FormClosing(object? sender, FormClosingEventArgs e)
    {
        if (_closeReady)
        {
            return;
        }

        e.Cancel = true;

        if (_closing)
        {
            return;
        }

        _closing = true;

        // La ventana desaparece al instante. La limpieza del audio y del puerto
        // se realiza después, con un límite de tiempo para que la X nunca deje la
        // aplicación bloqueada en pantalla.
        StopPtt();
        Enabled = false;
        Hide();

        Task disconnectTask = Task.Run(async () => await _radioController.DisconnectAsync());

        try
        {
            await disconnectTask.WaitAsync(TimeSpan.FromSeconds(2));
        }
        catch (TimeoutException)
        {
            // Cerramos igualmente la interfaz. La tarea de limpieza podrá acabar
            // en segundo plano sin impedir que termine la aplicación.
        }
        catch (Exception)
        {
            // Durante el cierre no mostramos cuadros de error que puedan volver a
            // bloquear la salida de la aplicación.
        }
        _closeReady = true;
        Close();
    }
}
