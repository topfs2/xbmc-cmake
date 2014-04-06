#pragma once
/*
 *      Copyright (C) 2012-2013 Team XBMC
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

#include "guilib/GUIDialog.h"
#include "dialogs/GUIDialogContextMenu.h"
#include "view/GUIViewControl.h"
#include "pvr/channels/PVRChannelGroup.h"

namespace ActiveAE
{
  class CGUIDialogAudioDSPManager : public CGUIDialog
  {
  public:
    CGUIDialogAudioDSPManager(void);
    virtual ~CGUIDialogAudioDSPManager(void);
    virtual bool OnMessage(CGUIMessage& message);
    virtual bool OnAction(const CAction& action);
    virtual void OnWindowLoaded(void);
    virtual void OnWindowUnload(void);
    virtual bool HasListItems() const { return true; };
    virtual CFileItemPtr GetCurrentListItem(int offset = 0);

  protected:
    virtual void OnInitWindow();
    virtual void OnDeinitWindow(int nextWindowID);

    virtual bool OnPopupMenu(int iItem, int listType);
    virtual bool OnContextButton(int itemNumber, CONTEXT_BUTTON button, int listType);

    virtual bool OnActionMove(const CAction &action);

    virtual bool OnMessageClick(CGUIMessage &message);

    bool OnClickListAvailable(CGUIMessage &message);
    bool OnClickListActive(CGUIMessage &message);
    bool OnClickButtonOK(CGUIMessage &message);
    bool OnClickButtonApply(CGUIMessage &message);
    bool OnClickButtonCancel(CGUIMessage &message);
    bool OnClickProcessTypeSpin(CGUIMessage &message);

    void SetItemsUnchanged(unsigned int listId);

  private:
    void Clear(void);
    void Update(void);
    void SaveList(void);
    void Renumber(void);
    bool m_bMovingMode;
    bool m_bContainsChanges;

    int m_iCurrentType;
    int m_iCurrentList;
    int m_iSelected[2];
    CFileItemList* m_Items[2];

    CGUIViewControl m_availableViewControl;
    CGUIViewControl m_activeViewControl;
  };
}
