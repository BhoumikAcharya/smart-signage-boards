namespace CompanyUtilityApp.ProgramFiles
{
    public static class DatabaseHelper
    {
        // For LocalDB: A default instance is created per user.
        // The connection string uses "Server=(localdb)\MSSQLLocalDB;Integrated Security=true".
        // For SQL Express/Developer, it would be: "Server=YOUR_COMPUTER_NAME\SQLEXPRESS;Database=YourDatabaseName;Integrated Security=true;TrustServerCertificate=true"
        private static readonly string connectionString = @"Server=(localdb)\MSSQLLocalDB;Database=NodeManagementDB;Integrated Security=true;TrustServerCertificate=true";

        public static string ConnectionString => connectionString;


    }
}