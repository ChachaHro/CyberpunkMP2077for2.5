using EmbedIO;
using EmbedIO.WebApi;

namespace CyberpunkSdk;

public interface IWebApiHook
{
    public Func<PluginWebApiController> BuildController();

    /// <summary>
    /// Routes du contrôleur (relatives à sa base, ex. "/auth/login") accessibles sans l'authentification
    /// admin du serveur. Par défaut aucune : tout reste protégé en Release.
    /// </summary>
    public IEnumerable<string> PublicRoutes => Array.Empty<string>();
}

public class PluginWebApiController : WebApiController
{
    public Func<T> Build<T>() where T : WebApiController => () => this as T;
}