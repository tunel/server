/*------------------------------------------------------------------------------
    Tune Land - Sandbox RPG
    Copyright (C) 2012-2013
        Antony Martin <antony(dot)martin(at)scengine(dot)org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 -----------------------------------------------------------------------------*/

#ifndef H_SERVER
#define H_SERVER

#include <SCE/utils/SCEUtils.h>
#include <SCE/core/SCECore.h>

#include <tunel/common/netserver.h>

#define SERVER_DEFAULT_PORT 13337
#define SERVER_MAX_TERRAIN_PATH_LENGTH 256

//typedef struct ???

typedef struct server Server;
struct server {
    int port;
    NetServer server;
    /* Chat *chat; */

    int running;
    int id_generator;
    SCE_SList clients;          /* all clients */

    char terrain_path[SERVER_MAX_TERRAIN_PATH_LENGTH];
    SCE_SFileCache fcache;
    SCE_SFileSystem fsys;
    SCE_SVoxelWorld *vw;        /* terrain */
};

int Init_Server (void);

void Server_Init (Server*);
void Server_Clear (Server*);
Server* Server_New (void);
void Server_Free (Server*);

void Server_SetTerrainPath (Server*, const char*);

int Server_Launch (Server*);
void Server_Stop (Server*);

#endif /* guard */
