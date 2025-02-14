
/*
  ==============================================================================

    MappingLayer.cpp
    Created: 17 Nov 2016 8:00:02pm
    Author:  Ben Kuper

  ==============================================================================
*/

#include "MappingLayer.h"

#include "../../ChataigneSequence.h"

#include "Common/Processor/Mapping/Mapping.h"
#include "Module/ModuleManager.h"

#include "ui/MappingLayerPanel.h"
#include "ui/MappingLayerTimeline.h"
#include "ui/MappingLayerEditor.h"

MappingLayer::MappingLayer(Sequence *_sequence, var params) :
	SequenceLayer(_sequence, "Mapping"),
	mode(nullptr),
    curveValue(nullptr)

{
	
	canInspectChildContainers = true;
	
	mapping.reset(new Mapping(false));
	mapping->editorIsCollapsed = false;
	mapping->editorCanBeCollapsed = false;
	mapping->hideEditorHeader = true;

	alwaysUpdate = addBoolParameter("Always Update", "If checked, the mapping will be processed and output will be sent at each time change of the sequence", false);
	sendOnPlay = addBoolParameter("Send On Play", " If checked, this will force the value to go through the mapping when sequence starts playing", true);
	sendOnStop = addBoolParameter("Send On Stop", " If checked, this will force the value to go through the mapping when sequence stops playing", true);
	sendOnSeek = addBoolParameter("Send On Seek", " If checked, this will force the value to go through the mapping when jumping time", false);

	recordSendMode = addEnumParameter("Record Send Mode", "Choose what to do when recording");
	recordSendMode->addOption("Do not send", DONOTSEND)->addOption("Send original value", SEND_ORIGINAL)->addOption("Send new value", SEND_NEW);

	addChildControllableContainer(&recorder);
	addChildControllableContainer(mapping.get());
	recorder.input->customGetTargetFunc = &ModuleManager::showAllValuesAndGetControllable;
	recorder.input->customGetControllableLabelFunc = &Module::getTargetLabelForValueControllable;
	recorder.input->customCheckAssignOnNextChangeFunc = &ModuleManager::checkControllableIsAValue;

	mode = new EnumParameter("Mode", "Automation Mode, 1D, 2D, 3D or Color");
	mode->addOption("Single Value", MODE_1D);
	mode->addOption("Point 2D (XY)", MODE_2D);
	mode->addOption("Point 3D (XYZ)", MODE_3D);
	mode->addOption("Color (RGBA)", MODE_COLOR);
	mode->setValueWithData((Mode)(int)params.getProperty("mode", MODE_1D));
	//mode->defaultValue = (int)params.getProperty("mode", MODE_1D);
	//mode->resetValue(false);

	mode->hideInEditor = true;
	addParameter(mode); //avoid setting up


	Mode m = mode->getValueDataAsEnum<Mode>();
	if (m == MODE_COLOR) setNiceName("New Color Layer");

	color->setColor(BG_COLOR.brighter(.1f));
	
	setupMappingForCurrentMode();
	uiHeight->setValue(m == MODE_COLOR?50:120);
}

MappingLayer::~MappingLayer()
{
}

void MappingLayer::setupMappingForCurrentMode()
{
	Mode m = mode->getValueDataAsEnum<Mode>();
	int numAutomations = 0;

	if (curveValue != nullptr)
	{
		removeControllable(curveValue);
	}

	switch (m)
	{
	case MODE_1D:
		numAutomations = 1;
		curveValue = addFloatParameter("Value", "Direct curve value of the curve at the current sequence time", 0, 0, 1);
		break;
	case MODE_2D:
		numAutomations = 2;
		curveValue = addPoint2DParameter("Value", "2D Value of the curves");
		break;
	case MODE_3D:
		numAutomations = 3;
		curveValue = addPoint3DParameter("Value", "3D Value of the curves");
		break;

	case MODE_COLOR:
		numAutomations = 0;
		curveValue = new ColorParameter("Value", "Color value of the curve");
		addParameter(curveValue);
		break;
	}

	if (m == MODE_COLOR)
	{
		if (colorManager == nullptr)
		{
			colorManager.reset(new GradientColorManager(sequence->totalTime->floatValue(), !isCurrentlyLoadingData));
			colorManager->allowKeysOutside = false;
			addChildControllableContainer(colorManager.get());
			colorManager->setLength(sequence->totalTime->floatValue());
		}
	}
	else
	{
		if (colorManager != nullptr)
		{
			removeChildControllableContainer(colorManager.get());
			colorManager = nullptr;
		}
	}

	while (automations.size() > numAutomations)
	{
		removeChildControllableContainer(automations.getLast());
		automations.removeLast();
	}

	for (int i = 0; i < numAutomations;i++)
	{
		if(i >= automations.size())
		{
			Automation * a = new Automation("Mapping curve",automations.size() == 0 ? &recorder : nullptr); //only put the record on the first automation for now
			a->hideInEditor = true;
			automations.add(a);

			addChildControllableContainer(automations[i]);
		}

		automations[i]->length->setValue(sequence->totalTime->floatValue());
	}

	curveValue->isControllableFeedbackOnly = true;
	mapping->lockInputTo(curveValue);

	updateCurvesValues();
}

void MappingLayer::updateCurvesValues()
{
	Mode mappingMode = mode->getValueDataAsEnum<Mode>();
	switch (mappingMode)
	{
	case MODE_COLOR:
		((ColorParameter *)curveValue)->setColor(colorManager->currentColor->getColor(), false);
		break;

	case MODE_1D:
	{
		if (recorder.isRecording->boolValue())
		{
			RecordSendMode m = recordSendMode->getValueDataAsEnum<RecordSendMode>();
			if (m == SEND_ORIGINAL)
			{
				if (automations[0] != nullptr && !automations[0]->items.isEmpty()) curveValue->setValue(automations[0]->value->floatValue(), false);
			}
			else if(m == SEND_NEW)
			{
				if (recorder.keys.size() > 0) curveValue->setValue(recorder.keys[recorder.keys.size()-1].y);
			}
		}
		else
		{
			if (automations[0] != nullptr && !automations[0]->items.isEmpty()) curveValue->setValue(automations[0]->value->floatValue(), false);
		}
	}
	break;

	case MODE_2D:
	case MODE_3D:
		var cv;
		for (auto &a : automations) cv.append(a->value->floatValue());
		curveValue->setValue(cv);
		break;
	}

}

void MappingLayer::stopRecorderAndAddKeys()
{
	if (automations.size() == 0) return;

	Array<Point<float>> keys = automations[0]->recorder->stopRecordingAndGetKeys(); 
	if (keys.size() >= 2)
	{
		automations[0]->addItems(keys, true, true);
	}
}

String MappingLayer::getHelpID()
{
	Mode mappingMode = mode->getValueDataAsEnum<Mode>(); 
	switch (mappingMode)
	{
	case MODE_COLOR: return "ColorLayer";  break;

	case MODE_1D:
	case MODE_2D:
	case MODE_3D:
		return "AutomationLayer";
		break;
	}

	return "UnknownLayer";
}

var MappingLayer::getJSONData()
{
	var data = SequenceLayer::getJSONData();
	data.getDynamicObject()->setProperty("mapping", mapping->getJSONData());
	for (int i = 0; i < automations.size(); i++)
	{
		var aData = automations[i]->getJSONData();
		if(!aData.isVoid()) data.getDynamicObject()->setProperty("automation"+String(i), aData);
	}
	if (colorManager != nullptr)
	{
		data.getDynamicObject()->setProperty("colors", colorManager->getJSONData());
	}

	var rData = recorder.getJSONData();
	if(!rData.isVoid()) data.getDynamicObject()->setProperty("recorder", rData);

	return data;
}

void MappingLayer::loadJSONDataInternal(var data)
{
	SequenceLayer::loadJSONDataInternal(data);
	mapping->loadJSONData(data.getProperty("mapping", var()));
	for (int i = 0; i < automations.size(); i++)
	{
		automations[i]->loadJSONData(data.getProperty("automation"+String(i), var()));
	}
	if (colorManager != nullptr)
	{
		colorManager->loadJSONData(data.getProperty("colors", var()));
	}

	recorder.loadJSONData(data.getProperty("recorder", var()));
}

void MappingLayer::selectAll(bool addToSelection)
{
	if (mode->getValueDataAsEnum<Mode>() == MODE_COLOR)
	{
		deselectThis(colorManager->items.size() == 0);
		colorManager->askForSelectAllItems(addToSelection);
	}
	else if (automations.size() > 0)
	{
		deselectThis(automations[0]->items.size() == 0);
		automations[0]->askForSelectAllItems(addToSelection);
	}
}

SequenceLayerPanel * MappingLayer::getPanel()
{
	return new MappingLayerPanel(this);
}

SequenceLayerTimeline * MappingLayer::getTimelineUI()
{
	return new MappingLayerTimeline(this);
}

void MappingLayer::onContainerParameterChangedInternal(Parameter * p)
{
	if (p == mode)
	{
		setupMappingForCurrentMode();
	}
	else if (p == alwaysUpdate)
	{
		mapping->setProcessMode(alwaysUpdate->boolValue() ? Mapping::MANUAL : Mapping::VALUE_CHANGE);
	}
}

void MappingLayer::onContainerTriggerTriggered(Trigger * t)
{
	SequenceLayer::onContainerTriggerTriggered(t);
}

void MappingLayer::onControllableFeedbackUpdateInternal(ControllableContainer * cc, Controllable * c)
{
	bool doUpdate = false;
	if (mode->getValueDataAsEnum<Mode>() == MODE_COLOR)
	{
		doUpdate = c == colorManager->currentColor;
	} else
	{
		for (auto &a : automations)
		{
			if (a->value == c)
			{
				doUpdate = true;
				break;
			}
		}
	}

	if(doUpdate) updateCurvesValues();
}

void MappingLayer::sequenceTotalTimeChanged(Sequence *)
{
	if (mode->getValueDataAsEnum<Mode>() == MODE_COLOR)
	{
		colorManager->setLength(sequence->totalTime->floatValue());
	}
	else
	{
		for (auto &a : automations) a->length->setValue(sequence->totalTime->floatValue());
	}
}

void MappingLayer::sequenceCurrentTimeChanged(Sequence *, float prevTime, bool evaluateSkippedData)
{
	if (!enabled->boolValue() || !sequence->enabled->boolValue()) return;
	if (mode == nullptr) return; //not init yet

	if (mode->getValueDataAsEnum<Mode>() == MODE_COLOR)
	{
		if (colorManager == nullptr) return;
		colorManager->position->setValue(sequence->currentTime->floatValue());
	}

	for (auto &a : automations) a->position->setValue(sequence->currentTime->floatValue());
	
	if (sequence->isPlaying->boolValue())
	{
		if (automations.size() > 0)
		{
			if (automations[0]->recorder->isRecording->boolValue())
			{
				if (prevTime < sequence->currentTime->floatValue())
				{
					automations[0]->recorder->addKeyAt(sequence->currentTime->floatValue());
				} else
				{
					automations[0]->recorder->startRecording();
				}
			}
		}
	}

	if (alwaysUpdate->boolValue() || (sequence->isSeeking && sendOnSeek->boolValue()))
	{
		updateCurvesValues();
		
		if (mode->getValueDataAsEnum<Mode>() == MODE_1D)
		{
			if (automations[0] != nullptr && !automations[0]->items.isEmpty()) mapping->process(true); //process only if automation has keys
		}
		else
		{
			mapping->process(true);
		}
	}
}

void MappingLayer::sequencePlayStateChanged(Sequence *)
{
	if (!enabled->boolValue() || !sequence->enabled->boolValue()) return;

	if (automations.size() > 0)
	{
		if (sequence->isPlaying->boolValue())
		{
			if(recorder.shouldRecord()) automations[0]->recorder->startRecording();
			if (sendOnPlay->boolValue())
			{
				updateCurvesValues();

				if (mode->getValueDataAsEnum<Mode>() == MODE_1D)
				{
					if (automations[0] != nullptr && !automations[0]->items.isEmpty()) mapping->process(true); //process only if automation has keys
				}
				else
				{
					mapping->process(true);
				}
			}
		}
		else
		{
			if (automations[0]->recorder->isRecording->boolValue())
			{
				stopRecorderAndAddKeys();
			}
			if (sendOnStop->boolValue())
			{
				updateCurvesValues();

				if (mode->getValueDataAsEnum<Mode>() == MODE_1D)
				{
					if (automations[0] != nullptr && !automations[0]->items.isEmpty()) mapping->process(true); //process only if automation has keys
				}
				else
				{
					mapping->process(true);
				}
			}
		}
	}
}

void MappingLayer::sequenceLooped(Sequence *)
{
	if (automations.size() > 0 && automations[0]->recorder->isRecording->boolValue())
	{
		stopRecorderAndAddKeys();
	}
}

bool MappingLayer::paste()
{
	var data = JSON::fromString(SystemClipboard::getTextFromClipboard());
	String type = data.getProperty("itemType", "");
	if (automations.size() > 0 && type == automations[0]->itemDataType)
	{
		automations[0]->askForPaste();
		return true;
	}

	return SequenceLayer::paste();
}
