using System.Linq.Expressions;
using System.Reflection;
using CyberpunkSdk;
using EmbedIO.WebApi;

namespace Server.Loader.Extensions;

public static class EmbedIoExtensions
{
    /// <summary>Enregistre un contrôleur créé par la fabrique du plugin à CHAQUE requête.</summary>
    public static WebApiModule WithControllerFactory(this WebApiModule webApiModule, Func<PluginWebApiController> factory)
    {
        var type = factory().GetType(); // instance d'essai : uniquement pour connaître le type concret
        var typed = typeof(EmbedIoExtensions)
            .GetMethod(nameof(Typed), BindingFlags.Static | BindingFlags.NonPublic)!
            .MakeGenericMethod(type)
            .Invoke(null, new object[] { factory })!;
        return (WebApiModule)typeof(WebApiModuleExtensions)
            .GetMethods(BindingFlags.Static | BindingFlags.Public)
            .Single(mi => mi.IsGenericMethod & mi.Name == "WithController" && mi.GetParameters().Length == 2)
            .MakeGenericMethod(type)
            .Invoke(null, new object[] { webApiModule, typed });
    }

    private static Func<T> Typed<T>(Func<PluginWebApiController> factory) where T : WebApiController =>
        () => (T)(WebApiController)factory();

    public static WebApiModule WithController(this WebApiModule webApiModule, PluginWebApiController controller)
    {
        Delegate factoryFunc = Expression
            .Lambda(
                Expression.Call(
                    Expression.Constant(controller),
                    typeof(PluginWebApiController)
                        .GetMethod("Build")!
                        .MakeGenericMethod(controller.GetType())
                    )
                )
            .Compile();
        return (WebApiModule)typeof(WebApiModuleExtensions)
            .GetMethods(BindingFlags.Static | BindingFlags.Public)
            .Single(mi => mi.IsGenericMethod & mi.Name == "WithController" && mi.GetParameters().Length == 2)
            .MakeGenericMethod(controller.GetType())
            .Invoke(null, new object[] { webApiModule, factoryFunc.DynamicInvoke(Array.Empty<object>()) });
    }
}