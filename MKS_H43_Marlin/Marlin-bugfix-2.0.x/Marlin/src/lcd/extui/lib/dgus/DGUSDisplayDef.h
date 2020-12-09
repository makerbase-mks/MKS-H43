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
#pragma once

/* DGUS implementation written by coldtobi in 2019 for Marlin */

#include "DGUSVPVariable.h"

#include <stdint.h>

// This file defines the interaction between Marlin and the display firmware.

// information on which screen which VP is displayed
// As this is a sparse table, two arrays are needed:
// one to list the VPs of one screen and one to map screens to the lists.
// (Strictly this would not be necessary, but allows to only send data the display needs and reducing load on Marlin)
struct VPMapping {
  const uint8_t screen;
  const uint16_t *VPList;  // The list is null-terminated.
};

extern const struct VPMapping VPMap[];

// List of VPs handled by Marlin / The Display.
extern const struct DGUS_VP_Variable ListOfVP[];

#if ENABLED(DGUS_LCD_UI_MKS)
  extern uint16_t distanceMove;
  extern float distanceFilament;
  extern uint16_t FilamentSpeed;
  extern float ZOffset_distance;
  extern float mesh_adj_distance;

  extern float level_1_x_point;
  extern float level_1_y_point;
  extern float level_2_x_point;
  extern float level_2_y_point;
  extern float level_3_x_point;
  extern float level_3_y_point;
  extern float level_4_x_point;
  extern float level_4_y_point;
  extern float level_5_x_point;
  extern float level_5_y_point;

  extern uint16_t tim_h;
  extern uint16_t tim_m;
  extern uint16_t tim_s;

  extern uint16_t x_park_pos;
  extern uint16_t y_park_pos;
  extern uint16_t z_park_pos;
#endif 

#include "../../../../inc/MarlinConfig.h"

#if ENABLED(DGUS_LCD_UI_ORIGIN)
  #include "origin/DGUSDisplayDef.h"
#elif ENABLED(DGUS_LCD_UI_MKS)
  #include "mks/DGUSDisplayDef.h"
#elif ENABLED(DGUS_LCD_UI_FYSETC)
  #include "fysetc/DGUSDisplayDef.h"
#elif ENABLED(DGUS_LCD_UI_HIPRECY)
  #include "hiprecy/DGUSDisplayDef.h"
#endif
