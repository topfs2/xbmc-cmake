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

#include "GUIDialogAudioDSPManager.h"

#include "FileItem.h"
#include "cores/AudioEngine/DSPAddons/ActiveAEDSP.h"
#include "dialogs/GUIDialogFileBrowser.h"
#include "dialogs/GUIDialogTextViewer.h"
#include "guilib/GUIKeyboardFactory.h"
#include "dialogs/GUIDialogOK.h"
#include "dialogs/GUIDialogProgress.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIEditControl.h"
#include "guilib/GUIRadioButtonControl.h"
#include "guilib/GUISpinControlEx.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/Key.h"
#include "guilib/LocalizeStrings.h"
#include "settings/Settings.h"
#include "storage/MediaManager.h"
#include "utils/StringUtils.h"

#define BUTTON_OK                 4
#define BUTTON_APPLY              5
#define BUTTON_CANCEL             6
#define IMAGE_CHANNEL_LOGO        10
#define SPIN_DSP_TYPE_SELECTION   13
#define CONTROL_LIST_AVAILABLE    20
#define CONTROL_LIST_ACTIVE       21
#define HELP_LABEL                22

#define LIST_AVAILABLE            0
#define LIST_ACTIVE               1

using namespace std;
using namespace PVR;
using namespace ActiveAE;

CGUIDialogAudioDSPManager::CGUIDialogAudioDSPManager(void) :
    CGUIDialog(WINDOW_DIALOG_AUDIO_DSP_MANAGER, "DialogAudioDSPManager.xml")
{
  m_bMovingMode               = false;
  m_bContainsChanges          = false;
  m_iCurrentList              = LIST_AVAILABLE;
  m_iSelected[LIST_AVAILABLE] = 0;
  m_iSelected[LIST_ACTIVE]    = 0;
  m_Items[LIST_AVAILABLE]     = new CFileItemList;
  m_Items[LIST_ACTIVE]        = new CFileItemList;
  m_iCurrentType              = AE_DSP_MODE_TYPE_UNDEFINED;
}

CGUIDialogAudioDSPManager::~CGUIDialogAudioDSPManager(void)
{
  delete m_Items[LIST_AVAILABLE];
  delete m_Items[LIST_ACTIVE];
}

bool CGUIDialogAudioDSPManager::OnActionMove(const CAction &action)
{
  bool bReturn(false);
  int iActionId = action.GetID();
  if (GetFocusedControlID() == CONTROL_LIST_ACTIVE &&
      (iActionId == ACTION_MOVE_DOWN || iActionId == ACTION_MOVE_UP ||
       iActionId == ACTION_PAGE_DOWN || iActionId == ACTION_PAGE_UP))
  {
    bReturn = true;
    if (!m_bMovingMode)
    {
      CGUIDialog::OnAction(action);
      int iSelected = m_activeViewControl.GetSelectedItem();
      if (iSelected != m_iSelected[LIST_ACTIVE])
      {
        m_iSelected[LIST_ACTIVE] = iSelected;
      }
    }
    else
    {
      CStdString strNumber;
      CGUIDialog::OnAction(action);

      bool bMoveUp        = iActionId == ACTION_PAGE_UP || iActionId == ACTION_MOVE_UP;
      unsigned int iLines = bMoveUp ? abs(m_iSelected[LIST_ACTIVE] - m_activeViewControl.GetSelectedItem()) : 1;
      bool bOutOfBounds   = bMoveUp ? m_iSelected[LIST_ACTIVE] <= 0  : m_iSelected[LIST_ACTIVE] >= m_Items[LIST_ACTIVE]->Size() - 1;
      if (bOutOfBounds)
      {
        bMoveUp = !bMoveUp;
        iLines  = m_Items[LIST_ACTIVE]->Size() - 1;
      }

      for (unsigned int iLine = 0; iLine < iLines; iLine++)
      {
        unsigned int iNewSelect = bMoveUp ? m_iSelected[LIST_ACTIVE] - 1 : m_iSelected[LIST_ACTIVE] + 1;
        if (m_Items[LIST_ACTIVE]->Get(iNewSelect)->GetProperty("Number").asString() != "-")
        {
          strNumber = StringUtils::Format("%i", m_iSelected[LIST_ACTIVE]+1);
          m_Items[LIST_ACTIVE]->Get(iNewSelect)->SetProperty("Number", strNumber);
          strNumber = StringUtils::Format("%i", iNewSelect+1);
          m_Items[LIST_ACTIVE]->Get(m_iSelected[LIST_ACTIVE])->SetProperty("Number", strNumber);
        }
        m_Items[LIST_ACTIVE]->Swap(iNewSelect, m_iSelected[LIST_ACTIVE]);
        m_iSelected[LIST_ACTIVE] = iNewSelect;
      }

      m_activeViewControl.SetItems(*m_Items[LIST_ACTIVE]);
      m_activeViewControl.SetSelectedItem(m_iSelected[LIST_ACTIVE]);
    }
  }

  return bReturn;
}

bool CGUIDialogAudioDSPManager::OnAction(const CAction& action)
{
  return OnActionMove(action) ||
         CGUIDialog::OnAction(action);
}

void CGUIDialogAudioDSPManager::OnInitWindow()
{
  CGUIDialog::OnInitWindow();

  m_iSelected[LIST_AVAILABLE]  = 0;
  m_iSelected[LIST_ACTIVE]     = 0;

  m_bMovingMode         = false;
  m_bContainsChanges    = false;

  Update();
}

void CGUIDialogAudioDSPManager::OnDeinitWindow(int nextWindowID)
{
  Clear();

  CGUIDialog::OnDeinitWindow(nextWindowID);
}

bool CGUIDialogAudioDSPManager::OnClickListAvailable(CGUIMessage &message)
{
  int iAction = message.GetParam1();
  int iItem = m_availableViewControl.GetSelectedItem();

  /* Check file item is in list range and get his pointer */
  if (iItem < 0 || iItem >= (int)m_Items[LIST_AVAILABLE]->Size()) return true;

  /* Process actions */
  if (iAction == ACTION_SELECT_ITEM || iAction == ACTION_CONTEXT_MENU || iAction == ACTION_MOUSE_LEFT_CLICK || iAction == ACTION_MOUSE_RIGHT_CLICK)
  {
    /* Show Contextmenu */
    OnPopupMenu(iItem, LIST_AVAILABLE);

    return true;
  }

  return false;
}

bool CGUIDialogAudioDSPManager::OnClickListActive(CGUIMessage &message)
{
  if (!m_bMovingMode)
  {
    int iAction = message.GetParam1();
    int iItem = m_activeViewControl.GetSelectedItem();

    /* Check file item is in list range and get his pointer */
    if (iItem < 0 || iItem >= (int)m_Items[LIST_ACTIVE]->Size()) return true;

    /* Process actions */
    if (iAction == ACTION_SELECT_ITEM || iAction == ACTION_CONTEXT_MENU || iAction == ACTION_MOUSE_LEFT_CLICK || iAction == ACTION_MOUSE_RIGHT_CLICK)
    {
      /* Show Contextmenu */
      OnPopupMenu(iItem, LIST_ACTIVE);

      return true;
    }
  }
  else
  {
    CFileItemPtr pItem = m_Items[LIST_ACTIVE]->Get(m_iSelected[LIST_ACTIVE]);
    if (pItem)
    {
      pItem->SetProperty("Changed", true);
      m_bMovingMode = false;
      m_bContainsChanges = true;
      return true;
    }
  }

  return false;
}

bool CGUIDialogAudioDSPManager::OnClickButtonOK(CGUIMessage &message)
{
  SaveList();
  Close();
  return true;
}

bool CGUIDialogAudioDSPManager::OnClickButtonApply(CGUIMessage &message)
{
  SaveList();
  return true;
}

bool CGUIDialogAudioDSPManager::OnClickButtonCancel(CGUIMessage &message)
{
  Close();
  return true;
}

bool CGUIDialogAudioDSPManager::OnClickProcessTypeSpin(CGUIMessage &message)
{
  CGUISpinControlEx *pSpin = (CGUISpinControlEx *)GetControl(SPIN_DSP_TYPE_SELECTION);
  if (pSpin)
  {
    if (m_bContainsChanges)
    {

      if (CGUIDialogYesNo::ShowAndGetInput(19098, 15078, -1, 15079))
        SaveList();

      m_bContainsChanges = false;
    }

    m_iSelected[LIST_AVAILABLE] = 0;
    m_iSelected[LIST_ACTIVE]    = 0;
    m_bMovingMode               = false;

    Update();
    return true;
  }
  return false;
}

bool CGUIDialogAudioDSPManager::OnMessageClick(CGUIMessage &message)
{
  int iControl = message.GetSenderId();
  switch(iControl)
  {
  case CONTROL_LIST_AVAILABLE:
    return OnClickListAvailable(message);
  case CONTROL_LIST_ACTIVE:
    return OnClickListActive(message);
  case BUTTON_OK:
    return OnClickButtonOK(message);
  case BUTTON_APPLY:
    return OnClickButtonApply(message);
  case BUTTON_CANCEL:
    return OnClickButtonCancel(message);
  case SPIN_DSP_TYPE_SELECTION:
    return OnClickProcessTypeSpin(message);
  default:
    return false;
  }
}

bool CGUIDialogAudioDSPManager::OnMessage(CGUIMessage& message)
{
  unsigned int iMessage = message.GetMessage();

  switch (iMessage)
  {
    case GUI_MSG_CLICKED:
      return OnMessageClick(message);
    case GUI_MSG_ITEM_SELECT:
      {
        bool ret = CGUIDialog::OnMessage(message);
        if (ret)
        {
          int iItem = -1;
          int focusedControl = GetFocusedControlID();
          if (focusedControl == CONTROL_LIST_AVAILABLE)
          {
            m_iCurrentList = LIST_AVAILABLE;
            iItem = m_availableViewControl.GetSelectedItem();
          }
          else if (focusedControl == CONTROL_LIST_ACTIVE)
          {
            m_iCurrentList = LIST_ACTIVE;
            iItem = m_activeViewControl.GetSelectedItem();
          }

          /* Check file item is in list range and get his pointer */
          if (iItem >= 0 || iItem < (int)m_Items[m_iCurrentList]->Size())
          {
            CFileItemPtr pItem = m_Items[m_iCurrentList]->Get(iItem);
            if (pItem)
              SET_CONTROL_LABEL(HELP_LABEL, pItem->GetProperty("Description").c_str());
          }
        }
        return ret;
      }
  }

  return CGUIDialog::OnMessage(message);
}

void CGUIDialogAudioDSPManager::OnWindowLoaded(void)
{
  CGUISpinControlEx *pSpin;

  g_graphicsContext.Lock();

  m_availableViewControl.Reset();
  m_availableViewControl.SetParentWindow(GetID());
  m_availableViewControl.AddView(GetControl(CONTROL_LIST_AVAILABLE));

  m_activeViewControl.Reset();
  m_activeViewControl.SetParentWindow(GetID());
  m_activeViewControl.AddView(GetControl(CONTROL_LIST_ACTIVE));

  pSpin = (CGUISpinControlEx *)GetControl(SPIN_DSP_TYPE_SELECTION);
  if (pSpin)
  {
    pSpin->Clear();
    pSpin->AddLabel(g_localizeStrings.Get(15059), AE_DSP_MODE_TYPE_MASTER_PROCESS);
    pSpin->AddLabel(g_localizeStrings.Get(15060), AE_DSP_MODE_TYPE_POST_PROCESS);
    pSpin->AddLabel(g_localizeStrings.Get(15061), AE_DSP_MODE_TYPE_OUTPUT_RESAMPLE);
    pSpin->AddLabel(g_localizeStrings.Get(15057), AE_DSP_MODE_TYPE_INPUT_RESAMPLE);
    pSpin->AddLabel(g_localizeStrings.Get(15058), AE_DSP_MODE_TYPE_PRE_PROCESS);
  }

  g_graphicsContext.Unlock();

  CGUIDialog::OnWindowLoaded();
}

void CGUIDialogAudioDSPManager::OnWindowUnload(void)
{
  CGUIDialog::OnWindowUnload();

  m_availableViewControl.Reset();
  m_activeViewControl.Reset();
}

CFileItemPtr CGUIDialogAudioDSPManager::GetCurrentListItem(int offset)
{
  return m_Items[m_iCurrentList]->Get(m_iSelected[m_iCurrentList]);
}

bool CGUIDialogAudioDSPManager::OnPopupMenu(int iItem, int listType)
{
  // popup the context menu
  // grab our context menu
  CContextButtons buttons;

  // mark the item
  if (iItem >= 0 && iItem < m_Items[listType]->Size())
    m_Items[listType]->Get(iItem)->Select(true);
  else
    return false;

  CFileItemPtr pItem = m_Items[listType]->Get(iItem);
  if (!pItem)
    return false;

  if (listType == LIST_ACTIVE)
  {
    if (m_Items[LIST_ACTIVE]->Size() > 1)
      buttons.Add(CONTEXT_BUTTON_MOVE, 116);              /* Move mode up or down */
    buttons.Add(CONTEXT_BUTTON_ACTIVATE, 15063);          /* Used to deactivate addon process type */
  }
  else if (listType == LIST_AVAILABLE)
  {
    if (!pItem->GetProperty("ActiveMode").asBoolean())
    {
      if ((m_iCurrentType == AE_DSP_MODE_TYPE_INPUT_RESAMPLE || m_iCurrentType == AE_DSP_MODE_TYPE_OUTPUT_RESAMPLE) && m_Items[LIST_ACTIVE]->Size() > 0)
        buttons.Add(CONTEXT_BUTTON_ACTIVATE, 15080);          /* Used to swap addon process type */
      else
        buttons.Add(CONTEXT_BUTTON_ACTIVATE, 15062);          /* Used to activate addon process type */
    }
    else
      buttons.Add(CONTEXT_BUTTON_ACTIVATE, 15063);          /* Used to deactivate addon process type */
  }

  if (pItem->GetProperty("Settings").asBoolean())
    buttons.Add(CONTEXT_BUTTON_SETTINGS, 15077);          /* Used to activate addon process type help description*/

  if (pItem->GetProperty("Help").asInteger() > 0)
    buttons.Add(CONTEXT_BUTTON_HELP, 15064);          /* Used to activate addon process type help description*/

  int choice = CGUIDialogContextMenu::ShowAndGetChoice(buttons);

  // deselect our item
  if (iItem >= 0 && iItem < m_Items[listType]->Size())
    m_Items[listType]->Get(iItem)->Select(false);

  if (choice < 0)
    return false;

  return OnContextButton(iItem, (CONTEXT_BUTTON)choice, listType);
}

bool CGUIDialogAudioDSPManager::OnContextButton(int itemNumber, CONTEXT_BUTTON button, int listType)
{
  /* Check file item is in list range and get his pointer */
  if (itemNumber < 0 || itemNumber >= (int)m_Items[listType]->Size()) return false;

  CFileItemPtr pItem = m_Items[listType]->Get(itemNumber);
  if (!pItem)
    return false;

  /*!
   * Open audio dsp addon mode help text dialog
   */
  if (button == CONTEXT_BUTTON_HELP)
  {
    AE_DSP_ADDON addon;
    if (CActiveAEDSP::Get().GetAudioDSPAddon(pItem->GetProperty("AddonId").asInteger(), addon))
    {
      CGUIDialogTextViewer* pDlgInfo = (CGUIDialogTextViewer*)g_windowManager.GetWindow(WINDOW_DIALOG_TEXT_VIEWER);
      pDlgInfo->SetHeading(g_localizeStrings.Get(15064)+" - "+pItem->GetProperty("Name").c_str());
      pDlgInfo->SetText(addon->GetString(pItem->GetProperty("Help").asInteger()));
      pDlgInfo->DoModal();
    }
  }
  else if (button == CONTEXT_BUTTON_ACTIVATE)
  {
    if (pItem->GetProperty("ActiveMode").asBoolean())
    {
      /// TODO: Find better way?!
      for (int iListPtr = 0; iListPtr < m_Items[LIST_AVAILABLE]->Size(); iListPtr++)
      {
        CFileItemPtr pAvailItem = m_Items[LIST_AVAILABLE]->Get(iListPtr);
        if (pAvailItem && pAvailItem->GetProperty("Name") == pItem->GetProperty("Name") &&
                          pAvailItem->GetProperty("AddonName") == pItem->GetProperty("AddonName"))
        {
          pAvailItem->SetProperty("ActiveMode", false);
          pAvailItem->SetProperty("Changed", true);

          for (int iListActivePtr = 0; iListActivePtr < m_Items[LIST_ACTIVE]->Size(); iListActivePtr++)
          {
            CFileItemPtr pSelItem = m_Items[LIST_ACTIVE]->Get(iListActivePtr);
            if (pSelItem && pSelItem->GetProperty("Name") == pAvailItem->GetProperty("Name") &&
                            pSelItem->GetProperty("AddonName") == pAvailItem->GetProperty("AddonName"))
            {
              m_Items[LIST_ACTIVE]->Remove(iListActivePtr);
              break;
            }
          }
          break;
        }
      }
    }
    else
    {
      if ((m_iCurrentType == AE_DSP_MODE_TYPE_INPUT_RESAMPLE || m_iCurrentType == AE_DSP_MODE_TYPE_OUTPUT_RESAMPLE) && m_Items[LIST_ACTIVE]->Size() > 0)
      {
        for (int iListPtr = 0; iListPtr < m_Items[LIST_AVAILABLE]->Size(); iListPtr++)
        {
          CFileItemPtr pSelItem = m_Items[LIST_AVAILABLE]->Get(iListPtr);
          if (pSelItem && pSelItem->GetProperty("Name") == m_Items[LIST_AVAILABLE]->Get(0)->GetProperty("Name") &&
                          pSelItem->GetProperty("AddonName") == m_Items[LIST_AVAILABLE]->Get(0)->GetProperty("AddonName"))
          {
            pSelItem->SetProperty("ActiveMode", false);
            pSelItem->SetProperty("Changed", true);
            break;
          }
        }
        m_Items[LIST_ACTIVE]->Clear();
      }

      CFileItemPtr modeFile(new CFileItem(pItem->GetLabel()));

      pItem->SetProperty("ActiveMode", true);

      modeFile->SetProperty("ActiveMode", true);
      modeFile->SetProperty("Number", (int)m_Items[LIST_ACTIVE]->Size());
      modeFile->SetProperty("Name", pItem->GetProperty("Name"));
      modeFile->SetProperty("Description", pItem->GetProperty("Description"));
      modeFile->SetProperty("Help", pItem->GetProperty("Help"));
      modeFile->SetProperty("Icon", pItem->GetProperty("Icon"));
      modeFile->SetProperty("Settings", pItem->GetProperty("Settings"));
      modeFile->SetProperty("AddonId", pItem->GetProperty("AddonId"));
      modeFile->SetProperty("AddonModeNumber", pItem->GetProperty("AddonModeNumber"));
      modeFile->SetProperty("AddonName", pItem->GetProperty("AddonName"));
      modeFile->SetProperty("Changed", true);
      pItem->SetProperty("Changed", true);
      m_Items[LIST_ACTIVE]->Add(modeFile);
    }
    m_bContainsChanges = true;
    m_activeViewControl.SetItems(*m_Items[LIST_ACTIVE]);
    Renumber();
  }
  else if (button == CONTEXT_BUTTON_MOVE)
  {
    m_bMovingMode = true;
    pItem->Select(true);
  }
  else if (button == CONTEXT_BUTTON_SETTINGS)
  {
    AE_DSP_ADDON addon;
    if (CActiveAEDSP::Get().GetAudioDSPAddon(pItem->GetProperty("AddonId").asInteger(), addon))
    {
      AE_DSP_MENUHOOK       hook;
      AE_DSP_MENUHOOK_DATA  hookData;

      hook.category           = AE_DSP_MENUHOOK_ALL;
      hook.iHookId            = pItem->GetProperty("AddonModeNumber").asInteger();
      hookData.category       = AE_DSP_MENUHOOK_ALL;
      hookData.data.iStreamId = -1;

      Close();
      addon->CallMenuHook(hook, hookData);

      //Lock graphic context here as it is sometimes called from non rendering threads
      //maybe we should have a critical section per window instead??
      CSingleLock lock(g_graphicsContext);

      if (!g_windowManager.Initialized())
        return false; // don't do anything

      m_closing = false;
      m_bModal = true;
      // set running before it's added to the window manager, else the auto-show code
      // could show it as well if we are in a different thread from
      // the main rendering thread (this should really be handled via
      // a thread message though IMO)
      m_active = true;
      g_windowManager.RouteToWindow(this);

      // active this window...
      CGUIMessage msg(GUI_MSG_WINDOW_INIT, 0, 0);
      OnMessage(msg);

      if (!m_windowLoaded)
        Close(true);

      lock.Leave();
    }
  }

  return true;
}

void CGUIDialogAudioDSPManager::Update()
{
  /* lock our display, as this window is rendered from the player thread */
  g_graphicsContext.Lock();
  m_availableViewControl.SetCurrentView(CONTROL_LIST_AVAILABLE);
  m_activeViewControl.SetCurrentView(CONTROL_LIST_ACTIVE);

  Clear();

  m_iCurrentType = AE_DSP_MODE_TYPE_UNDEFINED;
  CGUISpinControlEx *pSpin = (CGUISpinControlEx *)GetControl(SPIN_DSP_TYPE_SELECTION);
  if (pSpin)
    m_iCurrentType = pSpin->GetValue();

  AE_DSP_MODELIST modes;
//
  CActiveAEDSPDatabase db;
  db.Open();
  db.GetModes(modes, m_iCurrentType);

  // No modes available, nothing to do.
  if(modes.empty())
    return;

  int continuesNo = 1;
  for (unsigned int iModePtr = 0; iModePtr < modes.size(); iModePtr++)
  {
    AE_DSP_ADDON addon;
    if (!CActiveAEDSP::Get().GetAudioDSPAddon(modes[iModePtr].first->AddonID(), addon))
      continue;

    CFileItemPtr pItem(new CFileItem(addon->GetString(modes[iModePtr].first->ModeName())));

    CStdString addonName;
    CActiveAEDSP::Get().GetAudioDSPAddonName(modes[iModePtr].first->AddonID(), addonName);

    bool isActive = !modes[iModePtr].first->IsHidden();

    CStdString description;
    if (modes[iModePtr].first->ModeDescription() > 0)
      description = addon->GetString(modes[iModePtr].first->ModeDescription());
    else
      description = g_localizeStrings.Get(15065);

    pItem->SetProperty("ActiveMode", isActive);
    pItem->SetProperty("Name", addon->GetString(modes[iModePtr].first->ModeName()));
    pItem->SetProperty("Description", description);
    pItem->SetProperty("Help", modes[iModePtr].first->ModeHelp());
    pItem->SetProperty("Icon", modes[iModePtr].first->IconOwnModePath());
    pItem->SetProperty("Settings", modes[iModePtr].first->HasSettingsDialog());
    pItem->SetProperty("AddonId", modes[iModePtr].first->AddonID());
    pItem->SetProperty("AddonModeNumber", modes[iModePtr].first->AddonModeNumber());
    pItem->SetProperty("AddonName", addonName);
    pItem->SetProperty("Changed", false);

    m_Items[LIST_AVAILABLE]->Add(pItem);

    if (isActive)
    {
      CFileItemPtr pItem(new CFileItem(modes[iModePtr].first->ModeName()));

      int number = modes[iModePtr].first->ModePosition();
      if (number <= 0)
        number = continuesNo;

      pItem->SetProperty("ActiveMode", isActive);
      pItem->SetProperty("Number", number);
      pItem->SetProperty("Name", addon->GetString(modes[iModePtr].first->ModeName()));
      pItem->SetProperty("Description", description);
      pItem->SetProperty("Help", modes[iModePtr].first->ModeHelp());
      pItem->SetProperty("Icon", modes[iModePtr].first->IconOwnModePath());
      pItem->SetProperty("Settings", modes[iModePtr].first->HasSettingsDialog());
      pItem->SetProperty("AddonId", modes[iModePtr].first->AddonID());
      pItem->SetProperty("AddonModeNumber", modes[iModePtr].first->AddonModeNumber());
      pItem->SetProperty("AddonName", addonName);
      pItem->SetProperty("Changed", false);

      m_Items[LIST_ACTIVE]->Add(pItem);
      continuesNo++;
    }
  }

  m_Items[LIST_AVAILABLE]->Sort(SortByLabel, SortOrderAscending);
  if (m_iCurrentType == AE_DSP_MODE_TYPE_MASTER_PROCESS)
    m_Items[LIST_ACTIVE]->Sort(SortByLabel, SortOrderAscending);

  db.Close();

  m_availableViewControl.SetItems(*m_Items[LIST_AVAILABLE]);
  m_availableViewControl.SetSelectedItem(m_iSelected[LIST_AVAILABLE]);

  m_activeViewControl.SetItems(*m_Items[LIST_ACTIVE]);
  m_activeViewControl.SetSelectedItem(m_iSelected[LIST_ACTIVE]);

  g_graphicsContext.Unlock();
}

void CGUIDialogAudioDSPManager::Clear(void)
{
  m_availableViewControl.Clear();
  m_activeViewControl.Clear();

  m_Items[LIST_AVAILABLE]->Clear();
  m_Items[LIST_ACTIVE]->Clear();
}

void CGUIDialogAudioDSPManager::SaveList(void)
{
  if (!m_bContainsChanges)
   return;

  /* display the progress dialog */
  CGUIDialogProgress* pDlgProgress = (CGUIDialogProgress*)g_windowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
  pDlgProgress->SetHeading(190);
  pDlgProgress->SetLine(0, "");
  pDlgProgress->SetLine(1, 328);
  pDlgProgress->SetLine(2, "");
  pDlgProgress->StartModal();
  pDlgProgress->Progress();
  pDlgProgress->SetPercentage(0);

  CActiveAEDSPDatabase db;
  db.Open();

  /* persist all modes */
  for (int iListPtr = 0; iListPtr < m_Items[LIST_AVAILABLE]->Size(); iListPtr++)
  {
    CFileItemPtr pItem = m_Items[LIST_AVAILABLE]->Get(iListPtr);
    db.UpdateMode(m_iCurrentType,
                  pItem->GetProperty("ActiveMode").asBoolean(),
                  pItem->GetProperty("AddonId").asInteger(),
                  pItem->GetProperty("AddonModeNumber").asInteger(),
                  pItem->GetProperty("Number").asInteger());

    pDlgProgress->SetPercentage(iListPtr * 100 / m_Items[LIST_AVAILABLE]->Size());
  }
  for (int iListPtr = 0; iListPtr < m_Items[LIST_ACTIVE]->Size(); iListPtr++)
  {
    CFileItemPtr pItem = m_Items[LIST_ACTIVE]->Get(iListPtr);
    db.UpdateMode(m_iCurrentType,
                  pItem->GetProperty("ActiveMode").asBoolean(),
                  pItem->GetProperty("AddonId").asInteger(),
                  pItem->GetProperty("AddonModeNumber").asInteger(),
                  pItem->GetProperty("Number").asInteger());

    pDlgProgress->SetPercentage(iListPtr * 100 / m_Items[LIST_ACTIVE]->Size());
  }
  db.Close();

  CActiveAEDSP::Get().TriggerModeUpdate();

  m_bContainsChanges = false;
  SetItemsUnchanged(LIST_AVAILABLE);
  SetItemsUnchanged(LIST_ACTIVE);
  pDlgProgress->Close();
}

void CGUIDialogAudioDSPManager::SetItemsUnchanged(unsigned int listId)
{
  for (int iItemPtr = 0; iItemPtr < m_Items[listId]->Size(); iItemPtr++)
  {
    CFileItemPtr pItem = m_Items[listId]->Get(iItemPtr);
    if (pItem)
      pItem->SetProperty("Changed", false);
  }
}

void CGUIDialogAudioDSPManager::Renumber(void)
{
  int iNextModeNumber(0);
  CStdString strNumber;
  CFileItemPtr pItem;

  for (int iModePtr = 0; iModePtr < m_Items[LIST_ACTIVE]->Size(); iModePtr++)
  {
    pItem = m_Items[LIST_ACTIVE]->Get(iModePtr);
    if (pItem->GetProperty("ActiveMode").asBoolean())
    {
      strNumber = StringUtils::Format("%i", ++iNextModeNumber);
      pItem->SetProperty("Number", strNumber);
    }
    else
      pItem->SetProperty("Number", "-");
  }
}
