#ifndef _TEXTCONSOLE_H_
#define _TEXTCONSOLE_H_
#pragma once

#define MAX_CONSOLE_TEXTLEN 256
#define MAX_BUFFER_LINES 30

/**
 * Minimal local reconstruction of the dedicated server's CTextConsole type for
 * L4D2. This type is not exposed as a stable public SDK interface, so this
 * header intentionally models only the subset required by srcds-autocomplete.
 *
 * Members used by this extension:
 * - Echo()
 * - GetWidth()
 * - m_szConsoleText
 * - m_nConsoleTextLen
 * - m_nCursorPosition
 * - m_nInputLine
 * - m_nBrowseLine
 *
 * The remaining fields are preserved only to keep the observed L4D2 layout
 * aligned with the code path that detours CTextConsole::ReceiveTab.
 */
class CTextConsole
{
public:
	virtual ~CTextConsole() = 0;
	virtual bool Init() = 0;
	virtual void ShutDown() = 0;
	virtual void Print(char *msg) = 0;
	virtual void SetTitle(char *title) = 0;
	virtual void SetStatusLine(char *status) = 0;
	virtual void UpdateStatus() = 0;
	virtual void Echo(const char *msg, int len = 0) = 0;
	virtual char *GetLine() = 0;
	virtual int GetWidth() = 0;
	virtual void SetVisible(bool visible) = 0;
	virtual bool IsVisible() = 0;

public:
	// Active editable input line shown in the SRCDS console.
	char m_szConsoleText[MAX_CONSOLE_TEXTLEN];
	int m_nConsoleTextLen;
	int m_nCursorPosition;

	// Saved input line while browsing console history.
	char m_szSavedConsoleText[MAX_CONSOLE_TEXTLEN];
	int m_nSavedConsoleTextLen;

	// History ring and browse state maintained by the dedicated console.
	char m_aszLineBuffer[MAX_BUFFER_LINES][MAX_CONSOLE_TEXTLEN];
	int m_nInputLine;
	int m_nBrowseLine;
	int m_nTotalLines;

	bool m_ConsoleVisible;
};

#endif // _TEXTCONSOLE_H_
