#ifndef GRAY_ARROW_H
#define GRAY_ARROW_H
/*
-----------------------------------------------------------------------------
 Class: ReceptorArrow

 Desc: A gray arrow that "receives" ColorNotes.

 Copyright (c) 2001-2002 by the person(s) listed below.  All rights reserved.
	Ben Nordstrom
	Chris Danford
-----------------------------------------------------------------------------
*/


#include "Sprite.h"
#include "PlayerNumber.h"

class ReceptorArrow : public ActorFrame
{
public:
	ReceptorArrow();
	bool Load( CString NoteSkin, PlayerNumber pn, int iColNo );

	virtual void Draw();
	virtual void  Update( float fDeltaTime );
	void Step();
	void UpdateBars() { m_bIsPressed = true; };
private:

	Sprite m_sprReceptorWaiting;
	Sprite m_sprReceptorGo;

	CString m_sReceptorStepCommand;

	Sprite m_sprPressBlock;
	bool m_bIsPressed;
};

#endif 
