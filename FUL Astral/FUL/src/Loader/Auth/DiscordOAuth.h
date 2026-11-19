#pragma once
#include <string>

// Discord OAuth2 (authorization code) client for the Framework Unified Loader.
// Replaces the username / access-key login. Tokens are stored in
// HKCU\Software\AstralMod so the user stays logged in across restarts.
//
// Setup (do this once):
//   1. Create an application at https://discord.com/developers/applications
//   2. Add OAuth2 redirect URI:  http://localhost:17987/callback
//   3. Add the identify scope and copy the Client ID into kClientId below.
//   4. (Optional) If your app is a confidential client, set kClientSecret too.
namespace DiscordOAuth
{

struct Session
{
    bool ok = false;
    std::string error;      // human-readable failure reason
    std::string userId;     // Discord snowflake
    std::string username;   // Discord username / global name
    std::string avatar;     // Discord avatar hash (display only)
    std::string accessToken;
    std::string refreshToken;
    std::string expiresAt;  // unix epoch (seconds) as a string
};

// Runs the full authorization-code flow on the calling thread:
// opens the user's browser, listens on localhost for the redirect,
// exchanges the code for tokens, fetches the identity and stores tokens.
// Blocking - intended to be run on a worker thread.
Session Authenticate();

// Returns a valid session without opening the browser when possible:
//   * stored access token still valid      -> return it
//   * access token expired, refresh token  -> refresh, store, return it
//   * nothing usable                       -> ok = false (show the login UI)
Session EnsureLoggedIn();

// Clears every stored Discord token (sign out).
void Logout();

// ---- configuration ------------------------------------------------------
extern const char* kClientId;     // <-- paste your Discord Client ID here
extern const char* kClientSecret; // optional for confidential clients
extern const wchar_t* kRedirectUri;
extern const int kCallbackPort;

} // namespace DiscordOAuth
