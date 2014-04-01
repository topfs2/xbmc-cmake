/*
 *      Copyright (C) 2005-2014 Team XBMC
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

#include "system.h"
#include "GUIDialogAudioSettings.h"
#include "GUIPassword.h"
#include "Application.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogYesNo.h"
#include "addons/Skin.h"
#include "profiles/ProfilesManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/GUIImage.h"
#include "guilib/GUIWindowManager.h"
#include "cores/AudioEngine/DSPAddons/ActiveAEDSPDatabase.h"
#include "cores/AudioEngine/DSPAddons/ActiveAEDSPMode.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#include "cores/IPlayer.h"
#include "utils/StringUtils.h"

using namespace std;
using namespace XFILE;
using namespace ActiveAE;

/*! Skin control variables */
#define CONTROL_SETTINGS_LABEL                  2
#define CONTROL_ADDON_STREAM_INFO               20
#define CONTROL_ADDON_MODE_IMAGE                21
#define CONTROL_OVERRIDE_IMAGE                  22
#define CONTROL_ORIGINAL_IMAGE                  23
#define CONTROL_TYPE_LABEL                      24
#define CONTROL_AUDIO_CHANNELS_IMAGE            25

#define CONTROL_START                           30

/*! Menu types */
#define MENU_MAIN                               0
#define MENU_MASTER_MODE_SETUP                  1
#define MENU_SPEAKER_OUTPUT_SETUP               2
#define MENU_AUDIO_INFORMATION                  3

/*! Control id's */
#define AUDIO_SETTING_CONTENT_TYPE              1
#define AUDIO_SETTING_CONTENT_LABEL             2
#define AUDIO_SETTING_STREAM_SELECTION          3
#define AUDIO_SETTINGS_DELAY                    4
#define AUDIO_SEPERATOR_1                       5
#define AUDIO_SETTINGS_VOLUME                   6
#define AUDIO_SETTINGS_VOLUME_AMPLIFICATION     7
#define AUDIO_SEPERATOR_2                       8
#define AUDIO_BUTTON_MASTER_MODE_SETUP          9
#define AUDIO_BUTTON_SPEAKER_OUTPUT_SETUP       10
#define AUDIO_BUTTON_RESAMPLER_SETUP            11
#define AUDIO_BUTTON_PRE_PROCESS_SETUP          12
#define AUDIO_BUTTON_MISC_SETUP                 13
#define AUDIO_BUTTON_AUDIO_INFORMATION          14
#define AUDIO_SEPERATOR_3                       15
#define AUDIO_SETTINGS_MAKE_DEFAULT             16
#define AUDIO_SELECT_MASTER_MODE                17
#define AUDIO_SETTING_CONTENT_TYPE_INPUT        18
#define AUDIO_SETTING_CONTENT_TYPE_PREPROC      19
#define AUDIO_SETTING_CONTENT_TYPE_MASTER       20
#define AUDIO_SETTING_CONTENT_TYPE_POSTPROC     21
#define AUDIO_SETTING_CONTENT_TYPE_OUTPUT       22
#define AUDIO_SETTING_CONTENT_INPUT_CHANNELS    23
#define AUDIO_SETTING_CONTENT_INPUT_CHANNEL_NAMES    24
#define AUDIO_SETTING_CONTENT_INPUT_SAMPLERATE  25
#define AUDIO_SETTING_CONTENT_OUTPUT_CHANNELS   26
#define AUDIO_SETTING_CONTENT_OUTPUT_CHANNEL_NAMES    27
#define AUDIO_SETTING_CONTENT_OUTPUT_SAMPLERATE 28
#define AUDIO_SETTING_CONTENT_CPU_USAGE         29

/*! Addon settings control list id's */
#define AUDIO_SETTINGS_MENUS                    31 // to 60
#define AUDIO_SETTINGS_MENUS_END                60

/*! Label list id */
#define INPUT_TYPE_LABEL_START                  15001


CGUIDialogAudioDSPSettings::CGUIDialogAudioDSPSettings(void)
    : CGUIDialogSettings(WINDOW_DIALOG_AUDIO_DSP_OSD_SETTINGS, "AudioDSPSettings.xml")
{
  m_ActiveStreamId  = 0;
  m_CurrentMenu     = MENU_MAIN;
}

CGUIDialogAudioDSPSettings::~CGUIDialogAudioDSPSettings(void)
{
}

bool CGUIDialogAudioDSPSettings::OnBack(int actionID)
{
  int oldMenu = m_CurrentMenu;
  switch (m_CurrentMenu)
  {
    case AUDIO_BUTTON_MASTER_MODE_SETUP:
    case AUDIO_BUTTON_SPEAKER_OUTPUT_SETUP:
    case AUDIO_BUTTON_AUDIO_INFORMATION:
    case AUDIO_BUTTON_RESAMPLER_SETUP:
    case AUDIO_BUTTON_PRE_PROCESS_SETUP:
    case AUDIO_BUTTON_MISC_SETUP:
      FreeControls();
      m_CurrentMenu = MENU_MAIN;
      GoWindowBack(oldMenu);
      return true;
  }

  return CGUIDialogSettings::OnBack(actionID);
}

void CGUIDialogAudioDSPSettings::GoWindowBack(unsigned int prevId)
{
  CreateSettings();
  SetInitialVisibility();
  SetupPage();

  // set the default focus control
  m_lastControlID = CONTROL_START;

  for (unsigned int i = 0; i < m_settings.size(); i++)
  {
    if (m_settings.at(i).id == prevId)
    {
      m_lastControlID = CONTROL_START + i;
      break;
    }
  }

  CGUIDialog::OnInitWindow();
}

void CGUIDialogAudioDSPSettings::CreateSettings()
{
  m_usePopupSliders = g_SkinInfo->HasSkinFile("DialogSlider.xml");

  // clear out any old settings
  m_settings.clear();

  m_ActiveStreamId      = CActiveAEDSP::Get().GetActiveStreamId();
  m_ActiveStreamProcess = CActiveAEDSP::Get().GetDSPProcess(m_ActiveStreamId);
  if (m_ActiveStreamId == (unsigned int)-1 || !m_ActiveStreamProcess)
  {
    m_CurrentMenu = MENU_MAIN;
    Close(true);
    return;
  }

  SET_CONTROL_HIDDEN(CONTROL_ADDON_MODE_IMAGE);
  SET_CONTROL_HIDDEN(CONTROL_OVERRIDE_IMAGE);

  int modeUniqueId;
  m_ActiveStreamProcess->GetMasterModeTypeInformation(m_streamTypeUsed, m_baseTypeUsed, modeUniqueId);

  // create our settings
  if (m_CurrentMenu == MENU_MAIN)
  {
    SET_CONTROL_LABEL(CONTROL_SETTINGS_LABEL, g_localizeStrings.Get(15028));

    int modesAvailable = 0;
    for (int i = 0; i < AE_DSP_ASTREAM_AUTO; i++)
    {
      m_MasterModes[i].clear();
      m_ActiveStreamProcess->GetAvailableMasterModes((AE_DSP_STREAMTYPE)i, m_MasterModes[i]);
      if (!m_MasterModes[i].empty()) modesAvailable++;
    }

    if (modesAvailable > 0)
    {
      /* about size() > 1, it is always the fallback (ignore of master processing) present. */
      vector<pair<int, int> > modeEntries;
      if (m_MasterModes[AE_DSP_ASTREAM_BASIC].size() > 1)
        modeEntries.push_back(pair<int, int>(AE_DSP_ASTREAM_BASIC,   INPUT_TYPE_LABEL_START+AE_DSP_ASTREAM_BASIC));
      if (m_MasterModes[AE_DSP_ASTREAM_MUSIC].size() > 1)
        modeEntries.push_back(pair<int, int>(AE_DSP_ASTREAM_MUSIC,   INPUT_TYPE_LABEL_START+AE_DSP_ASTREAM_MUSIC));
      if (m_MasterModes[AE_DSP_ASTREAM_MOVIE].size() > 1)
        modeEntries.push_back(pair<int, int>(AE_DSP_ASTREAM_MOVIE,   INPUT_TYPE_LABEL_START+AE_DSP_ASTREAM_MOVIE));
      if (m_MasterModes[AE_DSP_ASTREAM_GAME].size() > 1)
        modeEntries.push_back(pair<int, int>(AE_DSP_ASTREAM_GAME,    INPUT_TYPE_LABEL_START+AE_DSP_ASTREAM_GAME));
      if (m_MasterModes[AE_DSP_ASTREAM_APP].size() > 1)
        modeEntries.push_back(pair<int, int>(AE_DSP_ASTREAM_APP,     INPUT_TYPE_LABEL_START+AE_DSP_ASTREAM_APP));
      if (m_MasterModes[AE_DSP_ASTREAM_MESSAGE].size() > 1)
        modeEntries.push_back(pair<int, int>(AE_DSP_ASTREAM_MESSAGE, INPUT_TYPE_LABEL_START+AE_DSP_ASTREAM_MESSAGE));
      if (m_MasterModes[AE_DSP_ASTREAM_PHONE].size() > 1)
        modeEntries.push_back(pair<int, int>(AE_DSP_ASTREAM_PHONE,   INPUT_TYPE_LABEL_START+AE_DSP_ASTREAM_PHONE));
      if (modesAvailable > 1 && m_MasterModes[m_streamTypeUsed].size() > 1)
        modeEntries.insert(modeEntries.begin(), pair<int, int>(AE_DSP_ASTREAM_AUTO, 14061));

      AddSpin(AUDIO_SETTING_CONTENT_TYPE, 15021, (int*)&CMediaSettings::Get().GetCurrentAudioSettings().m_MasterStreamTypeSel, modeEntries);
    }

    CMediaSettings::Get().GetCurrentAudioSettings().m_MasterStreamType = m_streamTypeUsed;
    SET_CONTROL_LABEL(CONTROL_TYPE_LABEL, g_localizeStrings.Get(m_ActiveStreamProcess->GetDetectedStreamType()+INPUT_TYPE_LABEL_START));

    m_AddonMasterModeSetupPresent = false;

    vector<pair<int, CStdString> > entries;
    for (unsigned int i = 0; i < m_MasterModes[m_streamTypeUsed].size(); i++)
    {
      if (m_MasterModes[m_streamTypeUsed][i])
      {
        AE_DSP_ADDON client;
        if (m_MasterModes[m_streamTypeUsed][i]->ModeID() == AE_DSP_MASTER_MODE_ID_PASSOVER)
        {
          entries.push_back(pair<int, CStdString>(AE_DSP_MASTER_MODE_ID_PASSOVER, g_localizeStrings.Get(m_MasterModes[m_streamTypeUsed][i]->ModeName())));
        }
        else if (CActiveAEDSP::Get().GetAudioDSPAddon(m_MasterModes[m_streamTypeUsed][i]->AddonID(), client))
        {
          entries.push_back(pair<int, CStdString>(m_MasterModes[m_streamTypeUsed][i]->ModeID(), client->GetString(m_MasterModes[m_streamTypeUsed][i]->ModeName())));
          if (!m_AddonMasterModeSetupPresent)
            m_AddonMasterModeSetupPresent = m_MasterModes[m_streamTypeUsed][i]->HasSettingsDialog();
        }
      }
    }
    AddSpin(AUDIO_SELECT_MASTER_MODE,
            15022,
            (int*)&CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[m_streamTypeUsed][m_baseTypeUsed],
            entries);

    AddSeparator(AUDIO_SEPERATOR_1);

    m_volume = g_application.GetVolume(false);
    AddSlider(AUDIO_SETTINGS_VOLUME, 13376, &m_volume, VOLUME_MINIMUM, VOLUME_MAXIMUM / 100.0f, VOLUME_MAXIMUM, StringUtils::FormatPercentAsDecibel, false);
    if (SupportsAudioFeature(IPC_AUD_AMP))
    {
      AddSlider(AUDIO_SETTINGS_VOLUME_AMPLIFICATION,
                660,
                &CMediaSettings::Get().GetCurrentVideoSettings().m_VolumeAmplification,
                VOLUME_DRC_MINIMUM * 0.01f, (VOLUME_DRC_MAXIMUM - VOLUME_DRC_MINIMUM) / 6000.0f, VOLUME_DRC_MAXIMUM * 0.01f,
                StringUtils::FormatDecibel,
                false);
      AddSeparator(AUDIO_SEPERATOR_2);
    }

    if (m_AddonMasterModeSetupPresent)
      AddButton(AUDIO_BUTTON_MASTER_MODE_SETUP, 15025);
    AddButton(AUDIO_BUTTON_SPEAKER_OUTPUT_SETUP, 15026);
    if (CActiveAEDSP::Get().HaveMenuHooks(AE_DSP_MENUHOOK_RESAMPLE))
      AddButton(AUDIO_BUTTON_RESAMPLER_SETUP, 15033);
    if (CActiveAEDSP::Get().HaveMenuHooks(AE_DSP_MENUHOOK_PRE_PROCESS))
      AddButton(AUDIO_BUTTON_PRE_PROCESS_SETUP, 15039);
    if (CActiveAEDSP::Get().HaveMenuHooks(AE_DSP_MENUHOOK_MISCELLANEOUS))
      AddButton(AUDIO_BUTTON_MISC_SETUP, 15034);
    AddButton(AUDIO_BUTTON_AUDIO_INFORMATION, 15027);

    AddSeparator(AUDIO_SEPERATOR_3);
    AddButton(AUDIO_SETTINGS_MAKE_DEFAULT, 15032);
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_MASTER_MODE_SETUP)
  {
    SET_CONTROL_LABEL(CONTROL_SETTINGS_LABEL, g_localizeStrings.Get(15029));

    for (unsigned int i = 0; i < m_MasterModes[m_streamTypeUsed].size(); i++)
    {
      AE_DSP_ADDON client;
      if (CActiveAEDSP::Get().GetAudioDSPAddon(m_MasterModes[m_streamTypeUsed][i]->AddonID(), client))
      {
        if (m_MasterModes[m_streamTypeUsed][i]->HasSettingsDialog())
          AddButton(AUDIO_SETTINGS_MENUS+i, client->GetString(m_MasterModes[m_streamTypeUsed][i]->ModeSetupName()));
      }
    }
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_SPEAKER_OUTPUT_SETUP)
  {
    SET_CONTROL_LABEL(CONTROL_SETTINGS_LABEL, g_localizeStrings.Get(15030));

    if (SupportsAudioFeature(IPC_AUD_OFFSET))
      AddSlider(AUDIO_SETTINGS_DELAY, 297, &CMediaSettings::Get().GetCurrentVideoSettings().m_AudioDelay, -g_advancedSettings.m_videoAudioDelayRange, .025f, g_advancedSettings.m_videoAudioDelayRange, StringUtils::FormatDelay);

    AddSeparator(AUDIO_SEPERATOR_1);

    m_Menus.clear();
    GetAudioDSPMenus(AE_DSP_MENUHOOK_POST_PROCESS, m_Menus);
    AddAddonButtons();
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_RESAMPLER_SETUP)
  {
    SET_CONTROL_LABEL(CONTROL_SETTINGS_LABEL, g_localizeStrings.Get(15035));

    m_Menus.clear();
    GetAudioDSPMenus(AE_DSP_MENUHOOK_RESAMPLE, m_Menus);
    AddAddonButtons();
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_PRE_PROCESS_SETUP)
  {
    SET_CONTROL_LABEL(CONTROL_SETTINGS_LABEL, g_localizeStrings.Get(15037));

    m_Menus.clear();
    GetAudioDSPMenus(AE_DSP_MENUHOOK_PRE_PROCESS, m_Menus);
    AddAddonButtons();
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_MISC_SETUP)
  {
    SET_CONTROL_LABEL(CONTROL_SETTINGS_LABEL, g_localizeStrings.Get(15038));

    m_Menus.clear();
    GetAudioDSPMenus(AE_DSP_MENUHOOK_MISCELLANEOUS, m_Menus);
    AddAddonButtons();
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_AUDIO_INFORMATION)
  {
    m_usePopupSliders = false;

    SET_CONTROL_LABEL(CONTROL_SETTINGS_LABEL, g_localizeStrings.Get(15031));

    m_ActiveModes.clear();
    m_Menus.clear();
    m_ActiveStreamProcess->GetActiveModes(m_ActiveModes);
    m_ActiveModesCPUUsage.resize(m_ActiveModes.size());

    AddLabel(AUDIO_SETTING_CONTENT_LABEL, 15089);
    m_InputChannels = StringUtils::Format("%i", m_ActiveStreamProcess->GetInputChannels());
    AddString(AUDIO_SETTING_CONTENT_INPUT_CHANNELS, 15091, &m_InputChannels, false);
    m_InputChannelNames = m_ActiveStreamProcess->GetInputChannelNames();
    AddString(AUDIO_SETTING_CONTENT_INPUT_CHANNEL_NAMES, 15093, &m_InputChannelNames, false);
    m_InputSamplerate = StringUtils::Format("%i Hz", (int)m_ActiveStreamProcess->GetInputSamplerate());
    AddString(AUDIO_SETTING_CONTENT_INPUT_SAMPLERATE, 15092, &m_InputSamplerate, false);

    AddSeparator(AUDIO_SEPERATOR_1);

    AddLabel(AUDIO_SETTING_CONTENT_LABEL, 15090);
    m_OutputChannels = StringUtils::Format("%i", m_ActiveStreamProcess->GetOutputChannels());
    AddString(AUDIO_SETTING_CONTENT_OUTPUT_CHANNELS, 15091, &m_OutputChannels, false);
    m_OutputChannelNames = m_ActiveStreamProcess->GetOutputChannelNames();
    AddString(AUDIO_SETTING_CONTENT_OUTPUT_CHANNEL_NAMES, 15093, &m_OutputChannelNames, false);
    m_OutputSamplerate = StringUtils::Format("%i Hz", (int)m_ActiveStreamProcess->GetOutputSamplerate());
    AddString(AUDIO_SETTING_CONTENT_OUTPUT_SAMPLERATE, 15092, &m_OutputSamplerate, false);

    AddSeparator(AUDIO_SEPERATOR_2);

    AddLabel(AUDIO_SETTING_CONTENT_LABEL, 15081);
    m_CPUUsage = StringUtils::Format("%.02f %%", m_ActiveStreamProcess->GetCPUUsage());
    AddString(AUDIO_SETTING_CONTENT_CPU_USAGE, 15094, &m_CPUUsage, false);

    bool foundPreProcess, foundPostProcess = false;
    for (unsigned int i = 0; i < m_ActiveModes.size(); i++)
    {
      AE_DSP_ADDON client;
      if (CActiveAEDSP::Get().GetAudioDSPAddon(m_ActiveModes[i]->AddonID(), client))
      {
        CStdString label;
        switch (m_ActiveModes[i]->ModeType())
        {
          case AE_DSP_MODE_TYPE_INPUT_RESAMPLE:
            AddLabel(AUDIO_SETTING_CONTENT_TYPE_INPUT, 15087);
            label = StringUtils::Format(g_localizeStrings.Get(15082), m_ActiveStreamProcess->GetProcessSamplerate());
            break;
          case AE_DSP_MODE_TYPE_OUTPUT_RESAMPLE:
            AddLabel(AUDIO_SETTING_CONTENT_TYPE_OUTPUT, 15088);
            label = StringUtils::Format(g_localizeStrings.Get(15083), m_ActiveStreamProcess->GetOutputSamplerate());
            break;
          case AE_DSP_MODE_TYPE_MASTER_PROCESS:
            AddLabel(AUDIO_SETTING_CONTENT_TYPE_MASTER, 15084);
            label = client->GetString(m_ActiveModes[i]->ModeName());
            break;
          case AE_DSP_MODE_TYPE_PRE_PROCESS:
            if (!foundPreProcess)
            {
              foundPreProcess = true;
              AddLabel(AUDIO_SETTING_CONTENT_TYPE_PREPROC, 15085);
            }
            label = client->GetString(m_ActiveModes[i]->ModeName());
            break;
          case AE_DSP_MODE_TYPE_POST_PROCESS:
            if (!foundPostProcess)
            {
              foundPostProcess = true;
              AddLabel(AUDIO_SETTING_CONTENT_TYPE_POSTPROC, 15086);
            }
            label = client->GetString(m_ActiveModes[i]->ModeName());
            break;
          default:
          {
            label += client->GetString(m_ActiveModes[i]->ModeName());
            label += " - ";
            label += client->GetFriendlyName();
          }
        };
        m_ActiveModesCPUUsage[i] = m_ActiveModes[i]->CPUUsage();
        AddButton(AUDIO_SETTINGS_MENUS+i, label, &m_ActiveModesCPUUsage[i], 0.0f, 1.0f, 100.0f, StringUtils::FormatPercent);

        MenuHookMember menu;
        menu.clientId = -1;

        AE_DSP_MENUHOOKS hooks;
        if (CActiveAEDSP::Get().GetMenuHooks(m_ActiveModes[i]->AddonID(), AE_DSP_MENUHOOK_INFORMATION, hooks))
        {
          for (unsigned int j = 0; i < hooks.size(); j++)
          {
            if (hooks[j].iRelevantModeId != m_ActiveModes[i]->AddonModeNumber())
              continue;

            menu.clientId                 = m_ActiveModes[i]->AddonID();
            menu.hook.category            = hooks[j].category;
            menu.hook.iHookId             = hooks[j].iHookId;
            menu.hook.iLocalizedStringId  = hooks[j].iLocalizedStringId;
            menu.hook.iRelevantModeId     = hooks[j].iRelevantModeId;
            break;
          }
        }
        m_Menus.push_back(menu);
      }
    }
  }

  UpdateModeIcons();
}

void CGUIDialogAudioDSPSettings::OnSettingChanged(SettingInfo &setting)
{
  if (g_application.m_pPlayer->HasPlayer())
    g_application.m_pPlayer->GetAudioCapabilities(m_audioCaps);

  // check and update anything that needs it
  if (setting.id == AUDIO_SETTINGS_MAKE_DEFAULT)
  {
    if (!g_passwordManager.CheckSettingLevelLock(SettingLevelExpert) &&
        CProfilesManager::Get().GetMasterProfile().getLockMode() != LOCK_MODE_EVERYONE)
      return;

    // prompt user if they are sure
    if (CGUIDialogYesNo::ShowAndGetInput(12376, 750, 0, 12377))
    { // reset the settings
      CActiveAEDSPDatabase db;
      db.Open();
      db.EraseActiveDSPSettings();
      db.Close();
      CMediaSettings::Get().GetDefaultAudioSettings() = CMediaSettings::Get().GetCurrentAudioSettings();
      CMediaSettings::Get().GetDefaultAudioSettings().m_MasterStreamType = AE_DSP_ASTREAM_AUTO;
      CSettings::Get().Save();
    }
  }

  if (m_CurrentMenu == MENU_MAIN)
  {
    if (setting.id == AUDIO_SETTING_CONTENT_TYPE)
    {
      int type = CMediaSettings::Get().GetCurrentAudioSettings().m_MasterStreamTypeSel;
      if (type == AE_DSP_ASTREAM_AUTO)
        type = m_ActiveStreamProcess->GetDetectedStreamType();

      CMediaSettings::Get().GetCurrentAudioSettings().m_MasterStreamType = type;

      /* Set the input stream type if any modes are available for this type */
      if (type >= AE_DSP_ASTREAM_BASIC && type < AE_DSP_ASTREAM_AUTO && !m_MasterModes[type].empty())
      {
        /* Find the master mode id for the selected stream type if it was not known before */
        if (CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[type][m_baseTypeUsed] < 0)
          CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[type][m_baseTypeUsed] = m_MasterModes[type][0]->ModeID();

        /* Switch now the master mode and stream type for audio dsp processing */
        m_ActiveStreamProcess->SetMasterMode((AE_DSP_STREAMTYPE)type,
                                             CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[type][m_baseTypeUsed],
                                             true);

        UpdateModeIcons();
      }
      else
      {
        CLog::Log(LOGERROR, "ActiveAE DSP Settings - %s - Change of audio stream type failed (type = %i)", __FUNCTION__, type);
      }
    }
    else if (setting.id == AUDIO_SETTING_STREAM_SELECTION)
    {
      FreeControls();
      OnInitWindow();
    }
    else if (setting.id == AUDIO_SELECT_MASTER_MODE)
    {
      m_ActiveStreamProcess->SetMasterMode(m_streamTypeUsed,
                                           CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[m_streamTypeUsed][m_baseTypeUsed]);
      UpdateModeIcons();
    }
    else if (setting.id == AUDIO_SETTINGS_VOLUME)
    {
      g_application.SetVolume(m_volume, false); //false - value is not in percent
    }
    else if (setting.id == AUDIO_SETTINGS_VOLUME_AMPLIFICATION)
    {
      g_application.m_pPlayer->SetDynamicRangeCompression((long)(CMediaSettings::Get().GetCurrentVideoSettings().m_VolumeAmplification * 100));
    }
    else if (setting.id >= AUDIO_BUTTON_MASTER_MODE_SETUP && setting.id <= AUDIO_BUTTON_AUDIO_INFORMATION)
    {
      FreeControls();
      m_CurrentMenu = setting.id;
      OnInitWindow();
    }
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_MASTER_MODE_SETUP)
  {
    unsigned int setupEntry = setting.id-AUDIO_SETTINGS_MENUS;
    if (setting.id >= AUDIO_SETTINGS_MENUS && setting.id <= AUDIO_SETTINGS_MENUS_END && setupEntry < m_MasterModes[m_streamTypeUsed].size())
    {
      AE_DSP_ADDON client;
      if (m_MasterModes[m_streamTypeUsed][setupEntry]->HasSettingsDialog() && CActiveAEDSP::Get().GetAudioDSPAddon(m_MasterModes[m_streamTypeUsed][setupEntry]->AddonID(), client))
        OpenAudioDSPMenu(AE_DSP_MENUHOOK_MASTER_PROCESS, client, m_MasterModes[m_streamTypeUsed][setupEntry]->AddonModeNumber(), m_MasterModes[m_streamTypeUsed][setupEntry]->ModeName());
    }
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_SPEAKER_OUTPUT_SETUP)
  {
    unsigned int setupEntry = setting.id-AUDIO_SETTINGS_MENUS;
    if (setting.id >= AUDIO_SETTINGS_MENUS && setting.id <= AUDIO_SETTINGS_MENUS_END && setupEntry < m_Menus.size())
    {
      AE_DSP_ADDON client;
      if (CActiveAEDSP::Get().GetAudioDSPAddon(m_Menus[setupEntry].clientId, client))
        OpenAudioDSPMenu(AE_DSP_MENUHOOK_POST_PROCESS, client, m_Menus[setupEntry].hook.iHookId, m_Menus[setupEntry].hook.iLocalizedStringId);
    }
    else if (setting.id == AUDIO_SETTINGS_DELAY)
      g_application.m_pPlayer->SetAVDelay(CMediaSettings::Get().GetCurrentVideoSettings().m_AudioDelay);
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_RESAMPLER_SETUP)
  {
    unsigned int setupEntry = setting.id-AUDIO_SETTINGS_MENUS;
    if (setting.id >= AUDIO_SETTINGS_MENUS && setting.id <= AUDIO_SETTINGS_MENUS_END && setupEntry < m_Menus.size())
    {
      AE_DSP_ADDON client;
      if (CActiveAEDSP::Get().GetAudioDSPAddon(m_Menus[setupEntry].clientId, client))
        OpenAudioDSPMenu(m_Menus[setupEntry].hook.category, client, m_Menus[setupEntry].hook.iHookId, m_Menus[setupEntry].hook.iLocalizedStringId);
    }
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_PRE_PROCESS_SETUP)
  {
    unsigned int setupEntry = setting.id-AUDIO_SETTINGS_MENUS;
    if (setting.id >= AUDIO_SETTINGS_MENUS && setting.id <= AUDIO_SETTINGS_MENUS_END && setupEntry < m_Menus.size())
    {
      AE_DSP_ADDON client;
      if (CActiveAEDSP::Get().GetAudioDSPAddon(m_Menus[setupEntry].clientId, client))
        OpenAudioDSPMenu(AE_DSP_MENUHOOK_PRE_PROCESS, client, m_Menus[setupEntry].hook.iHookId, m_Menus[setupEntry].hook.iLocalizedStringId);
    }
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_MISC_SETUP)
  {
    unsigned int setupEntry = setting.id-AUDIO_SETTINGS_MENUS;
    if (setting.id >= AUDIO_SETTINGS_MENUS && setting.id <= AUDIO_SETTINGS_MENUS_END && setupEntry < m_Menus.size())
    {
      AE_DSP_ADDON client;
      if (CActiveAEDSP::Get().GetAudioDSPAddon(m_Menus[setupEntry].clientId, client))
        OpenAudioDSPMenu(AE_DSP_MENUHOOK_MISCELLANEOUS, client, m_Menus[setupEntry].hook.iHookId, m_Menus[setupEntry].hook.iLocalizedStringId);
    }
  }
  else if (m_CurrentMenu == AUDIO_BUTTON_AUDIO_INFORMATION)
  {
    unsigned int setupEntry = setting.id-AUDIO_SETTINGS_MENUS;
    if (setting.id >= AUDIO_SETTINGS_MENUS && setting.id <= AUDIO_SETTINGS_MENUS_END && setupEntry < m_Menus.size())
    {
      AE_DSP_ADDON client;
      if (m_Menus[setupEntry].clientId != -1 && CActiveAEDSP::Get().GetAudioDSPAddon(m_Menus[setupEntry].clientId, client))
      {
        OpenAudioDSPMenu(AE_DSP_MENUHOOK_INFORMATION, client, m_Menus[setupEntry].hook.iHookId, m_Menus[setupEntry].hook.iLocalizedStringId);
        return;
      }
      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, g_localizeStrings.Get(15031), g_localizeStrings.Get(15095));
    }
  }
}

void CGUIDialogAudioDSPSettings::FrameMove()
{
  if (CActiveAEDSP::Get().GetProcessingStreamsAmount() == 0)
  {
    Close(true);
    return;
  }

  m_volume = g_application.GetVolume(false);
  UpdateSetting(AUDIO_SETTINGS_VOLUME);

  if (g_application.m_pPlayer->HasPlayer())
  {
    bool forceReload = false;
    unsigned int  streamId = CActiveAEDSP::Get().GetActiveStreamId();
    if (m_ActiveStreamId != streamId)
    {
      m_ActiveStreamId      = streamId;
      m_ActiveStreamProcess = CActiveAEDSP::Get().GetDSPProcess(m_ActiveStreamId);
      if (m_ActiveStreamId == (unsigned int)-1 || !m_ActiveStreamProcess)
      {
        Close(true);
        return;
      }
      forceReload = true;
    }

    int               modeUniqueId;
    AE_DSP_BASETYPE   usedBaseType;
    AE_DSP_STREAMTYPE streamTypeUsed;
    m_ActiveStreamProcess->GetMasterModeTypeInformation(streamTypeUsed, usedBaseType, modeUniqueId);
    if (forceReload || m_baseTypeUsed != usedBaseType || m_streamTypeUsed != streamTypeUsed)
    {
      m_baseTypeUsed        = usedBaseType;
      m_streamTypeUsed      = streamTypeUsed;

      /*!
      * Update settings
      */
      int selType = CMediaSettings::Get().GetCurrentAudioSettings().m_MasterStreamTypeSel;
      CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[streamTypeUsed][usedBaseType] = modeUniqueId;
      CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[selType][usedBaseType]        = modeUniqueId;
      CMediaSettings::Get().GetCurrentAudioSettings().m_MasterStreamBase                          = usedBaseType;
      CMediaSettings::Get().GetCurrentAudioSettings().m_MasterStreamType                          = streamTypeUsed;

      FreeControls();
      OnInitWindow();
    }

    // these settings can change on the fly
    CStdString strInfo;
    m_ActiveStreamProcess->GetMasterModeStreamInfoString(strInfo);
    SET_CONTROL_LABEL(CONTROL_ADDON_STREAM_INFO, strInfo);
    SET_CONTROL_LABEL(CONTROL_TYPE_LABEL, g_localizeStrings.Get(m_ActiveStreamProcess->GetDetectedStreamType()+INPUT_TYPE_LABEL_START));

    if (m_CurrentMenu == AUDIO_BUTTON_AUDIO_INFORMATION)
    {
      m_CPUUsage = StringUtils::Format("%.02f %%", m_ActiveStreamProcess->GetCPUUsage());
      UpdateSetting(AUDIO_SETTING_CONTENT_CPU_USAGE);

      for (unsigned int i = 0; i < m_ActiveModes.size(); i++)
      {
        m_ActiveModesCPUUsage[i] = m_ActiveModes[i]->CPUUsage();
        UpdateSetting(AUDIO_SETTINGS_MENUS+i);
      }
    }
    UpdateSetting(AUDIO_SETTINGS_DELAY);
  }

  CGUIDialogSettings::FrameMove();
}

bool CGUIDialogAudioDSPSettings::SupportsAudioFeature(int feature)
{
  for (Features::iterator itr = m_audioCaps.begin(); itr != m_audioCaps.end(); itr++)
  {
    if(*itr == feature || *itr == IPC_AUD_ALL)
      return true;
  }
  return false;
}

void CGUIDialogAudioDSPSettings::AddAddonButtons()
{
  for (unsigned int i = 0; i < m_Menus.size(); i++)
  {
    AE_DSP_ADDON client;
    if (CActiveAEDSP::Get().GetAudioDSPAddon(m_Menus[i].clientId, client))
    {
      if (m_Menus[i].hook.iLocalizedStringId > 0)
        AddButton(AUDIO_SETTINGS_MENUS+i, client->GetString(m_Menus[i].hook.iLocalizedStringId));
    }
  }
}

void CGUIDialogAudioDSPSettings::GetAudioDSPMenus(AE_DSP_MENUHOOK_CAT category, std::vector<MenuHookMember> &menus)
{
  AE_DSP_ADDONMAP clientMap;
  if (CActiveAEDSP::Get().GetEnabledAudioDSPAddons(clientMap) > 0)
  {
    for (AE_DSP_ADDONMAP_ITR itr = clientMap.begin(); itr != clientMap.end(); itr++)
    {
      AE_DSP_MENUHOOKS hooks;
      if (CActiveAEDSP::Get().GetMenuHooks(itr->second->GetID(), category, hooks))
      {
        for (unsigned int i = 0; i < hooks.size(); i++)
        {
          if (!m_ActiveStreamProcess->IsModeActive(hooks[i].category, itr->second->GetID(), hooks[i].iRelevantModeId))
            continue;

          MenuHookMember menu;
          menu.clientId                 = itr->second->GetID();
          menu.hook.category            = hooks[i].category;
          menu.hook.iHookId             = hooks[i].iHookId;
          menu.hook.iLocalizedStringId  = hooks[i].iLocalizedStringId;
          menu.hook.iRelevantModeId     = hooks[i].iRelevantModeId;
          menus.push_back(menu);
        }
      }
    }
  }
}

void CGUIDialogAudioDSPSettings::OpenAudioDSPMenu(AE_DSP_MENUHOOK_CAT category, AE_DSP_ADDON client, unsigned int iHookId, unsigned int iLocalizedStringId)
{
  AE_DSP_MENUHOOK       hook;
  AE_DSP_MENUHOOK_DATA  hookData;

  hook.category           = category;
  hook.iHookId            = iHookId;
  hook.iLocalizedStringId = iLocalizedStringId;
  hookData.category       = category;
  switch (category)
  {
    case AE_DSP_MENUHOOK_PRE_PROCESS:
    case AE_DSP_MENUHOOK_MASTER_PROCESS:
    case AE_DSP_MENUHOOK_RESAMPLE:
    case AE_DSP_MENUHOOK_POST_PROCESS:
      hookData.data.iStreamId = m_ActiveStreamId;
      break;
    default:
      break;
  }

  Close();
  client->CallMenuHook(hook, hookData);

  //Lock graphic context here as it is sometimes called from non rendering threads
  //maybe we should have a critical section per window instead??
  CSingleLock lock(g_graphicsContext);

  if (!g_windowManager.Initialized())
    return; // don't do anything

  m_closing = false;
  m_bModal = true;
  // set running before it's added to the window manager, else the auto-show code
  // could show it as well if we are in a different thread from
  // the main rendering thread (this should really be handled via
  // a thread message though IMO)
  m_active = true;
  g_windowManager.RouteToWindow(this);

  // active this window...
  CGUIMessage msg(GUI_MSG_WINDOW_INIT, 0, 0);
  OnMessage(msg);

  if (!m_windowLoaded)
  {
    m_CurrentMenu = MENU_MAIN;
    Close(true);
  }

  lock.Leave();
}

void CGUIDialogAudioDSPSettings::UpdateModeIcons()
{
  for (unsigned int i = 0; i < m_MasterModes[m_streamTypeUsed].size(); i++)
  {
    if (CMediaSettings::Get().GetCurrentAudioSettings().m_MasterModes[m_streamTypeUsed][m_baseTypeUsed] == m_MasterModes[m_streamTypeUsed][i]->ModeID())
    {
      if (m_MasterModes[m_streamTypeUsed][i]->IconOwnModePath() != "")
      {
        CGUIImage *pImage = (CGUIImage *)GetControl(CONTROL_ADDON_MODE_IMAGE);
        if (pImage)
        {
          pImage->SetFileName(m_MasterModes[m_streamTypeUsed][i]->IconOwnModePath());
          SET_CONTROL_VISIBLE(CONTROL_ADDON_MODE_IMAGE);
        }
      }
      else
        SET_CONTROL_HIDDEN(CONTROL_ADDON_MODE_IMAGE);

      if (m_MasterModes[m_streamTypeUsed][i]->IconOverrideModePath() != "")
      {
        CGUIImage *pImage = (CGUIImage *)GetControl(CONTROL_OVERRIDE_IMAGE);
        if (pImage)
        {
          pImage->SetFileName(m_MasterModes[m_streamTypeUsed][i]->IconOverrideModePath());
          SET_CONTROL_VISIBLE(CONTROL_OVERRIDE_IMAGE);
        }
      }
      else
        SET_CONTROL_HIDDEN(CONTROL_OVERRIDE_IMAGE);
      break;
    }
  }
}
