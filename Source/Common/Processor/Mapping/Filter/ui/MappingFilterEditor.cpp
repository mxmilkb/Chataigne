/*
  ==============================================================================

    MappingFilterEditor.cpp
    Created: 4 Feb 2017 8:31:15pm
    Author:  Ben

  ==============================================================================
*/

#include "MappingFilterEditor.h"

MappingFilterEditor::MappingFilterEditor(MappingFilter * m, bool isRoot) :
	BaseItemEditor(m,isRoot),
	filter(m),
	filteredUI(nullptr)
{
	updateFilteredUI();
	filter->addAsyncFilterListener(this);
}

MappingFilterEditor::~MappingFilterEditor()
{
	if (!inspectable.wasObjectDeleted()) filter->removeAsyncFilterListener(this);
}

void MappingFilterEditor::resizedInternalHeaderItemInternal(Rectangle<int>& r)
{
	if (filteredUI != nullptr) filteredUI->setBounds(r.removeFromRight(140).reduced(2));
}


void MappingFilterEditor::updateFilteredUI()
{
	if (filteredUI != nullptr && filteredUI->parameter == filter->filteredParameter) return;

	if (filteredUI != nullptr)
	{
		removeChildComponent(filteredUI.get());
	}

	filteredUI = nullptr;

	if (filter->filteredParameter != nullptr)
	{
		filteredUI.reset((ParameterUI*)(filter->filteredParameter->createDefaultUI()));
		filteredUI->showLabel = false;
		addAndMakeVisible(filteredUI.get());
	}

	resized();
}

void MappingFilterEditor::newMessage(const MappingFilter::FilterEvent & e)
{
	switch (e.type)
	{
	case MappingFilter::FilterEvent::FILTER_PARAM_CHANGED:
		filteredParamChangedAsync(e.filter);
		break;
            
        default:
            break;
	}
}

void MappingFilterEditor::filteredParamChangedAsync(MappingFilter *)
{
	updateFilteredUI();
}
