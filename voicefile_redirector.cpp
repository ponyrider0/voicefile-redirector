#include <StdAfx.h>
#include "obse/PluginAPI.h"
#include "obse/CommandTable.h"
#include "obse_common/SafeWrite.h"
#include "obse/GameData.h"
//#include "shlwapi.h" -- previously needed for PathFileExists()

#if OBLIVION
#include "obse/GameAPI.h"

/*	
As of 0020, ExtractArgsEx() and ExtractFormatStringArgs() are no longer directly included in plugin builds.
They are available instead through the OBSEScriptInterface.
To make it easier to update plugins to account for this, the following can be used.
It requires that g_scriptInterface is assigned correctly when the plugin is first loaded.
*/
#define ENABLE_EXTRACT_ARGS_MACROS 1	// #define this as 0 if you prefer not to use this

#if ENABLE_EXTRACT_ARGS_MACROS

OBSEScriptInterface * g_scriptInterface = NULL;	// make sure you assign to this
#define ExtractArgsEx(...) g_scriptInterface->ExtractArgsEx(__VA_ARGS__)
#define ExtractFormatStringArgs(...) g_scriptInterface->ExtractFormatStringArgs(__VA_ARGS__)

#endif

#else
#include "obse_editor/EditorAPI.h"
#endif

#include "obse/ParamInfos.h"
#include "obse/Script.h"
#include "obse/GameObjects.h"
#include <string>

PluginHandle				g_pluginHandle = kPluginHandle_Invalid;
OBSESerializationInterface	* g_serialization = NULL;
OBSEArrayVarInterface		* g_arrayIntfc = NULL;
OBSEScriptInterface			* g_scriptIntfc = NULL;
OBSEMessagingInterface*		g_msg;
// (already in GameData.h/.cpp) FileFinder** g_FileFinder = (FileFinder**)0xB33A04;

IDebugLog		gLog("voicefile_redirector.log");

static const UInt32 loc_59EC65 = 0x0059EC65;
static const UInt32 SilentVoiceHookPatchAddr = 0x006A9EB7;
static const UInt32 SilentVoiceHookRetnAddr = 0x006A9ECE;
static const UInt32 DialogSubtitleHookPatchAddr = 0x0059EC5F;
static const UInt32 DialogSubtitleHookRetnAddr = 0x0059EC65;
static const UInt32 GeneralSubtitleHookPatchAddr = 0x0062AE3C;
static const UInt32 GeneralSubtitleHookRetnAddr = 0x0062AE43;
static const UInt32 LipsHookPatchAddr = 0x0049411F;
static const UInt32 LipsHookRetnAddr = 0x00494125;

//static const UInt32 CheckFileSub = 0x00431020;
//typedef UInt32 (*_CheckFileSub)(char*, byte, byte, int);
//static const _CheckFileSub CheckFileSub = (_CheckFileSub) 0x00431020;

#define MAX_VOICENAME 256

static void OverWriteSoundFile(void);
static char *pSoundFile;
static char *SilentVoiceMp3 = "Data\\OBSE\\Plugins\\elys_usv.mp3";
static char *SilentVoiceLip = "Data\\OBSE\\Plugins\\Elys_USV";
static char NewSoundFile[MAX_VOICENAME];
std::string g_NewVoiceFilename;
//static char *SoundFile = "Data\\sound\\voice\\morrowind_ob.esm\\breton\\m\\fbmwchargen_greeting_00000f48_1.mp3";

static byte DialogSubtitle;
static byte GeneralSubtitle;

static char *pLipFile;
static char TmpLip[MAX_VOICENAME];
static char *pTmpLip;

const char* strstr_caseinsensitive(const char* mainstring, const char* searchterm)
{
	int i, j, k;

	for (i = 0; mainstring[i]; i++)
	{
		for (j = i, k = 0; tolower(mainstring[j]) == tolower(searchterm[k]); j++, k++)
		{
			if (searchterm[k + 1] == NULL)
				return (mainstring + i);
		}
	}
	return NULL;
}

// try to detect if this is a greeting
bool IsGreeting(const char *voicefilename)
{
	const char *resultstr;

	resultstr = strstr_caseinsensitive(voicefilename, "_greeting_");
	if (resultstr != nullptr)
	{
		resultstr = strstr_caseinsensitive(voicefilename, "_1.mp3");
		if (resultstr != nullptr)
			return true;
		else
			return false;
	}
	else
		return false;
}

// fallback to generic greeting from Oblivion
void ReplaceGreeting(char *voicefilename)
{
	const char *resultstr;


	// find race\sex, use hello from that race\sex
	resultstr = strstr_caseinsensitive(voicefilename, "argonian\\f");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\argonian\\f\\GenericKhajiit_HELLO_00062CCB_1.mp3");
		return;
	}
	resultstr = strstr_caseinsensitive(voicefilename, "argonian\\m");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Oblivion.esm\\argonian\\m\\GenericKhajiit_HELLO_00062CCB_1.mp3");
		return;
	}
	resultstr = strstr_caseinsensitive(voicefilename, "breton\\m");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\breton\\m\\GenericBreton_HELLO_00062CC7_1.mp3");
		return;
	}
	resultstr = strstr_caseinsensitive(voicefilename, "high elf\\f");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\high elf\\f\\GenericHighElf_HELLO_00062CAA_1.mp3");
		return;
	}
	resultstr = strstr_caseinsensitive(voicefilename, "high elf\\m");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\high elf\\m\\GenericHighElf_HELLO_00062CAA_1.mp3");
		return;
	}
	resultstr = strstr_caseinsensitive(voicefilename, "nord\\f");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\nord\\f\\GenericNord_HELLO_00062CB3_1.mp3");
		return;
	}
	resultstr = strstr_caseinsensitive(voicefilename, "nord\\m");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\nord\\m\\GenericNord_HELLO_00062CB3_1.mp3");
		return;
	}
	resultstr = strstr_caseinsensitive(voicefilename, "redguard\\f");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\redguard\\f\\GenericRedguard_HELLO_00062CC1_1.mp3");
		return;
	}
	resultstr = strstr_caseinsensitive(voicefilename, "redguard\\m");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\redguard\\m\\GenericRedguard_HELLO_00062CC1_1.mp3");
		return;
	}
	// if nothing found, just use imperial\m or \f
	resultstr = strstr_caseinsensitive(voicefilename, "\\f\\");
	if (resultstr != nullptr)
	{
		strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\imperial\\f\\GenericImperial_HELLO_000919A9_1.mp3");
		return;
	}
	// default to m for everything else
	strcpy(voicefilename, "Data\\Sound\\Voice\\Oblivion.esm\\imperial\\m\\GenericImperial_HELLO_000919A9_1.mp3");
	return;

}

// try to fallback to imperial voice set if available
// return true if imperial voice set available
bool FallbackToRace(char *voicefilename, const char *racestring)
{
	char tempstring[MAX_VOICENAME];
	const char *resultstr;
	char *startoffset;
	char *stopoffset;
	int racestr_len;

	// search for argonian and replace with racestring
	resultstr = strstr_caseinsensitive(voicefilename, "\\argonian\\");
	if (resultstr != nullptr)
	{
		// save start and stop offsets
		startoffset = (char*)(resultstr + 1);
		stopoffset = (char*)(startoffset + 8);
	}
	else
	{
		resultstr = strstr_caseinsensitive(voicefilename, "\\breton\\");
		if (resultstr != nullptr)
		{
			// save start and stop offsets
			startoffset = (char*)(resultstr + 1);
			stopoffset = (char*)(startoffset + 6);
		}
		else
		{
			resultstr = strstr_caseinsensitive(voicefilename, "\\high elf\\");
			if (resultstr != nullptr)
			{
				// save start and stop offsets
				startoffset = (char*)(resultstr + 1);
				stopoffset = (char*)(startoffset + 8);
			}
			else
			{
				resultstr = strstr_caseinsensitive(voicefilename, "\\nord\\");
				if (resultstr != nullptr)
				{
					// save start and stop offsets
					startoffset = (char*)(resultstr + 1);
					stopoffset = (char*)(startoffset + 4);
				}
				else
				{
					resultstr = strstr_caseinsensitive(voicefilename, "\\redguard\\");
					if (resultstr != nullptr)
					{
						// save start and stop offsets
						startoffset = (char*)(resultstr + 1);
						stopoffset = (char*)(startoffset + 8);
					}
					else
					{
						return false;
					}
				}
			}
		}
	}
	// shift stopoffset to accommadate racestring
	racestr_len = strlen(racestring);
	// place the bottom half of the pathname into a new string so that there is no memory corruption when shifting
	strcpy(tempstring, stopoffset);
	strcpy(startoffset + racestr_len, tempstring);
	strncpy(startoffset, racestring, racestr_len);
	
//	_MESSAGE("voicefile_redirector: Race Fallback: looking for fallback file: '%s'", &voicefilename[5]);
	if ((*g_FileFinder)->FindFile(&voicefilename[5], 0, 0, -1) == 0)
	{
		return false;
	}

	return true;

}

static __declspec(naked) void SilentVoiceHook(void)
{
	__asm
	{
		mov ecx, esp
		add ecx, 0x64
		mov pSoundFile, ecx
		push eax
		push ebx
		push edx
		call OverWriteSoundFile
		pop edx
		pop ebx
		pop eax
		jmp[SilentVoiceHookRetnAddr]
	}
}

//static char rawdata1[] = { 0x38, 0x0D, 0x00, 0x32, 0xB1, 0x00 };
static __declspec(naked) void DialogSubtitleHook(void)
{
	__asm
	{
		cmp DialogSubtitle, cl
		je No_change
		jmp Jump_back

No_change:
//		db 0x38, 0x0D, 0x00, 0x32, 0xB1, 0x00
		_emit 0x38
		_emit 0x0D
		_emit 0x00
		_emit 0x32
		_emit 0xB1
		_emit 0x00

Jump_back:
		mov DialogSubtitle, 0
		mov GeneralSubtitle, 0
		jmp [DialogSubtitleHookRetnAddr]
	}
}

//static char rawdata2[] = { 0x80, 0x3D, 0x08, 0x32, 0xB1, 0x00, 0x00 };
static __declspec(naked) void GeneralSubtitleHook(void)
{
	__asm
	{
		cmp GeneralSubtitle, 0
		je No_change
		jmp Jump_back

No_change:
//		db 0x80, 0x3D, 0x08, 0x32, 0xB1, 0x00, 0x00
		_emit 0x80
		_emit 0x3D
		_emit 0x08
		_emit 0x32
		_emit 0xB1
		_emit 0x00
		_emit 0x00

Jump_back:
		mov DialogSubtitle, 0
		mov GeneralSubtitle, 0
		jmp[GeneralSubtitleHookRetnAddr]
	}
}

/*
static int CheckFile(void)
{
	_MESSAGE("voicefile_redirector: Calling internal function CheckFile() with '%s'", pTmpLip);

// calling CheckFileSub crashes to desktop
	_asm
	{
		mov ecx, [0xB33A04];
		push -1
		push 0
		push 0
		push pTmpLip
		call CheckFileSub
	}
}
*/

static void CheckLipFile(void)
{
	int stringlength;

//	_MESSAGE("voicefile_redirector: raw Lip file = '%s'", pLipFile);
	strncpy(TmpLip, pLipFile, strlen(pLipFile) - 3);
	strcpy(TmpLip+strlen(pLipFile)-3, "lip");
	pTmpLip = &TmpLip[5];
//	_MESSAGE("voicefile_redirector: now searching for pTmpLip = '%s'", pTmpLip);
	//	if (CheckFile() == 0)
	if ((*g_FileFinder)->FindFile(pTmpLip, 0, 0, -1) == 0)
	{
//		_MESSAGE("voicefile_redirector: LipFile '%s' not found, proceeding with fallback cascade", pTmpLip);
		// file not found, proceed with fallback cascade:

		// check if it can fallback to imperial race
		strcpy(TmpLip, pLipFile);
	//	_MESSAGE("voicefile_redirector: testing '%s' with race fallback", TmpLip);
		if (FallbackToRace(TmpLip, "imperial") == true)
		{
			TmpLip[strlen(TmpLip) - 4] = '\0';
		}
		// Check if it is a greeting
		else if (IsGreeting(pLipFile) == true)
		{
			strcpy(TmpLip, pLipFile);
			ReplaceGreeting(TmpLip);
			TmpLip[strlen(TmpLip)-4] = '\0';
		}
		else
		{
			// no more fallbacks found, use silent voice
			strcpy(TmpLip, SilentVoiceLip);
		}
		// add 1 character to stringlength comparison to account for null character
		stringlength = strlen(TmpLip);
		if (stringlength + 1 > MAX_VOICENAME)
		{
			_MESSAGE("voicefile_redirector: ERROR, filename '%s' is too long. Aborting replacement operation.", TmpLip);
			return;
		}
		// BugFix for lip files: for some reason, doing strcpy is not working, so reverting back to old method of
		// redirecting to DLL's internal global variable for lipfilename.
		_MESSAGE("voicefile_redirector: Lips Hook: replacing '%s' with '%s'", pLipFile, TmpLip);
		pLipFile = &TmpLip[0];
//		strcpy(pLipFile, TmpLip);
	}
	else
	{
//		_MESSAGE("voicefile_redirector: .LIP file FOUND, using original file '%s'", pLipFile);
	}
}

static __declspec(naked) void LipsHook(void)
{
	__asm
	{
		mov pLipFile, ecx
		push eax
		push ebx
		push edx
		call CheckLipFile
		pop edx
		pop ebx
		pop eax
		mov ecx, pLipFile

		push ecx
		push 0x00A3D9A8
		jmp[LipsHookRetnAddr]
	}
}

static void OverWriteSoundFile(void)
{
	int stringlength;

	// voice file not found, proceed with fallback mechanism
//	_MESSAGE("voicefile_redirector: Silent Voice Hook activated on '%s'", pSoundFile);

	// check if fallback to imperial race is available
	strcpy(NewSoundFile, pSoundFile);
	if (FallbackToRace(NewSoundFile, "imperial") == true)
	{
		// nothing to do, ready for transfer
	}
	// check if it is a greeting
	else if (IsGreeting(pSoundFile) == true)
	{
		strcpy(NewSoundFile, pSoundFile);
		ReplaceGreeting(NewSoundFile);
	}
	else
	{
		// no other fallbacks found, replace with silent voice mp3
		strcpy(NewSoundFile, SilentVoiceMp3);
	}
	stringlength = strlen(NewSoundFile);
	if (stringlength + 1 > MAX_VOICENAME)
	{
		_MESSAGE("voicefile_redirector: ERROR: SilentVoiceHook: filename '%s' is too long. Aborting replacement operation.", NewSoundFile);
		return;
	}
	_MESSAGE("voicefile_redirector: Silent Voice Hook: replacing '%s' with '%s'", pSoundFile, NewSoundFile);
	strcpy(pSoundFile, NewSoundFile);
	DialogSubtitle = 1;
	GeneralSubtitle = 1;
}

/*
#if OBLIVION
bool Cmd_TestUSV_Execute(COMMAND_ARGS)
{
	_MESSAGE("voicefile_redirector");

	*result = 42;

	_MESSAGE("voicefile_redirector: executing test console command.");
	Console_Print("voicefile_redirector console command executed");

	return true;
}
#endif

static CommandInfo kPluginTestCommand =
{
	"testusv",
	"",
	0,
	"test command for obse plugin",
	0,		// requires parent obj
	0,		// doesn't have params
	NULL,	// no param table

	HANDLER(Cmd_TestUSV_Execute)
};
*/

void MessageHandler(OBSEMessagingInterface::Message* msg)
{
	switch (msg->type)
	{
	case OBSEMessagingInterface::kMessage_ExitGame:
		_MESSAGE("voicefile_redirector: received ExitGame message");
		break;
	case OBSEMessagingInterface::kMessage_ExitToMainMenu:
//		_MESSAGE("voicefile_redirector: received ExitToMainMenu message");
		break;
	case OBSEMessagingInterface::kMessage_PostLoad:
//		_MESSAGE("voicefile_redirector: received PostLoad mesage");
		break;
	case OBSEMessagingInterface::kMessage_LoadGame:
	case OBSEMessagingInterface::kMessage_SaveGame:
//		_MESSAGE("voicefile_redirector: received save/load message with file path %s", msg->data);
		break;
	case OBSEMessagingInterface::kMessage_Precompile:
	{
//		ScriptBuffer* buffer = (ScriptBuffer*)msg->data;
//		_MESSAGE("voicefile_redirector: received precompile message. Script Text:\n%s", buffer->scriptText);
		break;
	}
	case OBSEMessagingInterface::kMessage_PreLoadGame:
//		_MESSAGE("voicefile_redirector: received pre-loadgame message with file path %s", msg->data);
		break;
	case OBSEMessagingInterface::kMessage_ExitGame_Console:
//		_MESSAGE("voicefile_redirector: received quit game from console message");
		break;
	default:
		_MESSAGE("voicefile_redirector: received unknown message");
		break;
	}
}

extern "C" {

	bool OBSEPlugin_Query(const OBSEInterface * obse, PluginInfo * info)
	{
		_MESSAGE("voicefile_redirector: OBSE calling plugin's Query function.");

		// fill out the info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "voicefile_redirector";
		info->version = 1;

		// version checks
		if (!obse->isEditor)
		{
			if (obse->obseVersion < OBSE_VERSION_INTEGER)
			{
				_ERROR("OBSE version too old (got %08X expected at least %08X)", obse->obseVersion, OBSE_VERSION_INTEGER);
				return false;
			}

#if OBLIVION
			if (obse->oblivionVersion != OBLIVION_VERSION)
			{
				_ERROR("incorrect Oblivion version (got %08X need %08X)", obse->oblivionVersion, OBLIVION_VERSION);
				return false;
			}
#endif

			g_serialization = (OBSESerializationInterface *)obse->QueryInterface(kInterface_Serialization);
			if (!g_serialization)
			{
				_ERROR("serialization interface not found");
				return false;
			}

			if (g_serialization->version < OBSESerializationInterface::kVersion)
			{
				_ERROR("incorrect serialization version found (got %08X need %08X)", g_serialization->version, OBSESerializationInterface::kVersion);
				return false;
			}

			g_arrayIntfc = (OBSEArrayVarInterface*)obse->QueryInterface(kInterface_ArrayVar);
			if (!g_arrayIntfc)
			{
				_ERROR("Array interface not found");
				return false;
			}

			g_scriptIntfc = (OBSEScriptInterface*)obse->QueryInterface(kInterface_Script);
		}
		else
		{
			// no version checks needed for editor
			_MESSAGE("voicefile_redirector: not compatible with editor, exiting.");
			return false;

		}

		// version checks pass

		return true;
	}

	bool OBSEPlugin_Load(const OBSEInterface * obse)
	{
		_MESSAGE("voicefile_redirector: OBSE calling plugin's Load function.");

		g_pluginHandle = obse->GetPluginHandle();

		// Hook memory address for silent voice
		WriteRelJump(SilentVoiceHookPatchAddr, (UInt32)&SilentVoiceHook);
		_MESSAGE("voicefile_redirector: memory address for SilentVoiceHook patched.");

		// Dialog Subtitle Hook
		WriteRelJump(DialogSubtitleHookPatchAddr, (UInt32)&DialogSubtitleHook);
		_MESSAGE("voicefile_redirector: memory address for DialogSubtitlesHook patched.");

		// General Subtitle Hook
		WriteRelJump(GeneralSubtitleHookPatchAddr, (UInt32)&GeneralSubtitleHook);
		_MESSAGE("voicefile_redirector: memory address for GeneralSubtitlesHook patched.");

		// Lips Hook
		WriteRelJump(LipsHookPatchAddr, (UInt32)&LipsHook);
		_MESSAGE("voicefile_redirector: memory address for LipsHook patched.");

		// register commands
//		obse->SetOpcodeBase(0x2000);
//		obse->RegisterCommand(&kPluginTestCommand);

		// set up serialization callbacks when running in the runtime
		if (!obse->isEditor)
		{
			// register to use string var interface
			// this allows plugin commands to support '%z' format specifier in format string arguments
			OBSEStringVarInterface* g_Str = (OBSEStringVarInterface*)obse->QueryInterface(kInterface_StringVar);
			g_Str->Register(g_Str);

			// get an OBSEScriptInterface to use for argument extraction
			g_scriptInterface = (OBSEScriptInterface*)obse->QueryInterface(kInterface_Script);
		}

		// register to receive messages from OBSE
		OBSEMessagingInterface* msgIntfc = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
		msgIntfc->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
		g_msg = msgIntfc;

		// get command table, if needed
		OBSECommandTableInterface* cmdIntfc = (OBSECommandTableInterface*)obse->QueryInterface(kInterface_CommandTable);
		if (cmdIntfc) {
#if 0	// enable the following for loads of log output
			for (const CommandInfo* cur = cmdIntfc->Start(); cur != cmdIntfc->End(); ++cur) {
				_MESSAGE("%s", cur->longName);
			}
#endif
		}
		else {
			_MESSAGE("Couldn't read command table");
		}

		return true;
	}

};
