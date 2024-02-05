/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MDAOverdriveAudioProcessor::MDAOverdriveAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : foleys::MagicProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    FOLEYS_SET_SOURCE_PATH (__FILE__);
    
    magicState.setGuiValueTree (BinaryData::customMagic_xml, BinaryData::customMagic_xmlSize);
    /*
    //Load xml file for gui
    auto file = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
        .getChildFile ("Contents")
        .getChildFile ("Resources")
        .getChildFile ("customMagic.xml");


    if (file.existsAsFile())
    {
        std::cout << "File exists" << std::endl;
        magicState.setGuiValueTree (file);
    }
        
    
    else
    {
        std::cout << "file DON'T exists" << std::endl;
        magicState.setGuiValueTree (BinaryData::customMagic_xml, BinaryData::customMagic_xmlSize);
        
    }
     */
    
    //magicState.setGuiValueTree(BinaryData::customMagic_xml, BinaryData::customMagic_xmlSize);

    /*
    auto onOffButton = magicState.getObjectWithType<juce::ToggleButton>("onOffButton");
    
    onOffButton->onClick = []()
    {
        std::cout << "wewe" << std::endl;
    };
     
    magicState.addTrigger ("open", [&]
    {
        std::cout << "Trigger" << std::endl;
        
        auto onOffButton = magicState.getObjectWithType<juce::TextButton>("myTextButton");
       
        if(onOffButton!=nullptr) {
            onOffButton->setClickingTogglesState(true);
        }
        else{
            std::cout << "_nullptr" << std::endl;
        }
        
    });
     */
    
    
}

MDAOverdriveAudioProcessor::~MDAOverdriveAudioProcessor()
{
 
}

//==============================================================================
const juce::String MDAOverdriveAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MDAOverdriveAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MDAOverdriveAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MDAOverdriveAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MDAOverdriveAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MDAOverdriveAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MDAOverdriveAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MDAOverdriveAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MDAOverdriveAudioProcessor::getProgramName (int index)
{
    return {};
}

void MDAOverdriveAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MDAOverdriveAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{

    outputLevelSmoother.reset(sampleRate, 0.05);
    outputLevelSmoother.setCurrentAndTargetValue(0);
}

void MDAOverdriveAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MDAOverdriveAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void MDAOverdriveAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    update();
    

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        const float* inputData = buffer.getReadPointer(channel);
        float* outputData = buffer.getWritePointer(channel);
        
        //auto* channelData = buffer.getWritePointer (channel);
        
        for (int i=0; i < buffer.getNumSamples(); ++i)
        {
            float inputSample = inputData[i];
            
            // Distortion using Wave shaping
            float waveShaping = (inputSample > 0.0f) ? std::sqrt(inputSample) : -std::sqrt(-inputSample);
            
            // Linear interpolation between input and distorted sample using One pole filtering.
            float distortedSample = (1 - _drive) * inputSample + _drive * waveShaping;
            
            // Filtering
            float filteredSample = _filt * distortedSample + (1 - _filt) * prevFiltered[channel];
            
            // Apply Gain
            _gain = outputLevelSmoother.getNextValue();
            float y = filteredSample * _gain;
            
            // Write buffer
            outputData[i] = y;
            
            //catch denormals and set prevFiltered value
            if (std::abs(y) > 1.0e-10f) prevFiltered[channel] = filteredSample; else prevFiltered[channel] = 0.0f;

        }

    }
}

//==============================================================================
/*
bool MDAOverdriveAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MDAOverdriveAudioProcessor::createEditor()
{
    //Automatically generated Interface
    
    auto editor = new juce::GenericAudioProcessorEditor (*this);
    editor->setSize(500, 200);
    return editor;
     
    
    //return new MDAOverdriveAudioProcessorEditor (*this);
}


//==============================================================================
void MDAOverdriveAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    //foleys::MagicProcessor::getStateInformation(<#destData#>);
}

void MDAOverdriveAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

}
 */
//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MDAOverdriveAudioProcessor();
}


juce::AudioProcessorValueTreeState::ParameterLayout MDAOverdriveAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    auto driveParam = std::make_unique<juce::AudioParameterFloat>(
                                                             juce::ParameterID("Drive",1),
                                                             "Drive",
                                                             juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
                                                             0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel("%")
                                                             );
    
    auto muffleParam = std::make_unique<juce::AudioParameterFloat>(
                                                                   juce::ParameterID("Muffle",1),
                                                                   "Muffle",
                                                                   juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f),
                                                                   0.0f,
                                                                   juce::AudioParameterFloatAttributes().withLabel("%")
                                                                   );
    
    auto outputParam = std::make_unique<juce::AudioParameterFloat>(
                                                                   juce::ParameterID("Output",1),
                                                                   "Output",
                                                                   juce::NormalisableRange<float>(-70.0f, 20.0f, 0.01f, 2.75f),
                                                                   0.0f,
                                                                   juce::AudioParameterFloatAttributes().withLabel("dB")
                                                                   );
    
    
    
    
    auto group = std::make_unique<juce::AudioProcessorParameterGroup>("Controls", "CONTROLS", "|",
                                                                      std::move (driveParam),
                                                                      std::move (muffleParam),
                                                                      std::move (outputParam));
    layout.add( std::move(group) );
    
    return layout;
}

void MDAOverdriveAudioProcessor::update()
{
    _drive = apvts.getRawParameterValue("Drive")->load() / 100.0f;
    
    float muffle = apvts.getRawParameterValue("Muffle")->load() / 100.0f;
    _filt = std::pow(10, -1.6f * muffle);
    
    float gainDB = apvts.getRawParameterValue("Output")->load();
    //_gain = juce::Decibels::decibelsToGain(gainDB);
    
    outputLevelSmoother.setTargetValue(juce::Decibels::decibelsToGain(gainDB));
    

     
}
