// Minimal GIT_ASKPASS helper for SpecStudio. Git invokes this program
// non-interactively (no terminal available) whenever it needs a credential
// and no configured credential helper has already answered. It receives the
// prompt text as argv[1] (e.g. "Username for 'https://github.com': " or
// "Password for 'https://user@github.com': ") and must print the answer to
// stdout. SpecStudio's GitClient sets SPECSTUDIO_GIT_USERNAME/
// SPECSTUDIO_GIT_PASSWORD on the git child process's environment for the
// single invocation that needs them, so the credential never has to be
// written into the remote URL or .git/config.
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
    const char* prompt = argc > 1 ? argv[1] : "";
    const char* value = nullptr;

    if (std::strstr(prompt, "Username"))
        value = std::getenv("SPECSTUDIO_GIT_USERNAME");
    else if (std::strstr(prompt, "Password"))
        value = std::getenv("SPECSTUDIO_GIT_PASSWORD");

    std::printf("%s\n", value ? value : "");
    return 0;
}
