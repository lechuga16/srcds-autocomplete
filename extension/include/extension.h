#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

#include "smsdk_ext.h"

#if SOURCE_ENGINE != SE_LEFT4DEAD2
# error "srcds-autocomplete is now maintained as an L4D2-only extension."
#endif

// L4D2 ships a CTextConsole::ReceiveTab implementation on both Windows and Linux.
#define DETOUR_RECEIVE_TAB

// Keyvalues in gamedata are not platform-specific, so we still distinguish suffixes manually.
#ifdef WIN32
# define KEY_SUFFIX "_win"
#else
# define KEY_SUFFIX "_lin"
#endif


class AutoCompleteHook : public SDKExtension,
						 public IConCommandBaseAccessor
{
public:
	virtual bool SDK_OnLoad(char *error, size_t maxlength, bool late);
	virtual void SDK_OnUnload();
	bool RegisterConCommandBase(ConCommandBase *pVar) override;
public:
#if defined SMEXT_CONF_METAMOD
	virtual bool SDK_OnMetamodLoad(ISmmAPI *ismm, char *error, size_t maxlength, bool late);
#endif
};

size_t UTIL_DecodeHexString(unsigned char *buffer, size_t maxlength, const char *hexstr);

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
