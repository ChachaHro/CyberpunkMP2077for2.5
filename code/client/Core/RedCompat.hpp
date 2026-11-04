#pragma once

// Compatibilité avec l'ancienne copie de RedLib utilisée par CyberpunkMP (patch 2.2).
// RedLib 2.5 a ajouté un paramètre "frame" à CallFunctionWithArgs et sorti GetFunction de
// Red::Detail. Sémantique inchangée : la valeur de retour éventuelle est le 1er argument.
namespace Red::Detail
{
template<typename... Args>
inline bool CallFunctionWithArgs(CBaseFunction* aFunc, IScriptable* aContext, Args&&... aArgs)
{
    return CallFunctionWithArgs(nullptr, aFunc, aContext, std::forward<Args>(aArgs)...);
}

inline CBaseFunction* GetFunction(CClass* aType, CName aName)
{
    return Red::GetFunction(aType, aName);
}
} // namespace Red::Detail

namespace Red
{
// Valeur nommée pour CallByParamNames : nom du paramètre script + pointeur vers la valeur
// (le type doit correspondre au type RTTI du paramètre).
struct NamedArg
{
    const char* name;
    void* value;
};

// Appelle une fonction script membre en remplissant ses paramètres PAR NOM. Les paramètres non
// fournis reçoivent la valeur par défaut de leur type. Indépendant de l'ordre et du nombre de
// paramètres (et des vtables), donc robuste aux patchs qui ajoutent des paramètres optionnels.
inline bool CallByParamNames(IScriptable* aContext, CName aFunc, std::initializer_list<NamedArg> aArgs,
                             CStackType* aResult = nullptr)
{
    if (!aContext)
        return false;

    auto* func = Red::GetFunction(aContext->GetType(), aFunc);
    if (!func)
    {
        spdlog::error("[RedCompat] fonction {} introuvable", aFunc.ToString());
        return false;
    }

    const auto count = func->params.Size();
    std::vector<CStackType> stackArgs(count);
    std::vector<std::pair<CBaseRTTIType*, void*>> temps;

    for (uint32_t i = 0; i < count; ++i)
    {
        auto* param = func->params[i];
        void* value = nullptr;
        for (const auto& arg : aArgs)
        {
            if (param->name == CName(arg.name))
            {
                value = arg.value;
                break;
            }
        }
        if (!value)
        {
            const auto size = param->type->GetSize();
            const auto align = std::max<uint32_t>(param->type->GetAlignment(), 8);
            value = _aligned_malloc(size, align);
            std::memset(value, 0, size);
            param->type->Construct(value);
            temps.emplace_back(param->type, value);
        }
        stackArgs[i] = CStackType(param->type, value);
    }

    CStack stack(aContext, stackArgs.data(), count, aResult);
    const bool ok = Detail::CallFunctionWithStack(func, stack);

    for (auto& [type, mem] : temps)
    {
        type->Destruct(mem);
        _aligned_free(mem);
    }
    return ok;
}
} // namespace Red
