#pragma once
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

#include "addons/Addon.h"
#include "addons/AddonDll.h"
#include "addons/DllAudioDSP.h"

namespace ActiveAE
{
  class CActiveAEDSPAddon;

  typedef std::vector<AE_DSP_MENUHOOK>                      AE_DSP_MENUHOOKS;
  typedef boost::shared_ptr<ActiveAE::CActiveAEDSPAddon>    AE_DSP_ADDON;

  #define AE_DSP_INVALID_ADDON_ID (-1)

  /*!
   * Interface from XBMC to a Audio DSP add-on.
   *
   * Also translates XBMC's C++ structures to the addon's C structures.
   */
  class CActiveAEDSPAddon : public ADDON::CAddonDll<DllAudioDSP, AudioDSP, AE_DSP_PROPERTIES>
  {
  public:
    CActiveAEDSPAddon(const ADDON::AddonProps& props);
    CActiveAEDSPAddon(const cp_extension_t *ext);
    ~CActiveAEDSPAddon(void);

    /** @name Audio DSP add-on methods */
    //@{
    /*!
     * @brief Initialise the instance of this add-on.
     * @param iClientId The ID of this add-on.
     */
    ADDON_STATUS Create(int iClientId);

    /*!
     * @return True when the dll for this add-on was loaded, false otherwise (e.g. unresolved symbols)
     */
    bool DllLoaded(void) const;

    /*!
     * @brief Destroy the instance of this add-on.
     */
    void Destroy(void);

    /*!
     * @brief Destroy and recreate this add-on.
     */
    void ReCreate(void);

    /*!
     * @return True if this instance is initialised, false otherwise.
     */
    bool ReadyToUse(void) const;

    /*!
     * @return The ID of this instance.
     */
    int GetID(void) const;

    /*!
     * @return The false if this addon is currently not used.
     */
    virtual bool IsInUse() const;
    //@}

    /** @name Audio DSP processing methods */
    //@{
    /*!
     * @brief Query this add-on's capabilities.
     * @return pCapabilities The add-on's capabilities.
     */
    AE_DSP_ADDON_CAPABILITIES GetAddonCapabilities(void) const;

    /*!
     * @return The name reported by the backend.
     */
    CStdString GetAudioDSPName(void) const;

    /*!
     * @return The version string reported by the backend.
     */
    CStdString GetAudioDSPVersion(void) const;

    /*!
     * @return A friendly name for this add-on that can be used in log messages.
     */
    CStdString GetFriendlyName(void) const;

    /*!
     * @return True if this add-on has menu hooks, false otherwise.
     */
    bool HaveMenuHooks(AE_DSP_MENUHOOK_CAT cat) const;

    /*!
     * @return The menu hooks for this add-on.
     */
    AE_DSP_MENUHOOKS *GetMenuHooks(void);

    /*!
     * @brief Call one of the menu hooks of this addon.
     * @param hook The hook to call.
     * @param item The selected file item for which the hook was called.
     */
    void CallMenuHook(const AE_DSP_MENUHOOK &hook, AE_DSP_MENUHOOK_DATA &hookData);

    /*!
     * @brief Return the addon 'C' function table
     * @return Function table
     */
    AudioDSP *GetAudioDSPFunctionStruct();

    /*!
     * Set up Audio DSP with selected audio settings (use the basic present audio stream data format)
     * Used to detect available addons for present stream, as example stereo surround upmix not needed on 5.1 audio stream
     * @param addonSettings The add-on's audio settings.
     * @param pProperties The properties of the currently playing stream.
     * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully and data can be performed. AE_DSP_ERROR_IGNORE_ME if format is not supported, but without fault.
     */
    AE_DSP_ERROR StreamCreate(AE_DSP_SETTINGS *addonSettings, AE_DSP_STREAM_PROPERTIES* pProperties);

    /*!
     * Remove the selected id from currently used dsp processes
     * @param id The stream id
     */
    void StreamDestroy(unsigned int streamId);

    /*!
     * @brief Ask the addon about a requested processing mode that it is supported on the current
     * stream. Is called about every addon mode type after successed StreamCreate.
     * @param id The stream id
     * @param mode The mode to ask (structure data is known to XBMC from addons RegisterMode call)
     * @return true if supported
     */
    bool StreamIsModeSupported(unsigned int id, unsigned int mode_type, int client_mode_id, int unique_db_mode_id);

    /*!
     * Set up Audio DSP with selected audio settings (detected on data of first present audio packet)
     * @param addonSettings The add-on's audio settings.
     * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully.
     */
    AE_DSP_ERROR StreamInitialize(AE_DSP_SETTINGS *addonSettings);

    /*!
    * Returns the resampling generated new sample rate used before the master process
    * @param id The stream id
    * @return The new samplerate
    */
    int InputResampleSampleRate(unsigned int streamId);

   /*!
    * Returns the resampling generated new sample rate used after the final process
    * @param id The stream id
    * @return The new samplerate
    */
    int OutputResampleSampleRate(unsigned int streamId);

    /*!
     * @brief Get a from addon generated information string
     * @param id The stream id
     * @return Info string
     */
    CStdString GetMasterModeStreamInfoString(unsigned int streamId);

    bool SupportsInputInfoProcess(void) const;
    bool SupportsPreProcess(void) const;
    bool SupportsInputResample(void) const;
    bool SupportsMasterProcess(void) const;
    bool SupportsPostProcess(void) const;
    bool SupportsOutputResample(void) const;

    static const char *ToString(const AE_DSP_ERROR error);

  private:
    /*!
     * @brief Checks whether the provided API version is compatible with XBMC
     * @param minVersion The add-on's XBMC_AE_DSP_MIN_API_VERSION version
     * @param version The add-on's XBMC_AE_DSP_API_VERSION version
     * @return True when compatible, false otherwise
     */
    static bool IsCompatibleAPIVersion(const ADDON::AddonVersion &minVersion, const ADDON::AddonVersion &version);

    /*!
     * @brief Checks whether the provided GUI API version is compatible with XBMC
     * @param minVersion The add-on's XBMC_GUI_MIN_API_VERSION version
     * @param version The add-on's XBMC_GUI_API_VERSION version
     * @return True when compatible, false otherwise
     */
    static bool IsCompatibleGUIAPIVersion(const ADDON::AddonVersion &minVersion, const ADDON::AddonVersion &version);

    /*!
     * @brief Request the API version from the add-on, and check if it's compatible
     * @return True when compatible, false otherwise.
     */
    bool CheckAPIVersion(void);

    /*!
     * @brief Resets all class members to their defaults. Called by the constructors.
     */
    void ResetProperties(int iClientId = AE_DSP_INVALID_ADDON_ID);

    bool GetAddonProperties(void);

    bool LogError(const AE_DSP_ERROR error, const char *strMethod) const;
    void LogException(const std::exception &e, const char *strFunctionName) const;

    bool                      m_bReadyToUse;            /*!< true if this add-on is connected to the audio DSP, false otherwise */
    bool                      m_bIsInUse;               /*!< true if this add-on currentyl processing data */
    AE_DSP_MENUHOOKS          m_menuhooks;              /*!< the menu hooks for this add-on */
    int                       m_iClientId;              /*!< database ID of the audio DSP */

    /* cached data */
    CStdString                m_strAudioDSPName;        /*!< the cached audio DSP version */
    bool                      m_bGotAudioDSPName;       /*!< true if the audio DSP name has already been fetched */
    CStdString                m_strAudioDSPVersion;     /*!< the cached audio DSP version */
    bool                      m_bGotAudioDSPVersion;    /*!< true if the audio DSP version has already been fetched */
    CStdString                m_strFriendlyName;        /*!< the cached friendly name */
    bool                      m_bGotFriendlyName;       /*!< true if the friendly name has already been fetched */
    AE_DSP_ADDON_CAPABILITIES m_addonCapabilities;      /*!< the cached add-on capabilities */
    bool                      m_bGotAddonCapabilities;  /*!< true if the add-on capabilities have already been fetched */

    /* stored strings to make sure const char* members in AE_DSP_PROPERTIES stay valid */
    std::string               m_strUserPath;            /*!< @brief translated path to the user profile */
    std::string               m_strAddonPath ;          /*!< @brief translated path to this add-on */

    CCriticalSection          m_critSection;

    bool                      m_bIsPreProcessing;
    bool                      m_bIsPreResampling;
    bool                      m_bIsMasterProcessing;
    bool                      m_bIsPostResampling;
    bool                      m_bIsPostProcessing;
    ADDON::AddonVersion       m_apiVersion;
  };
}
