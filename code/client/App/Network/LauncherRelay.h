#pragma once

// Relais local du launcher (--launcher=<port> --launcher-key=<clé>) : au clic sur
// « Rejoindre », le jeu redemande un jeton de session neuf au launcher resté ouvert, au lieu
// d'utiliser celui de la ligne de commande (à usage unique, expiré au bout de quelques minutes).
namespace LauncherRelay
{
// Vrai si le launcher a donné l'adresse de son relais au lancement.
bool IsAvailable();

// Demande un jeton neuf (bloquant, 8 s maximum, 127.0.0.1 uniquement). En cas de succès, le jeton
// remplace celui des réglages. En cas d'échec, aMessage contient la raison (jamais le jeton).
bool FetchSessionToken(std::string& aMessage);
}
