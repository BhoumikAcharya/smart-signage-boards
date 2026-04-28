using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace CompanyUtilityApp
{
    public partial class AreaControl : UserControl
    {
        public AreaControl()
        {
            InitializeComponent();
            cmbRoute.Items.AddRange(new object[] { 1, 2, 3, 4 });
            cmbRoute.SelectedIndex = 0;
            LoadAreas();
        }

        private void LoadAreas()
        {
            int route = (int)cmbRoute.SelectedItem;
            dgvAreas.DataSource = AreaRepository.GetAreasByRoute(route);

            // Configure columns
            if (dgvAreas.Columns.Count > 0)
            {
                dgvAreas.Columns["Route"].Visible = false;               // hide route (already selected)
                dgvAreas.Columns["PanelLocation"].HeaderText = "Panel Location";
                dgvAreas.Columns["IPAddress"].HeaderText = "IP Address";
                dgvAreas.Columns["Description"].HeaderText = "Panel Area Location Description";

                // Make the description column wider (e.g., 300 pixels)
                dgvAreas.Columns["Description"].Width = 300;
            }
        }

        private void cmbRoute_SelectedIndexChanged(object sender, EventArgs e)
        {
            LoadAreas();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            if (dgvAreas.SelectedRows.Count == 0) return;

            AreaDisplayItem selected = dgvAreas.SelectedRows[0].DataBoundItem as AreaDisplayItem;
            if (selected == null) return;

            using (var dialog = new EditAreaForm(selected.Route, selected.PanelLocation, selected.Description))
            {
                if (dialog.ShowDialog() == DialogResult.OK)
                {
                    AreaRepository.UpdateDescription(selected.Route, selected.PanelLocation, dialog.Description);
                    LoadAreas(); // refresh grid to show updated description
                }
            }
        }

        //private void cmbRoute_KeyDown(object sender, KeyEventArgs e)
        //{
        //    if (e.KeyCode == Keys.Enter)
        //    {
        //        e.SuppressKeyPress = true; // prevent ding sound
        //        string typedText = cmbRoute.Text.Trim();
        //        if (int.TryParse(typedText, out int route) && route >= 1 && route <= 4)
        //        {
        //            cmbRoute.SelectedItem = route; // This will trigger SelectedIndexChanged
        //        }
        //        else
        //        {
        //            MessageBox.Show("Please enter a valid route (1‑4).", "Invalid Route", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        //            cmbRoute.Focus();
        //            cmbRoute.SelectAll();
        //        }
        //    }
        //}
    }
}
