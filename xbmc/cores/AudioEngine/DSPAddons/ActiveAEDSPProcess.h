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

#include <vector>

#include "ActiveAEDSP.h"

namespace ActiveAE
{
  class CSampleBuffer;
  class CActiveAEResample;

  //@{
  /*!
   * Individual DSP Processing class
   */
  class CActiveAEDSPProcess
  {
    public:
      CActiveAEDSPProcess(AE_DSP_STREAM_ID streamId);
      virtual ~CActiveAEDSPProcess();

      //@{
      /*!>
       * Create the dsp processing with check of all addons about the used input and output audio format.
       * @param inputFormat The used audio stream input format
       * @param outputFormat Audio output format which is needed to send to the sinks
       * @param quality The requested quality from settings
       * @param streamType The input stream type to find allowed master process dsp addons for it
       * @return True if the dsp processing becomes available
       */
      bool Create(AEAudioFormat inputFormat, AEAudioFormat outputFormat, bool upmix, AEQuality quality, AE_DSP_STREAMTYPE streamType);

      /*!>
       * Destroy all allocated dsp addons for this stream id and stops the processing.
       */
      void Destroy();

      /*!>
       * Force process function to perform a reinitialization of addons and data
       */
      void ForceReinit();

      /*!>
       * Get the stream id for this class
       * @return The current used stream id
       */
      AE_DSP_STREAM_ID GetStreamId() const;

      /*!>
       * Get the used output samplerate for this class
       * @return The current used output samplerate
       */
      unsigned int GetSamplerate();

      /*!>
       * Get the channel layout which is passed out from it
       * @return Channel information class
       */
      CAEChannelInfo GetChannelLayout();

      /*!>
       * Get the currently used output data fromat
       * @return Output data format
       * @note Is normally float
       */
      AEDataFormat GetDataFormat();

      /*!>
       * Get the currently used input stream fromat
       * @return Input stream format
       * @note used to have a fallback to normal operation without dsp
       */
      AEAudioFormat GetInputFormat();

      /*!>
       * It returns the on input source detected stream type, not always the active one.
       * @return Stream type
       */
      AE_DSP_STREAMTYPE GetDetectedStreamType();

      /*!>
       * Returns the from user requested stream type, is not directly the same as on
       * the addon.
       * @return The current used stream type
       */
      AE_DSP_STREAMTYPE GetStreamType();

      /*!>
       * Get the currently on addons processed audio stream type which is set from XBMC,
       * it is user selectable or if auto mode is enabled it becomes detected upon the
       * stream input source, eg. Movie, Music...
       * @return The currently used dsp stream type
       */
      AE_DSP_STREAMTYPE GetUsedAddonStreamType();

      /*!>
       * Get the currently on addons processed audio base type which is detected from XBMC.
       * The base type is relevant to the type of input source, eg. Mono, Stereo, Dolby Digital...
       * @return The currently used dsp base type
       */
      AE_DSP_BASETYPE GetUsedAddonBaseType();

      /*!>
       * Read a description string from currently processed audio dsp master mode.
       * It can be used to show additional stream information as string on the skin.
       * The addon can have more stream information.
       * @retval strInfo Pointer to a string where it is written in
       * @return Returns false if no master processing is enabled
       */
      bool GetMasterModeStreamInfoString(CStdString &strInfo);

      /*!>
       * Get all dsp addon relavant information to detect a processing mode type and base values.
       * @retval streamTypeUsed The current stream type processed by addon
       * @retval baseType The current base type type processed by addon
       * @retval iModeID
       * @return Returns false if no master processing is enabled
       */
      bool GetMasterModeTypeInformation(AE_DSP_STREAMTYPE &streamTypeUsed, AE_DSP_BASETYPE &baseType, int &iModeID);

      /*!>
       * Change master mode with the 32 bit individual identification code, the change
       * is not directly performed in this function, is changed on next processing
       * calls and must be observed that it becomes changed.
       * @param iModeID The identification code
       * @return True if the mode is allowed can can be become changed
       */
      bool SetMasterMode(AE_DSP_STREAMTYPE streamType, int iModeID, bool bSwitchStreamType = false);

      /*!>
       * Get the 32 bit individual identification code of the running master mode
       * @return The identification code, or 0 if no master process is running
       */
      int GetMasterModeID();

      /*!>
       * Used to get all available Master mode on current stream and base type.
       * It is used to get informations about selectable modes and can be used as information
       * for the gui to make the mode selection available.
       * @retval modes Pointer to a buffer array where all available master mode written in
       */
      void GetAvailableMasterModes(AE_DSP_STREAMTYPE streamType, std::vector<CActiveAEDSPModePtr> &modes);

      /*!>
       * Get the mode information class by add-on id and add-on mode number
       * @param iAddonID The database dsp add-on id
       * @param iModeNumber the from add-on used identifier for this mode
       * @return pointer to the info class, or unitialized class if no master processing present
       */
      CActiveAEDSPModePtr GetMasterModeByAddon(int iAddonID, int iModeNumber) const;

      /*!>
       * Returns the information class from the currently used dsp addon
       * @return pointer to the info class, or unitialized class if no master processing present
       */
      CActiveAEDSPModePtr GetMasterModeRunning() const;

    protected:
      friend class CActiveAEBufferPoolResample;

      /*!>
       * Master processing
       * @param in the ActiveAE input samples
       * @param out the processed ActiveAE output samples
       * @param resampler The used internal resampler, needed to detect exact channel alignment
       * @return true if processing becomes performed correct
       */
      bool Process(CSampleBuffer *in, CSampleBuffer *out, CActiveAEResample *resampler);

      /*!>
       * Returns the time in seconds that it will take
       * for the next added packet to be heard from the speakers.
       * @return seconds
       */
      float GetDelay();
    //@}
    private:
      friend class CActiveAEDSPDatabase;                            /*!< Need access for write back to m_MasterModes */
    //@{
      /*!
       * Helper functions
       */
      bool CreateStreamProfile();
      void ResetStreamFunctionsSelection();
      AE_DSP_STREAMTYPE DetectStreamType(const CFileItem *item);
      const char *GetStreamTypeName(AE_DSP_STREAMTYPE iStreamType);
      void ClearArray(float **array, unsigned int samples);
      bool MasterModeChange(int iModeID, AE_DSP_STREAMTYPE iStreamType = AE_DSP_ASTREAM_INVALID);
      AE_DSP_BASETYPE GetBaseType(AE_DSP_STREAM_PROPERTIES *props);
      bool ReallocProcessArray(unsigned int requestSize);
    //@}
    //@{
      /*!
       * Data
       */
      const AE_DSP_STREAM_ID            m_StreamId;                 /*!< stream id of this class, is a increase/decrease number of the amount of process streams */
      AE_DSP_STREAMTYPE                 m_StreamTypeDetected;       /*! The detected stream type of the stream from the source of it */
      AE_DSP_STREAMTYPE                 m_StreamTypeAsked;          /*! The stream type we want to have */
      AE_DSP_STREAMTYPE                 m_StreamType;               /*!< The currently used stream type */
      bool                              m_ForceInit;                /*!< if set to true the process function perform a reinitialization of addons and data */
      bool                              m_StereoUpmix;              /*!< true if it was enabled by settings */
      AE_DSP_ADDONMAP                   m_usedMap;                  /*!< a map of all currently used audio dsp add-on's */
      AEAudioFormat                     m_InputFormat;
      AEAudioFormat                     m_OutputFormat;
      unsigned int                      m_OutputSamplerate;
      AEQuality                         m_ResampleQuality;
      enum AEDataFormat                 m_dataFormat;
      AE_DSP_SETTINGS                   m_AddonSettings;            /*!< the current stream's settings passed to dsp add-ons */
      AE_DSP_STREAM_PROPERTIES          m_AddonStreamProperties;    /*!< the current stream's properties (eg. stream type) passed to dsp add-ons */
      int                               m_NewMasterMode;            /*!< if master mode is changed it set here and handled by Process function */
      AE_DSP_STREAMTYPE                 m_NewStreamType;            /*!< if stream type is changed it set here and handled by Process function */

      CCriticalSection                  m_critSection;
      CCriticalSection                  m_restartSection;

      /*!>
       * Selected dsp addon functions, used this way to speed up processing
       */
      struct sDSPProcessHandle
      {
        void Clear()
        {
          iAddonModeNumber = -1;
          pFunctions       = NULL;
        }
        int                 iAddonModeNumber;                       /*!< The identifier, send from addon during mode registration and can be used from addon to select mode from a function table */
        CActiveAEDSPModePtr pMode;                                  /*!< Processing mode information data */
        AudioDSP*           pFunctions;                             /*!< The Addon function table, separeted from pAddon to safe several calls on process chain */
        AE_DSP_ADDON        pAddon;                                 /*!< Addon control class */
      };
      std::vector <AudioDSP*>           m_Addons_InputProc;         /*!< Input processing list, called to all enabled dsp addons with the basic unchanged input stream, is read only. */
      sDSPProcessHandle                 m_Addon_InputResample;      /*!< Input stream resampling over one on settings enabled input resample function only on one addon */
      std::vector <sDSPProcessHandle>   m_Addons_PreProc;           /*!< Input stream preprocessing function calls set and aligned from dsp settings stored inside database */
      std::vector <sDSPProcessHandle>   m_Addons_MasterProc;        /*!< The current from user selected master processing function on addon */
      int                               m_ActiveMode;               /*!< the current used master mode, is a pointer to m_Addons_MasterProc */
      std::vector <sDSPProcessHandle>   m_Addons_PostProc;          /*!< Output stream postprocessing function calls set and aligned from dsp settings stored inside database */
      sDSPProcessHandle                 m_Addon_OutputResample;     /*!< Output stream resampling over one on settings enabled output resample function only on one addon */


      /*!>
       * Process arrays
       */
      float                            *m_AudioInArray[AE_DSP_CH_MAX];
      unsigned int                      m_AudioInArraySize;
      float                            *m_InputResampleArray[AE_DSP_CH_MAX];
      unsigned int                      m_InputResampleArraySize;
      float                            *m_ProcessArray[2][AE_DSP_CH_MAX];
      unsigned int                      m_ProcessArraySize;
      unsigned int                      m_ProcessArrayTogglePtr;
      float                            *m_OutputResampleArray[AE_DSP_CH_MAX];
      unsigned int                      m_OutputResampleArraySize;

      /*!>
       * Index pointers for interleaved audio streams to detect correct channel alignment
       */
      int                               m_idx_in[AE_CH_MAX];
      uint64_t                          m_ChannelLayoutIn;
      int                               m_idx_out[AE_CH_MAX];
      uint64_t                          m_ChannelLayoutOut;
    //@}
  };
  //@}
}
