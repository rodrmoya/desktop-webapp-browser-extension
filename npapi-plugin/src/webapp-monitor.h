/*
 * This file is part of the desktop-webapp-browser-extension.
 * Copyright (C) Canonical Ltd. 2012
 * Copyright (C) Collabora Ltd. 2013
 *
 * Author:
 *   Rodrigo Moya <rodrigo.moya@collabora.co.uk>
 *
 * Based on webaccounts-browser-plugin by:
 *   Alberto Mardegan <alberto.mardegan@canonical.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WEBAPP_MONITOR_H

#include "npapi-headers/headers/npapi.h"
#include "npapi-headers/headers/npruntime.h"

void      webapp_initialize_monitor (NPP instance);
void      webapp_monitor_set_icon_loader_callback (NPObject *callback);
void      webapp_destroy_monitor (void);

#endif
