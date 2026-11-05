using System.IO.Compression;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text.Json;
using CurseForge.APIClient;
using CurseForge.APIClient.Models.Enums;
using CurseForge.APIClient.Models.Files;
using CurseForge.APIClient.Models.Mods;
using CyberpunkSdk;
using CyberpunkSdk.Systems;
using File = System.IO.File;


namespace Server.Loader.Systems
{
    internal class Artifact
    {
        public string Checksum { get; set; } = "";
        public List<string> Directories { get; set; } = new List<string>();
    }

    internal class Cache
    {
        public Dictionary<int, Artifact> Mods { get; set; } = new Dictionary<int, Artifact>();
    }

    internal class Configuration
    {
        public List<int>? ServerMods { get; set; } = new List<int>();
        public List<int>? ClientMods { get; set; } = new List<int>();
        public string ApiKey { get; set; } = "";
    }

    internal class ModDef
    {
        public string Name { get; set; } = "";
        public int Id { get; set; }
        public string Description { get; set; } = "";
        public string Url { get; set; } = "";
        public string Checksum { get; set; } = "";
    }

    internal class PluginInfo
    {
        public string Name { get; set; } = "";
        public string FullName { get; set; } = "";
        public IWebApiHook? WebApi { get; set; }
        public string? Assets { get; set; }

        public string GetWebApiUrl => $"/api/v1/plugins/{Name.ToLower()}";
        
        public string GetAssetsUrl => $"/api/v1/plugins/{Name.ToLower()}/assets/";
        public string GetAssetsPath => Assets!;
    }

    internal class Plugins
    {
        private Configuration configuration = new();
        private Cache cache = new();
        private List<ModDef> serverMods = [];
        private List<ModDef> clientMods = [];
        private List<PluginInfo> plugins = [];
        private Logger logger = new("SDK");

        public IList<ModDef> ClientMods => clientMods;
        
        public IList<PluginInfo> GetPlugins(Func<PluginInfo, bool> predicate) => plugins.Where(predicate).ToList();

        private void LoadConfiguration(string path)
        {
            try
            {
                Directory.CreateDirectory(Path.Combine(path, "config"));
                var filepath = Path.Combine(path, "config", "mods.json");
                if (Path.Exists(filepath))
                {
                    var content = File.ReadAllText(filepath);
                    Configuration? config = JsonSerializer.Deserialize<Configuration>(content);

                    if (config != null)
                    {
                        configuration = config;
                    }
                }
                else
                {
                    JsonSerializerOptions options = new()
                    {
                        WriteIndented = true
                    };

                    var content = JsonSerializer.Serialize(configuration, options);
                    File.WriteAllText(filepath, content);
                }

                filepath = Path.Combine(path, "config", "_cache.json");
                if (Path.Exists(filepath))
                {
                    var content = File.ReadAllText(filepath);
                    Cache? c = JsonSerializer.Deserialize<Cache>(content);

                    if (c != null)
                    {
                        cache = c;
                    }
                }
            }
            catch (Exception ex)
            {
                logger.Error(ex.ToString());
            }
        }

        private void GetMods(List<int>? mods, bool client)
        {
            if (mods == null || mods.Count == 0)
                return;

            using (var cfApiClient = new ApiClient(configuration.ApiKey))
            {
                var modList = cfApiClient.GetModsByIdListAsync(new GetModsByIdsListRequestBody
                {
                    ModIds = mods
                }).Result;

                foreach (var m in modList.Data)
                {
                    ModDef def = new();

                    def.Description = m.Summary;
                    def.Id = m.Id;
                    def.Name = m.Name;
                    foreach (var f in m.LatestFiles)
                    {
                        if (f.Id != m.MainFileId)
                            continue;

                        if (f.FileStatus == FileStatus.Approved)
                        {
                            def.Url = f.DownloadUrl;
                            foreach (var hash in f.Hashes)
                            {
                                if (hash.Algo == HashAlgo.Sha1)
                                {
                                    def.Checksum = hash.Value;
                                    break;
                                }
                            }
                        }
                    }

                    if (client)
                        clientMods.Add(def);
                    else
                        serverMods.Add(def);
                }
            }
        }

        private void DownloadMods(string root)
        {
            logger.Info("Checking mods...");

            try
            {
                GetMods(configuration.ClientMods, true);
                GetMods(configuration.ServerMods, false);

                DownloadServerMods(root);
            }
            catch (Exception ex)
            {
                logger.Error(ex.ToString());
            }
        }

        private static readonly HttpClient DownloadClient = new() { Timeout = TimeSpan.FromMinutes(5) };

        // Lève une exception en cas d'échec : un téléchargement raté ne doit jamais passer pour réussi.
        private static byte[] DownloadBytes(string url)
        {
            using var response = DownloadClient.GetAsync(url).GetAwaiter().GetResult();
            response.EnsureSuccessStatusCode();
            return response.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult();
        }

        // Le fichier téléchargé doit avoir l'empreinte SHA-1 annoncée par CurseForge : sinon (fichier
        // altéré en route, miroir compromis) il n'est ni extrait ni chargé.
        internal static bool MatchesChecksum(byte[] content, string expectedSha1) =>
            !string.IsNullOrEmpty(expectedSha1) &&
            string.Equals(Convert.ToHexString(System.Security.Cryptography.SHA1.HashData(content)), expectedSha1,
                StringComparison.OrdinalIgnoreCase);

        private void DownloadMod(ModDef def)
        {
            if (!cache.Mods.TryGetValue(def.Id, out var artifact))
            {
                // If the artifact does not exist in the cache, create a new one
                artifact = new Artifact { Checksum = "", Directories = new List<string>() };
                cache.Mods[def.Id] = artifact;
            }

            if (def.Checksum == artifact.Checksum)
            {
                logger.Info($"Mod {def.Id} - {def.Name} up-to-date, using cache.");
                return;
            }

            logger.Info($"Mod {def.Id} - {def.Name} missing.");

            if (string.IsNullOrEmpty(def.Url) || string.IsNullOrEmpty(def.Checksum))
            {
                logger.Error($"Mod {def.Id} - {def.Name}: no approved file with a SHA-1 checksum, skipped.");
                return;
            }

            // Téléchargé et vérifié AVANT de toucher à la version en place
            byte[] content;
            try
            {
                content = DownloadBytes(def.Url);
            }
            catch (Exception ex)
            {
                logger.Error($"Mod {def.Id} - {def.Name}: download failed ({ex.Message}), keeping the current version.");
                return;
            }

            if (!MatchesChecksum(content, def.Checksum))
            {
                logger.Error($"Mod {def.Id} - {def.Name}: SHA-1 checksum mismatch, download refused.");
                return;
            }

            foreach (var directory in artifact.Directories)
            {
                logger.Debug($"Deleting {directory}");
                if (Directory.Exists(directory))
                    Directory.Delete(directory, true);
            }

            // Define the path for the downloaded zip file and extraction directory
            string downloadPath = Path.Combine("plugins", $"{def.Id}.zip");
            string tempExtractPath = Path.Combine("plugins", $"{def.Id}_temp");

            // Ensure the temporary extraction directory exists
            if (Directory.Exists(tempExtractPath))
            {
                Directory.Delete(tempExtractPath, true);
            }

            Directory.CreateDirectory(tempExtractPath);

            File.WriteAllBytes(downloadPath, content);

            // Unzip the file to the temporary extraction path
            ZipFile.ExtractToDirectory(downloadPath, tempExtractPath);

            // Clean up the downloaded zip file
            File.Delete(downloadPath);

            // Move contents of the temporary extraction directory to the plugins directory
            var topLevelDirectories = Directory.GetDirectories(tempExtractPath);
            foreach (var directory in topLevelDirectories)
            {
                string destinationPath = Path.Combine("plugins", Path.GetFileName(directory));
                Directory.Move(directory, destinationPath);
            }

            // Clean up the temporary extraction directory
            Directory.Delete(tempExtractPath, true);

            // Update the artifact directories to point to the new locations
            artifact.Directories.Clear();
            artifact.Directories.AddRange(topLevelDirectories.Select(dir =>
                Path.Combine("plugins", Path.GetFileName(dir))));

            // Update the artifact checksum
            artifact.Checksum = def.Checksum;

            logger.Info($"Mod {def.Id} - {def.Name} downloaded and extracted successfully.");
        }

        private void DownloadServerMods(string root)
        {
            foreach (var def in serverMods)
            {
                DownloadMod(def);
            }

            var filepath = Path.Combine(root, "config", "_cache.json");
            JsonSerializerOptions options = new()
            {
                WriteIndented = true
            };

            var content = JsonSerializer.Serialize(cache, options);
            File.WriteAllText(filepath, content);
        }

        private IWebApiHook? DetectWebApiHook(Type plugin, string name)
        {
            var property = plugin.GetProperty("Instance", BindingFlags.Static | BindingFlags.Public);
            if (property == null)
            {
                return null;
            }

            var instance = property.GetValue(null);
            if (instance is not IWebApiHook hook)
            {
                return null;
            }
            return hook;
        }

        private string? DetectAssets(string path, string name)
        {
            path = Path.Combine(path, "assets");

            if (!Directory.Exists(path))
            {
                return null;
            }

            if (Directory.GetFiles(path).Length == 0)
            {
                return null;
            }
            return path;
        }

        internal Plugins(RpcManager rpcManager)
        {
            // Get the location of the current assembly and its containing directory
            string currentAssemblyLocation = Assembly.GetExecutingAssembly().Location;
            string baseDirectory = Path.GetDirectoryName(currentAssemblyLocation)!;
            string exeRoot = baseDirectory;

            LoadConfiguration(exeRoot);
            DownloadMods(exeRoot);

            // Get all subdirectories in the base directory
            string[] subDirectories = Directory.GetDirectories(Path.Combine(baseDirectory, "plugins"));

            foreach (string directory in subDirectories)
            {
                string directoryName = new DirectoryInfo(directory).Name;
                string assemblyPath = Path.Combine(directory, directoryName + ".dll");

                // Check if the assembly file exists
                if (File.Exists(assemblyPath))
                {
                    try
                    {
                        // Load the assembly from the file path
                        var assembly = Assembly.LoadFrom(assemblyPath);
                        var typeName = directoryName + ".Plugin";

                        var plugin = assembly.GetType(typeName);

                        if (plugin != null)
                        {
                            RuntimeHelpers.RunClassConstructor(plugin.TypeHandle);

                            rpcManager.ParseAssembly(assembly);
                            
                            var info = new PluginInfo();
                            info.Name = directoryName[..^"System".Length];
                            info.FullName = directoryName;
                            info.WebApi = DetectWebApiHook(plugin, info.Name);
                            info.Assets = DetectAssets(directory, info.Name.ToLower());
                            plugins.Add(info);
                            
                            var hasHook = info.WebApi != null;
                            var hasAssets = info.Assets != null;
                            logger.Info($"Loaded Plugin" +
                                        $"{(hasHook ? " + WebApi" : "")}" +
                                        $"{(hasAssets ? " + Assets" : "")}: {directoryName}");
                        }
                        else
                        {
                            logger.Warn($"Failed to load assembly: {assemblyPath}. Error: Missing type {typeName}");
                        }
                    }
                    catch (Exception ex)
                    {
                        // Handle exceptions, e.g., if the file is not a .NET assembly
                        logger.Warn($"Failed to load assembly: {assemblyPath}. Error: {ex.Message}");
                    }
                }
                else
                {
                    logger.Warn($"Expected assembly not found: {assemblyPath}");
                }
            }
        }
    }
}