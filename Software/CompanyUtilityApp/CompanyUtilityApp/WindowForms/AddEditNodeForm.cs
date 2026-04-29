using CompanyUtilityApp.ProgramFiles;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace CompanyUtilityApp
{
    public partial class AddEditNodeForm : Form
    {
        public Node NodeData { get; private set; }

        public AddEditNodeForm(int selectedRoute)
        {
            InitializeComponent();
            txtRoute.Text = selectedRoute.ToString();
            PopulatePanelLocationComboBox();
            NodeData = new Node { Route = selectedRoute };
            this.Text = "Add New Node";
        }

        public AddEditNodeForm(Node existingNode)
        {
            InitializeComponent();
            PopulatePanelLocationComboBox();
            txtRoute.Text = existingNode.Route.ToString();
            txtIPAddress.Text = existingNode.IPAddress;
            cmbPanelLocation.SelectedItem = existingNode.PanelLocation;
            txtDescription.Text = existingNode.Description;
            NodeData = existingNode;
            this.Text = "Edit Node";
        }

        private void PopulatePanelLocationComboBox()
        {
            for (int i = 1; i <= 100; i++)
                cmbPanelLocation.Items.Add(i);
            cmbPanelLocation.SelectedIndex = 0;
        }

        private bool IsValidIpAddress(string ipString)
        {
            if (string.IsNullOrWhiteSpace(ipString))
                return false;

            // Use .NET's built-in IP address parser
            if (System.Net.IPAddress.TryParse(ipString, out System.Net.IPAddress address))
            {
                // Ensure it's IPv4 (not IPv6) and has exactly 4 parts
                return address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork;
            }
            return false;
        }

        private void btnSave_Click(object sender, EventArgs e)
        {
            // 1. Required IP address
            if (string.IsNullOrWhiteSpace(txtIPAddress.Text))
            {
                MessageBox.Show("IP Address is required.");
                txtIPAddress.Focus();
                return;  // <-- STOP HERE
            }

            // 2. Format validation
            if (!IsValidIpAddress(txtIPAddress.Text.Trim()))
            {
                MessageBox.Show("Please enter a valid IPv4 address...");
                txtIPAddress.Focus();
                return;
            }

            // 3. Panel location validation (typed or selected)
            int panelLocation;
            if (cmbPanelLocation.SelectedItem != null)
                panelLocation = (int)cmbPanelLocation.SelectedItem;
            else if (!int.TryParse(cmbPanelLocation.Text.Trim(), out panelLocation) || panelLocation < 1 || panelLocation > 100)
            {
                MessageBox.Show("Panel Location must be between 1 and 100.");
                cmbPanelLocation.Focus();
                return;
            }

            // 4. Route
            if (!int.TryParse(txtRoute.Text, out int route))
            {
                MessageBox.Show("Route is invalid.");
                return;
            }

            // Build NodeData
            NodeData.Route = route;
            NodeData.IPAddress = txtIPAddress.Text.Trim();
            NodeData.PanelLocation = panelLocation;
            NodeData.Description = txtDescription.Text.Trim();

            // 5. Duplicate IP check
            int? excludeId = (NodeData.Id > 0) ? NodeData.Id : (int?)null;
            if (NodeRepository.IpAddressExists(NodeData.IPAddress, excludeId))
            {
                MessageBox.Show($"The IP Address '{NodeData.IPAddress}' is already assigned...");
                txtIPAddress.Focus();
                return;
            }

            // 6. Duplicate Panel per Route check
            if (NodeRepository.PanelLocationExistsForRoute(NodeData.Route, NodeData.PanelLocation, excludeId))
            {
                MessageBox.Show($"Panel Location '{NodeData.PanelLocation}' is already used on Route {NodeData.Route}.");
                cmbPanelLocation.Focus();
                return;
            }

            // If we reach here, everything is valid.
            this.DialogResult = DialogResult.OK;
            this.Close();
        }

        private void txtRoute_TextChanged(object sender, EventArgs e)
        {

        }
    }
}
