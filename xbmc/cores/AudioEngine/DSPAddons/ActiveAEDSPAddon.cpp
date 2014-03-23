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

#include <vector>
#include "Application.h"
#include "ActiveAEDSPAddon.h"
#include "settings/AdvancedSettings.h"
#include "settings/Settings.h"
#include "utils/log.h"
#include "utils/StringUtils.h"

using namespace std;
using namespace ADDON;
using namespace ActiveAE;

#define DEFAULT_INFO_STRING_VALUE "unknown"

CActiveAEDSPAddon::CActiveAEDSPAddon(const AddonProps& props) :
    CAddonDll<DllAudioDSP, AudioDSP, AE_DSP_PROPERTIES>(props),
    m_apiVersion("0.0.0")
{
  ResetProperties();
}

CActiveAEDSPAddon::CActiveAEDSPAddon(const cp_extension_t *ext) :
    CAddonDll<DllAudioDSP, AudioDSP, AE_DSP_PROPERTIES>(ext),
    m_apiVersion("0.0.0")
{
  ResetProperties();
}

CActiveAEDSPAddon::~CActiveAEDSPAddon(void)
{
  Destroy();
  SAFE_DELETE(m_pInfo);
}

void CActiveAEDSPAddon::ResetProperties(int iClientId /* = AE_DSP_INVALID_ADDON_ID */)
{
  /* initialise members */
  SAFE_DELETE(m_pInfo);
  m_pInfo = new AE_DSP_PROPERTIES;
  m_strUserPath           = CSpecialProtocol::TranslatePath(Profile());
  m_pInfo->strUserPath    = m_strUserPath.c_str();
  m_strAddonPath          = CSpecialProtocol::TranslatePath(Path());
  m_pInfo->strAddonPath   = m_strAddonPath.c_str();
  m_menuhooks.clear();
  m_bReadyToUse           = false;
  m_bIsInUse              = false;
  m_iClientId             = iClientId;
  m_strAudioDSPVersion    = DEFAULT_INFO_STRING_VALUE;
  m_strFriendlyName       = DEFAULT_INFO_STRING_VALUE;
  m_strAudioDSPName       = DEFAULT_INFO_STRING_VALUE;
  m_bIsPreProcessing      = false;
  m_bIsPreResampling      = false;
  m_bIsMasterProcessing   = false;
  m_bIsPostResampling     = false;
  m_bIsPostProcessing     = false;
  memset(&m_addonCapabilities, 0, sizeof(m_addonCapabilities));
  m_apiVersion = AddonVersion("0.0.0");
}

ADDON_STATUS CActiveAEDSPAddon::Create(int iClientId)
{
  ADDON_STATUS status(ADDON_STATUS_UNKNOWN);
  if (iClientId <= AE_DSP_INVALID_ADDON_ID)
    return status;

  /* ensure that a previous instance is destroyed */
  Destroy();

  /* reset all properties to defaults */
  ResetProperties(iClientId);

  /* initialise the add-on */
  bool bReadyToUse(false);
  CLog::Log(LOGDEBUG, "ActiveAE DSP - %s - creating audio dsp add-on instance '%s'", __FUNCTION__, Name().c_str());
  try
  {
    if ((status = CAddonDll<DllAudioDSP, AudioDSP, AE_DSP_PROPERTIES>::Create()) == ADDON_STATUS_OK)
      bReadyToUse = GetAddonProperties();
  }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  m_bReadyToUse = bReadyToUse;

  return status;
}

bool CActiveAEDSPAddon::DllLoaded(void) const
{
  try { return CAddonDll<DllAudioDSP, AudioDSP, AE_DSP_PROPERTIES>::DllLoaded(); }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  return false;
}

void CActiveAEDSPAddon::Destroy(void)
{
  /* reset 'ready to use' to false */
  if (!m_bReadyToUse)
    return;
  m_bReadyToUse = false;

  CLog::Log(LOGDEBUG, "ActiveAE DSP - %s - destroying audio dsp add-on '%s'", __FUNCTION__, GetFriendlyName().c_str());

  /* destroy the add-on */
  try { CAddonDll<DllAudioDSP, AudioDSP, AE_DSP_PROPERTIES>::Destroy(); }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  /* reset all properties to defaults */
  ResetProperties();
}

void CActiveAEDSPAddon::ReCreate(void)
{
  int iClientID(m_iClientId);
  Destroy();

  /* recreate the instance */
  Create(iClientID);
}

bool CActiveAEDSPAddon::ReadyToUse(void) const
{
  return m_bReadyToUse;
}

int CActiveAEDSPAddon::GetID(void) const
{
  return m_iClientId;
}

bool CActiveAEDSPAddon::IsInUse() const
{
  return m_bIsInUse;
};

AudioDSP *CActiveAEDSPAddon::GetAudioDSPFunctionStruct()
{
  return m_pStruct;
}

bool CActiveAEDSPAddon::IsCompatibleAPIVersion(const ADDON::AddonVersion &minVersion, const ADDON::AddonVersion &version)
{
  AddonVersion myMinVersion = AddonVersion(XBMC_AE_DSP_MIN_API_VERSION);
  AddonVersion myVersion = AddonVersion(XBMC_AE_DSP_API_VERSION);
  return (version >= myMinVersion && minVersion <= myVersion);
}

bool CActiveAEDSPAddon::IsCompatibleGUIAPIVersion(const ADDON::AddonVersion &minVersion, const ADDON::AddonVersion &version)
{
  AddonVersion myMinVersion = AddonVersion(XBMC_GUI_MIN_API_VERSION);
  AddonVersion myVersion = AddonVersion(XBMC_GUI_API_VERSION);
  return (version >= myMinVersion && minVersion <= myVersion);
}

bool CActiveAEDSPAddon::CheckAPIVersion(void)
{
  /* check the API version */
  AddonVersion minVersion = AddonVersion(XBMC_AE_DSP_MIN_API_VERSION);
  try { m_apiVersion = AddonVersion(m_pStruct->GetAudioDSPAPIVersion()); }
  catch (exception &e) { LogException(e, "GetAudioDSPAPIVersion()"); return false;  }

  if (!IsCompatibleAPIVersion(minVersion, m_apiVersion))
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - Add-on '%s' is using an incompatible API version. XBMC minimum API version = '%s', add-on API version '%s'", Name().c_str(), minVersion.c_str(), m_apiVersion.c_str());
    return false;
  }

  /* check the GUI API version */
  AddonVersion guiVersion = AddonVersion("0.0.0");
  minVersion = AddonVersion(XBMC_GUI_MIN_API_VERSION);
  try { guiVersion = AddonVersion(m_pStruct->GetGUIAPIVersion()); }
  catch (exception &e) { LogException(e, "GetGUIAPIVersion()"); return false;  }

  if (!IsCompatibleGUIAPIVersion(minVersion, guiVersion))
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - Add-on '%s' is using an incompatible GUI API version. XBMC minimum GUI API version = '%s', add-on GUI API version '%s'", Name().c_str(), minVersion.c_str(), guiVersion.c_str());
    return false;
  }

  return true;
}

bool CActiveAEDSPAddon::GetAddonProperties(void)
{
  CStdString strDSPName, strFriendlyName, strAudioDSPVersion;
  AE_DSP_ADDON_CAPABILITIES addonCapabilities;

  /* get the capabilities */
  try
  {
    memset(&addonCapabilities, 0, sizeof(addonCapabilities));
    AE_DSP_ERROR retVal = m_pStruct->GetAddonCapabilities(&addonCapabilities);
    if (retVal != AE_DSP_ERROR_NO_ERROR)
    {
      CLog::Log(LOGERROR, "ActiveAE DSP - couldn't get the capabilities for add-on '%s'. Please contact the developer of this add-on: %s", GetFriendlyName().c_str(), Author().c_str());
      return false;
    }
  }
  catch (exception &e) { LogException(e, "GetAddonCapabilities()"); return false; }

  /* get the name of the dsp addon */
  try { strDSPName = m_pStruct->GetDSPName(); }
  catch (exception &e) { LogException(e, "GetDSPName()"); return false;  }

  /* display name = backend name string */
  strFriendlyName = StringUtils::Format("%s", strDSPName.c_str());

  /* backend version number */
  try { strAudioDSPVersion = m_pStruct->GetDSPVersion(); }
  catch (exception &e) { LogException(e, "GetDSPVersion()"); return false;  }

  /* update the members */
  m_strAudioDSPName     = strDSPName;
  m_strFriendlyName     = strFriendlyName;
  m_strAudioDSPVersion  = strAudioDSPVersion;
  m_addonCapabilities   = addonCapabilities;

  return true;
}

AE_DSP_ADDON_CAPABILITIES CActiveAEDSPAddon::GetAddonCapabilities(void) const
{
  AE_DSP_ADDON_CAPABILITIES addonCapabilities(m_addonCapabilities);
  return addonCapabilities;
}

CStdString CActiveAEDSPAddon::GetAudioDSPName(void) const
{
  CStdString strReturn(m_strAudioDSPName);
  return strReturn;
}

CStdString CActiveAEDSPAddon::GetAudioDSPVersion(void) const
{
  CStdString strReturn(m_strAudioDSPVersion);
  return strReturn;
}

CStdString CActiveAEDSPAddon::GetFriendlyName(void) const
{
  CStdString strReturn(m_strFriendlyName);
  return strReturn;
}

bool CActiveAEDSPAddon::HaveMenuHooks(AE_DSP_MENUHOOK_CAT cat) const
{
  bool bReturn(false);
  if (m_bReadyToUse && m_menuhooks.size() > 0)
  {
    for (unsigned int i = 0; i < m_menuhooks.size(); i++)
    {
      if (m_menuhooks[i].category == cat || m_menuhooks[i].category == AE_DSP_MENUHOOK_ALL)
      {
        bReturn = true;
        break;
      }
    }
  }
  return bReturn;
}

AE_DSP_MENUHOOKS *CActiveAEDSPAddon::GetMenuHooks(void)
{
  return &m_menuhooks;
}

void CActiveAEDSPAddon::CallMenuHook(const AE_DSP_MENUHOOK &hook, AE_DSP_MENUHOOK_DATA &hookData)
{
  if (!m_bReadyToUse || hookData.category == AE_DSP_MENUHOOK_UNKNOWN)
    return;

  try { m_pStruct->MenuHook(hook, hookData); }
  catch (exception &e) { LogException(e, __FUNCTION__); }
}

AE_DSP_ERROR CActiveAEDSPAddon::StreamCreate(AE_DSP_SETTINGS *addonSettings, AE_DSP_STREAM_PROPERTIES* pProperties)
{
  AE_DSP_ERROR retVal(AE_DSP_ERROR_UNKNOWN);

  try
  {
    retVal = m_pStruct->StreamCreate(addonSettings, pProperties);
    if (retVal == AE_DSP_ERROR_NO_ERROR)
      m_bIsInUse = true;
    LogError(retVal, __FUNCTION__);
  }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  return retVal;
}

void CActiveAEDSPAddon::StreamDestroy(unsigned int streamId)
{
  try { m_pStruct->StreamDestroy(streamId); }
  catch (exception &e) { LogException(e, __FUNCTION__); }
  m_bIsInUse = false;
}

AE_DSP_ERROR CActiveAEDSPAddon::StreamInitialize(AE_DSP_SETTINGS *addonSettings)
{
  AE_DSP_ERROR retVal(AE_DSP_ERROR_UNKNOWN);

  try
  {
    retVal = m_pStruct->StreamInitialize(addonSettings);
    LogError(retVal, __FUNCTION__);
  }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  return retVal;
}

int CActiveAEDSPAddon::InputResampleSampleRate(unsigned int streamId)
{
  try { return m_pStruct->InputResampleSampleRate(streamId); }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  return -1;
}

int CActiveAEDSPAddon::OutputResampleSampleRate(unsigned int streamId)
{
  try { return m_pStruct->OutputResampleSampleRate(streamId); }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  return -1;
}

AE_DSP_ERROR CActiveAEDSPAddon::MasterProcessGetModes(unsigned int streamId, AE_DSP_MODES &modes)
{
  AE_DSP_ERROR retVal(AE_DSP_ERROR_UNKNOWN);

  try
  {
    retVal = m_pStruct->MasterProcessGetModes(streamId, modes);
    LogError(retVal, __FUNCTION__);
  }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  return retVal;
}

CStdString CActiveAEDSPAddon::GetMasterModeStreamInfoString(unsigned int streamId)
{
  CStdString strReturn;

  if (!m_bReadyToUse)
    return strReturn;

  try { strReturn = m_pStruct->MasterProcessGetStreamInfoString(streamId); }
  catch (exception &e) { LogException(e, __FUNCTION__); }

  return strReturn;
}

bool CActiveAEDSPAddon::SupportsInputInfoProcess(void) const
{
  return m_addonCapabilities.bSupportsInputProcess;
}

bool CActiveAEDSPAddon::SupportsInputResample(void) const
{
  return m_addonCapabilities.bSupportsInputResample;
}

bool CActiveAEDSPAddon::SupportsPreProcess(void) const
{
  return m_addonCapabilities.bSupportsPreProcess;
}

bool CActiveAEDSPAddon::SupportsMasterProcess(void) const
{
  return m_addonCapabilities.bSupportsMasterProcess;
}

bool CActiveAEDSPAddon::SupportsPostProcess(void) const
{
  return m_addonCapabilities.bSupportsPostProcess;
}

bool CActiveAEDSPAddon::SupportsOutputResample(void) const
{
  return m_addonCapabilities.bSupportsOutputResample;
}

const char *CActiveAEDSPAddon::ToString(const AE_DSP_ERROR error)
{
  switch (error)
  {
  case AE_DSP_ERROR_NO_ERROR:
    return "no error";
  case AE_DSP_ERROR_NOT_IMPLEMENTED:
    return "not implemented";
  case AE_DSP_ERROR_REJECTED:
    return "rejected by the backend";
  case AE_DSP_ERROR_INVALID_PARAMETERS:
    return "invalid parameters for this method";
  case AE_DSP_ERROR_INVALID_SAMPLERATE:
    return "invalid samplerate for this method";
  case AE_DSP_ERROR_INVALID_IN_CHANNELS:
    return "invalid input channel layout for this method";
  case AE_DSP_ERROR_INVALID_OUT_CHANNELS:
    return "invalid output channel layout for this method";
  case AE_DSP_ERROR_FAILED:
    return "the command failed";
  case AE_DSP_ERROR_UNKNOWN:
  default:
    return "unknown error";
  }
}

bool CActiveAEDSPAddon::LogError(const AE_DSP_ERROR error, const char *strMethod) const
{
  if (error != AE_DSP_ERROR_NO_ERROR && error != AE_DSP_ERROR_IGNORE_ME)
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - %s - addon '%s' returned an error: %s",
        strMethod, GetFriendlyName().c_str(), ToString(error));
    return false;
  }
  return true;
}

void CActiveAEDSPAddon::LogException(const exception &e, const char *strFunctionName) const
{
  CLog::Log(LOGERROR, "ActiveAE DSP - exception '%s' caught while trying to call '%s' on add-on '%s'. Please contact the developer of this add-on: %s", e.what(), strFunctionName, GetFriendlyName().c_str(), Author().c_str());
}
