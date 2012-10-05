/*
 * Copyright © 2011 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 3, as published 
 * by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranties of 
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
 * PURPOSE.  See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __ALT_MONITOR_H__
#define __ALT_MONITOR_H__

#include <gdk/gdk.h>

#define ALT_MONITOR_TYPE                                    (alt_monitor_get_type ())
#define ALT_MONITOR(inst)                                   (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             ALT_MONITOR_TYPE, AltMonitor))
#define IS_ALT_MONITOR(inst)                                (G_TYPE_CHECK_INSTANCE_TYPE ((inst), ALT_MONITOR_TYPE))

#define ALT_MONITOR_PROP_ALT_PRESSED                         "alt-pressed"

typedef struct _AltMonitor                                  AltMonitor;

GType                   alt_monitor_get_type                            (void) G_GNUC_CONST;
AltMonitor *            alt_monitor_get_for_display                     (GdkDisplay *display);
gboolean                alt_monitor_get_alt_pressed                     (AltMonitor *monitor);

#endif /* __ALT_MONITOR_H__ */
