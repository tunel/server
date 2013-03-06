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

#include <errno.h>
#include <SCE/core/SCECore.h>
#include "server.h"

#define PORT 13338

static Server *server = NULL;
static pthread_t server_thread;


static void* ServerLoop (void *unused)
{
    (void)unused;
    SCEE_SendMsg ("launching server.\n");
    if (Server_Launch (server) < 0) {
        SCEE_SendMsg ("Server_Launch() failed\n");
        SCEE_LogSrc ();
        SCEE_Out ();
    }
    return NULL;
}

static int LaunchServer (void)
{
    if (!(server = Server_New ()))
        goto fail;
    Server_SetTerrainPath (server, "terrain");
    server->port = PORT;
    pthread_create (&server_thread, NULL, ServerLoop, NULL);
    return SCE_OK;
fail:
    return SCE_ERROR;
}

static void StopServer (void)
{
    Server_Stop (server);
    pthread_join (server_thread, NULL);
}

int main (int argc, char **argv)
{
    int c;

    SCE_Init_Core (stderr, 0);
    Init_Server ();

    if (LaunchServer () < 0)
        goto fail;
    do {
        errno = 0;
        c = fgetc (stdin);
    } while (errno == EINTR || (c >= 0 && c != EOF));

    if (SCEE_HaveError ()) goto fail;

    SCEE_SendMsg ("stopping server...\n");
    StopServer ();
    Server_Free (server);

    SCEE_SendMsg ("exiting program code 0\n");
    return 0;
fail:
    SCEE_LogSrc ();
    SCEE_Out ();
    SCEE_Clear ();
    StopServer ();
    Server_Free (server);
    SCEE_SendMsg ("exiting program code %d\n", EXIT_FAILURE);
    return EXIT_FAILURE;
}
