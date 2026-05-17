/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Console Autocompletion Extension
 * Copyright (C) AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#include "extension.h"
#include "amtl/am-string.h"
#include "CDetour/detours.h"
#include <tier0/icommandline.h>
#include "tier1/convar.h"
#include "textconsole.h"
#include "sourcehook.h"
#include <stdio.h>

#ifdef _LINUX
#include <dirent.h>
#include <link.h>
#else
#include <windows.h>
#endif

/**
 * @file extension.cpp
 * @brief Implement extension code here.
 */

static int DoAutocompletion(CTextConsole *tc, const char *consoleText, int consoleTextLen);
static void __stdcall HandleReceiveTab(CTextConsole *tc);
static bool ResolveDedicatedLibrary(char *error, size_t maxlength, void **dedicatedHandle);
static void ReleaseDedicatedLibrary(void *dedicatedHandle);
static bool EnableReceiveTabDetour(void *dedicatedHandle, IGameConfig *gameConfig, char *error, size_t maxlength);
static void PrintSuggestionTable(CTextConsole *tc, const CUtlVector<const char *> &matches, const CUtlVector<int> &matchKinds, int consoleTextLen);
static bool NormalizeConsoleTextState(CTextConsole *tc);
static void LogAutocompleteDebug(const char *fmt, ...);
static bool GetManualRootSuggestions(const char *partial, CUtlVector<const char *> &matches, CUtlVector<int> &matchKinds);
static bool GetManualFileSuggestions(const char *partial, CUtlVector<const char *> &matches, CUtlVector<int> &matchKinds);

extern ISmmAPI *g_SMAPI;

CDetour *receiveTab;
DETOUR_DECL_MEMBER0(DetourReceiveTab, void)
{
	HandleReceiveTab((CTextConsole *)this);
}

ICvar *icvar = NULL;
ConVar sm_autocomplete_debug(
	"sm_autocomplete_debug",
	"0",
	FCVAR_NOTIFY,
	"Enable debug logging for srcds-autocomplete.",
	true,
	0.0f,
	true,
	1.0f);
ConVar sm_autocomplete_max_results(
	"sm_autocomplete_max_results",
	"32",
	FCVAR_NOTIFY,
	"Maximum number of autocomplete suggestions to display.",
	true,
	1.0f,
	true,
	256.0f);

AutoCompleteHook g_AutoCompleteHook;		/**< Global singleton for extension's main interface */

SMEXT_LINK(&g_AutoCompleteHook);

namespace {
enum MatchKind
{
	MatchKind_Command = 0,
	MatchKind_ConVar,
	MatchKind_Manual,
};

const char *kSourceModSubcommands[] = {
	"sm credits",
	"sm cvars",
	"sm dump_handles",
	"sm exts",
	"sm info",
	"sm plugins",
	"sm profiler",
	"sm reload_translations",
	"sm version",
};

const char *kSourceModExtsSubcommands[] = {
	"sm exts info",
	"sm exts list",
	"sm exts load",
	"sm exts reload",
	"sm exts unload",
};

const char *kSourceModPluginsSubcommands[] = {
	"sm plugins info",
	"sm plugins list",
	"sm plugins load",
	"sm plugins reload",
	"sm plugins unload",
};

const char *kMetamodSubcommands[] = {
	"meta clear",
	"meta credits",
	"meta game",
	"meta list",
	"meta load",
	"meta pause",
	"meta refresh",
	"meta retry",
	"meta unload",
	"meta unpause",
	"meta version",
};

const char *kSourceModPluginFileCommands[] = {
	"sm plugins load ",
	"sm plugins reload ",
	"sm plugins unload ",
};

const char *kSourceModExtensionFileCommands[] = {
	"sm exts load ",
	"sm exts reload ",
	"sm exts unload ",
};

constexpr int kMaxFileSuggestionCandidates = 256;
constexpr int kMaxFileSuggestionLength = 512;

void SortMatches(CUtlVector<const char *> &matches, CUtlVector<int> *matchKinds = nullptr)
{
	if (matches.Count() < 2)
		return;

	for (int i = 0; i < matches.Count(); i++)
	{
		for (int j = i + 1; j < matches.Count(); j++)
		{
			if (stricmp(matches[i], matches[j]) > 0)
			{
				const char *temp = matches[i];
				matches[i] = matches[j];
				matches[j] = temp;
				if (matchKinds != nullptr)
				{
					int kindTemp = (*matchKinds)[i];
					(*matchKinds)[i] = (*matchKinds)[j];
					(*matchKinds)[j] = kindTemp;
				}
			}
		}
	}
}

void AddPrefixMatches(const char *partial, const char *const *candidates, size_t candidateCount, CUtlVector<const char *> &matches)
{
	for (size_t i = 0; i < candidateCount; i++)
	{
		const char *candidate = candidates[i];
		if (!Q_strnicmp(partial, candidate, strlen(partial)))
			matches.AddToTail(candidate);
	}
}

bool StartsWithNoCase(const char *text, const char *prefix)
{
	return Q_strnicmp(text, prefix, strlen(prefix)) == 0;
}

void AddFileSuggestionsFromDirectory(const char *partial,
	const char *commandPrefix,
	const char *relativeDirectory,
	const char *requiredExtension,
	CUtlVector<const char *> &matches,
	CUtlVector<int> &matchKinds)
{
	static char fileCandidates[kMaxFileSuggestionCandidates][kMaxFileSuggestionLength];
	int fileCandidateCount = 0;

	char baseDirectory[PLATFORM_MAX_PATH];
	ke::SafeSprintf(baseDirectory, sizeof(baseDirectory), "%s/%s", g_SMAPI->GetBaseDir(), relativeDirectory);

	const char *typedSuffix = partial + strlen(commandPrefix);
	const size_t extLength = strlen(requiredExtension);

#ifdef _LINUX
	DIR *dir = opendir(baseDirectory);
	if (dir == nullptr)
		return;

	struct dirent *entry = nullptr;
	while ((entry = readdir(dir)) != nullptr)
	{
		const char *fileName = entry->d_name;
		size_t fileNameLength = strlen(fileName);
		if (fileNameLength <= extLength)
			continue;
		if (stricmp(fileName + fileNameLength - extLength, requiredExtension) != 0)
			continue;
		if (!Q_strnicmp(typedSuffix, fileName, strlen(typedSuffix)))
		{
			if (fileCandidateCount >= kMaxFileSuggestionCandidates)
				break;
			ke::SafeSprintf(fileCandidates[fileCandidateCount], kMaxFileSuggestionLength, "%s%s", commandPrefix, fileName);
			fileCandidateCount++;
		}
	}

	closedir(dir);
#else
	char searchPattern[PLATFORM_MAX_PATH];
	ke::SafeSprintf(searchPattern, sizeof(searchPattern), "%s\\*%s", baseDirectory, requiredExtension);

	WIN32_FIND_DATAA findData;
	HANDLE handle = FindFirstFileA(searchPattern, &findData);
	if (handle == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;

		const char *fileName = findData.cFileName;
		if (!Q_strnicmp(typedSuffix, fileName, strlen(typedSuffix)))
		{
			if (fileCandidateCount >= kMaxFileSuggestionCandidates)
				break;
			ke::SafeSprintf(fileCandidates[fileCandidateCount], kMaxFileSuggestionLength, "%s%s", commandPrefix, fileName);
			fileCandidateCount++;
		}
	} while (FindNextFileA(handle, &findData));

	FindClose(handle);
#endif

	for (int i = 0; i < fileCandidateCount; i++)
	{
		matches.AddToTail(fileCandidates[i]);
		matchKinds.AddToTail(MatchKind_Manual);
	}
}

const char *GetMatchKindLabel(int kind)
{
	switch (kind)
	{
	case MatchKind_Command:
		return "cmd";
	case MatchKind_ConVar:
		return "cvar";
	case MatchKind_Manual:
		return "manual";
	default:
		return "?";
	}
}
}

void *GetAddressFromKeyValues(void *pBaseAddr, IGameConfig *pGameConfig, const char *key)
{
	const char *value = pGameConfig->GetKeyValue(key);
	if (!value)
		return nullptr;

	// Got a symbol here.
	if (value[0] == '@')
		return memutils->ResolveSymbol(pBaseAddr, &value[1]);

	// Convert hex signature to byte pattern
	unsigned char signature[200];
	size_t real_bytes = UTIL_DecodeHexString(signature, sizeof(signature), value);
	if (real_bytes < 1)
		return nullptr;

#ifdef _LINUX
	// The pointer returned by dlopen is not inside the loaded librarys memory region.
	struct link_map *dlmap = (struct link_map *)pBaseAddr;
	pBaseAddr = (void *)dlmap->l_addr;
#endif

	// Find that pattern in the pointed module.
	return memutils->FindPattern(pBaseAddr, (char *)signature, real_bytes);
}

bool AutoCompleteHook::SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);
	return true;
}

bool AutoCompleteHook::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
#if SOURCE_ENGINE >= SE_ORANGEBOX
	ICvar *cvarIface = static_cast<ICvar *>(g_SMAPI->VInterfaceMatch(g_SMAPI->GetEngineFactory(), CVAR_INTERFACE_VERSION));
	if (cvarIface == nullptr)
	{
		ke::SafeSprintf(error, maxlength, "Could not find interface: %s", CVAR_INTERFACE_VERSION);
		return false;
	}
	g_pCVar = cvarIface;
#endif

	IGameConfig *pGameConfig;
	char conf_error[255];
	if (!gameconfs->LoadGameConfigFile("autocomplete.games", &pGameConfig, conf_error, sizeof(conf_error)))
	{
		ke::SafeSprintf(error, maxlength, "Could not read autocomplete.games gamedata: %s", conf_error);
		return false;
	}

	void *dedicatedHandle = nullptr;
	if (!ResolveDedicatedLibrary(error, maxlength, &dedicatedHandle))
		return false;

	CDetourManager::Init(smutils->GetScriptingEngine(), nullptr);
	bool ok = EnableReceiveTabDetour(dedicatedHandle, pGameConfig, error, maxlength);
	ReleaseDedicatedLibrary(dedicatedHandle);
	gameconfs->CloseGameConfigFile(pGameConfig);
	if (!ok)
		return false;

	ConVar_Register(0, this);
	return ok;
}

void AutoCompleteHook::SDK_OnUnload()
{
	if (receiveTab)
		receiveTab->Destroy();

	ConVar_Unregister();
}

bool AutoCompleteHook::RegisterConCommandBase(ConCommandBase *pVar)
{
	return META_REGCVAR(pVar);
}

static ConCommand *FindAutoCompleteCommandFromPartial(const char *partial)
{
	char command[256];
	ke::SafeStrcpy(command, sizeof(command), partial);

	char *space = strchr(command, ' ');
	if (space)
	{
		*space = 0;
	}

	ConCommand *cmd = icvar->FindCommand(command);
	if (!cmd)
		return nullptr;

	if (!cmd->CanAutoComplete())
		return nullptr;

	return cmd;
}

static void GetSuggestions(const char *partial, const int numChars, CUtlVector<const char *> &matches, CUtlVector<int> &matchKinds)
{
	if (GetManualRootSuggestions(partial, matches, matchKinds))
		return;
	if (GetManualFileSuggestions(partial, matches, matchKinds))
		return;

	// Need to have this static, so the strings are still valid when added to |matches|
	static CUtlVector< CUtlString > commands;
	commands.Purge();

	// Already a space in the command, so try to display command completion suggestions.
	const char *space = strchr(partial, ' ');
	if (space)
	{
		ConCommand *pCommand = FindAutoCompleteCommandFromPartial(partial);
		if (!pCommand)
			return;

		int count = pCommand->AutoCompleteSuggest(partial, commands);
		for (int i = 0; i < count; i++)
		{
			matches.AddToTail(commands[i].String());
			matchKinds.AddToTail(MatchKind_Command);
		}
	}
	else
	{
		// Find all commands starting with the text typed in console yet
		ICvar::Iterator iter(icvar);
		for (iter.SetFirst(); iter.IsValid(); iter.Next())
		{
			ConCommandBase *cmd = iter.Get();
			if (cmd->IsFlagSet(FCVAR_DEVELOPMENTONLY) || cmd->IsFlagSet(FCVAR_HIDDEN))
				continue;

			if (!Q_strnicmp(partial, cmd->GetName(), numChars))
			{
				matches.AddToTail(cmd->GetName());
				matchKinds.AddToTail(cmd->IsCommand() ? MatchKind_Command : MatchKind_ConVar);
			}
		}
	}

	// Now sort the list by command name
	SortMatches(matches, &matchKinds);
}

static bool GetManualRootSuggestions(const char *partial, CUtlVector<const char *> &matches, CUtlVector<int> &matchKinds)
{
	const char *space = strchr(partial, ' ');
	if (!space)
		return false;

	size_t rootLength = static_cast<size_t>(space - partial);
	if (rootLength == 4 && !Q_strnicmp(partial, "meta", 4))
	{
		AddPrefixMatches(partial, kMetamodSubcommands, sizeof(kMetamodSubcommands) / sizeof(kMetamodSubcommands[0]), matches);
		for (int i = matchKinds.Count(); i < matches.Count(); i++)
			matchKinds.AddToTail(MatchKind_Manual);
		SortMatches(matches, &matchKinds);
		return matches.Count() > 0;
	}

	if (rootLength != 2 || strncmp(partial, "sm", 2) != 0)
		return false;

	const char *secondToken = space + 1;
	const char *secondSpace = strchr(secondToken, ' ');
	if (secondSpace != nullptr)
	{
		size_t secondTokenLength = static_cast<size_t>(secondSpace - secondToken);
		if (secondTokenLength == 4 && !Q_strnicmp(secondToken, "exts", 4))
		{
			AddPrefixMatches(partial, kSourceModExtsSubcommands, sizeof(kSourceModExtsSubcommands) / sizeof(kSourceModExtsSubcommands[0]), matches);
			for (int i = matchKinds.Count(); i < matches.Count(); i++)
				matchKinds.AddToTail(MatchKind_Manual);
			SortMatches(matches, &matchKinds);
			return matches.Count() > 0;
		}

		if (secondTokenLength == 7 && !Q_strnicmp(secondToken, "plugins", 7))
		{
			AddPrefixMatches(partial, kSourceModPluginsSubcommands, sizeof(kSourceModPluginsSubcommands) / sizeof(kSourceModPluginsSubcommands[0]), matches);
			for (int i = matchKinds.Count(); i < matches.Count(); i++)
				matchKinds.AddToTail(MatchKind_Manual);
			SortMatches(matches, &matchKinds);
			return matches.Count() > 0;
		}
	}

	AddPrefixMatches(partial, kSourceModSubcommands, sizeof(kSourceModSubcommands) / sizeof(kSourceModSubcommands[0]), matches);
	for (int i = matchKinds.Count(); i < matches.Count(); i++)
		matchKinds.AddToTail(MatchKind_Manual);
	SortMatches(matches, &matchKinds);

	return matches.Count() > 0;
}

static bool GetManualFileSuggestions(const char *partial, CUtlVector<const char *> &matches, CUtlVector<int> &matchKinds)
{
	for (size_t i = 0; i < sizeof(kSourceModPluginFileCommands) / sizeof(kSourceModPluginFileCommands[0]); i++)
	{
		const char *commandPrefix = kSourceModPluginFileCommands[i];
		if (StartsWithNoCase(partial, commandPrefix))
		{
			AddFileSuggestionsFromDirectory(partial, commandPrefix, "addons/sourcemod/plugins", ".smx", matches, matchKinds);
			SortMatches(matches, &matchKinds);
			return matches.Count() > 0;
		}
	}

	for (size_t i = 0; i < sizeof(kSourceModExtensionFileCommands) / sizeof(kSourceModExtensionFileCommands[0]); i++)
	{
		const char *commandPrefix = kSourceModExtensionFileCommands[i];
		if (StartsWithNoCase(partial, commandPrefix))
		{
#ifdef _WIN32
			AddFileSuggestionsFromDirectory(partial, commandPrefix, "addons/sourcemod/extensions", ".dll", matches, matchKinds);
#else
			AddFileSuggestionsFromDirectory(partial, commandPrefix, "addons/sourcemod/extensions", ".so", matches, matchKinds);
#endif
			SortMatches(matches, &matchKinds);
			return matches.Count() > 0;
		}
	}

	return false;
}

static void Echo(CTextConsole *tc, const char *msg)
{
	tc->Echo(msg);
}

static bool ResolveDedicatedLibrary(char *error, size_t maxlength, void **dedicatedHandle)
{
#ifdef WIN32
	ICommandLine *commandline = gamehelpers->GetValveCommandLine();
	if (commandline && !commandline->FindParm("-console"))
	{
		ke::SafeStrcpy(error, maxlength, "This extension is only needed when running in -console mode.");
		return false;
	}

	*dedicatedHandle = GetModuleHandleA("dedicated.dll");
#else
	*dedicatedHandle = dlopen("dedicated_srv.so", RTLD_LAZY);
	if (!*dedicatedHandle)
		*dedicatedHandle = dlopen("dedicated.so", RTLD_LAZY);
#endif

	if (!*dedicatedHandle)
	{
		ke::SafeStrcpy(error, maxlength, "Failed to find dedicated library in memory.");
		return false;
	}

	return true;
}

static void ReleaseDedicatedLibrary(void *dedicatedHandle)
{
#ifdef _LINUX
	if (dedicatedHandle)
		dlclose(dedicatedHandle);
#else
	(void)dedicatedHandle;
#endif
}

static bool EnableReceiveTabDetour(void *dedicatedHandle, IGameConfig *gameConfig, char *error, size_t maxlength)
{
	// This signature is the single runtime integration point we keep for L4D2.
	void *receiveTabAddr = GetAddressFromKeyValues(dedicatedHandle, gameConfig, "CTextConsole::ReceiveTab" KEY_SUFFIX);
	if (!receiveTabAddr)
	{
		ke::SafeStrcpy(error, maxlength, "Failed to find CTextConsole::ReceiveTab symbol in dedicated library");
		return false;
	}

	receiveTab = DETOUR_CREATE_MEMBER(DetourReceiveTab, receiveTabAddr);
	if (!receiveTab)
	{
		ke::SafeStrcpy(error, maxlength, "Failed to create detour for CTextConsole::ReceiveTab in dedicated library");
		return false;
	}

	receiveTab->EnableDetour();
	return true;
}

static bool InsertConsoleText(CTextConsole *tc, const char *msg, bool echo)
{
	if (!NormalizeConsoleTextState(tc))
		return false;

	if (echo)
		Echo(tc, msg);

	size_t currentLength = static_cast<size_t>(tc->m_nConsoleTextLen);
	size_t available = MAX_CONSOLE_TEXTLEN - 1 - currentLength;
	if (available == 0)
		return true;

	strncat(tc->m_szConsoleText, msg, available);
	tc->m_nConsoleTextLen = strlen(tc->m_szConsoleText);
	return true;
}

static bool NormalizeConsoleTextState(CTextConsole *tc)
{
	if (tc->m_nConsoleTextLen < 0 || tc->m_nConsoleTextLen >= MAX_CONSOLE_TEXTLEN)
		return false;

	// The engine-provided logical length is authoritative. Always terminate the
	// buffer there first so stale bytes after the active input line are ignored.
	tc->m_szConsoleText[tc->m_nConsoleTextLen] = '\0';
	tc->m_szConsoleText[MAX_CONSOLE_TEXTLEN - 1] = '\0';
	return true;
}

static void LogAutocompleteDebug(const char *fmt, ...)
{
	if (!sm_autocomplete_debug.GetBool())
		return;

	char buffer[512];
	va_list ap;
	va_start(ap, fmt);
	ke::SafeVsprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);
	smutils->LogMessage(myself, "[autocomplete] %s", buffer);
}

static void PrintSuggestionTable(CTextConsole *tc, const CUtlVector<const char *> &matches, const CUtlVector<int> &matchKinds, int consoleTextLen)
{
	int longestCmd = 0;
	for (int i = 0; i < matches.Count(); i++)
	{
		char display[256];
		ke::SafeSprintf(display, sizeof(display), "[%s] %s", GetMatchKindLabel(matchKinds[i]), matches[i]);
		const int cmdLength = strlen(display);
		if (cmdLength > longestCmd)
			longestCmd = cmdLength;
	}

	int totalColumns = (tc->GetWidth() - 1) / (longestCmd + 1);
	if (totalColumns < 1)
		totalColumns = 1;

	char longestCommonPrefix[MAX_CONSOLE_TEXTLEN];
	ke::SafeStrcpy(longestCommonPrefix, sizeof(longestCommonPrefix), matches[0]);
	int longestPrefixLength = strlen(longestCommonPrefix);
	int currentColumn = 0;

	Echo(tc, "\n");

	char formattedCommand[256];
	for (int i = 0; i < matches.Count(); i++)
	{
		const char *currentCmd = matches[i];
		currentColumn++;

		if (currentColumn > totalColumns)
		{
			Echo(tc, "\n");
			currentColumn = 1;
		}

		ke::SafeSprintf(formattedCommand, sizeof(formattedCommand), "[%s] %s", GetMatchKindLabel(matchKinds[i]), currentCmd);
		char paddedCommand[256];
		ke::SafeSprintf(paddedCommand, sizeof(paddedCommand), "%-*s ", longestCmd, formattedCommand);
		Echo(tc, paddedCommand);

		// Shrink the shared prefix as we walk the sorted suggestions.
		int overlapLength = strlen(currentCmd);
		if (overlapLength > longestPrefixLength)
			overlapLength = longestPrefixLength;

		int prefixIndex = 0;
		for (; prefixIndex < overlapLength; prefixIndex++)
		{
			if (currentCmd[prefixIndex] != longestCommonPrefix[prefixIndex])
				break;
		}

		longestPrefixLength = prefixIndex;
		longestCommonPrefix[prefixIndex] = 0;
	}

	if (longestPrefixLength > consoleTextLen)
	{
		InsertConsoleText(tc, &longestCommonPrefix[consoleTextLen], false);
	}
}

static int DoAutocompletion(CTextConsole *tc, const char *consoleText, int consoleTextLen)
{
	// See if any command matches the partial text
	CUtlVector<const char *> matches;
	CUtlVector<int> matchKinds;
	GetSuggestions(consoleText, consoleTextLen, matches, matchKinds);
	const int maxResults = sm_autocomplete_max_results.GetInt();
	if (matches.Count() > maxResults)
	{
		while (matches.Count() > maxResults)
		{
			matches.Remove(matches.Count() - 1);
			matchKinds.Remove(matchKinds.Count() - 1);
		}
	}
	LogAutocompleteDebug("input='%s' len=%d matches=%d", consoleText, consoleTextLen, matches.Count());
	for (int i = 0; i < matches.Count() && i < 8; i++)
	{
		LogAutocompleteDebug("match[%d]='%s' kind=%s", i, matches[i], GetMatchKindLabel(matchKinds[i]));
	}

	// No matches. Don't change the input line.
	if (matches.Count() == 0)
	{
		LogAutocompleteDebug("no matches");
		return 0;
	}

	// Exactly one match. Fill in the rest of the command on the command line.
	if (matches.Count() == 1)
	{
		const char * pszCmdName = matches[0];
		const char * pszRest = &pszCmdName[consoleTextLen];
		LogAutocompleteDebug("single match='%s' rest='%s'", pszCmdName, pszRest ? pszRest : "<null>");

		if (pszRest)
		{
			if (!InsertConsoleText(tc, pszRest, true))
				return 0;
			if (!InsertConsoleText(tc, " ", true))
				return 0;
		}

		return 1;
	}
	
	LogAutocompleteDebug("multiple matches, printing suggestion table");
	PrintSuggestionTable(tc, matches, matchKinds, consoleTextLen);
	return matches.Count();
}

static void __stdcall HandleReceiveTab(CTextConsole *tc)
{
	if (!NormalizeConsoleTextState(tc))
	{
		LogAutocompleteDebug("NormalizeConsoleTextState failed");
		return;
	}

	if (tc->m_nConsoleTextLen <= 0)
	{
		LogAutocompleteDebug("ignored empty console text");
		return;
	}

	// This replaces the dedicated server's default Tab handling.
	int matchCount = DoAutocompletion(tc, tc->m_szConsoleText, tc->m_nConsoleTextLen);
	LogAutocompleteDebug("HandleReceiveTab completed with matchCount=%d", matchCount);
	if (matchCount == 0)
		return;

	// Multiple possible matches shown.
	if (matchCount > 1)
	{
		// Provide the entered console text again for further editing.
		Echo(tc, "\n");
		Echo(tc, tc->m_szConsoleText);
	}

	tc->m_nCursorPosition = tc->m_nConsoleTextLen;
	tc->m_nBrowseLine = tc->m_nInputLine;
}

// From SM's stringutil.cpp
size_t UTIL_DecodeHexString(unsigned char *buffer, size_t maxlength, const char *hexstr)
{
	size_t written = 0;
	size_t length = strlen(hexstr);

	for (size_t i = 0; i < length; i++)
	{
		if (written >= maxlength)
			break;
		buffer[written++] = hexstr[i];
		if (hexstr[i] == '\\' && (i + 1) < length && hexstr[i + 1] == 'x')
		{
			if (i + 3 >= length)
				continue;
			/* Get the hex part. */
			char s_byte[3];
			int r_byte;
			s_byte[0] = hexstr[i + 2];
			s_byte[1] = hexstr[i + 3];
			s_byte[2] = '\0';
			/* Read it as an integer */
			sscanf(s_byte, "%x", &r_byte);
			/* Save the value */
			buffer[written - 1] = r_byte;
			/* Adjust index */
			i += 3;
		}
	}

	return written;
}
