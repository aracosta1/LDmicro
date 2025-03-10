//-----------------------------------------------------------------------------
// Copyright 2007 Jonathan Westhues
//
// This file is part of LDmicro.
//
// LDmicro is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// LDmicro is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with LDmicro.  If not, see <http://www.gnu.org/licenses/>.
//------
//
// Common controls in the main window. The main window consists of the drawing
// area, where the ladder diagram is displayed, plus various controls for
// scrolling, I/O list, menus.
// Jonathan Westhues, Nov 2004
//-----------------------------------------------------------------------------
#include "stdafx.h"

#include "ldmicro.h"

// scrollbars for the ladder logic area
static HWND HorizScrollBar;
static HWND VertScrollBar;
int         ScrollWidth;
int         ScrollHeight;
bool        NeedHoriz;

// status bar at the bottom of the screen, to display settings
static HWND StatusBar;

// have to get back to the menus to gray/ungray, check/uncheck things
static HMENU FileMenu;
static HMENU EditMenu;
static HMENU InstructionMenu;
static HMENU CourseMenu;
static HMENU FormatStrMenu;
static HMENU ProcessorMenu;
static HMENU ProcAvrMenu; /// JG3
static HMENU ProcPic16Menu;
static HMENU ProcPic18Menu;
static HMENU ProcEspMenu;
static HMENU ProcArmMenu;
static HMENU ProcOthersMenu; /// JG3
static HMENU ProcessorMenu2;
static HMENU SimulateMenu;
static HMENU TopMenu;
static HMENU SpecialFunction;
static HMENU DisplayMenu;
static HMENU ConMenu;
static HMENU CmpMenu;
static HMENU TmpMenu;
static HMENU CntMenu;
static HMENU EdgMenu;
static HMENU UrtMenu;
static HMENU SpiMenu;
static HMENU I2cMenu;
static HMENU SignedMenu;
static HMENU BitwiseMenu;
static HMENU PulseMenu;
static HMENU SchemeMenu;
static HMENU settings;

// listview used to maintain the list of I/O pins with symbolic names, plus
// the internal relay too
HWND        IoList;
static int  IoListSelectionPoint = 0;
static bool IoListOutOfSync = false;
char        IoListSelectionName[MAX_NAME_LEN] = "";
int         IoListHeight;
int         IoListTop;

// whether the simulation is running in real time
bool RealTimeSimulationRunning;

extern HWND UartSimulationWindow;
extern HWND SpiSimulationWindow;
extern HWND I2cSimulationWindow;

//-----------------------------------------------------------------------------
// Create the standard Windows controls used in the main window: a Listview
// for the I/O list, and a status bar for settings.
//-----------------------------------------------------------------------------
void MakeMainWindowControls()
{
    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
#define LV_ADD_COLUMN(hWnd, i, w, s)          \
    do {                                      \
        lvc.iSubItem = i;                     \
        lvc.pszText = (LPSTR)(s);             \
        lvc.iOrder = 0;                       \
        lvc.cx = w;                           \
        ListView_InsertColumn(hWnd, i, &lvc); \
    } while(0)
    // create child window for IO list
    IoList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD | LVS_REPORT | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS | WS_TABSTOP | LVS_SINGLESEL | WS_CLIPSIBLINGS, 12, 25, 300, 300, MainWindow, nullptr, Instance, nullptr);
    ListView_SetExtendedListViewStyle(IoList, LVS_EX_FULLROWSELECT);

    int typeWidth = 110;
    int pinWidth = 70;
    int portWidth = 60;
    int pinNameWidth = 140;
    int modbusWidth = 80;

    LV_ADD_COLUMN(IoList, LV_IO_NAME, 150, _("Name"));
    LV_ADD_COLUMN(IoList, LV_IO_TYPE, typeWidth, _("Type"));
    LV_ADD_COLUMN(IoList, LV_IO_STATE, 100 + 50, _("State"));
    LV_ADD_COLUMN(IoList, LV_IO_PIN, pinWidth, _("Pin on MCU"));
    LV_ADD_COLUMN(IoList, LV_IO_PORT, portWidth, _("MCU Port"));
    LV_ADD_COLUMN(IoList, LV_IO_PINNAME, pinNameWidth, _("Pin Name"));
    LV_ADD_COLUMN(IoList, LV_IO_RAM_ADDRESS, 75, _("Address"));
    LV_ADD_COLUMN(IoList, LV_IO_SISE_OF_VAR, 60, _("Size"));
    LV_ADD_COLUMN(IoList, LV_IO_MODBUS, modbusWidth, _("Modbus addr"));
    // IO list horizontal scroll bar
    HorizScrollBar = CreateWindowEx(0, WC_SCROLLBAR, "", WS_CHILD | SBS_HORZ | SBS_BOTTOMALIGN | WS_VISIBLE | WS_CLIPSIBLINGS, 100, 100, 100, 100, MainWindow, nullptr, Instance, nullptr);
    // IO list Verticle scroll bar
    VertScrollBar = CreateWindowEx(0, WC_SCROLLBAR, "", WS_CHILD | SBS_VERT | SBS_LEFTALIGN | WS_VISIBLE | WS_CLIPSIBLINGS, 200, 100, 100, 100, MainWindow, nullptr, Instance, nullptr);
    RECT scroll;
    GetWindowRect(HorizScrollBar, &scroll);
    ScrollHeight = scroll.bottom - scroll.top;
    GetWindowRect(VertScrollBar, &scroll);
    ScrollWidth = scroll.right - scroll.left;

    StatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, "LDmicro started", MainWindow, 0);
    int edges[] = {60, 250 + 60, 370 + 130, -1};
    SendMessage(StatusBar, SB_SETPARTS, 4, (LPARAM)edges);

    // display IO list window with white background - no actual list
    ShowWindow(IoList, SW_SHOW);
}

//-----------------------------------------------------------------------------
// Set up the title bar text for the main window; indicate whether we are in
// simulation or editing mode, and indicate the filename.
//-----------------------------------------------------------------------------
void UpdateMainWindowTitleBar()
{
    char line[MAX_PATH + 100];
    if(InSimulationMode) {
        if(RealTimeSimulationRunning) {
            strcpy(line, _("LDmicro - Simulation (Running)"));
        } else {
            strcpy(line, _("LDmicro - Simulation (Stopped)"));
        }
    } else {
        strcpy(line, _("LDmicro - Program Editor"));
    }
    if(strlen(CurrentSaveFile) > 0) {
        sprintf(line + strlen(line), " - %s", CurrentSaveFile);
    } else {
        strcat(line, _(" - (not yet saved)"));
    }

    SetWindowText(MainWindow, line);
}

//-----------------------------------------------------------------------------
// Set the enabled state of the logic menu items to reflect where we are on
// the schematic (e.g. can't insert two coils in series).
//-----------------------------------------------------------------------------
void SetMenusEnabled(bool canNegate, bool canNormal, bool canResetOnly, bool canSetOnly, bool canDelete, bool canInsertEnd, bool canInsertOther, bool canPushRungDown, bool canPushRungUp, bool canInsertComment)
{
    EnableMenuItem(EditMenu, MNU_PUSH_RUNG_UP, canPushRungUp ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(EditMenu, MNU_PUSH_RUNG_DOWN, canPushRungDown ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(EditMenu, MNU_DELETE_RUNG, (Prog.numRungs > 1) ? MF_ENABLED : MF_GRAYED);

    EnableMenuItem(InstructionMenu, MNU_NEGATE, canNegate ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(InstructionMenu, MNU_MAKE_NORMAL, canNormal ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(InstructionMenu, MNU_MAKE_RESET_ONLY, canResetOnly ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(InstructionMenu, MNU_MAKE_TTRIGGER, canResetOnly ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(InstructionMenu, MNU_MAKE_SET_ONLY, canSetOnly ? MF_ENABLED : MF_GRAYED);

    EnableMenuItem(InstructionMenu, MNU_INSERT_COMMENT, canInsertComment ? MF_ENABLED : MF_GRAYED);

    EnableMenuItem(EditMenu, MNU_DELETE_ELEMENT, canDelete ? MF_ENABLED : MF_GRAYED);

    int t;
    t = canInsertEnd ? MF_ENABLED : MF_GRAYED;
    EnableMenuItem(InstructionMenu, MNU_INSERT_COIL, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_COIL_RELAY, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_RES, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_MOV, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_ADD, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SUB, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_MUL, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_DIV, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_MOD, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_PERSIST, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_READ_ADC, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SET_PWM, t);
    //EnableMenuItem(InstructionMenu, MNU_INSERT_PWM_OFF, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_NPULSE_OFF, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_MASTER_RLY, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SLEEP, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CLRWDT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_LOCK, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_GOTO, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_GOSUB, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_RETURN, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SHIFT_REG, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_LUT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_PWL, t);

    t = canInsertOther ? MF_ENABLED : MF_GRAYED;
    EnableMenuItem(InstructionMenu, MNU_INSERT_SET_BIT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CLEAR_BIT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_IF_BIT_SET, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_IF_BIT_CLEAR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_AND, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_OR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_XOR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_NOT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_NEG, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_RANDOM, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SEED_RANDOM, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SHL, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SHR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SR0, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_ROL, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_ROR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_BIN2BCD, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_BCD2BIN, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SWAP, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_OPPOSITE, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_TIME2COUNT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_TIME2DELAY, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_TCY, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_TON, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_TOF, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_THI, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_TLO, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_OSR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_OSF, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_OSC, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_ODF, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_ODR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_STEPPER, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_PULSER, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_NPULSE, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_QUAD_ENCOD, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_RTL, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_RTO, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CONTACTS, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CONT_RELAY, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CONT_OUTPUT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CTU, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CTD, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CTC, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_CTR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_EQU, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_NEQ, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_GRT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_GEQ, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_LES, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_LEQ, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SHORT, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_OPEN, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_DELAY, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_LABEL, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SUBPROG, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_ENDSUB, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_UART_SEND, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_UART_RECV, t);
    /*
    EnableMenuItem(InstructionMenu, MNU_INSERT_UART_SENDn, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_UART_RECVn, t);
*/
    EnableMenuItem(InstructionMenu, MNU_INSERT_UART_SEND_READY, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_UART_RECV_AVAIL, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_STRING, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_UART_WRITE, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_FMTD_STRING, t);
    /*
    EnableMenuItem(InstructionMenu, ELEM_CPRINTF, t);
    EnableMenuItem(InstructionMenu, ELEM_SPRINTF, t);
    EnableMenuItem(InstructionMenu, ELEM_FPRINTF, t);
    EnableMenuItem(InstructionMenu, ELEM_PRINTF, t);
    EnableMenuItem(InstructionMenu, ELEM_I2C_CPRINTF, t);
    EnableMenuItem(InstructionMenu, ELEM_ISP_CPRINTF, t);
    EnableMenuItem(InstructionMenu, ELEM_UART_CPRINTF, t);
*/

#ifdef USE_SFR
    EnableMenuItem(InstructionMenu, MNU_INSERT_SFR, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SFW, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SSFB, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_csFB, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_TSFB, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_T_C_SFB, t);
#endif

    EnableMenuItem(InstructionMenu, MNU_INSERT_SPI, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_SPI_WRITE, t);

    EnableMenuItem(InstructionMenu, MNU_INSERT_I2C_READ, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_I2C_WRITE, t);

    EnableMenuItem(InstructionMenu, MNU_INSERT_BUS, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_7SEG, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_9SEG, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_14SEG, t);
    EnableMenuItem(InstructionMenu, MNU_INSERT_16SEG, t);

    t = Prog.mcu() ? MF_ENABLED : MF_GRAYED;
    EnableMenuItem(settings, MNU_PULL_UP_RESISTORS, t);
    /*
    t = Prog.mcu() && (Prog.mcu()->core > ARMcores) && (Prog.mcu()->core < ARMcores_) ? MF_ENABLED : MF_GRAYED;
    EnableMenuItem(settings, MNU_COMPILE_ARMGCC, t);
*/
}

//-----------------------------------------------------------------------------
// Set the enabled state of the undo/redo menus.
//-----------------------------------------------------------------------------
void SetUndoEnabled(bool undoEnabled, bool redoEnabled)
{
    EnableMenuItem(EditMenu, MNU_UNDO, undoEnabled ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(EditMenu, MNU_REDO, redoEnabled ? MF_ENABLED : MF_GRAYED);
}

//-----------------------------------------------------------------------------
// Create the top-level menu bar for the main window. Mostly static, but we
// create the "select processor" menu from the list in mcutable.h dynamically.
//-----------------------------------------------------------------------------
HMENU MakeMainWindowMenus()
{
    HMENU compile, help;
    HMENU ConfigMenu;
    // file popup menu
    FileMenu = CreatePopupMenu();
    AppendMenu(FileMenu, MF_STRING, MNU_NEW, _("&New\tCtrl+N"));
    AppendMenu(FileMenu, MF_STRING, MNU_OPEN, _("&Open...\tCtrl+O"));
    AppendMenu(FileMenu, MF_STRING, MNU_NOTEPAD_LD, _("Open ld in notepad\tF4"));
    AppendMenu(FileMenu, MF_STRING, MNU_NOTEPAD_HEX, _("Open hex in notepad\tAlt+F6"));
    AppendMenu(FileMenu, MF_STRING, MNU_NOTEPAD_ASM, _("Open asm in notepad\tAlt+F3"));
    AppendMenu(FileMenu, MF_STRING, MNU_NOTEPAD_C, _("Open c in notepad"));
    AppendMenu(FileMenu, MF_STRING, MNU_NOTEPAD_INO, _("Open ino in notepad"));
    AppendMenu(FileMenu, MF_STRING, MNU_NOTEPAD_PAS, _("Open pas in notepad"));
    AppendMenu(FileMenu, MF_STRING, MNU_NOTEPAD_PL, _("Open pl in notepad\tAlt+F5"));
    AppendMenu(FileMenu, MF_STRING, MNU_EXPLORE_DIR, _("Explore ld directory"));
    AppendMenu(FileMenu, MF_STRING, MNU_SAVE, _("&Save\tCtrl+S or F2"));
    AppendMenu(FileMenu, MF_STRING, MNU_SAVE_01, _("Save LDmicro0.1 file format v2.3 compatible"));
    AppendMenu(FileMenu, MF_STRING, MNU_SAVE_02, _("Save LDmicro0.2 file format"));
    AppendMenu(FileMenu, MF_STRING, MNU_SAVE_AS, _("Save &As..."));

    AppendMenu(FileMenu, MF_SEPARATOR, 0, "");
    AppendMenu(FileMenu, MF_STRING, MNU_EXPORT, _("&Export As Text...\tCtrl+E"));
    AppendMenu(FileMenu, MF_STRING, MNU_NOTEPAD_TXT, _("Open Text in notepad\tF3"));

    AppendMenu(FileMenu, MF_SEPARATOR, 0, "");
    AppendMenu(FileMenu, MF_STRING, MNU_EXIT, _("E&xit\tAlt+X"));

    EditMenu = CreatePopupMenu();
    AppendMenu(EditMenu, MF_STRING, MNU_UNDO, _("&Undo\tCtrl+Z or Alt+Backspace"));
    AppendMenu(EditMenu, MF_STRING, MNU_REDO, _("&Redo\tCtrl+Y or Alt+Shift+Backspace"));

    AppendMenu(EditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(EditMenu, MF_STRING, MNU_INSERT_RUNG_BEFORE, _("Insert Rung &Before\tShift+6"));
    AppendMenu(EditMenu, MF_STRING, MNU_INSERT_RUNG_AFTER, _("Insert Rung &After\tShift+V"));
    AppendMenu(EditMenu, MF_STRING, MNU_PUSH_RUNG_UP, _("Move Selected Rung &Up\tAlt+Up"));
    AppendMenu(EditMenu, MF_STRING, MNU_PUSH_RUNG_DOWN, _("Move Selected Rung &Down\tAlt+Down"));

    AppendMenu(EditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(EditMenu, MF_STRING, MNU_COPY_RUNG_DOWN, _("Dup&licate Selected Rung\tCtrl+D"));
    AppendMenu(EditMenu, MF_STRING, MNU_SELECT_RUNG, _("Select Rungs\tShift+Up or Shift+Down"));
    AppendMenu(EditMenu, MF_STRING, MNU_SELECT_RUNG, _("Select Rungs\tCtrl+Left Mouse Button Click"));
    AppendMenu(EditMenu, MF_STRING, MNU_CUT_RUNG, _("Cu&t Rungs\tCtrl+X or Shift+Del"));
    AppendMenu(EditMenu, MF_STRING, MNU_COPY_RUNG, _("&Copy Rungs\tCtrl+C or Ctrl+Insert"));
    AppendMenu(EditMenu, MF_STRING, MNU_COPY_ELEM, _("Copy Selected Element\tInsert"));
    AppendMenu(EditMenu, MF_STRING, MNU_PASTE_RUNG, _("Paste Rungs\tCtrl+V or Shift+Insert"));
    AppendMenu(EditMenu, MF_STRING, MNU_PASTE_INTO_RUNG, _("Paste Rungs or Element &Into Rung\tAlt+Insert"));

    AppendMenu(EditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(EditMenu, MF_STRING, MNU_CUT_ELEMENT, _("Cut Selected Element\tAlt+Del"));
    AppendMenu(EditMenu, MF_STRING, MNU_DELETE_ELEMENT, _("&Delete Selected Element\tDel"));
    AppendMenu(EditMenu, MF_STRING, MNU_DELETE_RUNG, _("D&elete Rung\tShift+Del"));

    AppendMenu(EditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(EditMenu, MF_STRING, MNU_REPLACE_ELEMENT, _("Replace Selected Element in Group\tSpace"));

    AppendMenu(EditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(EditMenu, MF_STRING, MNU_SCROLL_UP, _("Scroll Up\tCtrl+Up"));
    AppendMenu(EditMenu, MF_STRING, MNU_SCROLL_DOWN, _("Scroll Down\tCtrl+Down"));
    AppendMenu(EditMenu, MF_STRING, MNU_SCROLL_PgUP, _("Scroll PgUp\tCtrl+PgUp"));
    AppendMenu(EditMenu, MF_STRING, MNU_SCROLL_PgDOWN, _("Scroll PgDown\tCtrl+PgDown"));
    AppendMenu(EditMenu, MF_STRING, MNU_ROLL_HOME, _("Roll Home\tCtrl+Home"));
    AppendMenu(EditMenu, MF_STRING, MNU_ROLL_END, _("Roll End\tCtrl+End"));
    AppendMenu(EditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(EditMenu, MF_STRING, MNU_TAB, _("Moving cursor between the main window and the I/O list\tTab"));

    // instruction popup  menu
    InstructionMenu = CreatePopupMenu();
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_COMMENT, _("Insert Co&mment\t;"));

#if 0
    CourseMenu = CreatePopupMenu();
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_OPEN, _("Insert -+        +- Open-Circuit\tShift+Enter"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_SHORT, _("Insert -+------+- Short-Circuit\tCtrl+Enter"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_MASTER_RLY, _("Insert Master Control Relay"));

    AppendMenu(CourseMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_LABEL, _("Insert LABEL declaration"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_GOTO, _("Insert GOTO Label or Rung"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_SUBPROG, _("Insert SUBPROG declaration"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_RETURN, _("Insert RETURN"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_ENDSUB, _("Insert ENDSUB declaration"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_GOSUB, _("Insert GOSUB call"));

    AppendMenu(CourseMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_SLEEP, _("Insert SLEEP"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_CLRWDT, _("Insert CLRWDT"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_LOCK, _("Insert LOCK"));

    AppendMenu(CourseMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_DELAY, _("Insert DELAY(us)"));
    AppendMenu(CourseMenu, MF_STRING, MNU_INSERT_TIME2DELAY, _("Insert TIME to DELAY converter"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)CourseMenu, _("Operations that change the course of the program"));
#endif

    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    ConMenu = CreatePopupMenu();
    AppendMenu(ConMenu, MF_STRING, MNU_INSERT_CONTACTS, _("Insert &Contacts: Input Pin\tC"));
    AppendMenu(ConMenu, MF_STRING, MNU_INSERT_CONT_RELAY, _("Insert Contacts: Internal Relay\tShift+C"));
    AppendMenu(ConMenu, MF_STRING, MNU_INSERT_CONT_OUTPUT, _("Insert Contacts: Output Pin\tShift+L"));

    AppendMenu(ConMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(ConMenu, MF_STRING, MNU_INSERT_COIL, _("Insert Coi&l: Output Pin\tL"));
    AppendMenu(ConMenu, MF_STRING, MNU_INSERT_COIL_RELAY, _("Insert Coil: Internal Relay\tAlt+L"));

    AppendMenu(ConMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(ConMenu, MF_STRING, MNU_MAKE_NORMAL, _("Make &Normal\tN"));
    AppendMenu(ConMenu, MF_STRING, MNU_NEGATE, _("Make &Negated\tN"));
    AppendMenu(ConMenu, MF_STRING, MNU_MAKE_SET_ONLY, _("Make &Set-Only\tS"));
    AppendMenu(ConMenu, MF_STRING, MNU_MAKE_RESET_ONLY, _("Make &Reset-Only\tR"));
    AppendMenu(ConMenu, MF_STRING, MNU_MAKE_TTRIGGER, _("Make T-trigger"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)ConMenu, _("Contacts and Coils / Relays"));

    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);

    EdgMenu = CreatePopupMenu();
    /////   AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(EdgMenu, MF_STRING, MNU_INSERT_OSR, _("Insert _/OSR_/\\_ (One Shot Rising)\t&/"));
    AppendMenu(EdgMenu, MF_STRING, MNU_INSERT_OSF, _("Insert \\_OSF_/\\_ (One Shot Falling)\t&\\ "));
    AppendMenu(EdgMenu, MF_STRING, MNU_INSERT_ODR, _("Insert _/ODR\"\\/\" (One Drop Rising)"));
    AppendMenu(EdgMenu, MF_STRING, MNU_INSERT_ODF, _("Insert \\_ODF\"\\/\" (One Shot Low)"));
    AppendMenu(EdgMenu, MF_STRING, MNU_INSERT_OSC, _("Insert OSC/\\_/\\_ (Oscillator F=1/(2*Tcycle))"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)EdgMenu, _("Edge"));

    PulseMenu = CreatePopupMenu();
    AppendMenu(PulseMenu, MF_STRING, MNU_INSERT_PULSER, _("Insert PULSER"));
    AppendMenu(PulseMenu, MF_STRING, MNU_INSERT_STEPPER, _("Insert STEPPER"));
    AppendMenu(PulseMenu, MF_STRING, MNU_INSERT_NPULSE, _("EDIT: Insert N PULSE"));
    AppendMenu(PulseMenu, MF_STRING, MNU_INSERT_NPULSE_OFF, _("EDIT: Insert N PULSE OFF"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)PulseMenu, _("Pulse generators"));

    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);

    TmpMenu = CreatePopupMenu();
    /////   AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(TmpMenu, MF_STRING, MNU_INSERT_TON, _("Insert T&ON (Delayed Turn On)\tO"));
    AppendMenu(TmpMenu, MF_STRING, MNU_INSERT_TOF, _("Insert TO&F (Delayed Turn Off)\tF"));
    AppendMenu(TmpMenu, MF_STRING, MNU_INSERT_RTO, _("Insert R&TO (Retentive Delayed Turn On)\tT"));
    AppendMenu(TmpMenu, MF_STRING, MNU_INSERT_RTL, _("Insert RTL (Retentive Delayed Turn On If Low Input)"));
    AppendMenu(TmpMenu, MF_STRING, MNU_INSERT_TCY, _("Insert TCY (Cyclic On/Off)"));
    AppendMenu(TmpMenu, MF_STRING, MNU_INSERT_THI, _("Insert THI (High Delay)"));
    AppendMenu(TmpMenu, MF_STRING, MNU_INSERT_TLO, _("Insert TLO (Low Delay)"));
    AppendMenu(TmpMenu, MF_STRING, MNU_INSERT_TIME2COUNT, _("Insert TIME to COUNTER converter"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)TmpMenu, _("Temporization"));

    CntMenu = CreatePopupMenu();
    /////   AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(CntMenu, MF_STRING, MNU_INSERT_CTU, _("Insert CT&U (Count Up)\tU"));
    AppendMenu(CntMenu, MF_STRING, MNU_INSERT_CTD, _("Insert CT&D (Count Down)\tI"));
    AppendMenu(CntMenu, MF_STRING, MNU_INSERT_CTC, _("Insert CT&C (Count Circular)\tJ"));
    AppendMenu(CntMenu, MF_STRING, MNU_INSERT_CTR, _("Insert CT&R (Count Circular Reversive)\tK"));
    AppendMenu(CntMenu, MF_STRING, MNU_INSERT_RES, _("Insert R&ES (Counter/RTO/RTL/PWM Reset)\tE"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)CntMenu, _("Counter"));

    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    /*
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_EQU,
        _("Insert EQU (Compare for Equals)\t="));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_NEQ,
        _("Insert NEQ (Compare for Not Equals)\t!"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_GRT,
        _("Insert GRT (Compare for Greater Than)\t>"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_GEQ,
        _("Insert GEQ (Compare for Greater Than or Equal)\t."));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_LES,
        _("Insert LES (Compare for Less Than)\t<"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_LEQ,
        _("Insert LEQ (Compare for Less Than or Equal)\t,"));
*/
    CmpMenu = CreatePopupMenu();
    AppendMenu(CmpMenu, MF_STRING, MNU_INSERT_EQU, _("Insert EQU (Compare for Equals)\t="));
    AppendMenu(CmpMenu, MF_STRING, MNU_INSERT_NEQ, _("Insert NEQ (Compare for Not Equals)\t!"));
    AppendMenu(CmpMenu, MF_STRING, MNU_INSERT_GRT, _("Insert GRT (Compare for Greater Than)\t>"));
    AppendMenu(CmpMenu, MF_STRING, MNU_INSERT_GEQ, _("Insert GEQ (Compare for Greater Than or Equal)\t."));
    AppendMenu(CmpMenu, MF_STRING, MNU_INSERT_LES, _("Insert LES (Compare for Less Than)\t<"));
    AppendMenu(CmpMenu, MF_STRING, MNU_INSERT_LEQ, _("Insert LEQ (Compare for Less Than or Equal)\t,"));
    AppendMenu(CmpMenu, MF_STRING, MNU_INSERT_IF_BIT_SET, _("Insert Test If Bit Set"));
    AppendMenu(CmpMenu, MF_STRING, MNU_INSERT_IF_BIT_CLEAR, _("Insert Test If Bit Clear"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)CmpMenu, _("Compare variable"));

    /*
    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_MOV,
        _("Insert MOV (Move)\tM"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_ADD,
        _("Insert ADD (16-bit Integer Add)\t+"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_SUB,
        _("Insert SUB (16-bit Integer Subtract)\t-"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_MUL,
        _("Insert MUL (16-bit Integer Multiply)\t*"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_DIV,
        _("Insert DIV (16-bit Integer Divide)\tD"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_MOD,_("TODO: Insert MOD (Integer Divide Remainder)"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_NEG,_("TODO: Insert NEG (Integer Negate)"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_BIN2BCD, _("TODO: Insert BIN2BCD"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_BCD2BIN, _("TODO: Insert BCD2BIN"));
*/
    SignedMenu = CreatePopupMenu();
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_MOV, _("Insert MOV (Move)\tM"));
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_ADD, _("Insert ADD (16-bit Integer Add)\t+"));
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_SUB, _("Insert SUB (16-bit Integer Subtract)\t-"));
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_MUL, _("Insert MUL (16-bit Integer Multiply)\t*"));
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_DIV, _("Insert DIV (16-bit Integer Divide)\tD"));
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_MOD, _("Insert MOD (Integer Divide Remainder)"));
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_NEG, _("Insert NEG (Integer Negate)"));
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_RANDOM, _("Insert Random"));
    AppendMenu(SignedMenu, MF_STRING, MNU_INSERT_SEED_RANDOM, _("Insert Seed of Random"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)SignedMenu, _("Signed variable operations"));

#if 0
    BitwiseMenu = CreatePopupMenu();
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_AND, _("Insert bitwise AND"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_OR, _("Insert bitwise OR     |"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_XOR, _("Insert bitwise XOR  ^"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_NOT, _("Insert bitwise NOT  ~"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_SHL, _("Insert SHL << arithmetic, logic shift to the left"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_SHR, _("Insert SHR >> arithmetic shift to the right"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_SR0, _("Insert SR0 >> logic shift to the right"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_ROL, _("Insert ROL cyclic shift to the left"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_ROR, _("Insert ROR cyclic shift to the right"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_SWAP, _("Insert SWAP"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_OPPOSITE, _("Insert OPPOSITE"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_SET_BIT, _("Insert Set Bit #"));
    AppendMenu(BitwiseMenu, MF_STRING, MNU_INSERT_CLEAR_BIT, _("Insert Clear Bit #"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)BitwiseMenu, _("Bitwise variable operations (Unsigned)"));

    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_SHIFT_REG, _("Insert Shift Register"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_LUT, _("Insert Look-Up Table"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_PWL, _("Insert Piecewise Linear"));

    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);

    FormatStrMenu = CreatePopupMenu();
    AppendMenu(FormatStrMenu, MF_STRING, MNU_INSERT_STRING, _("Insert Formatted String"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)FormatStrMenu, _("Formatted string"));

    UrtMenu = CreatePopupMenu();
    // AppendMenu(UrtMenu, MF_STRING, MNU_INSERT_UART_WRITE, _("Insert String Over UART"));
    AppendMenu(UrtMenu, MF_STRING, MNU_INSERT_FMTD_STRING, _("Insert Formatted String Over &UART"));
    AppendMenu(UrtMenu, MF_STRING, MNU_INSERT_UART_SEND, _("Insert &UART SEND"));
    AppendMenu(UrtMenu, MF_STRING, MNU_INSERT_UART_RECV, _("Insert &UART RECEIVE"));
    //  AppendMenu(UrtMenu, MF_STRING, MNU_INSERT_UART_SENDn, _("Insert &UART SENDn Variable")); // obsolete
    //  AppendMenu(UrtMenu, MF_STRING, MNU_INSERT_UART_RECVn, _("Insert &UART RECEIVE Variable")); // obsolete
    AppendMenu(UrtMenu, MF_STRING, MNU_INSERT_UART_SEND_READY, _("Insert &UART SEND: Is ready to send ?"));
    AppendMenu(UrtMenu, MF_STRING, MNU_INSERT_UART_RECV_AVAIL, _("Insert &UART RECEIVE: Is data available ?"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)UrtMenu, _("UART functions"));

    SpiMenu = CreatePopupMenu();
    AppendMenu(SpiMenu, MF_STRING, MNU_INSERT_SPI, _("Insert SPI Send/Receive"));
    AppendMenu(SpiMenu, MF_STRING, MNU_INSERT_SPI_WRITE, _("Insert SPI Write"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)SpiMenu, _("SPI functions"));

    I2cMenu = CreatePopupMenu();
    AppendMenu(I2cMenu, MF_STRING, MNU_INSERT_I2C_READ, _("Insert I2C Read"));
    AppendMenu(I2cMenu, MF_STRING, MNU_INSERT_I2C_WRITE, _("Insert I2C Write"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)I2cMenu, _("I2C functions"));

    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_SET_PWM, _("Insert Set &PWM Output\tP"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_READ_ADC, _("Insert &A/D Converter Read\tA"));
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_PERSIST, _("Insert Make Persistent"));

    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_SET_PWM_SOFT, _("TODO: Insert Software &PWM (AVR136 AppNote)\tP"));

    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(InstructionMenu, MF_STRING, MNU_INSERT_QUAD_ENCOD, _("Insert QUADRATURE ENCODER"));
    DisplayMenu = CreatePopupMenu();
    AppendMenu(DisplayMenu, MF_STRING, MNU_INSERT_BIN2BCD, _("Insert BIN2BCD converter"));
    AppendMenu(DisplayMenu, MF_STRING, MNU_INSERT_BCD2BIN, _("Insert BCD2BIN converter"));
    AppendMenu(DisplayMenu, MF_STRING, MNU_INSERT_BUS, _("Insert BUS tracer"));
    AppendMenu(DisplayMenu, MF_STRING, MNU_INSERT_7SEG, _("Insert 7-SEGMENT FONT converter"));
    AppendMenu(DisplayMenu, MF_STRING, MNU_INSERT_9SEG, _("Insert 9-SEGMENT FONT converter"));
    AppendMenu(DisplayMenu, MF_STRING, MNU_INSERT_14SEG, _("Insert 14-SEGMENT FONT converter"));
    AppendMenu(DisplayMenu, MF_STRING, MNU_INSERT_16SEG, _("Insert 16-SEGMENT FONT converter"));
    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)DisplayMenu, _("Displays"));

#ifdef USE_SFR
    AppendMenu(InstructionMenu, MF_SEPARATOR, 0, nullptr);
    // Special function menu
    SpecialFunction = CreatePopupMenu();
    AppendMenu(SpecialFunction, MF_STRING, MNU_OPEN_SFR, _("Replace the obsolete elements"));
    AppendMenu(SpecialFunction, MF_STRING, MNU_INSERT_SFR, _("Insert Read From SFR"));
    AppendMenu(SpecialFunction, MF_STRING, MNU_INSERT_SFW, _("Insert Write To SFR"));
    AppendMenu(SpecialFunction, MF_STRING, MNU_INSERT_SSFB, _("Insert Set Bit In SFR"));
    AppendMenu(SpecialFunction, MF_STRING, MNU_INSERT_csFB, _("Insert Clear Bit In SFR"));
    AppendMenu(SpecialFunction, MF_STRING, MNU_INSERT_TSFB, _("Insert Test If Bit Set in SFR"));
    AppendMenu(SpecialFunction, MF_STRING, MNU_INSERT_T_C_SFB, _("Insert Test If Bit Clear in SFR"));

    AppendMenu(InstructionMenu, MF_STRING | MF_POPUP, (UINT_PTR)SpecialFunction, _("Special Function for AVR (Obsolete)"));
#endif
#endif
    settings = CreatePopupMenu();
    ProcessorMenu = CreatePopupMenu();
#if 1
    /// Modified by JG3
    ProcAvrMenu = CreatePopupMenu();
    ProcPic16Menu = CreatePopupMenu();
    ProcPic18Menu = CreatePopupMenu();
    ProcEspMenu = CreatePopupMenu();
    ProcArmMenu = CreatePopupMenu();
    ProcOthersMenu = CreatePopupMenu();
    static HMENU ProcMenu = nullptr;
    static HMENU ProcMenu_ = nullptr;
    Core core_ = supportedMcus()[0].core;
    for(uint32_t i = 0; i < supportedMcus().size(); i++) {
        Core core = supportedMcus()[i].core;
        if((core > AVRcores) && (core < AVRcores_) && (supportedMcus()[i].whichIsa == ISA_AVR))
            ProcMenu = ProcAvrMenu;
        else if((core > PICcores) && (core < PICcores_)) {
            if(core == PIC18HighEndCore16bit)
                ProcMenu = ProcPic18Menu;
            else
                ProcMenu = ProcPic16Menu;
        } else if((core > ESPcores) && (core < ESPcores_))
            ProcMenu = ProcEspMenu;
        else if((core > ARMcores) && (core < ARMcores_))
            ProcMenu = ProcArmMenu;
        else // if(core > PCcores)
            ProcMenu = ProcOthersMenu;

        if((core_ != supportedMcus()[i].core) && (ProcMenu_ == ProcMenu)) {
            core_ = supportedMcus()[i].core;
            AppendMenu(ProcMenu, MF_SEPARATOR, 0, "");
        }
        AppendMenu(ProcMenu, MF_STRING, MNU_PROCESSOR_0 + i, supportedMcus()[i].mcuName);
        ProcMenu_ = ProcMenu;
    }
    AppendMenu(ProcessorMenu, MF_STRING | MF_POPUP, (UINT_PTR)ProcAvrMenu, _("Atmel AVR MCUs"));            /// To translate
    AppendMenu(ProcessorMenu, MF_STRING | MF_POPUP, (UINT_PTR)ProcPic16Menu, _("Microchip Pic10-16 MCUs")); /// To translate
    AppendMenu(ProcessorMenu, MF_STRING | MF_POPUP, (UINT_PTR)ProcPic18Menu, _("Microchip Pic18 MCUs"));    /// To translate
    AppendMenu(ProcessorMenu, MF_STRING | MF_POPUP, (UINT_PTR)ProcArmMenu, _("ARM MCUs"));                  /// To translate
    AppendMenu(ProcessorMenu, MF_STRING | MF_POPUP, (UINT_PTR)ProcEspMenu, _("ESP MCUs"));                  /// To translate
    AppendMenu(ProcessorMenu, MF_STRING | MF_POPUP, (UINT_PTR)ProcOthersMenu, _("Other MCUs"));             /// To translate
#endif                                                                                                            ///

    /*
    Core core = supportedMcus()[0].core;
    for(uint32_t i = 0; i < supportedMcus().size(); i++) {
        if(core != supportedMcus()[i].core) {
            core = supportedMcus()[i].core;
            AppendMenu(ProcessorMenu, MF_SEPARATOR, 0, ""); //separate AVR MCU core
        }
        AppendMenu(ProcessorMenu, MF_STRING, MNU_PROCESSOR_0 + i, supportedMcus()[i].mcuName);
    }
*/

    AppendMenu(ProcessorMenu, MF_SEPARATOR, 0, "");
    AppendMenu(ProcessorMenu, MF_STRING, MNU_PROCESSOR_0 + supportedMcus().size(), _("(no microcontroller)"));
    AppendMenu(settings, MF_STRING | MF_POPUP, (UINT_PTR)ProcessorMenu, _("&Microcontroller"));
    AppendMenu(settings, MF_STRING, MNU_MCU_SETTINGS, _("&MCU Parameters...\tCtrl+F5"));
//    AppendMenu(settings, MF_STRING, MNU_PULL_UP_RESISTORS, _("Set Pull-up input resistors"));

#if 0
    ProcessorMenu2 = CreatePopupMenu();
    AppendMenu(settings, MF_STRING | MF_POPUP, (UINT_PTR)ProcessorMenu2, _("Microcontrollers: TODO and DONE"));
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Atmel AVR ATmega32U4 44-Pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Atmel AVR ATmega32 44-Pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Atmel AVR ATmega328 32-Pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Atmel AVR ATtiny85 8/20-Pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Atmel AVR ATtiny10 6-Pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "TODO: Atmel AVR ATtinyXXX");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "TODO: Atmel AVR AT90USB646");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "TODO: Atmel AVR AT90USB1286");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "TODO: Atmel AVR AT90USB1287");
    AppendMenu(ProcessorMenu2, MF_SEPARATOR, 0, "");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW_PIC12, "DONE: Microchip PIC10F220/222 6-SOT");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW_PIC12, "DONE: Microchip PIC10F200/202/204/206 6-SOT");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "TODO: Microchip PIC12Fxxx");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW_PIC12, "DONE: Microchip PIC12F752 8-pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW_PIC12, "DONE: Microchip PIC12F683 8-pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW_PIC12, "DONE: Microchip PIC12F675 8-pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW_PIC12, "DONE: Microchip PIC12F629 8-pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Microchip PIC16F72 28-Pin PDIP, SOIC, SSOP");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Microchip PIC16F887 44-TQFP");
    AppendMenu(ProcessorMenu2, MF_SEPARATOR, 0, "");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Microchip PIC16F1512 - PIC16F1527");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Microchip PIC16F1512 28-Pin SPDIP, SOIC, SSOP");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Microchip PIC16F1516 28-Pin SPDIP, SOIC, SSOP");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Microchip PIC16F1527 64-Pin packages");
    AppendMenu(ProcessorMenu2, MF_STRING, MNU_PROCESSOR_NEW, "DONE: Microchip PIC16F1933 - PIC16F1947");
#endif

    // simulate popup menu
    SimulateMenu = CreatePopupMenu();
    AppendMenu(SimulateMenu, MF_STRING, MNU_SIMULATION_MODE, _("Si&mulation Mode\tCtrl+M or F7"));
    AppendMenu(SimulateMenu, MF_STRING | MF_GRAYED, MNU_START_SIMULATION, _("Start &Real-Time Simulation\tCtrl+R or F8"));
    AppendMenu(SimulateMenu, MF_STRING | MF_GRAYED, MNU_STOP_SIMULATION, _("&Halt Simulation\tCtrl+H or F8"));
    AppendMenu(SimulateMenu, MF_STRING | MF_GRAYED, MNU_SINGLE_CYCLE, _("Single &Cycle\tSpace"));

    compile = CreatePopupMenu();
    //AppendMenu(compile, MF_STRING, MNU_COMPILE, _("&Compile\tF5"));
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_AS, _("Compile &As..."));
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_IHEX, _("Compile HEX->ASM"));
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_IHEX, _("Compile ASM->HEX"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_ANSIC, _("Compile ANSIC"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_HI_TECH_C, _("Compile HI-TECH C for PIC"));
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_CCS_PIC_C, _("Compile CCS C for PIC"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_GNUC, _("Compile Atmel WinAVR C"));
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_AVRGCC, _("Compile Atmel AVR-GCC"));
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_CODEVISIONAVR, _("Compile CodeVisionAVR C"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_ARDUINO, _("Compile Sketch for ARDUINO"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_ARMGCC, _("Compile ARM-GCC for 32-bit ARM"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_PASCAL, _("Compile PASCAL"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    AppendMenu(compile, MF_STRING, MNU_COMPILE_INT, _("Compile Interpretable Byte Code"));
    //AppendMenu(compile, MF_STRING, MNU_COMPILE_XINT, _("Compile Interpretable Extended Byte Code"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_BUILD_ALL, _("Build C Solution\tF6"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_FLASH_BAT, _("Call flashMcu\tF9"));
    //AppendMenu(compile, MF_STRING, MNU_READ_BAT, _("Call readMcu\tCtrl+F9"));
    //AppendMenu(compile, MF_SEPARATOR, 0, nullptr);
    //AppendMenu(compile, MF_STRING, MNU_CLEAR_BAT, _("Call clear"));

    ConfigMenu = CreatePopupMenu();
    SchemeMenu = CreatePopupMenu();
    SetSyntaxHighlightingColours();
    for(int i = 0; i < NUM_SUPPORTED_SCHEMES; i++) {
        AppendMenu(SchemeMenu, MF_STRING, MNU_SCHEME_BLACK + i, Schemes[i].sName);
    }
    AppendMenu(SchemeMenu, MF_SEPARATOR, 0, "");
    AppendMenu(SchemeMenu, MF_STRING, MNU_SELECT_COLOR, _("Select user colors"));
    AppendMenu(ConfigMenu, MF_STRING | MF_POPUP, (UINT_PTR)SchemeMenu, _("Select color scheme"));

    help = CreatePopupMenu();
    AppendMenu(help, MF_STRING, MNU_MANUAL, _("&Manual...\tF1"));
    AppendMenu(help, MF_STRING, MNU_HOW, _("HOW TO:..."));
    AppendMenu(help, MF_STRING, MNU_ABOUT, _("&About..."));
    AppendMenu(help, MF_STRING, MNU_RELEASE, _("Releases..."));
    AppendMenu(help, MF_STRING, MNU_CHANGES, _("Latest release changes..."));
    AppendMenu(help, MF_STRING, MNU_FORUM, _("LDmicro Forum..."));
    AppendMenu(help, MF_STRING, MNU_ISSUE, _("Create new issue..."));
    AppendMenu(help, MF_STRING, MNU_EMAIL, _("E-mail..."));

    TopMenu = CreateMenu();
    AppendMenu(TopMenu, MF_STRING | MF_POPUP, (UINT_PTR)FileMenu, _("&File"));
    AppendMenu(TopMenu, MF_STRING | MF_POPUP, (UINT_PTR)EditMenu, _("&Edit"));
    AppendMenu(TopMenu, MF_STRING | MF_POPUP, (UINT_PTR)settings, _("&Settings"));
    AppendMenu(TopMenu, MF_STRING | MF_POPUP, (UINT_PTR)InstructionMenu, _("&Instruction"));
    AppendMenu(TopMenu, MF_STRING | MF_POPUP, (UINT_PTR)SimulateMenu, _("Si&mulate"));
    AppendMenu(TopMenu, MF_STRING | MF_POPUP, (UINT_PTR)compile, _("&Compile"));
    AppendMenu(TopMenu, MF_STRING | MF_POPUP, (UINT_PTR)ConfigMenu, _("Config"));
    AppendMenu(TopMenu, MF_STRING | MF_POPUP, (UINT_PTR)help, _("&Help"));

    return TopMenu;
}

//-----------------------------------------------------------------------------
// Adjust the size and visibility of the scrollbars as necessary, either due
// to a change in the size of the program or a change in the size of the
// window.
//-----------------------------------------------------------------------------
void RefreshScrollbars()
{
    SCROLLINFO vert, horiz;
    SetUpScrollbars(&NeedHoriz, &horiz, &vert);
    SetScrollInfo(HorizScrollBar, SB_CTL, &horiz, true);
    SetScrollInfo(VertScrollBar, SB_CTL, &vert, true);

    RECT main;
    GetClientRect(MainWindow, &main);

    if(NeedHoriz) {
        MoveWindow(HorizScrollBar, 0, IoListTop - ScrollHeight - 2, main.right - ScrollWidth - 2, ScrollHeight, true);
        ShowWindow(HorizScrollBar, SW_SHOW);
        EnableWindow(HorizScrollBar, true);
    } else {
        ShowWindow(HorizScrollBar, SW_HIDE);
    }
    MoveWindow(VertScrollBar, main.right - ScrollWidth - 2, 1, ScrollWidth, NeedHoriz ? (IoListTop - ScrollHeight - 4) : (IoListTop - 3), true);

    MoveWindow(VertScrollBar, main.right - ScrollWidth - 2, 1, ScrollWidth, NeedHoriz ? (IoListTop - ScrollHeight - 4) : (IoListTop - 3), true);

    InvalidateRect(MainWindow, nullptr, false);
}

//-----------------------------------------------------------------------------
// Respond to a WM_VSCROLL sent to the main window, presumably by the one and
// only vertical scrollbar that it has as a child.
//-----------------------------------------------------------------------------
void VscrollProc(WPARAM wParam)
{
    int prevY = ScrollYOffset;
    switch(LOWORD(wParam)) {
        case SB_LINEUP:
        case SB_PAGEUP:
            if(ScrollYOffset > 0) {
                ScrollYOffset--;
            }
            break;

        case SB_LINEDOWN:
        case SB_PAGEDOWN:
            if(ScrollYOffset < ScrollYOffsetMax) {
                ScrollYOffset++;
            }
            break;

        case SB_TOP:
            ScrollYOffset = 0;
            break;

        case SB_BOTTOM:
            ScrollYOffset = ScrollYOffsetMax;
            break;

        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            ScrollYOffset = HIWORD(wParam);
            break;
    }
    if(prevY != ScrollYOffset) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        si.nPos = ScrollYOffset;
        SetScrollInfo(VertScrollBar, SB_CTL, &si, true);

        InvalidateRect(MainWindow, nullptr, false);
    }
}

//-----------------------------------------------------------------------------
// Respond to a WM_HSCROLL sent to the main window, presumably by the one and
// only horizontal scrollbar that it has as a child.
//-----------------------------------------------------------------------------
void HscrollProc(WPARAM wParam)
{
    int prevX = ScrollXOffset;
    switch(LOWORD(wParam)) {
        case SB_LINEUP:
            ScrollXOffset -= FONT_WIDTH;
            break;

        case SB_PAGEUP:
            ScrollXOffset -= POS_WIDTH * FONT_WIDTH;
            break;

        case SB_LINEDOWN:
            ScrollXOffset += FONT_WIDTH;
            break;

        case SB_PAGEDOWN:
            ScrollXOffset += POS_WIDTH * FONT_WIDTH;
            break;

        case SB_TOP:
            ScrollXOffset = 0;
            break;

        case SB_BOTTOM:
            ScrollXOffset = ScrollXOffsetMax;
            break;

        case SB_THUMBTRACK:
        case SB_THUMBPOSITION:
            ScrollXOffset = HIWORD(wParam);
            break;
    }

    if(ScrollXOffset > ScrollXOffsetMax)
        ScrollXOffset = ScrollXOffsetMax;
    if(ScrollXOffset < 0)
        ScrollXOffset = 0;

    if(prevX != ScrollXOffset) {
        SCROLLINFO si;
        si.cbSize = sizeof(si);
        si.fMask = SIF_POS;
        si.nPos = ScrollXOffset;
        SetScrollInfo(HorizScrollBar, SB_CTL, &si, true);

        InvalidateRect(MainWindow, nullptr, false);
    }
}

//-----------------------------------------------------------------------------
void RefreshStatusBar()
{
    SendMessage(StatusBar, SB_SETTEXT, 0, (LPARAM)_(ProgramChangedNotSaved ? _("modified") : "        "));

    if(Prog.mcu()) {
        SendMessage(StatusBar, SB_SETTEXT, 1, (LPARAM)Prog.mcu()->mcuName);
    } else {
        SendMessage(StatusBar, SB_SETTEXT, 1, (LPARAM)_("no MCU selected"));
    }
    char buf[1000];
    char Tunits[3];
    char Funits[3];
    char F2units[3];
    char TNunits[3];

    double T = SIprefix(1.0 * Prog.cycleTime / 1000000, Tunits);

    double F = 0;
    double F2 = 0;
    if(Prog.cycleTime > 0) {
        F = SIprefix(1000000.0 / Prog.cycleTime, Funits);
        F2 = SIprefix(1000000.0 / Prog.cycleTime / 2, F2units);
    }

    double TN = SIprefix(1.0 * Prog.cycleTime * CyclesCount / 1000000, TNunits);

    if(Prog.cycleTime > 0) {
        sprintf(buf, "Tcycle=%.6g %ss F=%.6g %sHz F/2=%.6g %sHz Ncycle=%u T=%.6g %ss", T, Tunits, F, Funits, F2, F2units, CyclesCount, TN, TNunits);
    } else {
        sprintf(buf, "Tcycle=%.6g %ss Ncycle=%u T=%.6g %ss", T, Tunits, CyclesCount, TN, TNunits);
    }
    SendMessage(StatusBar, SB_SETTEXT, 3, (LPARAM)buf);

    if(Prog.mcu() && (Prog.mcu()->whichIsa == ISA_NETZER || Prog.mcu()->whichIsa == ISA_INTERPRETED || Prog.mcu()->whichIsa == ISA_XINTERPRETED)) {
        strcpy(buf, "");
    } else {
        sprintf(buf, _("processor clock %.9g MHz"), (double)Prog.mcuClock / 1000000.0);
    }
    SendMessage(StatusBar, SB_SETTEXT, 2, (LPARAM)buf);
}

//-----------------------------------------------------------------------------
// Cause the status bar and the list view to be in sync with the actual data
// structures describing the settings and the I/O configuration. Listview
// does callbacks to get the strings it displays, so it just needs to know
// how many elements to populate.
//-----------------------------------------------------------------------------
void RefreshControlsToSettings()
{
    if(!IoListOutOfSync) {
        IoListSelectionPoint = -1;
        for(int i = 0; i < Prog.io.count; i++) {
            if(ListView_GetItemState(IoList, i, LVIS_SELECTED)) {
                IoListSelectionPoint = i;
                break;
            }
        }
        IoListSelectionPoint = SendMessage(IoList, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    }

    ListView_DeleteAllItems(IoList);
    for(int i = 0; i < Prog.io.count; i++) {
        LVITEM lvi;
        lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
        lvi.state = lvi.stateMask = 0;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = LPSTR_TEXTCALLBACK;
        lvi.lParam = i;

        if(ListView_InsertItem(IoList, &lvi) < 0)
            oops();
    }

    if(IoListSelectionPoint < 0)
        IoListSelectionPoint = 0;
    if(IoListSelectionPoint >= 0) {
        for(int i = 0; i < Prog.io.count; i++) {
            ListView_SetItemState(IoList, i, 0, LVIS_SELECTED);
            ListView_SetItemState(IoList, i, 0, LVIS_FOCUSED);
        }
        ListView_SetItemState(IoList, IoListSelectionPoint, LVIS_SELECTED, LVIS_SELECTED);
        ListView_SetItemState(IoList, IoListSelectionPoint, LVIS_FOCUSED, LVIS_FOCUSED);
        ListView_EnsureVisible(IoList, IoListSelectionPoint, false);
    }
    IoListOutOfSync = false;

    RefreshStatusBar();

    for(uint32_t i = 0; i < supportedMcus().size(); i++) {
        if(&(supportedMcus()[i]) == Prog.mcu()) {
            CheckMenuItem(ProcessorMenu, MNU_PROCESSOR_0 + i, MF_CHECKED);
        } else {
            CheckMenuItem(ProcessorMenu, MNU_PROCESSOR_0 + i, MF_UNCHECKED);
        }
    }
    // `(no microcontroller)' setting
    if(!Prog.mcu()) {
        CheckMenuItem(ProcessorMenu, MNU_PROCESSOR_0 + supportedMcus().size(), MF_CHECKED);
    } else {
        CheckMenuItem(ProcessorMenu, MNU_PROCESSOR_0 + supportedMcus().size(), MF_UNCHECKED);
    }

    for(uint32_t i = 0; i < NUM_SUPPORTED_SCHEMES; i++)
        CheckMenuItem(SchemeMenu, MNU_SCHEME_BLACK + i, (i == scheme) ? MF_CHECKED : MF_UNCHECKED);
}

//-----------------------------------------------------------------------------
// Regenerate the I/O list, keeping the selection in the same place if
// possible.
//-----------------------------------------------------------------------------
void GenerateIoListDontLoseSelection()
{
    int SaveIoListSelectionPoint = IoListSelectionPoint;
    IoListSelectionPoint = -1;
    for(int i = 0; i < Prog.io.count; i++) {
        if(ListView_GetItemState(IoList, i, LVIS_SELECTED)) {
            IoListSelectionPoint = i;
            break;
        }
    }
    IoListSelectionPoint = GenerateIoList(IoListSelectionPoint);

    // can't just update the listview index; if I/O has been added then the
    // new selection point might be out of range till we refill it
    if(IoListSelectionPoint >= 0) {
        if(IoListSelectionPoint != SaveIoListSelectionPoint) {
            IoListOutOfSync = true;
            strcpy(IoListSelectionName, Prog.io.assignment[IoListSelectionPoint].name);
        }
    }
    RefreshControlsToSettings();
}

//-----------------------------------------------------------------------------
// Called when the main window has been resized. Adjust the size of the
// status bar and the listview to reflect the new window size.
//-----------------------------------------------------------------------------
void MainWindowResized()
{
    RECT main;
    GetClientRect(MainWindow, &main);
    if(main.bottom) {
        RECT status;
        GetWindowRect(StatusBar, &status);
        int statusHeight = status.bottom - status.top;

        MoveWindow(StatusBar, 0, main.bottom - statusHeight, main.right, statusHeight, true);

        // Make sure that the I/O list can't disappear entirely.
        if(IoListHeight < 30) {
            IoListHeight = 30;
        }
        IoListTop = main.bottom - IoListHeight - statusHeight;
        // Make sure that we can't drag the top of the I/O list above the
        // bottom of the menu bar, because it then becomes inaccessible.
        if(IoListTop < 5) {
            IoListHeight = main.bottom - statusHeight - 5;
            IoListTop = main.bottom - IoListHeight - statusHeight;
        }
        MoveWindow(IoList, 0, IoListTop, main.right, IoListHeight, true);

        RefreshScrollbars();
    }
    InvalidateRect(MainWindow, nullptr, false);
}

//-----------------------------------------------------------------------------
// Toggle whether we are in simulation mode. A lot of options are only
// available in one mode or the other.
//-----------------------------------------------------------------------------
void ToggleSimulationMode(bool doSimulateOneRung)
{
    /*
    ///// Added by JG to avoid some bugs when clicking on non assigned contacts in simulation mode
    if (!Prog.mcu())
    {
        Error(_("Must choose a target microcontroller before simulating."));
        return;
    }
    /////
*/
    InSimulationMode = !InSimulationMode;

    if(InSimulationMode) {
        EnableMenuItem(SimulateMenu, MNU_START_SIMULATION, MF_ENABLED);
        EnableMenuItem(SimulateMenu, MNU_SINGLE_CYCLE, MF_ENABLED);

        EnableMenuItem(FileMenu, MNU_OPEN, MF_GRAYED);
        EnableMenuItem(FileMenu, MNU_SAVE, MF_GRAYED);
        EnableMenuItem(FileMenu, MNU_SAVE_01, MF_GRAYED);
        EnableMenuItem(FileMenu, MNU_SAVE_02, MF_GRAYED);
        EnableMenuItem(FileMenu, MNU_SAVE_AS, MF_GRAYED);
        EnableMenuItem(FileMenu, MNU_NEW, MF_GRAYED);
        EnableMenuItem(FileMenu, MNU_EXPORT, MF_GRAYED);

        EnableMenuItem(TopMenu, 1, MF_GRAYED | MF_BYPOSITION);
        EnableMenuItem(TopMenu, 2, MF_GRAYED | MF_BYPOSITION);
        EnableMenuItem(TopMenu, 3, MF_GRAYED | MF_BYPOSITION);
        EnableMenuItem(TopMenu, 5, MF_GRAYED | MF_BYPOSITION);

        CheckMenuItem(SimulateMenu, MNU_SIMULATION_MODE, MF_CHECKED);

        if(ClearSimulationData()) {
            // Recheck InSimulationMode, because there could have been a compile
            // error, which would have kicked us out of simulation mode.
            if(UartFunctionUsed() && InSimulationMode)
                ShowSimulationWindow(SIM_UART);
            if(SpiFunctionUsed() && InSimulationMode)
                ShowSimulationWindow(SIM_SPI);
            if(I2cFunctionUsed() && InSimulationMode)
                ShowSimulationWindow(SIM_I2C);
            /////
            /////   SimulateOneCycle(true); // If comment this line, then you can see initial state in ladder diagram. It is same interesting.
            /////   Commented by JG
        }
    } else {
        RealTimeSimulationRunning = false;
        KillTimer(MainWindow, TIMER_SIMULATE);

        EnableMenuItem(SimulateMenu, MNU_START_SIMULATION, MF_GRAYED);
        EnableMenuItem(SimulateMenu, MNU_STOP_SIMULATION, MF_GRAYED);
        EnableMenuItem(SimulateMenu, MNU_SINGLE_CYCLE, MF_GRAYED);

        EnableMenuItem(FileMenu, MNU_OPEN, MF_ENABLED);
        EnableMenuItem(FileMenu, MNU_SAVE, MF_ENABLED);
        EnableMenuItem(FileMenu, MNU_SAVE_01, MF_ENABLED);
        EnableMenuItem(FileMenu, MNU_SAVE_02, MF_ENABLED);
        EnableMenuItem(FileMenu, MNU_SAVE_AS, MF_ENABLED);
        EnableMenuItem(FileMenu, MNU_NEW, MF_ENABLED);
        EnableMenuItem(FileMenu, MNU_EXPORT, MF_ENABLED);

        EnableMenuItem(TopMenu, 1, MF_ENABLED | MF_BYPOSITION);
        EnableMenuItem(TopMenu, 2, MF_ENABLED | MF_BYPOSITION);
        EnableMenuItem(TopMenu, 3, MF_ENABLED | MF_BYPOSITION);
        EnableMenuItem(TopMenu, 5, MF_ENABLED | MF_BYPOSITION);

        CheckMenuItem(SimulateMenu, MNU_SIMULATION_MODE, MF_UNCHECKED);

        if(UartFunctionUsed())
            DestroySimulationWindow(UartSimulationWindow);
        if(SpiFunctionUsed())
            DestroySimulationWindow(SpiSimulationWindow);
        if(I2cFunctionUsed())
            DestroySimulationWindow(I2cSimulationWindow);
    }

    UpdateMainWindowTitleBar();

    DrawMenuBar(MainWindow);
    InvalidateRect(MainWindow, nullptr, false);
    ListView_RedrawItems(IoList, 0, Prog.io.count - 1);
}

void ToggleSimulationMode()
{
    ToggleSimulationMode(false);
}

//-----------------------------------------------------------------------------
// Start real-time simulation. Have to update the controls grayed status
// to reflect this.
//-----------------------------------------------------------------------------
void StartSimulation()
{
    RealTimeSimulationRunning = true;

    EnableMenuItem(SimulateMenu, MNU_SINGLE_CYCLE, MF_GRAYED);
    EnableMenuItem(SimulateMenu, MNU_START_SIMULATION, MF_GRAYED);
    EnableMenuItem(SimulateMenu, MNU_STOP_SIMULATION, MF_ENABLED);
    StartSimulationTimer();

    UpdateMainWindowTitleBar();

    if(UartFunctionUsed())
        ShowWindow(UartSimulationWindow, true);
    if(SpiFunctionUsed())
        ShowWindow(SpiSimulationWindow, true);
    if(I2cFunctionUsed())
        ShowWindow(I2cSimulationWindow, true);
}

//-----------------------------------------------------------------------------
// Stop real-time simulation. Have to update the controls grayed status
// to reflect this.
//-----------------------------------------------------------------------------
void StopSimulation()
{
    RealTimeSimulationRunning = false;

    EnableMenuItem(SimulateMenu, MNU_SINGLE_CYCLE, MF_ENABLED);
    EnableMenuItem(SimulateMenu, MNU_START_SIMULATION, MF_ENABLED);
    EnableMenuItem(SimulateMenu, MNU_STOP_SIMULATION, MF_GRAYED);
    KillTimer(MainWindow, TIMER_SIMULATE);

    UpdateMainWindowTitleBar();
}
