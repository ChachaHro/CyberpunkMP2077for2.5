using CyberpunkSdk.Internal;

namespace CyberpunkSdk.Game
{
    public class Player
    {
        private IPlayer player;

        internal Player(IPlayer player)
        {
            this.player = player;
        }

        public void SendChat(string From, string Message)
        {
            player.SendChat(From, Message);
        }

        public ulong Id
        {
            get { return player.Id; }
        }

        public ulong PuppetId
        {
            get {  return player.PuppetId; }
        }

        public uint ConnectionId
        {
            get {  return player.ConnectionId; }
        }

        public Components.MovementComponent MovementComponent
        {
            get { return new Components.MovementComponent(player.MovementComponent); }
        }

        public Components.AttachmentComponent AttachmentComponent
        {
            get { return new Components.AttachmentComponent(player.AttachmentComponent); }
        }

        public string Username
        {
            get { return player.Username; }
        }

        /// <summary>
        /// Jeton de session envoyé par le client (launcher). Lu une seule fois : le serveur l'efface
        /// ensuite. Chaîne vide si absent. Ne jamais le journaliser.
        /// </summary>
        public string TakeSessionToken()
        {
            return player.TakeSessionToken() ?? string.Empty;
        }

        /// <summary>Expulse le joueur au prochain tick serveur (sûr depuis PlayerJoinEvent).</summary>
        public void Kick()
        {
            player.Kick();
        }

        /// <summary>
        /// Valide le joueur (par exemple après vérification de son jeton). Si le serveur exige une
        /// validation (RequireScriptAuthorization), le joueur ne peut rien faire avant cet appel.
        /// </summary>
        public void Authorize()
        {
            player.Authorize();
        }

        /// <summary>
        /// Apparence que les autres joueurs voient, au format du client (JSON {"v":1,"ccstate":"hex","equipment":[...]},
        /// voir AppearanceSystem.GetLocalAppearance côté client). Gardée pour la connexion ; si le personnage est
        /// déjà apparu, il est rechargé chez les autres. Faux si l'apparence est refusée (format, tailles).
        /// </summary>
        public bool SetAppearance(string appearance)
        {
            return player.SetAppearance(appearance);
        }
    }
}
