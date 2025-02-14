/*
  ==============================================================================

	StreamingModule.cpp
	Created: 5 Jan 2018 10:39:38am
	Author:  Ben

  ==============================================================================
*/

#include "StreamingModule.h"
#include "commands/SendStreamRawDataCommand.h"
#include "commands/SendStreamStringCommand.h"
#include "commands/SendStreamValuesCommand.h"
#include "commands/SendStreamStringValuesCommand.h"
#include "UI/ChataigneAssetManager.h"

StreamingModule::StreamingModule(const String & name) :
	Module(name)
{
	includeValuesInSave = true;
	setupIOConfiguration(true, true);
	canHandleRouteValues = hasOutput;

	streamingType = moduleParams.addEnumParameter("Protocol", "Protocol for treating the incoming data");
	streamingType->addOption("Lines", LINES)->addOption("Raw", RAW)->addOption("Data255", DATA255)->addOption("COBS", COBS);

	autoAdd = moduleParams.addBoolParameter("Auto Add", "If checked, incoming data will be parsed depending on the Message Structure parameter, and if eligible will be added as values", true);
	messageStructure = moduleParams.addEnumParameter("Message Structure", "The expected structure of a message, determining how it should be interpreted to auto create values from it");
	firstValueIsTheName = moduleParams.addBoolParameter("First value is the name", "If checked, the first value of a parsed message will be used to name the value, otherwise each values will be named by their index", true);
	buildMessageStructureOptions();

	defManager->add(CommandDefinition::createDef(this, "", "Send string", &SendStreamStringCommand::create, CommandContext::BOTH));
	defManager->add(CommandDefinition::createDef(this, "", "Send values as string", &SendStreamStringValuesCommand::create, CommandContext::BOTH));
	defManager->add(CommandDefinition::createDef(this, "", "Send raw bytes", &SendStreamRawDataCommand::create, CommandContext::BOTH));
	defManager->add(CommandDefinition::createDef(this, "", "Send custom values", &SendStreamValuesCommand::create, CommandContext::BOTH));
	defManager->add(CommandDefinition::createDef(this, "", "Send hex data", &SendStreamStringCommand::create, CommandContext::BOTH)->addParam("mode", SendStreamStringCommand::DataMode::HEX));
	
	scriptObject.setMethod(sendId, StreamingModule::sendStringFromScript);
	scriptObject.setMethod(sendBytesId, StreamingModule::sendBytesFromScript);
	
	scriptManager->scriptTemplate += ChataigneAssetManager::getInstance()->getScriptTemplate("streaming");

	valuesCC.userCanAddControllables = true;
	valuesCC.customUserCreateControllableFunc = &StreamingModule::showMenuAndCreateValue;
}

StreamingModule::~StreamingModule()
{

}

void StreamingModule::setAutoAddAvailable(bool value)
{
	if(!value) autoAdd->setValue(false);
	autoAdd->hideInEditor = !value;
	streamingType->hideInEditor = !value;
	messageStructure->hideInEditor = !value;
	firstValueIsTheName->hideInEditor = !value;
}

void StreamingModule::buildMessageStructureOptions()
{
	StreamingType t = streamingType->getValueDataAsEnum<StreamingType>();
	messageStructure->clearOptions();

	switch (t)
	{
	
	case LINES:
	{
		messageStructure->addOption("Space separated", LINES_SPACE)
			->addOption("Tab separated", LINES_TAB)
			->addOption("Comma (,) separated", LINES_COMMA)
			->addOption("Colon (:) separated", LINES_COLON)
			->addOption("Semicolon (;) separated", LINES_SEMICOLON)
			->addOption("Equals (=) separated", LINES_EQUALS)
			->addOption("No separation (will create only one parameter)", NO_SEPARATION);
	}
	break;
	
	case RAW:
	case COBS:
	case DATA255:
	{
		messageStructure->addOption("1 value per byte", RAW_1BYTE)->addOption("4x4 (floats)", RAW_FLOATS)->addOption("4x4 (RGBA colors)", RAW_COLORS);
	}
	break;
	}
}

void StreamingModule::processDataLine(const String & msg)
{
	if (!enabled->boolValue()) return;
	if (logIncomingData->boolValue()) NLOG(niceName, "Message received : " << (msg.isNotEmpty() ? msg : "(Empty message)"));
	inActivityTrigger->trigger();

	const String message = msg.removeCharacters("\r\n");
	if (message.isEmpty()) return;

	processDataLineInternal(message);
	
	scriptManager->callFunctionOnAllItems(dataEventId, message);

	MessageStructure s = messageStructure->getValueDataAsEnum<MessageStructure>();
	StringArray valuesString;
	String separator;
	switch (s)
	{
	case LINES_SPACE: separator = " "; break;
	case LINES_TAB:   separator = "\t"; break;
	case LINES_COMMA: separator = ","; break;
	case LINES_EQUALS: separator = "="; break;
	case LINES_COLON: separator = ":"; break;
	case LINES_SEMICOLON: separator = ";"; break;
    default:
        break;
	}

	if(s != NO_SEPARATION) valuesString.addTokens(message, separator, "\"");
	else
	{
		if (firstValueIsTheName->boolValue()) valuesString.add("Value");
		valuesString.add(message);
	}

	if (valuesString.size() == 0)
	{
		//LOG("No usable data");
		return;
	}

	if (firstValueIsTheName->boolValue())
	{
		String valueName = valuesString[0];
		int numArgs = valuesString.size() - 1;
		
		Controllable * c = valuesCC.getControllableByName(valueName, true);

		if (c == nullptr)
		{
			if (!autoAdd->boolValue()) return;

			if (numArgs > 0 && valuesString[1].getFloatValue() == 0 && !valuesString[1].containsChar('0'))
			{
				valuesString.remove(0);
				c = new StringParameter(valueName, valueName, valuesString.joinIntoString(" "));
			}
			else
			{
				switch (numArgs)
				{
				case 0: c = new Trigger(valueName, valueName); break;
				case 1:	c = new FloatParameter(valueName, valueName, valuesString[1].getFloatValue()); break;
				case 2: c = new Point2DParameter(valueName, valueName); ((Point2DParameter*)c)->setPoint(valuesString[1].getFloatValue(), valuesString[2].getFloatValue()); break;
				case 3: c = new Point3DParameter(valueName, valueName); ((Point3DParameter*)c)->setVector(valuesString[1].getFloatValue(), valuesString[2].getFloatValue(), valuesString[3].getFloatValue()); break;
				case 4: c = new ColorParameter(valueName, valueName, Colour::fromFloatRGBA(valuesString[1].getFloatValue(), valuesString[2].getFloatValue(), valuesString[3].getFloatValue(), valuesString[4].getFloatValue()));
				default:
				{
					valuesString.remove(0);
					c = new StringParameter(valueName, valueName, valuesString.joinIntoString(" "));
				}

				}
			}

			if (c != nullptr)
			{
				c->isCustomizableByUser = true;
				c->isRemovableByUser = true;
				c->saveValueOnly = false;
				valuesCC.addControllable(c);
			}
		} else
		{
			Controllable::Type t = c->type;
			switch (t)
			{
			case Controllable::TRIGGER:
				((Trigger *)c)->trigger();
				break;

			case Controllable::FLOAT:
				if (numArgs >= 1) ((FloatParameter *)c)->setValue(valuesString[1].getFloatValue());
				break;

			case Controllable::INT:
				if (numArgs >= 1) ((IntParameter*)c)->setValue(valuesString[1].getIntValue());
				break;

			case Controllable::POINT2D:
				if (numArgs >= 2) ((Point2DParameter *)c)->setPoint(valuesString[1].getFloatValue(), valuesString[2].getFloatValue());
				break;

			case Controllable::POINT3D:
				if (numArgs >= 3) ((Point3DParameter *)c)->setVector(valuesString[1].getFloatValue(), valuesString[2].getFloatValue(), valuesString[3].getFloatValue());
				break; 

			case Controllable::COLOR:
				if (numArgs >= 4) ((ColorParameter *)c)->setColor(Colour::fromFloatRGBA(valuesString[1].getFloatValue(), valuesString[2].getFloatValue(), valuesString[3].getFloatValue(), valuesString[4].getFloatValue()));
				break;
                    
			case Controllable::STRING:
			{
				valuesString.remove(0);
				if(numArgs >= 1) ((StringParameter *)c)->setValue(valuesString.joinIntoString(" "));
			}
			break;

            default:
                break;

			}
		}
		
	} else
	{
		int numArgs = valuesString.size();

		for (int i = 0; i < numArgs; i++)
		{
			Controllable* c = valuesCC.getControllableByName("Value " + String(i), true);

			if (c == nullptr)
			{
				if (autoAdd->boolValue())
				{
					if (valuesString[i].getFloatValue() == 0 && !valuesString[i].containsChar('0'))
					{
						c = new StringParameter("Value " + String(i), "Value " + String(i), "");
					}
					else
					{
						c = new FloatParameter("Value " + String(i), "Value " + String(i), 0);
					}

					if (c != nullptr)
					{
						c->isCustomizableByUser = true;
						c->isRemovableByUser = true;
						c->saveValueOnly = false;

						valuesCC.addControllable(c);
					}

				}
			}
			
			if (c != nullptr)
			{
				switch (c->type)
				{
				case Controllable::FLOAT: ((FloatParameter*)c)->setValue(valuesString[i].getFloatValue()); break;
				case Controllable::INT:((IntParameter*)c)->setValue(valuesString[i].getIntValue()); break;
				case Controllable::STRING: ((StringParameter*)c)->setValue(valuesString[i]); break;
				default:
					((Parameter*)c)->setValue(valuesString[i].getFloatValue()); break;
					break;
				}
			}
		}
	}
}

void StreamingModule::processDataBytes(Array<uint8_t> data)
{
	if (!enabled->boolValue()) return;
	if (logIncomingData->boolValue())
	{
		String msg = String(data.size()) + "bytes received :";
		for (auto &d : data) msg += "\n" + String(d);
		NLOG(niceName, msg);
	}

	inActivityTrigger->trigger();

	processDataBytesInternal(data);

	if (scriptManager->items.size() > 0)
	{
		var args;
		for (auto &d : data) args.append(d);
		scriptManager->callFunctionOnAllItems(dataEventId, args);
	}


	MessageStructure st = messageStructure->getValueDataAsEnum<MessageStructure>();

	switch (st)
	{
	case RAW_1BYTE:
	{
		int numArgs = data.size();
		if (autoAdd->boolValue())
		{
			int numValues = valuesCC.controllables.size();
			while (numValues < numArgs)
			{
				IntParameter * p = new IntParameter("Value " + String(numValues), "Value " + String(numValues), 0, 0, 255);
				p->isCustomizableByUser = true;
				p->isRemovableByUser = true;
				p->saveValueOnly = false;
				p->hexMode = true;
				valuesCC.addControllable(p);
				numValues = valuesCC.controllables.size();
			}
		}

		for (int i = 0; i < numArgs; i++)
		{
			IntParameter * c = dynamic_cast<IntParameter *>(valuesCC.controllables[i]);
			if (c != nullptr)
			{
				c->setValue(data[i]);
			}
		}
	}
	break;

	case RAW_FLOATS:
	{
		int numArgs = data.size() / 4;
		
		if (autoAdd->boolValue())
		{
			int numValues = valuesCC.controllables.size();
			while (numValues < numArgs)
			{
				FloatParameter * p = new FloatParameter("Value " + String(numValues), "Value " + String(numValues), 0);
				p->isCustomizableByUser = true;
				p->isRemovableByUser = true;
				p->saveValueOnly = false;
				valuesCC.addControllable(p);

				numValues = valuesCC.controllables.size();
			}

		}
		
		for (int i = 0; i < numArgs; i++)
		{
			FloatParameter * c = dynamic_cast<FloatParameter *>(valuesCC.controllables[i]);
			if (c != nullptr)
			{
				float value = data[i * 4] + (data[i * 4 + 1] << 8) + (data[i * 4 + 2] << 16) + (data[i * 4 + 3] << 24);
				c->setValue(value);
			}
		}
	}
	break;

	case RAW_COLORS:
	{
		int numArgs = data.size() / 4;

		if (autoAdd->boolValue())
		{
			int numValues = valuesCC.controllables.size();
			while (numValues < numArgs)
			{
				ColorParameter * colP = new ColorParameter("Value " + String(numValues), "Value " + String(numValues));
				colP->isCustomizableByUser = true;
				colP->isRemovableByUser = true;
				colP->saveValueOnly = false;

				valuesCC.addControllable(colP);
				numValues = valuesCC.controllables.size();
			}
		}
		

		for (int i = 0; i < numArgs; i++)
		{
			ColorParameter * c = dynamic_cast<ColorParameter *>(valuesCC.controllables[i]);
			if (c != nullptr)
			{
				Colour col = Colour(data[i * 4], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]);
				c->setColor(col);
			}
		}
	}
	break;
            
    default:
        break;
    }
	
}

void StreamingModule::sendMessage(const String & message)
{
	if (!enabled->boolValue()) return;
	if(!isReadyToSend())
	{
		if (logOutgoingData->boolValue()) NLOGWARNING(niceName, "Can't send message, output is not connected");
		return;
	}
	
	sendMessageInternal(message);
	outActivityTrigger->trigger();
	
	if (logOutgoingData->boolValue()) NLOG(niceName, "Sending : " << message);
}

void StreamingModule::sendBytes(Array<uint8> bytes)
{
	if (!enabled->boolValue()) return;
	if(!isReadyToSend())
	{
		if (logOutgoingData->boolValue()) NLOGWARNING(niceName, "Can't send  data, output is not connected.");
		return;
	}

	if (streamingType->getValueDataAsEnum<StreamingType>() == COBS)
	{
		Array<uint8> sourceBytes = Array<uint8>(bytes);
		cobs_encode(sourceBytes.getRawDataPointer(), bytes.size(), bytes.getRawDataPointer());
	}

	sendBytesInternal(bytes);
	outActivityTrigger->trigger();

	if (logOutgoingData->boolValue())
	{
		String s = "Sending " + String(bytes.size()) + " bytes :";
		for (auto& b : bytes) s += "\n0x" + String::toHexString(b);
		NLOG(niceName, s);
	}
}

void StreamingModule::showMenuAndCreateValue(ControllableContainer * container)
{
	StringArray filters = ControllableFactory::getTypesWithout(StringArray(EnumParameter::getTypeStringStatic(), TargetParameter::getTypeStringStatic(), FileParameter::getTypeStringStatic()));
	Controllable * c = ControllableFactory::showFilteredCreateMenu(filters);
	if (c == nullptr) return;

	AlertWindow window("Add a value", "Configure the parameters for this value", AlertWindow::AlertIconType::NoIcon);
	window.addTextEditor("address", "MyValue", "OSC Address");
	window.addButton("OK", 1, KeyPress(KeyPress::returnKey));
	window.addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));

	int result = window.runModalLoop();

	if (result)
	{
		String addString = window.getTextEditorContents("address").replace(" ", "");
		c->setNiceName(addString);
		c->isCustomizableByUser = true;
		c->isRemovableByUser = true;
		c->saveValueOnly = false;
		container->addControllable(c);
	} else
	{
		delete c;
	}
}

void StreamingModule::onControllableFeedbackUpdateInternal(ControllableContainer * cc, Controllable * c)
{
	Module::onControllableFeedbackUpdateInternal(cc, c);

	if (c == autoAdd || c == streamingType)
	{
		bool streamingLines = streamingType->getValueDataAsEnum<StreamingType>() == LINES;
		messageStructure->setEnabled(autoAdd->boolValue());
		firstValueIsTheName->setEnabled(streamingLines && autoAdd->boolValue());
	}

	if (c == streamingType)
	{
		buildMessageStructureOptions();
	} 
}

void StreamingModule::loadJSONDataInternal(var data)
{
	Module::loadJSONDataInternal(data);
	for (auto& v : valuesCC.controllables) v->isCustomizableByUser = true;
}

var StreamingModule::sendStringFromScript(const var::NativeFunctionArgs & a)
{
	StreamingModule * m = getObjectFromJS<StreamingModule>(a);
	if (a.numArguments == 0) return var();
	m->sendMessage(a.arguments[0].toString());
	return var();
}

var StreamingModule::sendBytesFromScript(const var::NativeFunctionArgs & a)
{
	StreamingModule * m = getObjectFromJS<StreamingModule>(a);
	if (a.numArguments == 0) return var();
	Array<uint8> data;
	for (int i = 0; i < a.numArguments; i++)
	{
		if (a.arguments[i].isArray())
		{
			Array<var> * aa = a.arguments[i].getArray();
			for (auto &vaa : *aa) data.add((uint8)(int)vaa);
		} else if (a.arguments[i].isInt() || a.arguments[i].isDouble())
		{
			data.add((uint8)(int)a.arguments[i]);
		}
	}

	m->sendBytes(data);
	return var();
}

StreamingModule::StreamingRouteParams::StreamingRouteParams(Module* sourceModule, Controllable* c)
{
	prefix = addStringParameter("Prefix", "Prefix to put before the actual value", c->shortName+" ");
	appendNL = addBoolParameter("NL", "Append NL (New Line) at the end", false);
	appendCR = addBoolParameter("CR", "Append CR (Charriot Return) at the end", false);
}


void StreamingModule::handleRoutedModuleValue(Controllable* c, RouteParams* p)
{
	StreamingRouteParams* op = dynamic_cast<StreamingRouteParams*>(p);
	
	String s = op->prefix->stringValue();
	if (c->type != Controllable::TRIGGER)
	{
		s += dynamic_cast<Parameter*>(c)->stringValue();
	}

	s += op->appendCR->boolValue() ? "\r" : "";
	s += op->appendNL->boolValue() ? "\n" : "";

	sendMessage(s);
}
