#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
// Spawnflags

class CGhostEntity : public CBaseAnimating
{
public:
	DECLARE_CLASS( CGhostEntity, CBaseAnimating );
	DECLARE_DATADESC();
 
	CGhostEntity()
	{
		m_bActive = false;
	}
	void Spawn( void );
	void Precache( void );
	void MoveThink( void );
 
	// Input function
	void InputToggle( inputdata_t &inputData );
 
private:
	bool	m_bActive;
	float	m_flNextChangeTime;
};
//TODO make this a dynamic model


LINK_ENTITY_TO_CLASS( ghost_entity, CGhostEntity );
 
// Start of our data description for the class
BEGIN_DATADESC( CGhostEntity )
 
	// Save/restore our active state
	DEFINE_FIELD( m_bActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flNextChangeTime, FIELD_TIME ),
 
	// Links our input name from Hammer to our input member function
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
 
	// Declare our think function
	DEFINE_THINKFUNC( MoveThink ),
 
END_DATADESC()

#define ENTITY_MODEL  "models/cone.mdl"

//-----------------------------------------------------------------------------
// Purpose: Precache assets used by the entity
//-----------------------------------------------------------------------------
void CGhostEntity::Precache( void )
{
	PrecacheModel( ENTITY_MODEL );
 
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: Sets up the entity's initial state
//-----------------------------------------------------------------------------
void CGhostEntity::Spawn( void )
{
	Precache();
	
	SetModel( ENTITY_MODEL );
	SetSolid( SOLID_NONE );

}

//-----------------------------------------------------------------------------
// Purpose: Think function to move the ghost
//-----------------------------------------------------------------------------
void CGhostEntity::MoveThink( void )
{
	EntityText(0, "Ghost", 0);
	// See if we should change direction again
	//if ( m_flNextChangeTime < gpGlobals->curtime )
	//{
		
		// Randomly take a new direction and speed
		//TODO change this V
		Vector vecNewVelocity = RandomVector( -64.0f, 64.0f );
		SetAbsVelocity( vecNewVelocity );
 
		// Randomly change it again within one to three seconds
		//TODO change this to not be random
		//m_flNextChangeTime = gpGlobals->curtime + random->RandomFloat( 1.0f, 3.0f );
	//}
 
	// Snap our facing to where we're heading
	Vector velFacing = GetAbsVelocity();
	QAngle angFacing;
	VectorAngles( velFacing, angFacing );
 	SetAbsAngles( angFacing );
 
	// Think at 100Hz
	//TODO make a global var for setting the frequency
	SetNextThink( gpGlobals->curtime + 0.01f );
}

//-----------------------------------------------------------------------------
// Purpose: Toggle the movement of the entity
//-----------------------------------------------------------------------------
void CGhostEntity::InputToggle( inputdata_t &inputData )
{
	// Toggle our active state
	if ( !m_bActive )
	{
		// Start thinking
		SetThink( &CGhostEntity::MoveThink );
		// set our next think
		SetNextThink( gpGlobals->curtime + 0.01f );
 
		// Not affected by anything but moves through the world
		SetMoveType( MOVETYPE_NOCLIP );
 
		// Force MoveThink() to choose a new speed and direction immediately
		m_flNextChangeTime = gpGlobals->curtime;
 
		// Update m_bActive to reflect our new state
		m_bActive = true;
	}
	else
	{
		// Stop thinking
		SetThink( NULL );
 
		// Stop moving
		SetAbsVelocity( vec3_origin );
 		SetMoveType( MOVETYPE_NONE );
 
		m_bActive = false;
	}
}

CON_COMMAND(create_ghost_entity, "Creates an instance of the sdk model entity in front of the player.")
{
	Vector vecForward;
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if(!pPlayer)
	{
		Warning("Could not determine calling player!\n");
		return;
	}
 
	AngleVectors( pPlayer->EyeAngles(), &vecForward );
	CBaseEntity *pEnt = CreateEntityByName( "ghost_entity" );
	if ( pEnt )
	{
		Vector vecOrigin = pPlayer->GetAbsOrigin() + vecForward * 256 + Vector(0,0,64);
		QAngle vecAngles(0, pPlayer->GetAbsAngles().y - 90, 0);
		pEnt->SetAbsOrigin(vecOrigin);
		pEnt->SetAbsAngles(vecAngles);
		DispatchSpawn(pEnt);
	}
}