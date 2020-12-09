/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../../../inc/MarlinConfigPre.h"

#if HAS_DGUS_LCD

#include "DGUSScreenHandler.h"
#include "DGUSDisplay.h"
#include "DGUSVPVariable.h"
#include "DGUSDisplayDef.h"

#include "../../ui_api.h"
#include "../../../../MarlinCore.h"
#include "../../../../module/temperature.h"
#include "../../../../module/motion.h"
#include "../../../../gcode/queue.h"
#include "../../../../module/planner.h"
#include "../../../../sd/cardreader.h"
#include "../../../../libs/duration_t.h"
#include "../../../../module/printcounter.h"
#if ENABLED(DGUS_LCD_UI_MKS)
  #include "src/gcode/gcode.h"
#endif 

#if ENABLED(POWER_LOSS_RECOVERY)
  #include "../../../../feature/powerloss.h"
#endif

uint16_t DGUSScreenHandler::ConfirmVP;

#if ENABLED(SDSUPPORT)
  int16_t DGUSScreenHandler::top_file = 0;
  int16_t DGUSScreenHandler::file_to_print = 0;
  static ExtUI::FileList filelist;
#endif

void (*DGUSScreenHandler::confirm_action_cb)() = nullptr;

//DGUSScreenHandler ScreenHandler;

DGUSLCD_Screens DGUSScreenHandler::current_screen;
DGUSLCD_Screens DGUSScreenHandler::past_screens[NUM_PAST_SCREENS];
uint8_t DGUSScreenHandler::update_ptr;
uint16_t DGUSScreenHandler::skipVP;
bool DGUSScreenHandler::ScreenComplete;


#if ENABLED(DGUS_LCD_UI_MKS)
uint16_t DGUSScreenHandler::DGUSLanguageSwitch = 0;   // switch language for MKS DGUS
#endif

//DGUSDisplay dgusdisplay;

// endianness swap
uint16_t swap16(const uint16_t value) { return (value & 0xffU) << 8U | (value >> 8U); }

#if ENABLED(DGUS_LCD_UI_MKS)
uint32_t swap32(const uint32_t value) { return (value & 0x000000ffU) << 24U | (value & 0x0000ff00U) << 8U  |  (value & 0x00ff0000U) >> 8U  | (value& 0xff000000U) >> 24U; }
#endif

void DGUSScreenHandler::sendinfoscreen(const char* line1, const char* line2, const char* line3, const char* line4, bool l1inflash, bool l2inflash, bool l3inflash, bool l4inflash) {
  DGUS_VP_Variable ramcopy;
  if (populate_VPVar(VP_MSGSTR1, &ramcopy)) {
    ramcopy.memadr = (void*) line1;
    l1inflash ? DGUSScreenHandler::DGUSLCD_SendStringToDisplayPGM(ramcopy) : DGUSScreenHandler::DGUSLCD_SendStringToDisplay(ramcopy);
  }
  if (populate_VPVar(VP_MSGSTR2, &ramcopy)) {
    ramcopy.memadr = (void*) line2;
    l2inflash ? DGUSScreenHandler::DGUSLCD_SendStringToDisplayPGM(ramcopy) : DGUSScreenHandler::DGUSLCD_SendStringToDisplay(ramcopy);
  }
  if (populate_VPVar(VP_MSGSTR3, &ramcopy)) {
    ramcopy.memadr = (void*) line3;
    l3inflash ? DGUSScreenHandler::DGUSLCD_SendStringToDisplayPGM(ramcopy) : DGUSScreenHandler::DGUSLCD_SendStringToDisplay(ramcopy);
  }
  if (populate_VPVar(VP_MSGSTR4, &ramcopy)) {
    ramcopy.memadr = (void*) line4;
    l4inflash ? DGUSScreenHandler::DGUSLCD_SendStringToDisplayPGM(ramcopy) : DGUSScreenHandler::DGUSLCD_SendStringToDisplay(ramcopy);
  }
}

void DGUSScreenHandler::HandleUserConfirmationPopUp(uint16_t VP, const char* line1, const char* line2, const char* line3, const char* line4, bool l1, bool l2, bool l3, bool l4) {
  if (current_screen == DGUSLCD_SCREEN_CONFIRM) {
    // Already showing a pop up, so we need to cancel that first.
    PopToOldScreen();
  }

  ConfirmVP = VP;
  sendinfoscreen(line1, line2, line3, line4, l1, l2, l3, l4);
  ScreenHandler.GotoScreen(DGUSLCD_SCREEN_CONFIRM);
}

void DGUSScreenHandler::setstatusmessage(const char *msg) {
  DGUS_VP_Variable ramcopy;
  if (populate_VPVar(VP_M117, &ramcopy)) {
    ramcopy.memadr = (void*) msg;
    DGUSLCD_SendStringToDisplay(ramcopy);
  }
}

void DGUSScreenHandler::setstatusmessagePGM(PGM_P const msg) {
  DGUS_VP_Variable ramcopy;
  if (populate_VPVar(VP_M117, &ramcopy)) {
    ramcopy.memadr = (void*) msg;
    DGUSLCD_SendStringToDisplayPGM(ramcopy);
  }
}

// Send an 8 bit or 16 bit value to the display.
void DGUSScreenHandler::DGUSLCD_SendWordValueToDisplay(DGUS_VP_Variable &var) {
  if (var.memadr) {
    //DEBUG_ECHOPAIR(" DGUS_LCD_SendWordValueToDisplay ", var.VP);
    //DEBUG_ECHOLNPAIR(" data ", *(uint16_t *)var.memadr);
    if (var.size > 1)
      dgusdisplay.WriteVariable(var.VP, *(int16_t*)var.memadr);
    else
      dgusdisplay.WriteVariable(var.VP, *(int8_t*)var.memadr);
  }
}

// Send an uint8_t between 0 and 255 to the display, but scale to a percentage (0..100)
void DGUSScreenHandler::DGUSLCD_SendPercentageToDisplay(DGUS_VP_Variable &var) {
  if (var.memadr) {
    //DEBUG_ECHOPAIR(" DGUS_LCD_SendWordValueToDisplay ", var.VP);
    //DEBUG_ECHOLNPAIR(" data ", *(uint16_t *)var.memadr);
    uint16_t tmp = *(uint8_t *) var.memadr +1 ; // +1 -> avoid rounding issues for the display.
    tmp = map(tmp, 0, 255, 0, 100);
    dgusdisplay.WriteVariable(var.VP, tmp);
  }
}

#if ENABLED(DGUS_LCD_UI_MKS)
void DGUSScreenHandler::DGUSLCD_SendFanToDisplay(DGUS_VP_Variable &var) {
  if (var.memadr) {
    //DEBUG_ECHOPAIR(" DGUS_LCD_SendWordValueToDisplay ", var.VP);
    //DEBUG_ECHOLNPAIR(" data ", *(uint16_t *)var.memadr);
    uint16_t tmp = *(uint8_t *) var.memadr; // +1 -> avoid rounding issues for the display.
    //tmp = map(tmp, 0, 255, 0, 100);
    dgusdisplay.WriteVariable(var.VP, tmp);
  }
}
#endif



// Send the current print progress to the display.
void DGUSScreenHandler::DGUSLCD_SendPrintProgressToDisplay(DGUS_VP_Variable &var) {
  //DEBUG_ECHOPAIR(" DGUSLCD_SendPrintProgressToDisplay ", var.VP);
  uint16_t tmp = ExtUI::getProgress_percent();
  //DEBUG_ECHOLNPAIR(" data ", tmp);
  dgusdisplay.WriteVariable(var.VP, tmp);
}

// Send the current print time to the display.
// It is using a hex display for that: It expects BSD coded data in the format xxyyzz
void DGUSScreenHandler::DGUSLCD_SendPrintTimeToDisplay(DGUS_VP_Variable &var) {
  duration_t elapsed = print_job_timer.duration();
  char buf[32];
  elapsed.toString(buf);
  dgusdisplay.WriteVariable(VP_PrintTime, buf, var.size, true);
}

#if ENABLED(DGUS_LCD_UI_MKS)

void DGUSScreenHandler::DGUSLCD_SendBabyStepToDisplay_MKS(DGUS_VP_Variable &var)
{
  float value = current_position.z;
  DEBUG_ECHOLNPAIR_F(" >> ", value, 6);
  value *= cpow(10, 2);
  dgusdisplay.WriteVariable(VP_SD_Print_Baby, (uint16_t)value);
}


void DGUSScreenHandler::DGUSLCD_SendPrintTimeToDisplay_MKS(DGUS_VP_Variable &var) {
  duration_t elapsed = print_job_timer.duration();

  uint32_t time = elapsed.value;
  uint16_t time_h;
  uint16_t time_m;
  uint16_t time_s;
  time_h = time / 3600;
  time_m = time %3600 / 60;
  time_s = (time % 3600) % 60;

  dgusdisplay.WriteVariable(VP_PrintTime_H, (uint16_t)time_h);
  dgusdisplay.WriteVariable(VP_PrintTime_M, (uint16_t)time_m);
  dgusdisplay.WriteVariable(VP_PrintTime_S, (uint16_t)time_s);
}

void DGUSScreenHandler::DGUSLCD_SetUint8(DGUS_VP_Variable &var, void *val_ptr) {
  if (var.memadr) {
    uint16_t value = swap16(*(uint16_t*)val_ptr);
    DEBUG_ECHOLNPAIR("FAN value get:", value);
    *(uint8_t*)var.memadr = map(constrain(value, 0, 255), 0, 255, 0, 255);
    DEBUG_ECHOLNPAIR("FAN value change:", *(uint8_t*)var.memadr);
  }
}

void DGUSScreenHandler::DGUSLCD_SendGbkToDisplay(DGUS_VP_Variable &var) {

  DEBUG_ECHOLNPAIR(" data ", *(uint16_t *)var.memadr);
  uint16_t *tmp = (uint16_t*) var.memadr;
  dgusdisplay.WriteVariable(var.VP, tmp, var.size, true);
}

#endif

// Send an uint8_t between 0 and 100 to a variable scale to 0..255
void DGUSScreenHandler::DGUSLCD_PercentageToUint8(DGUS_VP_Variable &var, void *val_ptr) {
  if (var.memadr) {
    uint16_t value = swap16(*(uint16_t*)val_ptr);
    DEBUG_ECHOLNPAIR("FAN value get:", value);
    *(uint8_t*)var.memadr = map(constrain(value, 0, 100), 0, 100, 0, 255);
    DEBUG_ECHOLNPAIR("FAN value change:", *(uint8_t*)var.memadr);
  }
}

// Sends a (RAM located) string to the DGUS Display
// (Note: The DGUS Display does not clear after the \0, you have to
// overwrite the remainings with spaces.// var.size has the display buffer size!
void DGUSScreenHandler::DGUSLCD_SendStringToDisplay(DGUS_VP_Variable &var) {
  char *tmp = (char*) var.memadr;
  dgusdisplay.WriteVariable(var.VP, tmp, var.size, true);
}

// Sends a (flash located) string to the DGUS Display
// (Note: The DGUS Display does not clear after the \0, you have to
// overwrite the remainings with spaces.// var.size has the display buffer size!
void DGUSScreenHandler::DGUSLCD_SendStringToDisplayPGM(DGUS_VP_Variable &var) {
  char *tmp = (char*) var.memadr;
  dgusdisplay.WriteVariablePGM(var.VP, tmp, var.size, true);
}

#if HAS_PID_HEATING
  void DGUSScreenHandler::DGUSLCD_SendTemperaturePID(DGUS_VP_Variable &var) {
    float value = *(float *)var.memadr;
    float valuesend = 0;
    switch (var.VP) {
      default: return;
      #if HOTENDS >= 1
        case VP_E0_PID_P: valuesend = value; break;
        case VP_E0_PID_I: valuesend = unscalePID_i(value); break;
        case VP_E0_PID_D: valuesend = unscalePID_d(value); break;
      #endif
      #if HOTENDS >= 2
        case VP_E1_PID_P: valuesend = value; break;
        case VP_E1_PID_I: valuesend = unscalePID_i(value); break;
        case VP_E1_PID_D: valuesend = unscalePID_d(value); break;
      #endif
      #if HAS_HEATED_BED
        case VP_BED_PID_P: valuesend = value; break;
        case VP_BED_PID_I: valuesend = unscalePID_i(value); break;
        case VP_BED_PID_D: valuesend = unscalePID_d(value); break;
      #endif
    }

    valuesend *= cpow(10, 1);
    union { int16_t i; char lb[2]; } endian;

    char tmp[2];
    endian.i = valuesend;
    tmp[0] = endian.lb[1];
    tmp[1] = endian.lb[0];
    dgusdisplay.WriteVariable(var.VP, tmp, 2);
  }
#endif

#if ENABLED(PRINTCOUNTER)

  // Send the accumulate print time to the display.
  // It is using a hex display for that: It expects BSD coded data in the format xxyyzz
  void DGUSScreenHandler::DGUSLCD_SendPrintAccTimeToDisplay(DGUS_VP_Variable &var) {
    printStatistics state = print_job_timer.getStats();
    char buf[22];
    duration_t elapsed = state.printTime;
    elapsed.toString(buf);
    dgusdisplay.WriteVariable(VP_PrintAccTime, buf, var.size, true);
  }

  void DGUSScreenHandler::DGUSLCD_SendPrintsTotalToDisplay(DGUS_VP_Variable &var) {
    printStatistics state = print_job_timer.getStats();
    char buf[10];
    sprintf_P(buf, PSTR("%u"), state.totalPrints);
    dgusdisplay.WriteVariable(VP_PrintsTotal, buf, var.size, true);
  }

#endif

// Send fan status value to the display.
#if HAS_FAN
  void DGUSScreenHandler::DGUSLCD_SendFanStatusToDisplay(DGUS_VP_Variable &var) {
    if (var.memadr) {
      DEBUG_ECHOPAIR(" DGUSLCD_SendFanStatusToDisplay ", var.VP);
      DEBUG_ECHOLNPAIR(" data ", *(uint8_t *)var.memadr);
      uint16_t data_to_send = 0;
      if (*(uint8_t *) var.memadr) data_to_send = 1;
      dgusdisplay.WriteVariable(var.VP, data_to_send);
    }
  }
#endif

// Send heater status value to the display.
void DGUSScreenHandler::DGUSLCD_SendHeaterStatusToDisplay(DGUS_VP_Variable &var) {
  if (var.memadr) {
    DEBUG_ECHOPAIR(" DGUSLCD_SendHeaterStatusToDisplay ", var.VP);
    DEBUG_ECHOLNPAIR(" data ", *(int16_t *)var.memadr);
    uint16_t data_to_send = 0;
    if (*(int16_t *) var.memadr) data_to_send = 1;
    dgusdisplay.WriteVariable(var.VP, data_to_send);
  }
}

#if ENABLED(DGUS_UI_WAITING)
  void DGUSScreenHandler::DGUSLCD_SendWaitingStatusToDisplay(DGUS_VP_Variable &var) {
    // In FYSETC UI design there are 10 statuses to loop
    static uint16_t period = 0;
    static uint16_t index = 0;
    //DEBUG_ECHOPAIR(" DGUSLCD_SendWaitingStatusToDisplay ", var.VP);
    //DEBUG_ECHOLNPAIR(" data ", swap16(index));
    if (period++ > DGUS_UI_WAITING_STATUS_PERIOD) {
      dgusdisplay.WriteVariable(var.VP, index);
      //DEBUG_ECHOLNPAIR(" data ", swap16(index));
      if (++index >= DGUS_UI_WAITING_STATUS) index = 0;
      period = 0;
    }
  }
#endif

#if ENABLED(SDSUPPORT)

  void DGUSScreenHandler::ScreenChangeHookIfSD(DGUS_VP_Variable &var, void *val_ptr) {
    // default action executed when there is a SD card, but not printing
    if (ExtUI::isMediaInserted() && !ExtUI::isPrintingFromMedia()) {
      ScreenChangeHook(var, val_ptr);
      dgusdisplay.RequestScreen(current_screen);
      return;
    }

    // if we are printing, we jump to two screens after the requested one.
    // This should host e.g a print pause / print abort / print resume dialog.
    // This concept allows to recycle this hook for other file
    if (ExtUI::isPrintingFromMedia() && !card.flag.abort_sd_printing) {
      GotoScreen(DGUSLCD_SCREEN_SDPRINTMANIPULATION);
      return;
    }

    // Don't let the user in the dark why there is no reaction.
    if (!ExtUI::isMediaInserted()) {
      setstatusmessagePGM(GET_TEXT(MSG_NO_MEDIA));
      return;
    }
    if (card.flag.abort_sd_printing) {
      setstatusmessagePGM(GET_TEXT(MSG_MEDIA_ABORTING));
      return;
    }
  }

  void DGUSScreenHandler::DGUSLCD_SD_ScrollFilelist(DGUS_VP_Variable& var, void *val_ptr) {
    auto old_top = top_file;
    const int16_t scroll = (int16_t)swap16(*(uint16_t*)val_ptr);

    if (scroll) {
      top_file += scroll;
      DEBUG_ECHOPAIR("new topfile calculated:", top_file);
      if (top_file < 0) {
        top_file = 0;
        DEBUG_ECHOLNPGM("Top of filelist reached");
      }
      else {
        int16_t max_top = filelist.count() -  DGUS_SD_FILESPERSCREEN;
        NOLESS(max_top, 0);
        NOMORE(top_file, max_top);
      }
      DEBUG_ECHOPAIR("new topfile adjusted:", top_file);
    }
    else if (!filelist.isAtRootDir()) {
      filelist.upDir();
      top_file = 0;
      ForceCompleteUpdate();
    }

    if (old_top != top_file) ForceCompleteUpdate();
  }

  void DGUSScreenHandler::DGUSLCD_SD_FileSelected(DGUS_VP_Variable &var, void *val_ptr) {
    uint16_t touched_nr = (int16_t)swap16(*(uint16_t*)val_ptr) + top_file;
    #if !ENABLED(DGUS_LCD_UI_MKS)
    if (touched_nr > filelist.count()) return;
    if (!filelist.seek(touched_nr)) return;
    #else
    if ((touched_nr != 0x0F) && (touched_nr > filelist.count())) return;
    if ((!filelist.seek(touched_nr)) && (touched_nr !=0x0f)) return;
    #endif

    #if ENABLED(DGUS_LCD_UI_MKS)
      if (touched_nr == 0X0F)
      { 
        if(filelist.isAtRootDir())
          GotoScreen(DGUSLCD_SCREEN_MAIN);
        else 
          filelist.upDir();

        return ;
      }
    #endif 

    if (filelist.isDir()) {
      filelist.changeDir(filelist.filename());
      top_file = 0;
      ForceCompleteUpdate();
      return;
    }

    #if ENABLED(DGUS_PRINT_FILENAME)
      // Send print filename
      dgusdisplay.WriteVariable(VP_SD_Print_Filename, filelist.filename(), VP_SD_FileName_LEN, true);
    #endif

    // Setup Confirmation screen
    file_to_print = touched_nr;
    #if ENABLED(DGUS_LCD_UI_MKS)
      GotoScreen(MKSLCD_SCREEN_PRINT_CONFIRM);
    #endif

    #if !ENABLED(DGUS_LCD_UI_MKS)
    HandleUserConfirmationPopUp(VP_SD_FileSelectConfirm, nullptr, PSTR("Print file"), filelist.filename(), PSTR("from SD Card?"), true, true, false, true);
    #endif
  }

  void DGUSScreenHandler::DGUSLCD_SD_StartPrint(DGUS_VP_Variable &var, void *val_ptr) {
    if (!filelist.seek(file_to_print)) return;
    ExtUI::printFile(filelist.shortFilename());
    ScreenHandler.GotoScreen(
      #if ENABLED(DGUS_LCD_UI_ORIGIN)
        DGUSLCD_SCREEN_STATUS
      #elif ENABLED(DGUS_LCD_UI_MKS)    
        MKSLCD_SCREEN_PRINT  
      #else
        DGUSLCD_SCREEN_SDPRINTMANIPULATION
      #endif
    );
  }

  void DGUSScreenHandler::DGUSLCD_SD_ResumePauseAbort(DGUS_VP_Variable &var, void *val_ptr) {
    if (!ExtUI::isPrintingFromMedia()) return; // avoid race condition when user stays in this menu and printer finishes.
    switch (swap16(*(uint16_t*)val_ptr)) {
      case 0:  // Resume
        if (ExtUI::isPrintingFromMediaPaused()) ExtUI::resumePrint();
        break;
      case 1:  // Pause
        if (!ExtUI::isPrintingFromMediaPaused()) ExtUI::pausePrint();
        break;
      case 2:  // Abort
        ScreenHandler.HandleUserConfirmationPopUp(VP_SD_AbortPrintConfirmed, nullptr, PSTR("Abort printing"), filelist.filename(), PSTR("?"), true, true, false, true);
        break;
    }
  }

  void DGUSScreenHandler::DGUSLCD_SD_ReallyAbort(DGUS_VP_Variable &var, void *val_ptr) {
    ExtUI::stopPrint();
    GotoScreen(DGUSLCD_SCREEN_MAIN);
  }

  void DGUSScreenHandler::DGUSLCD_SD_PrintTune(DGUS_VP_Variable &var, void *val_ptr) {
    if (!ExtUI::isPrintingFromMedia()) return; // avoid race condition when user stays in this menu and printer finishes.
    GotoScreen(DGUSLCD_SCREEN_SDPRINTTUNE);
  }

  void DGUSScreenHandler::DGUSLCD_SD_SendFilename(DGUS_VP_Variable& var) {
    uint16_t target_line = (var.VP - VP_SD_FileName0) / VP_SD_FileName_LEN;
    if (target_line > DGUS_SD_FILESPERSCREEN) return;
    char tmpfilename[VP_SD_FileName_LEN + 1] = "";
    var.memadr = (void*)tmpfilename;
    if (filelist.seek(top_file + target_line))
      snprintf_P(tmpfilename, VP_SD_FileName_LEN, PSTR("%s%c"), filelist.filename(), filelist.isDir() ? '/' : 0);
    DGUSLCD_SendStringToDisplay(var);
  }

  void DGUSScreenHandler::SDCardInserted() {
    top_file = 0;
    filelist.refresh();
    auto cs = ScreenHandler.getCurrentScreen();
    if (cs == DGUSLCD_SCREEN_MAIN || cs == DGUSLCD_SCREEN_STATUS)
    {
        #if ENABLED(DGUS_LCD_UI_MKS)
          ScreenHandler.GotoScreen(MKSLCD_SCREEN_CHOOSE_FILE);
        #else 

        #endif 
    }
      
  }

  void DGUSScreenHandler::SDCardRemoved() {
    if (current_screen == DGUSLCD_SCREEN_SDFILELIST
        || (current_screen == DGUSLCD_SCREEN_CONFIRM && (ConfirmVP == VP_SD_AbortPrintConfirmed || ConfirmVP == VP_SD_FileSelectConfirm))
        || current_screen == DGUSLCD_SCREEN_SDPRINTMANIPULATION
    ) 
    {
      #if ENABLED(DGUS_LCD_UI_MKS)
        filelist.refresh();
        // ScreenHandler.GotoScreen(MKSLCD_SCREEN_NO_CHOOSE_FILE);
      #else 

      #endif
    }
  }

  void DGUSScreenHandler::SDCardError() {
    DGUSScreenHandler::SDCardRemoved();
    ScreenHandler.sendinfoscreen(PSTR("NOTICE"), nullptr, PSTR("SD card error"), nullptr, true, true, true, true);
    ScreenHandler.SetupConfirmAction(nullptr);
    ScreenHandler.GotoScreen(DGUSLCD_SCREEN_POPUP);
  }

#endif // SDSUPPORT

void DGUSScreenHandler::ScreenConfirmedOK(DGUS_VP_Variable &var, void *val_ptr) {
  DGUS_VP_Variable ramcopy;
  if (!populate_VPVar(ConfirmVP, &ramcopy)) return;
  if (ramcopy.set_by_display_handler) ramcopy.set_by_display_handler(ramcopy, val_ptr);
}

const uint16_t* DGUSLCD_FindScreenVPMapList(uint8_t screen) {
  const uint16_t *ret;
  const struct VPMapping *map = VPMap;
  while ((ret = (uint16_t*) pgm_read_ptr(&(map->VPList)))) {
    if (pgm_read_byte(&(map->screen)) == screen) return ret;
    map++;
  }
  return nullptr;
}

const DGUS_VP_Variable* DGUSLCD_FindVPVar(const uint16_t vp) {
  const DGUS_VP_Variable *ret = ListOfVP;
  do {
    const uint16_t vpcheck = pgm_read_word(&(ret->VP));
    if (vpcheck == 0) break;
    if (vpcheck == vp) return ret;
    ++ret;
  } while (1);

  DEBUG_ECHOLNPAIR("FindVPVar NOT FOUND ", vp);
  return nullptr;
}

void DGUSScreenHandler::ScreenChangeHookIfIdle(DGUS_VP_Variable &var, void *val_ptr) {
  if (!ExtUI::isPrinting()) {
    ScreenChangeHook(var, val_ptr);
    dgusdisplay.RequestScreen(current_screen);
  }
}

void DGUSScreenHandler::ScreenChangeHook(DGUS_VP_Variable &var, void *val_ptr) {
  uint8_t *tmp = (uint8_t*)val_ptr;

  // The keycode in target is coded as <from-frame><to-frame>, so 0x0100A means
  // from screen 1 (main) to 10 (temperature). DGUSLCD_SCREEN_POPUP is special,
  // meaning "return to previous screen"
  DGUSLCD_Screens target = (DGUSLCD_Screens)tmp[1];

  DEBUG_ECHOLNPAIR("\n DEBUG target",target);

  // #if ENABLED(DGUS_LCD_UI_MKS)
  // when the dgus had reboot,it will enter the DGUSLCD_SCREEN_MAIN page,
  // so user can change any page to use this function, an it will check 
  // if robin nano is printting? when it is, dgus will enter the printting  
  // page to continue print;  
  
  // if(print_job_timer.isRunning() || print_job_timer.isPaused())
  // {                                                   
  //     if ( (target = MKSLCD_PAUSE_SETTING_MOVE)
  //       || (target = MKSLCD_PAUSE_SETTING_EX)
  //       || (target = MKSLCD_SCREEN_PRINT)
  //       || (target = MKSLCD_SCREEN_PAUSE))
  //     {
  //     }
  //     else 
  //     {
  //         GotoScreen(MKSLCD_SCREEN_PRINT);  
  //     }                                                                        
  //     return ;                                        
  // } 
  // #endif 

  if (target == DGUSLCD_SCREEN_POPUP) {

    #if ENABLED(DGUS_LCD_UI_MKS)
      ScreenHandler.SetupConfirmAction(ExtUI::setUserConfirmed);
    #endif
    // special handling for popup is to return to previous menu
    if (current_screen == DGUSLCD_SCREEN_POPUP && confirm_action_cb) confirm_action_cb();
    PopToOldScreen();
    return;
  }

  UpdateNewScreen(target);

  #ifdef DEBUG_DGUSLCD
    if (!DGUSLCD_FindScreenVPMapList(target)) DEBUG_ECHOLNPAIR("WARNING: No screen Mapping found for ", target);
  #endif
}


#if ENABLED(DGUS_LCD_UI_MKS)

void DGUSScreenHandler::ScreenBackChange(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t target = swap16(*(uint16_t *)val_ptr);

  DEBUG_ECHOLNPAIR(" back = 0x%x", target);
  switch(target)
  {

  }
}

#endif


void DGUSScreenHandler::HandleAllHeatersOff(DGUS_VP_Variable &var, void *val_ptr) {
  thermalManager.disable_all_heaters();
  ScreenHandler.ForceCompleteUpdate(); // hint to send all data.
}

void DGUSScreenHandler::HandleTemperatureChanged(DGUS_VP_Variable &var, void *val_ptr) {
  uint16_t newvalue = swap16(*(uint16_t*)val_ptr);
  uint16_t acceptedvalue;

  switch (var.VP) {
    default: return;
    #if HOTENDS >= 1
      case VP_T_E0_Set:
        thermalManager.setTargetHotend(newvalue, 0);
        acceptedvalue = thermalManager.temp_hotend[0].target;
        break;
    #endif
    #if HOTENDS >= 2
      case VP_T_E1_Set:
        thermalManager.setTargetHotend(newvalue, 1);
        acceptedvalue = thermalManager.temp_hotend[1].target;
      break;
    #endif
    #if HAS_HEATED_BED
      case VP_T_Bed_Set:
        thermalManager.setTargetBed(newvalue);
        acceptedvalue = thermalManager.temp_bed.target;
        break;
    #endif
  }

  // reply to display the new value to update the view if the new value was rejected by the Thermal Manager.
  if (newvalue != acceptedvalue && var.send_to_display_handler) var.send_to_display_handler(var);
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
}

void DGUSScreenHandler::HandleFlowRateChanged(DGUS_VP_Variable &var, void *val_ptr) {
  #if EXTRUDERS
    uint16_t newvalue = swap16(*(uint16_t*)val_ptr);
    uint8_t target_extruder;
    switch (var.VP) {
      default: return;
      #if HOTENDS >= 1
        case VP_Flowrate_E0: target_extruder = 0; break;
      #endif
      #if HOTENDS >= 2
        case VP_Flowrate_E1: target_extruder = 1; break;
      #endif
    }

    planner.set_flow(target_extruder, newvalue);
    ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  #else
    UNUSED(var); UNUSED(val_ptr);
  #endif
}

void DGUSScreenHandler::HandleManualExtrude(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleManualExtrude");

  int16_t movevalue = swap16(*(uint16_t*)val_ptr);
  float target = movevalue * 0.01f;
  ExtUI::extruder_t target_extruder;

  switch (var.VP) {
    #if HOTENDS >= 1
      case VP_MOVE_E0: target_extruder = ExtUI::extruder_t::E0; break;
    #endif
    #if HOTENDS >= 2
      case VP_MOVE_E1: target_extruder = ExtUI::extruder_t::E1; break;
    #endif
    default: return;
  }

  target += ExtUI::getAxisPosition_mm(target_extruder);
  ExtUI::setAxisPosition_mm(target, target_extruder);
  skipVP = var.VP;
}

#if ENABLED(DGUS_UI_MOVE_DIS_OPTION)
  void DGUSScreenHandler::HandleManualMoveOption(DGUS_VP_Variable &var, void *val_ptr) {
    DEBUG_ECHOLNPGM("HandleManualMoveOption");
    *(uint16_t*)var.memadr = swap16(*(uint16_t*)val_ptr);
  }
#endif

#if ENABLED(DGUS_LCD_UI_MKS)



void DGUSScreenHandler::ZoffsetConfirm(DGUS_VP_Variable &var, void *val_ptr)
{
  gcode.mks_m500();
  ScreenHandler.GotoScreen(MKSLCD_SCREEN_PRINT);
}

void DGUSScreenHandler::GetZoffsetDistance(DGUS_VP_Variable &var, void *val_ptr)
{
  DEBUG_ECHOLNPGM("Zoffset DistanceChange");
  uint16_t value = swap16(*(uint16_t *)val_ptr);
  float val_distance = 0;
  switch(value)
  {
    case 0: val_distance = 0.01; break;
    case 1: val_distance = 0.1; break;
    case 2: val_distance = 0.5; break;
    default:
      val_distance = 0.01;
    break;
  }
  ZOffset_distance = val_distance;
}


void DGUSScreenHandler::GetManualMovestep(DGUS_VP_Variable &var, void *val_ptr)
{
  *(uint16_t *)var.memadr = swap16(*(uint16_t *)val_ptr);

  DEBUG_ECHOLNPGM("DistanceChange");
}


void DGUSScreenHandler::EEPROM_CTRL(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t eep_flag = swap16(*(uint16_t *)val_ptr);

  switch(eep_flag)
  {
    case 0: 
    
      gcode.mks_m500();
      GotoScreen(MKSLCD_SCREEN_EEP_Config);
    break;

    case 1:
      gcode.mks_m501();
      gcode.mks_m502();
      GotoScreen(MKSLCD_SCREEN_EEP_Config);
    break;

    default:
    break;
  }
}

void DGUSScreenHandler::LanguageChange_MKS(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t lag_flag = swap16(*(uint16_t *)val_ptr);

  switch(lag_flag)
  {
    case 0:
      DGUS_LanguageDisplay(1);
    break;

    case 1:
      DGUS_LanguageDisplay(0);
    break;

    default:
    break;
  }
}





void DGUSScreenHandler::Level_Ctrl_MKS(DGUS_VP_Variable &var, void *val_ptr)
{
  
  #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    // char level_buf[20] = "AutoLevel";
    char cmd_buf[10];
    static uint8_t a_first_level = 1;
  
    if(a_first_level == 1)
    {
      a_first_level = 0;

      sprintf(cmd_buf,"G28\n");
      queue.enqueue_one_now(cmd_buf);
    }

    // dgusdisplay.WriteVariable(0x5A50, level_buf, 9, true);
    sprintf(cmd_buf,"G29\n");
    queue.enqueue_one_now(cmd_buf);

  #elif ENABLED(MESH_BED_LEVELING)
    auto cs = ScreenHandler.getCurrentScreen();
    if(cs != MKSLCD_SCREEN_LEVEL ) ScreenHandler.GotoScreen(MKSLCD_AUTO_LEVEL);

  #else
    // char level_buf[20] = "Level";
    ScreenHandler.GotoScreen(MKSLCD_SCREEN_LEVEL); 
    // dgusdisplay.WriteVariable(0x5A50, level_buf, 10, true);
  #endif
}



void DGUSScreenHandler::MeshLevelDistanceConfig(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t mesh_dist = swap16(*(uint16_t *)val_ptr);

  switch(mesh_dist)
  {
    case 0:
      mesh_adj_distance = 0.01;
    break;

    case 1:
      mesh_adj_distance = 0.1;
    break;

    case 2:
      mesh_adj_distance = 0.5;
    break;

    default:
      mesh_adj_distance = 0.1;
    break;
  }



}

void DGUSScreenHandler::MeshLevel(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t mesh_value = swap16(*(uint16_t *)val_ptr);
  static uint8_t a_first_level = 1;
  char cmd_buf[30];
  float offset = ZOffset_distance;
  uint8_t point_count = 0;

  switch(mesh_value)
  {
    case 0:
      sprintf(cmd_buf,"G91\nG1 Z%.3f\nG90\n",offset);
      queue.enqueue_one_now(cmd_buf);
    break;

    case 1:
      sprintf(cmd_buf,"G91\nG1 Z-%.3f\nG90\n",offset);
      queue.enqueue_one_now(cmd_buf);
    break;

    case 2:
      if (a_first_level == 1)
      {
        a_first_level = 0;
        sprintf(cmd_buf,"G28\n");
        queue.enqueue_one_now(cmd_buf);
      }
      sprintf(cmd_buf,"G29\n");
      queue.enqueue_one_now(cmd_buf);
      point_count++;

      if(point_count != 6)
      {
        char level_buf[20] = "Next Point";
        dgusdisplay.WriteVariable(0x5A50, level_buf, 9, true);
      }
      else 
      {
        point_count == 0;
        char level_buf1[20] = "Finsh";
        dgusdisplay.WriteVariable(0x5A50, level_buf1, 9, true);
      }
    break;

    default:
    break;
  }
}




void DGUSScreenHandler::ManualAssistLeveling(DGUS_VP_Variable &var, void *val_ptr)
{
  int16_t point_value = swap16(*(uint16_t *)val_ptr);

  int16_t level_x_pos, level_y_pos;
  char buf_level[32] = {0};
  unsigned int level_speed = 1500;
  static uint8_t first_level_flag = 1;


  if(first_level_flag == 1)
  {
    sprintf(buf_level, PSTR("G28"));
    queue.enqueue_one_now(buf_level);
  }

  switch(point_value)
  {
    case 0x0001:
      if(first_level_flag != 1)
      {
        sprintf(buf_level, PSTR("G28"));
        queue.enqueue_one_now(buf_level);
      }
      sprintf(buf_level, PSTR("G1 Z10"));
      queue.enqueue_one_now(buf_level);
      // level_x_pos = X_MIN_POS + 20;
      // level_y_pos = Y_MIN_POS + 20;
      level_x_pos = X_MIN_POS + abs(level_1_x_point);
      level_y_pos = Y_MIN_POS + abs(level_1_y_point);

      memset(buf_level, 0, sizeof(buf_level));
      sprintf_P(buf_level, "G0 X%d Y%d F%d", level_x_pos, level_y_pos, level_speed);
      queue.enqueue_one_now(buf_level);
      sprintf(buf_level, PSTR("G28 Z"));
      queue.enqueue_one_now(buf_level); 
      break;
    case 0x0002:
    sprintf(buf_level, PSTR("G1 Z10"));
    queue.enqueue_one_now(buf_level);

    // level_x_pos = X_MAX_POS - 20;
    // level_y_pos = Y_MIN_POS + 20;

    level_x_pos = X_MAX_POS - abs(level_2_x_point);
    level_y_pos = Y_MIN_POS + abs(level_2_y_point);

    sprintf_P(buf_level, "G0 X%d Y%d F%d", level_x_pos, level_y_pos, level_speed);
    queue.enqueue_one_now(buf_level);
    // sprintf(buf_level, PSTR("G28 Z"));
    // queue.enqueue_one_now(buf_level);
    sprintf(buf_level, PSTR("G1 Z-10"));
    queue.enqueue_one_now(buf_level);
    break;
   case 0x0003:
    sprintf(buf_level, PSTR("G1 Z10"));
    queue.enqueue_one_now(buf_level);

    // level_x_pos = X_MAX_POS - 20;
    // level_y_pos = Y_MAX_POS - 20;

    level_x_pos = X_MAX_POS - abs(level_3_x_point);
    level_y_pos = Y_MAX_POS - abs(level_3_y_point); 

    sprintf_P(buf_level, "G0 X%d Y%d F%d", level_x_pos, level_y_pos, level_speed);
    queue.enqueue_one_now(buf_level);
    // sprintf(buf_level, PSTR("G28 Z"));
    sprintf(buf_level, PSTR("G1 Z-10"));
    queue.enqueue_one_now(buf_level);
    break;
   case 0x0004:
    sprintf(buf_level, PSTR("G1 Z10"));
    queue.enqueue_one_now(buf_level);

    // level_x_pos = X_MIN_POS + 20;
    // level_y_pos = Y_MAX_POS - 20;
    level_x_pos = X_MIN_POS + abs(level_4_x_point);
    level_y_pos = Y_MAX_POS - abs(level_4_y_point);  

    sprintf_P(buf_level, "G0 X%d Y%d F%d", level_x_pos, level_y_pos, level_speed);
    queue.enqueue_one_now(buf_level);
    // sprintf(buf_level, PSTR("G28 Z"));
    sprintf(buf_level, PSTR("G1 Z-10"));
    queue.enqueue_one_now(buf_level);
    break;
   case 0x0005:
    sprintf(buf_level, PSTR("G1 Z10"));
    queue.enqueue_one_now(buf_level);
    // level_x_pos = (uint16_t)(X_MAX_POS / 2);
    // level_y_pos = (uint16_t)(Y_MAX_POS / 2);
    level_x_pos = abs(level_5_x_point);
    level_y_pos = abs(level_5_y_point); 

    sprintf_P(buf_level, "G0 X%d Y%d F%d", level_x_pos, level_y_pos, level_speed);
    queue.enqueue_one_now(buf_level);
    // sprintf(buf_level, PSTR("G28 Z"));
    sprintf(buf_level, PSTR("G1 Z-10"));
    queue.enqueue_one_now(buf_level);
    break;
  }

    /* Only once */
    if(first_level_flag == 1)
      first_level_flag = 0;
}
#endif 

#if ENABLED(DGUS_LCD_UI_MKS)
void DGUSScreenHandler::HandleManualMove(DGUS_VP_Variable &var, void *val_ptr) {

  int16_t movevalue = swap16(*(uint16_t*)val_ptr);

  DEBUG_ECHOLNPGM("HandleManualMove");
  char axiscode;
  unsigned int speed = 1500;  //FIXME: get default feedrate for manual moves, dont hardcode.
  /* Choose Move distance */

  if (distanceMove == 0x01)
    distanceMove = 10;
  else if (distanceMove == 0x02)
    distanceMove = 100;
  else if (distanceMove == 0x03)
    distanceMove = 1000;

  switch (var.VP) {     // switch X Y Z or Home
    default: return;
    case VP_MOVE_X:
      DEBUG_ECHOLNPGM("X Move");
      axiscode = 'X';
      if (!ExtUI::canMove(ExtUI::axis_t::X)) goto cannotmove;
      break;

    case VP_MOVE_Y:
      DEBUG_ECHOLNPGM("Y Move");
      axiscode = 'Y';
      if (!ExtUI::canMove(ExtUI::axis_t::Y)) goto cannotmove;
      break;

    case VP_MOVE_Z:
      DEBUG_ECHOLNPGM("Z Move");
      axiscode = 'Z';
      speed = 300; // default to 5mm/s
      if (!ExtUI::canMove(ExtUI::axis_t::Z)) goto cannotmove;
    break;

    case VP_MOTOR_LOCK_UNLOK:
      DEBUG_ECHOLNPGM("Motor Unlock");
      // gcode.motor_unlock();
      movevalue = 5;
      axiscode = '\0';
      // return ;
    break;

    case VP_HOME_ALL: // only used for homing
      DEBUG_ECHOLNPGM("Home all");
      axiscode = '\0';
      movevalue = 0; // ignore value sent from display, this VP is _ONLY_ for homing.
        // gcode.motor_all_back();
        // return ;
      break;
                       
    case VP_X_HOME:
      DEBUG_ECHOLNPGM("X Home");
      axiscode = 'X';
      movevalue = 0;
    break;

    case VP_Y_HOME:
      DEBUG_ECHOLNPGM("Y Home");
      axiscode = 'Y';
      movevalue = 0;
    break;

    case VP_Z_HOME:
      DEBUG_ECHOLNPGM("Z Home");
      axiscode = 'Z';
      movevalue = 0;
    break;           
  }
  
  DEBUG_ECHOPAIR("movevalue = ",movevalue);
  if ((movevalue != 0) && (movevalue != 5))   // get move distance
  {
    switch (movevalue)
    {
      case 0x0001: movevalue = 0 + distanceMove;  
      break;
      case 0x0002: movevalue = 0 - distanceMove;
      break;
      
      default:
          movevalue = 0 ;
      break;
    }
  }

  if (!movevalue) {
    // homing
    DEBUG_ECHOPAIR(" homing ", axiscode);
    // char buf[6] = "G28 X";
    // buf[4] = axiscode;
    char buf[6];
    sprintf(buf,"G28 %c",axiscode);
    //DEBUG_ECHOPAIR(" ", buf);
    queue.enqueue_one_now(buf);
    //DEBUG_ECHOLNPGM(" ✓");
    ScreenHandler.ForceCompleteUpdate();
    return;
  }
  else if(movevalue == 5)
  {
    DEBUG_ECHOPAIR("send M84");
    char buf[6];
    sprintf(buf,"M84 %c",axiscode);
    queue.enqueue_one_now(buf);
    ScreenHandler.ForceCompleteUpdate();
    return ;
  }
  else {
    //movement
    DEBUG_ECHOPAIR(" move ", axiscode);
    bool old_relative_mode = relative_mode;

    if (!relative_mode) {
      //DEBUG_ECHOPGM(" G91");
      queue.enqueue_now_P(PSTR("G91"));
      //DEBUG_ECHOPGM(" ✓ ");
    }
    char buf[32];  // G1 X9999.99 F12345
    unsigned int backup_speed = MMS_TO_MMM(feedrate_mm_s);
    char sign[]="\0";
    int16_t value = movevalue / 100;
    if (movevalue < 0) { value = -value; sign[0] = '-'; }
    int16_t fraction = ABS(movevalue) % 100;
    snprintf_P(buf, 32, PSTR("G0 %c%s%d.%02d F%d"), axiscode, sign, value, fraction, speed);
    //DEBUG_ECHOPAIR(" ", buf);
    queue.enqueue_one_now(buf);
    //DEBUG_ECHOLNPGM(" ✓ ");
    if (backup_speed != speed) {
      snprintf_P(buf, 32, PSTR("G0 F%d"), backup_speed);
      queue.enqueue_one_now(buf);
      //DEBUG_ECHOPAIR(" ", buf);
    }
    //while (!enqueue_and_echo_command(buf)) idle();
    //DEBUG_ECHOLNPGM(" ✓ ");
    if (!old_relative_mode) {
      //DEBUG_ECHOPGM("G90");
      queue.enqueue_now_P(PSTR("G90"));
      //DEBUG_ECHOPGM(" ✓ ");
    }
  }

  ScreenHandler.ForceCompleteUpdate();
  DEBUG_ECHOLNPGM("manmv done.");
  return;
  cannotmove:
  DEBUG_ECHOLNPAIR(" cannot move ", axiscode);
  return;
}
#else
void DGUSScreenHandler::HandleManualMove(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleManualMove");

  int16_t movevalue = swap16(*(uint16_t*)val_ptr);
  #if ENABLED(DGUS_UI_MOVE_DIS_OPTION)
    if (movevalue) {
      const uint16_t choice = *(uint16_t*)var.memadr;
      movevalue = movevalue < 0 ? -choice : choice;
    }
  #endif
  char axiscode;
  unsigned int speed = 1500;  //FIXME: get default feedrate for manual moves, dont hardcode.

  switch (var.VP) {
    default: return;

    case VP_MOVE_X:
      axiscode = 'X';
      if (!ExtUI::canMove(ExtUI::axis_t::X)) goto cannotmove;
      break;

    case VP_MOVE_Y:
      axiscode = 'Y';
      if (!ExtUI::canMove(ExtUI::axis_t::Y)) goto cannotmove;
      break;

    case VP_MOVE_Z:
      axiscode = 'Z';
      speed = 300; // default to 5mm/s
      if (!ExtUI::canMove(ExtUI::axis_t::Z)) goto cannotmove;
      break;

    case VP_HOME_ALL: // only used for homing
      axiscode = '\0';
      movevalue = 0; // ignore value sent from display, this VP is _ONLY_ for homing.
      break;
  }

  if (!movevalue) {
    // homing
    DEBUG_ECHOPAIR(" homing ", axiscode);
    char buf[6] = "G28 X";
    buf[4] = axiscode;
    //DEBUG_ECHOPAIR(" ", buf);
    queue.enqueue_one_now(buf);
    //DEBUG_ECHOLNPGM(" ✓");
    ScreenHandler.ForceCompleteUpdate();
    return;
  }
  else {
    //movement
    DEBUG_ECHOPAIR(" move ", axiscode);
    bool old_relative_mode = relative_mode;
    if (!relative_mode) {
      //DEBUG_ECHOPGM(" G91");
      queue.enqueue_now_P(PSTR("G91"));
      //DEBUG_ECHOPGM(" ✓ ");
    }
    char buf[32];  // G1 X9999.99 F12345
    unsigned int backup_speed = MMS_TO_MMM(feedrate_mm_s);
    char sign[]="\0";
    int16_t value = movevalue / 100;
    if (movevalue < 0) { value = -value; sign[0] = '-'; }
    int16_t fraction = ABS(movevalue) % 100;
    snprintf_P(buf, 32, PSTR("G0 %c%s%d.%02d F%d"), axiscode, sign, value, fraction, speed);
    //DEBUG_ECHOPAIR(" ", buf);
    queue.enqueue_one_now(buf);
    //DEBUG_ECHOLNPGM(" ✓ ");
    if (backup_speed != speed) {
      snprintf_P(buf, 32, PSTR("G0 F%d"), backup_speed);
      queue.enqueue_one_now(buf);
      //DEBUG_ECHOPAIR(" ", buf);
    }
    //while (!enqueue_and_echo_command(buf)) idle();
    //DEBUG_ECHOLNPGM(" ✓ ");
    if (!old_relative_mode) {
      //DEBUG_ECHOPGM("G90");
      queue.enqueue_now_P(PSTR("G90"));
      //DEBUG_ECHOPGM(" ✓ ");
    }
  }

  ScreenHandler.ForceCompleteUpdate();
  DEBUG_ECHOLNPGM("manmv done.");
  return;

  cannotmove:
  DEBUG_ECHOLNPAIR(" cannot move ", axiscode);
  return;
}
#endif

void DGUSScreenHandler::HandleMotorLockUnlock(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleMotorLockUnlock");

  char buf[4];
  const int16_t lock = swap16(*(uint16_t*)val_ptr);
  strcpy_P(buf, lock ? PSTR("M18") : PSTR("M17"));

  //DEBUG_ECHOPAIR(" ", buf);
  queue.enqueue_one_now(buf);
}

#if ENABLED(POWER_LOSS_RECOVERY)

  void DGUSScreenHandler::HandlePowerLossRecovery(DGUS_VP_Variable &var, void *val_ptr) {
    uint16_t value = swap16(*(uint16_t*)val_ptr);
    if (value) {
      queue.inject_P(PSTR("M1000"));
      ScreenHandler.GotoScreen(DGUSLCD_SCREEN_SDPRINTMANIPULATION);
    }
    else {
      recovery.cancel();
      ScreenHandler.GotoScreen(DGUSLCD_SCREEN_STATUS);
    }
  }

#endif

void DGUSScreenHandler::HandleSettings(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleSettings");
  uint16_t value = swap16(*(uint16_t*)val_ptr);
  switch (value) {
    default: break;
    case 1:
      TERN_(PRINTCOUNTER, print_job_timer.initStats());
      queue.inject_P(PSTR("M502\nM500"));
      break;
    case 2: queue.inject_P(PSTR("M501")); break;
    case 3: queue.inject_P(PSTR("M500")); break;
  }
}

void DGUSScreenHandler::HandleStepPerMMChanged(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleStepPerMMChanged");

  uint16_t value_raw = swap16(*(uint16_t*)val_ptr);
  DEBUG_ECHOLNPAIR("value_raw:", value_raw);
  float value = (float)value_raw/10;
  ExtUI::axis_t axis;
  switch (var.VP) {
    case VP_X_STEP_PER_MM: axis = ExtUI::axis_t::X; break;
    case VP_Y_STEP_PER_MM: axis = ExtUI::axis_t::Y; break;
    case VP_Z_STEP_PER_MM: axis = ExtUI::axis_t::Z; break;
    default: return;
  }
  DEBUG_ECHOLNPAIR_F("value:", value);
  ExtUI::setAxisSteps_per_mm(value, axis);
  DEBUG_ECHOLNPAIR_F("value_set:", ExtUI::getAxisSteps_per_mm(axis));
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

#if ENABLED(DGUS_LCD_UI_MKS)

void DGUSScreenHandler::GetPrintTime_MKS(DGUS_VP_Variable &var, void *val_ptr) 
{

}

void DGUSScreenHandler::GetParkPos_MKS(DGUS_VP_Variable &var, void *val_ptr) 
{
  uint16_t value_pos = swap16(*(int16_t*)val_ptr);

  switch(var.VP)
  {
    case VP_X_PARK_POS:
      x_park_pos = value_pos;
    break;
    case VP_Y_PARK_POS:
      y_park_pos = value_pos;
    break;
    case VP_Z_PARK_POS:
      z_park_pos = value_pos;
    break;
    default:
    break;
  }
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return ;
}

void DGUSScreenHandler::HandleChangeLevelPoint_MKS(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleStepPerMMChanged");

  int16_t value_raw = swap16(*(int16_t*)val_ptr);
  float value = (float)value_raw;

  DEBUG_ECHOLNPAIR_F("value:", value);

  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}
  

void DGUSScreenHandler::HandleStepPerMMChanged_MKS(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleStepPerMMChanged");

  uint16_t value_raw = swap16(*(uint16_t*)val_ptr);

  DEBUG_ECHOLNPAIR("value_raw:", value_raw);
  float value = (float)value_raw;
  ExtUI::axis_t axis;
  switch (var.VP) {
    case VP_X_STEP_PER_MM: axis = ExtUI::axis_t::X; break;
    case VP_Y_STEP_PER_MM: axis = ExtUI::axis_t::Y; break;
    case VP_Z_STEP_PER_MM: axis = ExtUI::axis_t::Z; break;
    default: return;
  }
  DEBUG_ECHOLNPAIR_F("value:", value);
  ExtUI::setAxisSteps_per_mm(value, axis);
  DEBUG_ECHOLNPAIR_F("value_set:", ExtUI::getAxisSteps_per_mm(axis));
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleStepPerMMExtruderChanged_MKS(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleStepPerMMExtruderChanged");

  uint16_t value_raw = swap16(*(uint16_t*)val_ptr);
  DEBUG_ECHOLNPAIR("value_raw:", value_raw);
  float value = (float)value_raw;
  ExtUI::extruder_t extruder;
  switch (var.VP) {
    default: return;
    #if HOTENDS >= 1
      case VP_E0_STEP_PER_MM: extruder = ExtUI::extruder_t::E0; break;
    #endif
    #if HOTENDS >= 2
    #endif
      case VP_E1_STEP_PER_MM: extruder = ExtUI::extruder_t::E1; break;
  }
  DEBUG_ECHOLNPAIR_F("value:", value);
  ExtUI::setAxisSteps_per_mm(value,extruder);
  DEBUG_ECHOLNPAIR_F("value_set:", ExtUI::getAxisSteps_per_mm(extruder));
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleMaxSpeedChange_MKS(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleMaxSpeedChange_MKS");

  uint16_t value_raw = swap16(*(uint16_t*)val_ptr);
  DEBUG_ECHOLNPAIR("value_raw:", value_raw);
  float value = (float)value_raw;
  ExtUI::axis_t axis;
  switch (var.VP) {
    case VP_X_MAX_SPEED: axis = ExtUI::axis_t::X; break;
    case VP_Y_MAX_SPEED: axis = ExtUI::axis_t::Y; break;
    case VP_Z_MAX_SPEED: axis = ExtUI::axis_t::Z; break;
    default : return;
  }
  DEBUG_ECHOLNPAIR_F("value:", value);
  // ExtUI::setAxisSteps_per_mm(value,extruder);
  ExtUI::setAxisMaxFeedrate_mm_s(value, axis);
  DEBUG_ECHOLNPAIR_F("value_set:", ExtUI::getAxisMaxFeedrate_mm_s(axis)); 
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleExtruderMaxSpeedChange_MKS(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleMaxSpeedChange_MKS");

  uint16_t value_raw = swap16(*(uint16_t*)val_ptr);
  DEBUG_ECHOLNPAIR("value_raw:", value_raw);
  float value = (float)value_raw;
  ExtUI::extruder_t extruder;
  switch (var.VP) {
    default: return;
    #if HOTENDS >= 1
      case VP_E0_MAX_SPEED: extruder = ExtUI::extruder_t::E0; break;
    #endif
    #if HOTENDS >= 2
    #endif
      case VP_E1_MAX_SPEED: extruder = ExtUI::extruder_t::E1; break;
  }
  DEBUG_ECHOLNPAIR_F("value:", value);
  // ExtUI::setAxisSteps_per_mm(value,extruder);
  ExtUI::setAxisMaxFeedrate_mm_s(value, extruder);
  DEBUG_ECHOLNPAIR_F("value_set:", ExtUI::getAxisMaxFeedrate_mm_s(extruder)); 
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}


void DGUSScreenHandler::HandleMaxAccChange_MKS(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleMaxSpeedChange_MKS");

  uint16_t value_raw = swap16(*(uint16_t*)val_ptr);
  DEBUG_ECHOLNPAIR("value_raw:", value_raw);
  float value = (float)value_raw;
  ExtUI::axis_t axis;
  switch (var.VP) {
    case VP_X_ACC_MAX_SPEED: axis = ExtUI::axis_t::X; break;
    case VP_Y_ACC_MAX_SPEED: axis = ExtUI::axis_t::Y; break;
    case VP_Z_ACC_MAX_SPEED: axis = ExtUI::axis_t::Z; break;
    default : return;
  }
  DEBUG_ECHOLNPAIR_F("value:", value);
  ExtUI::setAxisMaxAcceleration_mm_s2(value,axis);
  DEBUG_ECHOLNPAIR_F("value_set:", ExtUI::getAxisMaxAcceleration_mm_s2(axis)); 
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleExtruderAccChange_MKS(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleMaxSpeedChange_MKS");

  uint16_t value_raw = swap16(*(uint16_t*)val_ptr);
  DEBUG_ECHOLNPAIR("value_raw:", value_raw);
  float value = (float)value_raw;
  ExtUI::extruder_t extruder;
  switch (var.VP) {
    default: return;
    #if HOTENDS >= 1
      case VP_E0_ACC_MAX_SPEED: extruder = ExtUI::extruder_t::E0; break;
    #endif
    #if HOTENDS >= 2
    #endif
      case VP_E1_ACC_MAX_SPEED: extruder = ExtUI::extruder_t::E1; break;
  }
  DEBUG_ECHOLNPAIR_F("value:", value);
  // ExtUI::setAxisSteps_per_mm(value,extruder);
  ExtUI::setAxisMaxAcceleration_mm_s2(value, extruder);
  DEBUG_ECHOLNPAIR_F("value_set:", ExtUI::getAxisMaxAcceleration_mm_s2(extruder)); 
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleTravelAccChange_MKS(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t value_travel = swap16(*(uint16_t*)val_ptr);
  float value = (float)value_travel;
  planner.settings.travel_acceleration = value;
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleFeedRateMinChange_MKS(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t value_t = swap16(*(uint16_t*)val_ptr);
  float value = (float)value_t;
  planner.settings.min_feedrate_mm_s = value;
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleMin_T_F_MKS(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t value_t_f = swap16(*(uint16_t*)val_ptr);
  float value = (float)value_t_f;
  planner.settings.min_travel_feedrate_mm_s = value;
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleAccChange_MKS(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t value_acc = swap16(*(uint16_t*)val_ptr);
  float value = (float)value_acc;
  planner.settings.acceleration = value;
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::HandleGetExMinTemp_MKS(DGUS_VP_Variable &var, void *val_ptr)
{
  uint16_t value_ex_min_temp = swap16(*(uint16_t*)val_ptr);
  float value = (float)value_ex_min_temp;
  thermalManager.extrude_min_temp = value;
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

#endif

void DGUSScreenHandler::HandleStepPerMMExtruderChanged(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleStepPerMMExtruderChanged");

  uint16_t value_raw = swap16(*(uint16_t*)val_ptr);
  DEBUG_ECHOLNPAIR("value_raw:", value_raw);
  float value = (float)value_raw/10;
  ExtUI::extruder_t extruder;
  switch (var.VP) {
    default: return;
    #if HOTENDS >= 1
      case VP_E0_STEP_PER_MM: extruder = ExtUI::extruder_t::E0; break;
    #endif
    #if HOTENDS >= 2
      case VP_E1_STEP_PER_MM: extruder = ExtUI::extruder_t::E1; break;
    #endif
  }
  DEBUG_ECHOLNPAIR_F("value:", value);
  ExtUI::setAxisSteps_per_mm(value,extruder);
  DEBUG_ECHOLNPAIR_F("value_set:", ExtUI::getAxisSteps_per_mm(extruder));
  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

#if HAS_PID_HEATING
  void DGUSScreenHandler::HandleTemperaturePIDChanged(DGUS_VP_Variable &var, void *val_ptr) {
    uint16_t rawvalue = swap16(*(uint16_t*)val_ptr);
    DEBUG_ECHOLNPAIR("V1:", rawvalue);
    float value = (float)rawvalue / 10;
    DEBUG_ECHOLNPAIR("V2:", value);
    float newvalue = 0;

    switch (var.VP) {
      default: return;
      #if HOTENDS >= 1
        case VP_E0_PID_P: newvalue = value; break;
        case VP_E0_PID_I: newvalue = scalePID_i(value); break;
        case VP_E0_PID_D: newvalue = scalePID_d(value); break;
      #endif
      #if HOTENDS >= 2
        case VP_E1_PID_P: newvalue = value; break;
        case VP_E1_PID_I: newvalue = scalePID_i(value); break;
        case VP_E1_PID_D: newvalue = scalePID_d(value); break;
      #endif
      #if HAS_HEATED_BED
        case VP_BED_PID_P: newvalue = value; break;
        case VP_BED_PID_I: newvalue = scalePID_i(value); break;
        case VP_BED_PID_D: newvalue = scalePID_d(value); break;
      #endif
    }

    DEBUG_ECHOLNPAIR_F("V3:", newvalue);
    *(float *)var.memadr = newvalue;
    ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  }

  void DGUSScreenHandler::HandlePIDAutotune(DGUS_VP_Variable &var, void *val_ptr) {
    DEBUG_ECHOLNPGM("HandlePIDAutotune");

    char buf[32] = {0};

    switch (var.VP) {
      default: break;
      #if ENABLED(PIDTEMP)
        #if HOTENDS >= 1
          case VP_PID_AUTOTUNE_E0: // Autotune Extruder 0
            sprintf(buf, "M303 E%d C5 S210 U1", ExtUI::extruder_t::E0);
            break;
        #endif
        #if HOTENDS >= 2
          case VP_PID_AUTOTUNE_E1:
            sprintf(buf, "M303 E%d C5 S210 U1", ExtUI::extruder_t::E1);
            break;
        #endif
      #endif
      #if ENABLED(PIDTEMPBED)
        case VP_PID_AUTOTUNE_BED:
          sprintf(buf, "M303 E-1 C5 S70 U1");
          break;
      #endif
    }

    if (buf[0]) queue.enqueue_one_now(buf);

    #if ENABLED(DGUS_UI_WAITING)
      sendinfoscreen(PSTR("PID is autotuning"), PSTR("please wait"), NUL_STR, NUL_STR, true, true, true, true);
      GotoScreen(DGUSLCD_SCREEN_WAITING);
    #endif
  }
#endif

#if HAS_BED_PROBE
  #if !ENABLED(DGUS_LCD_UI_MKS)
    void DGUSScreenHandler::HandleProbeOffsetZChanged(DGUS_VP_Variable &var, void *val_ptr) {
      DEBUG_ECHOLNPGM("HandleProbeOffsetZChanged");

      const float offset = float(int16_t(swap16(*(uint16_t*)val_ptr))) / 100.0f;
      ExtUI::setZOffset_mm(offset);
      ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
      return;
    }
  #else 

  void DGUSScreenHandler::HandleProbeOffsetZChanged_MKS(DGUS_VP_Variable &var, void *val_ptr) {

    DEBUG_ECHOLNPGM("HandleProbeOffsetZChanged");

    // const float offset = float(int16_t(swap16(*(uint16_t*)val_ptr))) / 100.0f;
    uint16_t value = swap16(*(uint16_t*)val_ptr);

    float offset = ZOffset_distance;

    switch(value)
    {
      case 1:
          offset = offset;
      break;

      case 2:
          offset = -offset;
      break;

      default:
      break;
    }

    DEBUG_ECHOLNPAIR_F("Offset = ", offset);
    ExtUI::setZOffset_mm(offset);

    ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
    return;
  }
  #endif 
#endif

#if ENABLED(BABYSTEPPING)
  #if ENABLED(DGUS_LCD_UI_MKS)
  void DGUSScreenHandler::HandleLiveAdjustZ(DGUS_VP_Variable &var, void *val_ptr) {
    DEBUG_ECHOLNPGM("HandleLiveAdjustZ");

    int16_t flag = swap16(*(uint16_t*)val_ptr);
    int16_t steps = flag ? -20 : 20;
    ExtUI::smartAdjustAxis_steps(steps, ExtUI::axis_t::Z, true);
    ScreenHandler.ForceCompleteUpdate();
    return;
  }
  #elif
  void DGUSScreenHandler::HandleLiveAdjustZ(DGUS_VP_Variable &var, void *val_ptr) {
    DEBUG_ECHOLNPGM("HandleLiveAdjustZ");
    int16_t flag = swap16(*(uint16_t*)val_ptr);
    char babystep_buf[30];

    sprintf_P(babystep_buf,PSTR("M290 X%.3f",ZOffset_distance));
    gcode.process_subcommands_now_P(PSTR(babystep_buf));
    ScreenHandler.ForceCompleteUpdate();
    return;
  }
  #endif
#endif

#if HAS_FAN
  void DGUSScreenHandler::HandleFanControl(DGUS_VP_Variable &var, void *val_ptr) {
    DEBUG_ECHOLNPGM("HandleFanControl");
    *(uint8_t*)var.memadr = *(uint8_t*)var.memadr > 0 ? 0 : 255;

  }
#endif

void DGUSScreenHandler::HandleHeaterControl(DGUS_VP_Variable &var, void *val_ptr) {
  DEBUG_ECHOLNPGM("HandleHeaterControl");

  uint8_t preheat_temp = 0;
  switch (var.VP) {
    #if HOTENDS >= 1
      case VP_E0_CONTROL:
    #endif
    #if HOTENDS >= 2
      case VP_E1_CONTROL:
    #endif
    #if HOTENDS >= 3
      case VP_E2_CONTROL:
    #endif
      preheat_temp = PREHEAT_1_TEMP_HOTEND;
      break;

    case VP_BED_CONTROL:
      preheat_temp = PREHEAT_1_TEMP_BED;
      break;
  }

  *(int16_t*)var.memadr = *(int16_t*)var.memadr > 0 ? 0 : preheat_temp;
}

#if ENABLED(DGUS_PREHEAT_UI)

  void DGUSScreenHandler::HandlePreheat(DGUS_VP_Variable &var, void *val_ptr) {
    DEBUG_ECHOLNPGM("HandlePreheat");

    uint8_t e_temp = 0;
    TERN_(HAS_HEATED_BED, uint8_t bed_temp = 0);
    const uint16_t preheat_option = swap16(*(uint16_t*)val_ptr);
    switch (preheat_option) {
      default:
      case 0: // Preheat PLA
        #if defined(PREHEAT_1_TEMP_HOTEND) && defined(PREHEAT_1_TEMP_BED)
          e_temp = PREHEAT_1_TEMP_HOTEND;
          TERN_(HAS_HEATED_BED, bed_temp = PREHEAT_1_TEMP_BED);
        #endif
        break;
      case 1: // Preheat ABS
        #if defined(PREHEAT_2_TEMP_HOTEND) && defined(PREHEAT_2_TEMP_BED)
          e_temp = PREHEAT_2_TEMP_HOTEND;
          TERN_(HAS_HEATED_BED, bed_temp = PREHEAT_2_TEMP_BED);
        #endif
        break;
      case 2: // Preheat PET
        #if defined(PREHEAT_3_TEMP_HOTEND) && defined(PREHEAT_3_TEMP_BED)
          e_temp = PREHEAT_3_TEMP_HOTEND;
          TERN_(HAS_HEATED_BED, bed_temp = PREHEAT_3_TEMP_BED);
        #endif
        break;
      case 3: // Preheat FLEX
        #if defined(PREHEAT_4_TEMP_HOTEND) && defined(PREHEAT_4_TEMP_BED)
          e_temp = PREHEAT_4_TEMP_HOTEND;
          TERN_(HAS_HEATED_BED, bed_temp = PREHEAT_4_TEMP_BED);
        #endif
        break;
      case 7: break; // Custom preheat
      case 9: break; // Cool down
    }

    switch (var.VP) {
      default: return;
      #if HOTENDS >= 1
        case VP_E0_BED_PREHEAT:
          thermalManager.setTargetHotend(e_temp, 0);
          TERN_(HAS_HEATED_BED, thermalManager.setTargetBed(bed_temp));
          break;
      #endif
      #if HOTENDS >= 2
        case VP_E1_BED_PREHEAT:
          thermalManager.setTargetHotend(e_temp, 1);
          TERN_(HAS_HEATED_BED, thermalManager.setTargetBed(bed_temp));
        break;
      #endif
    }

    // Go to the preheat screen to show the heating progress
    GotoScreen(DGUSLCD_SCREEN_PREHEAT);
  }

#endif


#if ENABLED(DGUS_LCD_UI_MKS)
void DGUSScreenHandler::GetManualFilament(DGUS_VP_Variable &var, void *val_ptr)
{
  DEBUG_ECHOLNPGM("HandleGetFilament");

  uint16_t value_len = swap16(*(uint16_t*)val_ptr);

  float value = (float)value_len;

  DEBUG_ECHOLNPAIR_F("Get Filament len value:", value);
  distanceFilament = value;

  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}

void DGUSScreenHandler::GetManualFilamentSpeed(DGUS_VP_Variable &var, void *val_ptr)
{
  DEBUG_ECHOLNPGM("HandleGetFilamentSpeed");

  uint16_t value_len = swap16(*(uint16_t*)val_ptr);

  float value = (float)value_len;
 
  DEBUG_ECHOLNPAIR_F("FilamentSpeed value:", value);

  // FilamentSpeed = value;

  *(float *)var.memadr = value;

  ScreenHandler.skipVP = var.VP; // don't overwrite value the next update time as the display might autoincrement in parallel
  return;
}


void DGUSScreenHandler::MKS_FilamentLoad(DGUS_VP_Variable &var, void *val_ptr)
{
  DEBUG_ECHOLNPGM("Load Filament");
  // uint16_t filament_long;
  char buf[40];
  
  if(thermalManager.temp_hotend[0].celsius < EXTRUDE_MINTEMP)
  {
    thermalManager.setTargetHotend(PREHEAT_1_TEMP_HOTEND, 0);
    ScreenHandler.sendinfoscreen(PSTR("NOTICE"), nullptr, PSTR("Wait NZ hot"), nullptr, true, true, true, true);
    ScreenHandler.SetupConfirmAction(nullptr);
    ScreenHandler.GotoScreen(DGUSLCD_SCREEN_POPUP);
    
  }
  else 
  {
    DEBUG_ECHOLNPAIR_F("distanceFilament_long = ",distanceFilament);
    DEBUG_ECHOLNPAIR_F("In speed  = ",FilamentSpeed);
    sprintf(buf,"T0\nG91\nG1 E%d F%d\nG90 ",(uint32_t)distanceFilament,FilamentSpeed);
    queue.enqueue_now_P(buf);
  }
}

void DGUSScreenHandler:: MKS_FilamentUnLoad(DGUS_VP_Variable &var, void *val_ptr)
{
  DEBUG_ECHOLNPGM("UnLoad Filament");

  // uint16_t filament_long;
  char buf[40];

  /* makesure the temp had arr */
  if(thermalManager.temp_hotend[0].celsius < EXTRUDE_MINTEMP)
  {
    thermalManager.setTargetHotend(PREHEAT_1_TEMP_HOTEND, 0);
    
    ScreenHandler.sendinfoscreen(PSTR("NOTICE"), nullptr, PSTR("Wait NZ hot"), nullptr, true, true, true, true);
    ScreenHandler.SetupConfirmAction(nullptr);
    ScreenHandler.GotoScreen(DGUSLCD_SCREEN_POPUP);
  }
  else 
  {
    // DEBUG_ECHOLNPAIR_F("distanceFilament_long = ",distanceFilament);
    // filament_long = ExtUI::getAxisPosition_mm(ExtUI::extruder_t::E0) - distanceFilament;
    // DEBUG_ECHOLNPAIR_F("filament_long = ",filament_long);
    // DEBUG_ECHOLNPAIR_F("Out speed  = ",FilamentSpeed);
    // ExtUI::setAxisPosition_mm(filament_long, ExtUI::extruder_t::E0,(FilamentSpeed/60));
    sprintf(buf,"T0\nG91\nG1 E-%d F%d\nG90 ",(uint32_t)distanceFilament,FilamentSpeed);
    queue.enqueue_now_P(buf);
  }
}
#endif


#if ENABLED(DGUS_FILAMENT_LOADUNLOAD)

  typedef struct  {
    ExtUI::extruder_t extruder; // which extruder to operate
    uint8_t action; // load or unload
    bool heated; // heating done ?
    float purge_length; // the length to extrude before unload, prevent filament jam
  } filament_data_t;

  static filament_data_t filament_data;

  void DGUSScreenHandler::HandleFilamentOption(DGUS_VP_Variable &var, void *val_ptr) {
    DEBUG_ECHOLNPGM("HandleFilamentOption");

    uint8_t e_temp = 0;
    filament_data.heated = false;
    uint16_t preheat_option = swap16(*(uint16_t*)val_ptr);
    if (preheat_option <= 8)          // Load filament type
      filament_data.action = 1;
    else if (preheat_option >= 10) {  // Unload filament type
      preheat_option -= 10;
      filament_data.action = 2;
      filament_data.purge_length = DGUS_FILAMENT_PURGE_LENGTH;
    }
    else                              // Cancel filament operation
      filament_data.action = 0;

    switch (preheat_option) {
      case 0: // Load PLA
        #ifdef PREHEAT_1_TEMP_HOTEND
          e_temp = PREHEAT_1_TEMP_HOTEND;
        #endif
        break;
      case 1: // Load ABS
        TERN_(PREHEAT_2_TEMP_HOTEND, e_temp = PREHEAT_2_TEMP_HOTEND);
        break;
      case 2: // Load PET
        #ifdef PREHEAT_3_TEMP_HOTEND
          e_temp = PREHEAT_3_TEMP_HOTEND;
        #endif
        break;
      case 3: // Load FLEX
        #ifdef PREHEAT_4_TEMP_HOTEND
          e_temp = PREHEAT_4_TEMP_HOTEND;
        #endif
        break;
      case 9: // Cool down
      default:
        e_temp = 0;
        break;
    }

    if (filament_data.action == 0) { // Go back to utility screen
      #if HOTENDS >= 1
        thermalManager.setTargetHotend(e_temp, ExtUI::extruder_t::E0);
      #endif
      #if HOTENDS >= 2
        thermalManager.setTargetHotend(e_temp, ExtUI::extruder_t::E1);
      #endif
      GotoScreen(DGUSLCD_SCREEN_UTILITY);
    }
    else { // Go to the preheat screen to show the heating progress
      switch (var.VP) {
        default: return;
        #if HOTENDS >= 1
          case VP_E0_FILAMENT_LOAD_UNLOAD:
            filament_data.extruder = ExtUI::extruder_t::E0;
            thermalManager.setTargetHotend(e_temp, filament_data.extruder);
            break;
        #endif
        #if HOTENDS >= 2
          case VP_E1_FILAMENT_LOAD_UNLOAD:
            filament_data.extruder = ExtUI::extruder_t::E1;
            thermalManager.setTargetHotend(e_temp, filament_data.extruder);
          break;
        #endif
      }
      // GotoScreen(DGUSLCD_SCREEN_FILAMENT_HEATING);
    }
  }

  void DGUSScreenHandler::HandleFilamentLoadUnload(DGUS_VP_Variable &var) {
    DEBUG_ECHOLNPGM("HandleFilamentLoadUnload");
    if (filament_data.action <= 0) return;

    // If we close to the target temperature, we can start load or unload the filament
    if (thermalManager.hotEnoughToExtrude(filament_data.extruder) && \
       thermalManager.targetHotEnoughToExtrude(filament_data.extruder)) {
      float movevalue = DGUS_FILAMENT_LOAD_LENGTH_PER_TIME;

      if (filament_data.action == 1) { // load filament
        if (!filament_data.heated) {
          // GotoScreen(DGUSLCD_SCREEN_FILAMENT_LOADING);
          filament_data.heated = true;
        }
        movevalue = ExtUI::getAxisPosition_mm(filament_data.extruder)+movevalue;
      }
      else { // unload filament
        if (!filament_data.heated) {
          GotoScreen(DGUSLCD_SCREEN_FILAMENT_UNLOADING);
          filament_data.heated = true;
        }
        // Before unloading extrude to prevent jamming
        if (filament_data.purge_length >= 0) {
          movevalue = ExtUI::getAxisPosition_mm(filament_data.extruder) + movevalue;
          filament_data.purge_length -= movevalue;
        }
        else
          movevalue = ExtUI::getAxisPosition_mm(filament_data.extruder) - movevalue;
      }
      ExtUI::setAxisPosition_mm(movevalue, filament_data.extruder);
    }
  }
#endif

void DGUSScreenHandler::UpdateNewScreen(DGUSLCD_Screens newscreen, bool popup) {
  DEBUG_ECHOLNPAIR("SetNewScreen: ", newscreen);

  if (!popup) {
    memmove(&past_screens[1], &past_screens[0], sizeof(past_screens) - 1);
    past_screens[0] = current_screen;
  }

  current_screen = newscreen;
  skipVP = 0;
  ForceCompleteUpdate();
}

void DGUSScreenHandler::PopToOldScreen() {
  DEBUG_ECHOLNPAIR("PopToOldScreen s=", past_screens[0]);
  GotoScreen(past_screens[0], true);
  memmove(&past_screens[0], &past_screens[1], sizeof(past_screens) - 1);
  past_screens[sizeof(past_screens) - 1] = DGUSLCD_SCREEN_MAIN;
}

void DGUSScreenHandler::UpdateScreenVPData() {
  DEBUG_ECHOPAIR(" UpdateScreenVPData Screen: ", current_screen);

  const uint16_t *VPList = DGUSLCD_FindScreenVPMapList(current_screen);
  if (!VPList) {
    DEBUG_ECHOLNPAIR(" NO SCREEN FOR: ", current_screen);
    ScreenComplete = true;
    return;  // nothing to do, likely a bug or boring screen.
  }

  // Round-robin updating of all VPs.
  VPList += update_ptr;

  bool sent_one = false;
  do {
    uint16_t VP = pgm_read_word(VPList);
    DEBUG_ECHOPAIR(" VP: ", VP);
    if (!VP) {
      update_ptr = 0;
      DEBUG_ECHOLNPGM(" UpdateScreenVPData done");
      ScreenComplete = true;
      return;  // Screen completed.
    }

    if (VP == skipVP) { skipVP = 0; continue; }

    DGUS_VP_Variable rcpy;
    if (populate_VPVar(VP, &rcpy)) {
      uint8_t expected_tx = 6 + rcpy.size;  // expected overhead is 6 bytes + payload.
      // Send the VP to the display, but try to avoid overrunning the Tx Buffer.
      // But send at least one VP, to avoid getting stalled.
      if (rcpy.send_to_display_handler && (!sent_one || expected_tx <= dgusdisplay.GetFreeTxBuffer())) {
        //DEBUG_ECHOPAIR(" calling handler for ", rcpy.VP);
        sent_one = true;
        rcpy.send_to_display_handler(rcpy);
      }
      else {
        //auto x=dgusdisplay.GetFreeTxBuffer();
        //DEBUG_ECHOLNPAIR(" tx almost full: ", x);
        //DEBUG_ECHOPAIR(" update_ptr ", update_ptr);
        ScreenComplete = false;
        return;  // please call again!
      }
    }

  } while (++update_ptr, ++VPList, true);
}

void DGUSScreenHandler::GotoScreen(DGUSLCD_Screens screen, bool ispopup) {
  dgusdisplay.RequestScreen(screen);
  UpdateNewScreen(screen, ispopup);
}

bool DGUSScreenHandler::loop() {
  dgusdisplay.loop();

  const millis_t ms = millis();
  static millis_t next_event_ms = 0;

  if (!IsScreenComplete() || ELAPSED(ms, next_event_ms)) {
    next_event_ms = ms + DGUS_UPDATE_INTERVAL_MS;
    UpdateScreenVPData();
  }

  #if ENABLED(SHOW_BOOTSCREEN)
    static bool booted = false;
    if (!booted && TERN0(POWER_LOSS_RECOVERY, recovery.valid()))
      booted = true;
    if (!booted && ELAPSED(ms, BOOTSCREEN_TIMEOUT)) {
      booted = true;
      GotoScreen(DGUSLCD_SCREEN_MAIN);
    }
  #endif
  return IsScreenComplete();
}

void DGUSDisplay::RequestScreen(DGUSLCD_Screens screen) {
  DEBUG_ECHOLNPAIR("GotoScreen ", screen);
  const unsigned char gotoscreen[] = { 0x5A, 0x01, (unsigned char) (screen >> 8U), (unsigned char) (screen & 0xFFU) };
  WriteVariable(0x84, gotoscreen, sizeof(gotoscreen));
}


#if ENABLED(DGUS_LCD_UI_MKS)
void DGUSScreenHandler::DGUS_LanguageDisplay(uint8_t var)
{
  if (var == 0)       
  {
    const char home_buf_en[] = "Home";
    dgusdisplay.WriteVariable(VP_HOME_Dis, home_buf_en, 4, true);

    const char setting_buf_en[] = "Setting";
    dgusdisplay.WriteVariable(VP_Setting_Dis, setting_buf_en, 7, true);

    const char Tool_buf_en[] = "Tool";
    dgusdisplay.WriteVariable(VP_Tool_Dis, Tool_buf_en, 4, true);

    const char Print_buf_en[] = "Print";
    dgusdisplay.WriteVariable(VP_Print_Dis, Print_buf_en, 6, true);

    const char Language_buf_en[] = "Language";
    dgusdisplay.WriteVariable(VP_Language_Dis, Language_buf_en, 8, true);

    const char About_buf_en[] = "About";
    dgusdisplay.WriteVariable(VP_About_Dis, About_buf_en, 6, true);

    const char Config_buf_en[] = "Config";
    dgusdisplay.WriteVariable(VP_Config_Dis, Config_buf_en, 6, true);

    const char MotorConfig_buf_en[] = "MotorConfig";
    dgusdisplay.WriteVariable(VP_MotorConfig_Dis, MotorConfig_buf_en, 12, true); 

    const char LevelConfig_buf_en[] = "LevelConfig";
    dgusdisplay.WriteVariable(VP_LevelConfig_Dis, LevelConfig_buf_en, 11, true);

    const char TemperatureConfig_buf_en[] = "Temperature";
    dgusdisplay.WriteVariable(VP_TemperatureConfig_Dis, TemperatureConfig_buf_en, 11, true);

    const char Advance_buf_en[] = "Advance";
    dgusdisplay.WriteVariable(VP_Advance_Dis, Advance_buf_en, 16, true);

    const char Filament_buf_en[] = "Filament";
    dgusdisplay.WriteVariable(VP_Filament_Dis, Filament_buf_en, 8, true);

    const char Move_buf_en[] = "Move";
    dgusdisplay.WriteVariable(VP_Move_Dis, Move_buf_en, 4, true);

    #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    const char Level_buf_en[] = "AutoLevel";
    dgusdisplay.WriteVariable(VP_Level_Dis, Level_buf_en, 16, true);
    #else
    const char Level_buf_en[] = "Level";
    dgusdisplay.WriteVariable(VP_Level_Dis, Level_buf_en, 6, true);
    #endif

    const char MotorPluse_buf_en[] = "MotorPluse";
    dgusdisplay.WriteVariable(VP_MotorPluse_Dis, MotorPluse_buf_en, 16, true);

    const char MotorMaxSpeed_buf_en[] = "MotorMaxSpeed";
    dgusdisplay.WriteVariable(VP_MotorMaxSpeed_Dis, MotorMaxSpeed_buf_en, 16, true);

    const char MotorMaxAcc_buf_en[] = "MotorMaxAcc";
    dgusdisplay.WriteVariable(VP_MotorMaxAcc_Dis, MotorMaxAcc_buf_en, 16, true);

    const char TravelAcc_buf_en[] = "TravelAcc";
    dgusdisplay.WriteVariable(VP_TravelAcc_Dis, TravelAcc_buf_en, 16, true);

    const char FeedRateMin_buf_en[] = "FeedRateMin";
    dgusdisplay.WriteVariable(VP_FeedRateMin_Dis, FeedRateMin_buf_en, 16, true);

    const char TravelFeeRateMin_buf_en[] = "TravelFeedRateMin";
    dgusdisplay.WriteVariable(VP_TravelFeeRateMin_Dis, TravelFeeRateMin_buf_en, 24, true);

    const char Acc_buf_en[] = "Acc";
    dgusdisplay.WriteVariable(VP_ACC_Dis, Acc_buf_en, 16, true);

    const char Point_One_buf_en[] = "Point_One";
    dgusdisplay.WriteVariable(VP_Point_One_Dis, Point_One_buf_en, 12, true);

    const char Point_Two_buf_en[] = "Point_Two";
    dgusdisplay.WriteVariable(VP_Point_Two_Dis, Point_Two_buf_en, 12, true);

    const char Point_Three_buf_en[] = "Point_Three";
    dgusdisplay.WriteVariable(VP_Point_Three_Dis, Point_Three_buf_en, 12, true);

    const char Point_Four_buf_en[] = "Point_Four";
    dgusdisplay.WriteVariable(VP_Point_Four_Dis, Point_Four_buf_en, 12, true);

    const char Point_Five_buf_en[] = "Point_Five";
    dgusdisplay.WriteVariable(VP_Point_Five_Dis, Point_Five_buf_en, 12, true);

    const char Extrusion_buf_en[] = "Extrusion";
    dgusdisplay.WriteVariable(VP_Extrusion_Dis, Extrusion_buf_en, 12, true);

    const char HeatBed_buf_en[] = "HeatBed";
    dgusdisplay.WriteVariable(VP_HeatBed_Dis, HeatBed_buf_en, 12, true);

    const char FactoryDefaults_buf_en[] = "FactoryDefaults";
    dgusdisplay.WriteVariable(VP_FactoryDefaults_Dis, FactoryDefaults_buf_en, 16, true);

    const char StoreSetting_buf_en[] = "StoreSetting";
    dgusdisplay.WriteVariable(VP_StoreSetting_Dis, StoreSetting_buf_en, 12, true);

    const char PrintPauseConfig_buf_en[] = "PrintPauseConfig";
    dgusdisplay.WriteVariable(VP_PrintPauseConfig_Dis, PrintPauseConfig_buf_en, 17, true);

    const char X_Pluse_buf_en[] = "X_Pluse";
    dgusdisplay.WriteVariable(VP_X_Pluse_Dis, X_Pluse_buf_en, 16, true);

    const char Y_Pluse_buf_en[] = "Y_Pluse";
    dgusdisplay.WriteVariable(VP_Y_Pluse_Dis, Y_Pluse_buf_en, 16, true);

    const char Z_Pluse_buf_en[] = "Z_Pluse";
    dgusdisplay.WriteVariable(VP_Z_Pluse_Dis, Z_Pluse_buf_en, 16, true);

    const char E0_Pluse_buf_en[] = "E0_Pluse";
    dgusdisplay.WriteVariable(VP_E0_Pluse_Dis, E0_Pluse_buf_en, 16, true);

    const char E1_Pluse_buf_en[] = "E1_Pluse";
    dgusdisplay.WriteVariable(VP_E1_Pluse_Dis, E1_Pluse_buf_en, 16, true);

    const char X_Max_Speed_buf_en[] = "X_Max_Speed";
    dgusdisplay.WriteVariable(VP_X_Max_Speed_Dis, X_Max_Speed_buf_en, 16, true);

    const char Y_Max_Speed_buf_en[] = "Y_Max_Speed";
    dgusdisplay.WriteVariable(VP_Y_Max_Speed_Dis, Y_Max_Speed_buf_en, 16, true);

    const char Z_Max_Speed_buf_en[] = "Z_Max_Speed";
    dgusdisplay.WriteVariable(VP_Z_Max_Speed_Dis, Z_Max_Speed_buf_en, 16, true);

    const char E0_Max_Speed_buf_en[] = "E0_Max_Speed";
    dgusdisplay.WriteVariable(VP_E0_Max_Speed_Dis, E0_Max_Speed_buf_en, 16, true);

    const char E1_Max_Speed_buf_en[] = "E1_Max_Speed";
    dgusdisplay.WriteVariable(VP_E1_Max_Speed_Dis, E1_Max_Speed_buf_en, 16, true);

    const char X_Max_Acc_Speed_buf_en[] = "X_Max_Acc_Speed";
    dgusdisplay.WriteVariable(VP_X_Max_Acc_Speed_Dis, X_Max_Acc_Speed_buf_en, 16, true);

    const char Y_Max_Acc_Speed_buf_en[] = "Y_Max_Acc_Speed";
    dgusdisplay.WriteVariable(VP_Y_Max_Acc_Speed_Dis, Y_Max_Acc_Speed_buf_en, 16, true);

    const char Z_Max_Acc_Speed_buf_en[] = "Z_Max_Acc_Speed";
    dgusdisplay.WriteVariable(VP_Z_Max_Acc_Speed_Dis, Z_Max_Acc_Speed_buf_en, 16, true);

    const char E0_Max_Acc_Speed_buf_en[] = "E0_Max_Acc_Speed";
    dgusdisplay.WriteVariable(VP_E0_Max_Acc_Speed_Dis, E0_Max_Acc_Speed_buf_en, 16, true);

    const char E1_Max_Acc_Speed_buf_en[] = "E1_Max_Acc_Speed";
    dgusdisplay.WriteVariable(VP_E1_Max_Acc_Speed_Dis, E1_Max_Acc_Speed_buf_en, 16, true);

    const char X_PARK_POS_buf_en[] = "X_PARK_POS";
    dgusdisplay.WriteVariable(VP_X_PARK_POS_Dis, X_PARK_POS_buf_en, 16, true);

    const char Y_PARK_POS_buf_en[] = "Y_PARK_POS";
    dgusdisplay.WriteVariable(VP_Y_PARK_POS_Dis, Y_PARK_POS_buf_en, 16, true);

    const char Z_PARK_POS_buf_en[] = "Z_PARK_POS";
    dgusdisplay.WriteVariable(VP_Z_PARK_POS_Dis, Z_PARK_POS_buf_en, 16, true);

    const char Length_buf_en[] = "Length";
    dgusdisplay.WriteVariable(VP_Length_Dis, Length_buf_en, 8, true);

    const char Speed_buf_en[] = "Speed";
    dgusdisplay.WriteVariable(VP_Speed_Dis, Speed_buf_en, 8, true);

    const char InOut_buf_en[] = "InOut";
    dgusdisplay.WriteVariable(VP_InOut_Dis, InOut_buf_en, 8, true);

    const char PrintTimet_buf_en[] = "PrintTime";
    dgusdisplay.WriteVariable(VP_PrintTime_Dis, PrintTimet_buf_en, 10, true);

    const char E0_Temp_buf_en[] = "E0_Temp";
    dgusdisplay.WriteVariable(VP_E0_Temp_Dis, E0_Temp_buf_en, 10, true);

    const char E1_Temp_buf_en[] = "E1_Temp";
    dgusdisplay.WriteVariable(VP_E1_Temp_Dis, E1_Temp_buf_en, 10, true);

    const char HB_Temp_buf_en[] = "HB_Temp";
    dgusdisplay.WriteVariable(VP_HB_Temp_Dis, HB_Temp_buf_en, 10, true);

    const char Feedrate_buf_en[] = "Feedrate";
    dgusdisplay.WriteVariable(VP_Feedrate_Dis, Feedrate_buf_en, 10, true);

    const char PrintAcc_buf_en[] = "PrintAcc";
    dgusdisplay.WriteVariable(VP_PrintAcc_Dis, PrintAcc_buf_en, 10, true);

    const char FAN_Speed_buf_en[] = "FAN_Speed";
    dgusdisplay.WriteVariable(VP_FAN_Speed_Dis, FAN_Speed_buf_en, 10, true);

    const char Printing_buf_en[] = "Printing";
    dgusdisplay.WriteVariable(VP_Printing_Dis, Printing_buf_en, 10, true);

    const char Info_EEPROM_2_buf_en[] = "Store setting to EEPROM?";
    dgusdisplay.WriteVariable(VP_Info_EEPROM_2_Dis, Info_EEPROM_2_buf_en, 32, true);

    const char Info_EEPROM_1_buf_en[] = "Revert setting to fatory?";
    dgusdisplay.WriteVariable(VP_Info_EEPROM_1_Dis, Info_EEPROM_1_buf_en, 32, true);

    const char Info_PrinfFinsh_1_buf_en[] = "Print Done";
    dgusdisplay.WriteVariable(VP_Info_PrinfFinsh_1_Dis, Info_PrinfFinsh_1_buf_en, 32, true);
  }
  else if (var == 1)
  {
    uint16_t home_buf_ch[] = {0xF7D6,0xB3D2}; 
    dgusdisplay.WriteVariable(VP_HOME_Dis, home_buf_ch, 4, true);

    const uint16_t Setting_Dis[] PROGMEM = {0xE8C9,0xC3D6,0x2000,0x2000,0x2000};
    dgusdisplay.WriteVariable(VP_Setting_Dis, Setting_Dis, 7, true);

    const uint16_t Tool_Dis[] PROGMEM = {0xA4B9,0xDFBE};  
    dgusdisplay.WriteVariable(VP_Tool_Dis, Tool_Dis, 4, true);
    
    const uint16_t Print_buf_ch[] PROGMEM = {0xF2B4,0xA1D3,0x2000};  
    dgusdisplay.WriteVariable(VP_Print_Dis, Print_buf_ch, 6, true);

    const uint16_t Language_buf_ch[] = {0xEFD3,0xD4D1,0x2000,0x2000};
    dgusdisplay.WriteVariable(VP_Language_Dis, Language_buf_ch, 8, true);

    const uint16_t About_buf_ch[] = {0xD8B9,0xDAD3,0x2000};
    dgusdisplay.WriteVariable(VP_About_Dis, About_buf_ch, 6, true);

    const uint16_t Config_buf_ch[] = {0xE4C5,0xC3D6,0x2000};
    dgusdisplay.WriteVariable(VP_Config_Dis, Config_buf_ch, 6, true);

    const uint16_t MotorConfig_buf_ch[] = {0xE7B5,0xFABB,0xE4C5,0xC3D6,0x2000};
    dgusdisplay.WriteVariable(VP_MotorConfig_Dis, MotorConfig_buf_ch, 12, true);

    const uint16_t LevelConfig_buf_ch[] = {0xF7B5,0xBDC6,0xE4C5,0xC3D6,0x2000};
    dgusdisplay.WriteVariable(VP_LevelConfig_Dis, LevelConfig_buf_ch, 11, true);

    const uint16_t TemperatureConfig_buf_ch[] = {0xC2CE,0XC8B6,0x2000};
    dgusdisplay.WriteVariable(VP_TemperatureConfig_Dis, TemperatureConfig_buf_ch, 11, true);

    const uint16_t Advance_buf_ch[] = {0xdfb8,0xB6BC,0XE8C9,0XC3D6,0x2000};
    dgusdisplay.WriteVariable(VP_Advance_Dis, Advance_buf_ch, 16, true);

    const uint16_t Filament_buf_ch[] = {0xB7BC,0xF6B3,0X2000};
    dgusdisplay.WriteVariable(VP_Filament_Dis, Filament_buf_ch, 8, true);

    const uint16_t Move_buf_ch[] = {0xC6D2,0xAFB6,0x2000 }; 
    dgusdisplay.WriteVariable(VP_Move_Dis, Move_buf_ch, 4, true);

    #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    const uint16_t Level_buf_ch[] = {0xD4D7,0xAFB6,0xf7b5,0xbdc6,0x2000};
    dgusdisplay.WriteVariable(VP_Level_Dis, Level_buf_ch, 16, true);
    #else
    const uint16_t Level_buf_ch[] = {0xf7b5,0xbdc6,0x2000};
    dgusdisplay.WriteVariable(VP_Level_Dis, Level_buf_ch, 6, true);
    #endif

    const uint16_t MotorPluse_buf_ch[] = {0XF6C2,0XE5B3,0X2000};
    dgusdisplay.WriteVariable(VP_MotorPluse_Dis, MotorPluse_buf_ch, 16, true);

    const uint16_t MotorMaxSpeed_buf_ch[] = {0XEED7,0XF3B4,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_MotorMaxSpeed_Dis, MotorMaxSpeed_buf_ch, 16, true);

    const uint16_t MotorMaxAcc_buf_ch[] ={0XEED7,0XF3B4,0XD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_MotorMaxAcc_Dis, MotorMaxAcc_buf_ch, 16, true);

    const uint16_t TravelAcc_buf_ch[] = {0XD5BF,0XD0D0,0XD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_TravelAcc_Dis, TravelAcc_buf_ch, 16, true);

    const uint16_t FeedRateMin_buf_ch[] = {0XEED7,0XA1D0,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_FeedRateMin_Dis, FeedRateMin_buf_ch, 12, true);

    const uint16_t TravelFeeRateMin_buf_ch[] = {0XEED7,0XA1D0,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_TravelFeeRateMin_Dis, TravelFeeRateMin_buf_ch, 24, true);

    const uint16_t Acc_buf_ch[] = {0XD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_ACC_Dis, Acc_buf_ch, 16, true);

    const uint16_t Point_One_buf_ch[] = {0XDAB5,0XBBD2,0XE3B5,0X2000};
    dgusdisplay.WriteVariable(VP_Point_One_Dis, Point_One_buf_ch, 12, true);

    const uint16_t Point_Two_buf_ch[] = {0XDAB5,0XFEB6,0XE3B5,0X2000};
    dgusdisplay.WriteVariable(VP_Point_Two_Dis, Point_Two_buf_ch, 12, true);

    const uint16_t Point_Three_buf_ch[] = {0XDAB5,0XFDC8,0XE3B5,0X2000};
    dgusdisplay.WriteVariable(VP_Point_Three_Dis, Point_Three_buf_ch, 12, true);

    const uint16_t Point_Four_buf_ch[] = {0XDAB5,0XC4CB,0XE3B5,0X2000};
    dgusdisplay.WriteVariable(VP_Point_Four_Dis, Point_Four_buf_ch, 12, true);

    const uint16_t Point_Five_buf_ch[] = {0XDAB5,0XE5CE,0XE3B5,0X2000};
    dgusdisplay.WriteVariable(VP_Point_Five_Dis, Point_Five_buf_ch, 12, true);

    const uint16_t Extrusion_buf_ch[] = {0xB7BC,0XF6B3,0XB7CD,0X2000};
    dgusdisplay.WriteVariable(VP_Extrusion_Dis, Extrusion_buf_ch, 12, true);

    const uint16_t HeatBed_buf_ch[] = {0XC8C8,0XB2B4,0X2000};
    dgusdisplay.WriteVariable(VP_HeatBed_Dis, HeatBed_buf_ch, 12, true);

    const uint16_t FactoryDefaults_buf_ch[] = {0xD6BB,0XB4B8,0XF6B3,0XA7B3,0XE8C9,0XC3D6,0X2000};
    dgusdisplay.WriteVariable(VP_FactoryDefaults_Dis, FactoryDefaults_buf_ch, 16, true);

    const uint16_t StoreSetting_buf_ch[] = {0XA3B1,0XE6B4,0XC9E8,0XD6C3,0X2000};
    dgusdisplay.WriteVariable(VP_StoreSetting_Dis, StoreSetting_buf_ch, 12, true);

    const uint16_t PrintPauseConfig_buf_ch[] = {0xF2B4,0XA1D3,0XA3CD,0XBFBF,0X2000};
    dgusdisplay.WriteVariable(VP_PrintPauseConfig_Dis, PrintPauseConfig_buf_ch, 17, true);

    const uint16_t X_Pluse_buf_ch[] = {0x2058,0xE1D6,0XF6C2,0XE5B3,0x2000};
    dgusdisplay.WriteVariable(VP_X_Pluse_Dis, X_Pluse_buf_ch, 16, true);

    const uint16_t Y_Pluse_buf_ch[] = {0x2059,0xE1D6,0XF6C2,0XE5B3,0x2000};
    dgusdisplay.WriteVariable(VP_Y_Pluse_Dis, Y_Pluse_buf_ch, 16, true);

    const uint16_t Z_Pluse_buf_ch[] = {0x205A,0xE1D6,0XF6C2,0XE5B3,0x2000};
    dgusdisplay.WriteVariable(VP_Z_Pluse_Dis, Z_Pluse_buf_ch, 16, true);

    const uint16_t E0_Pluse_buf_ch[] = {0x3045,0xE1D6,0XF6C2,0XE5B3,0x2000};
    dgusdisplay.WriteVariable(VP_E0_Pluse_Dis, E0_Pluse_buf_ch, 16, true);

    const uint16_t E1_Pluse_buf_ch[] = {0x3145,0xE1D6,0XF6C2,0XE5B3,0x2000};
    dgusdisplay.WriteVariable(VP_E1_Pluse_Dis, E1_Pluse_buf_ch, 16, true);

    const uint16_t X_Max_Speed_buf_ch[] = {0x2058,0xEED7,0XF3B4,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_X_Max_Speed_Dis, X_Max_Speed_buf_ch, 16, true);

    const uint16_t Y_Max_Speed_buf_ch[] = {0x2059,0xEED7,0XF3B4,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_Y_Max_Speed_Dis, Y_Max_Speed_buf_ch, 16, true);

    const uint16_t Z_Max_Speed_buf_ch[] = {0x205A,0xEED7,0XF3B4,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_Z_Max_Speed_Dis, Z_Max_Speed_buf_ch, 16, true);

    const uint16_t E0_Max_Speed_buf_ch[] = {0x3045,0xEED7,0XF3B4,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_E0_Max_Speed_Dis, E0_Max_Speed_buf_ch, 16, true);

    const uint16_t E1_Max_Speed_buf_ch[] = {0x3145,0xEED7,0XF3B4,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_E1_Max_Speed_Dis, E1_Max_Speed_buf_ch, 16, true);

    const uint16_t X_Max_Acc_Speed_buf_ch[] = {0x2058,0xEED7,0xF3B4,0xD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_X_Max_Acc_Speed_Dis, X_Max_Acc_Speed_buf_ch, 16, true);

    const uint16_t Y_Max_Acc_Speed_buf_ch[] = {0x2059,0xEED7,0xF3B4,0xD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_Y_Max_Acc_Speed_Dis, Y_Max_Acc_Speed_buf_ch, 16, true);

    const uint16_t Z_Max_Acc_Speed_buf_ch[] = {0x205A,0xEED7,0xF3B4,0xD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_Z_Max_Acc_Speed_Dis, Z_Max_Acc_Speed_buf_ch, 16, true);

    const uint16_t E0_Max_Acc_Speed_buf_ch[] ={0x3045,0xEED7,0xF3B4,0xD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_E0_Max_Acc_Speed_Dis, E0_Max_Acc_Speed_buf_ch, 16, true);

    const uint16_t E1_Max_Acc_Speed_buf_ch[] = {0x3145,0xEED7,0xF3B4,0xD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_E1_Max_Acc_Speed_Dis, E1_Max_Acc_Speed_buf_ch, 16, true);

    const uint16_t X_PARK_POS_buf_ch[] = {0X2058,0XA3CD,0XBFBF,0XBBCE,0XD6C3,0X2000};
    dgusdisplay.WriteVariable(VP_X_PARK_POS_Dis, X_PARK_POS_buf_ch, 16, true);

    const uint16_t Y_PARK_POS_buf_ch[] = {0X2059,0XA3CD,0XBFBF,0XBBCE,0XD6C3,0X2000};
    dgusdisplay.WriteVariable(VP_Y_PARK_POS_Dis, Y_PARK_POS_buf_ch, 16, true);

    const uint16_t Z_PARK_POS_buf_ch[] = {0X205A,0XA3CD,0XBFBF,0XBBCE,0XD6C3,0X2000};
    dgusdisplay.WriteVariable(VP_Z_PARK_POS_Dis, Z_PARK_POS_buf_ch, 16, true);

    const uint16_t Length_buf_ch[] = {0xA4B3,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_Length_Dis, Length_buf_ch, 8, true);

    const uint16_t Speed_buf_ch[] = {0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_Speed_Dis, Speed_buf_ch, 8, true);

    const uint16_t InOut_buf_ch[] = {0XF8BD,0XF6B3,0X2000};
    dgusdisplay.WriteVariable(VP_InOut_Dis, InOut_buf_ch, 8, true);

    const uint16_t PrintTimet_buf_en[] = {0XF2B4,0XA1D3,0XB1CA,0XE4BC,0X2000};
    dgusdisplay.WriteVariable(VP_PrintTime_Dis, PrintTimet_buf_en, 16, true);

    const uint16_t E0_Temp_buf_ch[] = {0x3045,0XC2CE,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_E0_Temp_Dis, E0_Temp_buf_ch, 16, true);

    const uint16_t E1_Temp_buf_ch[] = {0x3145,0XC2CE,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_E1_Temp_Dis, E1_Temp_buf_ch, 16, true);

    const uint16_t HB_Temp_buf_ch[] = {0XC8C8,0XB2B4,0XC2CE,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_HB_Temp_Dis, HB_Temp_buf_ch, 16, true);

    const uint16_t Feedrate_buf_ch[] = {0XB7BC,0XF6B3,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_Feedrate_Dis, Feedrate_buf_ch, 16, true);

    const uint16_t PrintAcc_buf_ch[] = {0xF2B4,0XA1D3,0XD3BC,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_PrintAcc_Dis, PrintAcc_buf_ch, 16, true);

    const uint16_t FAN_Speed_buf_ch[] = {0XE7B7,0XC8C9,0XD9CB,0XC8B6,0X2000};
    dgusdisplay.WriteVariable(VP_FAN_Speed_Dis, FAN_Speed_buf_ch, 16, true);

    const uint16_t Printing_buf_ch[] = {0XF2B4,0XA1D3,0XD0D6,0X2000};
    dgusdisplay.WriteVariable(VP_Printing_Dis, Printing_buf_ch, 16, true);

    const uint16_t Info_EEPROM_2_buf_ch[] = {0XC7CA,0XF1B7,0XA3B1,0XE6B4,0XE8C9,0XC3D6,0XBFA3,0X2000};
    dgusdisplay.WriteVariable(VP_Info_EEPROM_2_Dis, Info_EEPROM_2_buf_ch, 32, true);

    const uint16_t Info_EEPROM_1_buf_ch[] = {0xC7CA,0XF1B7,0XD6BB,0XB4B8,0XF6B3,0XA7B3,0XE8C9,0XC3D6,0x2000};
    dgusdisplay.WriteVariable(VP_Info_EEPROM_1_Dis, Info_EEPROM_1_buf_ch, 32, true);


  }
}
#endif 

#endif // HAS_DGUS_LCD
