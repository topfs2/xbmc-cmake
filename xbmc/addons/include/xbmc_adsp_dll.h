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

#ifndef __XBMC_AUDIODSP_H__
#define __XBMC_AUDIODSP_H__

#include "xbmc_addon_dll.h"
#include "xbmc_adsp_types.h"

/*!
 * Functions that the Audio DSP client add-on must implement, but some can be empty.
 *
 * The 'remarks' field indicates which methods should be implemented, and which ones are optional.
 */

extern "C"
{
  /*! @name Audio DSP add-on methods */
  //@{
  /*!
   * Get the XBMC_AE_DSP_API_VERSION that was used to compile this add-on.
   * Used to check if this add-on is compatible with XBMC.
   * @return The XBMC_AE_DSP_API_VERSION that was used to compile this add-on.
   * @remarks Valid implementation required.
   */
  const char* GetAudioDSPAPIVersion(void);

  /*!
   * Get the XBMC_AE_DSP_MIN_API_VERSION that was used to compile this add-on.
   * Used to check if this add-on is compatible with XBMC.
   * @return The XBMC_AE_DSP_MIN_API_VERSION that was used to compile this add-on.
   * @remarks Valid implementation required.
   */
  const char* GetMininumAudioDSPAPIVersion(void);

  /*!
   * Get the XBMC_GUI_API_VERSION that was used to compile this add-on.
   * Used to check if this add-on is compatible with XBMC.
   * @return The XBMC_GUI_API_VERSION that was used to compile this add-on.
   * @remarks Valid implementation required.
   */
  const char* GetGUIAPIVersion(void);

  /*!
   * Get the XBMC_GUI_MIN_API_VERSION that was used to compile this add-on.
   * Used to check if this add-on is compatible with XBMC.
   * @return The XBMC_GUI_MIN_API_VERSION that was used to compile this add-on.
   * @remarks Valid implementation required.
   */
  const char* GetMininumGUIAPIVersion(void);

  /*!
   * Get the list of features that this add-on provides.
   * Called by XBMC to query the add-on's capabilities.
   * Used to check which options should be presented in the DSP, which methods to call, etc.
   * All capabilities that the add-on supports should be set to true.
   * @param pCapabilities The add-on's capabilities.
   * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully.
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR GetAddonCapabilities(AE_DSP_ADDON_CAPABILITIES *pCapabilities);

  /*!
   * @return The name reported by the backend that will be displayed in the UI.
   * @remarks Valid implementation required.
   */
  const char* GetDSPName(void);

  /*!
   * @return The version string reported by the backend that will be displayed in the UI.
   * @remarks Valid implementation required.
   */
  const char* GetDSPVersion(void);

  /*!
   * Call one of the menu hooks (if supported).
   * Supported AE_DSP_MENUHOOK instances have to be added in ADDON_Create(), by calling AddMenuHook() on the callback.
   * @param menuhook The hook to call.
   * @param item The selected item for which the hook was called.
   * @return AE_DSP_ERROR_NO_ERROR if the hook was called successfully.
   * @remarks Optional. Return AE_DSP_ERROR_NOT_IMPLEMENTED if this add-on won't provide this function.
   */
  AE_DSP_ERROR CallMenuHook(const AE_DSP_MENUHOOK& menuhook, const AE_DSP_MENUHOOK_DATA &item);
  //@}

  /** @name DSP processing control, used to open and close a stream
   *  @remarks Valid implementation required.
   */
  //@{
  /*!
   * Set up Audio DSP with selected audio settings (use the basic present audio stream data format)
   * Used to detect available addons for present stream, as example stereo surround upmix not needed on 5.1 audio stream
   * @param addonSettings The add-on's audio settings.
   * @param pProperties The properties of the currently playing stream.
   * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully and data can be performed. AE_DSP_ERROR_IGNORE_ME if format is not supported, but without fault.
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR StreamCreate(const AE_DSP_SETTINGS *addonSettings, const AE_DSP_STREAM_PROPERTIES* pProperties);

  /*!
   * Remove the selected id from currently used dsp processes
   * @param id The stream id
   * @return AE_DSP_ERROR_NO_ERROR if the becomes found and removed
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR StreamDestroy(unsigned int id);

  /*!
   * Set up Audio DSP with selected audio settings (detected on data of first present audio packet)
   * @param addonSettings The add-on's audio settings.
   * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully.
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR StreamInitialize(const AE_DSP_SETTINGS *addonSettings);
  //@}

  /** @name DSP pre processing
   *  @remarks Only used by XBMC if bSupportsPreProcess is set to true.
   */
  //@{
  /*!
   * @brief DSP pre processing
   * Can be used to rework input stream to remove maybe stream faults.
   * Channels (upmix/downmix) or sample rate is not to change! Only the
   * input stream data can be changed, no access to other data point!
   * All DSP addons allowed todo this.
   * @param id The stream id
   * @param array_in_out Pointer to data memory
   * @param samples Amount of samples inside array_in
   * @return true if work was ok
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  bool PreProcess(unsigned int id, float **array_in_out, unsigned int samples);
  //@}

  /** @name DSP pre resampling
   *  @remarks Only used by XBMC if bSupportsInputResample is set to true.
   */
  //@{
  /*!
   * If the addon operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any InputResampleProcess call.
   * @param id The stream id
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int InputResampleProcessNeededSamplesize(unsigned int id);

  /*!
   * @brief DSP resample processing before master Here a high quality resample can be performed.
   * Only one DSP addon is allowed todo this!
   * @param id The stream id
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int InputResampleProcess(unsigned int id, float **array_in, float **array_out, unsigned int samples);

  /*!
   * Returns the resampling generated new sample rate used before the master process
   * @param id The stream id
   * @return The new samplerate
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  int InputResampleSampleRate(unsigned int id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @return the delay in seconds
   */
  float InputResampleGetDelay(unsigned int id);
  //@}

  /** @name DSP Master processing
   *  @remarks Only used by XBMC if bSupportsMasterProcess is set to true.
   */
  //@{
  /*!
   * @brief Get the by Create call from addon detected master processing modes which are available
   * @param id The stream id
   * @retval modes The return modes list table to write in.
   * @return AE_DSP_ERROR_NO_ERROR if the properties have been fetched successfully.
   */
  AE_DSP_ERROR MasterProcessGetModes(unsigned int id, AE_DSP_MODES &modes);

  /*!
   * @brief Set the active master process mode
   * @param id The stream id
   * @param client_mode_id The Mode identifier.
   * @param unique_mode_id The Mode unique id generated from dsp database.
   * @return AE_DSP_ERROR_NO_ERROR if the setup was successful
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  AE_DSP_ERROR MasterProcessSetMode(unsigned int id, unsigned int mode_type, int client_mode_id, int unique_db_mode_id);

  /*!
   * @brief If the addon operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any MasterProcess call.
   * @param id The stream id
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int MasterProcessNeededSamplesize(unsigned int id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @return the delay in seconds
   */
  float MasterProcessGetDelay(unsigned int id);

  /*!
   * @brief Master processing becomes performed with it
   * Here a channel upmix/downmix for stereo surround sound can be performed
   * Only one DSP addon is allowed todo this!
   * @param id The stream id
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int MasterProcess(unsigned int id, float **array_in, float **array_out, unsigned int samples);

  /*!
   * Used to get a information string about the processed work to show on skin
   * @return A string to show
   * @remarks Valid implementation required.
   */
  const char *MasterProcessGetStreamInfoString(unsigned int id);
  //@}

  /** @name DSP Post processing
   *  @remarks Only used by XBMC if bSupportsPostProcess is set to true.
   */
  //@{
  /*!
   * If the addon operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any PostProcess call.
   * @param id The stream id
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int PostProcessNeededSamplesize(unsigned int id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @return the delay in seconds
   */
  float PostProcessGetDelay(unsigned int id);

  /*!
   * @brief DSP post processing
   * On the post processing can be things performed with additional channel upmix like 6.1 to 7.1
   * or frequency/volume corrections, speaker distance handling, equalizer... .
   * All DSP addons allowed todo this.
   * @param id The stream id
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int PostProcess(unsigned int id, float **array_in, float **array_out, unsigned int samples);
  //@}

  /** @name DSP Post resampling
   *  @remarks Only used by XBMC if bSupportsOutputResample is set to true.
   */
  //@{
  /*!
   * @brief If the addon operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any OutputResampleProcess call.
   * @param id The stream id
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int OutputResampleProcessNeededSamplesize(unsigned int id);

  /*!
   * @brief Resampling after master processing becomes performed with it if neeeded, only
   * one addon can perfom it.
   * @param id The stream id
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int OutputResampleProcess(unsigned int id, float **array_in, float **array_out, unsigned int samples);

  /*!
   * Returns the resampling generated new sample rate used after the master process
   * @param id The stream id
   * @return The new samplerate
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  int OutputResampleSampleRate(unsigned int id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @return the delay in seconds
   */
  float OutputResampleGetDelay(unsigned int id);
  //@}

  // function to export the above structure to XBMC
  void __declspec(dllexport) get_addon(struct AudioDSP* pDSP)
  {
    pDSP->GetAudioDSPAPIVersion                 = GetAudioDSPAPIVersion;
    pDSP->GetMininumAudioDSPAPIVersion          = GetMininumAudioDSPAPIVersion;
    pDSP->GetGUIAPIVersion                      = GetGUIAPIVersion;
    pDSP->GetMininumGUIAPIVersion               = GetMininumGUIAPIVersion;
    pDSP->GetAddonCapabilities                  = GetAddonCapabilities;
    pDSP->GetDSPName                            = GetDSPName;
    pDSP->GetDSPVersion                         = GetDSPVersion;
    pDSP->MenuHook                              = CallMenuHook;
    pDSP->StreamCreate                          = StreamCreate;
    pDSP->StreamDestroy                         = StreamDestroy;
    pDSP->StreamInitialize                      = StreamInitialize;
    pDSP->PreProcess                            = PreProcess;
    pDSP->InputResampleProcessNeededSamplesize  = InputResampleProcessNeededSamplesize;
    pDSP->InputResampleProcess                  = InputResampleProcess;
    pDSP->InputResampleGetDelay                 = InputResampleGetDelay;
    pDSP->InputResampleSampleRate               = InputResampleSampleRate;
    pDSP->MasterProcessGetModes                 = MasterProcessGetModes;
    pDSP->MasterProcessSetMode                  = MasterProcessSetMode;
    pDSP->MasterProcessNeededSamplesize         = MasterProcessNeededSamplesize;
    pDSP->MasterProcessGetDelay                 = MasterProcessGetDelay;
    pDSP->MasterProcess                         = MasterProcess;
    pDSP->MasterProcessGetStreamInfoString      = MasterProcessGetStreamInfoString;
    pDSP->PostProcessNeededSamplesize           = PostProcessNeededSamplesize;
    pDSP->PostProcessGetDelay                   = PostProcessGetDelay;
    pDSP->PostProcess                           = PostProcess;
    pDSP->OutputResampleProcessNeededSamplesize = OutputResampleProcessNeededSamplesize;
    pDSP->OutputResampleProcess                 = OutputResampleProcess;
    pDSP->OutputResampleSampleRate              = OutputResampleSampleRate;
    pDSP->OutputResampleGetDelay                = OutputResampleGetDelay;
  };
};

#endif // __XBMC_AUDIODSP_H__
