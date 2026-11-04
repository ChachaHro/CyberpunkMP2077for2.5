#include "Core/Stl.hpp"
#include <RED4ext/Scripting/Natives/Generated/ink/TextInputWidget.hpp>

// Patch 2.5 : ink::TextWidget est passé de 0x430 à 0x460 octets, la partie propre à
// TextInputWidget fait toujours 0x40 octets (0x430-0x470 -> 0x460-0x4A0). caretPosition
// passe donc de 0x434 à 0x464. Si la taille change encore, la compilation échoue ici.
static_assert(sizeof(RED4ext::ink::TextInputWidget) == 0x4A0, "TextInputWidget a changé : revoir l'offset de caretPosition");
constexpr uintptr_t kCaretPositionOffset = 0x464;

struct TextInputExtension : RED4ext::ink::TextInputWidget
{
    void MoveCaret(int32_t position)
    {
        *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(this) + kCaretPositionOffset) = position;
    }
};

RTTI_EXPAND_CLASS(RED4ext::ink::TextInputWidget, { RTTI_METHOD_FQN(TextInputExtension::MoveCaret); });