#pragma once
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

#include "cores/AudioEngine/DSPAddons/ActiveAEDSP.h"
#include "settings/dialogs/GUIDialogSettings.h"

typedef std::vector<int> Features;

class CGUIDialogAudioDSPSettings : public CGUIDialogSettings
{
public:
  CGUIDialogAudioDSPSettings(void);
  virtual ~CGUIDialogAudioDSPSettings(void);

  virtual bool OnBack(int actionID);
  virtual void FrameMove();

protected:
  virtual void CreateSettings();
  virtual void OnSettingChanged(SettingInfo &setting);

  typedef struct
  {
    int              clientId;
    AE_DSP_MENUHOOK  hook;
  } MenuHookMember;

  void GoWindowBack(unsigned int prevId);
  bool SupportsAudioFeature(int feature);
  void UpdateModeIcons();
  void AddAddonButtons();
  void GetAudioDSPMenus(AE_DSP_MENUHOOK_CAT category, std::vector<MenuHookMember> &menus);
  void OpenAudioDSPMenu(AE_DSP_MENUHOOK_CAT category, ActiveAE::AE_DSP_ADDON client, unsigned int iHookId, unsigned int iLocalizedStringId);

  AE_DSP_STREAM_ID                            m_ActiveStreamId;
  bool                                        m_AddonMasterModeSetupPresent;             /* If any addon have a own settings dialog for a master mode it is set to true */
  bool                                        m_streamTypeReset;
  AE_DSP_STREAMTYPE                           m_streamTypeUsed;
  AE_DSP_BASETYPE                             m_baseTypeUsed;
  std::vector<ActiveAE::CActiveAEDSPModePtr>  m_ActiveModes;                            /*!< The process modes currently active on dsp processing stream */
  std::vector<float>                          m_ActiveModesCPUUsage;                    /*!< CPU usauge of the modes in previous vector, used seperate vector to have CActiveAEDSP::GetActiveModes call with a constant vector table value */
  std::vector<ActiveAE::CActiveAEDSPModePtr>  m_MasterModes[AE_DSP_ASTREAM_MAX];        /*!< table about selectable and usable master processing modes */
  int                                         m_CurrentMenu;
  std::vector<MenuHookMember>                 m_Menus;
  Features                                    m_audioCaps;

  /*! Settings control selection data */
  CStdString                                  m_InputChannels;
  CStdString                                  m_InputChannelNames;
  CStdString                                  m_InputSamplerate;
  CStdString                                  m_OutputChannels;
  CStdString                                  m_OutputChannelNames;
  CStdString                                  m_OutputSamplerate;
  CStdString                                  m_CPUUsage;
  float                                       m_volume;
};
