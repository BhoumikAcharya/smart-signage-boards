namespace CompanyUtilityApp
{
    partial class EditAreaForm
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
            label2 = new Label();
            txtPanelLocation = new TextBox();
            txtDescription = new TextBox();
            button1 = new Button();
            button2 = new Button();
            label1 = new Label();
            SuspendLayout();
            // 
            // label2
            // 
            label2.Location = new Point(66, 89);
            label2.Name = "label2";
            label2.Size = new Size(100, 23);
            label2.TabIndex = 6;
            label2.Text = "Discription:";
            // 
            // txtPanelLocation
            // 
            txtPanelLocation.Enabled = false;
            txtPanelLocation.Location = new Point(184, 43);
            txtPanelLocation.Name = "txtPanelLocation";
            txtPanelLocation.Size = new Size(125, 27);
            txtPanelLocation.TabIndex = 2;
            // 
            // txtDescription
            // 
            txtDescription.Location = new Point(184, 86);
            txtDescription.Name = "txtDescription";
            txtDescription.Size = new Size(125, 27);
            txtDescription.TabIndex = 3;
            // 
            // button1
            // 
            button1.Location = new Point(109, 161);
            button1.Name = "button1";
            button1.Size = new Size(94, 29);
            button1.TabIndex = 4;
            button1.Text = "SAVE";
            button1.UseVisualStyleBackColor = true;
            button1.Click += btnSave_Click;
            // 
            // button2
            // 
            button2.Location = new Point(243, 161);
            button2.Name = "button2";
            button2.Size = new Size(94, 29);
            button2.TabIndex = 5;
            button2.Text = "Cancel";
            button2.UseVisualStyleBackColor = true;
            button2.Click += button2_Click;
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new Point(66, 46);
            label1.Name = "label1";
            label1.Size = new Size(112, 20);
            label1.TabIndex = 0;
            label1.Text = "Panel Location: ";
            // 
            // EditAreaForm
            // 
            AutoScaleDimensions = new SizeF(8F, 20F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(487, 258);
            Controls.Add(button2);
            Controls.Add(button1);
            Controls.Add(txtDescription);
            Controls.Add(txtPanelLocation);
            Controls.Add(label2);
            Controls.Add(label1);
            Name = "EditAreaForm";
            Text = "EditAreaForm";
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion
        private Label label2;
        private TextBox txtPanelLocation;
        private TextBox txtDescription;
        private Button button1;
        private Button button2;
        private Label label1;
    }
}