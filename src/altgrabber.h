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

#ifndef __ALT_GRABBER_H__
#define __ALT_GRABBER_H__

#include <gdk/gdk.h>

#define ALT_GRABBER_TYPE                                    (alt_grabber_get_type ())
#define ALT_GRABBER(inst)                                   (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             ALT_GRABBER_TYPE, AltGrabber))
#define IS_ALT_GRABBER(inst)                                (G_TYPE_CHECK_INSTANCE_TYPE ((inst), ALT_GRABBER_TYPE))

typedef struct _AltGrabber                                  AltGrabber;

typedef void         (* AltGrabberCallback)                             (AltGrabber         *grabber,
                                                                         gunichar            c,
                                                                         gpointer            user_data);

GType                   alt_grabber_get_type                            (void) G_GNUC_CONST;
AltGrabber *            alt_grabber_get_for_screen                      (GdkScreen          *screen);
void                    alt_grabber_clear                               (AltGrabber         *grabber);
void                    alt_grabber_add_unichar                         (AltGrabber         *grabber,
                                                                         gunichar            c,
                                                                         AltGrabberCallback  callback,
                                                                         gpointer            user_data,
                                                                         GDestroyNotify      notify);

#endif /* __ALT_GRABBER_H__ */
