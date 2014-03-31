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

#include "ActiveAEDSPDatabase.h"
#include "ActiveAEDSPMode.h"
#include "cores/IPlayer.h"
#include "Engines/ActiveAE/ActiveAEBuffer.h"
#include "Engines/ActiveAE/ActiveAEResample.h"
#include "cores/AudioEngine/Utils/AEUtil.h"
#include "utils/StringUtils.h"

using namespace std;
using namespace ADDON;
using namespace ActiveAE;

bool CActiveAEDSPMode::operator==(const CActiveAEDSPMode &right) const
{
  return (m_iModeId           == right.m_iModeId &&
          m_iAddonId          == right.m_iAddonId &&
          m_iAddonModeNumber  == right.m_iAddonModeNumber &&
          m_iModeType         == right.m_iModeType &&
          m_iModePosition     == right.m_iModePosition);
}

bool CActiveAEDSPMode::operator!=(const CActiveAEDSPMode &right) const
{
  return !(*this == right);
}

CActiveAEDSPMode::CActiveAEDSPMode()
{
  m_iModeType               = AE_DSP_MODE_TYPE_UNDEFINED;
  m_iModeId                 = -1;
  m_iModePosition           = -1;
  m_bIsEnabled              = false;
  m_strOwnIconPath          = StringUtils::EmptyString;
  m_strOverrideIconPath     = StringUtils::EmptyString;
  m_iStreamTypeFlags        = 0;
  m_iBaseType               = AE_DSP_ABASE_INVALID;
  m_iModeName               = -1;
  m_iModeSetupName          = -1;
  m_iModeDescription        = -1;
  m_iModeHelp               = -1;
  m_bChanged                = false;
  m_bHasSettingsDialog      = false;

  m_fCPUUsage               = 0.0f;

  m_iAddonId                = -1;
  m_iAddonModeNumber        = -1;
  m_strAddonModeName        = StringUtils::EmptyString;
}

CActiveAEDSPMode::CActiveAEDSPMode(const AE_DSP_BASETYPE baseType)
{
  m_iModeType               = AE_DSP_MODE_TYPE_MASTER_PROCESS;
  m_iModeId                 = AE_DSP_MASTER_MODE_ID_PASSOVER;
  m_iModePosition           = 0;
  m_bIsEnabled              = true;
  m_strOwnIconPath          = StringUtils::EmptyString;
  m_strOverrideIconPath     = StringUtils::EmptyString;
  m_iStreamTypeFlags        = AE_DSP_PRSNT_ASTREAM_BASIC |
                              AE_DSP_PRSNT_ASTREAM_MUSIC |
                              AE_DSP_PRSNT_ASTREAM_MOVIE |
                              AE_DSP_PRSNT_ASTREAM_GAME |
                              AE_DSP_PRSNT_ASTREAM_APP |
                              AE_DSP_PRSNT_ASTREAM_PHONE |
                              AE_DSP_PRSNT_ASTREAM_MESSAGE;
  m_iBaseType               = baseType;
  m_iModeName               = 16039;
  m_iModeSetupName          = -1;
  m_iModeDescription        = -1;
  m_iModeHelp               = -1;
  m_bChanged                = false;
  m_bHasSettingsDialog      = false;

  m_fCPUUsage               = 0.0f;

  m_iAddonId                = -1;
  m_iAddonModeNumber        = -1;
  m_strAddonModeName        = StringUtils::EmptyString;
}

CActiveAEDSPMode::CActiveAEDSPMode(const AE_DSP_MODES::AE_DSP_MODE &mode, int iAddonId)
{
  m_iModeType               = mode.iModeType;
  m_iModePosition           = -1;
  m_iModeId                 = mode.iUniqueDBModeId;
  m_iAddonId                = iAddonId;
  m_iBaseType               = AE_DSP_ABASE_INVALID;
  m_bIsEnabled              = m_iModeType == AE_DSP_MODE_TYPE_MASTER_PROCESS ? !mode.bIsDisabled : false;
  m_strOwnIconPath          = mode.strOwnModeImage;
  m_strOverrideIconPath     = mode.strOverrideModeImage;
  m_iStreamTypeFlags        = mode.iModeSupportTypeFlags;
  m_iModeName               = mode.iModeName;
  m_iModeSetupName          = mode.iModeSetupName;
  m_iModeDescription        = mode.iModeDescription;
  m_iModeHelp               = mode.iModeHelp;
  m_iAddonModeNumber        = mode.iModeNumber;
  m_strAddonModeName        = mode.strModeName;
  m_bHasSettingsDialog      = mode.bHasSettingsDialog;
  m_bChanged                = false;

  m_fCPUUsage               = 0.0f;

  if (m_strAddonModeName.empty())
    m_strAddonModeName = StringUtils::Format("%s %d", g_localizeStrings.Get(15023).c_str(), m_iModeId);
}

CActiveAEDSPMode::CActiveAEDSPMode(const CActiveAEDSPMode &mode)
{
  *this = mode;
}

CActiveAEDSPMode &CActiveAEDSPMode::operator=(const CActiveAEDSPMode &mode)
{
  m_iModeId                 = mode.m_iModeId;
  m_iModeType               = mode.m_iModeType;
  m_iModePosition           = mode.m_iModePosition;
  m_bIsEnabled              = mode.m_bIsEnabled;
  m_strOwnIconPath          = mode.m_strOwnIconPath;
  m_strOverrideIconPath     = mode.m_strOverrideIconPath;
  m_iStreamTypeFlags        = mode.m_iStreamTypeFlags;
  m_iBaseType               = mode.m_iBaseType;
  m_iModeName               = mode.m_iModeName;
  m_iModeSetupName          = mode.m_iModeSetupName;
  m_iModeDescription        = mode.m_iModeDescription;
  m_iModeHelp               = mode.m_iModeHelp;
  m_iAddonId                = mode.m_iAddonId;
  m_iAddonModeNumber        = mode.m_iAddonModeNumber;
  m_strAddonModeName        = mode.m_strAddonModeName;
  m_bChanged                = mode.m_bChanged;
  m_bHasSettingsDialog      = mode.m_bHasSettingsDialog;
  m_fCPUUsage               = mode.m_fCPUUsage;

  return *this;
}

/********** XBMC related mode methods **********/

bool CActiveAEDSPMode::Delete(void)
{
  bool bReturn = false;

  CActiveAEDSPDatabase *database = CActiveAEDSP::Get().GetADSPDatabase();
  if (!database || !database->IsOpen())
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - failed to open the database");
    return bReturn;
  }

  bReturn = database->DeleteMode(*this);
  return bReturn;
}

int CActiveAEDSPMode::AddUpdate(bool force)
{
  if (!force)
  {
    // not changed
    CSingleLock lock(m_critSection);
    if (!m_bChanged && m_iModeId > 0)
      return m_iModeId;
  }

  CActiveAEDSPDatabase *database = CActiveAEDSP::Get().GetADSPDatabase();
  if (!database || !database->IsOpen())
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - failed to open the database");
    return -1;
  }

  database->AddUpdateMode(*this);
  m_iModeId = database->GetModeId(*this);

  return m_iModeId;
}

bool CActiveAEDSPMode::IsKnown(void)
{
  CActiveAEDSPDatabase *database = CActiveAEDSP::Get().GetADSPDatabase();
  if (!database || !database->IsOpen())
  {
    CLog::Log(LOGERROR, "ActiveAE DSP - failed to open the database");
    return -1;
  }

  return database->GetModeId(*this) > 0;
}

bool CActiveAEDSPMode::UpdateFromAddon(const CActiveAEDSPMode &mode)
{
  SetAddonID(mode.AddonID());
  SetAddonModeNumber(mode.AddonModeNumber());
  SetAddonModeName(mode.AddonModeName());
  SetBaseType(mode.BaseType());
  SetStreamTypeFlags(mode.StreamTypeFlags());
  SetModeSetupName(mode.ModeSetupName());
  SetModeName(mode.ModeName());
  SetModeHelp(mode.ModeHelp());
  SetModeDescription(mode.ModeDescription());

  CSingleLock lock(m_critSection);

  if (m_strOwnIconPath.empty() || !m_strOwnIconPath.Equals(mode.IconOwnModePath()))
    SetIconOwnModePath(mode.IconOwnModePath());
  if (m_strOverrideIconPath.empty() || !m_strOverrideIconPath.Equals(mode.IconOverrideModePath()))
    SetIconOverrideModePath(mode.IconOverrideModePath());

  return m_bChanged;
}

bool CActiveAEDSPMode::SetModeType(AE_DSP_MODE_TYPE iModeType)
{
  CSingleLock lock(m_critSection);
  if (m_iModeType != iModeType)
  {
    /* update the type */
    m_iModeType = iModeType;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetModePosition(int iModePosition)
{
  CSingleLock lock(m_critSection);
  if (m_iModePosition != iModePosition)
  {
    /* update the type */
    m_iModePosition = iModePosition;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetModeID(int iModeId)
{
  CSingleLock lock(m_critSection);
  if (m_iModeId != iModeId)
  {
    /* update the id */
    m_iModeId = iModeId;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetEnabled(bool bIsEnabled)
{
  CSingleLock lock(m_critSection);

  if (m_bIsEnabled != bIsEnabled)
  {
    /* update the Enabled flag */
    m_bIsEnabled = bIsEnabled;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetIconOwnModePath(const CStdString &strIconPath)
{
  CSingleLock lock(m_critSection);

  if (m_strOwnIconPath != strIconPath)
  {
    /* update the path */
    m_strOwnIconPath = StringUtils::Format("%s", strIconPath.c_str());
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetIconOverrideModePath(const CStdString &strIconPath)
{
  CSingleLock lock(m_critSection);

  if (m_strOverrideIconPath != strIconPath)
  {
    /* update the path */
    m_strOverrideIconPath = StringUtils::Format("%s", strIconPath.c_str());
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetModeName(int iModeName)
{
  CSingleLock lock(m_critSection);
  if (m_iModeName != iModeName)
  {
    /* update the mode name */
    m_iModeName = iModeName;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetModeSetupName(int iModeSetupName)
{
  CSingleLock lock(m_critSection);
  if (m_iModeSetupName != iModeSetupName)
  {
    /* update the mode name */
    m_iModeSetupName = iModeSetupName;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetBaseType(AE_DSP_BASETYPE baseType)
{
  CSingleLock lock(m_critSection);
  if (m_iBaseType != baseType)
  {
    /* update the mode base */
    m_iBaseType = baseType;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SupportStreamType(AE_DSP_STREAMTYPE streamType, unsigned int flags)
{
       if (streamType == AE_DSP_ASTREAM_BASIC   && (flags & AE_DSP_PRSNT_ASTREAM_BASIC))   return true;
  else if (streamType == AE_DSP_ASTREAM_MUSIC   && (flags & AE_DSP_PRSNT_ASTREAM_MUSIC))   return true;
  else if (streamType == AE_DSP_ASTREAM_MOVIE   && (flags & AE_DSP_PRSNT_ASTREAM_MOVIE))   return true;
  else if (streamType == AE_DSP_ASTREAM_GAME    && (flags & AE_DSP_PRSNT_ASTREAM_GAME))    return true;
  else if (streamType == AE_DSP_ASTREAM_APP     && (flags & AE_DSP_PRSNT_ASTREAM_APP))     return true;
  else if (streamType == AE_DSP_ASTREAM_PHONE   && (flags & AE_DSP_PRSNT_ASTREAM_PHONE))   return true;
  else if (streamType == AE_DSP_ASTREAM_MESSAGE && (flags & AE_DSP_PRSNT_ASTREAM_MESSAGE)) return true;
  return false;
}

bool CActiveAEDSPMode::SupportStreamType(AE_DSP_STREAMTYPE streamType)
{
  return SupportStreamType(streamType, m_iStreamTypeFlags);
}

bool CActiveAEDSPMode::SetStreamTypeFlags(unsigned int streamTypeFlags)
{
  CSingleLock lock(m_critSection);
  if (m_iStreamTypeFlags != streamTypeFlags)
  {
    /* update the mode type */
    m_iStreamTypeFlags = streamTypeFlags;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetModeDescription(int iModeDescription)
{
  CSingleLock lock(m_critSection);
  if (m_iModeDescription != iModeDescription)
  {
    /* update the mode name */
    m_iModeDescription = iModeDescription;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetModeHelp(int iModeHelp)
{
  CSingleLock lock(m_critSection);
  if (m_iModeHelp != iModeHelp)
  {
    /* update the mode name */
    m_iModeHelp = iModeHelp;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

/********** Addon related Mode methods **********/

bool CActiveAEDSPMode::SetAddonID(int iAddonId)
{
  CSingleLock lock(m_critSection);

  if (m_iAddonId != iAddonId)
  {
    /* update the Addon ID */
    m_iAddonId = iAddonId;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetAddonModeNumber(unsigned int iAddonModeNumber)
{
  CSingleLock lock(m_critSection);

  if (m_iAddonModeNumber != iAddonModeNumber)
  {
    /* update the Addon Mode number */
    m_iAddonModeNumber = iAddonModeNumber;
    SetChanged();
    m_bChanged = true;

    return true;
  }

  return false;
}

bool CActiveAEDSPMode::SetAddonModeName(const CStdString &strAddonModeName)
{
  CSingleLock lock(m_critSection);

  if (m_strAddonModeName != strAddonModeName)
  {
    /* update the Addon Mode name */
    m_strAddonModeName = StringUtils::Format("%s", strAddonModeName.c_str());
    SetChanged();
    // this is not persisted, so don't update m_bChanged

    return true;
  }

  return false;
}

void CActiveAEDSPMode::SetCPUUsage(float percent)
{
  CSingleLock lock(m_critSection);
  m_fCPUUsage = percent;
}

AE_DSP_MODE_TYPE CActiveAEDSPMode::ModeType(void) const
{
  CSingleLock lock(m_critSection);
  return m_iModeType;
}

int CActiveAEDSPMode::ModePosition(void) const
{
  CSingleLock lock(m_critSection);
  return m_iModePosition;
}

int CActiveAEDSPMode::ModeID(void) const
{
  CSingleLock lock(m_critSection);
  return m_iModeId;
}

bool CActiveAEDSPMode::IsNew(void) const
{
  CSingleLock lock(m_critSection);
  return m_iModeId <= 0;
}

bool CActiveAEDSPMode::IsEnabled(void) const
{
  CSingleLock lock(m_critSection);
  return m_bIsEnabled;
}

CStdString CActiveAEDSPMode::IconOwnModePath(void) const
{
  CSingleLock lock(m_critSection);
  CStdString strReturn(m_strOwnIconPath);
  return strReturn;
}

CStdString CActiveAEDSPMode::IconOverrideModePath(void) const
{
  CSingleLock lock(m_critSection);
  CStdString strReturn(m_strOverrideIconPath);
  return strReturn;
}

int CActiveAEDSPMode::ModeName(void) const
{
  CSingleLock lock(m_critSection);
  return m_iModeName;
}

int CActiveAEDSPMode::ModeSetupName(void) const
{
  CSingleLock lock(m_critSection);
  return m_iModeSetupName;
}

AE_DSP_BASETYPE CActiveAEDSPMode::BaseType(void) const
{
  CSingleLock lock(m_critSection);
  return m_iBaseType;
}

unsigned int CActiveAEDSPMode::StreamTypeFlags(void) const
{
  CSingleLock lock(m_critSection);
  return m_iStreamTypeFlags;
}

int CActiveAEDSPMode::ModeDescription(void) const
{
  CSingleLock lock(m_critSection);
  return m_iModeDescription;
}

int CActiveAEDSPMode::ModeHelp(void) const
{
  CSingleLock lock(m_critSection);
  return m_iModeHelp;
}

bool CActiveAEDSPMode::HasSettingsDialog(void) const
{
  CSingleLock lock(m_critSection);
  return m_bHasSettingsDialog;
}

bool CActiveAEDSPMode::IsChanged(void) const
{
  CSingleLock lock(m_critSection);
  return m_bChanged;
}

int CActiveAEDSPMode::AddonID(void) const
{
  CSingleLock lock(m_critSection);
  return m_iAddonId;
}

unsigned int CActiveAEDSPMode::AddonModeNumber(void) const
{
  CSingleLock lock(m_critSection);
  return m_iAddonModeNumber;
}

CStdString CActiveAEDSPMode::AddonModeName(void) const
{
  CSingleLock lock(m_critSection);
  CStdString strReturn(m_strAddonModeName);
  return strReturn;
}

float CActiveAEDSPMode::CPUUsage(void) const
{
  CSingleLock lock(m_critSection);
  return m_fCPUUsage;
}
