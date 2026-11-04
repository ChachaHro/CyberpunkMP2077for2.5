module CyberpunkMP.World

import Codeware.*
import CyberpunkMP.*

public native class NetworkWorldSystem extends IGameSystem {
    public native func Connect() -> Void;
    public native func Disconnect() -> Void;
    // Intégration d'un launcher externe (--session=, --launcher=, --launcher-key=) : connexion
    // automatique après le choix au menu, jeton de session et relais local pour en obtenir un neuf.
    public native func ConsumeAutoConnect() -> Bool;
    public native func RequestJoin() -> Void;
    public native func IsLaunchedByLauncher() -> Bool;
    public native func HasSessionToken() -> Bool;
    public native func HasLauncherRelay() -> Bool;
    public native func FetchSessionToken() -> Bool;
    // Ligne dans le journal du client (CyberpunkMP.log) : traces de diagnostic des scripts et mods.
    public native func WriteLog(message: String) -> Void;
    public native func GetEntityIdByServerId(serverId: Uint64) -> EntityID;
    public native func GetAppearanceSystem() -> ref<AppearanceSystem>;
    public native func GetChatSystem() -> ref<ChatSystem>;
    public native func GetInterpolationSystem() -> ref<InterpolationSystem>;
    public native func GetVehicleSystem() -> ref<VehicleSystem>;

    // Extension points, served by the mods registered in CyberpunkMPExtensions (see below).

    // Text typed in the chat box. Default: the built-in global chat.
    public func SendChatInput(text: String) -> Void {
        if !CyberpunkMPExtensions.SendChatInput(text) {
            this.GetChatSystem().Send(text);
        }
    }

    // Every UI text shown by the mod goes through here, so another mod can translate it.
    public func Translate(text: String) -> String {
        return CyberpunkMPExtensions.Translate(text);
    }

    // Called right before connecting automatically after the launcher's join request.
    public func OnAutoConnect() -> Void {
        CyberpunkMPExtensions.OnAutoConnect();
    }

    // Launched by an external launcher: it decides when to join, so no connect/disconnect hotkeys.
    public func AllowsManualConnection() -> Bool {
        return !this.IsLaunchedByLauncher();
    }

    public func OnConnected() -> Void {
        CyberpunkMPExtensions.OnConnected();
        // let evt: ref<ConnectedToServer>;
        // evt.m_connected = true;
        // GameInstance.GetUISystem(GetGameInstance()).QueueEvent(evt);
        
        let blackboardSystem: ref<BlackboardSystem> = GameInstance.GetBlackboardSystem(GetGameInstance());
        let blackboard: ref<IBlackboard> = blackboardSystem.Get(GetAllBlackboardDefs().UIGameData);
        blackboard.SetBool(GetAllBlackboardDefs().UIGameData.UIMultiplayerConnectedToServer, true, true);
    }

    public func OnDisconnected(reason: Uint32) -> Void {
        // let evt: ref<ConnectedToServer>;
        // evt.m_connected = true;
        // GameInstance.GetUISystem(GetGameInstance()).QueueEvent(evt);
        
        let blackboardSystem: ref<BlackboardSystem> = GameInstance.GetBlackboardSystem(GetGameInstance());
        let blackboard: ref<IBlackboard> = blackboardSystem.Get(GetAllBlackboardDefs().UIGameData);
        blackboard.SetBool(GetAllBlackboardDefs().UIGameData.UIMultiplayerConnectedToServer, false, true);
    }

    public func CreatePuppet(position: Vector4, rotation: Quaternion, isMale: Bool) -> EntityID {
        let npcSpec = new DynamicEntitySpec();

        if isMale {
            npcSpec.recordID = t"Character.MaMuppet";
        } else {
            npcSpec.recordID = t"Character.WaMuppet";
            // npcSpec.recordID = t"Character.Panam";
        }
        npcSpec.alwaysSpawned = true;
        npcSpec.position = position;
        npcSpec.orientation = rotation;
        npcSpec.persistState = false;
        npcSpec.persistSpawn = false;
        npcSpec.tags = [n"CyberpunkMP.Puppet"];


        return GameInstance.GetDynamicEntitySystem().CreateEntity(npcSpec);
    }

    public func DeletePuppet(entityId: EntityID) {
        GameInstance.GetDynamicEntitySystem().DeleteEntity(entityId);
    }
}

@addMethod(GameInstance)
public static native func GetNetworkWorldSystem() -> ref<NetworkWorldSystem>

// Base class for mods that customise CyberpunkMP's client behaviour. Subclass it, override what you
// need, and register an instance from your own ScriptableSystem:
//     CyberpunkMPExtensions.Register(new MyExtension());
public abstract class CyberpunkMPExtension extends IScriptable {
    // Return true when the text was handled (the built-in global chat is then skipped).
    public func SendChatInput(text: String) -> Bool { return false; }
    // Return an empty string to leave the text unchanged.
    public func Translate(text: String) -> String { return ""; }
    public func OnAutoConnect() -> Void {}
    public func OnConnected() -> Void {}
}

public class CyberpunkMPExtensions extends ScriptableSystem {
    private let m_extensions: array<ref<CyberpunkMPExtension>>;

    public static func Get() -> ref<CyberpunkMPExtensions> {
        let game = GetGameInstance();
        if !GameInstance.IsValid(game) {
            return null;
        }
        return GameInstance.GetScriptableSystemsContainer(game).Get(n"CyberpunkMP.World.CyberpunkMPExtensions") as CyberpunkMPExtensions;
    }

    public static func Register(extension: ref<CyberpunkMPExtension>) -> Void {
        let self = CyberpunkMPExtensions.Get();
        if IsDefined(self) && IsDefined(extension) {
            ArrayPush(self.m_extensions, extension);
        }
    }

    public static func SendChatInput(text: String) -> Bool {
        let self = CyberpunkMPExtensions.Get();
        if IsDefined(self) {
            for extension in self.m_extensions {
                if extension.SendChatInput(text) {
                    return true;
                }
            }
        }
        return false;
    }

    public static func Translate(text: String) -> String {
        let self = CyberpunkMPExtensions.Get();
        if IsDefined(self) {
            for extension in self.m_extensions {
                let translated = extension.Translate(text);
                if StrLen(translated) > 0 {
                    return translated;
                }
            }
        }
        return text;
    }

    public static func OnAutoConnect() -> Void {
        let self = CyberpunkMPExtensions.Get();
        if IsDefined(self) {
            for extension in self.m_extensions {
                extension.OnAutoConnect();
            }
        }
    }

    public static func OnConnected() -> Void {
        let self = CyberpunkMPExtensions.Get();
        if IsDefined(self) {
            for extension in self.m_extensions {
                extension.OnConnected();
            }
        }
    }
}