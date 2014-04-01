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

#include "ActiveAEDSP.h"
#include "ActiveAEDSPProcess.h"

#include "cores/AudioEngine/Engines/ActiveAE/ActiveAEBuffer.h"
#include "cores/AudioEngine/Engines/ActiveAE/ActiveAEResample.h"
#include "cores/IPlayer.h"
#include "cores/AudioEngine/Utils/AEUtil.h"

#include "Application.h"
#include "ApplicationMessenger.h"
#include "GUIUserMessages.h"
#include "addons/GUIDialogAddonSettings.h"
#include "dialogs/GUIDialogOK.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIWindowManager.h"
#include "settings/AdvancedSettings.h"
#include "settings/MediaSettings.h"
#include "settings/MediaSourceSettings.h"
#include "settings/Settings.h"
#include "settings/dialogs/GUIDialogAudioDSPManager.h"
#include "utils/StringUtils.h"
#include "utils/JobManager.h"

using namespace std;
using namespace ADDON;
using namespace ActiveAE;

#define MIN_DSP_ARRAY_SIZE 4096

/*! @name Master audio dsp control class */
//@{
CActiveAEDSP::CActiveAEDSP()
  : CThread("ActiveAEDSP")
  , m_isActive(false)
  , m_bNoAddonWarningDisplayed(false)
  , m_usedProcessesCnt(0)
  , m_activeProcessId(-1)
  , m_bIsValidAudioDSPSettings(false)
{
  Cleanup();
}

CActiveAEDSP::~CActiveAEDSP()
{
  /* Deactivate all present dsp addons */
  Deactivate();
  CLog::Log(LOGDEBUG, "ActiveAE DSP - destroyed");
}

CActiveAEDSP &CActiveAEDSP::Get(void)
{
  static CActiveAEDSP activeAEDSPManagerInstance;
  return activeAEDSPManagerInstance;
}
//@}

/*! @name initialization and configuration methods */
//@{
class CActiveAEDSPStartJob : public CJob
{
public:
  CActiveAEDSPStartJob() {}
  ~CActiveAEDSPStartJob(void) {}

  bool DoWork(void)
  {
    CActiveAEDSP::Get().Activate(false);
    return true;
  }
};

void CActiveAEDSP::Activate(bool bAsync /* = false */)
{
  if (bAsync)
  {
    CActiveAEDSPStartJob *job = new CActiveAEDSPStartJob();
    CJobManager::GetInstance().AddJob(job, NULL);
    return;
  }

  CSingleLock lock(m_critSection);

  /* first stop and remove any audio dsp add-on's */
  Deactivate();

  CLog::Log(LOGNOTICE, "ActiveAE DSP - starting");

  /* don't start if Settings->System->Audio->Audio DSP isn't checked */
  if (!CSettings::Get().GetBool("audiooutput.dspaddonsenabled"))
    return;

  Cleanup();

  /* create and open database */
  m_DatabaseDSP.Open();
  m_DatabaseAddon.Open();

  Create();
  SetPriority(-1);

//  TriggerModeUpdate(false);
}

class CActiveAEDSPModeUpdateJob : public CJob
{
public:
  CActiveAEDSPModeUpdateJob() {}
  ~CActiveAEDSPModeUpdateJob(void) {}

  bool DoWork(void)
  {
    CActiveAEDSP::Get().TriggerModeUpdate(false);
    return true;
  }
};

void CActiveAEDSP::TriggerModeUpdate(bool bAsync /* = true */)
{
  if (bAsync)
  {
    CActiveAEDSPModeUpdateJob *job = new CActiveAEDSPModeUpdateJob();
    CJobManager::GetInstance().AddJob(job, NULL);
    return;
  }

  CLog::Log(LOGINFO, "ActiveAE DSP - %s - Update mode selections", __FUNCTION__);

  if (!m_DatabaseDSP.IsOpen())
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - failed to open the database");
    return;
  }

  for (unsigned int i = 0; i < AE_DSP_MODE_TYPE_MAX; i++)
  {
    m_Modes[i].clear();
    m_DatabaseDSP.GetModes(m_Modes[i], i);
  }
}

void CActiveAEDSP::Deactivate(void)
{
  /* check whether the audio dsp is loaded */
  if (!m_isActive)
    return;

  CLog::Log(LOGNOTICE, "ActiveAE DSP - stopping");

  CSingleLock lock(m_critSection);

  /*
   * if any dsp processing is active restart playback with this dsp
   * disabled (m_isActive = false).
   */
  if (m_usedProcessesCnt > 0)
  {
    CLog::Log(LOGNOTICE, "ActiveAE DSP - restarting playback without dsp");
    CApplicationMessenger::Get().MediaRestart(true);
  }

  /* stop thread */
  StopThread();

  /* unload all data */
  Cleanup();

  /* close databases */
  if (m_DatabaseAddon.IsOpen())
    m_DatabaseAddon.Close();
  if (m_DatabaseDSP.IsOpen())
    m_DatabaseDSP.Close();

  /* destroy all addons */
  for (AE_DSP_ADDONMAP_ITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
    itr->second->Destroy();

  m_AddonMap.clear();
}

void CActiveAEDSP::Cleanup(void)
{
  CActiveAEDSPProcessPtr tmp;
  for (int i = 0; i < AE_DSP_STREAM_MAX_STREAMS; i++)
    m_usedProcesses[i] = tmp;

  m_isActive                  = false;
  m_usedProcessesCnt          = 0;
  m_bIsValidAudioDSPSettings  = false;

  for (unsigned int i = 0; i < AE_DSP_MODE_TYPE_MAX; i++)
    m_Modes[i].clear();
}

bool CActiveAEDSP::InstallAddonAllowed(const std::string &strAddonId) const
{
  return !m_isActive ||
         !IsInUse(strAddonId) ||
         m_usedProcessesCnt == 0;
}

void CActiveAEDSP::ResetDatabase(void)
{
  CLog::Log(LOGNOTICE, "ActiveAE DSP - clearing the audio DSP database");

  if (IsProcessing())
  {
    CLog::Log(LOGNOTICE, "ActiveAE DSP - stopping playback");
    CApplicationMessenger::Get().MediaStop();
  }

  /* stop the thread */
  Deactivate();

  if (m_DatabaseDSP.Open())
  {
    m_DatabaseDSP.DeleteModes();
    m_DatabaseDSP.DeleteActiveDSPSettings();
    m_DatabaseDSP.DeleteAddons();

    m_DatabaseDSP.Close();
  }

  CLog::Log(LOGNOTICE, "ActiveAE DSP - database cleared");

  if (CSettings::Get().GetBool("audiooutput.dspaddonsenabled"))
  {
    CLog::Log(LOGNOTICE, "ActiveAE DSP - restarting the audio DSP handler");
    m_DatabaseDSP.Open();
    Cleanup();
    Activate();
  }
}
//@}

/*! @name Settings and action callback methods (OnAction currently unused */
//@{
void CActiveAEDSP::OnSettingChanged(const CSetting *setting)
{
  if (setting == NULL)
    return;

  const std::string &settingId = setting->GetId();
  if (settingId == "audiooutput.dspaddonsenabled")
  {
    if (((CSettingBool *) setting)->GetValue())
      CApplicationMessenger::Get().ExecBuiltIn("XBMC.StartAudioDSPEngine", false);
    else
      CApplicationMessenger::Get().ExecBuiltIn("XBMC.StopAudioDSPEngine", false);
  }
}

void CActiveAEDSP::OnSettingAction(const CSetting *setting)
{
  if (setting == NULL)
    return;

  const std::string &settingId = setting->GetId();
  if (settingId == "audiooutput.dspsettings")
  {
    if (IsActivated())
    {
      CGUIDialogAudioDSPManager *dialog = (CGUIDialogAudioDSPManager *)g_windowManager.GetWindow(WINDOW_DIALOG_AUDIO_DSP_MANAGER);
      if (dialog)
        dialog->DoModal();
    }
  }
  else if (settingId == "audiooutput.dspresetdb")
  {
    if (CGUIDialogYesNo::ShowAndGetInput(19098, 36432, 750, 0))
    {
      CDateTime::ResetTimezoneBias();
      ResetDatabase();
    }
  }
}
//@}

/*! @name addon installation callback methods */
//@{
bool CActiveAEDSP::RequestRestart(AddonPtr addon, bool bDataChanged)
{
  return StopAudioDSPAddon(addon, true);
}

bool CActiveAEDSP::RequestRemoval(AddonPtr addon)
{
  /* During playback is not recommend for me to change the used addons (need to fix) */
  if (IsInUse(addon->ID()) && IsProcessing())
  {
    CGUIDialogOK::ShowAndGetInput(24077, 24078, 24079, 0);
    return false;
  }

  StopAudioDSPAddon(addon, false);
  return true;
}

bool CActiveAEDSP::IsInUse(const std::string &strAddonId) const
{
  CSingleLock lock(m_critSection);

  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
    if (itr->second->Enabled() && itr->second->ID().Equals(strAddonId.c_str()))
      return true;
  return false;
}

bool CActiveAEDSP::IsKnownAudioDSPAddon(const AddonPtr addon) const
{
  // database IDs start at 1
  return GetAudioDSPAddonId(addon) > 0;
}

int CActiveAEDSP::GetAudioDSPAddonId(const AddonPtr addon) const
{
  CSingleLock lock(m_critUpdateSection);

  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
    if (itr->second->ID() == addon->ID())
      return itr->first;

  return -1;
}
//@}

/*! @name Current processing streams control function methods */
//@{
bool CActiveAEDSP::CreateDSPs(unsigned int &streamId, CActiveAEDSPProcessPtr &process, AEAudioFormat inputFormat, AEAudioFormat outputFormat, bool upmix, AEQuality quality, bool wasActive)
{
  if (inputFormat.m_dataFormat != AE_FMT_FLOAT)
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - %s - dsp processing with input data format '%s' not supported!", __FUNCTION__, CAEUtil::DataFormatToStr(inputFormat.m_dataFormat));
    return false;
  }

  if (!IsActivated() || m_usedProcessesCnt >= AE_DSP_STREAM_MAX_STREAMS)
    return false;

  CSingleLock lock(m_critSection);

  AE_DSP_STREAMTYPE requestedStreamType = LoadCurrentAudioSettings();

  CActiveAEDSPProcessPtr usedProc;
  if (wasActive && streamId < AE_DSP_STREAM_MAX_STREAMS)
  {
    if (m_usedProcesses[streamId] != NULL)
    {
      usedProc = m_usedProcesses[streamId];
    }
  }
  else
  {
    for (int i = 0; i < AE_DSP_STREAM_MAX_STREAMS; i++)
    {
      /* find a free position */
      if (m_usedProcesses[i] == NULL)
      {
        usedProc = CActiveAEDSPProcessPtr(new CActiveAEDSPProcess(i));
        streamId = i;
        break;
      }
    }
  }

  if (usedProc == NULL)
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - %s - can't find active processing class", __FUNCTION__);
    return false;
  }

  if (!usedProc->Create(inputFormat, outputFormat, upmix, quality, requestedStreamType))
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - %s - Creation of processing class failed", __FUNCTION__);
    return false;
  }

  if (!wasActive)
  {
    process = usedProc;
    m_activeProcessId = streamId;
    m_usedProcesses[streamId] = usedProc;
    m_usedProcessesCnt++;
  }
  return true;
}

void CActiveAEDSP::DestroyDSPs(unsigned int streamId)
{
  CSingleLock lock(m_critSection);

  CActiveAEDSPProcessPtr emptyProc;
  if (m_usedProcesses[streamId] != NULL)
  {
    m_usedProcesses[streamId] = emptyProc;
    m_usedProcessesCnt--;
  }
  if (m_usedProcessesCnt == 0)
  {
    m_activeProcessId = -1;
  }
}

CActiveAEDSPProcessPtr CActiveAEDSP::GetDSPProcess(unsigned int streamId)
{
  CSingleLock lock(m_critSection);

  CActiveAEDSPProcessPtr emptyProc;

  if (m_usedProcesses[streamId])
    return m_usedProcesses[streamId];
  return emptyProc;
}

unsigned int CActiveAEDSP::GetProcessingStreamsAmount(void)
{
  CSingleLock lock(m_critSection);
  return m_usedProcessesCnt;
}

unsigned int CActiveAEDSP::GetActiveStreamId(void)
{
  CSingleLock lock(m_critSection);

  return m_activeProcessId;
}

const AE_DSP_MODELIST &CActiveAEDSP::GetAvailableModes(AE_DSP_MODE_TYPE modeType)
{
  static AE_DSP_MODELIST emptyArray;
  if (modeType < 0 || modeType >= AE_DSP_MODE_TYPE_MAX)
    return emptyArray;

  CSingleLock lock(m_critSection);
  return m_Modes[modeType];
}

/*! @name addon update process methods */
//@{
bool CActiveAEDSP::StopAudioDSPAddon(AddonPtr addon, bool bRestart)
{
  CSingleLock lock(m_critSection);

  int iId = GetAudioDSPAddonId(addon);
  AE_DSP_ADDON mappedAddon;
  if (GetReadyAudioDSPAddon(iId, mappedAddon))
  {
    if (bRestart)
      mappedAddon->ReCreate();
    else
      mappedAddon->Destroy();

    return true;
  }

  return false;
}

bool CActiveAEDSP::UpdateAndInitialiseAudioDSPAddons(bool bInitialiseAllAudioDSPAddons /* = false */)
{
  bool bReturn(true);
  VECADDONS map;
  VECADDONS disableAddons;
  {
    CSingleLock lock(m_critUpdateSection);
    map = m_Addons;
  }

  if (map.size() == 0)
    return false;

  for (unsigned iAddonPtr = 0; iAddonPtr < map.size(); iAddonPtr++)
  {
    const AddonPtr dspAddon = map.at(iAddonPtr);
    bool bEnabled = dspAddon->Enabled() &&
                    !CAddonMgr::Get().IsAddonDisabled(dspAddon->ID());

    if (!bEnabled && IsKnownAudioDSPAddon(dspAddon))
    {
      CSingleLock lock(m_critUpdateSection);
      /* stop the dsp addon and remove it from the db */
      StopAudioDSPAddon(dspAddon, false);
      VECADDONS::iterator addonPtr = std::find(m_Addons.begin(), m_Addons.end(), dspAddon);
      if (addonPtr != m_Addons.end())
        m_Addons.erase(addonPtr);

    }
    else if (bEnabled && (bInitialiseAllAudioDSPAddons || !IsKnownAudioDSPAddon(dspAddon) || !IsReadyAudioDSPAddon(dspAddon)))
    {
      bool bDisabled(false);

      /* register the add-on in the audio dsp db, and create the CActiveAEDSPAddon instance */
      int iAddonId = RegisterAudioDSPAddon(dspAddon);
      if (iAddonId < 0)
      {
        /* failed to register or create the add-on, disable it */
        CLog::Log(LOGWARNING, "ActiveAE DSP - %s - failed to register add-on %s, disabling it", __FUNCTION__, dspAddon->Name().c_str());
        disableAddons.push_back(dspAddon);
        bDisabled = true;
      }
      else
      {
        ADDON_STATUS status(ADDON_STATUS_UNKNOWN);
        AE_DSP_ADDON addon;
        {
          CSingleLock lock(m_critUpdateSection);
          if (!GetAudioDSPAddon(iAddonId, addon))
          {
            CLog::Log(LOGWARNING, "ActiveAE DSP - %s - failed to find add-on %s, disabling it", __FUNCTION__, dspAddon->Name().c_str());
            disableAddons.push_back(dspAddon);
            bDisabled = true;
          }
        }

        /* re-check the enabled status. newly installed dsps get disabled when they're added to the db */
        if (!bDisabled && addon->Enabled() && (status = addon->Create(iAddonId)) != ADDON_STATUS_OK)
        {
          CLog::Log(LOGWARNING, "ActiveAE DSP - %s - failed to create add-on %s, status = %d", __FUNCTION__, dspAddon->Name().c_str(), status);
          if (!addon.get() || !addon->DllLoaded() || status == ADDON_STATUS_PERMANENT_FAILURE)
          {
            /* failed to load the dll of this add-on, disable it */
            CLog::Log(LOGWARNING, "ActiveAE DSP - %s - failed to load the dll for add-on %s, disabling it", __FUNCTION__, dspAddon->Name().c_str());
            disableAddons.push_back(dspAddon);
            bDisabled = true;
          }
        }
      }

      if (bDisabled && IsActivated())
        CGUIDialogOK::ShowAndGetInput(24070, 24071, 16029, 0);
    }
  }

  /* disable add-ons that failed to initialise */
  if (disableAddons.size() > 0)
  {
    CSingleLock lock(m_critUpdateSection);
    for (VECADDONS::iterator it = disableAddons.begin(); it != disableAddons.end(); it++)
    {
      /* disable in the add-on db */
      CAddonMgr::Get().DisableAddon((*it)->ID(), true);

      /* remove from the audio dsp add-on list */
      VECADDONS::iterator addonPtr = std::find(m_Addons.begin(), m_Addons.end(), *it);
      if (addonPtr != m_Addons.end())
        m_Addons.erase(addonPtr);
    }
  }

  return bReturn;
}

bool CActiveAEDSP::UpdateAddons(void)
{
  VECADDONS addons;
  bool bReturn(CAddonMgr::Get().GetAddons(ADDON_ADSPDLL, addons, true));

  if (bReturn)
  {
    CSingleLock lock(m_critUpdateSection);
    m_Addons = addons;
  }

  /* handle "new" addons which aren't yet in the db - these have to be added first */
  for (unsigned iAddonPtr = 0; iAddonPtr < m_Addons.size(); iAddonPtr++)
  {
    const AddonPtr dspAddon = m_Addons.at(iAddonPtr);

    if (!m_DatabaseAddon.HasAddon(dspAddon->ID()))
    {
      m_DatabaseAddon.AddAddon(dspAddon, -1);
    }
  }

  if ((!bReturn || addons.size() == 0) && !m_bNoAddonWarningDisplayed &&
      !CAddonMgr::Get().HasAddons(ADDON_ADSPDLL, false) &&
      IsActivated())
  {
    m_bNoAddonWarningDisplayed = true;
    CSettings::Get().SetBool("audiooutput.dspaddonsenabled", false);
    CGUIDialogOK::ShowAndGetInput(24055, 24056, 24057, 24058);
    CGUIMessage msg(GUI_MSG_UPDATE, WINDOW_SETTINGS_SYSTEM, 0);
    g_windowManager.SendThreadMessage(msg, WINDOW_SETTINGS_SYSTEM);
  }

  return bReturn;
}

void CActiveAEDSP::Notify(const Observable &obs, const ObservableMessage msg)
{
  if (msg == ObservableMessageAddons)
    UpdateAddons();
}

void CActiveAEDSP::Process(void)
{
  bool bCheckedEnabledAddonsOnStartup(false);

  CAddonMgr::Get().RegisterAddonMgrCallback(ADDON_ADSPDLL, this);
  CAddonMgr::Get().RegisterObserver(this);

  UpdateAddons();

  m_isActive = true;

  while (!g_application.m_bStop && !m_bStop)
  {
    UpdateAndInitialiseAudioDSPAddons();

    if (!bCheckedEnabledAddonsOnStartup)
    {
      bCheckedEnabledAddonsOnStartup = true;
      if (HasEnabledAudioDSPAddons())
        TriggerModeUpdate(false);
      else if (!m_bNoAddonWarningDisplayed)
        ShowDialogNoAddonsEnabled();
    }

    Sleep(1000);
  }

  m_isActive = false;
}

void CActiveAEDSP::ShowDialogNoAddonsEnabled(void)
{
  if (!IsActivated())
    return;

  CGUIDialogOK::ShowAndGetInput(15048, 15049, 15050, 15051);

  vector<CStdString> params;
  params.push_back("addons://disabled/xbmc.adspaddon");
  params.push_back("return");
  g_windowManager.ActivateWindow(WINDOW_ADDON_BROWSER, params);
}

int CActiveAEDSP::RegisterAudioDSPAddon(AddonPtr addon)
{
  int iAddonId(-1);
  if (!addon->Enabled())
    return -1;

  CLog::Log(LOGDEBUG, "ActiveAE DSP - %s - registering add-on '%s'", __FUNCTION__, addon->Name().c_str());

  if (!m_DatabaseDSP.IsOpen())
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - %s - failed to get the database", __FUNCTION__);
    return -1;
  }

  /* check whether we already know this dsp addon */
  iAddonId = m_DatabaseDSP.GetAudioDSPAddonId(addon->ID());

  /* try to register the new dsp addon in the db */
  if (iAddonId < 0 && (iAddonId = m_DatabaseDSP.Persist(addon)) < 0)
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - %s - can't add dsp addon '%s' to the database", __FUNCTION__, addon->Name().c_str());
    return -1;
  }

  AE_DSP_ADDON dspAddon;
  /* load and initialise the dsp addon libraries */
  {
    CSingleLock lock(m_critSection);
    AE_DSP_ADDONMAP_CITR existingAddon = m_AddonMap.find(iAddonId);
    if (existingAddon != m_AddonMap.end())
    {
      /* return existing addon */
      dspAddon = existingAddon->second;
    }
    else
    {
      /* create a new addon instance */
      dspAddon = boost::dynamic_pointer_cast<CActiveAEDSPAddon> (addon);
      m_AddonMap.insert(std::make_pair(iAddonId, dspAddon));
    }
  }

  if (iAddonId < 0)
    CLog::Log(LOGERROR, "ActiveAE DSP - %s - can't register dsp add-on '%s'", __FUNCTION__, addon->Name().c_str());

  return iAddonId;
}
//@}

/*! @name Played source settings methods */
//@{
void CActiveAEDSP::SaveCurrentAudioSettings(void)
{
  CSingleLock lock(m_critSection);

  if (!IsProcessing() || !m_bIsValidAudioDSPSettings)
    return;

  CFileItem currentFile(g_application.CurrentFileItem());

  if (CMediaSettings::Get().GetCurrentAudioSettings() != CMediaSettings::Get().GetDefaultAudioSettings())
  {
    CLog::Log(LOGDEBUG, "ActiveAE DSP - %s - persisting custom audio settings for file '%s'", __FUNCTION__, currentFile.GetPath().c_str());
    m_DatabaseDSP.SetActiveDSPSettings(&currentFile, CMediaSettings::Get().GetCurrentAudioSettings());
  }
  else
  {
    CLog::Log(LOGDEBUG, "ActiveAE DSP - %s - no custom audio settings for file '%s'", __FUNCTION__, currentFile.GetPath().c_str());
    m_DatabaseDSP.DeleteActiveDSPSettings(&currentFile);
  }
}

AE_DSP_STREAMTYPE CActiveAEDSP::LoadCurrentAudioSettings(void)
{
  CSingleLock lock(m_critSection);

  AE_DSP_STREAMTYPE type = AE_DSP_ASTREAM_INVALID;

  if (g_application.m_pPlayer->HasPlayer())
  {
    CFileItem currentFile(g_application.CurrentFileItem());

    /* load the persisted audio settings and set them as current */
    CAudioSettings loadedAudioSettings = CMediaSettings::Get().GetDefaultAudioSettings();
    m_DatabaseDSP.GetActiveDSPSettings(&currentFile, loadedAudioSettings);

    CMediaSettings::Get().GetCurrentAudioSettings() = loadedAudioSettings;
    type = (AE_DSP_STREAMTYPE) loadedAudioSettings.m_MasterStreamTypeSel;

    /* settings can be saved on next audio stream change */
    m_bIsValidAudioDSPSettings = true;
  }
  return type;
}
//@}

/*! @name Backend methods */
//@{

bool CActiveAEDSP::OnAction(const CAction &action)
{
  return false;
}

bool CActiveAEDSP::IsProcessing(void) const
{
  return m_isActive && m_usedProcessesCnt > 0;
}

bool CActiveAEDSP::IsActivated(void) const
{
  return m_isActive;
}

int CActiveAEDSP::EnabledAudioDSPAddonAmount(void) const
{
  int iReturn(0);
  CSingleLock lock(m_critUpdateSection);

  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
    if (itr->second->Enabled())
      ++iReturn;

  return iReturn;
}

bool CActiveAEDSP::HasEnabledAudioDSPAddons(void) const
{
  return EnabledAudioDSPAddonAmount() > 0;
}

int CActiveAEDSP::GetEnabledAudioDSPAddons(AE_DSP_ADDONMAP &addons) const
{
  int iReturn(0);
  CSingleLock lock(m_critUpdateSection);

  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
  {
    if (itr->second->Enabled() && itr->second->GetAudioDSPFunctionStruct())
    {
      addons.insert(std::make_pair(itr->second->GetID(), itr->second));
      ++iReturn;
    }
  }

  return iReturn;
}

int CActiveAEDSP::ReadyAudioDSPAddonAmount(void) const
{
  int iReturn(0);
  CSingleLock lock(m_critUpdateSection);

  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
    if (itr->second->ReadyToUse())
      ++iReturn;

  return iReturn;
}

bool CActiveAEDSP::HasReadyAudioDSPAddons(void) const
{
  return ReadyAudioDSPAddonAmount() > 0;
}

bool CActiveAEDSP::IsReadyAudioDSPAddon(int iAddonId) const
{
  AE_DSP_ADDON addon;
  return GetReadyAudioDSPAddon(iAddonId, addon);
}

bool CActiveAEDSP::IsReadyAudioDSPAddon(const AddonPtr addon)
{
  CSingleLock lock(m_critUpdateSection);

  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
    if (itr->second->ID() == addon->ID())
      return itr->second->ReadyToUse();
  return false;
}

int CActiveAEDSP::GetReadyAddons(AE_DSP_ADDONMAP &addons) const
{
  int iReturn(0);
  CSingleLock lock(m_critSection);

  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
  {
    if (itr->second->ReadyToUse())
    {
      addons.insert(std::make_pair(itr->second->GetID(), itr->second));
      ++iReturn;
    }
  }

  return iReturn;
}

bool CActiveAEDSP::GetReadyAudioDSPAddon(int iAddonId, AE_DSP_ADDON &addon) const
{
  if (GetAudioDSPAddon(iAddonId, addon))
    return addon->ReadyToUse();
  return false;
}

bool CActiveAEDSP::GetAudioDSPAddonName(int iAddonId, CStdString &strName) const
{
  bool bReturn(false);
  AE_DSP_ADDON addon;
  if ((bReturn = GetReadyAudioDSPAddon(iAddonId, addon)) == true)
    strName = addon->GetAudioDSPName();

  return bReturn;
}

bool CActiveAEDSP::GetAudioDSPAddon(int iAddonId, AE_DSP_ADDON &addon) const
{
  bool bReturn(false);
  if (iAddonId <= AE_DSP_INVALID_ADDON_ID)
    return bReturn;

  CSingleLock lock(m_critUpdateSection);

  AE_DSP_ADDONMAP_CITR itr = m_AddonMap.find(iAddonId);
  if (itr != m_AddonMap.end())
  {
    addon = itr->second;
    bReturn = true;
  }

  return bReturn;
}

bool CActiveAEDSP::GetAudioDSPAddon(const CStdString &strId, AddonPtr &addon) const
{
  CSingleLock lock(m_critUpdateSection);
  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
  {
    if (itr->second->ID() == strId)
    {
      addon = itr->second;
      return true;
    }
  }
  return false;
}
//@}

/*! @name Menu hook methods */
//@{
bool CActiveAEDSP::HaveMenuHooks(AE_DSP_MENUHOOK_CAT cat, int iDSPAddonID)
{
  for (AE_DSP_ADDONMAP_CITR itr = m_AddonMap.begin(); itr != m_AddonMap.end(); itr++)
  {
    if (itr->second->ReadyToUse())
    {
      if (itr->second->HaveMenuHooks(cat))
      {
        if (iDSPAddonID > 0 && itr->second->GetID() == iDSPAddonID)
          return true;
        else if (iDSPAddonID < 0)
          return true;
      }
      else if (cat == AE_DSP_MENUHOOK_SETTING)
      {
        AddonPtr addon;
        if (CAddonMgr::Get().GetAddon(itr->second->ID(), addon) && addon->HasSettings())
          return true;
      }
    }
  }

  return false;
}

bool CActiveAEDSP::GetMenuHooks(int iDSPAddonID, AE_DSP_MENUHOOK_CAT cat, AE_DSP_MENUHOOKS &hooks)
{
  bool bReturn(false);

  if (iDSPAddonID < 0)
    return bReturn;

  AE_DSP_ADDON addon;
  if (GetReadyAudioDSPAddon(iDSPAddonID, addon) && addon->HaveMenuHooks(cat))
  {
    AE_DSP_MENUHOOKS *addonhooks = addon->GetMenuHooks();
    for (unsigned int i = 0; i < addonhooks->size(); i++)
    {
      if (cat == AE_DSP_MENUHOOK_ALL || addonhooks->at(i).category == cat)
      {
        hooks.push_back(addonhooks->at(i));
        bReturn = true;
      }
    }
  }

  return bReturn;
}

void CActiveAEDSP::ProcessMenuHooks(int iDSPAddonID, AE_DSP_MENUHOOK_CAT cat)
{
  AE_DSP_MENUHOOKS *hooks = NULL;

  /* get aduio dsp addon id */
  if (iDSPAddonID < 0 && cat == AE_DSP_MENUHOOK_SETTING)
  {
    AE_DSP_ADDONMAP addons;
    GetReadyAddons(addons);

    if (addons.size() == 1)
    {
      iDSPAddonID = addons.begin()->first;
    }
    else if (addons.size() > 1)
    {
      /* have user select addon */
      CGUIDialogSelect *pDialog = (CGUIDialogSelect *) g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);
      pDialog->Reset();
      pDialog->SetHeading(15040);

      AE_DSP_ADDONMAP_CITR itrAddons;
      for (itrAddons = addons.begin(); itrAddons != addons.end(); itrAddons++)
      {
        pDialog->Add(itrAddons->second->GetAudioDSPName());
      }
      pDialog->DoModal();

      int selection = pDialog->GetSelectedLabel();
      if (selection >= 0)
      {
        itrAddons = addons.begin();
        for (int i = 0; i < selection; i++)
          itrAddons++;
        iDSPAddonID = itrAddons->first;
      }
    }
  }

  if (iDSPAddonID < 0)
    return;

  AE_DSP_ADDON dspAddon;
  GetReadyAudioDSPAddon(iDSPAddonID, dspAddon);

  if (GetReadyAudioDSPAddon(iDSPAddonID, dspAddon) && HaveMenuHooks(cat, iDSPAddonID))
  {
    hooks = dspAddon->GetMenuHooks();
    std::vector<int> hookIDs;

    CGUIDialogSelect *pDialog = (CGUIDialogSelect *) g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);
    pDialog->Reset();
    pDialog->SetHeading(15040);
    unsigned int id;
    for (id = 0; id < hooks->size(); id++)
    {
      if (hooks->at(id).category == cat || hooks->at(id).category == AE_DSP_MENUHOOK_ALL)
      {
        pDialog->Add(dspAddon->GetString(hooks->at(id).iLocalizedStringId));
        hookIDs.push_back(id);
      }
    }

    int addOnListId = -1;
    if (cat == AE_DSP_MENUHOOK_SETTING && dspAddon->HasSettings())
    {
      pDialog->Add(g_localizeStrings.Get(1045));
      hookIDs.push_back(++id);
      addOnListId = id;
    }

    pDialog->DoModal();

    int selection = pDialog->GetSelectedLabel();
    if (selection >= 0)
    {
      if (addOnListId >= 0 && hookIDs.at(selection) == addOnListId)
        CGUIDialogAddonSettings::ShowAndGetInput(dspAddon);
      else
      {

        AE_DSP_MENUHOOK_DATA hookData;
        hookData.category  = hooks->at(hookIDs.at(selection)).category;
        hookData.data.iStreamId = -1;
        dspAddon->CallMenuHook(hooks->at(hookIDs.at(selection)), hookData);
      }
    }
  }
}
//@}

