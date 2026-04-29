using CompanyUtilityApp.UserControls;
using Microsoft.Data.SqlClient;
using System;
using System.Collections.Generic;
using System.Data;

namespace CompanyUtilityApp.ProgramFiles
{
    public class Node
    {
        public int Id { get; set; }
        public int Route { get; set; }
        public string IPAddress { get; set; }
        public int PanelLocation { get; set; }
        public string Description { get; set; }
    }

    public static class NodeRepository
    {
        // --- READ all nodes ---
        public static List<Node> GetAllNodesForRoute(int route)
        {
            var nodes = new List<Node>();
            using (var connection = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                connection.Open();
                string query = @"SELECT Id, Route, IPAddress, PanelLocation, Description 
                         FROM Nodes
                         WHERE Route = @Route
                         ORDER BY PanelLocation ASC";
                using (var command = new SqlCommand(query, connection))
                {
                    command.Parameters.AddWithValue("@Route", route);
                    using (var reader = command.ExecuteReader())
                    {
                        while (reader.Read())
                        {
                            nodes.Add(new Node
                            {
                                Id = reader.GetInt32(0),
                                Route = reader.GetInt32(1),
                                IPAddress = reader.GetString(2),
                                PanelLocation = reader.GetInt32(3),
                                Description = reader.IsDBNull(4) ? null : reader.GetString(4)
                            });
                        }
                    }
                }
            }
            return nodes;
        }

        public static bool IpAddressExists(string ipAddress, int? excludeNodeId = null)
        {
            using (var connection = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                connection.Open();
                string query = "SELECT COUNT(*) FROM Nodes WHERE IPAddress = @IPAddress";
                if (excludeNodeId.HasValue)
                    query += " AND Id != @Id";

                using (var command = new SqlCommand(query, connection))
                {
                    command.Parameters.AddWithValue("@IPAddress", ipAddress);
                    if (excludeNodeId.HasValue)
                        command.Parameters.AddWithValue("@Id", excludeNodeId.Value);

                    int count = (int)command.ExecuteScalar();
                    return count > 0;
                }
            }
        }

        public static bool PanelLocationExistsForRoute(int route, int panelLocation, int? excludeNodeId = null)
        {
            using (var connection = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                connection.Open();
                string query = "SELECT COUNT(*) FROM Nodes WHERE Route = @Route AND PanelLocation = @PanelLocation";
                if (excludeNodeId.HasValue)
                    query += " AND Id != @Id";

                using (var command = new SqlCommand(query, connection))
                {
                    command.Parameters.AddWithValue("@Route", route);
                    command.Parameters.AddWithValue("@PanelLocation", panelLocation);
                    if (excludeNodeId.HasValue)
                        command.Parameters.AddWithValue("@Id", excludeNodeId.Value);

                    int count = (int)command.ExecuteScalar();
                    return count > 0;
                }
            }
        }

        // --- CREATE a new node ---
        public static void AddNode(Node node)
        {
            using (var connection = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                connection.Open();
                string query = @"INSERT INTO Nodes (Route, IPAddress, PanelLocation, Description) 
                                 VALUES (@Route, @IPAddress, @PanelLocation, @Description)";
                using (var command = new SqlCommand(query, connection))
                {
                    command.Parameters.AddWithValue("@Route", node.Route);
                    command.Parameters.AddWithValue("@IPAddress", node.IPAddress);
                    command.Parameters.AddWithValue("@PanelLocation", node.PanelLocation);
                    command.Parameters.AddWithValue("@Description", (object)node.Description ?? DBNull.Value);
                    command.ExecuteNonQuery();
                }
            }
        }

        // --- UPDATE an existing node ---
        public static void UpdateNode(Node node)
        {
            using (var connection = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                connection.Open();
                string query = @"UPDATE Nodes SET 
                                 Route = @Route,
                                 IPAddress = @IPAddress,
                                 PanelLocation = @PanelLocation,
                                 Description = @Description
                                 WHERE Id = @Id";
                using (var command = new SqlCommand(query, connection))
                {
                    command.Parameters.AddWithValue("@Id", node.Id);
                    command.Parameters.AddWithValue("@Route", node.Route);
                    command.Parameters.AddWithValue("@IPAddress", node.IPAddress);
                    command.Parameters.AddWithValue("@PanelLocation", node.PanelLocation);
                    command.Parameters.AddWithValue("@Description", (object)node.Description ?? DBNull.Value);
                    command.ExecuteNonQuery();
                }
            }
        }

        // --- DELETE a node ---
        public static void DeleteNode(int id)
        {
            using (var connection = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                connection.Open();
                string query = "DELETE FROM Nodes WHERE Id = @Id";
                using (var command = new SqlCommand(query, connection))
                {
                    command.Parameters.AddWithValue("@Id", id);
                    command.ExecuteNonQuery();
                }
            }
        }

        // In NodeRepository.cs from PanelSetting.cs
        public static List<PanelSettingsModel> GetAssignedNodesForRoute(int route)
        {
            var list = new List<PanelSettingsModel>();
            using (var conn = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                conn.Open();
                string query = @"
            SELECT N.Route, N.PanelLocation, N.IPAddress, COALESCE(N.Description, 'No description') AS Description
            FROM Nodes N
            LEFT JOIN Areas A ON N.Route = A.Route AND N.PanelLocation = A.PanelLocation
            WHERE N.Route = @Route
            ORDER BY N.PanelLocation ASC";
                using (var cmd = new SqlCommand(query, conn))
                {
                    cmd.Parameters.AddWithValue("@Route", route);
                    using (var reader = cmd.ExecuteReader())
                    {
                        while (reader.Read())
                        {
                            list.Add(new PanelSettingsModel
                            {
                                Route = reader.GetInt32(0),
                                PanelLocation = reader.GetInt32(1),
                                IPAddress = reader.GetString(2),
                                Description = reader.GetString(3)
                            });
                        }
                    }
                }
            }
            return list;
        }

    }
}