namespace CompanyUtilityApp.UserControls
{
    partial class PanelSettingsControl
    {
        /// <summary> 
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary> 
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                if (_serialPort != null && _serialPort.IsOpen)
                {
                    _serialPort.Close();
                    _serialPort.DataReceived -= SerialPort_DataReceived;
                    _serialPort = null;
                }
                if (components != null)
                {
                    components.Dispose();
                }
            }
            base.Dispose(disposing);
        }

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            cmbRoute = new ComboBox();
            label1 = new Label();
            panel1 = new Panel();
            btnManualCalibrate = new Button();
            btnAutoCalibrate = new Button();
            txtDescription = new TextBox();
            cmbIPAddress = new ComboBox();
            cmbPanelLocation = new ComboBox();
            label4 = new Label();
            label3 = new Label();
            label2 = new Label();
            pnlManual = new Panel();
            panel3 = new Panel();
            grpSerialMonitor = new GroupBox();
            panel5 = new Panel();
            txtSerialOutput = new TextBox();
            panel4 = new Panel();
            btnDisconnect = new Button();
            btnConnect = new Button();
            panel2 = new Panel();
            btnRefreshPorts = new Button();
            btnDeploy = new Button();
            cmbSerialPort = new ComboBox();
            txtSens1 = new TextBox();
            txtCalib = new TextBox();
            label8 = new Label();
            txtSens2 = new TextBox();
            label7 = new Label();
            label6 = new Label();
            label5 = new Label();
            panel1.SuspendLayout();
            pnlManual.SuspendLayout();
            panel3.SuspendLayout();
            grpSerialMonitor.SuspendLayout();
            panel5.SuspendLayout();
            panel4.SuspendLayout();
            panel2.SuspendLayout();
            SuspendLayout();
            // 
            // cmbRoute
            // 
            cmbRoute.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbRoute.FormattingEnabled = true;
            cmbRoute.Location = new Point(187, 44);
            cmbRoute.Name = "cmbRoute";
            cmbRoute.Size = new Size(151, 28);
            cmbRoute.TabIndex = 1;
            cmbRoute.SelectedIndexChanged += cmbRoute_SelectedIndexChanged;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(43, 44);
            label1.Name = "label1";
            label1.Size = new Size(51, 20);
            label1.TabIndex = 2;
            label1.Text = "Route:";
            // 
            // panel1
            // 
            panel1.Controls.Add(btnManualCalibrate);
            panel1.Controls.Add(btnAutoCalibrate);
            panel1.Controls.Add(txtDescription);
            panel1.Controls.Add(cmbIPAddress);
            panel1.Controls.Add(cmbPanelLocation);
            panel1.Controls.Add(label4);
            panel1.Controls.Add(label3);
            panel1.Controls.Add(label2);
            panel1.Controls.Add(label1);
            panel1.Controls.Add(cmbRoute);
            panel1.Dock = DockStyle.Top;
            panel1.Location = new Point(0, 0);
            panel1.Name = "panel1";
            panel1.Size = new Size(1025, 253);
            panel1.TabIndex = 3;
            // 
            // btnManualCalibrate
            // 
            btnManualCalibrate.Location = new Point(396, 181);
            btnManualCalibrate.Name = "btnManualCalibrate";
            btnManualCalibrate.Size = new Size(140, 29);
            btnManualCalibrate.TabIndex = 10;
            btnManualCalibrate.Text = "ManualCalibrate";
            btnManualCalibrate.UseVisualStyleBackColor = true;
            btnManualCalibrate.Click += btnManualCalibrate_Click;
            // 
            // btnAutoCalibrate
            // 
            btnAutoCalibrate.Enabled = false;
            btnAutoCalibrate.Location = new Point(178, 181);
            btnAutoCalibrate.Name = "btnAutoCalibrate";
            btnAutoCalibrate.Size = new Size(140, 29);
            btnAutoCalibrate.TabIndex = 9;
            btnAutoCalibrate.Text = "AutoCalibrate";
            btnAutoCalibrate.UseVisualStyleBackColor = true;
            // 
            // txtDescription
            // 
            txtDescription.Location = new Point(540, 87);
            txtDescription.Multiline = true;
            txtDescription.Name = "txtDescription";
            txtDescription.ReadOnly = true;
            txtDescription.Size = new Size(422, 57);
            txtDescription.TabIndex = 8;
            // 
            // cmbIPAddress
            // 
            cmbIPAddress.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbIPAddress.FormattingEnabled = true;
            cmbIPAddress.Location = new Point(540, 41);
            cmbIPAddress.Name = "cmbIPAddress";
            cmbIPAddress.Size = new Size(151, 28);
            cmbIPAddress.TabIndex = 7;
            cmbIPAddress.SelectedIndexChanged += cmbIPAddress_SelectedIndexChanged;
            // 
            // cmbPanelLocation
            // 
            cmbPanelLocation.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbPanelLocation.FormattingEnabled = true;
            cmbPanelLocation.Location = new Point(187, 86);
            cmbPanelLocation.Name = "cmbPanelLocation";
            cmbPanelLocation.Size = new Size(151, 28);
            cmbPanelLocation.TabIndex = 6;
            cmbPanelLocation.SelectedIndexChanged += cmbPanelLocation_SelectedIndexChanged;
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Location = new Point(396, 87);
            label4.Name = "label4";
            label4.Size = new Size(109, 40);
            label4.TabIndex = 5;
            label4.Text = "Panel Location \r\nDescription:";
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(396, 41);
            label3.Name = "label3";
            label3.Size = new Size(81, 20);
            label3.TabIndex = 4;
            label3.Text = "IP Address:";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(43, 86);
            label2.Name = "label2";
            label2.Size = new Size(108, 20);
            label2.TabIndex = 3;
            label2.Text = "Panel Location:";
            // 
            // pnlManual
            // 
            pnlManual.Controls.Add(panel3);
            pnlManual.Controls.Add(panel2);
            pnlManual.Dock = DockStyle.Fill;
            pnlManual.Location = new Point(0, 253);
            pnlManual.Name = "pnlManual";
            pnlManual.Size = new Size(1025, 510);
            pnlManual.TabIndex = 4;
            pnlManual.Visible = false;
            pnlManual.Paint += panel2_Paint;
            // 
            // panel3
            // 
            panel3.Controls.Add(grpSerialMonitor);
            panel3.Dock = DockStyle.Fill;
            panel3.Location = new Point(413, 0);
            panel3.Name = "panel3";
            panel3.Size = new Size(612, 510);
            panel3.TabIndex = 20;
            // 
            // grpSerialMonitor
            // 
            grpSerialMonitor.Controls.Add(panel5);
            grpSerialMonitor.Controls.Add(panel4);
            grpSerialMonitor.Dock = DockStyle.Fill;
            grpSerialMonitor.Location = new Point(0, 0);
            grpSerialMonitor.Name = "grpSerialMonitor";
            grpSerialMonitor.Size = new Size(612, 510);
            grpSerialMonitor.TabIndex = 0;
            grpSerialMonitor.TabStop = false;
            grpSerialMonitor.Text = "Serial Monitor";
            // 
            // panel5
            // 
            panel5.Controls.Add(txtSerialOutput);
            panel5.Dock = DockStyle.Fill;
            panel5.Location = new Point(3, 100);
            panel5.Name = "panel5";
            panel5.Size = new Size(606, 407);
            panel5.TabIndex = 4;
            // 
            // txtSerialOutput
            // 
            txtSerialOutput.Dock = DockStyle.Fill;
            txtSerialOutput.Location = new Point(0, 0);
            txtSerialOutput.Multiline = true;
            txtSerialOutput.Name = "txtSerialOutput";
            txtSerialOutput.ReadOnly = true;
            txtSerialOutput.ScrollBars = ScrollBars.Vertical;
            txtSerialOutput.Size = new Size(606, 407);
            txtSerialOutput.TabIndex = 2;
            // 
            // panel4
            // 
            panel4.Controls.Add(btnDisconnect);
            panel4.Controls.Add(btnConnect);
            panel4.Dock = DockStyle.Top;
            panel4.Location = new Point(3, 23);
            panel4.Name = "panel4";
            panel4.Size = new Size(606, 77);
            panel4.TabIndex = 3;
            // 
            // btnDisconnect
            // 
            btnDisconnect.Enabled = false;
            btnDisconnect.Location = new Point(140, 24);
            btnDisconnect.Name = "btnDisconnect";
            btnDisconnect.Size = new Size(94, 29);
            btnDisconnect.TabIndex = 1;
            btnDisconnect.Text = "Disconnect";
            btnDisconnect.UseVisualStyleBackColor = true;
            btnDisconnect.Click += btnDisconnect_Click;
            // 
            // btnConnect
            // 
            btnConnect.Location = new Point(14, 24);
            btnConnect.Name = "btnConnect";
            btnConnect.Size = new Size(94, 29);
            btnConnect.TabIndex = 0;
            btnConnect.Text = "Connect";
            btnConnect.UseVisualStyleBackColor = true;
            btnConnect.Click += btnConnect_Click;
            // 
            // panel2
            // 
            panel2.Controls.Add(btnRefreshPorts);
            panel2.Controls.Add(btnDeploy);
            panel2.Controls.Add(cmbSerialPort);
            panel2.Controls.Add(txtSens1);
            panel2.Controls.Add(txtCalib);
            panel2.Controls.Add(label8);
            panel2.Controls.Add(txtSens2);
            panel2.Controls.Add(label7);
            panel2.Controls.Add(label6);
            panel2.Controls.Add(label5);
            panel2.Dock = DockStyle.Left;
            panel2.Location = new Point(0, 0);
            panel2.Name = "panel2";
            panel2.Size = new Size(413, 510);
            panel2.TabIndex = 19;
            // 
            // btnRefreshPorts
            // 
            btnRefreshPorts.Location = new Point(336, 178);
            btnRefreshPorts.Name = "btnRefreshPorts";
            btnRefreshPorts.Size = new Size(28, 29);
            btnRefreshPorts.TabIndex = 19;
            btnRefreshPorts.Text = "⟳";
            btnRefreshPorts.UseVisualStyleBackColor = true;
            btnRefreshPorts.Click += btnRefreshPorts_Click;
            // 
            // btnDeploy
            // 
            btnDeploy.Location = new Point(115, 252);
            btnDeploy.Name = "btnDeploy";
            btnDeploy.Size = new Size(94, 29);
            btnDeploy.TabIndex = 14;
            btnDeploy.Text = "Deploy";
            btnDeploy.UseVisualStyleBackColor = true;
            btnDeploy.Click += btnDeploy_Click;
            // 
            // cmbSerialPort
            // 
            cmbSerialPort.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbSerialPort.FormattingEnabled = true;
            cmbSerialPort.Location = new Point(179, 179);
            cmbSerialPort.Name = "cmbSerialPort";
            cmbSerialPort.Size = new Size(151, 28);
            cmbSerialPort.TabIndex = 18;
            // 
            // txtSens1
            // 
            txtSens1.Location = new Point(205, 35);
            txtSens1.Name = "txtSens1";
            txtSens1.Size = new Size(125, 27);
            txtSens1.TabIndex = 15;
            // 
            // txtCalib
            // 
            txtCalib.Location = new Point(205, 134);
            txtCalib.Name = "txtCalib";
            txtCalib.Size = new Size(125, 27);
            txtCalib.TabIndex = 17;
            // 
            // label8
            // 
            label8.AutoSize = true;
            label8.Location = new Point(35, 38);
            label8.Name = "label8";
            label8.Size = new Size(161, 20);
            label8.TabIndex = 10;
            label8.Text = "ACS Sensitivity Value 1:";
            // 
            // txtSens2
            // 
            txtSens2.Location = new Point(205, 77);
            txtSens2.Name = "txtSens2";
            txtSens2.Size = new Size(125, 27);
            txtSens2.TabIndex = 16;
            // 
            // label7
            // 
            label7.AutoSize = true;
            label7.Location = new Point(35, 80);
            label7.Name = "label7";
            label7.Size = new Size(161, 20);
            label7.TabIndex = 11;
            label7.Text = "ACS Sensitivity Value 2:";
            // 
            // label6
            // 
            label6.AutoSize = true;
            label6.Location = new Point(61, 137);
            label6.Name = "label6";
            label6.Size = new Size(136, 20);
            label6.TabIndex = 12;
            label6.Text = "Battery Calibration:";
            // 
            // label5
            // 
            label5.AutoSize = true;
            label5.Location = new Point(61, 183);
            label5.Name = "label5";
            label5.Size = new Size(79, 20);
            label5.TabIndex = 13;
            label5.Text = "Serial Port:";
            // 
            // PanelSettingsControl
            // 
            AutoScaleDimensions = new SizeF(8F, 20F);
            AutoScaleMode = AutoScaleMode.Font;
            Controls.Add(pnlManual);
            Controls.Add(panel1);
            Name = "PanelSettingsControl";
            Size = new Size(1025, 763);
            Load += PanelSettings_Load;
            panel1.ResumeLayout(false);
            panel1.PerformLayout();
            pnlManual.ResumeLayout(false);
            panel3.ResumeLayout(false);
            grpSerialMonitor.ResumeLayout(false);
            panel5.ResumeLayout(false);
            panel5.PerformLayout();
            panel4.ResumeLayout(false);
            panel2.ResumeLayout(false);
            panel2.PerformLayout();
            ResumeLayout(false);
        }

        #endregion

        private ComboBox cmbRoute;
        private Label label1;
        private Panel panel1;
        private TextBox txtDescription;
        private ComboBox cmbIPAddress;
        private ComboBox cmbPanelLocation;
        private Label label4;
        private Label label3;
        private Label label2;
        private Button btnManualCalibrate;
        private Button btnAutoCalibrate;
        private Panel pnlManual;
        private TextBox txtCalib;
        private TextBox txtSens2;
        private TextBox txtSens1;
        private Button btnDeploy;
        private Label label5;
        private Label label6;
        private Label label7;
        private Label label8;
        private ComboBox cmbSerialPort;
        private Panel panel2;
        private Panel panel3;
        private GroupBox grpSerialMonitor;
        private TextBox txtSerialOutput;
        private Button btnDisconnect;
        private Button btnConnect;
        private Panel panel4;
        private Panel panel5;
        private Button btnRefreshPorts;
    }
}
