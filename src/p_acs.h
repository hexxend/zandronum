/*
** p_acs.h
** ACS script stuff
**
**---------------------------------------------------------------------------
** Copyright 1998-2012 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __P_ACS_H__
#define __P_ACS_H__

#include "dobject.h"
#include "dthinker.h"
#include "doomtype.h"
// [BB] New #includes.
#include "r_data/r_translate.h"
#include <algorithm>
#include <iterator>
#include "i_system.h"
#include "sv_commands.h"

#define LOCAL_SIZE				20
#define NUM_MAPVARS				128

class FFont;
class FileReader;


enum
{
	NUM_WORLDVARS = 256,
	NUM_GLOBALVARS = 64
};

struct InitIntToZero
{
	void Init(int &v)
	{
		v = 0;
	}
};
typedef TMap<SDWORD, SDWORD, THashTraits<SDWORD>, InitIntToZero> FWorldGlobalArray;

// Type of elements count is unsigned int instead of size_t to match ACSStringPool interface
template <typename T, unsigned int N>
struct BoundsCheckingArray
{
	T &operator[](const unsigned int index)
	{
		if (index >= N)
		{
			I_Error("Out of bounds memory access in ACS VM");
		}

		return buffer[index];
	}

	T *Pointer() { return buffer; }
	unsigned int Size() const { return N; }

	void Fill(const T &value) { std::fill(std::begin(buffer), std::end(buffer), value); }

private:
	T buffer[N];
};

// ACS variables with global scope
extern BoundsCheckingArray<SDWORD, NUM_GLOBALVARS> ACS_GlobalVars;
extern BoundsCheckingArray<FWorldGlobalArray, NUM_GLOBALVARS> ACS_GlobalArrays;

#define LIBRARYID_MASK			0xFFF00000
#define LIBRARYID_SHIFT			20

// Global ACS string table
#define STRPOOL_LIBRARYID		(INT_MAX >> LIBRARYID_SHIFT)
#define STRPOOL_LIBRARYID_OR	(STRPOOL_LIBRARYID << LIBRARYID_SHIFT)

// [TP]
namespace ServerCommands
{
	class ReplaceTextures;
};

class ACSStringPool
{
public:
	ACSStringPool();
	int AddString(const char *str);
	int AddString(FString &str);
	const char *GetString(int strnum);
	void LockString(int strnum);
	void UnlockString(int strnum);
	void UnlockAll();
	void MarkString(int strnum);
	void LockStringArray(const int *strnum, unsigned int count);
	void UnlockStringArray(const int *strnum, unsigned int count);
	void MarkStringArray(const int *strnum, unsigned int count);
	void MarkStringMap(const FWorldGlobalArray &array);
	void PurgeStrings();
	void Clear();
	void Dump() const;
	void ReadStrings(PNGHandle *png, DWORD id);
	void WriteStrings(FILE *file, DWORD id) const;

private:
	int FindString(const char *str, size_t len, unsigned int h, unsigned int bucketnum);
	int InsertString(FString &str, unsigned int h, unsigned int bucketnum);
	void FindFirstFreeEntry(unsigned int base);

	enum { NUM_BUCKETS = 251 };
	enum { FREE_ENTRY = 0xFFFFFFFE };	// Stored in PoolEntry's Next field
	enum { NO_ENTRY = 0xFFFFFFFF };
	enum { MIN_GC_SIZE = 100 };			// Don't auto-collect until there are this many strings
	struct PoolEntry
	{
		FString Str;
		unsigned int Hash;
		unsigned int Next;
		unsigned int LockCount;
	};
	TArray<PoolEntry> Pool;
	unsigned int PoolBuckets[NUM_BUCKETS];
	unsigned int FirstFreeEntry;
};
extern ACSStringPool GlobalACSStrings;

void P_CollectACSGlobalStrings();
void P_ReadACSVars(PNGHandle *);
void P_WriteACSVars(FILE*);
void P_ClearACSVars(bool);
void P_SerializeACSScriptNumber(FArchive &arc, int &scriptnum, bool was2byte);

struct ACSProfileInfo
{
	unsigned long long TotalInstr;
	unsigned int NumRuns;
	unsigned int MinInstrPerRun;
	unsigned int MaxInstrPerRun;

	ACSProfileInfo();
	void AddRun(unsigned int num_instr);
	void Reset();
};

struct ProfileCollector
{
	ACSProfileInfo *ProfileData;
	class FBehavior *Module;
	int Index;
};

class ACSLocalVariables
{
public:
	ACSLocalVariables(SDWORD *Memory, size_t Count)
	: memory(Memory)
	, count(Count)
	{
	}

	void Reset(SDWORD *const memory, const size_t count)
	{
		// TODO: pointer sanity check?
		// TODO: constraints on count?

		this->memory = memory;
		this->count = count;
	}

	SDWORD& operator[](const size_t index)
	{
		if (index >= count)
		{
			I_Error("Out of bounds access to local variables in ACS VM");
		}

		return memory[index];
	}

	const SDWORD *GetPointer() const
	{
		return memory;
	}

private:
	SDWORD *memory;
	size_t count;
};

struct ACSLocalArrayInfo
{
	unsigned int Size;
	int Offset;
};

struct ACSLocalArrays
{
	unsigned int Count;
	ACSLocalArrayInfo *Info;

	ACSLocalArrays()
	{
		Count = 0;
		Info = NULL;
	}
	~ACSLocalArrays()
	{
		if (Info != NULL)
		{
			delete[] Info;
			Info = NULL;
		}
	}

	// Bounds-checking Set and Get for local arrays
	void Set(ACSLocalVariables &locals, int arraynum, int arrayentry, int value)
	{
		if ((unsigned int)arraynum < Count &&
			(unsigned int)arrayentry < Info[arraynum].Size)
		{
			locals[Info[arraynum].Offset + arrayentry] = value;
		}
	}
	int Get(ACSLocalVariables &locals, int arraynum, int arrayentry)
	{
		if ((unsigned int)arraynum < Count &&
			(unsigned int)arrayentry < Info[arraynum].Size)
		{
			return locals[Info[arraynum].Offset + arrayentry];
		}
		return 0;
	}
};

// [TDRR] Required context to allow native functions to access local
// variables (but most importantly arrays).
struct ACSLocals
{
	ACSLocalVariables *Vars;
	ACSLocalArrays *Arrays;
};

// The in-memory version
struct ScriptPtr
{
	int Number;
	DWORD Address;
	BYTE Type;
	BYTE ArgCount;
	WORD VarCount;
	WORD Flags;
	ACSLocalArrays LocalArrays;

	ACSProfileInfo ProfileData;
};

// The present ZDoom version
struct ScriptPtr3
{
	SWORD Number;
	BYTE Type;
	BYTE ArgCount;
	DWORD Address;
};

// The intermediate ZDoom version
struct ScriptPtr1
{
	SWORD Number;
	WORD Type;
	DWORD Address;
	DWORD ArgCount;
};

// The old Hexen version
struct ScriptPtr2
{
	DWORD Number;	// Type is Number / 1000
	DWORD Address;
	DWORD ArgCount;
};

struct ScriptFlagsPtr
{
	WORD Number;
	WORD Flags;
};

struct ScriptFunctionInFile
{
	BYTE ArgCount;
	BYTE LocalCount;
	BYTE HasReturnValue;
	BYTE ImportNum;
	DWORD Address;
};

struct ScriptFunction
{
	BYTE ArgCount;
	BYTE HasReturnValue;
	BYTE ImportNum;
	int  LocalCount;
	DWORD Address;
	ACSLocalArrays LocalArrays;
};

// Script types
enum
{
	SCRIPT_Closed		= 0,
	SCRIPT_Open			= 1,
	SCRIPT_Respawn		= 2,
	SCRIPT_Death		= 3,
	SCRIPT_Enter		= 4,
	SCRIPT_Pickup		= 5,
	SCRIPT_BlueReturn	= 6,
	SCRIPT_RedReturn	= 7,
	SCRIPT_WhiteReturn	= 8,
	SCRIPT_Lightning	= 12,
	SCRIPT_Unloading	= 13,
	SCRIPT_Disconnect	= 14,
	SCRIPT_Return		= 15,
	SCRIPT_Event		= 16, // [BB]
	SCRIPT_Kill			= 17, // [JM]
};

// Script flags
enum
{
	SCRIPTF_Net = 0x0001,	// Safe to "puke" in multiplayer
	SCRIPTF_ClientSide = 0x0002	// [BB] Is executed on the clients, not on the server.
};

enum ACSFormat { ACS_Old, ACS_Enhanced, ACS_LittleEnhanced, ACS_Unknown };

// [AK] Moved all HUD message definitions here from p_acs.cpp
// HUD message flags
#define HUDMSG_LOG					(0x80000000)
#define HUDMSG_COLORSTRING			(0x40000000)
#define HUDMSG_ADDBLEND				(0x20000000)
#define HUDMSG_ALPHA				(0x10000000)
#define HUDMSG_NOWRAP				(0x08000000)

// HUD message layers; these are not flags
#define HUDMSG_LAYER_SHIFT			12
#define HUDMSG_LAYER_MASK			(0x0000F000)
// See HUDMSGLayer enumerations in sbar.h

// HUD message visibility flags
#define HUDMSG_VISIBILITY_SHIFT		16
#define HUDMSG_VISIBILITY_MASK		(0x00070000)
// See HUDMSG visibility enumerations in sbar.h

// [BB] Moved here from p_acs.cpp
enum
{
	APROP_Health		= 0,
	APROP_Speed			= 1,
	APROP_Damage		= 2,
	APROP_Alpha			= 3,
	APROP_RenderStyle	= 4,
	APROP_SeeSound		= 5,	// Sounds can only be set, not gotten
	APROP_AttackSound	= 6,
	APROP_PainSound		= 7,
	APROP_DeathSound	= 8,
	APROP_ActiveSound	= 9,
	APROP_Ambush		= 10,
	APROP_Invulnerable	= 11,
	APROP_JumpZ			= 12,	// [GRB]
	APROP_ChaseGoal		= 13,
	APROP_Frightened	= 14,
	APROP_Gravity		= 15,
	APROP_Friendly		= 16,
	APROP_SpawnHealth   = 17,
	APROP_Dropped		= 18,
	APROP_Notarget		= 19,
	APROP_Species		= 20,
	APROP_NameTag		= 21,
	APROP_Score			= 22,
	APROP_Notrigger		= 23,
	APROP_DamageFactor	= 24,
	APROP_MasterTID     = 25,
	APROP_TargetTID		= 26,
	APROP_TracerTID		= 27,
	APROP_WaterLevel	= 28,
	APROP_ScaleX        = 29,
	APROP_ScaleY        = 30,
	APROP_Dormant		= 31,
	APROP_Mass			= 32,
	APROP_Accuracy      = 33,
	APROP_Stamina       = 34,
	APROP_Height		= 35,
	APROP_Radius		= 36,
	APROP_ReactionTime  = 37,
	APROP_MeleeRange	= 38,
	APROP_ViewHeight	= 39,
	APROP_AttackZOffset	= 40,
	APROP_StencilColor	= 41,
};	

// [Dusk] Enumeration for GetTeamProperty
enum
{
	TPROP_Name = 0,
	TPROP_Score,
	TPROP_IsValid,
	TPROP_NumPlayers,
	TPROP_NumLivePlayers,
	TPROP_TextColor,
	TPROP_PlayerStartNum,
	TPROP_Spread,
	TPROP_Carrier,
	TPROP_Assister,
	TPROP_FragCount,
	TPROP_DeathCount,
	TPROP_WinCount,
	TPROP_PointCount,
	TPROP_ReturnTics,
	TPROP_TeamItem,
	TPROP_WinnerTheme,
	TPROP_LoserTheme,
};

// [AK] Enumeration for GetMapRotationInfo
enum
{
	MAPROTATION_Name = 0,
	MAPROTATION_LumpName,
	MAPROTATION_Used,
	MAPROTATION_MinPlayers,
	MAPROTATION_MaxPlayers,
};

class FBehavior
{
public:
	FBehavior (int lumpnum, FileReader * fr=NULL, int len=0);
	~FBehavior ();

	bool IsGood ();
	BYTE *FindChunk (DWORD id) const;
	BYTE *NextChunk (BYTE *chunk) const;
	const ScriptPtr *FindScript (int number) const;
	void StartTypedScripts (WORD type, AActor *activator, bool always, int arg1, bool runNow, bool onlyClientSideScripts=false, int arg2=0, int arg3=0); // [BB] Added arg2+arg3
	int CountTypedScripts( WORD type );
	DWORD PC2Ofs (int *pc) const { return (DWORD)((BYTE *)pc - Data); }
	int *Ofs2PC (DWORD ofs) const {	return (int *)(Data + ofs); }
	int *Jump2PC (DWORD jumpPoint) const { return Ofs2PC(JumpPoints[jumpPoint]); }
	ACSFormat GetFormat() const { return Format; }
	ScriptFunction *GetFunction (int funcnum, FBehavior *&module) const;
	int GetArrayVal (int arraynum, int index) const;
	void SetArrayVal (int arraynum, int index, int value);
	int GetArraySize (unsigned int arraynum) const; // [TDRR]
	inline bool CopyStringToArray(int arraynum, int index, int maxLength, const char * string);

	int FindFunctionName (const char *funcname) const;
	int FindMapVarName (const char *varname) const;
	int FindMapArray (const char *arrayname) const;
	int GetLibraryID () const { return LibraryID; }
	int *GetScriptAddress (const ScriptPtr *ptr) const { return (int *)(ptr->Address + Data); }
	int GetScriptIndex (const ScriptPtr *ptr) const { ptrdiff_t index = ptr - Scripts; return index >= NumScripts ? -1 : (int)index; }
	ScriptPtr *GetScriptPtr(int index) const { return index >= 0 && index < NumScripts ? &Scripts[index] : NULL; }
	int GetLumpNum() const { return LumpNum; }
	const char *GetModuleName() const { return ModuleName; }
	ACSProfileInfo *GetFunctionProfileData(int index) { return index >= 0 && index < NumFunctions ? &FunctionProfileData[index] : NULL; }
	ACSProfileInfo *GetFunctionProfileData(ScriptFunction *func) { return GetFunctionProfileData((int)(func - (ScriptFunction *)Functions)); }
	const char *LookupString (DWORD index) const;

	BoundsCheckingArray<SDWORD *, NUM_MAPVARS> MapVars;

	static FBehavior *StaticLoadModule (int lumpnum, FileReader * fr=NULL, int len=0);
	static void StaticLoadDefaultModules ();
	static void StaticUnloadModules ();
	static bool StaticCheckAllGood ();
	static FBehavior *StaticGetModule (int lib);
	static void StaticSerializeModuleStates (FArchive &arc);
	static void StaticMarkLevelVarStrings();
	static void StaticLockLevelVarStrings();
	static void StaticUnlockLevelVarStrings();

	static const ScriptPtr *StaticFindScript (int script, FBehavior *&module);
	static const char *StaticLookupString (DWORD index);
	static void StaticStartTypedScripts (WORD type, AActor *activator, bool always, int arg1=0, bool runNow=false, bool onlyClientSideScripts=false, int arg2=0, int arg3=0); // [BB] Added arg2+arg3
	static void StaticStopMyScripts (AActor *actor);
	static int StaticCountTypedScripts( WORD type );

	// [TP]
	static FString RepresentScript ( int script );
	static TArray<FName> StaticGetAllScriptNames();

private:
	struct ArrayInfo;

	ACSFormat Format;

	int LumpNum;
	BYTE *Data;
	int DataSize;
	BYTE *Chunks;
	ScriptPtr *Scripts;
	int NumScripts;
	ScriptFunction *Functions;
	ACSProfileInfo *FunctionProfileData;
	int NumFunctions;
	ArrayInfo *ArrayStore;
	int NumArrays;
	ArrayInfo **Arrays;
	int NumTotalArrays;
	DWORD StringTable;
	SDWORD MapVarStore[NUM_MAPVARS];
	TArray<FBehavior *> Imports;
	DWORD LibraryID;
	char ModuleName[9];
	TArray<int> JumpPoints;

	static TArray<FBehavior *> StaticModules;

	void LoadScriptsDirectory ();

	static int STACK_ARGS SortScripts (const void *a, const void *b);
	void UnencryptStrings ();
	void UnescapeStringTable(BYTE *chunkstart, BYTE *datastart, bool haspadding);
	int FindStringInChunk (DWORD *chunk, const char *varname) const;

	void SerializeVars (FArchive &arc);
	void SerializeVarSet (FArchive &arc, SDWORD *vars, int max);

	void MarkMapVarStrings() const;
	void LockMapVarStrings() const;
	void UnlockMapVarStrings() const;

	friend void ArrangeScriptProfiles(TArray<ProfileCollector> &profiles);
	friend void ArrangeFunctionProfiles(TArray<ProfileCollector> &profiles);
};

class DLevelScript : public DObject
{
	DECLARE_CLASS (DLevelScript, DObject)
	HAS_OBJECT_POINTERS
public:

	// P-codes for ACS scripts
	enum
	{
/*  0*/	PCD_NOP,
		PCD_TERMINATE,
		PCD_SUSPEND,
		PCD_PUSHNUMBER,
		PCD_LSPEC1,
		PCD_LSPEC2,
		PCD_LSPEC3,
		PCD_LSPEC4,
		PCD_LSPEC5,
		PCD_LSPEC1DIRECT,
/* 10*/	PCD_LSPEC2DIRECT,
		PCD_LSPEC3DIRECT,
		PCD_LSPEC4DIRECT,
		PCD_LSPEC5DIRECT,
		PCD_ADD,
		PCD_SUBTRACT,
		PCD_MULTIPLY,
		PCD_DIVIDE,
		PCD_MODULUS,
		PCD_EQ,
/* 20*/ PCD_NE,
		PCD_LT,
		PCD_GT,
		PCD_LE,
		PCD_GE,
		PCD_ASSIGNSCRIPTVAR,
		PCD_ASSIGNMAPVAR,
		PCD_ASSIGNWORLDVAR,
		PCD_PUSHSCRIPTVAR,
		PCD_PUSHMAPVAR,
/* 30*/	PCD_PUSHWORLDVAR,
		PCD_ADDSCRIPTVAR,
		PCD_ADDMAPVAR,
		PCD_ADDWORLDVAR,
		PCD_SUBSCRIPTVAR,
		PCD_SUBMAPVAR,
		PCD_SUBWORLDVAR,
		PCD_MULSCRIPTVAR,
		PCD_MULMAPVAR,
		PCD_MULWORLDVAR,
/* 40*/	PCD_DIVSCRIPTVAR,
		PCD_DIVMAPVAR,
		PCD_DIVWORLDVAR,
		PCD_MODSCRIPTVAR,
		PCD_MODMAPVAR,
		PCD_MODWORLDVAR,
		PCD_INCSCRIPTVAR,
		PCD_INCMAPVAR,
		PCD_INCWORLDVAR,
		PCD_DECSCRIPTVAR,
/* 50*/	PCD_DECMAPVAR,
		PCD_DECWORLDVAR,
		PCD_GOTO,
		PCD_IFGOTO,
		PCD_DROP,
		PCD_DELAY,
		PCD_DELAYDIRECT,
		PCD_RANDOM,
		PCD_RANDOMDIRECT,
		PCD_THINGCOUNT,
/* 60*/	PCD_THINGCOUNTDIRECT,
		PCD_TAGWAIT,
		PCD_TAGWAITDIRECT,
		PCD_POLYWAIT,
		PCD_POLYWAITDIRECT,
		PCD_CHANGEFLOOR,
		PCD_CHANGEFLOORDIRECT,
		PCD_CHANGECEILING,
		PCD_CHANGECEILINGDIRECT,
		PCD_RESTART,
/* 70*/	PCD_ANDLOGICAL,
		PCD_ORLOGICAL,
		PCD_ANDBITWISE,
		PCD_ORBITWISE,
		PCD_EORBITWISE,
		PCD_NEGATELOGICAL,
		PCD_LSHIFT,
		PCD_RSHIFT,
		PCD_UNARYMINUS,
		PCD_IFNOTGOTO,
/* 80*/	PCD_LINESIDE,
		PCD_SCRIPTWAIT,
		PCD_SCRIPTWAITDIRECT,
		PCD_CLEARLINESPECIAL,
		PCD_CASEGOTO,
		PCD_BEGINPRINT,
		PCD_ENDPRINT,
		PCD_PRINTSTRING,
		PCD_PRINTNUMBER,
		PCD_PRINTCHARACTER,
/* 90*/	PCD_PLAYERCOUNT,
		PCD_GAMETYPE,
		PCD_GAMESKILL,
		PCD_TIMER,
		PCD_SECTORSOUND,
		PCD_AMBIENTSOUND,
		PCD_SOUNDSEQUENCE,
		PCD_SETLINETEXTURE,
		PCD_SETLINEBLOCKING,
		PCD_SETLINESPECIAL,
/*100*/	PCD_THINGSOUND,
		PCD_ENDPRINTBOLD,		// [RH] End of Hexen p-codes
		PCD_ACTIVATORSOUND,
		PCD_LOCALAMBIENTSOUND,
		PCD_SETLINEMONSTERBLOCKING,
		PCD_PLAYERBLUESKULL,	// [BC] Start of new [Skull Tag] pcodes
		PCD_PLAYERREDSKULL,
		PCD_PLAYERYELLOWSKULL,
		PCD_PLAYERMASTERSKULL,
		PCD_PLAYERBLUECARD,
/*110*/	PCD_PLAYERREDCARD,
		PCD_PLAYERYELLOWCARD,
		PCD_PLAYERMASTERCARD,
		PCD_PLAYERBLACKSKULL,
		PCD_PLAYERSILVERSKULL,
		PCD_PLAYERGOLDSKULL,
		PCD_PLAYERBLACKCARD,
		PCD_PLAYERSILVERCARD,
		PCD_ISMULTIPLAYER,
		PCD_PLAYERTEAM,
/*120*/	PCD_PLAYERHEALTH,
		PCD_PLAYERARMORPOINTS,
		PCD_PLAYERFRAGS,
		PCD_PLAYEREXPERT,
		PCD_BLUETEAMCOUNT,
		PCD_REDTEAMCOUNT,
		PCD_BLUETEAMSCORE,
		PCD_REDTEAMSCORE,
		PCD_ISONEFLAGCTF,
		PCD_GETINVASIONWAVE,
/*130*/	PCD_GETINVASIONSTATE,
		PCD_PRINTNAME,
		PCD_MUSICCHANGE,
		PCD_CONSOLECOMMANDDIRECT,
		PCD_CONSOLECOMMAND,
		PCD_SINGLEPLAYER,		// [RH] End of Skull Tag p-codes
		PCD_FIXEDMUL,
		PCD_FIXEDDIV,
		PCD_SETGRAVITY,
		PCD_SETGRAVITYDIRECT,
/*140*/	PCD_SETAIRCONTROL,
		PCD_SETAIRCONTROLDIRECT,
		PCD_CLEARINVENTORY,
		PCD_GIVEINVENTORY,
		PCD_GIVEINVENTORYDIRECT,
		PCD_TAKEINVENTORY,
		PCD_TAKEINVENTORYDIRECT,
		PCD_CHECKINVENTORY,
		PCD_CHECKINVENTORYDIRECT,
		PCD_SPAWN,
/*150*/	PCD_SPAWNDIRECT,
		PCD_SPAWNSPOT,
		PCD_SPAWNSPOTDIRECT,
		PCD_SETMUSIC,
		PCD_SETMUSICDIRECT,
		PCD_LOCALSETMUSIC,
		PCD_LOCALSETMUSICDIRECT,
		PCD_PRINTFIXED,
		PCD_PRINTLOCALIZED,
		PCD_MOREHUDMESSAGE,
/*160*/	PCD_OPTHUDMESSAGE,
		PCD_ENDHUDMESSAGE,
		PCD_ENDHUDMESSAGEBOLD,
		PCD_SETSTYLE,
		PCD_SETSTYLEDIRECT,
		PCD_SETFONT,
		PCD_SETFONTDIRECT,
		PCD_PUSHBYTE,
		PCD_LSPEC1DIRECTB,
		PCD_LSPEC2DIRECTB,
/*170*/	PCD_LSPEC3DIRECTB,
		PCD_LSPEC4DIRECTB,
		PCD_LSPEC5DIRECTB,
		PCD_DELAYDIRECTB,
		PCD_RANDOMDIRECTB,
		PCD_PUSHBYTES,
		PCD_PUSH2BYTES,
		PCD_PUSH3BYTES,
		PCD_PUSH4BYTES,
		PCD_PUSH5BYTES,
/*180*/	PCD_SETTHINGSPECIAL,
		PCD_ASSIGNGLOBALVAR,
		PCD_PUSHGLOBALVAR,
		PCD_ADDGLOBALVAR,
		PCD_SUBGLOBALVAR,
		PCD_MULGLOBALVAR,
		PCD_DIVGLOBALVAR,
		PCD_MODGLOBALVAR,
		PCD_INCGLOBALVAR,
		PCD_DECGLOBALVAR,
/*190*/	PCD_FADETO,
		PCD_FADERANGE,
		PCD_CANCELFADE,
		PCD_PLAYMOVIE,
		PCD_SETFLOORTRIGGER,
		PCD_SETCEILINGTRIGGER,
		PCD_GETACTORX,
		PCD_GETACTORY,
		PCD_GETACTORZ,
		PCD_STARTTRANSLATION,
/*200*/	PCD_TRANSLATIONRANGE1,
		PCD_TRANSLATIONRANGE2,
		PCD_ENDTRANSLATION,
		PCD_CALL,
		PCD_CALLDISCARD,
		PCD_RETURNVOID,
		PCD_RETURNVAL,
		PCD_PUSHMAPARRAY,
		PCD_ASSIGNMAPARRAY,
		PCD_ADDMAPARRAY,
/*210*/	PCD_SUBMAPARRAY,
		PCD_MULMAPARRAY,
		PCD_DIVMAPARRAY,
		PCD_MODMAPARRAY,
		PCD_INCMAPARRAY,
		PCD_DECMAPARRAY,
		PCD_DUP,
		PCD_SWAP,
		PCD_WRITETOINI,
		PCD_GETFROMINI,
/*220*/ PCD_SIN,
		PCD_COS,
		PCD_VECTORANGLE,
		PCD_CHECKWEAPON,
		PCD_SETWEAPON,
		PCD_TAGSTRING,
		PCD_PUSHWORLDARRAY,
		PCD_ASSIGNWORLDARRAY,
		PCD_ADDWORLDARRAY,
		PCD_SUBWORLDARRAY,
/*230*/	PCD_MULWORLDARRAY,
		PCD_DIVWORLDARRAY,
		PCD_MODWORLDARRAY,
		PCD_INCWORLDARRAY,
		PCD_DECWORLDARRAY,
		PCD_PUSHGLOBALARRAY,
		PCD_ASSIGNGLOBALARRAY,
		PCD_ADDGLOBALARRAY,
		PCD_SUBGLOBALARRAY,
		PCD_MULGLOBALARRAY,
/*240*/	PCD_DIVGLOBALARRAY,
		PCD_MODGLOBALARRAY,
		PCD_INCGLOBALARRAY,
		PCD_DECGLOBALARRAY,
		PCD_SETMARINEWEAPON,
		PCD_SETACTORPROPERTY,
		PCD_GETACTORPROPERTY,
		PCD_PLAYERNUMBER,
		PCD_ACTIVATORTID,
		PCD_SETMARINESPRITE,
/*250*/	PCD_GETSCREENWIDTH,
		PCD_GETSCREENHEIGHT,
		PCD_THING_PROJECTILE2,
		PCD_STRLEN,
		PCD_SETHUDSIZE,
		PCD_GETCVAR,
		PCD_CASEGOTOSORTED,
		PCD_SETRESULTVALUE,
		PCD_GETLINEROWOFFSET,
		PCD_GETACTORFLOORZ,
/*260*/	PCD_GETACTORANGLE,
		PCD_GETSECTORFLOORZ,
		PCD_GETSECTORCEILINGZ,
		PCD_LSPEC5RESULT,
		PCD_GETSIGILPIECES,
		PCD_GETLEVELINFO,
		PCD_CHANGESKY,
		PCD_PLAYERINGAME,
		PCD_PLAYERISBOT,
		PCD_SETCAMERATOTEXTURE,
/*270*/	PCD_ENDLOG,
		PCD_GETAMMOCAPACITY,
		PCD_SETAMMOCAPACITY,
		PCD_PRINTMAPCHARARRAY,		// [JB] start of new p-codes
		PCD_PRINTWORLDCHARARRAY,
		PCD_PRINTGLOBALCHARARRAY,	// [JB] end of new p-codes
		PCD_SETACTORANGLE,			// [GRB]
		PCD_GRABINPUT,				// Unused but acc defines them
		PCD_SETMOUSEPOINTER,		// "
		PCD_MOVEMOUSEPOINTER,		// "
/*280*/	PCD_SPAWNPROJECTILE,
		PCD_GETSECTORLIGHTLEVEL,
		PCD_GETACTORCEILINGZ,
		PCD_SETACTORPOSITION,
		PCD_CLEARACTORINVENTORY,
		PCD_GIVEACTORINVENTORY,
		PCD_TAKEACTORINVENTORY,
		PCD_CHECKACTORINVENTORY,
		PCD_THINGCOUNTNAME,
		PCD_SPAWNSPOTFACING,
/*290*/	PCD_PLAYERCLASS,			// [GRB]
		//[MW] start my p-codes
		PCD_ANDSCRIPTVAR,
		PCD_ANDMAPVAR, 
		PCD_ANDWORLDVAR, 
		PCD_ANDGLOBALVAR, 
		PCD_ANDMAPARRAY, 
		PCD_ANDWORLDARRAY, 
		PCD_ANDGLOBALARRAY,
		PCD_EORSCRIPTVAR, 
		PCD_EORMAPVAR, 
/*300*/	PCD_EORWORLDVAR, 
		PCD_EORGLOBALVAR, 
		PCD_EORMAPARRAY, 
		PCD_EORWORLDARRAY, 
		PCD_EORGLOBALARRAY,
		PCD_ORSCRIPTVAR, 
		PCD_ORMAPVAR, 
		PCD_ORWORLDVAR, 
		PCD_ORGLOBALVAR, 
		PCD_ORMAPARRAY, 
/*310*/	PCD_ORWORLDARRAY, 
		PCD_ORGLOBALARRAY,
		PCD_LSSCRIPTVAR, 
		PCD_LSMAPVAR, 
		PCD_LSWORLDVAR, 
		PCD_LSGLOBALVAR, 
		PCD_LSMAPARRAY, 
		PCD_LSWORLDARRAY, 
		PCD_LSGLOBALARRAY,
		PCD_RSSCRIPTVAR, 
/*320*/	PCD_RSMAPVAR, 
		PCD_RSWORLDVAR, 
		PCD_RSGLOBALVAR, 
		PCD_RSMAPARRAY, 
		PCD_RSWORLDARRAY, 
		PCD_RSGLOBALARRAY, 
		//[MW] end my p-codes
		PCD_GETPLAYERINFO,			// [GRB]
		PCD_CHANGELEVEL,
		PCD_SECTORDAMAGE,
		PCD_REPLACETEXTURES,
/*330*/	PCD_NEGATEBINARY,
		PCD_GETACTORPITCH,
		PCD_SETACTORPITCH,
		PCD_PRINTBIND,
		PCD_SETACTORSTATE,
		PCD_THINGDAMAGE2,
		PCD_USEINVENTORY,
		PCD_USEACTORINVENTORY,
		PCD_CHECKACTORCEILINGTEXTURE,
		PCD_CHECKACTORFLOORTEXTURE,
/*340*/	PCD_GETACTORLIGHTLEVEL,
		PCD_SETMUGSHOTSTATE,
		PCD_THINGCOUNTSECTOR,
		PCD_THINGCOUNTNAMESECTOR,
		PCD_CHECKPLAYERCAMERA,		// [TN]
		PCD_MORPHACTOR,				// [MH]
		PCD_UNMORPHACTOR,			// [MH]
		PCD_GETPLAYERINPUT,
		PCD_CLASSIFYACTOR,
		PCD_PRINTBINARY,
/*350*/	PCD_PRINTHEX,
		PCD_CALLFUNC,
		PCD_SAVESTRING,			// [FDARI] create string (temporary)
		PCD_PRINTMAPCHRANGE,	// [FDARI] output range (print part of array)
		PCD_PRINTWORLDCHRANGE,
		PCD_PRINTGLOBALCHRANGE,
		PCD_STRCPYTOMAPCHRANGE,	// [FDARI] input range (copy string to all/part of array)
		PCD_STRCPYTOWORLDCHRANGE,
		PCD_STRCPYTOGLOBALCHRANGE,
		PCD_PUSHFUNCTION,		// from Eternity
/*360*/	PCD_CALLSTACK,			// from Eternity
		PCD_SCRIPTWAITNAMED,
		PCD_TRANSLATIONRANGE3,
		PCD_GOTOSTACK,
		PCD_ASSIGNSCRIPTARRAY,
		PCD_PUSHSCRIPTARRAY,
		PCD_ADDSCRIPTARRAY,
		PCD_SUBSCRIPTARRAY,
		PCD_MULSCRIPTARRAY,
		PCD_DIVSCRIPTARRAY,
/*370*/	PCD_MODSCRIPTARRAY,
		PCD_INCSCRIPTARRAY,
		PCD_DECSCRIPTARRAY,
		PCD_ANDSCRIPTARRAY,
		PCD_EORSCRIPTARRAY,
		PCD_ORSCRIPTARRAY,
		PCD_LSSCRIPTARRAY,
		PCD_RSSCRIPTARRAY,
		PCD_PRINTSCRIPTCHARARRAY,
		PCD_PRINTSCRIPTCHRANGE,
/*380*/	PCD_STRCPYTOSCRIPTCHRANGE,

		// [BB] We need to fix the number for the new commands!
		// [CW] Begin team additions.
		PCD_GETTEAMPLAYERCOUNT,
		// [CW] End team additions.
/*381*/	PCODE_COMMAND_COUNT
	};

	// Some constants used by ACS scripts
	enum {
		LINE_FRONT =			0,
		LINE_BACK =				1
	};
	enum {
		SIDE_FRONT =			0,
		SIDE_BACK =				1
	};
	enum {
		TEXTURE_TOP =			0,
		TEXTURE_MIDDLE =		1,
		TEXTURE_BOTTOM =		2
	};
	enum {
		GAME_SINGLE_PLAYER =	0,
		GAME_NET_COOPERATIVE =	1,
		GAME_NET_DEATHMATCH =	2,
		GAME_TITLE_MAP =		3,
		GAME_NET_TEAMGAME =		4,
	};
	enum {
		CLASS_FIGHTER =			0,
		CLASS_CLERIC =			1,
		CLASS_MAGE =			2
	};
	enum {
		SKILL_VERY_EASY =		0,
		SKILL_EASY =			1,
		SKILL_NORMAL =			2,
		SKILL_HARD =			3,
		SKILL_VERY_HARD =		4
	};
	enum {
		BLOCK_NOTHING =			0,
		BLOCK_CREATURES =		1,
		BLOCK_EVERYTHING =		2,
		BLOCK_RAILING =			3,
		BLOCK_PLAYERS =			4
	};
	enum {
		LEVELINFO_PAR_TIME,
		LEVELINFO_CLUSTERNUM,
		LEVELINFO_LEVELNUM,
		LEVELINFO_TOTAL_SECRETS,
		LEVELINFO_FOUND_SECRETS,
		LEVELINFO_TOTAL_ITEMS,
		LEVELINFO_FOUND_ITEMS,
		LEVELINFO_TOTAL_MONSTERS,
		LEVELINFO_KILLED_MONSTERS,
		LEVELINFO_SUCK_TIME
	};
	enum {
		PLAYERINFO_TEAM,
		PLAYERINFO_AIMDIST,
		PLAYERINFO_COLOR,
		PLAYERINFO_GENDER,
		PLAYERINFO_NEVERSWITCH,
		PLAYERINFO_MOVEBOB,
		PLAYERINFO_STILLBOB,
		PLAYERINFO_PLAYERCLASS,
		PLAYERINFO_FOV,
		PLAYERINFO_DESIREDFOV,
	};

	enum EScriptState
	{
		SCRIPT_Running,
		SCRIPT_Suspended,
		SCRIPT_Delayed,
		SCRIPT_TagWait,
		SCRIPT_PolyWait,
		SCRIPT_ScriptWaitPre,
		SCRIPT_ScriptWait,
		SCRIPT_PleaseRemove,
		SCRIPT_DivideBy0,
		SCRIPT_ModulusBy0,
	};

	DLevelScript (AActor *who, line_t *where, int num, const ScriptPtr *code, FBehavior *module,
		const int *args, int argcount, int flags);
	~DLevelScript ();

	void Serialize (FArchive &arc);
	int RunScript ();

	inline void SetState (EScriptState newstate) { state = newstate; }
	inline EScriptState GetState () { return state; }

	DLevelScript *GetNext() const { return next; }

	void MarkLocalVarStrings() const
	{
		GlobalACSStrings.MarkStringArray(localvars, numlocalvars);
	}
	void LockLocalVarStrings() const
	{
		GlobalACSStrings.LockStringArray(localvars, numlocalvars);
	}
	void UnlockLocalVarStrings() const
	{
		GlobalACSStrings.UnlockStringArray(localvars, numlocalvars);
	}

protected:
	DLevelScript	*next, *prev;
	int				script;
	SDWORD			*localvars;
	int				numlocalvars;
	int				*pc;
	EScriptState	state;
	int				statedata;
	TObjPtr<AActor>	activator;
	line_t			*activationline;
	bool			backSide;
	FFont			*activefont;
	int				hudwidth, hudheight;
	int				ClipRectLeft, ClipRectTop, ClipRectWidth, ClipRectHeight;
	int				WrapWidth;
	FBehavior	    *activeBehavior;
	int				InModuleScriptNumber;
	FString			activefontname; // [TP]

	// [AK] Pointers to the source, inflictor, and target actors that triggered a GAMEEVENT_ACTOR_DAMAGED or
	// GAMEEVENT_ACTOR_ARMORDAMAGED event. In all other cases, these pointers should be equal to NULL.
	TObjPtr<AActor>	pDamageSource;
	TObjPtr<AActor> pDamageInflictor;
	TObjPtr<AActor> pDamageTarget;

	void Link ();
	void Unlink ();
	void PutLast ();
	void PutFirst ();
	static int Random (int min, int max);
	static int ThingCount (int type, int stringid, int tid, int tag);
	static void ChangeFlat (int tag, int name, bool floorOrCeiling);
	static int CountPlayers ();
	static void SetLineTexture (int lineid, int side, int position, int name);
	static void ReplaceTextures (int fromname, int toname, int flags);
	// [BB]
	static void ReplaceTextures (const char *fromname, const char *toname, int flags);
	static int DoSpawn (int type, fixed_t x, fixed_t y, fixed_t z, int tid, int angle, bool force);
	static bool DoCheckActorTexture(int tid, AActor *activator, int string, bool floor);
	int DoSpawnSpot (int type, int spot, int tid, int angle, bool forced);
	int DoSpawnSpotFacing (int type, int spot, int tid, bool forced);
	int DoClassifyActor (int tid);
	int CallFunction(int argCount, int funcIndex, SDWORD *args, struct ACSLocals *locals); // [TDRR] Added "locals" parameter to allow accessing local arrays.

	void DoFadeTo (int r, int g, int b, int a, fixed_t time);
	void DoFadeRange (int r1, int g1, int b1, int a1,
		int r2, int g2, int b2, int a2, fixed_t time);
	void DoSetFont (int fontnum);
	void SetActorProperty (int tid, int property, int value);
	void DoSetActorProperty (AActor *actor, int property, int value);
	int GetActorProperty (int tid, int property);
	int CheckActorProperty (int tid, int property, int value);
	int GetPlayerInput (int playernum, int inputnum);

	int LineFromID(int id);
	int SideFromID(int id, int side);

private:
	DLevelScript ();

	friend class DACSThinker;

	// [BB/TP] The client needs to call DLevelScript::ReplaceTextures.
	friend class ServerCommands::ReplaceTextures;
	// [AK] We need to access protected variables from this class when we tell the clients to print a HUD message.
	friend void SERVERCOMMANDS_PrintACSHUDMessage( DLevelScript *pScript, const char *pszString, float fX, float fY, LONG lType, LONG lColor, float fHoldTime, float fInTime, float fOutTime, fixed_t Alpha, LONG lID, ULONG ulPlayerExtra, ServerCommandFlags flags );
	// [AK] If the current running script is a GAMEEVENT_ACTOR_DAMAGED or GAMEEVENT_ACTOR_ARMORDAMAGED event, this returns a pointer to the source, inflictor, or target actor.
	friend AActor *ACS_GetScriptDamagePointers( unsigned int pointer );
};

class DACSThinker : public DThinker
{
	DECLARE_CLASS (DACSThinker, DThinker)
	HAS_OBJECT_POINTERS
public:
	DACSThinker ();
	~DACSThinker ();

	void Serialize (FArchive &arc);
	void Tick ();

	typedef TMap<int, DLevelScript *> ScriptMap;
	ScriptMap RunningScripts;	// Array of all synchronous scripts
	static TObjPtr<DACSThinker> ActiveThinker;

	void DumpScriptStatus();
	void StopScriptsFor (AActor *actor);
	// [BB] Added StopAndDestroyAllScripts, which is needed in GAME_ResetMap.
	void StopAndDestroyAllScripts ();

private:
	DLevelScript *LastScript;
	DLevelScript *Scripts;				// List of all running scripts

	friend class DLevelScript;
	friend class FBehavior;
};

// The structure used to control scripts between maps
struct acsdefered_t
{
	struct acsdefered_t *next;

	enum EType
	{
		defexecute,
		defexealways,
		defsuspend,
		defterminate
	} type;
	int script;
	int args[3];
	int playernum;
};

FArchive &operator<< (FArchive &arc, acsdefered_t *&defer);

//*****************************************************************************
//	PROTOTYPES

bool	ACS_IsCalledFromConsoleCommand( void );
bool	ACS_IsEventScript( int script ); // [AK]
bool	ACS_IsCalledFromScript( void ); // [AK]
bool	ACS_IsScriptClientSide( int script );
bool	ACS_IsScriptClientSide( const ScriptPtr *pScriptData );
bool	ACS_IsScriptPukeable( ULONG ulScript );
int		ACS_GetTranslationIndex( FRemapTable *pTranslation );
int		ACS_PushAndReturnDynamicString ( const FString &Work );
bool	ACS_ExistsScript( int script );
AActor	*ACS_GetScriptDamagePointers( unsigned int pointer ); // [AK]

// [BB] Export DoGiveInv
bool	DoGiveInv(AActor *actor, const PClass *info, int amount);

#endif //__P_ACS_H__
