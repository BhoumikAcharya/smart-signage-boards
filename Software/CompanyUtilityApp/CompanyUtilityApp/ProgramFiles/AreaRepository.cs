// Data/AreaRepository.cs
using CompanyUtilityApp.ProgramFiles;
using Microsoft.Data.SqlClient;
using System.Collections.Generic;

namespace CompanyUtilityApp
{

    public class AreaDisplayItem
    {
        public int Route { get; set; }
        public int PanelLocation { get; set; }
        public string IPAddress { get; set; }   // "Not Assigned" if null
        public string Description { get; set; }
    }

    public static class AreaRepository
    {
        /// <summary>
        /// Get all panel locations for a route, with IP address (if assigned) and description.
        /// </summary>
        public static List<AreaDisplayItem> GetAreasByRoute(int route)
        {
            var items = new List<AreaDisplayItem>();
            using (var conn = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                conn.Open();
                string query = @"
                    SELECT A.Route, A.PanelLocation, 
                           COALESCE(N.IPAddress, 'N/A') AS IPAddress, 
                           A.Description
                    FROM Areas A
                    LEFT JOIN Nodes N ON A.Route = N.Route AND A.PanelLocation = N.PanelLocation
                    WHERE A.Route = @Route
                    ORDER BY A.PanelLocation ASC;";

                using (var cmd = new SqlCommand(query, conn))
                {
                    cmd.Parameters.AddWithValue("@Route", route);
                    using (var reader = cmd.ExecuteReader())
                    {
                        while (reader.Read())
                        {
                            items.Add(new AreaDisplayItem
                            {
                                Route = reader.GetInt32(0),
                                PanelLocation = reader.GetInt32(1),
                                IPAddress = reader.GetString(2),
                                Description = reader.IsDBNull(3) ? null : reader.GetString(3)
                            });
                        }
                    }
                }
            }
            return items;
        }

        /// <summary>
        /// Update only the description for a specific panel location on a route.
        /// </summary>
        public static void UpdateDescription(int route, int panelLocation, string description)
        {
            using (var conn = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                conn.Open();
                string query = @"
                    UPDATE Areas 
                    SET Description = @Description 
                    WHERE Route = @Route AND PanelLocation = @PanelLocation";

                using (var cmd = new SqlCommand(query, conn))
                {
                    cmd.Parameters.AddWithValue("@Route", route);
                    cmd.Parameters.AddWithValue("@PanelLocation", panelLocation);
                    cmd.Parameters.AddWithValue("@Description", (object)description ?? DBNull.Value);
                    cmd.ExecuteNonQuery();
                }
            }
        }
    }
}