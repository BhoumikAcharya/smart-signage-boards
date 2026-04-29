using CompanyUtilityApp.ProgramFiles;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO.Ports;
using System.Text;
using System.Text.Json;
using System.Windows.Forms;
using System.Linq;
//using System.IO.Ports;


namespace CompanyUtilityApp.UserControls
{

    public partial class PanelSettingsControl : UserControl
    {
        private List<PanelSettingsModel> _assignedNodes = new List<PanelSettingsModel>();
        private bool _updating = false; // prevent recursive dropdown events
        private SerialPort _serialPort = null;                     // shared serial port


        public PanelSettingsControl()
        {
            InitializeComponent();
            PopulateRouteDropdown();
            LoadComPorts();
            btnRefreshPorts.Enabled = true;

        }

        // ===================== Initialisation =====================

        private void PopulateRouteDropdown()
        {
            cmbRoute.Items.AddRange(new object[] { 1, 2, 3, 4 });
            cmbRoute.SelectedIndex = 0;
        }

        private void LoadComPorts()
        {
            string previousSelection = cmbSerialPort.Text;
            cmbSerialPort.Items.Clear();
            cmbSerialPort.Items.AddRange(SerialPort.GetPortNames());
            if (cmbSerialPort.Items.Count > 0)
                {
                    // Try to reselect the previously selected port, if it still exists
                    if (!string.IsNullOrEmpty(previousSelection) && cmbSerialPort.Items.Contains(previousSelection))
                        cmbSerialPort.SelectedItem = previousSelection;
                    else
                        cmbSerialPort.SelectedIndex = 0;
                }
        }

        // ===================== Route & Panel Logic =====================

        private void cmbRoute_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_updating) return;
            int route = (int)cmbRoute.SelectedItem;
            LoadAssignedNodes(route);
        }

        private void LoadAssignedNodes(int route)
        {
            _assignedNodes = NodeRepository.GetAssignedNodesForRoute(route);
            // Populate Panel Location dropdown
            cmbPanelLocation.DataSource = null;
            cmbPanelLocation.DataSource = _assignedNodes;
            cmbPanelLocation.DisplayMember = "PanelLocation"; // shows ToString()
            // Populate IP Address dropdown
            cmbIPAddress.DataSource = null;
            cmbIPAddress.DataSource = _assignedNodes.Select(n => n.IPAddress).ToList();

            if (_assignedNodes.Count == 0)
            {
                cmbPanelLocation.Text = "";
                cmbIPAddress.Text = "";
                txtDescription.Text = "No assigned nodes on this route.";
                return;
            }
            cmbPanelLocation.SelectedIndex = 0; // triggers cmbPanelLocation_SelectedIndexChanged
        }

        private void cmbPanelLocation_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_updating) return;
            if (cmbPanelLocation.SelectedItem == null) return;

            var selected = (PanelSettingsModel)cmbPanelLocation.SelectedItem;
            UpdateIPFromPanelLocation(selected);
        }

        private void cmbIPAddress_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (_updating) return;
            if (cmbIPAddress.SelectedItem == null) return;

            string ip = cmbIPAddress.SelectedItem.ToString();
            UpdatePanelLocationFromIP(ip);
        }

        private void UpdatePanelLocationFromIP(string ip)
        {
            var match = _assignedNodes.FirstOrDefault(n => n.IPAddress == ip);
            if (match != null)
            {
                _updating = true;
                cmbPanelLocation.SelectedItem = match;
                txtDescription.Text = match.Description;
                _updating = false;
            }
        }

        private void UpdateIPFromPanelLocation(PanelSettingsModel selected)
        {
            _updating = true;
            cmbIPAddress.Text = selected.IPAddress;
            txtDescription.Text = selected.Description;
            _updating = false;
        }

        // ======================== ManualCalibration. AutoCalibration. [Under Development.]

        private void btnManualCalibrate_Click(object sender, EventArgs e)
        {
            if (cmbPanelLocation.SelectedItem == null)
            {
                MessageBox.Show("Please select a node first.", "No Node Selected", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            pnlManual.Visible = !pnlManual.Visible;
            if (pnlManual.Visible) LoadComPorts();
        }

        private void btnAutoCalibrate_Click(object sender, EventArgs e)
        {
            MessageBox.Show("Auto calibration is under development.", "Coming Soon", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        //protected override void Dispose(bool disposing)
        //{
        //    if (disposing)
        //    {
        //        if (_serialPort != null && _serialPort.IsOpen)
        //        {
        //            _serialPort.Close();
        //            _serialPort.DataReceived -= SerialPort_DataReceived;
        //            _serialPort = null;
        //        }
        //        components?.Dispose();
        //    }
        //    base.Dispose(disposing);
        //}

        // ========================

        private void btnDeploy_Click(object sender, EventArgs e)
        {
            if (cmbPanelLocation.SelectedItem == null || string.IsNullOrWhiteSpace(cmbIPAddress.Text))
            {
                MessageBox.Show("Select a valid node before deploying.", "Validation Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (cmbSerialPort.Text == "")
            {
                MessageBox.Show("Select a serial port.", "Serial Port", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (!decimal.TryParse(txtSens1.Text, out decimal sens1) || !decimal.TryParse(txtSens2.Text, out decimal sens2) || !decimal.TryParse(txtCalib.Text, out decimal calib))
            {
                MessageBox.Show("Please enter valid numeric calibration values.", "Validation Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            // Prepare payload
            var selectedNode = (PanelSettingsModel)cmbPanelLocation.SelectedItem;
            var payload = new
            {
                Route = selectedNode.Route,
                PanelLocation = selectedNode.PanelLocation,
                IPAddress = selectedNode.IPAddress,
                Description = selectedNode.Description,
                SENSITIVITY_1 = sens1,
                SENSITIVITY_2 = sens2,
                Battery_Calibration = calib,
                //Restart = true           // ← add this

            };
            string json = JsonSerializer.Serialize(payload);



            // Send via serial
            try
            {
                using (SerialPort port = new SerialPort(cmbSerialPort.Text, 115200))
                {
                    port.Open();
                    port.WriteLine(json);           // send JSON line to ESP32
                    MessageBox.Show("Configuration sent successfully:\n" + json, "Deploy", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Serial communication error: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void PanelSettings_Load(object sender, EventArgs e)
        {

        }

        private void panel2_Paint(object sender, PaintEventArgs e)
        {

        }

        // ===================== Connection Buttons =====================
        private void btnConnect_Click(object sender, EventArgs e)
        {
            if (_serialPort != null && _serialPort.IsOpen)
                return; // already open

            string portName = cmbSerialPort.Text;
            if (string.IsNullOrWhiteSpace(portName))
            {
                MessageBox.Show("Select a COM port first.", "No Port", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            try
            {
                _serialPort = new SerialPort(portName, 115200);
                _serialPort.DataReceived += SerialPort_DataReceived;
                _serialPort.Open();
                btnConnect.Enabled = false;
                btnDisconnect.Enabled = true;
                cmbSerialPort.Enabled = false;
                btnRefreshPorts.Enabled = false; // lock port selection
                AppendToSerialOutput("Connected to " + portName + "\n");
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to open COM port: " + ex.Message, "Connection Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                _serialPort = null;
            }
        }

        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            if (_serialPort != null && _serialPort.IsOpen)
            {
                _serialPort.Close();
                _serialPort.DataReceived -= SerialPort_DataReceived;
                _serialPort = null;
            }
            btnConnect.Enabled = true;
            btnDisconnect.Enabled = false;
            cmbSerialPort.Enabled = true;
            btnRefreshPorts.Enabled = true;
            AppendToSerialOutput("Disconnected.\n");
        }

        // ===================== Data Received Handler =====================
        private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            // This event runs on a background thread – must use Invoke
            string data = _serialPort.ReadExisting();
            if (!string.IsNullOrEmpty(data))
            {
                // Update the text box on the UI thread
                this.Invoke((MethodInvoker)(() => AppendToSerialOutput(data)));
            }
        }

        private void AppendToSerialOutput(string text)
        {
            txtSerialOutput.AppendText(text);
            // Auto-scroll to end
            txtSerialOutput.SelectionStart = txtSerialOutput.TextLength;
            txtSerialOutput.ScrollToCaret();
        }

        private void btnRefreshPorts_Click(object sender, EventArgs e)
        {
            LoadComPorts();

        }

        // ===================== Clean up on control dispose =====================
        //protected override void Dispose(bool disposing)
        //{
        //    if (disposing)
        //    {
        //        if (_serialPort != null && _serialPort.IsOpen)
        //        {
        //            _serialPort.Close();
        //            _serialPort.DataReceived -= SerialPort_DataReceived;
        //            _serialPort = null;
        //        }
        //        components?.Dispose();
        //    }
        //    base.Dispose(disposing);
        //}
    }
}
