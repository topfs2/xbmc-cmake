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
   * Called by XBMC to query the add-ons capabilities.
   * Used to check which options should be presented in the DSP, which methods to call, etc.
   * All capabilities that the add-on supports should be set to true.
   * @param pCapabilities The add-ons capabilities.
   * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully.
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR GetAddonCapabilities(AE_DSP_ADDON_CAPABILITIES *pCapabilities);

  /*!
   * @return The name reported by the back end that will be displayed in the UI.
   * @remarks Valid implementation required.
   */
  const char* GetDSPName(void);

  /*!
   * @return The version string reported by the back end that will be displayed in the UI.
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
   * Used to detect available add-ons for present stream, as example stereo surround upmix not needed on 5.1 audio stream
   * @param addonSettings The add-ons audio settings.
   * @param pProperties The properties of the currently playing stream.
   * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully and data can be performed. AE_DSP_ERROR_IGNORE_ME if format is not supported, but without fault.
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR StreamCreate(const AE_DSP_SETTINGS *addonSettings, const AE_DSP_STREAM_PROPERTIES* pProperties);

  /*!
   * Remove the selected id from currently used DSP processes
   * @param id The stream id
   * @return AE_DSP_ERROR_NO_ERROR if the becomes found and removed
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR StreamDestroy(AE_DSP_STREAM_ID id);

  /*!
   * @brief Ask the add-on about a requested processing mode that it is supported on the current
   * stream. Is called about every add-on mode after successed StreamCreate.
   * @param id The stream id
   * @param type The processing mode type, see AE_DSP_MODE_TYPE for definitions
   * @param mode_id The mode inside add-on which must be performed on call. Id is set from add-on by iModeNumber on AE_DSP_MODE structure during RegisterMode callback,
   * @param unique_mode_id The Mode unique id generated from dsp database.
   * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully or if the stream
   * is not supported the add-on must return AE_DSP_ERROR_IGNORE_ME.
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR StreamIsModeSupported(AE_DSP_STREAM_ID id, AE_DSP_MODE_TYPE type, unsigned int mode_id, int unique_db_mode_id);

  /*!
   * Set up Audio DSP with selected audio settings (detected on data of first present audio packet)
   * @param addonSettings The add-ons audio settings.
   * @return AE_DSP_ERROR_NO_ERROR if the properties were fetched successfully.
   * @remarks Valid implementation required.
   */
  AE_DSP_ERROR StreamInitialize(const AE_DSP_SETTINGS *addonSettings);
  //@}

  /** @name DSP input processing
   *  @remarks Only used by XBMC if bSupportsInputProcess is set to true.
   */
  //@{
  /*!
   * @brief DSP input processing
   * Can be used to have unchanged stream..
   * All DSP add-ons allowed to-do this.
   * @param id The stream id
   * @param array_in Pointer to data memory
   * @param samples Amount of samples inside array_in
   * @return true if work was OK
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  bool InputProcess(AE_DSP_STREAM_ID id, float **array_in, unsigned int samples);
  //@}

  /** @name DSP pre-resampling
   *  @remarks Only used by XBMC if bSupportsInputResample is set to true.
   */
  //@{
  /*!
   * If the add-on operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any InputResampleProcess call.
   * @param id The stream id
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int InputResampleProcessNeededSamplesize(AE_DSP_STREAM_ID id);

  /*!
   * @brief DSP re sample processing before master Here a high quality resample can be performed.
   * Only one DSP add-on is allowed to-do this!
   * @param id The stream id
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int InputResampleProcess(AE_DSP_STREAM_ID id, float **array_in, float **array_out, unsigned int samples);

  /*!
   * Returns the re-sampling generated new sample rate used before the master process
   * @param id The stream id
   * @return The new sample rate
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  int InputResampleSampleRate(AE_DSP_STREAM_ID id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @return the delay in seconds
   */
  float InputResampleGetDelay(AE_DSP_STREAM_ID id);
  //@}

  /** @name DSP Pre processing
   *  @remarks Only used by XBMC if bSupportsPreProcess is set to true.
   */
  //@{
  /*!
   * If the addon operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any PreProcess call.
   * @param id The stream id
   * @param mode_id The mode inside add-on which must be performed on call. Id is set from add-on by iModeNumber on AE_DSP_MODE structure during RegisterMode callback,
   * and can be defined from add-on as a structure pointer or anything else what is needed to find it.
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int PreProcessNeededSamplesize(AE_DSP_STREAM_ID id, unsigned int mode_id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @param mode_id The mode inside add-on which must be performed on call. Id is set from add-on by iModeNumber on AE_DSP_MODE structure during RegisterMode callback,
   * and can be defined from add-on as a structure pointer or anything else what is needed to find it.
   * @return the delay in seconds
   */
  float PreProcessGetDelay(AE_DSP_STREAM_ID id, unsigned int mode_id);

  /*!
   * @brief DSP preprocessing
   * All DSP add-ons allowed to-do this.
   * @param id The stream id
   * @param mode_id The mode inside add-on which must be performed on call. Id is set from add-on by iModeNumber on AE_DSP_MODE structure during RegisterMode callback,
   * and can be defined from add-on as a structure pointer or anything else what is needed to find it.
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int PreProcess(AE_DSP_STREAM_ID id, unsigned int mode_id, float **array_in, float **array_out, unsigned int samples);
  //@}

  /** @name DSP Master processing
   *  @remarks Only used by XBMC if bSupportsMasterProcess is set to true.
   */
  //@{
  /*!
   * @brief Set the active master process mode
   * @param id The stream id
   * @param type Requested stream type for the selected master mode
   * @param mode_id The Mode identifier.
   * @param unique_mode_id The Mode unique id generated from DSP database.
   * @return AE_DSP_ERROR_NO_ERROR if the setup was successful
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  AE_DSP_ERROR MasterProcessSetMode(AE_DSP_STREAM_ID id, AE_DSP_STREAMTYPE type, unsigned int mode_id, int unique_db_mode_id);

  /*!
   * @brief If the add-on operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any MasterProcess call.
   * @param id The stream id
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int MasterProcessNeededSamplesize(AE_DSP_STREAM_ID id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @return the delay in seconds
   */
  float MasterProcessGetDelay(AE_DSP_STREAM_ID id);

  /*!
   * @brief Master processing becomes performed with it
   * Here a channel up-mix/down-mix for stereo surround sound can be performed
   * Only one DSP add-on is allowed to-do this!
   * @param id The stream id
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int MasterProcess(AE_DSP_STREAM_ID id, float **array_in, float **array_out, unsigned int samples);

  /*!
   * Used to get a information string about the processed work to show on skin
   * @return A string to show
   * @remarks Valid implementation required.
   */
  const char *MasterProcessGetStreamInfoString(AE_DSP_STREAM_ID id);
  //@}

  /** @name DSP Post processing
   *  @remarks Only used by XBMC if bSupportsPostProcess is set to true.
   */
  //@{
  /*!
   * If the add-on operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any PostProcess call.
   * @param id The stream id
   * @param mode_id The mode inside add-on which must be performed on call. Id is set from add-on by iModeNumber on AE_DSP_MODE structure during RegisterMode callback,
   * and can be defined from add-on as a structure pointer or anything else what is needed to find it.
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int PostProcessNeededSamplesize(AE_DSP_STREAM_ID id, unsigned int mode_id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @param mode_id The mode inside add-on which must be performed on call. Id is set from add-on by iModeNumber on AE_DSP_MODE structure during RegisterMode callback,
   * and can be defined from add-on as a structure pointer or anything else what is needed to find it.
   * @return the delay in seconds
   */
  float PostProcessGetDelay(AE_DSP_STREAM_ID id, unsigned int mode_id);

  /*!
   * @brief DSP post processing
   * On the post processing can be things performed with additional channel upmix like 6.1 to 7.1
   * or frequency/volume corrections, speaker distance handling, equalizer... .
   * All DSP add-ons allowed to-do this.
   * @param id The stream id
   * @param mode_id The mode inside add-on which must be performed on call. Id is set from add-on by iModeNumber on AE_DSP_MODE structure during RegisterMode callback,
   * and can be defined from add-on as a structure pointer or anything else what is needed to find it.
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int PostProcess(AE_DSP_STREAM_ID id, unsigned int mode_id, float **array_in, float **array_out, unsigned int samples);
  //@}

  /** @name DSP Post re-sampling
   *  @remarks Only used by XBMC if bSupportsOutputResample is set to true.
   */
  //@{
  /*!
   * @brief If the add-on operate with buffered arrays and the output size can be higher as the input
   * it becomes asked about needed size before any OutputResampleProcess call.
   * @param id The stream id
   * @return The needed size of output array or 0 if no changes within it
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int OutputResampleProcessNeededSamplesize(AE_DSP_STREAM_ID id);

  /*!
   * @brief Re-sampling after master processing becomes performed with it if needed, only
   * one add-on can perform it.
   * @param id The stream id
   * @param array_in Pointer to input data memory
   * @param array_out Pointer to output data memory
   * @param samples Amount of samples inside array_in
   * @return Amount of samples processed
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  unsigned int OutputResampleProcess(AE_DSP_STREAM_ID id, float **array_in, float **array_out, unsigned int samples);

  /*!
   * Returns the re-sampling generated new sample rate used after the master process
   * @param id The stream id
   * @return The new sample rate
   * @remarks Optional. Is set by AE_DSP_ADDON_CAPABILITIES and asked with GetAddonCapabilities
   */
  int OutputResampleSampleRate(AE_DSP_STREAM_ID id);

  /*!
   * Returns the time in seconds that it will take
   * for the next added packet to be heard from the speakers.
   * @param id The stream id
   * @return the delay in seconds
   */
  float OutputResampleGetDelay(AE_DSP_STREAM_ID id);
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
    pDSP->StreamIsModeSupported                 = StreamIsModeSupported;
    pDSP->StreamInitialize                      = StreamInitialize;

    pDSP->InputProcess                          = InputProcess;

    pDSP->InputResampleProcessNeededSamplesize  = InputResampleProcessNeededSamplesize;
    pDSP->InputResampleProcess                  = InputResampleProcess;
    pDSP->InputResampleGetDelay                 = InputResampleGetDelay;
    pDSP->InputResampleSampleRate               = InputResampleSampleRate;

    pDSP->PreProcessNeededSamplesize            = PreProcessNeededSamplesize;
    pDSP->PreProcessGetDelay                    = PreProcessGetDelay;
    pDSP->PreProcess                            = PreProcess;

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
