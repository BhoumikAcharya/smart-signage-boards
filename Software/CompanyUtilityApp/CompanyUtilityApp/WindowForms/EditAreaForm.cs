using System;
using System.Windows.Forms;

namespace CompanyUtilityApp
{
    public partial class EditAreaForm : Form
    {
        public string Description { get; private set; }

        public EditAreaForm(int route, int panelLocation, string currentDescription)
        {
            InitializeComponent();
            txtPanelLocation.Text = panelLocation.ToString();
            txtDescription.Text = currentDescription ?? string.Empty;
            this.Text = $"Edit Panel Description – Route {route}, Panel {panelLocation}";
        }

        private void btnSave_Click(object sender, EventArgs e)
        {
            string desc = txtDescription.Text.Trim();
            // Convert empty string to null so DB stores NULL
            Description = string.IsNullOrEmpty(desc) ? null : desc;
            this.DialogResult = DialogResult.OK;
            this.Close();
        }

        private void button2_Click(object sender, EventArgs e)
        {
            this.Close();
        }
    }
}