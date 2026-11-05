using System.Security;
using CyberpunkSdk.Internal;
using EmbedIO;
using EmbedIO.Actions;
using EmbedIO.Authentication;
using Server.Loader.Extensions;
using Swan.Logging;

namespace Server.Loader.Systems
{
    internal class WebApiPluginDto
    {
        public required string Name { get; set; }
    }

    internal class WebApi
    {
        private Plugins Plugins { get; set; }

        private Statistics Statistics { get; }

        private Task? ServerTask { get; set; }

        private bool Running { get; set; } = true;

        internal WebApi(Plugins plugins, Statistics statistics)
        {
            Statistics = statistics;
            Plugins = plugins;

            var port = IConfig.Get().Port;
            Credentials = ReadAdminCredentials();

            // Sans identifiants admin, une API sans mot de passe n'écoute que sur la machine locale (build
            // Debug seulement : en Release, les identifiants sont obligatoires).
            var host = Credentials == null ? "127.0.0.1" : "+";
            if (Credentials == null)
                new CyberpunkSdk.Systems.Logger("WebApi").Warn("Admin API without credentials: listening on 127.0.0.1 only.");

            ServerTask = CreateWebServer($"http://{host}:{port}").RunAsync();
        }

        private (string Username, string Password)? Credentials { get; }

        private static (string Username, string Password)? ReadAdminCredentials()
        {
            // NOTE: should be replaced with Discord authentication later.
            var username = Environment.GetEnvironmentVariable("CYBERPUNKMP_ADMIN_USERNAME");
            var password = Environment.GetEnvironmentVariable("CYBERPUNKMP_ADMIN_PASSWORD");

            if (!string.IsNullOrEmpty(username) && !string.IsNullOrEmpty(password))
                return (username, password);

            if (EnvironmentHelper.IsDebug)
                return null;

            throw new SecurityException("You must provide admin credentials using environment variables.");
        }

        ~WebApi()
        {
            ServerTask?.Dispose();
        }

        private WebServer CreateWebServer(string url)
        {
            Logger.UnregisterLogger<ConsoleLogger>();

            var server = new WebServer(o => o
                .WithUrlPrefix(url)
                .WithMode(HttpListenerMode.EmbedIO))
                .WithModule(new ActionModule("/api/v1/mods/", HttpVerbs.Get, HandleModsRoute))
                .WithModule(new ActionModule("/api/v1/statistics/", HttpVerbs.Get, HandleStatistics));

            RegisterAuthentication(server);
            RegisterPlugins(server);
            RegisterAssets(server);
            return server;
        }

        private void RegisterPlugins(WebServer server)
        {
            var withAssets = Plugins.GetPlugins(p => p.Assets != null);

            foreach (var plugin in withAssets)
            {
                server.WithStaticFolder(
                    baseRoute: plugin.GetAssetsUrl,
                    fileSystemPath: plugin.GetAssetsPath,
                    isImmutable: EnvironmentHelper.IsRelease,
                    configure: m => { m.AddCustomMimeType(".umd.js", "application/javascript"); });
            }
            var withHooks = Plugins.GetPlugins(p => p.WebApi != null);

            foreach (var plugin in withHooks)
            {
                var hook = plugin.WebApi!;

                // Un contrôleur par requête (EmbedIO réécrit HttpContext sur l'instance : une instance
                // partagée mélangerait les requêtes simultanées).
                server.WithWebApi(
                    baseRoute: plugin.GetWebApiUrl,
                    configure: m => m.WithControllerFactory(hook.BuildController()));
            }

            server.WithModule(new ActionModule("/api/v1/plugins/", HttpVerbs.Get, HandleListPlugins));
        }

        private void RegisterAssets(WebServer server)
        {
            var assetsPath = FileSystemHelper.GetPath("assets");

            server.WithStaticFolder(
                baseRoute: "/",
                fileSystemPath: assetsPath,
                isImmutable: EnvironmentHelper.IsRelease);
        }

        private void RegisterAuthentication(WebServer server)
        {
            // Pas d'identifiants (Debug seulement) : l'API n'écoute que sur 127.0.0.1, voir le constructeur
            if (Credentials is not { } credentials)
            {
                return;
            }

            // NOTE: always allow access to the following routes for the Server
            //       List System.
            // Plus les routes que chaque plugin déclare publiques (ex. connexion du launcher).
            var whitelist = new List<string>
            {
                "/api/v1/mods",
                "/api/v1/statistics",
            };
            foreach (var plugin in Plugins.GetPlugins(p => p.WebApi != null))
            {
                foreach (var route in plugin.WebApi!.PublicRoutes)
                {
                    if (!string.IsNullOrWhiteSpace(route) && route.StartsWith('/') && !route.Contains(".."))
                        whitelist.Add(plugin.GetWebApiUrl + route);
                }
            }

            server.WithModule(new AdminAuthModule(whitelist.ToArray(), credentials.Username, credentials.Password));
        }

        #region Routes

        private Task HandleModsRoute(IHttpContext context)
        {
            return context.SendDataAsync(new
            {
                Mods = Plugins.ClientMods
            });
        }

        private Task HandleStatistics(IHttpContext context)
        {
            return context.SendDataAsync(new
            {
                Statistics.Rpcs
            });
        }

        private Task HandleListPlugins(IHttpContext context)
        {
            var withHooks = Plugins.GetPlugins(p => p.WebApi != null);

            return context.SendDataAsync(
                withHooks.Select(plugin => new WebApiPluginDto
                    {
                        Name = plugin.Name,
                    })
                    .ToList()
            );
        }

        #endregion
    }

    internal class AdminAuthModule(string[] whitelist, string adminUsername, string adminPassword)
        : BasicAuthenticationModule("/", "Admin")
    {
        // Empreintes calculées une fois : la comparaison se fait sur des tailles égales, en temps constant
        private readonly byte[] _username = Hash(adminUsername);
        private readonly byte[] _password = Hash(adminPassword);

        private static byte[] Hash(string? value) =>
            System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(value ?? string.Empty));

        protected override Task<bool> VerifyCredentialsAsync(string path, string userName, string password,
            CancellationToken cancellationToken)
        {
            var normalized = path.Length > 1 ? path.TrimEnd('/') : path;
            if (whitelist.Any(p => string.Equals(p, normalized, StringComparison.Ordinal)))
            {
                return Task.FromResult(true);
            }

            // Les deux comparaisons sont toujours faites (pas de raccourci qui trahirait un bon pseudo)
            var userOk = System.Security.Cryptography.CryptographicOperations.FixedTimeEquals(Hash(userName), _username);
            var passwordOk = System.Security.Cryptography.CryptographicOperations.FixedTimeEquals(Hash(password), _password);
            return Task.FromResult(userOk & passwordOk);
        }
    }
}