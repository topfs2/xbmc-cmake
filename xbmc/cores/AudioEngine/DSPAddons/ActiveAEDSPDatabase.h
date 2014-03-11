#pragma once
/*
 *      Copyright (C) 2012-2014 Team XBMC
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
#include "ActiveAEDSP.h"
#include "dbwrappers/Database.h"
#include "XBDateTime.h"
#include "utils/log.h"

class CAudioSettings;

namespace ActiveAE
{
  class CActiveAEDSPAddon;
  class CActiveAEDSPMode;
  class CActiveAEDSPProcess;

  /** The audio DSP database */

  class CActiveAEDSPDatabase : public CDatabase
  {
  public:
    /*!
     * @brief Create a new instance of the audio DSP database.
     */
    CActiveAEDSPDatabase(void) {};
    virtual ~CActiveAEDSPDatabase(void) {};

    /*!
     * @brief Open the database.
     * @return True if it was opened successfully, false otherwise.
     */
    virtual bool Open();

    /*!
     * @brief Get the minimal database version that is required to operate correctly.
     * @return The minimal database version.
     */
    virtual int GetMinVersion() const { return 1; };

    /*!
     * @brief Get the default sqlite database filename.
     * @return The default filename.
     */
    const char *GetBaseDBName() const { return "ADSP"; };

    /*! @name mode methods */
    //@{
    /*!
     * @brief Remove all modes from the database.
     * @return True if all modes were removed, false otherwise.
     */
    bool DeleteMasterModes(void);

    /*!
     * @brief Remove all modes from a add-on from the database.
     * @param addon The add-on to delete the modes for.
     * @return True if the modes were deleted, false otherwise.
     */
    bool DeleteMasterModes(const CActiveAEDSPAddon &addon);

    /*!
     * @brief Remove a mode entry from the database
     * @param mode The mode to remove.
     * @return True if the mode was removed, false otherwise.
     */
    bool DeleteMasterMode(const CActiveAEDSPMode &mode);

    /*!
     * @brief Add or if present update master mode inside database
     * @param addon The add-on to check the modes for.
     * @return True if the modes were updated or added, false otherwise.
     */
    bool AddUpdateMasterMode(const CActiveAEDSPMode &mode);

    /*!
     * @brief Get id of master mode inside database
     * @param mode The Master mode to check for inside the database
     * @return The id or -1 if not found
     */
    int GetMasterModeId(const CActiveAEDSPMode &mode);

    /*!
     * @brief Get the list of modes from the database
     * @param results The mode group to store the results in.
     * @param all if true write all available modes to results
     * @return The amount of modes that were added.
     */
    int Get(CActiveAEDSPProcess &results, bool all = false);
    //@}

    /*! @name Add-on methods */
    //@{
    /*!
     * @brief Remove all add-on information from the database.
     * @return True if all add-on's were removed successfully.
     */
    bool DeleteAddons();

    /*!
     * @brief Remove a add-on from the database
     * @param strGuid The unique ID of the add-on.
     * @return True if the add-on was removed successfully, false otherwise.
     */
    bool Delete(const CActiveAEDSPAddon &addon);

    /*!
     * @brief Get the database ID of a add-on.
     * @param strAddonUid The unique ID of the add-on.
     * @return The database ID of the add-on or -1 if it wasn't found.
     */
    int GetAudioDSPAddonId(const CStdString &strAddonUid);

    /*!
     * @brief Add a add-on to the database if it's not already in there.
     * @param addon The pointer to the addon class
     * @return The database ID of the client.
     */
    int Persist(const ADDON::AddonPtr addon);
    //@}

    /*! @name Settings methods */
    //@{

    /*!
     * @brief Remove all active dsp settings from the database.
     * @return True if all dsp data were removed successfully, false if not.
     */
    bool DeleteActiveDSPSettings();

    /*!
     * @brief Remove active dsp settings from the database for file.
     * @return True if dsp data were removed successfully, false if not.
     */
    bool DeleteActiveDSPSettings(const CFileItem *file);

    /*!
     * @brief GetVideoSettings() obtains any saved video settings for the current file.
     * @return Returns true if the settings exist, false otherwise.
     */
    bool GetActiveDSPSettings(const CFileItem *file, CAudioSettings &settings);

    /*!
     * @brief Sets the settings for a particular used file
     */
    void SetActiveDSPSettings(const CFileItem *file, const CAudioSettings &settings);

    /*!
     * @brief EraseActiveDSPSettings() Erases the dsp Settings table and reconstructs it
     */
    void EraseActiveDSPSettings();
    //@}

  private:
    /*!
     * @brief Create the audio DSP database tables.
     */
    virtual void CreateTables();
    virtual void CreateAnalytics();

    /*!
     * @brief Update an old version of the database.
     * @param version The version to update the database from.
     */
    virtual void UpdateTables(int version);
    virtual int GetSchemaVersion() const { return 0; }

    void SplitPath(const CStdString& strFileNameAndPath, CStdString& strPath, CStdString& strFileName);
  };
}
