using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using CompanyUtilityApp.ProgramFiles;

namespace CompanyUtilityApp
{
    public partial class NodeControl : UserControl
    {
        public NodeControl()
        {
            InitializeComponent();
            cmbRoute.Items.AddRange(new object[] { 1, 2, 3, 4 });
            cmbRoute.SelectedIndex = 0;
            LoadData();
        }

        // Refresh the DataGridView
        private void LoadData()
        {
            int route = (int)cmbRoute.SelectedItem;
            //dgvNodes.DataSource = null;
            dgvNodes.DataSource = NodeRepository.GetAllNodesForRoute(route);

            if (dgvNodes.Columns.Count > 0)
            {
                dgvNodes.Columns["Id"].Visible = false;
                dgvNodes.Columns["Route"].HeaderText = "Route";
                dgvNodes.Columns["IPAddress"].HeaderText = "IP Address";
                dgvNodes.Columns["PanelLocation"].HeaderText = "Panel Location";
                dgvNodes.Columns["Description"].HeaderText = "Description";

                dgvNodes.Columns["Description"].Width = 300;
            }
        }

        private void cmbRoute_SelectedIndexChanged(object sender, EventArgs e)
        {
            LoadData();
        }

        private void btnAdd_Click(object sender, EventArgs e)
        {
            int selectedRoute = (int)cmbRoute.SelectedItem;
            using (var dialog = new AddEditNodeForm(selectedRoute))
            {
                //Debug.WriteLine("Flag1");
                if (dialog.ShowDialog() == DialogResult.OK && dialog.NodeData != null)
                {
                    //Debug.WriteLine("Flag2");
                    NodeRepository.AddNode(dialog.NodeData);
                    LoadData(); // Refresh grid
                }
            }
        }

        private void dgvNodes_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {

        }

        private void btnEdit_Click(object sender, EventArgs e)
        {
            if (dgvNodes.SelectedRows.Count == 0)
            {
                MessageBox.Show("Please select a node to edit.", "Info", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            Node selectedNode = (Node)dgvNodes.SelectedRows[0].DataBoundItem;
            using (var dialog = new AddEditNodeForm(selectedNode))
            {
                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    NodeRepository.UpdateNode(dialog.NodeData);
                    LoadData(); // Refresh grid
                }
            }
        }

        private void btnDelete_Click(object sender, EventArgs e)
        {
            if (dgvNodes.SelectedRows.Count == 0)
            {
                MessageBox.Show("Please select a node to delete.", "Info", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            Node selectedNode = (Node)dgvNodes.SelectedRows[0].DataBoundItem;
            var result = MessageBox.Show($"Delete node with IP {selectedNode.IPAddress}?",
                "Confirm Delete", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (result == DialogResult.Yes)
            {
                NodeRepository.DeleteNode(selectedNode.Id);
                LoadData(); // Refresh grid
            }
        }
    }
}
