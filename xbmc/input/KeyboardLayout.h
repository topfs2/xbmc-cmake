/*!
\file KeyboardLayout.h
\brief
*/

#pragma once

/*
 *      Copyright (C) 2005-2013 Team XBMC
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
#include <map>
#include <vector>

/*!
 \ingroup keyboardlayouts
 \brief
 */

class TiXmlElement;
class CSetting;

class CKeyboardLayout
{
public:
  CKeyboardLayout();
  CKeyboardLayout(const std::string &name, const TiXmlElement& element, const std::string& fallback="");
  virtual ~CKeyboardLayout(void);
  const std::string& GetName() const { return m_name; }
  const std::string& GetFallback() const { return m_fallback; }
  std::string GetCharAt(unsigned int row, unsigned int column, unsigned int modifiers = 0) const;

  void SetUseFallback(bool use) { m_useFallback = use; }

  bool UseFallback() const { return m_useFallback; }

  enum MODIFIER_KEYS
  {
    MODIFIER_KEY_NONE   = 0x00,
    MODIFIER_KEY_SHIFT  = 0x01,
    MODIFIER_KEY_SYMBOL = 0x02
  };

  /*! \brief helper to load a keyboard layout
   \param layout the layout to load.
   \return a CKeyboardLayout.
   */
  static CKeyboardLayout Load(const std::string &layout);

  /*! \brief helper to list available keyboard layouts
   \return a vector of CKeyboardLayouts with the layout names.
   */
  static std::vector<CKeyboardLayout> LoadLayouts();

  static void SettingOptionsKeyboardLayoutsFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void* data);

private:
  static std::vector<std::string> BreakCharacters(const std::string &chars);

  typedef std::vector< std::vector<std::string> > KeyboardRows;
  typedef std::map<unsigned int, KeyboardRows> Keyboards;

  std::string m_name;
  std::string m_fallback;
  bool        m_useFallback;
  Keyboards   m_keyboards;
};
