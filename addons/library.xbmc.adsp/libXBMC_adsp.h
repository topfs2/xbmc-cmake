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

#include <string>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "xbmc_adsp_types.h"
#include "libXBMC_addon.h"

#ifdef _WIN32
#define ADSP_HELPER_DLL "\\library.xbmc.adsp\\libXBMC_adsp" ADDON_HELPER_EXT
#else
#define ADSP_HELPER_DLL_NAME "libXBMC_adsp-" ADDON_HELPER_ARCH ADDON_HELPER_EXT
#define ADSP_HELPER_DLL "/library.xbmc.adsp/" ADSP_HELPER_DLL_NAME
#endif

#define DVD_TIME_BASE 1000000
#define DVD_NOPTS_VALUE    (-1LL<<52) // should be possible to represent in both double and __int64

class CHelper_libXBMC_adsp
{
public:
  CHelper_libXBMC_adsp(void)
  {
    m_libXBMC_adsp = NULL;
    m_Handle      = NULL;
  }

  ~CHelper_libXBMC_adsp(void)
  {
    if (m_libXBMC_adsp)
    {
      ADSP_unregister_me(m_Handle, m_Callbacks);
      dlclose(m_libXBMC_adsp);
    }
  }

  /*!
   * @brief Resolve all callback methods
   * @param handle Pointer to the add-on
   * @return True when all methods were resolved, false otherwise.
   */
  bool RegisterMe(void* handle)
  {
    m_Handle = handle;

    std::string libBasePath;
    libBasePath  = ((cb_array*)m_Handle)->libPath;
    libBasePath += ADSP_HELPER_DLL;

#if defined(ANDROID)
      struct stat st;
      if(stat(libBasePath.c_str(),&st) != 0)
      {
        std::string tempbin = getenv("XBMC_ANDROID_LIBS");
        libBasePath = tempbin + "/" + ADSP_HELPER_DLL_NAME;
      }
#endif

    m_libXBMC_adsp = dlopen(libBasePath.c_str(), RTLD_LAZY);
    if (m_libXBMC_adsp == NULL)
    {
      fprintf(stderr, "Unable to load %s\n", dlerror());
      return false;
    }

    ADSP_register_me = (void* (*)(void *HANDLE))
      dlsym(m_libXBMC_adsp, "ADSP_register_me");
    if (ADSP_register_me == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    ADSP_unregister_me = (void (*)(void* HANDLE, void* CB))
      dlsym(m_libXBMC_adsp, "ADSP_unregister_me");
    if (ADSP_unregister_me == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    ADSP_add_menu_hook = (void (*)(void* HANDLE, void* CB, AE_DSP_MENUHOOK *hook))
      dlsym(m_libXBMC_adsp, "ADSP_add_menu_hook");
    if (ADSP_add_menu_hook == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    ADSP_remove_menu_hook = (void (*)(void* HANDLE, void* CB, AE_DSP_MENUHOOK *hook))
      dlsym(m_libXBMC_adsp, "ADSP_remove_menu_hook");
    if (ADSP_remove_menu_hook == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    ADSP_register_mode = (void (*)(void *HANDLE, void* CB, AE_DSP_MODES::AE_DSP_MODE *modes))
      dlsym(m_libXBMC_adsp, "ADSP_register_mode");
    if (ADSP_register_mode == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    ADSP_unregister_mode = (void (*)(void* HANDLE, void* CB, AE_DSP_MODES::AE_DSP_MODE *modes))
      dlsym(m_libXBMC_adsp, "ADSP_unregister_mode");
    if (ADSP_unregister_mode == NULL) { fprintf(stderr, "Unable to assign function %s\n", dlerror()); return false; }

    m_Callbacks = ADSP_register_me(m_Handle);
    return m_Callbacks != NULL;
  }

  /*!
   * @brief Add or replace a menu hook for the context menu for this add-on
   * @param hook The hook to add
   */
  void AddMenuHook(AE_DSP_MENUHOOK* hook)
  {
    return ADSP_add_menu_hook(m_Handle, m_Callbacks, hook);
  }

  /*!
   * @brief Remove a menu hook for the context menu for this add-on
   * @param hook The hook to remove
   */
  void RemoveMenuHook(AE_DSP_MENUHOOK* hook)
  {
    return ADSP_remove_menu_hook(m_Handle, m_Callbacks, hook);
  }

  /*!
   * @brief Add or replace master mode information inside audio dsp database.
   * Becomes identifier written inside mode to iModeID if it was 0 (undefined)
   * @param mode The master mode to add or update inside database
   */
  void RegisterMode(AE_DSP_MODES::AE_DSP_MODE* mode)
  {
    return ADSP_register_mode(m_Handle, m_Callbacks, mode);
  }

  /*!
   * @brief Remove a master mode from audio dsp database
   * @param mode The Mode to remove
   */
  void UnregisterMode(AE_DSP_MODES::AE_DSP_MODE* mode)
  {
    return ADSP_unregister_mode(m_Handle, m_Callbacks, mode);
  }

protected:
  void* (*ADSP_register_me)(void*);

  void (*ADSP_unregister_me)(void*, void*);
  void (*ADSP_add_menu_hook)(void*, void*, AE_DSP_MENUHOOK*);
  void (*ADSP_remove_menu_hook)(void*, void*, AE_DSP_MENUHOOK*);
  void (*ADSP_register_mode)(void*, void*, AE_DSP_MODES::AE_DSP_MODE*);
  void (*ADSP_unregister_mode)(void*, void*, AE_DSP_MODES::AE_DSP_MODE*);

private:
  void* m_libXBMC_adsp;
  void* m_Handle;
  void* m_Callbacks;
  struct cb_array
  {
    const char* libPath;
  };
};
