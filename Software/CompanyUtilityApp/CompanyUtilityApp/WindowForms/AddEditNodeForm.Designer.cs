namespace CompanyUtilityApp
{
    partial class AddEditNodeForm
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
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            label1 = new Label();
            label2 = new Label();
            label3 = new Label();
            txtRoute = new TextBox();
            txtIPAddress = new TextBox();
            cmbPanelLocation = new ComboBox();
            label4 = new Label();
            txtDescription = new TextBox();
            btnSave = new Button();
            btnCancel = new Button();
            SuspendLayout();
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(32, 45);
            label1.Name = "label1";
            label1.Size = new Size(55, 20);
            label1.TabIndex = 0;
            label1.Text = "Route: ";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new Point(178, 45);
            label2.Name = "label2";
            label2.Size = new Size(85, 20);
            label2.TabIndex = 1;
            label2.Text = "IP Address: ";
            // 
            // label3
            // 
            label3.AutoSize = true;
            label3.Location = new Point(341, 45);
            label3.Name = "label3";
            label3.Size = new Size(112, 20);
            label3.TabIndex = 2;
            label3.Text = "Panel Location: ";
            // 
            // txtRoute
            // 
            txtRoute.Enabled = false;
            txtRoute.Location = new Point(32, 68);
            txtRoute.Name = "txtRoute";
            txtRoute.Size = new Size(125, 27);
            txtRoute.TabIndex = 3;
            txtRoute.TextChanged += txtRoute_TextChanged;
            // 
            // txtIPAddress
            // 
            txtIPAddress.Location = new Point(178, 68);
            txtIPAddress.Name = "txtIPAddress";
            txtIPAddress.Size = new Size(125, 27);
            txtIPAddress.TabIndex = 4;
            // 
            // cmbPanelLocation
            // 
            cmbPanelLocation.AutoCompleteMode = AutoCompleteMode.SuggestAppend;
            cmbPanelLocation.AutoCompleteSource = AutoCompleteSource.ListItems;
            cmbPanelLocation.FormattingEnabled = true;
            cmbPanelLocation.Location = new Point(341, 68);
            cmbPanelLocation.Name = "cmbPanelLocation";
            cmbPanelLocation.Size = new Size(151, 28);
            cmbPanelLocation.TabIndex = 5;
            // 
            // label4
            // 
            label4.AutoSize = true;
            label4.Location = new Point(32, 128);
            label4.Name = "label4";
            label4.Size = new Size(158, 20);
            label4.TabIndex = 6;
            label4.Text = "Description (optional):";
            // 
            // txtDescription
            // 
            txtDescription.Location = new Point(32, 151);
            txtDescription.Multiline = true;
            txtDescription.Name = "txtDescription";
            txtDescription.Size = new Size(421, 49);
            txtDescription.TabIndex = 7;
            // 
            // btnSave
            // 
            btnSave.Location = new Point(32, 240);
            btnSave.Name = "btnSave";
            btnSave.Size = new Size(94, 29);
            btnSave.TabIndex = 8;
            btnSave.Text = "Save";
            btnSave.UseVisualStyleBackColor = true;
            btnSave.Click += btnSave_Click;
            // 
            // btnCancel
            // 
            btnCancel.DialogResult = DialogResult.Cancel;
            btnCancel.Location = new Point(169, 240);
            btnCancel.Name = "btnCancel";
            btnCancel.Size = new Size(94, 29);
            btnCancel.TabIndex = 9;
            btnCancel.Text = "Cancel";
            btnCancel.UseVisualStyleBackColor = true;
            // 
            // AddEditNodeForm
            // 
            AutoScaleDimensions = new SizeF(8F, 20F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(527, 324);
            Controls.Add(btnCancel);
            Controls.Add(btnSave);
            Controls.Add(txtDescription);
            Controls.Add(label4);
            Controls.Add(cmbPanelLocation);
            Controls.Add(txtIPAddress);
            Controls.Add(txtRoute);
            Controls.Add(label3);
            Controls.Add(label2);
            Controls.Add(label1);
            Name = "AddEditNodeForm";
            Text = "AddEditNodeForm";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Label label1;
        private Label label2;
        private Label label3;
        private TextBox txtRoute;
        private TextBox txtIPAddress;
        private ComboBox cmbPanelLocation;
        private Label label4;
        private TextBox txtDescription;
        private Button btnSave;
        private Button btnCancel;
    }
}