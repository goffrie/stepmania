#include "global.h"
#include "UnlockManager.h"
#include "PrefsManager.h"
#include "RageLog.h"
#include "song.h"
#include "Course.h"
#include "RageUtil.h"
#include "SongManager.h"
#include "GameState.h"
#include "ProfileManager.h"
#include "Profile.h"
#include "ThemeManager.h"
#include "Foreach.h"
#include "Steps.h"
#include <float.h>
#include "CommonMetrics.h"
#include "LuaFunctions.h"

UnlockManager*	UNLOCKMAN = NULL;	// global and accessable from anywhere in our program

#define UNLOCK_NAMES		THEME->GetMetric ("Unlocks","UnlockNames")
#define UNLOCK(sLineName)	THEME->GetMetric ("Unlocks",ssprintf("Unlock%s",sLineName.c_str()))

static const char *UnlockRequirementNames[] =
{
	"ArcadePoints",
	"DancePoints",
	"SongPoints",
	"ExtraCleared",
	"ExtraFailed",
	"Toasties",
	"StagesCleared"
};
XToString( UnlockRequirement, NUM_UnlockRequirement );
StringToX( UnlockRequirement );
LuaXType( UnlockRequirement, NUM_UnlockRequirement, "UnlockRequirement_", false )

static const char *UnlockRewardTypeNames[] =
{
	"Song",
	"Steps",
	"Course",
	"Modifier",
};
XToString( UnlockRewardType, NUM_UnlockRewardType );
XToLocalizedString( UnlockRewardType );
LuaFunction( UnlockRewardTypeToLocalizedString, UnlockRewardTypeToLocalizedString((UnlockRewardType) IArg(1)) );

UnlockManager::UnlockManager()
{
	UNLOCKMAN = this;

	Load();
}

void UnlockManager::UnlockSong( const Song *song )
{
	const UnlockEntry *p = FindSong( song );
	if( !p )
		return;  // does not exist
	if( p->m_iEntryID == -1 )
		return;

	UnlockEntryID( p->m_iEntryID );
}

int UnlockManager::FindEntryID( const RString &sName ) const
{
	const UnlockEntry *pEntry = NULL;
	
	const Song *pSong = SONGMAN->FindSong( sName );
	if( pSong != NULL )
		pEntry = FindSong( pSong );

	const Course *pCourse = SONGMAN->FindCourse( sName );
	if( pCourse != NULL )
		pEntry = FindCourse( pCourse );
	
	if( pEntry == NULL )
		pEntry = FindModifier( sName );

	if( pEntry == NULL )
	{
		LOG->Warn( "Couldn't find locked entry \"%s\"", sName.c_str() );
		return -1;
	}

	return pEntry->m_iEntryID;
}

bool UnlockManager::CourseIsLocked( const Course *course ) const
{
	if( !PREFSMAN->m_bUseUnlockSystem )
		return false;

	const UnlockEntry *p = FindCourse( course );
	if( p == NULL )
		return false;

	return p->IsLocked();
}

bool UnlockManager::SongIsLocked( const Song *song ) const
{
	if( !PREFSMAN->m_bUseUnlockSystem )
		return false;

	const UnlockEntry *p = FindSong( song );
	if( p == NULL )
		return false;

	return p->IsLocked();
}

/* Return true if the song is *only* available in roulette. */
bool UnlockManager::SongIsRouletteOnly( const Song *song ) const
{
	if( !PREFSMAN->m_bUseUnlockSystem )
		return false;

	const UnlockEntry *p = FindSong( song );
	if( p == NULL )
		return false;

	/* If the song is locked by a code, and it's a roulette code, honor IsLocked. */
	if( p->m_iEntryID == -1 || m_RouletteCodes.find( p->m_iEntryID ) == m_RouletteCodes.end() )
		return false;

	return p->IsLocked();
}

bool UnlockManager::StepsIsLocked( const Song *pSong, const Steps *pSteps ) const
{
	if( !PREFSMAN->m_bUseUnlockSystem )
		return false;

	const UnlockEntry *p = FindSteps( pSong, pSteps );
	if( p == NULL )
		return false;

	return p->IsLocked();
}

bool UnlockManager::ModifierIsLocked( const RString &sOneMod ) const
{
	if( !PREFSMAN->m_bUseUnlockSystem )
		return false;

	const UnlockEntry *p = FindModifier( sOneMod );
	if( p == NULL )
		return false;

	return p->IsLocked();
}

const UnlockEntry *UnlockManager::FindSong( const Song *pSong ) const
{
	FOREACH_CONST( UnlockEntry, m_UnlockEntries, e )
		if( e->m_pSong == pSong  &&  e->m_dc == DIFFICULTY_INVALID )
			return &(*e);
	return NULL;
}

const UnlockEntry *UnlockManager::FindSteps( const Song *pSong, const Steps *pSteps ) const
{
	ASSERT( pSong && pSteps );
	FOREACH_CONST( UnlockEntry, m_UnlockEntries, e )
		if( e->m_pSong == pSong  &&  e->m_dc == pSteps->GetDifficulty() )
			return &(*e);
	return NULL;
}

const UnlockEntry *UnlockManager::FindCourse( const Course *pCourse ) const
{
	FOREACH_CONST( UnlockEntry, m_UnlockEntries, e )
		if( e->m_pCourse == pCourse )
			return &(*e);
	return NULL;
}

const UnlockEntry *UnlockManager::FindModifier( const RString &sOneMod ) const
{
	FOREACH_CONST( UnlockEntry, m_UnlockEntries, e )
		if( e->GetModifier() == 0 )
			return &(*e);
	return NULL;
}

static float GetArcadePoints( const Profile *pProfile )
{
	float fAP =	0;

	FOREACH_Grade(g)
	{
		switch(g)
		{
		case Grade_Tier01:
		case Grade_Tier02:	fAP += 9 * pProfile->m_iNumStagesPassedByGrade[g]; break;
		default:			fAP += 1 * pProfile->m_iNumStagesPassedByGrade[g]; break;

		case Grade_Failed:
		case Grade_NoData:
			;	// no points
			break;
		}
	}

	FOREACH_PlayMode(pm)
	{
		switch(pm)
		{
		case PLAY_MODE_NONSTOP:
		case PLAY_MODE_ONI:
		case PLAY_MODE_ENDLESS:
			fAP += pProfile->m_iNumSongsPlayedByPlayMode[pm];
			break;
		}

	}

	return fAP;
}

static float GetSongPoints( const Profile *pProfile )
{
	float fSP =	0;

	FOREACH_Grade(g)
	{
		switch( g )
		{
		case Grade_Tier01:/*AAAA*/	fSP += 20 * pProfile->m_iNumStagesPassedByGrade[g];	break;
		case Grade_Tier02:/*AAA*/	fSP += 10* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case Grade_Tier03:/*AA*/	fSP += 5* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case Grade_Tier04:/*A*/		fSP += 4* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case Grade_Tier05:/*B*/		fSP += 3* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case Grade_Tier06:/*C*/		fSP += 2* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case Grade_Tier07:/*D*/		fSP += 1* pProfile->m_iNumStagesPassedByGrade[g];	break;
		case Grade_Failed:
		case Grade_NoData:
			;	// no points
			break;
		}
	}

	FOREACH_PlayMode(pm)
	{
		switch(pm)
		{
		case PLAY_MODE_NONSTOP:
		case PLAY_MODE_ONI:
		case PLAY_MODE_ENDLESS:
			fSP += pProfile->m_iNumSongsPlayedByPlayMode[pm];
			break;
		}

	}

	return fSP;
}

void UnlockManager::GetPoints( const Profile *pProfile, float fScores[NUM_UnlockRequirement] ) const
{
	fScores[UnlockRequirement_ArcadePoints] = GetArcadePoints( pProfile );
	fScores[UnlockRequirement_SongPoints] = GetSongPoints( pProfile );
	fScores[UnlockRequirement_DancePoints] = (float) pProfile->m_iTotalDancePoints;
	fScores[UnlockRequirement_StagesCleared] = (float) pProfile->GetTotalNumSongsPassed();
}

/* Return true if all songs and/or courses referenced by an unlock are available. */
bool UnlockEntry::IsValid() const
{
	switch( m_Type )
	{
	case UnlockRewardType_Song:
		return m_pSong != NULL;

	case UnlockRewardType_Steps:
		return m_pSong != NULL && m_dc != DIFFICULTY_INVALID;

	case UnlockRewardType_Course:
		return m_pCourse != NULL;

	case UnlockRewardType_Modifier:
		return true;

	default:
		WARN( ssprintf("%i", m_Type) );
		return false;
	}
}

bool UnlockEntry::IsLocked() const
{
	float fScores[NUM_UnlockRequirement];
	UNLOCKMAN->GetPoints( PROFILEMAN->GetMachineProfile(), fScores );

	for( int i = 0; i < NUM_UnlockRequirement; ++i )
		if( m_fRequirement[i] && fScores[i] >= m_fRequirement[i] )
			return false;

	if( m_iEntryID != -1 && PROFILEMAN->GetMachineProfile()->m_UnlockedEntryIDs.find(m_iEntryID) != PROFILEMAN->GetMachineProfile()->m_UnlockedEntryIDs.end() )
		return false;

	return true;
}

RString UnlockEntry::GetDescription() const
{
	switch( m_Type )
	{
	default:
		ASSERT(0);
		return "";
	case UnlockRewardType_Song:
		return m_pSong ? m_pSong->GetDisplayFullTitle() : "";
	case UnlockRewardType_Steps:
		return DifficultyToLocalizedString( m_dc );
	case UnlockRewardType_Course:
		return m_pCourse ? m_pCourse->GetDisplayFullTitle() : "";
	case UnlockRewardType_Modifier:
		return CommonMetrics::LocalizeOptionItem( GetModifier(), false );
	}	
}

RString	UnlockEntry::GetBannerFile() const
{
	switch( m_Type )
	{
	default:
		ASSERT(0);
		return "";
	case UnlockRewardType_Song:
		return m_pSong ? m_pSong->GetBannerPath() : "";
	case UnlockRewardType_Steps:
		return "";
	case UnlockRewardType_Course:
		return m_pCourse ? m_pCourse->m_sBannerPath : "";
	case UnlockRewardType_Modifier:
		return "";
	}	
}

RString	UnlockEntry::GetBackgroundFile() const
{
	switch( m_Type )
	{
	default:
		ASSERT(0);
		return "";
	case UnlockRewardType_Song:
	case UnlockRewardType_Steps:
		return m_pSong ? m_pSong->GetBackgroundPath() : "";
	case UnlockRewardType_Course:
		return "";
	case UnlockRewardType_Modifier:
		return "";
	}	
}

/////////////////////////////////////////////////////////

void UnlockManager::Load()
{
	LOG->Trace( "UnlockManager::Load()" );

	vector<RString> asUnlockNames;
	split( UNLOCK_NAMES, ",", asUnlockNames );
	if( asUnlockNames.empty() )
		return;

	for( unsigned i = 0; i < asUnlockNames.size(); ++i )
	{
		const RString &sUnlockName = asUnlockNames[i];
		RString sUnlock = UNLOCK(sUnlockName);

		Commands vCommands;
		ParseCommands( sUnlock, vCommands );

		UnlockEntry current;
		bool bRoulette = false;

		for( unsigned j = 0; j < vCommands.v.size(); ++j )
		{
			const Command &cmd = vCommands.v[j];
			RString sName = cmd.GetName();

			if( sName == "song" )
			{
				current.m_Type = UnlockRewardType_Song;
				current.m_cmd = cmd;
			}
			if( sName == "steps" )
			{
				current.m_Type = UnlockRewardType_Steps;
				current.m_cmd = cmd;
			}
			if( sName == "course" )
			{
				current.m_Type = UnlockRewardType_Course;
				current.m_cmd = cmd;
			}
			if( sName == "mod" )
			{
				current.m_Type = UnlockRewardType_Modifier;
				current.m_cmd = cmd;
			}
			else if( sName == "code" )
			{
				// Hack: Lua only has a floating point type, and codes may be big enough
				// that converting them from string to float to int introduces rounding
				// error.  Convert directly to int.
				current.m_iEntryID = atoi( (RString) cmd.GetArg(1) );
			}
			else if( sName == "roulette" )
			{
				bRoulette = true;
			}
			else
			{
				const UnlockRequirement ut = StringToUnlockRequirement( cmd.GetName() );
				if( ut != UnlockRequirement_INVALID )
					current.m_fRequirement[ut] = cmd.GetArg(1);
			}
		}

		if( bRoulette )
			m_RouletteCodes.insert( current.m_iEntryID );

		m_UnlockEntries.push_back( current );
	}

	UpdateCachedPointers();

	FOREACH_CONST( UnlockEntry, m_UnlockEntries, e )
	{
		RString str = ssprintf( "Unlock: %s; ", join("\n",e->m_cmd.m_vsArgs).c_str() );
		FOREACH_ENUM2( UnlockRequirement, j )
			if( e->m_fRequirement[j] )
				str += ssprintf( "%s = %f; ", UnlockRequirementToString(j).c_str(), e->m_fRequirement[j] );

		str += ssprintf( "entryID = %i ", e->m_iEntryID );
		str += e->IsLocked()? "locked":"unlocked";
		if( e->m_pSong )
			str += ( " (found song)" );
		if( e->m_pCourse )
			str += ( " (found course)" );
		LOG->Trace( "%s", str.c_str() );
	}
	
	return;
}

float UnlockManager::PointsUntilNextUnlock( UnlockRequirement t ) const
{
	float fScores[NUM_UnlockRequirement];
	UNLOCKMAN->GetPoints( PROFILEMAN->GetMachineProfile(), fScores );

	float fSmallestPoints = FLT_MAX;   // or an arbitrarily large value
	for( unsigned a=0; a<m_UnlockEntries.size(); a++ )
		if( m_UnlockEntries[a].m_fRequirement[t] > fScores[t] )
			fSmallestPoints = min( fSmallestPoints, m_UnlockEntries[a].m_fRequirement[t] );
	
	if( fSmallestPoints == FLT_MAX )
		return 0;  // no match found
	return fSmallestPoints - fScores[t];
}

/* Update the cached pointers.  Only call this when it's likely to have changed,
 * such as on load, or when a song title changes in the editor. */
void UnlockManager::UpdateCachedPointers()
{
	FOREACH( UnlockEntry, m_UnlockEntries, e )
	{
		switch( e->m_Type )
		{
		case UnlockRewardType_Song:
			e->m_pSong = SONGMAN->FindSong( e->m_cmd.GetArg(1) );
			if( e->m_pSong == NULL )
				LOG->Warn( "Unlock: Cannot find song matching \"%s\"", e->m_cmd.GetArg(1).s.c_str() );
			break;
		case UnlockRewardType_Steps:
			e->m_pSong = SONGMAN->FindSong( e->m_cmd.GetArg(1) );
			if( e->m_pSong == NULL )
			{
				LOG->Warn( "Unlock: Cannot find song matching \"%s\"", e->m_cmd.GetArg(1).s.c_str() );
				break;
			}

			e->m_dc = StringToDifficulty( e->m_cmd.GetArg(2) );
			if( e->m_dc == DIFFICULTY_INVALID )
			{
				LOG->Warn( "Unlock: Invalid difficulty \"%s\"", e->m_cmd.GetArg(2).s.c_str() );
				break;
			}

			break;
		case UnlockRewardType_Course:
			e->m_pCourse = SONGMAN->FindCourse( e->m_cmd.GetArg(1) );
			if( e->m_pCourse == NULL )
				LOG->Warn( "Unlock: Cannot find course matching \"%s\"", e->m_cmd.GetArg(1).s.c_str() );
			break;
		case UnlockRewardType_Modifier:
			// nothing to cache
			break;
		default:
			ASSERT(0);
		}
	}
}



void UnlockManager::UnlockEntryID( int iEntryID )
{
	FOREACH_PlayerNumber( pn )
		if( PROFILEMAN->IsPersistentProfile(pn) )
			PROFILEMAN->GetProfile(pn)->m_UnlockedEntryIDs.insert( iEntryID );

	PROFILEMAN->GetMachineProfile()->m_UnlockedEntryIDs.insert( iEntryID );
}

void UnlockManager::PreferUnlockEntryID( int iUnlockEntryID )
{
	for( unsigned i = 0; i < m_UnlockEntries.size(); ++i )
	{
		UnlockEntry &pEntry = m_UnlockEntries[i];
		if( pEntry.m_iEntryID != iUnlockEntryID )
			continue;

		if( pEntry.m_pSong != NULL )
			GAMESTATE->m_pPreferredSong = pEntry.m_pSong;
		if( pEntry.m_pCourse != NULL )
			GAMESTATE->m_pPreferredCourse = pEntry.m_pCourse;
	}
}

int UnlockManager::GetNumUnlocks() const
{
	return m_UnlockEntries.size();
}

void UnlockManager::GetUnlocksByType( UnlockRewardType t, vector<UnlockEntry *> &apEntries )
{
	for( unsigned i = 0; i < m_UnlockEntries.size(); ++i )
		if( m_UnlockEntries[i].IsValid() && m_UnlockEntries[i].m_Type == t )
			apEntries.push_back( &m_UnlockEntries[i] );
}

void UnlockManager::GetSongsUnlockedByEntryID( vector<Song *> &apSongsOut, int iUnlockEntryID )
{
	vector<UnlockEntry *> apEntries;
	GetUnlocksByType( UnlockRewardType_Song, apEntries );

	for( unsigned i = 0; i < apEntries.size(); ++i )
		if( apEntries[i]->m_iEntryID == iUnlockEntryID )
			apSongsOut.push_back( apEntries[i]->m_pSong );
}

void UnlockManager::GetStepsUnlockedByEntryID( vector<Song *> &apSongsOut, vector<Difficulty> &apDifficultyOut, int iUnlockEntryID )
{
	vector<UnlockEntry *> apEntries;
	GetUnlocksByType( UnlockRewardType_Steps, apEntries );

	for( unsigned i = 0; i < apEntries.size(); ++i )
	{
		if( apEntries[i]->m_iEntryID == iUnlockEntryID )
		{
			apSongsOut.push_back( apEntries[i]->m_pSong );
			apDifficultyOut.push_back( apEntries[i]->m_dc );
		}
	}
}


// lua start
#include "LuaBinding.h"

class LunaUnlockEntry: public Luna<UnlockEntry>
{
public:
	LunaUnlockEntry() { LUA->Register( Register ); }

	static int IsLocked( T* p, lua_State *L )		{ lua_pushboolean(L, p->IsLocked() ); return 1; }
	static int GetDescription( T* p, lua_State *L )		{ lua_pushstring(L, p->GetDescription() ); return 1; }
	static int GetUnlockRewardType( T* p, lua_State *L )	{ lua_pushnumber(L, p->m_Type ); return 1; }
	static int GetRequirement( T* p, lua_State *L )		{ UnlockRequirement i = (UnlockRequirement)IArg(1); lua_pushnumber(L, p->m_fRequirement[i] ); return 1; }

	static void Register(lua_State *L)
	{
		ADD_METHOD( IsLocked );
		ADD_METHOD( GetDescription );
		ADD_METHOD( GetUnlockRewardType );
		ADD_METHOD( GetRequirement );

		Luna<T>::Register( L );
	}
};

LUA_REGISTER_CLASS( UnlockEntry )

class LunaUnlockManager: public Luna<UnlockManager>
{
public:
	LunaUnlockManager() { LUA->Register( Register ); }

	static int FindEntryID( T* p, lua_State *L )		{ RString sName = SArg(1); int i = p->FindEntryID(sName); if( i == -1 ) lua_pushnil(L); else lua_pushnumber(L, i); return 1; }
	static int UnlockEntryID( T* p, lua_State *L )		{ int iUnlockEntryID = IArg(1); p->UnlockEntryID(iUnlockEntryID); return 0; }
	static int PreferUnlockEntryID( T* p, lua_State *L )	{ int iUnlockEntryID = IArg(1); p->PreferUnlockEntryID(iUnlockEntryID); return 0; }
	static int GetNumUnlocks( T* p, lua_State *L )		{ lua_pushnumber( L, p->GetNumUnlocks() ); return 1; }
	static int GetUnlockEntry( T* p, lua_State *L )		{ int iIndex = IArg(1); p->m_UnlockEntries[iIndex].PushSelf(L); return 1; }
	static int GetSongsUnlockedByEntryID( T* p, lua_State *L )
	{
		vector<Song *> apSongs;
		UNLOCKMAN->GetSongsUnlockedByEntryID( apSongs, IArg(1) );
		LuaHelpers::CreateTableFromArray( apSongs, L );
		return 1;
	}

	static int GetStepsUnlockedByEntryID( T* p, lua_State *L )
	{
		// Return the song each steps are associated with, too.
		vector<Song *> apSongs;
		vector<Difficulty> apDifficulty;
		UNLOCKMAN->GetStepsUnlockedByEntryID( apSongs, apDifficulty, IArg(1) );
		LuaHelpers::CreateTableFromArray( apSongs, L );
		LuaHelpers::CreateTableFromArray( apDifficulty, L );
		return 2;
	}

	static void Register(lua_State *L)
	{
		ADD_METHOD( FindEntryID );
		ADD_METHOD( UnlockEntryID );
		ADD_METHOD( PreferUnlockEntryID );
		ADD_METHOD( GetNumUnlocks );
		ADD_METHOD( GetUnlockEntry );
		ADD_METHOD( GetSongsUnlockedByEntryID );
		ADD_METHOD( GetStepsUnlockedByEntryID );

		Luna<T>::Register( L );

		// Add global singleton if constructed already.  If it's not constructed yet,
		// then we'll register it later when we reinit Lua just before 
		// initializing the display.
		if( UNLOCKMAN )
		{
			lua_pushstring(L, "UNLOCKMAN");
			UNLOCKMAN->PushSelf( L );
			lua_settable(L, LUA_GLOBALSINDEX);
		}
	}
};

LUA_REGISTER_CLASS( UnlockManager )
// lua end

/*
 * (c) 2001-2004 Kevin Slaughter, Andrew Wong, Glenn Maynard
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
