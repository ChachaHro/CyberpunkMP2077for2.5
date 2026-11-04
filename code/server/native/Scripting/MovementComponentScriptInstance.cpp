#include "Components/MovementComponent.h"
#include "MovementComponentScriptInstance.h"
#include <Components/PlayerComponent.h>


MovementComponentScriptInstance::MovementComponentScriptInstance(flecs::entity aPuppet)
    : m_puppet(aPuppet)
{

}

MovementComponentScriptInstance::~MovementComponentScriptInstance()
{
}

// Puppet reste une entité vide tant que le personnage du joueur n'est pas apparu (juste après la
// connexion) : appeler get<>() dessus plantait le serveur (AccessViolation côté plugins .NET).
static const MovementComponent* GetPuppetMovement(flecs::entity aPlayer)
{
    const auto pComponent = aPlayer.get<PlayerComponent>();
    if (!pComponent || !pComponent->Puppet.is_valid())
        return nullptr;

    return pComponent->Puppet.get<MovementComponent>();
}

Vector3 MovementComponentScriptInstance::GetPosition()
{
    if (const auto pMovementComponent = GetPuppetMovement(m_puppet))
    {
        auto vec = pMovementComponent->Position;
        return Vector3{vec.x, vec.y, vec.z};
    }
    return Vector3{0, 0, 0};
}

Vector3 MovementComponentScriptInstance::GetRotation()
{
    if (const auto pMovementComponent = GetPuppetMovement(m_puppet))
    {
        auto vec = pMovementComponent->Rotation;
        return Vector3{vec.x, vec.y, vec.z};
    }
    return Vector3{0, 0, 0};
}

float MovementComponentScriptInstance::GetVelocity()
{
    if (const auto pMovementComponent = GetPuppetMovement(m_puppet))
        return pMovementComponent->Velocity;

    return 0.f;
}
