/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for an ARA document controller implementation.

  ==============================================================================
*/

#include "PluginARADocumentController.h"
#include "PluginARAPlaybackRenderer.h"

//==============================================================================
juce::ARAPlaybackRenderer* DissonanceMeeterDocumentController::doCreatePlaybackRenderer() noexcept
{
    return new DissonanceMeeterPlaybackRenderer (getDocumentController());
}

//==============================================================================
bool DissonanceMeeterDocumentController::doRestoreObjectsFromStream (juce::ARAInputStream&, const juce::ARARestoreObjectsFilter*) noexcept
{
    return true;
}

bool DissonanceMeeterDocumentController::doStoreObjectsToStream (juce::ARAOutputStream&, const juce::ARAStoreObjectsFilter*) noexcept
{
    return true;
}

//==============================================================================
// This creates the static ARAFactory instances for the plugin.
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<DissonanceMeeterDocumentController>();
}
