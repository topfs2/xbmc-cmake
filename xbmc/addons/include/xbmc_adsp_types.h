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

/*
 * Common data structures shared between XBMC and XBMC's audio dsp addons
 */

#ifndef __AUDIODSP_TYPES_H__
#define __AUDIODSP_TYPES_H__

#ifdef TARGET_WINDOWS
#include <windows.h>
#else
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __declspec
#define __declspec(X)
#endif
#endif

#include <cstddef>

#include "xbmc_addon_types.h"
#include "xbmc_codec_types.h"

#undef ATTRIBUTE_PACKED
#undef PRAGMA_PACK_BEGIN
#undef PRAGMA_PACK_END

#if defined(__GNUC__)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define ATTRIBUTE_PACKED __attribute__ ((packed))
#define PRAGMA_PACK 0
#endif
#endif

#if !defined(ATTRIBUTE_PACKED)
#define ATTRIBUTE_PACKED
#define PRAGMA_PACK 1
#endif

#define AE_DSP_ADDON_STRING_LENGTH              1024

#define AE_DSP_STREAM_MAX_STREAMS               16
#define AE_DSP_STREAM_MAX_MODES                 32

/* current Audio DSP API version */
#define XBMC_AE_DSP_API_VERSION                 "0.0.3"

/* min. Audio DSP API version */
#define XBMC_AE_DSP_MIN_API_VERSION             "0.0.3"

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned int AE_DSP_STREAM_ID;

  /*!
   * @brief Audio DSP add-on error codes
   */
  typedef enum
  {
    AE_DSP_ERROR_NO_ERROR             = 0,      /*!< @brief no error occurred */
    AE_DSP_ERROR_UNKNOWN              = -1,     /*!< @brief an unknown error occurred */
    AE_DSP_ERROR_IGNORE_ME            = -2,     /*!< @brief the used input stream can not processed and addon want to ignore */
    AE_DSP_ERROR_NOT_IMPLEMENTED      = -3,     /*!< @brief the method that XBMC called is not implemented by the add-on */
    AE_DSP_ERROR_REJECTED             = -4,     /*!< @brief the command was rejected by the dsp */
    AE_DSP_ERROR_INVALID_PARAMETERS   = -5,     /*!< @brief the parameters of the method that was called are invalid for this operation */
    AE_DSP_ERROR_INVALID_SAMPLERATE   = -6,     /*!< @brief the processed samplerate is not supported */
    AE_DSP_ERROR_INVALID_IN_CHANNELS  = -7,     /*!< @brief the processed input channel format is not supported */
    AE_DSP_ERROR_INVALID_OUT_CHANNELS = -8,     /*!< @brief the processed output channel format is not supported */
    AE_DSP_ERROR_FAILED               = -9,     /*!< @brief the command failed */
  } AE_DSP_ERROR;

  /*!
   * @brief The possible dsp channels (used as pointer inside arrays)
   */
  typedef enum
  {
    AE_DSP_CH_INVALID = -1,
    AE_DSP_CH_FL = 0,
    AE_DSP_CH_FR,
    AE_DSP_CH_FC,
    AE_DSP_CH_LFE,
    AE_DSP_CH_BL,
    AE_DSP_CH_BR,
    AE_DSP_CH_FLOC,
    AE_DSP_CH_FROC,
    AE_DSP_CH_BC,
    AE_DSP_CH_SL,
    AE_DSP_CH_SR,
    AE_DSP_CH_TFL,
    AE_DSP_CH_TFR,
    AE_DSP_CH_TFC,
    AE_DSP_CH_TC,
    AE_DSP_CH_TBL,
    AE_DSP_CH_TBR,
    AE_DSP_CH_TBC,
    AE_DSP_CH_BLOC,
    AE_DSP_CH_BROC,

    AE_DSP_CH_MAX
  } AE_DSP_CHANNEL;

  /*!
   * @brief Present channel flags
   */
  typedef enum
  {
    AE_DSP_PRSNT_CH_UNDEFINED = 0,
    AE_DSP_PRSNT_CH_FL        = 1<<0,
    AE_DSP_PRSNT_CH_FR        = 1<<1,
    AE_DSP_PRSNT_CH_FC        = 1<<2,
    AE_DSP_PRSNT_CH_LFE       = 1<<3,
    AE_DSP_PRSNT_CH_BL        = 1<<4,
    AE_DSP_PRSNT_CH_BR        = 1<<5,
    AE_DSP_PRSNT_CH_FLOC      = 1<<6,
    AE_DSP_PRSNT_CH_FROC      = 1<<7,
    AE_DSP_PRSNT_CH_BC        = 1<<8,
    AE_DSP_PRSNT_CH_SL        = 1<<9,
    AE_DSP_PRSNT_CH_SR        = 1<<10,
    AE_DSP_PRSNT_CH_TFL       = 1<<11,
    AE_DSP_PRSNT_CH_TFR       = 1<<12,
    AE_DSP_PRSNT_CH_TFC       = 1<<13,
    AE_DSP_PRSNT_CH_TC        = 1<<14,
    AE_DSP_PRSNT_CH_TBL       = 1<<15,
    AE_DSP_PRSNT_CH_TBR       = 1<<16,
    AE_DSP_PRSNT_CH_TBC       = 1<<17,
    AE_DSP_PRSNT_CH_BLOC      = 1<<18,
    AE_DSP_PRSNT_CH_BROC      = 1<<19
  } AE_DSP_CHANNEL_PRESENT;

  /**
   * @brief The various stream type formats
   * Used for audio dsp processing to know input audio type
   */
  typedef enum
  {
    AE_DSP_ASTREAM_INVALID = -1,
    AE_DSP_ASTREAM_BASIC = 0,
    AE_DSP_ASTREAM_MUSIC,
    AE_DSP_ASTREAM_MOVIE,
    AE_DSP_ASTREAM_GAME,
    AE_DSP_ASTREAM_APP,
    AE_DSP_ASTREAM_PHONE,
    AE_DSP_ASTREAM_MESSAGE,

    AE_DSP_ASTREAM_AUTO,
    AE_DSP_ASTREAM_UNDEFINED,
    AE_DSP_ASTREAM_MAX
  } AE_DSP_STREAMTYPE;

  /*!
   * @brief Addons supported audio stream type flags
   * used on master mode information on AE_DSP_MODES to know
   * on which audio stream the master mode is supported
   */
  typedef enum
  {
    AE_DSP_PRSNT_ASTREAM_BASIC    = 1<<0,
    AE_DSP_PRSNT_ASTREAM_MUSIC    = 1<<1,
    AE_DSP_PRSNT_ASTREAM_MOVIE    = 1<<2,
    AE_DSP_PRSNT_ASTREAM_GAME     = 1<<3,
    AE_DSP_PRSNT_ASTREAM_APP      = 1<<4,
    AE_DSP_PRSNT_ASTREAM_MESSAGE  = 1<<5,
    AE_DSP_PRSNT_ASTREAM_PHONE    = 1<<6,
  } AE_DSP_ASTREAM_PRESENT;

  /**
   * @brief The various base type formats
   * Used for audio dsp processing to know input audio source
   */
  typedef enum
  {
    AE_DSP_ABASE_INVALID = -1,
    AE_DSP_ABASE_STEREO = 0,
    AE_DSP_ABASE_MONO,
    AE_DSP_ABASE_MULTICHANNEL,
    AE_DSP_ABASE_AC3,
    AE_DSP_ABASE_EAC3,
    AE_DSP_ABASE_DTS,
    AE_DSP_ABASE_DTSHD_MA,
    AE_DSP_ABASE_DTSHD_HRA,
    AE_DSP_ABASE_TRUEHD,
    AE_DSP_ABASE_MLP,
    AE_DSP_ABASE_FLAC,

    AE_DSP_ABASE_UNDEFINED,
    AE_DSP_ABASE_MAX
  } AE_DSP_BASETYPE;


  typedef enum
  {
    AE_DSP_QUALITY_UNKNOWN    = -1,             /*!< @brief  Unset, unknown or incorrect quality level */
    AE_DSP_QUALITY_DEFAULT    =  0,             /*!< @brief  Engine's default quality level */

    /* Basic quality levels */
    AE_DSP_QUALITY_LOW        = 20,             /*!< @brief  Low quality level */
    AE_DSP_QUALITY_MID        = 30,             /*!< @brief  Standard quality level */
    AE_DSP_QUALITY_HIGH       = 50,             /*!< @brief  Best sound processing quality */

    /* Optional quality levels */
    AE_DSP_QUALITY_REALLYHIGH = 100             /*!< @brief  Uncompromised optional quality level, usually with unmeasurable and unnoticeable improvement */
  } AE_DSP_QUALITY;

  /*!
   * @brief Audio DSP menu hook categories
   */
  typedef enum
  {
    AE_DSP_MENUHOOK_UNKNOWN         =-1,        /*!< @brief unknown menu hook */
    AE_DSP_MENUHOOK_ALL             = 0,        /*!< @brief all categories */
    AE_DSP_MENUHOOK_PRE_PROCESS     = 1,        /*!< @brief for pre processing */
    AE_DSP_MENUHOOK_MASTER_PROCESS  = 2,        /*!< @brief for master processing */
    AE_DSP_MENUHOOK_POST_PROCESS    = 3,        /*!< @brief for post processing */
    AE_DSP_MENUHOOK_RESAMPLE        = 4,        /*!< @brief for resample */
    AE_DSP_MENUHOOK_MISCELLANEOUS   = 5,        /*!< @brief for miscellaneous dialogs */
    AE_DSP_MENUHOOK_INFORMATION     = 6,        /*!< @brief dialog to show processing information */
    AE_DSP_MENUHOOK_SETTING         = 7,        /*!< @brief for settings */
  } AE_DSP_MENUHOOK_CAT;

  /*!
   * @brief Menu hooks that are available in the menus while playing a stream via this add-on.
   */
  typedef struct AE_DSP_MENUHOOK
  {
    unsigned int        iHookId;                /*!< @brief (required) this hook's identifier */
    unsigned int        iLocalizedStringId;     /*!< @brief (required) the id of the label for this hook in g_localizeStrings */
    AE_DSP_MENUHOOK_CAT category;               /*!< @brief (required) category of menu hook */
  } ATTRIBUTE_PACKED AE_DSP_MENUHOOK;

  /*!
   * @brief Properties passed to the Create() method of an add-on.
   */
  typedef struct AE_DSP_PROPERTIES
  {
    const char* strUserPath;                    /*!< @brief path to the user profile */
    const char* strAddonPath;                   /*!< @brief path to this add-on */
  } AE_DSP_PROPERTIES;

  /*!
   * @brief Audio DSP add-on capabilities. All capabilities are set to "false" as default.
   * If a capabilty is set to true, then the corresponding methods from xbmc_audiodsp_dll.h need to be implemented.
   */
  typedef struct AE_DSP_ADDON_CAPABILITIES
  {
    bool bSupportsInputProcess;                 /*!< @brief true if this add-on provides audio input processing */
    bool bSupportsInputResample;                /*!< @brief true if this add-on provides audio resample before master handling */
    bool bSupportsPreProcess;                   /*!< @brief true if this add-on provides audio pre processing */
    bool bSupportsMasterProcess;                /*!< @brief true if this add-on supports audio master processing */
    bool bSupportsPostProcess;                  /*!< @brief true if this add-on supports audio post processing */
    bool bSupportsOutputResample;               /*!< @brief true if this add-on supports audio resample after master handling */
  } ATTRIBUTE_PACKED AE_DSP_ADDON_CAPABILITIES;

  /*!
   * @brief Audio processing settings for in and out arrays
   * Send on creation and before first processed audio packet to addon
   */
  typedef struct AE_DSP_SETTINGS
  {
    AE_DSP_STREAM_ID  iStreamID;                /*!< @brief id of the audio stream packets */
    AE_DSP_STREAMTYPE iStreamType;              /*!< @brief the input stream type source eg, Movie or Music */
    int               iInChannels;              /*!< @brief the amount of input channels */
    unsigned long     lInChannelPresentFlags;   /*!< @brief the exact channel mapping flags of input */
    int               iInFrames;                /*!< @brief the input frame size from XBMC */
    unsigned int      iInSamplerate;            /*!< @brief the basic samplerate of the audio packet */
    int               iProcessFrames;           /*!< @brief the processing frame size inside add-on's */
    unsigned int      iProcessSamplerate;       /*!< @brief the samplerate after input resample present in master processing */
    int               iOutChannels;             /*!< @brief the amount of output channels */
    unsigned long     lOutChannelPresentFlags;  /*!< @brief the exact channel mapping flags for output */
    int               iOutFrames;               /*!< @brief the final out frame size for XBMC */
    unsigned int      iOutSamplerate;           /*!< @brief the final samplerate of the audio packet */
    bool              bInputResamplingActive;   /*!< @brief if a resampling is performed before master processing this flag is set to true */
    bool              bStereoUpmix;             /*!< @brief true if the stereo upmix setting on xbmc is set */
    int               iQualityLevel;            /*!< @brief the from XBMC selected quality level for signal processing */
    /*!
     * @note about "iProcessSamplerate" and "iProcessFrames" is set from XBMC after call of StreamCreate on resample add-on, if resampling
     * and processing is handled inside the same addon, this value must be ignored!
     */
  } ATTRIBUTE_PACKED AE_DSP_SETTINGS;

  /*!
   * @brief AC3 stream profile properties
   * Can be used to detect best master processing mode and for post processing methods.
   */
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_UNDEFINED      0
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_CH1_CH2        1
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_C              2
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_L_R            3
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_L_C_R          4
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_L_R_S          5
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_L_C_R_S        6
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_L_R_SL_SR      7
  #define AE_DSP_PROFILE_AC3_CHANNELMODE_L_C_R_SL_SR    8
  #define AE_DSP_PROFILE_AC3_ROOM_UNDEFINED             0
  #define AE_DSP_PROFILE_AC3_ROOM_LARGE                 1
  #define AE_DSP_PROFILE_AC3_ROOM_SMALL                 2
  typedef struct AE_DSP_PROFILE_AC3
  {
    unsigned int  iChannelMode;
    bool          bWithLFE;
    bool          bWithSurround;
    bool          bWithDolbyDigitalEx;
    bool          bWithDolbyHP;
    unsigned int  iRoomType;
  } ATTRIBUTE_PACKED AE_DSP_PROFILE_AC3;

  /*!
   * @brief Audio DSP stream properties
   * Used to check for the dsp addon that the stream is supported,
   * as example Dolby Digital Ex processing is only required on Dolby Digital with 5.1 layout
   */
  union AE_DSP_PROFILE
  {
    AE_DSP_PROFILE_AC3      ac3;
    /// TODO: add other profiles
  };

  typedef struct AE_DSP_STREAM_PROPERTIES
  {
    AE_DSP_STREAM_ID  iStreamID;                  /*!< @brief stream id of the audio stream packets */
    AE_DSP_STREAMTYPE iStreamType;                /*!< @brief the input stream type source eg, Movie or Music */
    int               iBaseType;                  /*!< @brief the input stream base type source eg, Dolby Digital */
    const char*       strName;                    /*!< @brief the audio stream name */
    const char*       strCodecId;                 /*!< @brief codec id string of the audio stream */
    const char*       strLanguage;                /*!< @brief language id of the audio stream */
    int               iIdentifier;                /*!< @brief audio stream id inside player */
    int               iChannels;                  /*!< @brief amount of basic channels */
    int               iSampleRate;                /*!< @brief sample rate */
    AE_DSP_PROFILE    Profile;
  } ATTRIBUTE_PACKED AE_DSP_STREAM_PROPERTIES;

  /*!
   * @brief Audio DSP mode categories
   */
  typedef enum
  {
    AE_DSP_MODE_TYPE_UNDEFINED       = -1,       /*!< @brief undefined type, never be used from addon! */
    AE_DSP_MODE_TYPE_INPUT_RESAMPLE  = 0,        /*!< @brief for input resample */
    AE_DSP_MODE_TYPE_PRE_PROCESS     = 1,        /*!< @brief for pre processing */
    AE_DSP_MODE_TYPE_MASTER_PROCESS  = 2,        /*!< @brief for master processing */
    AE_DSP_MODE_TYPE_POST_PROCESS    = 3,        /*!< @brief for post processing */
    AE_DSP_MODE_TYPE_OUTPUT_RESAMPLE = 4,        /*!< @brief for output resample */
    AE_DSP_MODE_TYPE_MAX             = 5
  } AE_DSP_MODE_TYPE;

  /*!
   * @brief Audio DSP master mode information
   * Used to get all available modes for current input stream
   */
  typedef struct AE_DSP_MODES
  {
    unsigned int iModesCount;
    struct AE_DSP_MODE
    {
      int               iUniqueDBModeId;                                  /*!< @brief (required) the inside addon used identfier for the mode, set by audio dsp database */
      AE_DSP_MODE_TYPE  iModeType;                                        /*!< @brief (required) the processong mode type, see AE_DSP_MODE_TYPE */
      bool              bIsPrimary;                                       /*!< @brief (required) if set to true this mode is the first used one (if nothing other becomes selected by hand) */
      unsigned int      iModeNumber;                                      /*!< @brief (required) mode number of this mode on the add-on, is used on process functions with value "mode_id" */
      unsigned int      iModeName;                                        /*!< @brief (required) the name id of the mode for this hook in g_localizeStrings */
      unsigned int      iModeSupportTypeFlags;                            /*!< @brief (required) flags about supported input types for this mode */
      unsigned int      iModeSetupName;                                   /*!< @brief (optional) the name id of the mode inside settings for this hook in g_localizeStrings */
      unsigned int      iModeDescription;                                 /*!< @brief (optional) the description id of the mode for this hook in g_localizeStrings */
      unsigned int      iModeHelp;                                        /*!< @brief (optional) help string id for inside dsp settings dialog of the mode for this hook in g_localizeStrings */
      bool              bHasSettingsDialog;                               /*!< @brief (required) if settings are available it must be set to true */
      char              strModeName[AE_DSP_ADDON_STRING_LENGTH];          /*!< @brief (required) the addon name of the mode  */
      char              strOwnModeImage[AE_DSP_ADDON_STRING_LENGTH];      /*!< @brief (optional) flag image for the mode */
      char              strOverrideModeImage[AE_DSP_ADDON_STRING_LENGTH]; /*!< @brief (optional) image to override XBMC Image for the mode, eg. Dolby Digital with Dolby Digital Ex */
      bool              bIsHidden;                                        /*!< @brief (optional) true if this mode is marked as hidden and not usable */
    } mode[AE_DSP_STREAM_MAX_MODES];
  } ATTRIBUTE_PACKED AE_DSP_MODES;

  /*!
   * @brief Audio DSP menu hook data
   */
  typedef struct AE_DSP_MENUHOOK_DATA
  {
    AE_DSP_MENUHOOK_CAT category;
    union data {
      AE_DSP_STREAM_ID  iStreamId;
    } data;
    /// TODO: complete defination of required menu data!!!
  } ATTRIBUTE_PACKED AE_DSP_MENUHOOK_DATA;

  /*!
   * @brief Structure to transfer the methods from xbmc_audiodsp_dll.h to XBMC
   */
  struct AudioDSP
  {
    const char*  (__cdecl* GetAudioDSPAPIVersion)                (void);
    const char*  (__cdecl* GetMininumAudioDSPAPIVersion)         (void);
    const char*  (__cdecl* GetGUIAPIVersion)                     (void);
    const char*  (__cdecl* GetMininumGUIAPIVersion)              (void);
    AE_DSP_ERROR (__cdecl* GetAddonCapabilities)                 (AE_DSP_ADDON_CAPABILITIES*);
    const char*  (__cdecl* GetDSPName)                           (void);
    const char*  (__cdecl* GetDSPVersion)                        (void);
    AE_DSP_ERROR (__cdecl* MenuHook)                             (const AE_DSP_MENUHOOK&, const AE_DSP_MENUHOOK_DATA&);

    AE_DSP_ERROR (__cdecl* StreamCreate)                         (const AE_DSP_SETTINGS*, const AE_DSP_STREAM_PROPERTIES*);
    AE_DSP_ERROR (__cdecl* StreamDestroy)                        (AE_DSP_STREAM_ID);
    AE_DSP_ERROR (__cdecl* StreamIsModeSupported)                (AE_DSP_STREAM_ID, AE_DSP_MODE_TYPE, unsigned int, int);
    AE_DSP_ERROR (__cdecl* StreamInitialize)                     (const AE_DSP_SETTINGS*);

    bool         (__cdecl* InputProcess)                         (AE_DSP_STREAM_ID, float**, unsigned int);

    unsigned int (__cdecl* InputResampleProcessNeededSamplesize) (AE_DSP_STREAM_ID);
    unsigned int (__cdecl* InputResampleProcess)                 (AE_DSP_STREAM_ID, float**, float**, unsigned int);
    float        (__cdecl* InputResampleGetDelay)                (AE_DSP_STREAM_ID);
    int          (__cdecl* InputResampleSampleRate)              (AE_DSP_STREAM_ID);

    unsigned int (__cdecl* PreProcessNeededSamplesize)           (AE_DSP_STREAM_ID, unsigned int);
    float        (__cdecl* PreProcessGetDelay)                   (AE_DSP_STREAM_ID, unsigned int);
    unsigned int (__cdecl* PreProcess)                           (AE_DSP_STREAM_ID, unsigned int, float**, float**, unsigned int);

    AE_DSP_ERROR (__cdecl* MasterProcessSetMode)                 (AE_DSP_STREAM_ID, AE_DSP_STREAMTYPE, unsigned int, int);
    unsigned int (__cdecl* MasterProcessNeededSamplesize)        (AE_DSP_STREAM_ID);
    float        (__cdecl* MasterProcessGetDelay)                (AE_DSP_STREAM_ID);
    unsigned int (__cdecl* MasterProcess)                        (AE_DSP_STREAM_ID, float**, float**, unsigned int);
    const char*  (__cdecl* MasterProcessGetStreamInfoString)     (AE_DSP_STREAM_ID);

    unsigned int (__cdecl* PostProcessNeededSamplesize)          (AE_DSP_STREAM_ID, unsigned int);
    float        (__cdecl* PostProcessGetDelay)                  (AE_DSP_STREAM_ID, unsigned int);
    unsigned int (__cdecl* PostProcess)                          (AE_DSP_STREAM_ID, unsigned int, float**, float**, unsigned int);

    unsigned int (__cdecl* OutputResampleProcessNeededSamplesize)(AE_DSP_STREAM_ID);
    unsigned int (__cdecl* OutputResampleProcess)                (AE_DSP_STREAM_ID, float**, float**, unsigned int);
    float        (__cdecl* OutputResampleGetDelay)               (AE_DSP_STREAM_ID);
    int          (__cdecl* OutputResampleSampleRate)             (AE_DSP_STREAM_ID);
  };

#ifdef __cplusplus
}
#endif

#endif //__AUDIODSP_TYPES_H__
