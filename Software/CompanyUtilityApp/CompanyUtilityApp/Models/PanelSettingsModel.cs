using System;
using System.Collections.Generic;
using System.Text;

namespace CompanyUtilityApp
{
    public class PanelSettingsModel
    {
        public int Route { get; set; }
        public int PanelLocation { get; set; }
        public string IPAddress { get; set; }
        public string Description { get; set; }

        public override string ToString() => PanelLocation.ToString(); // for dropdown display
    }
}
