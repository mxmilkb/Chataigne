/*
  ==============================================================================

    StateTransitionUI.h
    Created: 28 Oct 2016 8:20:06pm
    Author:  bkupe

  ==============================================================================
*/

#pragma once

#include "../StateTransition.h"
#include "../../State/ui/StateViewUI.h"

class StateTransitionUI :
	public BaseItemMinimalUI<StateTransition>,
	public StateViewUI::Listener,
	public StateViewUI::ItemUIListener,
	public StateViewUI::ItemMinimalUIListener
{
public:
	StateTransitionUI(StateTransition * st, StateViewUI * sourceSUI = nullptr, StateViewUI * destSUI = nullptr);
	~StateTransitionUI();

	StateViewUI * sourceSUI;
	StateViewUI * destSUI;

	Path hitPath;

	void updateBounds();
	
	void mouseDown(const MouseEvent &e) override;
    void mouseMove(const MouseEvent &e) override;
	bool keyPressed(const KeyPress &e) override;

	void paint(Graphics &g) override;
	void paintOverChildren(Graphics &g) override;
	void resized() override;

	bool hitTest(int x, int y) override;

	void itemUIMiniModeChanged(BaseItemUI<State>  *) override;
	void itemUIViewPositionChanged(BaseItemMinimalUI<State>*) override;
	void editorSelectionChanged(StateViewUI *) override;
};