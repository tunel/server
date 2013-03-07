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

#include <SCE/utils/SCEUtils.h>
#include <SCE/core/SCECore.h>

#include <tunel/common/netprotocol.h>
#include <tunel/common/netserver.h>
#include "perlin.h"
#include "server.h"

typedef struct serverclient ServerClient;
struct serverclient {
    char *nick;
    int id;
    NetClient *client;
    SCE_SListIterator it1;
};

static void Server_InitClient (ServerClient *sc)
{
    sc->nick = NULL;
    sc->id = -1;
    sc->client = NULL;
    SCE_List_InitIt (&sc->it1);
    SCE_List_SetData (&sc->it1, sc);
}
static void Server_ClearClient (ServerClient *sc)
{
    if (sc->client)
        NetClient_SetFreeDataFunc (sc->client, NULL);
    SCE_List_Remove (&sc->it1);
}
static ServerClient* Server_NewClient (void)
{
    ServerClient *sc = NULL;
    if (!(sc = SCE_malloc (sizeof *sc))) goto fail;
    Server_InitClient (sc);
    return sc;
fail:
    SCEE_LogSrc ();
    return NULL;
}
static void Server_FreeClient (ServerClient *sc)
{
    if (sc) {
        Server_ClearClient (sc);
        SCE_free (sc);
    }
}


#define Server_SENDFUNC(who, how, list)                                 \
    static void Server_Send##how##To##who (Server *ss, ServerClient *ex, int id, \
                                           char *data, size_t size)     \
    {                                                                   \
        SCE_SListIterator *it = NULL;                                   \
        SCE_List_ForEach (it, &ss->list) {                              \
            ServerClient *c = SCE_List_GetData (it);                    \
            if (c != ex)                                                \
                NetServer_Send##how (&ss->server, c->client, id, data, size); \
        }                                                               \
    }
#define Server_SENDSTRINGFUNC(who, how, list)                               \
    static void Server_Send##how##StringTo##who (Server *ss, ServerClient *ex, \
                                             int id, char *data)        \
    {                                                                   \
        SCE_SListIterator *it = NULL;                                   \
        SCE_List_ForEach (it, &ss->list) {                              \
            ServerClient *c = SCE_List_GetData (it);                        \
            if (c != ex)                                                \
                NetServer_Send##how##String (&ss->server, c->client, id, data); \
        }                                                               \
    }


static void
Server_StreamTCPToClients (Server *ss, ServerClient *ex, int id,
                           const char *data, size_t size, size_t total_size)
{
    SCE_SListIterator *it = NULL;
    SCE_List_ForEach (it, &ss->clients) {
        ServerClient *c = SCE_List_GetData (it);
        if (c != ex)
            NetServer_StreamTCP (&ss->server, c->client, id, data, size,
                                 total_size);
    }
}

/* Server_SendTCPToClients() */
Server_SENDFUNC (Clients,    TCP, clients)
/* Server_SendUDPToClients() */
Server_SENDFUNC (Clients,    UDP, clients)

/* Server_SendTCPStringToClients() */
Server_SENDSTRINGFUNC (Clients,    TCP, clients)
/* Server_SendUDPStringToClients() */
Server_SENDSTRINGFUNC (Clients,    UDP, clients)


static int Server_GenerateID (Server *ss)
{
    ss->id_generator++;
    return ss->id_generator;
}

static void Server_FreeClientCallback (NetClient *client, void *udata)
{
    ServerClient *sc = udata;
    Server_FreeClient (sc);
}

static ServerClient* Server_AddNewClient (Server *ss, NetClient *client)
{
    ServerClient *sc = NULL;
    if (!(sc = Server_NewClient ())) return NULL;
    sc->client = client;
    NetClient_SetData (client, sc);
    NetClient_SetFreeDataFunc (client, Server_FreeClientCallback);
    SCE_List_Appendl (&ss->clients, &sc->it1);
    sc->id = Server_GenerateID (ss);
    return sc;
}
static void Server_RemoveClient (Server *ss, ServerClient *client)
{
    SCE_List_Remove (&client->it1);
    (void)ss;
}


static ServerClient* Server_LocateClientByNick (Server *ss, const char *nick)
{
    SCE_SListIterator *it = NULL;
    SCE_List_ForEach (it, &ss->clients) {
        ServerClient *c = SCE_List_GetData (it);
        if (!strcmp (c->nick, nick))
            return c;
    }
    return NULL;
}
static ServerClient* Server_LocateClientByID (Server *ss, SockID id)
{
    SCE_SListIterator *it = NULL;
    SCE_List_ForEach (it, &ss->clients) {
        ServerClient *c = SCE_List_GetData (it);
        if (c->id == id)
            return c;
    }
    return NULL;
}

#define SERVER_MAX_CLIENTS 40        /* hihi */

#define TLP_MIN_NICK_LENGTH 3   /* TODO: move it in common/netprotocol.h */

#define Server_ANTIFAGS()                              \
    ServerClient *sc = NULL;                           \
    Server *ss = NetServer_GetData (serv);             \
    if (!(sc = NetClient_GetData (client))) {          \
        /* little fag is playing with us */            \
        NetServer_KickClient (serv, client, "-");      \
        return;                                        \
    }


static void
Server_tlp_connect (NetServer *serv, NetClient *client, void *udata,
                    const char *packet, size_t size)
{
    char data[2 * SOCKET_ID_SIZE] = {0};
    ServerClient *sc = NULL;
    char *nick = NULL;
    Server *ss = NetServer_GetData (serv);

    /* never trust user input */
    if (!(nick = SCE_String_NDup (packet, size))) {
        /* TODO ? */
        printf ("there is lol\n");
    }

    printf ("connection of %s (size %lu)\n", nick, size);

    if (SCE_List_GetLength (&ss->clients) >= SERVER_MAX_CLIENTS) {
        NetServer_SendTCPString (serv, client, TLP_CONNECT_REFUSED,
                                 "Sorry dude, server is full.");
    } else if (size < TLP_MIN_NICK_LENGTH + 1) {
        NetServer_SendTCPString (serv, client, TLP_CONNECT_REFUSED,
                                 "Your nick is too short, like your dick haha.");
    } else if (Server_LocateClientByNick (ss, nick)) {
        NetServer_SendTCPString (serv, client, TLP_CONNECT_REFUSED,
                                 "Someone already have your nick on the server.");
    } else {
        /* connection accepted */
        if (!(sc = Server_AddNewClient (ss, client))) { /* TODO ? */ }
        sc->nick = nick;
        Socket_SetID (data, sc->id);
        NetServer_SendTCP (serv, client, TLP_CONNECT_ACCEPTED, data, SOCKET_ID_SIZE);
        Server_SendTCPToClients (ss, sc, TLP_CONNECT, data, SOCKET_ID_SIZE);
    }

    if (!sc) SCE_free (nick);

    (void)udata;
}


static void
Server_tlp_disconnect (NetServer *serv, NetClient *client, void *udata,
                       const char *packet, size_t size)
{
    char data[8] = {0};
    Server_ANTIFAGS()
    Socket_SetID (data, sc->id);
    SCEE_SendMsg ("%s disconnected\n", sc->nick);
    Server_RemoveClient (ss, sc);
    Server_FreeClient (sc);
    /* dont send it if the client was only chatting */
    Server_SendTCPToClients (ss, NULL, TLP_DISCONNECT, data, 2);
    (void)udata; (void)packet; (void)size;
}

static void
Server_tlp_chat_msg (NetServer *serv, NetClient *client, void *udata,
                     const char *packet, size_t size)
{
#if 0
    size_t size;
    char data[CHAT_MSG_LENGTH + 5] = {0};
    Server_ANTIFAGS()
    Chat_HandleMsg (ss->chat, sc->chatclient, packet, size);
    Socket_SetID (data, sc->id);
    size = Chat_GetLastMessagev (ss->chat, &data[4]);
    /* this protocol sucks */
    Server_SendTCPToClients (ss, NULL, TLP_CHAT_MSG, data, size + 5);
#endif
}

static void
Server_tlp_get_client_num (NetServer *serv, NetClient *client, void *udata,
                           const char *packet, size_t size)
{
    long num;
    unsigned char data[4] = {0};
    Server *ss = NetServer_GetData (serv);
    num = SCE_List_GetLength (&ss->clients);
    Socket_SetID (data, num);
    NetServer_SendTCP (serv, client, TLP_GET_CLIENT_NUM, data, SOCKET_ID_SIZE);
}

static void
Server_tlp_get_client_list (NetServer *serv, NetClient *client, void *udata,
                            const char *packet, size_t size)
{
    size_t pos = 0;
    char data[SOCKET_ID_SIZE * SERVER_MAX_CLIENTS] = {0};
    SCE_SListIterator *it = NULL;
    Server *ss = NetServer_GetData (serv);

    SCE_List_ForEach (it, &ss->clients) {
        ServerClient *c = SCE_List_GetData (it);
        Socket_SetID (&data[pos], c->id);
        pos += SOCKET_ID_SIZE;
    }
    NetServer_SendTCP (serv, client, TLP_GET_CLIENT_LIST, data, pos);
}

static void
Server_tlp_get_client_nick (NetServer *serv, NetClient *client, void *udata,
                            const char *packet, size_t size)
{
    SockID id;
    const char *nick = ".";     /* length less than TLP_MIN_NICK_LENGTH */
    ServerClient *sc = NULL;
    Server *ss = NetServer_GetData (serv);
    if (size < 2 /* || size > 42 */) {
        /* malformed query */
        return;                 /* TODO: kick the client? */
    }
    id = Socket_GetID (packet);
    sc = Server_LocateClientByID (ss, id);
    if (sc)
        nick = sc->nick;
    NetServer_SendTCPString (serv, client, TLP_GET_CLIENT_NICK, nick);
    (void)udata;
}


/* ------------------------ terrain callbacks ------------------------ */
static void
Server_tlp_chunk_size (NetServer *serv, NetClient *client, void *udata,
                       const char *packet, size_t size)
{
    unsigned char buffer[4] = {0};
    SCEulong width;
    Server *ss = NetServer_GetData (serv);
    width = SCE_VWorld_GetWidth (ss->vw);
    SCE_Encode_Long (width, buffer);
    NetServer_SendTCP (serv, client, TLP_CHUNK_SIZE, buffer, 4);
    (void)udata; (void)packet; (void)size;
}

static void
Server_tlp_num_lod (NetServer *serv, NetClient *client, void *udata,
                    const char *packet, size_t size)
{
    unsigned char buffer[4] = {0};
    SCEulong n_levels;
    Server *ss = NetServer_GetData (serv);
    n_levels = SCE_VWorld_GetNumLevels (ss->vw);
    SCE_Encode_Long (n_levels, buffer);
    NetServer_SendTCP (serv, client, TLP_NUM_LOD, buffer, 4);
    (void)udata; (void)packet; (void)size;
}

static void Region_RegisterClient (void *a, void *b) {}
static void Region_UnregisterClient (void *a, void *b) {}

static void
Server_tlp_register_region (NetServer *serv, NetClient *client, void *udata,
                            const char *packet, size_t size)
{
    SCE_SVoxelWorldTree *wt = NULL;
    long x, y, z;
    Server *ss = NetServer_GetData (serv);

    if (size != 4 * 3) {
        SCEE_SendMsg ("corrupted register_region packet\n");
        return;
    }

    x = SCE_Decode_Long (packet);
    y = SCE_Decode_Long (&packet[4]);
    z = SCE_Decode_Long (&packet[8]);
    if ((wt = SCE_VWorld_GetTree (ss->vw, x, y, z))) {
        SCE_SVoxelOctree *vo = SCE_VWorld_GetOctree (wt);
        Region_RegisterClient (SCE_VOctree_GetData (vo),
                               NetClient_GetData (client));
    } else {
        SCEE_SendMsg ("register to a non existing region\n");
    }
}
static void
Server_tlp_unregister_region (NetServer *serv, NetClient *client, void *udata,
                              const char *packet, size_t size)
{
    SCE_SVoxelOctree *vo = NULL;
    SCE_SVoxelWorldTree *wt = NULL;
    long x, y, z;
    Server *ss = NetServer_GetData (serv);

    if (size != 4 * 3) {
        SCEE_SendMsg ("corrupted unregister_region packet\n");
        return;
    }

    x = SCE_Decode_Long (packet);
    y = SCE_Decode_Long (&packet[4]);
    z = SCE_Decode_Long (&packet[8]);
    if ((wt = SCE_VWorld_GetTree (ss->vw, x, y, z))) {
        vo = SCE_VWorld_GetOctree (wt);
        Region_UnregisterClient (SCE_VOctree_GetData (vo),
                                 NetClient_GetData (client));
    } else {
        SCEE_SendMsg ("unregister to a non existing region\n");
    }
    (void)udata;
}

static void
Server_tlp_query_octree (NetServer *serv, NetClient *client, void *udata,
                         const char *packet, size_t size)
{
    long x, y, z;
    SCE_SFileSystem fs;
    SCE_SFile fp;
    size_t s;
    SCE_SVoxelWorldTree *wt = NULL;
    SCE_SVoxelOctree *vo = NULL;
    Server *ss = NetServer_GetData (serv);

    /* TODO: sha1? we'll see when the trees get really big. */

    if (size != 4 * 3) {
        SCEE_SendMsg ("corrupted query_octree packet: %ld\n", size);
        return;
    }

    x = SCE_Decode_Long (packet);
    y = SCE_Decode_Long (&packet[4]);
    z = SCE_Decode_Long (&packet[8]);
    if (!(wt = SCE_VWorld_GetTree (ss->vw, x, y, z))) {
        /* just tell the client this tree is non-existent */
        NetServer_SendTCP (serv, client, TLP_NO_OCTREE, packet, 12);
        return;
    }

    vo = SCE_VWorld_GetOctree (wt);

    /* full in-memory file */
    fs = sce_cachefs;
    fs.subfs = &sce_nullfs;

    SCE_File_Init (&fp);
    if (SCE_File_Open (&fp, &fs, "foo", SCE_FILE_WRITE) < 0) {
        /* wtf */
        SCEE_LogSrc ();
        SCEE_Out ();
        SCEE_Clear ();
        return;
    }

    SCE_VOctree_SaveFile (vo, &fp);
    s = SCE_File_Length (&fp);
    /* bruteforce the whole octree :> */
    NetServer_StreamTCP (serv, client, TLP_QUERY_OCTREE, packet, 12,
                         s + 12);
    NetServer_StreamTCP (serv, client, NETWORK_CONTINUE_STREAM,
                         SCE_FileCache_GetRaw (&fp), s, 0);
    SCE_File_Close (&fp);

    (void)udata;
}

static void
Server_tlp_query_chunk (NetServer *serv, NetClient *client, void *udata,
                        const char *packet, size_t size)
{
    unsigned char buffer[32];
    SCE_SVoxelOctreeNode *node = NULL;
    SCE_SVoxelOctree *vo = NULL;
    SCEuint level;
    long x, y, z;
    SCE_EVoxelOctreeStatus status;
    size_t comp_size = 0;
    void *comp_data = NULL;
    Server *ss = NetServer_GetData (serv);

#define PACKET_SIZE 16
#define PACKET_SIZE_CHKSUM (PACKET_SIZE + SCE_SHA1_SIZE)

    if (size != PACKET_SIZE && size != PACKET_SIZE_CHKSUM) {
        SCEE_SendMsg ("corrupted query_chunk packet\n");
        return;
    }

    level = SCE_Decode_Long (packet);
    x = SCE_Decode_Long (&packet[4]);
    y = SCE_Decode_Long (&packet[8]);
    z = SCE_Decode_Long (&packet[12]);

    /* TODO: query should be queued here */

    if (!(node = SCE_VWorld_FetchNode (ss->vw, level, x, y, z))) {
        /* just tell the client this chunk is non-existent */
        NetServer_SendTCP (serv, client, TLP_NO_CHUNK, packet, PACKET_SIZE);
        return;
    }
    vo = SCE_VWorld_GetOctree (SCE_VWorld_FetchTree (ss->vw, level, x, y, z));
    /* NOTE: vo should not be NULL if node isnt NULL right? */

    memcpy (buffer, packet, PACKET_SIZE);

    /* status shouldn't be either FULL or EMPTY... but it probably will
       once materials are gonna be used :> */
    status = SCE_VOctree_GetNodeStatus (node);
    if (status == SCE_VOCTREE_NODE_EMPTY || status == SCE_VOCTREE_NODE_FULL) {
        SCEE_SendMsg ("user is a jerk.\n");
        return;
    }

    /* you might think that we only need to cache/sync the node if we need
       to send the data, but we actually need them for sha1 computation */
    if (SCE_VOctree_CacheNode (vo, node) < 0 ||
        SCE_VOctree_SyncNode (vo, node) < 0) {
        SCEE_LogSrc ();
        return;
    }
    comp_size = SCE_VOctree_GetNodeCompressedSize (node);
    comp_data = SCE_VOctree_GetNodeCompressedData (node);

    if (size == PACKET_SIZE) {
        NetServer_StreamTCP (serv, client, TLP_QUERY_CHUNK, buffer,
                             PACKET_SIZE, PACKET_SIZE + comp_size);
        NetServer_StreamTCP (serv, client, NETWORK_CONTINUE_STREAM,
                             comp_data, comp_size, 0);
    } else {
        SCE_TSha1 sum;
        SCE_Sha1_Init (sum);
        SCE_Sha1_Sum (sum, comp_data, comp_size);
        if (!SCE_Sha1_Equal (sum, &packet[PACKET_SIZE])) {
            NetServer_StreamTCP (serv, client, TLP_QUERY_CHUNK, buffer,
                                 PACKET_SIZE, PACKET_SIZE + comp_size);
            NetServer_StreamTCP (serv, client, NETWORK_CONTINUE_STREAM,
                                 comp_data, comp_size, 0);
        } else {
            SCEE_SendMsg ("checksums match!\n");
            /* no data means "ok you're good." */
            NetServer_SendTCP (serv, client, TLP_QUERY_CHUNK, buffer,
                               PACKET_SIZE);
        }
    }

    (void)udata;
}


static NetServerCmd ss_tcpcmds[TLP_NUM_COMMANDS];
static size_t ss_numtcp = 0;
static NetServerCmd ss_udpcmds[TLP_NUM_COMMANDS]; /* num commands : d */
static size_t ss_numudp = 0;

static void Server_InitAllCommands (void)
{
    size_t i = 0;

    /* TCP commands */
#define Server_SETTCPCMD(id, fun) do {                 \
        NetServer_InitCmd (&ss_tcpcmds[i]);               \
        NetServer_SetCmdID (&ss_tcpcmds[i], id);          \
        NetServer_SetCmdCallback (&ss_tcpcmds[i], fun);   \
        i++;                                           \
    } while (0)
    Server_SETTCPCMD (TLP_CONNECT, Server_tlp_connect);
    Server_SETTCPCMD (TLP_DISCONNECT, Server_tlp_disconnect);
    Server_SETTCPCMD (TLP_CHUNK_SIZE, Server_tlp_chunk_size);
    Server_SETTCPCMD (TLP_NUM_LOD, Server_tlp_num_lod);
    Server_SETTCPCMD (TLP_REGISTER_REGION, Server_tlp_register_region);
    Server_SETTCPCMD (TLP_UNREGISTER_REGION, Server_tlp_unregister_region);
    Server_SETTCPCMD (TLP_QUERY_OCTREE, Server_tlp_query_octree);
    Server_SETTCPCMD (TLP_QUERY_CHUNK, Server_tlp_query_chunk);
#if 0
    Server_SETTCPCMD (TLP_GET_CLIENT_NUM, Server_tlp_get_client_num);
    Server_SETTCPCMD (TLP_GET_CLIENT_LIST, Server_tlp_get_client_list);
    Server_SETTCPCMD (TLP_GET_CLIENT_NICK, Server_tlp_get_client_nick);
    Server_SETTCPCMD (TLP_CHAT_MSG, Server_tlp_chat_msg);
#endif
#undef Server_SETTCPCMD
    ss_numtcp = i;

    /* UDP commands */
    i = 0;
#if 0
#define Server_SETUDPCMD(id, fun) do {                 \
        Server_InitCmd (&ss_udpcmds[i]);               \
        Server_SetCmdID (&ss_udpcmds[i], id);          \
        Server_SetCmdCallback (&ss_udpcmds[i], fun);   \
        i++;                                           \
    } while (0)
    Server_SETUDPCMD (TLP_PLAYER_KEYSTATE, Server_tlp_player_keystate);
#endif
    ss_numudp = i;
}

int Init_Server (void)
{
    Server_InitAllCommands ();
    return SCE_OK;
}

static void Server_AssignCallbacks (NetServer *serv)
{
    size_t i;
    for (i = 0; i < ss_numtcp; i++)
        NetServer_AddTCPCmd (serv, &ss_tcpcmds[i]);
    for (i = 0; i < ss_numudp; i++)
        NetServer_AddUDPCmd (serv, &ss_udpcmds[i]);
}

void Server_Init (Server *ss)
{
    ss->port = SERVER_DEFAULT_PORT;
    NetServer_Init (&ss->server);
    NetServer_SetData (&ss->server, ss);
    Server_AssignCallbacks (&ss->server);

    ss->running = SCE_FALSE;
    ss->id_generator = 0;
    SCE_List_Init (&ss->clients);

    memset (ss->terrain_path, 0, sizeof ss->terrain_path);
    SCE_FileCache_InitCache (&ss->fcache);
    ss->vw = NULL;
}
void Server_Clear (Server *ss)
{
    SCE_List_Clear (&ss->clients);
    SCE_FileCache_ClearCache (&ss->fcache);
    SCE_VWorld_Delete (ss->vw);
}
Server* Server_New (void)
{
    Server *ss = NULL;
    if (!(ss = SCE_malloc (sizeof *ss)))
        SCEE_LogSrc ();
    else {
        Server_Init (ss);
    }
    return ss;
}
void Server_Free (Server *ss)
{
    if (ss) {
        Server_Clear (ss);
        SCE_free (ss);
    }
}

void Server_SetTerrainPath (Server *ss, const char *path)
{
    strncpy (ss->terrain_path, path, sizeof ss->terrain_path - 1);
}

#define OCTREE_SIZE 32
#define N_LOD 4

#define VWORLD_PREFIX "terrain"
#define VWORLD_FNAME "vworld.bin"

static float myrand (float min, float max)
{
    float r = rand ();
    r /= RAND_MAX;
    r *= (max - min);
    r += min;
    return r;
}

unsigned char density_function (long x, long y, long z)
{
    double p[3], p2[3];
//    float ground = 50.0 + 100.0 * (10.0/(x + 10) + 10.0/(y + 10));
#if 0
    double derp = (-z + 32) * 0.28;
#else
    /* TODO: first seed only work with these parameters */
//    double derp = (-z + 40) * 0.07;
    double derp = (-z + 100) * 0.07;
#endif
    double val;

    p[0] = x; p[1] = y; p[2] = z;

    SCE_Vector3_Operator1v (p2, = 0.012 *, p);
    p2[0] = noise3 (p2) * 10.0;
    p2[1] = p2[0];
    p2[2] = p2[0];


    SCE_Vector3_Operator1v (p, += 8.0 *, p2);
    SCE_Vector3_Operator1 (p, *=, 0.002);
    derp += (noise3 (p) + 0.1) * 38.0;

    SCE_Vector3_Operator1 (p, *=, 4.0);
    derp += noise3 (p) * 8.0;

    SCE_Vector3_Operator1 (p, *=, 2.0);
    derp += noise3 (p) * 5.0;
    SCE_Vector3_Operator1 (p, *=, 3.5);
    derp += noise3 (p) * 0.9;
    SCE_Vector3_Operator1 (p, *=, 2.6);
    derp += noise3 (p) * 0.27;

    derp = SCE_Math_Clampf (derp, 0.0, 1.0);
    return derp * 255;
}

static void generate_buffer (const SCE_SLongRect3 *r, SCEulong w, SCEulong h,
                             SCEubyte *buffer)
{
    long p1[3], p2[3];
    long x, y, z, i, j, k;

    SCE_Rectangle3_GetPointslv (r, p1, p2);

    for (z = p1[2], k = 0; z < p2[2]; z++, k++) {
        for (y = p1[1], j = 0; y < p2[1]; y++, j++) {
            for (x = p1[0], i = 0; x < p2[0]; x++, i++)
                buffer[w * (h * k + j) + i] = density_function (x, y, z);
        }
    }
}

static int generate_terrain (SCE_SVoxelWorld *vw)
{
    long x, y, z;
    SCEulong w, h, d, num;
    SCEulong height;
    SCEuint levels;
    SCE_SLongRect3 r;
    SCEubyte *buffer = NULL;

    w = SCE_VWorld_GetWidth (vw);
    h = SCE_VWorld_GetHeight (vw);
    d = SCE_VWorld_GetDepth (vw);
    levels = SCE_VWorld_GetNumLevels (vw);
    num = 1 << (levels - 1);

    if (!(buffer = SCE_malloc (w * h * d)))
        goto fail;

    height = MIN (num, 1 + 300 / OCTREE_SIZE);
    //height = num;

    for (z = 0; z < height; z++) {
        for (y = 0; y < num; y++) {
            printf ("%.2f %%\n", 100.0 * (z * num + y) / (num * height));
            for (x = 0; x < num; x++) {
                SCE_Rectangle3_SetFromOriginl (&r, x * w, y * h, z * d, w, h, d);
                generate_buffer (&r, w, h, buffer);
                if (SCE_VWorld_SetRegion (vw, &r, buffer) < 0)
                    goto fail;
                SCE_VWorld_GetNextUpdatedRegion (vw, &r);
            }
            /* TODO: direct access to vw member */
            SCE_FileCache_Update (vw->fcache);
            SCE_VWorld_UpdateCache (vw);
        }
    }

    SCE_free (buffer);
    return SCE_OK;
fail:
    SCE_free (buffer);
    SCEE_LogSrc ();
    return SCE_ERROR;
}

static int Server_InitTerrain (Server *ss)
{
    SCE_SFileCache *fcache = NULL;
    SCE_SFileSystem *fsys = NULL;
    SCE_SVoxelWorld *vw = NULL;

    fcache = &ss->fcache;
    fsys = &ss->fsys;

    /* create voxel world */
    ss->vw = vw = SCE_VWorld_Create ();
    if (!vw) goto fail;

    SCE_VWorld_SetPrefix (vw, ss->terrain_path);
    *fsys = sce_cachefs;
    fsys->udata = fcache;
    SCE_VWorld_SetFileSystem (vw, fsys);
    SCE_FileCache_SetMaxCachedFiles (fcache, 256);
    SCE_VWorld_SetFileCache (vw, fcache);
    SCE_VWorld_SetMaxCachedNodes (vw, 256);

    if (!(SCE_VWorld_Load (vw, VWORLD_FNAME) < 0)) {
        if (SCE_VWorld_LoadAllTrees (vw) < 0)
            goto fail;
        if (SCE_VWorld_Build (vw) < 0)
            goto fail;
    } else {
        SCEE_Clear ();
        SCE_VWorld_SetDimensions (vw, OCTREE_SIZE, OCTREE_SIZE, OCTREE_SIZE);
        SCE_VWorld_SetNumLevels (vw, N_LOD);
        if (SCE_VWorld_Build (vw) < 0)
            goto fail;

        SCE_VWorld_AddNewTree (vw, 0, 0, 0);

        printf ("generating terrain...\n");
        if (generate_terrain (vw) < 0)
            goto fail;
        printf ("terrain generated.\ngenerating LOD...\n");

        {
            SCE_SLongRect3 rect;
            SCEuint levels = SCE_VWorld_GetNumLevels (vw);
            long s = OCTREE_SIZE  << (levels - 1);
            SCE_Rectangle3_Setl (&rect, 0, 0, 0, s, s, s);
            SCE_VWorld_GenerateAllLOD (vw, 0, &rect);
        }
        printf ("LOD generated.\n");
    }

    return SCE_OK;
fail:
    SCEE_LogSrc ();
    return SCE_ERROR;
}

static int Server_UpdateTerrain (Server *ss)
{
    if (SCE_VWorld_UpdateCache (ss->vw) < 0) {
        SCEE_LogSrc ();
        return SCE_ERROR;
    }
    SCE_FileCache_Update (&ss->fcache);
    return SCE_OK;
}

int Server_Launch (Server *ss)
{
    float secs = 0.1f;
    unsigned int prev = 0, cur = 0;
    int r;

    strcpy (ss->terrain_path, VWORLD_PREFIX);

    /* load terrain */
    if (Server_InitTerrain (ss) < 0)
        goto fail;

    if (NetServer_Open (&ss->server, ss->port) < 0)
        goto fail;
    ss->running = SCE_TRUE;

    /*prev = SDL_GetTicks ();*/
    while (ss->running) {
        /*cur = SDL_GetTicks ();*/
        secs = (float)(cur - prev) / 1000.0f;
        prev = cur;

        while ((r = NetServer_Poll (&ss->server))) {
            if (r < 0) {
                SCEE_LogSrc ();
                break;
            }
            /* TODO: warning: we can be stuck in this loop */
            NetServer_Step (&ss->server);
        }

        Server_UpdateTerrain (ss);

#if 0
        if (ss->game->playing) {
            Servererver_SendRelevantPlayers (ss, secs);
            Servererver_SendRelevantObjects (ss);
        }
#endif

        /*SDL_Delay (10);*/
        if (SCEE_HaveError ()) {
            SCEE_Out ();
            SCEE_Clear ();
        }
    }

    if (SCE_VWorld_Save (ss->vw, VWORLD_FNAME) < 0) {
        SCEE_LogSrc ();
        SCEE_Out ();
    }
    if (SCE_VWorld_SaveAllTrees (ss->vw) < 0) {
        SCEE_LogSrc ();
        SCEE_Out ();
    }

    SCE_VWorld_SyncCache (ss->vw);

    NetServer_Close (&ss->server);

    return SCE_OK;
fail:
    SCEE_LogSrc ();
    return SCE_ERROR;
}

void Server_Stop (Server *ss)
{
    ss->running = SCE_FALSE;
}
