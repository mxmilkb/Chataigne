#include "Main.h"

#if JUCE_MAC //for chmod
#include <sys/types.h>
#include <sys/stat.h>
#endif

//==============================================================================

ChataigneApplication::ChataigneApplication() :
	OrganicApplication("Chataigne")
{
	enableSendAnalytics = appSettings.addBoolParameter("Send Analytics", "This helps me improve the software by sending basic start/stop/crash infos", true);
}


void ChataigneApplication::initialiseInternal(const String &)
{
	engine.reset(new ChataigneEngine());
	mainComponent.reset(new MainContentComponent());

	//Call after engine init
	AppUpdater::getInstance()->setURLs(URL("http://benjamin.kuperberg.fr/chataigne/releases/update.json"), "http://benjamin.kuperberg.fr/chataigne/user/data/", "Chataigne");
	HelpBox::getInstance()->helpURL = URL("http://benjamin.kuperberg.fr/chataigne/docs/");
	CrashDumpUploader::getInstance()->remoteURL = URL("http://benjamin.kuperberg.fr/chataigne/support/crash_report.php");

	ShapeShifterManager::getInstance()->setDefaultFileData(BinaryData::default_chalayout);
	ShapeShifterManager::getInstance()->setLayoutInformations("chalayout", "Chataigne/layouts");

}


void ChataigneApplication::afterInit()
{
	//ANALYTICS
	if (enableSendAnalytics->boolValue())
	{
		bool crashFound = CrashDumpUploader::getInstance()->crashFound;
		
		StringPairArray options;
		
		if (crashFound) MatomoAnalytics::getInstance()->log(MatomoAnalytics::CRASH); 
		else options.set("new_visit", "1");

		MatomoAnalytics::getInstance()->log(MatomoAnalytics::START, options);
	}

}

void ChataigneApplication::shutdown()
{   
	OrganicApplication::shutdown();

	if (enableSendAnalytics->boolValue())
	{
		MatomoAnalytics::getInstance()->log(MatomoAnalytics::STOP);
	}

	if(MatomoAnalytics::getInstanceWithoutCreating() != nullptr) MatomoAnalytics::deleteInstance();
	AppUpdater::deleteInstance();
}
