// File: Data/AuthRepository.cs
using Microsoft.Data.SqlClient;
using Microsoft.VisualBasic.ApplicationServices;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Text;

namespace CompanyUtilityApp.ProgramFiles
{

    public class User
    {
        public int Id { get; set; }
        public string Username { get; set; }
        public string FullName { get; set; }
        // PasswordHash is used internally only; not exposed to UI after login
    }

    public static class AuthRepository
    {
        private static string HashPassword(string password)
        {
            using (SHA256 sha256 = SHA256.Create())
            {
                byte[] bytes = sha256.ComputeHash(Encoding.UTF8.GetBytes(password));
                return BitConverter.ToString(bytes).Replace("-", "").ToLower();
            }
        }

        public static User ValidateUser(string username, string password)
        {
            string hash = HashPassword(password);
            //Debug.WriteLine($"[Auth] Username: '{username}'");
            //Debug.WriteLine($"[Auth] Hash: '{hash}'");
            using (var conn = new SqlConnection(DatabaseHelper.ConnectionString))
            {
                conn.Open();
                string query = @"SELECT Id, Username, FullName 
                                 FROM Users 
                                 WHERE Username = @u AND PasswordHash = @p";
                using (var cmd = new SqlCommand(query, conn))
                {
                    cmd.Parameters.AddWithValue("@u", username);
                    cmd.Parameters.AddWithValue("@p", hash);
                    //Debug.WriteLine("Flag1");

                    using (var reader = cmd.ExecuteReader())
                    {
                        //Debug.WriteLine("Flag2");
                        if (reader.Read())
                        {
                            //Debug.WriteLine("Flag3");
                            //Debug.WriteLine($"Password from database: '{reader.GetInt32(1)}'");
                            return new User
                            {
                                Id = reader.GetInt32(0),
                                Username = reader.GetString(1),
                                FullName = reader.GetString(2)
                            };
                        }
                    }
                }
            }
            return null;
        }
    }
}