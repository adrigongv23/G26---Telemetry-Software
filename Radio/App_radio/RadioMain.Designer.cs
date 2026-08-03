namespace App_radio
{
    partial class RadioMain
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            PuertoLabel = new Label();
            PuertoBox = new ComboBox();
            ActualizarButton = new Button();
            ConectarButton = new Button();
            PuertosTab = new TabControl();
            ComunicacionTab = new TabPage();
            label2 = new Label();
            EstadoRadioLabel = new Label();
            PttButton = new Button();
            tabPage1 = new TabPage();
            ConectadoPilotoLabel = new Label();
            ConectadoBoxLabel = new Label();
            EstadoPilotoLabel = new Label();
            EstadoBoxLabel = new Label();
            VolumenTab = new TabPage();
            SilenciarPilotoCheck = new CheckBox();
            SilenciarBoxCheck = new CheckBox();
            VolumenPilotoUpdown = new NumericUpDown();
            VolumenBoxUpdown = new NumericUpDown();
            VolumenPilotoLabel = new Label();
            VolumenBoxLabel = new Label();
            RegistrosTab = new TabPage();
            RegistrosTextBox = new RichTextBox();
            PuertosTab.SuspendLayout();
            ComunicacionTab.SuspendLayout();
            tabPage1.SuspendLayout();
            VolumenTab.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)VolumenPilotoUpdown).BeginInit();
            ((System.ComponentModel.ISupportInitialize)VolumenBoxUpdown).BeginInit();
            RegistrosTab.SuspendLayout();
            SuspendLayout();
            // 
            // PuertoLabel
            // 
            PuertoLabel.AutoSize = true;
            PuertoLabel.Location = new Point(17, 46);
            PuertoLabel.Name = "PuertoLabel";
            PuertoLabel.Size = new Size(98, 15);
            PuertoLabel.TabIndex = 0;
            PuertoLabel.Text = "Selección puerto:";
            // 
            // PuertoBox
            // 
            PuertoBox.FormattingEnabled = true;
            PuertoBox.Location = new Point(143, 43);
            PuertoBox.Name = "PuertoBox";
            PuertoBox.Size = new Size(121, 23);
            PuertoBox.TabIndex = 1;
            // 
            // ActualizarButton
            // 
            ActualizarButton.Location = new Point(298, 42);
            ActualizarButton.Name = "ActualizarButton";
            ActualizarButton.Size = new Size(75, 23);
            ActualizarButton.TabIndex = 2;
            ActualizarButton.Text = "Actualizar";
            ActualizarButton.UseVisualStyleBackColor = true;
            // 
            // ConectarButton
            // 
            ConectarButton.Location = new Point(389, 42);
            ConectarButton.Name = "ConectarButton";
            ConectarButton.Size = new Size(75, 23);
            ConectarButton.TabIndex = 3;
            ConectarButton.Text = "Conectar\r\n";
            ConectarButton.UseVisualStyleBackColor = true;
            // 
            // PuertosTab
            // 
            PuertosTab.Controls.Add(ComunicacionTab);
            PuertosTab.Controls.Add(tabPage1);
            PuertosTab.Controls.Add(VolumenTab);
            PuertosTab.Controls.Add(RegistrosTab);
            PuertosTab.Dock = DockStyle.Fill;
            PuertosTab.Location = new Point(0, 0);
            PuertosTab.Name = "PuertosTab";
            PuertosTab.SelectedIndex = 0;
            PuertosTab.Size = new Size(500, 248);
            PuertosTab.TabIndex = 4;
            // 
            // ComunicacionTab
            // 
            ComunicacionTab.Controls.Add(label2);
            ComunicacionTab.Controls.Add(EstadoRadioLabel);
            ComunicacionTab.Controls.Add(PttButton);
            ComunicacionTab.Location = new Point(4, 24);
            ComunicacionTab.Name = "ComunicacionTab";
            ComunicacionTab.Padding = new Padding(3);
            ComunicacionTab.Size = new Size(492, 220);
            ComunicacionTab.TabIndex = 3;
            ComunicacionTab.Text = "Comunicacion";
            ComunicacionTab.UseVisualStyleBackColor = true;
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.ForeColor = Color.Gray;
            label2.Location = new Point(248, 174);
            label2.Name = "label2";
            label2.Size = new Size(87, 15);
            label2.TabIndex = 2;
            label2.Text = "● Desconectada";
            // 
            // EstadoRadioLabel
            // 
            EstadoRadioLabel.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            EstadoRadioLabel.AutoSize = true;
            EstadoRadioLabel.Location = new Point(146, 174);
            EstadoRadioLabel.Name = "EstadoRadioLabel";
            EstadoRadioLabel.Size = new Size(75, 15);
            EstadoRadioLabel.TabIndex = 1;
            EstadoRadioLabel.Text = "Estado radio:";
            // 
            // PttButton
            // 
            PttButton.Font = new Font("Segoe UI", 12F, FontStyle.Regular, GraphicsUnit.Point, 0);
            PttButton.Location = new Point(98, 26);
            PttButton.Name = "PttButton";
            PttButton.Size = new Size(273, 126);
            PttButton.TabIndex = 0;
            PttButton.Text = "Pulsar para hablar";
            PttButton.UseVisualStyleBackColor = true;
            // 
            // tabPage1
            // 
            tabPage1.Controls.Add(ConectadoPilotoLabel);
            tabPage1.Controls.Add(ConectadoBoxLabel);
            tabPage1.Controls.Add(EstadoPilotoLabel);
            tabPage1.Controls.Add(EstadoBoxLabel);
            tabPage1.Controls.Add(PuertoLabel);
            tabPage1.Controls.Add(ConectarButton);
            tabPage1.Controls.Add(PuertoBox);
            tabPage1.Controls.Add(ActualizarButton);
            tabPage1.Location = new Point(4, 24);
            tabPage1.Name = "tabPage1";
            tabPage1.Padding = new Padding(3);
            tabPage1.Size = new Size(492, 220);
            tabPage1.TabIndex = 0;
            tabPage1.Text = "Puertos";
            tabPage1.UseVisualStyleBackColor = true;
            // 
            // ConectadoPilotoLabel
            // 
            ConectadoPilotoLabel.AutoSize = true;
            ConectadoPilotoLabel.ForeColor = Color.Gray;
            ConectadoPilotoLabel.Location = new Point(143, 146);
            ConectadoPilotoLabel.Name = "ConectadoPilotoLabel";
            ConectadoPilotoLabel.Size = new Size(75, 15);
            ConectadoPilotoLabel.TabIndex = 7;
            ConectadoPilotoLabel.Text = "● Desconectado";
            // 
            // ConectadoBoxLabel
            // 
            ConectadoBoxLabel.AutoSize = true;
            ConectadoBoxLabel.ForeColor = Color.Gray;
            ConectadoBoxLabel.Location = new Point(143, 111);
            ConectadoBoxLabel.Name = "ConectadoBoxLabel";
            ConectadoBoxLabel.Size = new Size(75, 15);
            ConectadoBoxLabel.TabIndex = 6;
            ConectadoBoxLabel.Text = "● Desconectado";
            // 
            // EstadoPilotoLabel
            // 
            EstadoPilotoLabel.AutoSize = true;
            EstadoPilotoLabel.Location = new Point(17, 146);
            EstadoPilotoLabel.Name = "EstadoPilotoLabel";
            EstadoPilotoLabel.Size = new Size(109, 15);
            EstadoPilotoLabel.TabIndex = 5;
            EstadoPilotoLabel.Text = "Estado radio piloto:";
            // 
            // EstadoBoxLabel
            // 
            EstadoBoxLabel.AutoSize = true;
            EstadoBoxLabel.Location = new Point(17, 111);
            EstadoBoxLabel.Name = "EstadoBoxLabel";
            EstadoBoxLabel.Size = new Size(97, 15);
            EstadoBoxLabel.TabIndex = 4;
            EstadoBoxLabel.Text = "Estado radio box:";
            // 
            // VolumenTab
            // 
            VolumenTab.Controls.Add(SilenciarPilotoCheck);
            VolumenTab.Controls.Add(SilenciarBoxCheck);
            VolumenTab.Controls.Add(VolumenPilotoUpdown);
            VolumenTab.Controls.Add(VolumenBoxUpdown);
            VolumenTab.Controls.Add(VolumenPilotoLabel);
            VolumenTab.Controls.Add(VolumenBoxLabel);
            VolumenTab.Location = new Point(4, 24);
            VolumenTab.Name = "VolumenTab";
            VolumenTab.Padding = new Padding(3);
            VolumenTab.Size = new Size(492, 220);
            VolumenTab.TabIndex = 1;
            VolumenTab.Text = "Volumen";
            VolumenTab.UseVisualStyleBackColor = true;
            // 
            // SilenciarPilotoCheck
            // 
            SilenciarPilotoCheck.AutoSize = true;
            SilenciarPilotoCheck.Location = new Point(309, 91);
            SilenciarPilotoCheck.Name = "SilenciarPilotoCheck";
            SilenciarPilotoCheck.Size = new Size(70, 19);
            SilenciarPilotoCheck.TabIndex = 5;
            SilenciarPilotoCheck.Text = "Silenciar";
            SilenciarPilotoCheck.UseVisualStyleBackColor = true;
            // 
            // SilenciarBoxCheck
            // 
            SilenciarBoxCheck.AutoSize = true;
            SilenciarBoxCheck.Location = new Point(309, 43);
            SilenciarBoxCheck.Name = "SilenciarBoxCheck";
            SilenciarBoxCheck.Size = new Size(70, 19);
            SilenciarBoxCheck.TabIndex = 4;
            SilenciarBoxCheck.Text = "Silenciar";
            SilenciarBoxCheck.UseVisualStyleBackColor = true;
            // 
            // VolumenPilotoUpdown
            // 
            VolumenPilotoUpdown.Location = new Point(155, 87);
            VolumenPilotoUpdown.Name = "VolumenPilotoUpdown";
            VolumenPilotoUpdown.Size = new Size(120, 23);
            VolumenPilotoUpdown.TabIndex = 3;
            VolumenPilotoUpdown.Value = new decimal(new int[] { 50, 0, 0, 0 });
            // 
            // VolumenBoxUpdown
            // 
            VolumenBoxUpdown.Location = new Point(155, 39);
            VolumenBoxUpdown.Name = "VolumenBoxUpdown";
            VolumenBoxUpdown.Size = new Size(120, 23);
            VolumenBoxUpdown.TabIndex = 2;
            VolumenBoxUpdown.Value = new decimal(new int[] { 50, 0, 0, 0 });
            // 
            // VolumenPilotoLabel
            // 
            VolumenPilotoLabel.AutoSize = true;
            VolumenPilotoLabel.Location = new Point(29, 89);
            VolumenPilotoLabel.Name = "VolumenPilotoLabel";
            VolumenPilotoLabel.Size = new Size(91, 15);
            VolumenPilotoLabel.TabIndex = 1;
            VolumenPilotoLabel.Text = "Volumen piloto:";
            // 
            // VolumenBoxLabel
            // 
            VolumenBoxLabel.AutoSize = true;
            VolumenBoxLabel.Location = new Point(29, 41);
            VolumenBoxLabel.Name = "VolumenBoxLabel";
            VolumenBoxLabel.Size = new Size(79, 15);
            VolumenBoxLabel.TabIndex = 0;
            VolumenBoxLabel.Text = "Volumen box:";
            // 
            // RegistrosTab
            // 
            RegistrosTab.Controls.Add(RegistrosTextBox);
            RegistrosTab.Location = new Point(4, 24);
            RegistrosTab.Name = "RegistrosTab";
            RegistrosTab.Padding = new Padding(3);
            RegistrosTab.Size = new Size(492, 220);
            RegistrosTab.TabIndex = 2;
            RegistrosTab.Text = "Registros";
            RegistrosTab.UseVisualStyleBackColor = true;
            // 
            // RegistrosTextBox
            // 
            RegistrosTextBox.Dock = DockStyle.Fill;
            RegistrosTextBox.Location = new Point(3, 3);
            RegistrosTextBox.Name = "RegistrosTextBox";
            RegistrosTextBox.ReadOnly = true;
            RegistrosTextBox.Size = new Size(486, 214);
            RegistrosTextBox.TabIndex = 0;
            RegistrosTextBox.Text = "";
            // 
            // RadioMain
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(500, 248);
            Controls.Add(PuertosTab);
            Name = "RadioMain";
            Text = "Radio - Formula Gades";
            PuertosTab.ResumeLayout(false);
            ComunicacionTab.ResumeLayout(false);
            ComunicacionTab.PerformLayout();
            tabPage1.ResumeLayout(false);
            tabPage1.PerformLayout();
            VolumenTab.ResumeLayout(false);
            VolumenTab.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)VolumenPilotoUpdown).EndInit();
            ((System.ComponentModel.ISupportInitialize)VolumenBoxUpdown).EndInit();
            RegistrosTab.ResumeLayout(false);
            ResumeLayout(false);
        }

        #endregion

        private Label PuertoLabel;
        private ComboBox PuertoBox;
        private Button ActualizarButton;
        private Button ConectarButton;
        private TabControl PuertosTab;
        private TabPage tabPage1;
        private TabPage VolumenTab;
        private Label VolumenBoxLabel;
        private Label VolumenPilotoLabel;
        private NumericUpDown VolumenPilotoUpdown;
        private NumericUpDown VolumenBoxUpdown;
        private CheckBox SilenciarBoxCheck;
        private CheckBox SilenciarPilotoCheck;
        private TabPage RegistrosTab;
        private Label EstadoPilotoLabel;
        private Label EstadoBoxLabel;
        private Label ConectadoPilotoLabel;
        private Label ConectadoBoxLabel;
        private TabPage ComunicacionTab;
        private Button PttButton;
        private Label label2;
        private Label EstadoRadioLabel;
        private RichTextBox RegistrosTextBox;
    }
}
