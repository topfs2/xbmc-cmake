/*
 *      Copyright (C) 2010-2014 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "Application.h"
#include "settings/MediaSettings.h"

#include "DllAvCodec.h"
#include "cores/AudioEngine/Engines/ActiveAE/ActiveAEBuffer.h"
#include "cores/AudioEngine/Engines/ActiveAE/ActiveAEResample.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#include "cores/IPlayer.h"
#include "utils/TimeUtils.h"

#include "ActiveAEDSPProcess.h"
#include "ActiveAEDSPMode.h"

using namespace std;
using namespace ADDON;
using namespace ActiveAE;

#define MIN_DSP_ARRAY_SIZE 4096

CActiveAEDSPProcess::CActiveAEDSPProcess(AE_DSP_STREAM_ID streamId)
 : m_StreamId(streamId)
{
  m_ChannelLayoutIn         = 0;      /* Undefined input channel layout */
  m_ChannelLayoutOut        = 0;      /* Undefined output channel layout */
  m_StreamType              = AE_DSP_ASTREAM_INVALID;
  m_NewStreamType           = AE_DSP_ASTREAM_INVALID;
  m_NewMasterMode           = AE_DSP_MASTER_MODE_ID_INVALID;
  m_ForceInit               = false;

  m_Addon_InputResample.Clear();
  m_Addon_OutputResample.Clear();

  /*!
   * Create predefined process arrays on every supported channel for audio dsp's.
   * All are set if used or not for safety reason and unsued ones can be used from
   * dsp addons as buffer arrays.
   * If a bigger size is neeeded it becomes reallocated during DSP processing.
   */
  m_ProcessArraySize          = MIN_DSP_ARRAY_SIZE;
  for (int i = 0; i < AE_DSP_CH_MAX; i++)
  {
    m_ProcessArray[0][i]      = (float*)calloc(m_ProcessArraySize, sizeof(float));
    m_ProcessArray[1][i]      = (float*)calloc(m_ProcessArraySize, sizeof(float));
  }
}

CActiveAEDSPProcess::~CActiveAEDSPProcess()
{
  ResetStreamFunctionsSelection();

  /* Clear the buffer arrays */
  for (int i = 0; i < AE_DSP_CH_MAX; i++)
  {
    if(m_ProcessArray[0][i])
      free(m_ProcessArray[0][i]);
    if(m_ProcessArray[1][i])
      free(m_ProcessArray[1][i]);
  }
}

void CActiveAEDSPProcess::ResetStreamFunctionsSelection()
{
  m_NewMasterMode         = AE_DSP_MASTER_MODE_ID_INVALID;
  m_NewStreamType         = AE_DSP_ASTREAM_INVALID;
  m_Addon_InputResample.Clear();
  m_Addon_OutputResample.Clear();

  m_Addons_InputProc.clear();
  m_Addons_PreProc.clear();
  m_Addons_MasterProc.clear();
  m_Addons_PostProc.clear();
  m_usedMap.clear();
}

bool CActiveAEDSPProcess::Create(AEAudioFormat inputFormat, AEAudioFormat outputFormat, bool upmix, AEQuality quality, AE_DSP_STREAMTYPE iStreamType)
{
  m_InputFormat       = inputFormat;
  m_OutputFormat      = outputFormat;
  m_OutputSamplerate  = m_InputFormat.m_sampleRate;         /*!< If no resampler addon is present output samplerate is the same as input */
  m_ResampleQuality   = quality;
  m_dataFormat        = AE_FMT_FLOAT;
  m_ActiveMode        = AE_DSP_MASTER_MODE_ID_PASSOVER;     /*!< Reset the pointer for m_MasterModes about active master process, set here during mode selection */

  CSingleLock lock(m_restartSection);

  CLog::Log(LOGDEBUG, "ActiveAE DSP - %s - Audio DSP processing id %d created:", __FUNCTION__, m_StreamId);

  ResetStreamFunctionsSelection();

  CFileItem currentFile(g_application.CurrentFileItem());

  m_StreamTypeDetected = DetectStreamType(&currentFile);
  m_StreamTypeAsked    = iStreamType;

  if (iStreamType == AE_DSP_ASTREAM_AUTO)
    m_StreamType = m_StreamTypeDetected;
  else if (iStreamType >= AE_DSP_ASTREAM_BASIC || iStreamType < AE_DSP_ASTREAM_AUTO)
    m_StreamType = iStreamType;
  else
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - %s - Unknown audio stream type, falling back to basic", __FUNCTION__);
    m_StreamType = AE_DSP_ASTREAM_BASIC;
  }

  /*!
   * Set general stream infos about the here processed stream
   */

  if (g_application.m_pPlayer->GetAudioStreamCount() > 0)
  {
    int identifier = CMediaSettings::Get().GetCurrentVideoSettings().m_AudioStream;
    if(identifier < 0)
      identifier = g_application.m_pPlayer->GetAudioStream();
    if (identifier < 0)
      identifier = 0;

    SPlayerAudioStreamInfo info;
    g_application.m_pPlayer->GetAudioStreamInfo(identifier, info);

    m_AddonStreamProperties.strName       = info.name.c_str();
    m_AddonStreamProperties.strLanguage   = info.language.c_str();
    m_AddonStreamProperties.strCodecId    = info.audioCodecName.c_str();
    m_AddonStreamProperties.iIdentifier   = identifier;
  }
  else
  {
    m_AddonStreamProperties.strName       = "Unknown";
    m_AddonStreamProperties.strLanguage   = "";
    m_AddonStreamProperties.strCodecId    = "";
    m_AddonStreamProperties.iIdentifier   = m_StreamId;
  }

  m_AddonStreamProperties.iStreamID       = m_StreamId;
  m_AddonStreamProperties.iStreamType     = m_StreamType;
  m_AddonStreamProperties.iChannels       = m_InputFormat.m_channelLayout.Count();
  m_AddonStreamProperties.iSampleRate     = m_InputFormat.m_sampleRate;
  m_AddonStreamProperties.iBaseType       = GetBaseType(&m_AddonStreamProperties);
  CreateStreamProfile();

  /*!
   * Set exact input and output format settings
   */
  m_AddonSettings.iStreamID               = m_StreamId;
  m_AddonSettings.iStreamType             = m_StreamType;
  m_AddonSettings.lInChannelPresentFlags  = 0;
  m_AddonSettings.iInChannels             = m_InputFormat.m_channelLayout.Count();
  m_AddonSettings.iInFrames               = m_InputFormat.m_frames;
  m_AddonSettings.iInSamplerate           = m_InputFormat.m_sampleRate;           /* The basic input samplerate from stream source */
  m_AddonSettings.iProcessFrames          = m_InputFormat.m_frames;
  m_AddonSettings.iProcessSamplerate      = m_InputFormat.m_sampleRate;           /* Default the same as input samplerate,  if resampler is present it becomes corrected */
  m_AddonSettings.lOutChannelPresentFlags = 0;
  m_AddonSettings.iOutChannels            = m_OutputFormat.m_channelLayout.Count();
  m_AddonSettings.iOutFrames              = m_OutputFormat.m_frames;
  m_AddonSettings.iOutSamplerate          = m_OutputFormat.m_sampleRate;          /* The required sample rate for pass over resampling on ActiveAEResample */
  m_AddonSettings.bStereoUpmix            = upmix;
  m_AddonSettings.bInputResamplingActive  = false;
  m_AddonSettings.iQualityLevel           = quality;

  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_FL))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FL;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_FR))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FR;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_FC))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FC;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_LFE))  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_LFE;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_BL))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BL;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_BR))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BR;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_FLOC)) m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FLOC;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_FROC)) m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FROC;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_BC))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BC;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_SL))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_SL;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_SR))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_SR;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_TFL))  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TFL;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_TFR))  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TFR;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_TFC))  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TFC;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_TC))   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TC;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_TBL))  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TBL;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_TBR))  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TBR;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_TBC))  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TBC;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_BLOC)) m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BLOC;
  if (m_InputFormat.m_channelLayout.HasChannel(AE_CH_BROC)) m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BROC;

  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_FL))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FL;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_FR))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FR;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_FC))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FC;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_LFE))  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_LFE;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_BL))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BL;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_BR))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BR;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_FLOC)) m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FLOC;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_FROC)) m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FROC;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_BC))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BC;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_SL))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_SL;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_SR))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_SR;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_TFL))  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TFL;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_TFR))  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TFR;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_TFC))  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TFC;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_TC))   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TC;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_TBL))  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TBL;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_TBR))  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TBR;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_TBC))  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TBC;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_BLOC)) m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BLOC;
  if (m_OutputFormat.m_channelLayout.HasChannel(AE_CH_BROC)) m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BROC;

  /*!
   * Setup off mode, used if dsp master processing is set off, required to have data
   * for stream information functions.
   */
  sDSPProcessHandle passoverMode;
  passoverMode.Clear();
  passoverMode.iAddonModeNumber = AE_DSP_MASTER_MODE_ID_PASSOVER;
  passoverMode.pMode            = CActiveAEDSPModePtr(new CActiveAEDSPMode((AE_DSP_BASETYPE)m_AddonStreamProperties.iBaseType));
  m_Addons_MasterProc.push_back(passoverMode);
  m_ActiveMode = AE_DSP_MASTER_MODE_ID_PASSOVER;

  /*!
   * Load all selected processing types, stored in a database and available from addons
   */
  AE_DSP_ADDONMAP addonMap;
  if (CActiveAEDSP::Get().GetEnabledAudioDSPAddons(addonMap) > 0)
  {
    int foundInputResamplerId = -1; /* Used to prevent double call of StreamCreate if input stream resampling is together with outer processing types */

    /* First find input resample addon to become information about processing sample rate and
     * load one allowed before master processing & final resample addon
     */
    CLog::Log(LOGDEBUG, "  ---- DSP input resample addon ---");
    const AE_DSP_MODELIST listInputResample = CActiveAEDSP::Get().GetAvailableModes(AE_DSP_MODE_TYPE_INPUT_RESAMPLE);
    if (listInputResample.size() == 0)
      CLog::Log(LOGDEBUG, "  | - no input resample addon present or enabled");
    for (unsigned int i = 0; i < listInputResample.size(); i++)
    {
      /// For resample only one call is allowed. Use first one and ignore everything else.
      CActiveAEDSPModePtr pMode = listInputResample[i].first;
      AE_DSP_ADDON        addon = listInputResample[i].second;
      if (addon->Enabled() && addon->SupportsInputResample() && pMode->IsEnabled())
      {
        AE_DSP_ERROR err = addon->StreamCreate(&m_AddonSettings, &m_AddonStreamProperties);
        if (err == AE_DSP_ERROR_NO_ERROR)
        {
          if (addon->StreamIsModeSupported(m_StreamId, pMode->ModeType(), pMode->AddonModeNumber(), pMode->ModeID()))
          {
            int processSamplerate = addon->InputResampleSampleRate(m_StreamId);
            if (processSamplerate == (int)m_InputFormat.m_sampleRate)
            {
              CLog::Log(LOGDEBUG, "  | - input resample addon %s ignored, input sample rate %i the same as process rate", addon->GetFriendlyName().c_str(), m_InputFormat.m_sampleRate);
            }
            else if (processSamplerate > 0)
            {
              CLog::Log(LOGDEBUG, "  | - %s with resampling from %i to %i", addon->GetAudioDSPName().c_str(), m_InputFormat.m_sampleRate, processSamplerate);

              m_OutputSamplerate                      = processSamplerate;                  /*!< overwrite output sample rate with the new rate */
              m_AddonSettings.iProcessSamplerate      = processSamplerate;                  /*!< the processing sample rate required for all behind called processes */
              m_AddonSettings.iProcessFrames          = (int) ceil((1.0 * m_AddonSettings.iProcessSamplerate) / m_AddonSettings.iInSamplerate * m_AddonSettings.iInFrames);
              m_AddonSettings.bInputResamplingActive  = true;

              m_Addon_InputResample.iAddonModeNumber  = pMode->AddonModeNumber();
              m_Addon_InputResample.pMode             = pMode;
              m_Addon_InputResample.pFunctions        = addon->GetAudioDSPFunctionStruct();
              m_Addon_InputResample.pAddon            = addon;
            }
            else
            {
              CLog::Log(LOGERROR, "ActiveAE DSP - %s - input resample addon %s return invalid samplerate and becomes disabled", __FUNCTION__, addon->GetFriendlyName().c_str());
            }

            foundInputResamplerId = addon->GetID();
            m_usedMap.insert(std::make_pair(addon->GetID(), addon));
          }
        }
        else if (err != AE_DSP_ERROR_IGNORE_ME)
          CLog::Log(LOGERROR, "ActiveAE DSP - %s - input resample addon creation failed on %s with %s", __FUNCTION__, addon->GetAudioDSPName().c_str(), CActiveAEDSPAddon::ToString(err));
        break;
      }
    }

    /* Now init all other dsp relavant addons
     */
    for (AE_DSP_ADDONMAP_ITR itr = addonMap.begin(); itr != addonMap.end(); itr++)
    {
      AE_DSP_ADDON addon = itr->second;
      if (addon->Enabled() && addon->GetID() != foundInputResamplerId)
      {
        AE_DSP_ERROR err = addon->StreamCreate(&m_AddonSettings, &m_AddonStreamProperties);
        if (err == AE_DSP_ERROR_NO_ERROR)
        {
          m_usedMap.insert(std::make_pair(addon->GetID(), addon));
        }
        else if (err == AE_DSP_ERROR_IGNORE_ME)
          continue;
        else
          CLog::Log(LOGERROR, "ActiveAE DSP - %s - addon creation failed on %s with %s", __FUNCTION__, addon->GetAudioDSPName().c_str(), CActiveAEDSPAddon::ToString(err));
      }
    }

    for (AE_DSP_ADDONMAP_ITR itr = m_usedMap.begin(); itr != m_usedMap.end(); itr++)
    {
      AE_DSP_ADDON addon = itr->second;
      if (addon->SupportsInputInfoProcess())
        m_Addons_InputProc.push_back(addon->GetAudioDSPFunctionStruct());
    }

    /*!
     * Load all required pre process dsp addon functions
     */
    CLog::Log(LOGDEBUG, "  ---- DSP active pre process modes ---");
    const AE_DSP_MODELIST listPreProcess = CActiveAEDSP::Get().GetAvailableModes(AE_DSP_MODE_TYPE_PRE_PROCESS);
    for (unsigned int i = 0; i < listPreProcess.size(); i++)
    {
      CActiveAEDSPModePtr pMode = listPreProcess[i].first;
      AE_DSP_ADDON        addon = listPreProcess[i].second;

      if (m_usedMap.find(addon->GetID()) == m_usedMap.end())
        continue;
      if (addon->Enabled() && addon->SupportsPreProcess() && pMode->IsEnabled() &&
          addon->StreamIsModeSupported(m_StreamId, pMode->ModeType(), pMode->AddonModeNumber(), pMode->ModeID()))
      {
        CLog::Log(LOGDEBUG, "  | - %i - %s (%s)", i, pMode->AddonModeName().c_str(), addon->GetAudioDSPName().c_str());

        sDSPProcessHandle modeHandle;
        modeHandle.iAddonModeNumber = pMode->AddonModeNumber();
        modeHandle.pMode            = pMode;
        modeHandle.pFunctions       = addon->GetAudioDSPFunctionStruct();
        modeHandle.pAddon           = addon;
        m_Addons_PreProc.push_back(modeHandle);
      }
    }
    if (m_Addons_PreProc.empty())
      CLog::Log(LOGDEBUG, "  | - no pre processing addon's present or enabled");

    /*!
     * Load all available master modes from addons and put together with database
     */
    CLog::Log(LOGDEBUG, "  ---- DSP active master process modes ---");
    const AE_DSP_MODELIST listMasterProcess = CActiveAEDSP::Get().GetAvailableModes(AE_DSP_MODE_TYPE_MASTER_PROCESS);
    for (unsigned int i = 0; i < listMasterProcess.size(); i++)
    {
      CActiveAEDSPModePtr pMode = listMasterProcess[i].first;
      AE_DSP_ADDON        addon = listMasterProcess[i].second;

      if (m_usedMap.find(addon->GetID()) == m_usedMap.end())
        continue;
      if (addon->Enabled() && addon->SupportsMasterProcess() && pMode->IsEnabled() &&
          addon->StreamIsModeSupported(m_StreamId, pMode->ModeType(), pMode->AddonModeNumber(), pMode->ModeID()))
      {
        CLog::Log(LOGDEBUG, "  | - %i - %s (%s)", i, pMode->AddonModeName().c_str(), addon->GetAudioDSPName().c_str());

        sDSPProcessHandle modeHandle;
        modeHandle.iAddonModeNumber = pMode->AddonModeNumber();
        modeHandle.pMode            = pMode;
        modeHandle.pFunctions       = addon->GetAudioDSPFunctionStruct();
        modeHandle.pAddon           = addon;
        modeHandle.pMode->SetBaseType((AE_DSP_BASETYPE)m_AddonStreamProperties.iBaseType);
        m_Addons_MasterProc.push_back(modeHandle);
      }
    }
    if (m_Addons_MasterProc.empty())
      CLog::Log(LOGDEBUG, "  | - no master processing addon's present or enabled");

    /* Get selected source for current input */
    int ModeID = CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[m_AddonStreamProperties.iStreamType][m_AddonStreamProperties.iBaseType];
    if (ModeID == AE_DSP_MASTER_MODE_ID_INVALID)
      ModeID = AE_DSP_MASTER_MODE_ID_PASSOVER;

    for (unsigned int ptr = 0; ptr < m_Addons_MasterProc.size(); ptr++)
    {
      CActiveAEDSPModePtr mode = m_Addons_MasterProc.at(ptr).pMode;
      if (mode->ModeID() != AE_DSP_MASTER_MODE_ID_PASSOVER && mode->ModeID() == ModeID)
      {
        m_ActiveMode = (int)ptr;
        CLog::Log(LOGDEBUG, "  | -- %s (selected)", mode->AddonModeName().c_str());
        break;
      }
    }

    /*!
     * Setup the one allowed master processing addon and inform about selected mode
     */
    if (m_Addons_MasterProc[m_ActiveMode].pFunctions)
    {
      try
      {
        AE_DSP_ERROR err = m_Addons_MasterProc[m_ActiveMode].pFunctions->MasterProcessSetMode(m_StreamId, m_AddonStreamProperties.iStreamType, m_Addons_MasterProc[m_ActiveMode].pMode->AddonModeNumber(), m_Addons_MasterProc[m_ActiveMode].pMode->ModeID());
        if (err != AE_DSP_ERROR_NO_ERROR)
        {
          CLog::Log(LOGERROR, "ActiveAE DSP - %s - addon master mode selection failed on %s with Mode '%s' with %s",
                                  __FUNCTION__,
                                  m_Addons_MasterProc[m_ActiveMode].pAddon->GetAudioDSPName().c_str(),
                                  m_Addons_MasterProc[m_ActiveMode].pMode->AddonModeName().c_str(),
                                  CActiveAEDSPAddon::ToString(err));
          m_Addons_MasterProc.erase(m_Addons_MasterProc.begin()+m_ActiveMode);
          m_ActiveMode = AE_DSP_MASTER_MODE_ID_PASSOVER;
        }
      }
      catch (exception &e)
      {
        CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'MasterProcessSetMode' on add-on '%s'. Please contact the developer of this add-on",
                                e.what(),
                                m_Addons_MasterProc[m_ActiveMode].pAddon->GetAudioDSPName().c_str());
        m_Addons_MasterProc.erase(m_Addons_MasterProc.begin()+m_ActiveMode);
        m_ActiveMode = AE_DSP_MASTER_MODE_ID_PASSOVER;
      }
    }
    else
    {
      CLog::Log(LOGDEBUG, "  | -- No master process selected!");
    }

    /*!
     * Load all required post process dsp addon functions
     */
    CLog::Log(LOGDEBUG, "  ---- DSP active post process modes ---");
    const AE_DSP_MODELIST listPostProcess = CActiveAEDSP::Get().GetAvailableModes(AE_DSP_MODE_TYPE_POST_PROCESS);
    for (unsigned int i = 0; i < listPostProcess.size(); i++)
    {
      CActiveAEDSPModePtr pMode = listPostProcess[i].first;
      AE_DSP_ADDON        addon = listPostProcess[i].second;

      if (m_usedMap.find(addon->GetID()) == m_usedMap.end())
        continue;

      if (addon->Enabled() && addon->SupportsPostProcess() && pMode->IsEnabled() &&
          addon->StreamIsModeSupported(m_StreamId, pMode->ModeType(), pMode->AddonModeNumber(), pMode->ModeID()))
      {
        CLog::Log(LOGDEBUG, "  | - %i - %s (%s)", i, pMode->AddonModeName().c_str(), addon->GetAudioDSPName().c_str());

        sDSPProcessHandle modeHandle;
        modeHandle.iAddonModeNumber = pMode->AddonModeNumber();
        modeHandle.pMode            = pMode;
        modeHandle.pFunctions       = addon->GetAudioDSPFunctionStruct();
        modeHandle.pAddon           = addon;
        m_Addons_PostProc.push_back(modeHandle);
      }
    }
    if (m_Addons_PostProc.empty())
      CLog::Log(LOGDEBUG, "  | - no post processing addon's present or enabled");

    /*!
     * Load one allowed addon for resampling after final post processing
     */
    CLog::Log(LOGDEBUG, "  ---- DSP post resample addon ---");
    if (m_AddonSettings.iProcessSamplerate != m_OutputFormat.m_sampleRate)
    {
      const AE_DSP_MODELIST listOutputResample = CActiveAEDSP::Get().GetAvailableModes(AE_DSP_MODE_TYPE_OUTPUT_RESAMPLE);
      if (listOutputResample.size() == 0)
        CLog::Log(LOGDEBUG, "  | - no final post resample addon present or enabled, becomes performed by XBMC");
      for (unsigned int i = 0; i < listOutputResample.size(); i++)
      {
        /// For resample only one call is allowed. Use first one and ignore everything else.
        CActiveAEDSPModePtr pMode = listOutputResample[i].first;
        AE_DSP_ADDON        addon = listOutputResample[i].second;
        if (m_usedMap.find(addon->GetID()) != m_usedMap.end() &&
            addon->Enabled() && addon->SupportsOutputResample() && pMode->IsEnabled() &&
            addon->StreamIsModeSupported(m_StreamId, pMode->ModeType(), pMode->AddonModeNumber(), pMode->ModeID()))
        {
          int outSamplerate = addon->OutputResampleSampleRate(m_StreamId);
          if (outSamplerate > 0)
          {
            CLog::Log(LOGDEBUG, "  | - %s with resampling to %i", addon->GetAudioDSPName().c_str(), outSamplerate);

            m_OutputSamplerate = outSamplerate;

            m_Addon_OutputResample.iAddonModeNumber = pMode->AddonModeNumber();
            m_Addon_OutputResample.pMode            = pMode;
            m_Addon_OutputResample.pFunctions       = addon->GetAudioDSPFunctionStruct();
            m_Addon_OutputResample.pAddon           = addon;
          }
          else
          {
            CLog::Log(LOGERROR, "ActiveAE DSP - %s - post resample addon %s return invalid samplerate and becomes disabled", __FUNCTION__, addon->GetFriendlyName().c_str());
          }
          break;
        }
      }
    }
    else
    {
      CLog::Log(LOGDEBUG, "  | - no final resampling needed, process and final samplerate the same");
    }
  }

  if (CLog::GetLogLevel() == LOGDEBUG) // Speed improve
  {
    CLog::Log(LOGDEBUG, "  ----  Input stream  ----");
    CLog::Log(LOGDEBUG, "  | Identifier           : %d", m_AddonStreamProperties.iIdentifier);
    CLog::Log(LOGDEBUG, "  | Stream Type          : %s", m_AddonStreamProperties.iStreamType == AE_DSP_ASTREAM_BASIC   ? "Basic"   :
                                                         m_AddonStreamProperties.iStreamType == AE_DSP_ASTREAM_MUSIC   ? "Music"   :
                                                         m_AddonStreamProperties.iStreamType == AE_DSP_ASTREAM_MOVIE   ? "Movie"   :
                                                         m_AddonStreamProperties.iStreamType == AE_DSP_ASTREAM_GAME    ? "Game"    :
                                                         m_AddonStreamProperties.iStreamType == AE_DSP_ASTREAM_APP     ? "App"     :
                                                         m_AddonStreamProperties.iStreamType == AE_DSP_ASTREAM_PHONE   ? "Phone"   :
                                                         m_AddonStreamProperties.iStreamType == AE_DSP_ASTREAM_MESSAGE ? "Message" :
                                                         "Unknown");
    CLog::Log(LOGDEBUG, "  | Name                 : %s", m_AddonStreamProperties.strName);
    CLog::Log(LOGDEBUG, "  | Language             : %s", m_AddonStreamProperties.strLanguage);
    CLog::Log(LOGDEBUG, "  | Codec                : %s", m_AddonStreamProperties.strCodecId);
    CLog::Log(LOGDEBUG, "  | Sample Rate          : %d", m_AddonStreamProperties.iSampleRate);
    CLog::Log(LOGDEBUG, "  | Channels             : %d", m_AddonStreamProperties.iChannels);
    CLog::Log(LOGDEBUG, "  ----  Input format  ----");
    CLog::Log(LOGDEBUG, "  | Sample Rate          : %d", m_InputFormat.m_sampleRate);
    CLog::Log(LOGDEBUG, "  | Sample Format        : %s", CAEUtil::DataFormatToStr(m_InputFormat.m_dataFormat));
    CLog::Log(LOGDEBUG, "  | Channel Count        : %d", m_InputFormat.m_channelLayout.Count());
    CLog::Log(LOGDEBUG, "  | Channel Layout       : %s", ((std::string)m_InputFormat.m_channelLayout).c_str());
    CLog::Log(LOGDEBUG, "  | Frames               : %d", m_InputFormat.m_frames);
    CLog::Log(LOGDEBUG, "  | Frame Samples        : %d", m_InputFormat.m_frameSamples);
    CLog::Log(LOGDEBUG, "  | Frame Size           : %d", m_InputFormat.m_frameSize);
    CLog::Log(LOGDEBUG, "  ----  Process format ----");
    CLog::Log(LOGDEBUG, "  | Sample Rate          : %d", m_AddonSettings.iProcessSamplerate);
    CLog::Log(LOGDEBUG, "  | Frames               : %d", m_AddonSettings.iProcessFrames);
    CLog::Log(LOGDEBUG, "  ----  Output format ----");
    CLog::Log(LOGDEBUG, "  | Sample Rate          : %d", m_AddonSettings.iOutSamplerate);
    CLog::Log(LOGDEBUG, "  | Sample Format        : %s", CAEUtil::DataFormatToStr(m_OutputFormat.m_dataFormat));
    CLog::Log(LOGDEBUG, "  | Channel Count        : %d", m_OutputFormat.m_channelLayout.Count());
    CLog::Log(LOGDEBUG, "  | Channel Layout       : %s", ((std::string)m_OutputFormat.m_channelLayout).c_str());
    CLog::Log(LOGDEBUG, "  | Frames               : %d", m_OutputFormat.m_frames);
    CLog::Log(LOGDEBUG, "  | Frame Samples        : %d", m_OutputFormat.m_frameSamples);
    CLog::Log(LOGDEBUG, "  | Frame Size           : %d", m_OutputFormat.m_frameSize);
  }

  m_ForceInit = true;
  return true;
}

bool CActiveAEDSPProcess::CreateStreamProfile()
{
  return false;
}

void CActiveAEDSPProcess::Destroy()
{
  CSingleLock lock(m_restartSection);

  if (!CActiveAEDSP::Get().IsActivated())
    return;

  for (AE_DSP_ADDONMAP_ITR itr = m_usedMap.begin(); itr != m_usedMap.end(); itr++)
  {
    itr->second->StreamDestroy(m_StreamId);
  }

  ResetStreamFunctionsSelection();
}

void CActiveAEDSPProcess::ForceReinit()
{
  CSingleLock lock(m_restartSection);
  m_ForceInit = true;
}

AE_DSP_STREAMTYPE CActiveAEDSPProcess::DetectStreamType(const CFileItem *item)
{
  AE_DSP_STREAMTYPE detected = AE_DSP_ASTREAM_BASIC;
  if (item->HasMusicInfoTag())
    detected = AE_DSP_ASTREAM_MUSIC;
  else if (item->HasVideoInfoTag() || g_application.m_pPlayer->HasVideo())
    detected = AE_DSP_ASTREAM_MOVIE;
//    else if (item->HasVideoInfoTag())
//      detected = AE_DSP_ASTREAM_GAME;
//    else if (item->HasVideoInfoTag())
//      detected = AE_DSP_ASTREAM_APP;
//    else if (item->HasVideoInfoTag())
//      detected = AE_DSP_ASTREAM_MESSAGE;
//    else if (item->HasVideoInfoTag())
//      detected = AE_DSP_ASTREAM_PHONE;
  else
    detected = AE_DSP_ASTREAM_BASIC;

  return detected;
}

AE_DSP_STREAM_ID CActiveAEDSPProcess::GetStreamId() const
{
  return m_StreamId;
}


unsigned int CActiveAEDSPProcess::GetInputChannels()
{
  return m_InputFormat.m_channelLayout.Count();
}

std::string CActiveAEDSPProcess::GetInputChannelNames()
{
  return m_InputFormat.m_channelLayout;
}

unsigned int CActiveAEDSPProcess::GetInputSamplerate()
{
  return m_InputFormat.m_sampleRate;
}

unsigned int CActiveAEDSPProcess::GetProcessSamplerate()
{
  return m_AddonSettings.iProcessSamplerate;
}

unsigned int CActiveAEDSPProcess::GetOutputChannels()
{
  return m_OutputFormat.m_channelLayout.Count();
}

std::string CActiveAEDSPProcess::GetOutputChannelNames()
{
  return m_OutputFormat.m_channelLayout;
}

unsigned int CActiveAEDSPProcess::GetOutputSamplerate()
{
  return m_OutputSamplerate;
}

float CActiveAEDSPProcess::GetCPUUsage(void) const
{
  return m_fLastProcessUsage;
}

CAEChannelInfo CActiveAEDSPProcess::GetChannelLayout()
{
  return m_OutputFormat.m_channelLayout;
}

AEDataFormat CActiveAEDSPProcess::GetDataFormat()
{
  return m_dataFormat;
}

AEAudioFormat CActiveAEDSPProcess::GetInputFormat()
{
  return m_InputFormat;
}

AE_DSP_STREAMTYPE CActiveAEDSPProcess::GetDetectedStreamType()
{
  return m_StreamTypeDetected;
}

AE_DSP_STREAMTYPE CActiveAEDSPProcess::GetStreamType()
{
  return m_StreamType;
}

AE_DSP_STREAMTYPE CActiveAEDSPProcess::GetUsedAddonStreamType()
{
  return (AE_DSP_STREAMTYPE)m_AddonStreamProperties.iStreamType;
}

AE_DSP_BASETYPE CActiveAEDSPProcess::GetBaseType(AE_DSP_STREAM_PROPERTIES *props)
{
  if (!strcmp(props->strCodecId, "ac3"))
    return AE_DSP_ABASE_AC3;
  else if (!strcmp(props->strCodecId, "eac3"))
    return AE_DSP_ABASE_EAC3;
  else if (!strcmp(props->strCodecId, "dca") || !strcmp(props->strCodecId, "dts"))
    return AE_DSP_ABASE_DTS;
  else if (!strcmp(props->strCodecId, "dtshd_hra"))
    return AE_DSP_ABASE_DTSHD_HRA;
  else if (!strcmp(props->strCodecId, "dtshd_ma"))
    return AE_DSP_ABASE_DTSHD_MA;
  else if (!strcmp(props->strCodecId, "truehd"))
    return AE_DSP_ABASE_TRUEHD;
  else if (!strcmp(props->strCodecId, "mlp"))
    return AE_DSP_ABASE_MLP;
  else if (!strcmp(props->strCodecId, "flac"))
    return AE_DSP_ABASE_FLAC;
  else if (props->iChannels > 2)
    return AE_DSP_ABASE_MULTICHANNEL;
  else if (props->iChannels == 2)
    return AE_DSP_ABASE_STEREO;
  else
    return AE_DSP_ABASE_MONO;
}

AE_DSP_BASETYPE CActiveAEDSPProcess::GetUsedAddonBaseType()
{
  return GetBaseType(&m_AddonStreamProperties);
}

bool CActiveAEDSPProcess::GetMasterModeStreamInfoString(CStdString &strInfo)
{
  if (m_ActiveMode == AE_DSP_MASTER_MODE_ID_PASSOVER)
  {
    strInfo = "";
    return true;
  }

  if (m_ActiveMode < 0 || !m_Addons_MasterProc[m_ActiveMode].pFunctions)
    return false;

  strInfo = m_Addons_MasterProc[m_ActiveMode].pAddon->GetMasterModeStreamInfoString(m_StreamId);

  return true;
}

bool CActiveAEDSPProcess::GetMasterModeTypeInformation(AE_DSP_STREAMTYPE &streamTypeUsed, AE_DSP_BASETYPE &baseType, int &iModeID)
{
  streamTypeUsed  = (AE_DSP_STREAMTYPE)m_AddonStreamProperties.iStreamType;

  if (m_ActiveMode < 0)
    return false;

  baseType        = m_Addons_MasterProc[m_ActiveMode].pMode->BaseType();
  iModeID         = m_Addons_MasterProc[m_ActiveMode].pMode->ModeID();
  return true;
}

const char *CActiveAEDSPProcess::GetStreamTypeName(AE_DSP_STREAMTYPE iStreamType)
{
  return iStreamType == AE_DSP_ASTREAM_BASIC   ? "Basic"     :
         iStreamType == AE_DSP_ASTREAM_MUSIC   ? "Music"     :
         iStreamType == AE_DSP_ASTREAM_MOVIE   ? "Movie"     :
         iStreamType == AE_DSP_ASTREAM_GAME    ? "Game"      :
         iStreamType == AE_DSP_ASTREAM_APP     ? "App"       :
         iStreamType == AE_DSP_ASTREAM_PHONE   ? "Phone"     :
         iStreamType == AE_DSP_ASTREAM_MESSAGE ? "Message"   :
         iStreamType == AE_DSP_ASTREAM_AUTO    ? "Automatic" :
         "Unknown";
}

bool CActiveAEDSPProcess::MasterModeChange(int iModeID, AE_DSP_STREAMTYPE iStreamType)
{
  bool bReturn = false;
  bool bSwitchStreamType = iStreamType != AE_DSP_ASTREAM_INVALID;

  /* The Mode is already used and need not to set up again */
  if (m_ActiveMode >= AE_DSP_MASTER_MODE_ID_PASSOVER && m_Addons_MasterProc[m_ActiveMode].pMode->ModeID() == iModeID && !bSwitchStreamType)
    return true;

  CSingleLock lock(m_restartSection);

  CLog::Log(LOGDEBUG, "ActiveAE DSP - %s - Audio DSP processing id %d mode change:", __FUNCTION__, m_StreamId);
  if (bSwitchStreamType && m_StreamType != iStreamType)
  {
    AE_DSP_STREAMTYPE old = m_StreamType;
    CLog::Log(LOGDEBUG, "  ----  Input stream  ----");
    if (iStreamType == AE_DSP_ASTREAM_AUTO)
      m_StreamType = m_StreamTypeDetected;
    else if (iStreamType >= AE_DSP_ASTREAM_BASIC || iStreamType < AE_DSP_ASTREAM_AUTO)
      m_StreamType = iStreamType;
    else
    {
      CLog::Log(LOGWARNING, "ActiveAE DSP - %s - Unknown audio stream type, falling back to basic", __FUNCTION__);
      m_StreamType = AE_DSP_ASTREAM_BASIC;
    }

    CLog::Log(LOGDEBUG, "  | Stream Type change   : From '%s' to '%s'", GetStreamTypeName(old), GetStreamTypeName(m_StreamType));
  }

  /*!
   * Set the new stream type to the addon settings and properties structures.
   * If the addon want to use another stream type, it can be becomes written inside
   * the m_AddonStreamProperties.iStreamType.
   */
  m_AddonStreamProperties.iStreamType = m_StreamType;
  m_AddonSettings.iStreamType         = m_StreamType;

  if (iModeID == AE_DSP_MASTER_MODE_ID_PASSOVER)
  {
    CLog::Log(LOGINFO, "ActiveAE DSP - Switching master mode off");
    m_ActiveMode        = AE_DSP_MASTER_MODE_ID_PASSOVER;
    bReturn             = true;
  }
  else
  {
    CActiveAEDSPModePtr mode;
    for (unsigned int ptr = 0; ptr < m_Addons_MasterProc.size(); ptr++)
    {
      mode = m_Addons_MasterProc.at(ptr).pMode;
      if (mode->ModeID() == iModeID && mode->IsEnabled() && m_Addons_MasterProc[ptr].pFunctions)
      {
        try
        {
          AE_DSP_ERROR err = m_Addons_MasterProc[ptr].pFunctions->MasterProcessSetMode(m_StreamId, m_AddonStreamProperties.iStreamType, mode->AddonModeNumber(), mode->ModeID());
          if (err != AE_DSP_ERROR_NO_ERROR)
          {
            CLog::Log(LOGERROR, "ActiveAE DSP - %s - addon master mode selection failed on %s with Mode '%s' with %s",
                                    __FUNCTION__,
                                    m_Addons_MasterProc[ptr].pAddon->GetAudioDSPName().c_str(),
                                    mode->AddonModeName().c_str(),
                                    CActiveAEDSPAddon::ToString(err));
          }
          else
          {
            CLog::Log(LOGINFO, "ActiveAE DSP - Switching master mode to '%s' as '%s' on '%s'",
                                    mode->AddonModeName().c_str(),
                                    GetStreamTypeName((AE_DSP_STREAMTYPE)m_AddonStreamProperties.iStreamType),
                                    m_Addons_MasterProc[ptr].pAddon->GetAudioDSPName().c_str());
            if (m_AddonStreamProperties.iStreamType != m_StreamType)
            {
              CLog::Log(LOGDEBUG, "ActiveAE DSP - Addon force stream type from '%s' to '%s'",
                                    GetStreamTypeName(m_StreamType),
                                    GetStreamTypeName((AE_DSP_STREAMTYPE)m_AddonStreamProperties.iStreamType));
            }
            m_ActiveMode  = (int)ptr;
            bReturn        = true;
          }
        }
        catch (exception &e)
        {
          CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'MasterModeChange' on add-on '%s'. Please contact the developer of this add-on",
                                  e.what(),
                                  m_Addons_MasterProc[ptr].pAddon->GetAudioDSPName().c_str());
        }

        return bReturn;
      }
    }
  }
  return bReturn;
}

void CActiveAEDSPProcess::ClearArray(float **array, unsigned int samples)
{
  unsigned int presentFlag = 1;
  for (int i = 0; i < AE_DSP_CH_MAX; i++)
  {
    if (m_AddonSettings.lOutChannelPresentFlags & presentFlag)
      memset(array[i], 0, samples*sizeof(float));
    presentFlag <<= 1;
  }
}

bool CActiveAEDSPProcess::Process(CSampleBuffer *in, CSampleBuffer *out, CActiveAEResample *resampler)
{
  CSingleLock lock(m_restartSection);

  bool needDSPAddonsReinit  = m_ForceInit;
  CThread *processThread    = CThread::GetCurrentThread();
  unsigned int iTime        = XbmcThreads::SystemClockMillis() * 10000;
  int64_t hostFrequency     = CurrentHostFrequency();

  /* Detect interleaved input stream channel positions if unknown or changed */
  if (m_ChannelLayoutIn != in->pkt->config.channel_layout)
  {
    m_ChannelLayoutIn = in->pkt->config.channel_layout;

    m_idx_in[AE_CH_FL]    = resampler->GetAVChannelIndex(AE_CH_FL,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_FR]    = resampler->GetAVChannelIndex(AE_CH_FR,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_FC]    = resampler->GetAVChannelIndex(AE_CH_FC,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_LFE]   = resampler->GetAVChannelIndex(AE_CH_LFE,  m_ChannelLayoutIn);
    m_idx_in[AE_CH_BL]    = resampler->GetAVChannelIndex(AE_CH_BL,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_BR]    = resampler->GetAVChannelIndex(AE_CH_BR,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_FLOC]  = resampler->GetAVChannelIndex(AE_CH_FLOC, m_ChannelLayoutIn);
    m_idx_in[AE_CH_FROC]  = resampler->GetAVChannelIndex(AE_CH_FROC, m_ChannelLayoutIn);
    m_idx_in[AE_CH_BC]    = resampler->GetAVChannelIndex(AE_CH_BC,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_SL]    = resampler->GetAVChannelIndex(AE_CH_SL,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_SR]    = resampler->GetAVChannelIndex(AE_CH_SR,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_TC]    = resampler->GetAVChannelIndex(AE_CH_TC,   m_ChannelLayoutIn);
    m_idx_in[AE_CH_TFL]   = resampler->GetAVChannelIndex(AE_CH_TFL,  m_ChannelLayoutIn);
    m_idx_in[AE_CH_TFC]   = resampler->GetAVChannelIndex(AE_CH_TFC,  m_ChannelLayoutIn);
    m_idx_in[AE_CH_TFR]   = resampler->GetAVChannelIndex(AE_CH_TFR,  m_ChannelLayoutIn);
    m_idx_in[AE_CH_TBL]   = resampler->GetAVChannelIndex(AE_CH_TBL,  m_ChannelLayoutIn);
    m_idx_in[AE_CH_TBC]   = resampler->GetAVChannelIndex(AE_CH_TBC,  m_ChannelLayoutIn);
    m_idx_in[AE_CH_TBR]   = resampler->GetAVChannelIndex(AE_CH_TBR,  m_ChannelLayoutIn);
    m_idx_in[AE_CH_BLOC]  = resampler->GetAVChannelIndex(AE_CH_BLOC, m_ChannelLayoutIn);
    m_idx_in[AE_CH_BROC]  = resampler->GetAVChannelIndex(AE_CH_BROC, m_ChannelLayoutIn);

    needDSPAddonsReinit = true;
  }

  /* Detect also interleaved output stream channel positions if unknown or changed */
  if (m_ChannelLayoutOut != out->pkt->config.channel_layout)
  {
    m_ChannelLayoutOut = out->pkt->config.channel_layout;

    m_idx_out[AE_CH_FL]   = resampler->GetAVChannelIndex(AE_CH_FL,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_FR]   = resampler->GetAVChannelIndex(AE_CH_FR,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_FC]   = resampler->GetAVChannelIndex(AE_CH_FC,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_LFE]  = resampler->GetAVChannelIndex(AE_CH_LFE,  m_ChannelLayoutOut);
    m_idx_out[AE_CH_BL]   = resampler->GetAVChannelIndex(AE_CH_BL,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_BR]   = resampler->GetAVChannelIndex(AE_CH_BR,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_FLOC] = resampler->GetAVChannelIndex(AE_CH_FLOC, m_ChannelLayoutOut);
    m_idx_out[AE_CH_FROC] = resampler->GetAVChannelIndex(AE_CH_FROC, m_ChannelLayoutOut);
    m_idx_out[AE_CH_BC]   = resampler->GetAVChannelIndex(AE_CH_BC,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_SL]   = resampler->GetAVChannelIndex(AE_CH_SL,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_SR]   = resampler->GetAVChannelIndex(AE_CH_SR,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_TC]   = resampler->GetAVChannelIndex(AE_CH_TC,   m_ChannelLayoutOut);
    m_idx_out[AE_CH_TFL]  = resampler->GetAVChannelIndex(AE_CH_TFL,  m_ChannelLayoutOut);
    m_idx_out[AE_CH_TFC]  = resampler->GetAVChannelIndex(AE_CH_TFC,  m_ChannelLayoutOut);
    m_idx_out[AE_CH_TFR]  = resampler->GetAVChannelIndex(AE_CH_TFR,  m_ChannelLayoutOut);
    m_idx_out[AE_CH_TBL]  = resampler->GetAVChannelIndex(AE_CH_TBL,  m_ChannelLayoutOut);
    m_idx_out[AE_CH_TBC]  = resampler->GetAVChannelIndex(AE_CH_TBC,  m_ChannelLayoutOut);
    m_idx_out[AE_CH_TBR]  = resampler->GetAVChannelIndex(AE_CH_TBR,  m_ChannelLayoutOut);
    m_idx_out[AE_CH_BLOC] = resampler->GetAVChannelIndex(AE_CH_BLOC, m_ChannelLayoutOut);
    m_idx_out[AE_CH_BROC] = resampler->GetAVChannelIndex(AE_CH_BROC, m_ChannelLayoutOut);

    needDSPAddonsReinit = true;
  }

  if (needDSPAddonsReinit)
  {
    m_AddonSettings.lInChannelPresentFlags = 0;
    if (m_idx_in[AE_CH_FL] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FL;
    if (m_idx_in[AE_CH_FR] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FR;
    if (m_idx_in[AE_CH_FC] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FC;
    if (m_idx_in[AE_CH_LFE] >= 0)   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_LFE;
    if (m_idx_in[AE_CH_BL] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BL;
    if (m_idx_in[AE_CH_BR] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BR;
    if (m_idx_in[AE_CH_FLOC] >= 0)  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FLOC;
    if (m_idx_in[AE_CH_FROC] >= 0)  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_FROC;
    if (m_idx_in[AE_CH_BC] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BC;
    if (m_idx_in[AE_CH_SL] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_SL;
    if (m_idx_in[AE_CH_SR] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_SR;
    if (m_idx_in[AE_CH_TFL] >= 0)   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TFL;
    if (m_idx_in[AE_CH_TFR] >= 0)   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TFR;
    if (m_idx_in[AE_CH_TFC] >= 0)   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TFC;
    if (m_idx_in[AE_CH_TC] >= 0)    m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TC;
    if (m_idx_in[AE_CH_TBL] >= 0)   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TBL;
    if (m_idx_in[AE_CH_TBR] >= 0)   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TBR;
    if (m_idx_in[AE_CH_TBC] >= 0)   m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_TBC;
    if (m_idx_in[AE_CH_BLOC] >= 0)  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BLOC;
    if (m_idx_in[AE_CH_BROC] >= 0)  m_AddonSettings.lInChannelPresentFlags |= AE_DSP_PRSNT_CH_BROC;

    m_AddonSettings.lOutChannelPresentFlags = 0;
    if (m_idx_out[AE_CH_FL] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FL;
    if (m_idx_out[AE_CH_FR] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FR;
    if (m_idx_out[AE_CH_FC] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FC;
    if (m_idx_out[AE_CH_LFE] >= 0)  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_LFE;
    if (m_idx_out[AE_CH_BL] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BL;
    if (m_idx_out[AE_CH_BR] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BR;
    if (m_idx_out[AE_CH_FLOC] >= 0) m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FLOC;
    if (m_idx_out[AE_CH_FROC] >= 0) m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_FROC;
    if (m_idx_out[AE_CH_BC] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BC;
    if (m_idx_out[AE_CH_SL] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_SL;
    if (m_idx_out[AE_CH_SR] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_SR;
    if (m_idx_out[AE_CH_TFL] >= 0)  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TFL;
    if (m_idx_out[AE_CH_TFR] >= 0)  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TFR;
    if (m_idx_out[AE_CH_TFC] >= 0)  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TFC;
    if (m_idx_out[AE_CH_TC] >= 0)   m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TC;
    if (m_idx_out[AE_CH_TBL] >= 0)  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TBL;
    if (m_idx_out[AE_CH_TBR] >= 0)  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TBR;
    if (m_idx_out[AE_CH_TBC] >= 0)  m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_TBC;
    if (m_idx_out[AE_CH_BLOC] >= 0) m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BLOC;
    if (m_idx_out[AE_CH_BROC] >= 0) m_AddonSettings.lOutChannelPresentFlags |= AE_DSP_PRSNT_CH_BROC;

    ClearArray(m_ProcessArray[0], m_ProcessArraySize);
    ClearArray(m_ProcessArray[1], m_ProcessArraySize);

    m_AddonSettings.iStreamID           = m_StreamId;
    m_AddonSettings.iInChannels         = in->pkt->config.channels;
    m_AddonSettings.iOutChannels        = out->pkt->config.channels;
    m_AddonSettings.iInSamplerate       = in->pkt->config.sample_rate;
    m_AddonSettings.iProcessSamplerate  = m_Addon_InputResample.pFunctions  ? m_Addon_InputResample.pFunctions->InputResampleSampleRate(m_StreamId)   : m_AddonSettings.iInSamplerate;
    m_AddonSettings.iOutSamplerate      = m_Addon_OutputResample.pFunctions ? m_Addon_OutputResample.pFunctions->OutputResampleSampleRate(m_StreamId) : m_AddonSettings.iProcessSamplerate;

    if (m_NewMasterMode >= 0)
    {
      MasterModeChange(m_NewMasterMode, m_NewStreamType);
      m_NewMasterMode = AE_DSP_MASTER_MODE_ID_INVALID;
      m_NewStreamType = AE_DSP_ASTREAM_INVALID;
    }

    for (AE_DSP_ADDONMAP_ITR itr = m_usedMap.begin(); itr != m_usedMap.end(); itr++)
    {
      AE_DSP_ERROR err = itr->second->StreamInitialize(&m_AddonSettings);
      if (err != AE_DSP_ERROR_NO_ERROR)
      {
        CLog::Log(LOGERROR, "ActiveAE DSP - %s - addon initialize failed on %s with %s", __FUNCTION__, itr->second->GetAudioDSPName().c_str(), CActiveAEDSPAddon::ToString(err));
      }
    }

    needDSPAddonsReinit = false;
    m_ForceInit         = false;
    m_iLastProcessTime  = XbmcThreads::SystemClockMillis() * 10000;
    m_iLastProcessUsage = 0;
    m_fLastProcessUsage = 0.0f;
  }

  int ptr;
  int64_t startTime;
  unsigned int framesOut;

  float * DataIn          = (float *)in->pkt->data[0];
  float * DataOut         = (float *)out->pkt->data[0];
  int     channelsIn      = in->pkt->config.channels;
  int     channelsOut     = out->pkt->config.channels;
  unsigned int frames     = in->pkt->nb_samples;
  float **lastOutArray    = m_ProcessArray[0];
  unsigned int togglePtr  = 1;

  /* Check for big enough input array */
  if (frames > m_ProcessArraySize)
  {
    if (!ReallocProcessArray(frames))
      return false;
  }

  /* Put every channel from the interleaved buffer to a own buffer */
  ptr = 0;
  for (unsigned int i = 0; i < frames; i++)
  {
    lastOutArray[AE_DSP_CH_FL][i]   = m_idx_in[AE_CH_FL]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_FL]]   : 0.0;
    lastOutArray[AE_DSP_CH_FR][i]   = m_idx_in[AE_CH_FR]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_FR]]   : 0.0;
    lastOutArray[AE_DSP_CH_FC][i]   = m_idx_in[AE_CH_FC]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_FC]]   : 0.0;
    lastOutArray[AE_DSP_CH_LFE][i]  = m_idx_in[AE_CH_LFE]  >= 0 ? DataIn[ptr+m_idx_in[AE_CH_LFE]]  : 0.0;
    lastOutArray[AE_DSP_CH_BR][i]   = m_idx_in[AE_CH_BL]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_BL]]   : 0.0;
    lastOutArray[AE_DSP_CH_BR][i]   = m_idx_in[AE_CH_BR]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_BR]]   : 0.0;
    lastOutArray[AE_DSP_CH_FLOC][i] = m_idx_in[AE_CH_FLOC] >= 0 ? DataIn[ptr+m_idx_in[AE_CH_FLOC]] : 0.0;
    lastOutArray[AE_DSP_CH_FROC][i] = m_idx_in[AE_CH_FROC] >= 0 ? DataIn[ptr+m_idx_in[AE_CH_FROC]] : 0.0;
    lastOutArray[AE_DSP_CH_BC][i]   = m_idx_in[AE_CH_BC]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_BC]]   : 0.0;
    lastOutArray[AE_DSP_CH_SL][i]   = m_idx_in[AE_CH_SL]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_SL]]   : 0.0;
    lastOutArray[AE_DSP_CH_SR][i]   = m_idx_in[AE_CH_SR]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_SR]]   : 0.0;
    lastOutArray[AE_DSP_CH_TC][i]   = m_idx_in[AE_CH_TC]   >= 0 ? DataIn[ptr+m_idx_in[AE_CH_TC]]   : 0.0;
    lastOutArray[AE_DSP_CH_TFL][i]  = m_idx_in[AE_CH_TFL]  >= 0 ? DataIn[ptr+m_idx_in[AE_CH_TFL]]  : 0.0;
    lastOutArray[AE_DSP_CH_TFC][i]  = m_idx_in[AE_CH_TFC]  >= 0 ? DataIn[ptr+m_idx_in[AE_CH_TFC]]  : 0.0;
    lastOutArray[AE_DSP_CH_TFR][i]  = m_idx_in[AE_CH_TFR]  >= 0 ? DataIn[ptr+m_idx_in[AE_CH_TFR]]  : 0.0;
    lastOutArray[AE_DSP_CH_TBL][i]  = m_idx_in[AE_CH_TBL]  >= 0 ? DataIn[ptr+m_idx_in[AE_CH_TBL]]  : 0.0;
    lastOutArray[AE_DSP_CH_TBC][i]  = m_idx_in[AE_CH_TBC]  >= 0 ? DataIn[ptr+m_idx_in[AE_CH_TBC]]  : 0.0;
    lastOutArray[AE_DSP_CH_TBR][i]  = m_idx_in[AE_CH_TBR]  >= 0 ? DataIn[ptr+m_idx_in[AE_CH_TBR]]  : 0.0;
    lastOutArray[AE_DSP_CH_BLOC][i] = m_idx_in[AE_CH_BLOC] >= 0 ? DataIn[ptr+m_idx_in[AE_CH_BLOC]] : 0.0;
    lastOutArray[AE_DSP_CH_BROC][i] = m_idx_in[AE_CH_BROC] >= 0 ? DataIn[ptr+m_idx_in[AE_CH_BROC]] : 0.0;

    ptr += channelsIn;
  }

    /**********************************************/
   /** DSP Processing Algorithms following here **/
  /**********************************************/

  /**
   * DSP input processing
   * Can be used to have unchanged input stream..
   * All DSP addons allowed todo this.
   */
  for (unsigned int i = 0; i < m_Addons_InputProc.size(); i++)
  {
    try
    {
      if (!m_Addons_InputProc[i]->InputProcess(m_StreamId, lastOutArray, frames))
      {
        CLog::Log(LOGERROR, "ActiveAE DSP - %s - input process failed on addon No. %i", __FUNCTION__, i);
        return false;
      }
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'InputProcess' on add-on id '%i'. Please contact the developer of this add-on", e.what(), i);
      m_Addons_InputProc.erase(m_Addons_InputProc.begin()+i);
      i--;
    }
  }

  /**
   * DSP resample processing before master
   * Here a high quality resample can be performed.
   * Only one DSP addon is allowed todo this!
   */
  if (m_Addon_InputResample.pFunctions)
  {
    /* Check for big enough array */
    try
    {
      framesOut = m_Addon_InputResample.pFunctions->InputResampleProcessNeededSamplesize(m_StreamId);
      if (framesOut > m_ProcessArraySize)
      {
        if (!ReallocProcessArray(framesOut))
          return false;
      }

      startTime = CurrentHostCounter();

      frames = m_Addon_InputResample.pFunctions->InputResampleProcess(m_StreamId, lastOutArray, m_ProcessArray[togglePtr], frames);
      if (frames == 0)
        return false;

      m_Addon_InputResample.iLastTime += 1000.f * 10000.f * (CurrentHostCounter() - startTime) / hostFrequency;

      lastOutArray = m_ProcessArray[togglePtr];
      togglePtr ^= 1;
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'InputResampleProcess' on add-on'. Please contact the developer of this add-on", e.what());
      m_Addon_InputResample.pFunctions = NULL;
    }
  }

  /**
   * DSP pre processing
   * All DSP addons allowed todo this and order of it set on settings.
   */
  for (unsigned int i = 0; i < m_Addons_PreProc.size(); i++)
  {
    /* Check for big enough array */
    try
    {
      framesOut = m_Addons_PreProc[i].pFunctions->PreProcessNeededSamplesize(m_StreamId, m_Addons_PreProc[i].iAddonModeNumber);
      if (framesOut > m_ProcessArraySize)
      {
        if (!ReallocProcessArray(framesOut))
          return false;
      }

      startTime = CurrentHostCounter();

      frames = m_Addons_PreProc[i].pFunctions->PreProcess(m_StreamId, m_Addons_PreProc[i].iAddonModeNumber, lastOutArray, m_ProcessArray[togglePtr], frames);
      if (frames == 0)
        return false;

      m_Addons_PreProc[i].iLastTime += 1000.f * 10000.f * (CurrentHostCounter() - startTime) / hostFrequency;

      lastOutArray = m_ProcessArray[togglePtr];
      togglePtr ^= 1;
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'PreProcess' on add-on id '%i'. Please contact the developer of this add-on", e.what(), i);
      m_Addons_PreProc.erase(m_Addons_PreProc.begin()+i);
      i--;
    }
  }

  /**
   * DSP master processing
   * Here a channel upmix/downmix for stereo surround sound can be performed
   * Only one DSP addon is allowed todo this!
   */
  if (m_Addons_MasterProc[m_ActiveMode].pFunctions)
  {
    /* Check for big enough array */
    try
    {
      framesOut = m_Addons_MasterProc[m_ActiveMode].pFunctions->MasterProcessNeededSamplesize(m_StreamId);
      if (framesOut > m_ProcessArraySize)
      {
        if (!ReallocProcessArray(framesOut))
          return false;
      }

      startTime = CurrentHostCounter();

      frames = m_Addons_MasterProc[m_ActiveMode].pFunctions->MasterProcess(m_StreamId, lastOutArray, m_ProcessArray[togglePtr], frames);
      if (frames == 0)
        return false;

      m_Addons_MasterProc[m_ActiveMode].iLastTime += 1000.f * 10000.f * (CurrentHostCounter() - startTime) / hostFrequency;

      lastOutArray = m_ProcessArray[togglePtr];
      togglePtr ^= 1;
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'MasterProcess' on add-on'. Please contact the developer of this add-on", e.what());
      m_Addons_MasterProc.erase(m_Addons_MasterProc.begin()+m_ActiveMode);
      m_ActiveMode = AE_DSP_MASTER_MODE_ID_PASSOVER;
    }
  }

  /**
   * DSP post processing
   * On the post processing can be things performed with additional channel upmix like 6.1 to 7.1
   * or frequency/volume corrections, speaker distance handling, equalizer... .
   * All DSP addons allowed todo this and order of it set on settings.
   */
  for (unsigned int i = 0; i < m_Addons_PostProc.size(); i++)
  {
    /* Check for big enough array */
    try
    {
      framesOut = m_Addons_PostProc[i].pFunctions->PostProcessNeededSamplesize(m_StreamId, m_Addons_PostProc[i].iAddonModeNumber);
      if (framesOut > m_ProcessArraySize)
      {
        if (!ReallocProcessArray(framesOut))
          return false;
      }

      startTime = CurrentHostCounter();

      frames = m_Addons_PostProc[i].pFunctions->PostProcess(m_StreamId, m_Addons_PostProc[i].iAddonModeNumber, lastOutArray, m_ProcessArray[togglePtr], frames);
      if (frames == 0)
        return false;

      m_Addons_PostProc[i].iLastTime += 1000.f * 10000.f * (CurrentHostCounter() - startTime) / hostFrequency;

      lastOutArray = m_ProcessArray[togglePtr];
      togglePtr ^= 1;
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'PostProcess' on add-on id '%i'. Please contact the developer of this add-on", e.what(), i);
      m_Addons_PostProc.erase(m_Addons_PostProc.begin()+i);
      i--;
    }
  }

  /**
   * DSP resample processing behind master
   * Here a high quality resample can be performed.
   * Only one DSP addon is allowed todo this!
   */
  if (m_Addon_OutputResample.pFunctions)
  {
    /* Check for big enough array */
    try
    {
      framesOut = m_Addon_OutputResample.pFunctions->OutputResampleProcessNeededSamplesize(m_StreamId);
      if (framesOut > m_ProcessArraySize)
      {
        if (!ReallocProcessArray(framesOut))
          return false;
      }

      startTime = CurrentHostCounter();

      frames = m_Addon_OutputResample.pFunctions->OutputResampleProcess(m_StreamId, lastOutArray, m_ProcessArray[togglePtr], frames);
      if (frames == 0)
        return false;

      m_Addon_OutputResample.iLastTime += 1000.f * 10000.f * (CurrentHostCounter() - startTime) / hostFrequency;

      lastOutArray = m_ProcessArray[togglePtr];
      togglePtr ^= 1;
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'OutputResampleProcess' on add-on'. Please contact the developer of this add-on", e.what());
      m_Addon_InputResample.pFunctions = NULL;
    }
  }

  if (iTime >= m_iLastProcessTime + 1000*10000)
  {
    int64_t iUsage = processThread->GetAbsoluteUsage();

    if (m_iLastProcessUsage > 0 && m_iLastProcessTime > 0)
      m_fLastProcessUsage = (float)( iUsage - m_iLastProcessUsage ) / (float)( iTime - m_iLastProcessTime) * 100.0f;

    if (m_Addon_InputResample.iLastUsage > 0 && m_Addon_InputResample.iLastTime > 0)
    {
      m_Addon_InputResample.pMode->SetCPUUsage(m_fLastProcessUsage / ((float)(iUsage - m_Addon_InputResample.iLastUsage) / (float)m_Addon_InputResample.iLastTime));
      m_Addon_InputResample.iLastTime = 0;
    }
    m_Addon_InputResample.iLastUsage = iUsage;

    for (unsigned int i = 0; i < m_Addons_PreProc.size(); i++)
    {
      if (m_Addons_PreProc[i].iLastUsage > 0 && m_Addons_PreProc[i].iLastTime > 0)
      {
        m_Addons_PreProc[i].pMode->SetCPUUsage(m_fLastProcessUsage / ((float)(iUsage - m_Addons_PreProc[i].iLastUsage) / (float)m_Addons_PreProc[i].iLastTime));
        m_Addons_PreProc[i].iLastTime = 0;
      }

      m_Addons_PreProc[i].iLastUsage = iUsage;
    }

    if (m_Addons_MasterProc[m_ActiveMode].iLastUsage > 0 && m_Addons_MasterProc[m_ActiveMode].iLastTime > 0)
    {
      m_Addons_MasterProc[m_ActiveMode].pMode->SetCPUUsage(m_fLastProcessUsage / ((float)(iUsage - m_Addons_MasterProc[m_ActiveMode].iLastUsage) / (float)m_Addons_MasterProc[m_ActiveMode].iLastTime));
      m_Addons_MasterProc[m_ActiveMode].iLastTime = 0;
    }
    m_Addons_MasterProc[m_ActiveMode].iLastUsage = iUsage;

    for (unsigned int i = 0; i < m_Addons_PostProc.size(); i++)
    {
      if (m_Addons_PostProc[i].iLastUsage > 0 && m_Addons_PostProc[i].iLastTime > 0)
      {
        m_Addons_PostProc[i].pMode->SetCPUUsage(m_fLastProcessUsage / ((float)(iUsage - m_Addons_PostProc[i].iLastUsage) / (float)m_Addons_PostProc[i].iLastTime));
        m_Addons_PostProc[i].iLastTime = 0;
      }
      m_Addons_PostProc[i].iLastUsage = iUsage;
    }

    if (m_Addon_OutputResample.iLastUsage > 0 && m_Addon_OutputResample.iLastTime > 0)
    {
      m_Addon_OutputResample.pMode->SetCPUUsage(m_fLastProcessUsage / ((float)(iUsage - m_Addon_OutputResample.iLastUsage) / (float)m_Addon_OutputResample.iLastTime));
      m_Addon_OutputResample.iLastTime = 0;
    }
    m_Addon_OutputResample.iLastUsage = iUsage;


    m_iLastProcessUsage = iUsage;
    m_iLastProcessTime = iTime;
  }

  /* Put every supported channel now back to the interleaved out buffer */
  ptr = 0;
  for (unsigned int i = 0; i < frames; i++)
  {
    if (m_idx_out[AE_CH_FL] >= 0)   DataOut[ptr+m_idx_out[AE_CH_FL]]   = lastOutArray[AE_DSP_CH_FL][i];
    if (m_idx_out[AE_CH_FR] >= 0)   DataOut[ptr+m_idx_out[AE_CH_FR]]   = lastOutArray[AE_DSP_CH_FR][i];
    if (m_idx_out[AE_CH_FC] >= 0)   DataOut[ptr+m_idx_out[AE_CH_FC]]   = lastOutArray[AE_DSP_CH_FC][i];
    if (m_idx_out[AE_CH_LFE] >= 0)  DataOut[ptr+m_idx_out[AE_CH_LFE]]  = lastOutArray[AE_DSP_CH_LFE][i];
    if (m_idx_out[AE_CH_BL] >= 0)   DataOut[ptr+m_idx_out[AE_CH_BL]]   = lastOutArray[AE_DSP_CH_BL][i];
    if (m_idx_out[AE_CH_BR] >= 0)   DataOut[ptr+m_idx_out[AE_CH_BR]]   = lastOutArray[AE_DSP_CH_BR][i];
    if (m_idx_out[AE_CH_FLOC] >= 0) DataOut[ptr+m_idx_out[AE_CH_FLOC]] = lastOutArray[AE_DSP_CH_FLOC][i];
    if (m_idx_out[AE_CH_FROC] >= 0) DataOut[ptr+m_idx_out[AE_CH_FROC]] = lastOutArray[AE_DSP_CH_FROC][i];
    if (m_idx_out[AE_CH_BC] >= 0)   DataOut[ptr+m_idx_out[AE_CH_BC]]   = lastOutArray[AE_DSP_CH_BC][i];
    if (m_idx_out[AE_CH_SL] >= 0)   DataOut[ptr+m_idx_out[AE_CH_SL]]   = lastOutArray[AE_DSP_CH_SL][i];
    if (m_idx_out[AE_CH_SR] >= 0)   DataOut[ptr+m_idx_out[AE_CH_SR]]   = lastOutArray[AE_DSP_CH_SR][i];
    if (m_idx_out[AE_CH_TC] >= 0)   DataOut[ptr+m_idx_out[AE_CH_TC]]   = lastOutArray[AE_DSP_CH_TC][i];
    if (m_idx_out[AE_CH_TFL] >= 0)  DataOut[ptr+m_idx_out[AE_CH_TFL]]  = lastOutArray[AE_DSP_CH_TFL][i];
    if (m_idx_out[AE_CH_TFC] >= 0)  DataOut[ptr+m_idx_out[AE_CH_TFC]]  = lastOutArray[AE_DSP_CH_TFC][i];
    if (m_idx_out[AE_CH_TFR] >= 0)  DataOut[ptr+m_idx_out[AE_CH_TFR]]  = lastOutArray[AE_DSP_CH_TFR][i];
    if (m_idx_out[AE_CH_TBL] >= 0)  DataOut[ptr+m_idx_out[AE_CH_TBL]]  = lastOutArray[AE_DSP_CH_TBL][i];
    if (m_idx_out[AE_CH_TBC] >= 0)  DataOut[ptr+m_idx_out[AE_CH_TBC]]  = lastOutArray[AE_DSP_CH_TBC][i];
    if (m_idx_out[AE_CH_TBR] >= 0)  DataOut[ptr+m_idx_out[AE_CH_TBR]]  = lastOutArray[AE_DSP_CH_TBR][i];
    if (m_idx_out[AE_CH_BLOC] >= 0) DataOut[ptr+m_idx_out[AE_CH_BLOC]] = lastOutArray[AE_DSP_CH_BLOC][i];
    if (m_idx_out[AE_CH_BROC] >= 0) DataOut[ptr+m_idx_out[AE_CH_BROC]] = lastOutArray[AE_DSP_CH_BROC][i];

    ptr += channelsOut;
  }
  out->pkt->nb_samples = frames;

  return true;
}

bool CActiveAEDSPProcess::ReallocProcessArray(unsigned int requestSize)
{
  m_ProcessArraySize = requestSize + MIN_DSP_ARRAY_SIZE / 10;
  for (int i = 0; i < AE_DSP_CH_MAX; i++)
  {
    m_ProcessArray[0][i] = (float*)realloc(m_ProcessArray[0][i], m_ProcessArraySize*sizeof(float));
    if (m_ProcessArray[0][i] == NULL)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - %s - realloc for post process data array 0 failed", __FUNCTION__);
      return false;
    }
    m_ProcessArray[1][i] = (float*)realloc(m_ProcessArray[1][i], m_ProcessArraySize*sizeof(float));
    if (m_ProcessArray[1][i] == NULL)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - %s - realloc for post process data array 1 failed", __FUNCTION__);
      return false;
    }
  }
  return true;
}

float CActiveAEDSPProcess::GetDelay()
{
  float delay = 0;

  if (m_Addon_InputResample.pFunctions)
  {
    try
    {
      delay += m_Addon_InputResample.pFunctions->InputResampleGetDelay(m_StreamId);
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'InputResampleGetDelay' on add-on '%s'. Please contact the developer of this add-on", e.what(), m_Addon_InputResample.pAddon->GetFriendlyName().c_str());
      m_Addon_InputResample.pFunctions = NULL;
    }
  }

  for (unsigned int i = 0; i < m_Addons_PreProc.size(); i++)
  {
    try
    {
      delay += m_Addons_PreProc[i].pFunctions->PreProcessGetDelay(m_StreamId, m_Addons_PreProc[i].iAddonModeNumber);
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'PreProcessGetDelay' on add-on '%s'. Please contact the developer of this add-on", e.what(), m_Addons_PreProc[i].pAddon->GetFriendlyName().c_str());
      m_Addons_PreProc.erase(m_Addons_PreProc.begin()+i);
      i--;
    }
  }

  if (m_Addons_MasterProc[m_ActiveMode].pFunctions)
  {
    try
    {
      delay += m_Addons_MasterProc[m_ActiveMode].pFunctions->MasterProcessGetDelay(m_StreamId);
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'MasterProcessGetDelay' on add-on '%s'. Please contact the developer of this add-on", e.what(), m_Addons_MasterProc[m_ActiveMode].pAddon->GetFriendlyName().c_str());
      m_Addons_MasterProc.erase(m_Addons_MasterProc.begin()+m_ActiveMode);
      m_ActiveMode = AE_DSP_MASTER_MODE_ID_PASSOVER;
    }
  }

  for (unsigned int i = 0; i < m_Addons_PostProc.size(); i++)
  {
    try
    {
      delay += m_Addons_PostProc[i].pFunctions->PostProcessGetDelay(m_StreamId, m_Addons_PostProc[i].iAddonModeNumber);
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'PostProcessGetDelay' on add-on '%s'. Please contact the developer of this add-on", e.what(), m_Addons_PostProc[i].pAddon->GetFriendlyName().c_str());
      m_Addons_PostProc.erase(m_Addons_PostProc.begin()+i);
      i--;
    }
  }

  if (m_Addon_OutputResample.pFunctions)
  {
    try
    {
      delay += m_Addon_OutputResample.pFunctions->OutputResampleGetDelay(m_StreamId);
    }
    catch (exception &e)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call 'OutputResampleGetDelay' on add-on '%s'. Please contact the developer of this add-on", e.what(), m_Addon_OutputResample.pAddon->GetFriendlyName().c_str());
      m_Addon_InputResample.pFunctions = NULL;
    }
  }

  return delay;
}

bool CActiveAEDSPProcess::SetMasterMode(AE_DSP_STREAMTYPE streamType, int iModeID, bool bSwitchStreamType)
{
  /*!
   * if the unique master mode id is already used a reinit is not needed
   */
  if (m_ActiveMode >= AE_DSP_MASTER_MODE_ID_PASSOVER && m_Addons_MasterProc[m_ActiveMode].pMode->ModeID() == iModeID && !bSwitchStreamType)
    return true;

  CSingleLock lock(m_restartSection);

  m_NewMasterMode = iModeID;
  m_NewStreamType = bSwitchStreamType ? streamType : AE_DSP_ASTREAM_INVALID;
  m_ForceInit     = true;
  return true;
}

int CActiveAEDSPProcess::GetMasterModeID()
{
  return m_ActiveMode < 0 ? AE_DSP_MASTER_MODE_ID_INVALID : m_Addons_MasterProc[m_ActiveMode].pMode->ModeID();
}

void CActiveAEDSPProcess::GetActiveModes(std::vector<CActiveAEDSPModePtr> &modes)
{
  CSingleLock lock(m_critSection);

  if (m_Addon_InputResample.pFunctions != NULL)
    modes.push_back(m_Addon_InputResample.pMode);

  for (unsigned int i = 0; i < m_Addons_PreProc.size(); i++)
    modes.push_back(m_Addons_PreProc[i].pMode);

  if (m_Addons_MasterProc[m_ActiveMode].pFunctions != NULL)
    modes.push_back(m_Addons_MasterProc[m_ActiveMode].pMode);

  for (unsigned int i = 0; i < m_Addons_PostProc.size(); i++)
    modes.push_back(m_Addons_PostProc[i].pMode);

  if (m_Addon_OutputResample.pFunctions != NULL)
    modes.push_back(m_Addon_OutputResample.pMode);
}

void CActiveAEDSPProcess::GetAvailableMasterModes(AE_DSP_STREAMTYPE streamType, std::vector<CActiveAEDSPModePtr> &modes)
{
  CSingleLock lock(m_critSection);

  for (unsigned int i = 0; i < m_Addons_MasterProc.size(); i++)
  {
    if (m_Addons_MasterProc[i].pMode->SupportStreamType(streamType))
      modes.push_back(m_Addons_MasterProc[i].pMode);
  }
}

CActiveAEDSPModePtr CActiveAEDSPProcess::GetMasterModeRunning() const
{
  CSingleLock lock(m_critSection);

  CActiveAEDSPModePtr mode;

  if (m_ActiveMode < 0)
    return mode;

  mode = m_Addons_MasterProc[m_ActiveMode].pMode;
  return mode;
}

bool CActiveAEDSPProcess::IsModeActive(AE_DSP_MENUHOOK_CAT category, int iAddonId, unsigned int iModeNumber)
{
  std::vector <sDSPProcessHandle> *m_Addons = NULL;

  switch (category)
  {
    case AE_DSP_MENUHOOK_MASTER_PROCESS:
      m_Addons = &m_Addons_MasterProc;
      break;
    case AE_DSP_MENUHOOK_PRE_PROCESS:
      m_Addons = &m_Addons_PreProc;
      break;
    case AE_DSP_MENUHOOK_POST_PROCESS:
      m_Addons = &m_Addons_PostProc;
      break;
    case AE_DSP_MENUHOOK_RESAMPLE:
      {
        if (m_Addon_InputResample.iAddonModeNumber > 0 &&
            m_Addon_InputResample.pMode->AddonID() == iAddonId &&
            m_Addon_InputResample.pMode->AddonModeNumber() == iModeNumber)
          return true;

        if (m_Addon_OutputResample.iAddonModeNumber > 0 &&
            m_Addon_OutputResample.pMode->AddonID() == iAddonId &&
            m_Addon_OutputResample.pMode->AddonModeNumber() == iModeNumber)
          return true;
      }
    default:
      break;
  }

  if (m_Addons)
  {
    for (unsigned int i = 0; i < m_Addons->size(); i++)
    {
      if (m_Addons->at(i).iAddonModeNumber > 0 &&
          m_Addons->at(i).pMode->AddonID() == iAddonId &&
          m_Addons->at(i).pMode->AddonModeNumber() == iModeNumber)
        return true;
    }
  }
  return false;
}
