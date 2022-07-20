/*
 * This file Copyright © Transmission authors and contributors.
 *
 * It may be used under the 3-Clause BSD License, the GNU Public License v2,
 * or v3, or any future license endorsed by Mnemosyne LLC.
 *
 */

/*
 * This file defines the public API for the libtransmission library.
 * The other public API headers are variant.h and utils.h.
 * Most of the remaining headers in libtransmission are private.
 */

#pragma once

/***
****
****  Basic Types
****
***/

#include <stdbool.h> /* bool */
#include <stddef.h> /* size_t */
#include <stdint.h> /* uintN_t */
#include <time.h> /* time_t */

#include "tr-macros.h"

using tr_file_index_t = uint32_t;
using tr_piece_index_t = uint32_t;
/* Assuming a 16 KiB block (tr_block_info::BlockSize), a 32-bit block index gives us a maximum torrent size of 64 TiB.
 * When we ever need to grow past that, change tr_block_index_t and  tr_piece_index_t to uint64_t. */
using tr_block_index_t = uint32_t;
using tr_tracker_tier_t = uint32_t;
using tr_tracker_id_t = uint32_t;
using tr_byte_index_t = uint64_t;
using tr_torrent_id_t = int;

struct tr_block_span_t
{
    tr_block_index_t begin;
    tr_block_index_t end;
};

struct tr_byte_span_t
{
    uint64_t begin;
    uint64_t end;
};

class tr_announce_list;

struct tr_ctor;
struct tr_error;
struct tr_session;
struct tr_torrent;
struct tr_torrent_metainfo;
struct tr_variant;

using tr_priority_t = int8_t;

#define TR_RPC_SESSION_ID_HEADER "X-Transmission-Session-Id"

enum tr_preallocation_mode
{
    TR_PREALLOCATE_NONE = 0,
    TR_PREALLOCATE_SPARSE = 1,
    TR_PREALLOCATE_FULL = 2
};

enum tr_encryption_mode
{
    TR_CLEAR_PREFERRED,
    TR_ENCRYPTION_PREFERRED,
    TR_ENCRYPTION_REQUIRED
};

#define TR_RATIO_NA -1
#define TR_RATIO_INF -2

/***
****
****  Startup & Shutdown
****
***/

/**
 * @addtogroup tr_session Session
 *
 * A libtransmission session is created by calling tr_sessionInit().
 * libtransmission creates a thread for itself so that it can operate
 * independently of the caller's event loop. The session will continue
 * until tr_sessionClose() is called.
 *
 * @{
 */

/**
 * @brief returns Transmission's default configuration file directory.
 *
 * The default configuration directory is determined this way:
 * -# If the TRANSMISSION_HOME environment variable is set, its value is used.
 * -# On Darwin, "${HOME}/Library/Application Support/${appname}" is used.
 * -# On Windows, "${CSIDL_APPDATA}/${appname}" is used.
 * -# If XDG_CONFIG_HOME is set, "${XDG_CONFIG_HOME}/${appname}" is used.
 * -# ${HOME}/.config/${appname}" is used as a last resort.
 */
char const* tr_getDefaultConfigDir(char const* appname);

/**
 * @brief returns Transmisson's default download directory.
 *
 * The default download directory is determined this way:
 * -# If the HOME environment variable is set, "${HOME}/Downloads" is used.
 * -# On Windows, "${CSIDL_MYDOCUMENTS}/Downloads" is used.
 * -# Otherwise, getpwuid(getuid())->pw_dir + "/Downloads" is used.
 */
char const* tr_getDefaultDownloadDir(void);

#define TR_DEFAULT_BIND_ADDRESS_IPV4 "0.0.0.0"
#define TR_DEFAULT_BIND_ADDRESS_IPV6 "::"
#define TR_DEFAULT_RPC_WHITELIST "127.0.0.1,::1"
#define TR_DEFAULT_RPC_HOST_WHITELIST ""
#define TR_DEFAULT_RPC_PORT_STR "9091"
#define TR_DEFAULT_RPC_PORT 9091
#define TR_DEFAULT_RPC_URL_STR "/transmission/"
#define TR_DEFAULT_PEER_PORT_STR "51413"
#define TR_DEFAULT_PEER_SOCKET_TOS_STR "le"
#define TR_DEFAULT_PEER_LIMIT_GLOBAL_STR "200"
#define TR_DEFAULT_PEER_LIMIT_TORRENT_STR "50"
#define TR_DEFAULT_PEER_LIMIT_TORRENT 50

/**
 * Add libtransmission's default settings to the benc dictionary.
 *
 * Example:
 * @code
 *     tr_variant settings;
 *     int64_t i;
 *
 *     tr_variantInitDict(&settings, 0);
 *     tr_sessionGetDefaultSettings(&settings);
 *     if (tr_variantDictFindInt(&settings, TR_PREFS_KEY_PEER_PORT, &i))
 *         fprintf(stderr, "the default peer port is %d\n", (int)i);
 *     tr_variantFree(&settings);
 * @endcode
 *
 * @param setme_dictionary pointer to a tr_variant dictionary
 * @see tr_sessionLoadSettings()
 * @see tr_sessionInit()
 * @see tr_getDefaultConfigDir()
 */
void tr_sessionGetDefaultSettings(struct tr_variant* setme_dictionary);

/**
 * Add the session's current configuration settings to the benc dictionary.
 *
 * TODO: if we ever make libtransmissionapp, this would go there.
 *
 * @param session          the session to query
 * @param setme_dictionary the dictionary to populate
 * @see tr_sessionGetDefaultSettings()
 */
void tr_sessionGetSettings(tr_session const* session, struct tr_variant* setme_dictionary);

/**
 * Load settings from the configuration directory's settings.json file,
 * using libtransmission's default settings as fallbacks for missing keys.
 *
 * TODO: if we ever make libtransmissionapp, this would go there.
 *
 * @param dictionary pointer to an uninitialized tr_variant
 * @param configDir the configuration directory to find settings.json
 * @param appName if configDir is empty, appName is used to find the default dir.
 * @return success true if the settings were loaded, false otherwise
 * @see tr_sessionGetDefaultSettings()
 * @see tr_sessionInit()
 * @see tr_sessionSaveSettings()
 */
bool tr_sessionLoadSettings(struct tr_variant* dictionary, char const* configDir, char const* appName);

/**
 * Add the session's configuration settings to the benc dictionary
 * and save it to the configuration directory's settings.json file.
 *
 * TODO: if we ever make libtransmissionapp, this would go there.
 *
 * @param session    the session to save
 * @param configDir  the directory to write to
 * @param dictionary the dictionary to save
 * @see tr_sessionLoadSettings()
 */
void tr_sessionSaveSettings(tr_session* session, char const* configDir, struct tr_variant const* dictionary);

/**
 * @brief Initialize a libtransmission session.
 *
 * For example, this will instantiate a session with all the default values:
 * @code
 *     tr_variant settings;
 *     tr_session* session;
 *     char const* configDir;
 *
 *     tr_variantInitDict(&settings, 0);
 *     tr_sessionGetDefaultSettings(&settings);
 *     configDir = tr_getDefaultConfigDir("Transmission");
 *     session = tr_sessionInit(configDir, true, &settings);
 *
 *     tr_variantFree(&settings);
 * @endcode
 *
 * @param configDir where Transmission will look for resume files, blocklists, etc.
 * @param messageQueueingEnabled if false, messages will be dumped to stderr
 * @param settings libtransmission settings
 * @see tr_sessionGetDefaultSettings()
 * @see tr_sessionLoadSettings()
 * @see tr_getDefaultConfigDir()
 */
tr_session* tr_sessionInit(char const* configDir, bool messageQueueingEnabled, struct tr_variant* settings);

/** @brief Update a session's settings from a benc dictionary
           like to the one used in tr_sessionInit() */
void tr_sessionSet(tr_session* session, struct tr_variant* settings);

/** @brief Rescan the blocklists directory and
           reload whatever blocklist files are found there */
void tr_sessionReloadBlocklists(tr_session* session);

/** @brief End a libtransmission session
    @see tr_sessionInit() */
void tr_sessionClose(tr_session*);

/**
 * @brief Return the session's configuration directory.
 *
 * This is where transmission stores its torrent files, .resume files,
 * blocklists, etc. It's set in tr_transmissionInit() and is immutable
 * during the session.
 */
char const* tr_sessionGetConfigDir(tr_session const*);

/**
 * @brief Set the per-session default download folder for new torrents.
 * @see tr_sessionInit()
 * @see tr_sessionGetDownloadDir()
 * @see tr_ctorSetDownloadDir()
 */
void tr_sessionSetDownloadDir(tr_session* session, char const* downloadDir);

/**
 * @brief Get the default download folder for new torrents.
 *
 * This is set by tr_sessionInit() or tr_sessionSetDownloadDir(),
 * and can be overridden on a per-torrent basis by tr_ctorSetDownloadDir().
 */
char const* tr_sessionGetDownloadDir(tr_session const* session);

/**
 * @brief Set the torrent's bandwidth priority.
 */
void tr_ctorSetBandwidthPriority(tr_ctor* ctor, tr_priority_t priority);

/**
 * @brief Get the torrent's bandwidth priority.
 */
tr_priority_t tr_ctorGetBandwidthPriority(tr_ctor const* ctor);

/**
 * @brief set the per-session incomplete download folder.
 *
 * When you add a new torrent and the session's incomplete directory is enabled,
 * the new torrent will start downloading into that directory, and then be moved
 * to tr_torrent.downloadDir when the torrent is finished downloading.
 *
 * Torrents aren't moved as a result of changing the session's incomplete dir --
 * it's applied to new torrents, not existing ones.
 *
 * tr_torrentSetLocation() overrules the incomplete dir: when a user specifies
 * a new location, that becomes the torrent's new downloadDir and the torrent
 * is moved there immediately regardless of whether or not it's complete.
 *
 * @see tr_sessionInit()
 * @see tr_sessionGetIncompleteDir()
 * @see tr_sessionSetIncompleteDirEnabled()
 * @see tr_sessionGetIncompleteDirEnabled()
 */
void tr_sessionSetIncompleteDir(tr_session* session, char const* dir);

/** @brief get the per-session incomplete download folder */
char const* tr_sessionGetIncompleteDir(tr_session const* session);

/** @brief enable or disable use of the incomplete download folder */
void tr_sessionSetIncompleteDirEnabled(tr_session* session, bool);

/** @brief get whether or not the incomplete download folder is enabled */
bool tr_sessionIsIncompleteDirEnabled(tr_session const* session);

/**
 * @brief When enabled, newly-created files will have ".part" appended
 *        to their filename until the file is fully downloaded
 *
 * This is not retroactive -- toggling this will not rename existing files.
 * It only applies to new files created by Transmission after this API call.
 *
 * @see tr_sessionIsIncompleteFileNamingEnabled()
 */
void tr_sessionSetIncompleteFileNamingEnabled(tr_session* session, bool);

/** @brief return true if files will end in ".part" until they're complete */
bool tr_sessionIsIncompleteFileNamingEnabled(tr_session const* session);

/**
 * @brief Set whether or not RPC calls are allowed in this session.
 *
 * @details If true, libtransmission will open a server socket to listen
 * for incoming http RPC requests as described in docs/rpc-spec.md.
 *
 * This is intially set by tr_sessionInit() and can be
 * queried by tr_sessionIsRPCEnabled().
 */
void tr_sessionSetRPCEnabled(tr_session* session, bool isEnabled);

/** @brief Get whether or not RPC calls are allowed in this session.
    @see tr_sessionInit()
    @see tr_sessionSetRPCEnabled() */
bool tr_sessionIsRPCEnabled(tr_session const* session);

/** @brief Specify which port to listen for RPC requests on.
    @see tr_sessionInit()
    @see tr_sessionGetRPCPort */
void tr_sessionSetRPCPort(tr_session* session, uint16_t port);

/** @brief Get which port to listen for RPC requests on.
    @see tr_sessionInit()
    @see tr_sessionSetRPCPort */
uint16_t tr_sessionGetRPCPort(tr_session const* session);

/**
 * @brief Specify which base URL to use.
 *
 * @param session the session to set
 * @param url     the base url to use. The RPC API is accessible under <url>/rpc, the web interface under * <url>/web.
 *
 *  @see tr_sessionGetRPCUrl
 */
void tr_sessionSetRPCUrl(tr_session* session, char const* url);

/**
 * @brief Get the base URL.
 * @see tr_sessionInit()
 * @see tr_sessionSetRPCUrl
 */
char const* tr_sessionGetRPCUrl(tr_session const* session);

/**
 * @brief Specify a whitelist for remote RPC access
 *
 * The whitelist is a comma-separated list of dotted-quad IP addresses
 * to be allowed. Wildmat notation is supported, meaning that
 * '?' is interpreted as a single-character wildcard and
 * '*' is interprted as a multi-character wildcard.
 */
void tr_sessionSetRPCWhitelist(tr_session* session, char const* whitelist);

/** @brief get the Access Control List for allowing/denying RPC requests.
    @return a comma-separated string of whitelist domains.
    @see tr_sessionInit
    @see tr_sessionSetRPCWhitelist */
char const* tr_sessionGetRPCWhitelist(tr_session const*);

void tr_sessionSetRPCWhitelistEnabled(tr_session* session, bool isEnabled);

bool tr_sessionGetRPCWhitelistEnabled(tr_session const* session);

void tr_sessionSetRPCPassword(tr_session* session, char const* password);

void tr_sessionSetRPCUsername(tr_session* session, char const* username);

// TODO(ckerr): rename function to indicate it returns the salted value
/** @brief get the salted version of the password used to restrict RPC requests.
    @return the password string.
    @see tr_sessionInit()
    @see tr_sessionSetRPCPassword() */
char const* tr_sessionGetRPCPassword(tr_session const* session);

char const* tr_sessionGetRPCUsername(tr_session const* session);

void tr_sessionSetRPCPasswordEnabled(tr_session* session, bool isEnabled);

bool tr_sessionIsRPCPasswordEnabled(tr_session const* session);

void tr_sessionSetDefaultTrackers(tr_session* session, char const* trackers);

enum tr_rpc_callback_type
{
    TR_RPC_TORRENT_ADDED,
    TR_RPC_TORRENT_STARTED,
    TR_RPC_TORRENT_STOPPED,
    TR_RPC_TORRENT_REMOVING,
    TR_RPC_TORRENT_TRASHING, /* _REMOVING + delete local data */
    TR_RPC_TORRENT_CHANGED, /* catch-all for the "torrent-set" rpc method */
    TR_RPC_TORRENT_MOVED,
    TR_RPC_SESSION_CHANGED,
    TR_RPC_SESSION_QUEUE_POSITIONS_CHANGED, /* catch potentially multiple torrents being moved in the queue */
    TR_RPC_SESSION_CLOSE
};

enum tr_rpc_callback_status
{
    /* no special handling is needed by the caller */
    TR_RPC_OK = 0,
    /* indicates to the caller that the client will take care of
     * removing the torrent itself. For example the client may
     * need to keep the torrent alive long enough to cleanly close
     * some resources in another thread. */
    TR_RPC_NOREMOVE = (1 << 1)
};

using tr_rpc_func = tr_rpc_callback_status (*)( //
    tr_session* session,
    tr_rpc_callback_type type,
    struct tr_torrent* tor_or_null,
    void* user_data);

/**
 * Register to be notified whenever something is changed via RPC,
 * such as a torrent being added, removed, started, stopped, etc.
 *
 * func is invoked FROM LIBTRANSMISSION'S THREAD!
 * This means func must be fast (to avoid blocking peers),
 * shouldn't call libtransmission functions (to avoid deadlock),
 * and shouldn't modify client-level memory without using a mutex!
 */
void tr_sessionSetRPCCallback(tr_session* session, tr_rpc_func func, void* user_data);

/**
***
**/

/** @brief Used by tr_sessionGetStats() and tr_sessionGetCumulativeStats() */
struct tr_session_stats
{
    float ratio; /* TR_RATIO_INF, TR_RATIO_NA, or total up/down */
    uint64_t uploadedBytes; /* total up */
    uint64_t downloadedBytes; /* total down */
    uint64_t filesAdded; /* number of files added */
    uint64_t sessionCount; /* program started N times */
    uint64_t secondsActive; /* how long Transmisson's been running */
};

/** @brief Get bandwidth use statistics for the current session */
void tr_sessionGetStats(tr_session const* session, tr_session_stats* setme);

/** @brief Get cumulative bandwidth statistics for current and past sessions */
void tr_sessionGetCumulativeStats(tr_session const* session, tr_session_stats* setme);

void tr_sessionClearStats(tr_session* session);

/**
 * @brief Set whether or not torrents are allowed to do peer exchanges.
 *
 * PEX is always disabled in private torrents regardless of this.
 * In public torrents, PEX is enabled by default.
 */
void tr_sessionSetPexEnabled(tr_session* session, bool isEnabled);
bool tr_sessionIsPexEnabled(tr_session const* session);

bool tr_sessionIsDHTEnabled(tr_session const* session);
void tr_sessionSetDHTEnabled(tr_session* session, bool);

bool tr_sessionIsUTPEnabled(tr_session const* session);
void tr_sessionSetUTPEnabled(tr_session* session, bool);

bool tr_sessionIsLPDEnabled(tr_session const* session);
void tr_sessionSetLPDEnabled(tr_session* session, bool enabled);

void tr_sessionSetCacheLimit_MB(tr_session* session, int mb);
int tr_sessionGetCacheLimit_MB(tr_session const* session);

tr_encryption_mode tr_sessionGetEncryption(tr_session const* session);
void tr_sessionSetEncryption(tr_session* session, tr_encryption_mode mode);

/***********************************************************************
** Incoming Peer Connections Port
*/

void tr_sessionSetPortForwardingEnabled(tr_session* session, bool enabled);

bool tr_sessionIsPortForwardingEnabled(tr_session const* session);

void tr_sessionSetPeerPort(tr_session* session, uint16_t port);

uint16_t tr_sessionGetPeerPort(tr_session const* session);

uint16_t tr_sessionSetPeerPortRandom(tr_session* session);

void tr_sessionSetPeerPortRandomOnStart(tr_session* session, bool random);

bool tr_sessionGetPeerPortRandomOnStart(tr_session const* session);

enum tr_port_forwarding
{
    TR_PORT_ERROR,
    TR_PORT_UNMAPPED,
    TR_PORT_UNMAPPING,
    TR_PORT_MAPPING,
    TR_PORT_MAPPED
};

tr_port_forwarding tr_sessionGetPortForwarding(tr_session const* session);

enum tr_direction
{
    TR_CLIENT_TO_PEER = 0,
    TR_UP = 0,
    TR_PEER_TO_CLIENT = 1,
    TR_DOWN = 1
};

/***
****
***/

/***
****  Primary session speed limits
***/

void tr_sessionSetSpeedLimit_KBps(tr_session*, tr_direction, unsigned int KBps);
unsigned int tr_sessionGetSpeedLimit_KBps(tr_session const*, tr_direction);

void tr_sessionLimitSpeed(tr_session*, tr_direction, bool);
bool tr_sessionIsSpeedLimited(tr_session const*, tr_direction);

/***
****  Alternative speed limits that are used during scheduled times
***/

void tr_sessionSetAltSpeed_KBps(tr_session*, tr_direction, unsigned int Bps);
unsigned int tr_sessionGetAltSpeed_KBps(tr_session const*, tr_direction);

void tr_sessionUseAltSpeed(tr_session*, bool);
bool tr_sessionUsesAltSpeed(tr_session const*);

void tr_sessionUseAltSpeedTime(tr_session*, bool);
bool tr_sessionUsesAltSpeedTime(tr_session const*);

void tr_sessionSetAltSpeedBegin(tr_session*, int minsSinceMidnight);
int tr_sessionGetAltSpeedBegin(tr_session const*);

void tr_sessionSetAltSpeedEnd(tr_session*, int minsSinceMidnight);
int tr_sessionGetAltSpeedEnd(tr_session const*);

enum tr_sched_day
{
    TR_SCHED_SUN = (1 << 0),
    TR_SCHED_MON = (1 << 1),
    TR_SCHED_TUES = (1 << 2),
    TR_SCHED_WED = (1 << 3),
    TR_SCHED_THURS = (1 << 4),
    TR_SCHED_FRI = (1 << 5),
    TR_SCHED_SAT = (1 << 6),
    TR_SCHED_WEEKDAY = (TR_SCHED_MON | TR_SCHED_TUES | TR_SCHED_WED | TR_SCHED_THURS | TR_SCHED_FRI),
    TR_SCHED_WEEKEND = (TR_SCHED_SUN | TR_SCHED_SAT),
    TR_SCHED_ALL = (TR_SCHED_WEEKDAY | TR_SCHED_WEEKEND)
};

void tr_sessionSetAltSpeedDay(tr_session*, tr_sched_day day);
tr_sched_day tr_sessionGetAltSpeedDay(tr_session const*);

using tr_altSpeedFunc = void (*)(tr_session*, bool active, bool userDriven, void*);

void tr_sessionSetAltSpeedFunc(tr_session*, tr_altSpeedFunc, void*);

bool tr_sessionGetActiveSpeedLimit_KBps(tr_session const* session, tr_direction dir, double* setme);

/***
****
***/

double tr_sessionGetRawSpeed_KBps(tr_session const*, tr_direction);

void tr_sessionSetRatioLimited(tr_session*, bool isLimited);
bool tr_sessionIsRatioLimited(tr_session const*);

void tr_sessionSetRatioLimit(tr_session*, double desiredRatio);
double tr_sessionGetRatioLimit(tr_session const*);

void tr_sessionSetIdleLimited(tr_session*, bool isLimited);
bool tr_sessionIsIdleLimited(tr_session const*);

void tr_sessionSetIdleLimit(tr_session*, uint16_t idleMinutes);
uint16_t tr_sessionGetIdleLimit(tr_session const*);

void tr_sessionSetPeerLimit(tr_session*, uint16_t maxGlobalPeers);
uint16_t tr_sessionGetPeerLimit(tr_session const*);

void tr_sessionSetPeerLimitPerTorrent(tr_session*, uint16_t maxPeers);
uint16_t tr_sessionGetPeerLimitPerTorrent(tr_session const*);

void tr_sessionSetPaused(tr_session*, bool isPaused);
bool tr_sessionGetPaused(tr_session const*);

void tr_sessionSetDeleteSource(tr_session*, bool deleteSource);
bool tr_sessionGetDeleteSource(tr_session const*);

tr_priority_t tr_torrentGetPriority(tr_torrent const*);
void tr_torrentSetPriority(tr_torrent*, tr_priority_t);

void tr_sessionSetAntiBruteForceThreshold(tr_session*, int bad_requests);
int tr_sessionGetAntiBruteForceThreshold(tr_session const*);

void tr_sessionSetAntiBruteForceEnabled(tr_session*, bool enabled);
bool tr_sessionGetAntiBruteForceEnabled(tr_session const*);

/**
**/

/***
****
****  Torrent Queueing
****
****  There are independent queues for seeding (TR_UP) and leeching (TR_DOWN).
****
****  If the session already has enough non-stalled seeds/leeches when
****  tr_torrentStart() is called, the torrent will be moved into the
****  appropriate queue and its state will be TR_STATUS_{DOWNLOAD,SEED}_WAIT.
****
****  To bypass the queue and unconditionally start the torrent use
****  tr_torrentStartNow().
****
****  Torrents can be moved in the queue using the simple functions
****  tr_torrentQueueMove{Top,Up,Down,Bottom}. They can be moved to
****  arbitrary points in the queue with tr_torrentSetQueuePosition().
****
***/

/** @brief Like tr_torrentStart(), but resumes right away regardless of the queues. */
void tr_torrentStartNow(tr_torrent*);

/** @brief Return the queued torrent's position in the queue it's in. [0...n) */
int tr_torrentGetQueuePosition(tr_torrent const*);

/** @brief Set the queued torrent's position in the queue it's in.
 * Special cases: pos <= 0 moves to the front; pos >= queue length moves to the back */
void tr_torrentSetQueuePosition(tr_torrent*, int queuePosition);

/**
**/

/** @brief Convenience function for moving a batch of torrents to the front of their queue(s) */
void tr_torrentsQueueMoveTop(tr_torrent* const* torrents, size_t torrentCount);

/** @brief Convenience function for moving a batch of torrents ahead one step in their queue(s) */
void tr_torrentsQueueMoveUp(tr_torrent* const* torrents, size_t torrentCount);

/** @brief Convenience function for moving a batch of torrents back one step in their queue(s) */
void tr_torrentsQueueMoveDown(tr_torrent* const* torrents, size_t torrentCount);

/** @brief Convenience function for moving a batch of torrents to the back of their queue(s) */
void tr_torrentsQueueMoveBottom(tr_torrent* const* torrents, size_t torrentCount);

/**
**/

/** @brief Set the number of torrents allowed to download (if direction is TR_DOWN) or seed (if direction is TR_UP) at the same time */
void tr_sessionSetQueueSize(tr_session*, tr_direction, int max_simultaneous_seed_torrents);

/** @brief Return the number of torrents allowed to download (if direction is TR_DOWN) or seed (if direction is TR_UP) at the same time */
int tr_sessionGetQueueSize(tr_session const*, tr_direction);

/** @brief Set whether or not to limit how many torrents can download (TR_DOWN) or seed (TR_UP) at the same time  */
void tr_sessionSetQueueEnabled(tr_session*, tr_direction, bool do_limit_simultaneous_seed_torrents);

/** @brief Return true if we're limiting how many torrents can concurrently download (TR_DOWN) or seed (TR_UP) at the same time */
bool tr_sessionGetQueueEnabled(tr_session const*, tr_direction);

/**
**/

/** @brief Consider torrent as 'stalled' when it's been inactive for N minutes.
    Stalled torrents are left running but are not counted by tr_sessionGetQueueSize(). */
void tr_sessionSetQueueStalledMinutes(tr_session*, int minutes);

/** @return the number of minutes a torrent can be idle before being considered as stalled */
int tr_sessionGetQueueStalledMinutes(tr_session const*);

/** @brief Set whether or not to count torrents idle for over N minutes as 'stalled' */
void tr_sessionSetQueueStalledEnabled(tr_session*, bool);

/** @return true if we're torrents idle for over N minutes will be flagged as 'stalled' */
bool tr_sessionGetQueueStalledEnabled(tr_session const*);

/**
**/

/** @brief Set a callback that is invoked when the queue starts a torrent */
void tr_torrentSetQueueStartCallback(tr_torrent* torrent, void (*callback)(tr_torrent*, void*), void* user_data);

/***
****
****
***/

/**
 *  Load all the torrents in tr_getTorrentDir().
 *  This can be used at startup to kickstart all the torrents
 *  from the previous session.
 */
tr_torrent** tr_sessionLoadTorrents(tr_session* session, tr_ctor* ctor, int* setmeCount);

/**
***
**/

enum TrScript
{
    TR_SCRIPT_ON_TORRENT_ADDED,
    TR_SCRIPT_ON_TORRENT_DONE,
    TR_SCRIPT_ON_TORRENT_DONE_SEEDING,

    TR_SCRIPT_N_TYPES
};

void tr_sessionSetScript(tr_session*, TrScript, char const* script_filename);

char const* tr_sessionGetScript(tr_session const*, TrScript);

void tr_sessionSetScriptEnabled(tr_session*, TrScript, bool enabled);

bool tr_sessionIsScriptEnabled(tr_session const*, TrScript);

/** @} */

/**
***
**/

/** @addtogroup Blocklists
    @{ */

/**
 * Specify a range of IPs for Transmission to block.
 *
 * Filename must be an uncompressed ascii file.
 *
 * libtransmission does not keep a handle to `filename'
 * after this call returns, so the caller is free to
 * keep or delete `filename' as it wishes.
 * libtransmission makes its own copy of the file
 * massaged into a binary format easier to search.
 *
 * The caller only needs to invoke this when the blocklist
 * has changed.
 *
 * Passing nullptr for a filename will clear the blocklist.
 */
size_t tr_blocklistSetContent(tr_session* session, char const* filename);

size_t tr_blocklistGetRuleCount(tr_session const* session);

bool tr_blocklistExists(tr_session const* session);

bool tr_blocklistIsEnabled(tr_session const* session);

void tr_blocklistSetEnabled(tr_session* session, bool isEnabled);

/** @brief The blocklist that ges updated when an RPC client
           invokes the "blocklist-update" method */
void tr_blocklistSetURL(tr_session*, char const* url);

char const* tr_blocklistGetURL(tr_session const*);

/** @brief the file in the $config/blocklists/ directory that's
           used by tr_blocklistSetContent() and "blocklist-update" */
#define DEFAULT_BLOCKLIST_FILENAME "blocklist.bin"

/** @} */

/**
 * Instantiating tr_torrents and wrangling torrent file metadata
 *
 * 1. Torrent metadata is handled in the tr_torrent_metadata class.
 *
 * 2. Torrents should be instantiated using a torrent builder (tr_ctor).
 * Calling one of the tr_ctorSetMetainfo*() functions is required.
 * Other settings, e.g. torrent priority, are optional.
 * When ready, pass the builder object to tr_torrentNew().
 */

enum tr_ctorMode
{
    TR_FALLBACK, /* indicates the ctor value should be used only in case of missing resume settings */
    TR_FORCE /* indicates the ctor value should be used regardless of what's in the resume settings */
};

/** @brief Create a torrent constructor object used to instantiate a tr_torrent
    @param session the tr_session. */
tr_ctor* tr_ctorNew(tr_session const* session);

/** @brief Free a torrent constructor object */
void tr_ctorFree(tr_ctor* ctor);

/** @brief Set whether or not to delete the source torrent file
           when the torrent is added. (Default: False) */
void tr_ctorSetDeleteSource(tr_ctor* ctor, bool doDelete);

/** @brief Set the constructor's metainfo from a magnet link */
bool tr_ctorSetMetainfoFromMagnetLink(tr_ctor* ctor, char const* magnet, tr_error** error);

/** @brief Set the constructor's metainfo from a raw benc already in memory */
bool tr_ctorSetMetainfo(tr_ctor* ctor, char const* metainfo, size_t len, tr_error** error);

/** @brief Set the constructor's metainfo from a local torrent file */
bool tr_ctorSetMetainfoFromFile(tr_ctor* ctor, char const* filename, tr_error** error);

tr_torrent_metainfo const* tr_ctorGetMetainfo(tr_ctor const* ctor);

/** @brief Set how many peers this torrent can connect to. (Default: 50) */
void tr_ctorSetPeerLimit(tr_ctor* ctor, tr_ctorMode mode, uint16_t limit);

/** @brief Set the download folder for the torrent being added with this ctor.
    @see tr_ctorSetDownloadDir()
    @see tr_sessionInit() */
void tr_ctorSetDownloadDir(tr_ctor* ctor, tr_ctorMode mode, char const* directory);

/**
 * @brief Set the incompleteDir for this torrent.
 *
 * This is not a supported API call.
 * It only exists so the mac client can migrate
 * its older incompleteDir settings, and that's
 * the only place where it should be used.
 */
void tr_ctorSetIncompleteDir(tr_ctor* ctor, char const* directory);

/** Set whether or not the torrent begins downloading/seeding when created.
  (Default: not paused) */
void tr_ctorSetPaused(tr_ctor* ctor, tr_ctorMode mode, bool isPaused);

/** @brief Set the priorities for files in a torrent */
void tr_ctorSetFilePriorities(tr_ctor* ctor, tr_file_index_t const* files, tr_file_index_t fileCount, tr_priority_t priority);

/** @brief Set the download flag for files in a torrent */
void tr_ctorSetFilesWanted(tr_ctor* ctor, tr_file_index_t const* fileIndices, tr_file_index_t fileCount, bool wanted);

/** @brief Get this peer constructor's peer limit */
bool tr_ctorGetPeerLimit(tr_ctor const* ctor, tr_ctorMode mode, uint16_t* setmeCount);

/** @brief Get the "isPaused" flag from this peer constructor */
bool tr_ctorGetPaused(tr_ctor const* ctor, tr_ctorMode mode, bool* setmeIsPaused);

/** @brief Get the download path from this peer constructor */
bool tr_ctorGetDownloadDir(tr_ctor const* ctor, tr_ctorMode mode, char const** setmeDownloadDir);

/** @brief Get the "delete torrent file" flag from this peer constructor */
bool tr_ctorGetDeleteSource(tr_ctor const* ctor, bool* setmeDoDelete);

/** @brief Get the torrent file that this ctor's metainfo came from,
           or nullptr if tr_ctorSetMetainfoFromFile() wasn't used */
char const* tr_ctorGetSourceFile(tr_ctor const* ctor);

// TODO(ckerr) remove
enum tr_parse_result
{
    TR_PARSE_OK,
    TR_PARSE_ERR,
    TR_PARSE_DUPLICATE
};

/**
 * Instantiate a single torrent.
 *
 * Returns a pointer to the torrent on success, or nullptr on failure.
 *
 * @param ctor               the builder struct
 * @param setme_duplicate_of If the torrent couldn't be created because it's a duplicate,
 *                           this is set to point to the original torrent.
 */
tr_torrent* tr_torrentNew(tr_ctor* ctor, tr_torrent** setme_duplicate_of);

/** @} */

/***********************************************************************
 ***
 ***  TORRENTS
 **/

/** @addtogroup tr_torrent Torrents
    @{ */

using tr_fileFunc = bool (*)(char const* filename, struct tr_error** error);

/** @brief Removes our torrent and .resume files for this torrent */
void tr_torrentRemove(tr_torrent* torrent, bool removeLocalData, tr_fileFunc removeFunc);

/** @brief Start a torrent */
void tr_torrentStart(tr_torrent* torrent);

/** @brief Stop (pause) a torrent */
void tr_torrentStop(tr_torrent* torrent);

using tr_torrent_rename_done_func = void (*)( //
    tr_torrent* torrent,
    char const* oldpath,
    char const* newname,
    int error,
    void* user_data);

/**
 * @brief Rename a file or directory in a torrent.
 *
 * @param tor           the torrent whose path will be renamed
 * @param oldpath       the path to the file or folder that will be renamed
 * @param newname       the file or folder's new name
 * @param callback      the callback invoked when the renaming finishes, or nullptr
 * @param callback_data the pointer to pass in the callback's user_data arg
 *
 * As a special case, renaming the root file in a torrent will also
 * update tr_torrentName().
 *
 * EXAMPLES
 *
 *   Consider a tr_torrent where its
 *   tr_torrentFile(tor, 0).name is "frobnitz-linux/checksum" and
 *   tr_torrentFile(tor, 1).name is "frobnitz-linux/frobnitz.iso".
 *
 *   1. tr_torrentRenamePath(tor, "frobnitz-linux", "foo") will rename
 *      the "frotbnitz-linux" folder as "foo", and update both
 *      tr_torrentName(tor) and tr_torrentFile(tor, *).name.
 *
 *   2. tr_torrentRenamePath(tor, "frobnitz-linux/checksum", "foo") will
 *      rename the "frobnitz-linux/checksum" file as "foo" and update
 *      files[0].name to "frobnitz-linux/foo".
 *
 * RETURN
 *
 *   Changing the torrent's internal fields requires a session thread lock,
 *   so this function returns asynchronously to avoid blocking. If you don't
 *   want to be notified when the function has finished, you can pass nullptr
 *   as the callback arg.
 *
 *   On success, the callback's error argument will be 0.
 *
 *   If oldpath can't be found in files[*].name, or if newname is already
 *   in files[*].name, or contains a directory separator, or is nullptr, "",
 *   ".", or "..", the error argument will be EINVAL.
 *
 *   If the path exists on disk but can't be renamed, the error argument
 *   will be the errno set by rename().
 */
void tr_torrentRenamePath(
    tr_torrent* tor,
    char const* oldpath,
    char const* newname,
    tr_torrent_rename_done_func callback,
    void* callback_data);

enum
{
    TR_LOC_MOVING,
    TR_LOC_DONE,
    TR_LOC_ERROR
};

/**
 * @brief Tell transmission where to find this torrent's local data.
 *
 * if move_from_previous_location is `true', the torrent's incompleteDir
 * will be clobbered s.t. additional files being added will be saved
 * to the torrent's downloadDir.
 */
void tr_torrentSetLocation(
    tr_torrent* torrent,
    char const* location,
    bool move_from_previous_location,
    double volatile* setme_progress,
    int volatile* setme_state);

uint64_t tr_torrentGetBytesLeftToAllocate(tr_torrent const* torrent);

/**
 * @brief Returns this torrent's unique ID.
 *
 * IDs are fast lookup keys, but are not persistent between sessions.
 * If you need that, use tr_torrentView().hash_string.
 */
tr_torrent_id_t tr_torrentId(tr_torrent const* torrent);

tr_torrent* tr_torrentFindFromId(tr_session* session, int id);

tr_torrent* tr_torrentFindFromMetainfo(tr_session*, tr_torrent_metainfo const*);

tr_torrent* tr_torrentFindFromMagnetLink(tr_session* session, char const* link);

/**
 * @brief Set metainfo if possible.
 * @return True if given metainfo was set.
 *
 */
bool tr_torrentSetMetainfoFromFile(tr_torrent* torrent, tr_torrent_metainfo* metainfo, char const* filename);

/**
 * @return this torrent's name.
 */
char const* tr_torrentName(tr_torrent const*);

uint64_t tr_torrentTotalSize(tr_torrent const*);

/**
 * @brief find the location of a torrent's file by looking with and without
 *        the ".part" suffix, looking in downloadDir and incompleteDir, etc.
 * @return a newly-allocated string (that must be tr_free()d by the caller
 *         when done) that gives the location of this file on disk,
 *         or nullptr if no file exists yet.
 * @param tor the torrent whose file we're looking for
 * @param fileNum the fileIndex, in [0...tr_torrentFileCount())
 */
char* tr_torrentFindFile(tr_torrent const* tor, tr_file_index_t fileNum);

/***
****  Torrent speed limits
****
***/

void tr_torrentSetSpeedLimit_KBps(tr_torrent*, tr_direction, unsigned int KBps);
unsigned int tr_torrentGetSpeedLimit_KBps(tr_torrent const*, tr_direction);

void tr_torrentUseSpeedLimit(tr_torrent*, tr_direction, bool);
bool tr_torrentUsesSpeedLimit(tr_torrent const*, tr_direction);

void tr_torrentUseSessionLimits(tr_torrent*, bool);
bool tr_torrentUsesSessionLimits(tr_torrent const*);

/****
*****  Ratio Limits
****/

enum tr_ratiolimit
{
    /* follow the global settings */
    TR_RATIOLIMIT_GLOBAL = 0,
    /* override the global settings, seeding until a certain ratio */
    TR_RATIOLIMIT_SINGLE = 1,
    /* override the global settings, seeding regardless of ratio */
    TR_RATIOLIMIT_UNLIMITED = 2
};

void tr_torrentSetRatioMode(tr_torrent* tor, tr_ratiolimit mode);

tr_ratiolimit tr_torrentGetRatioMode(tr_torrent const* tor);

void tr_torrentSetRatioLimit(tr_torrent* tor, double ratio);

double tr_torrentGetRatioLimit(tr_torrent const* tor);

bool tr_torrentGetSeedRatio(tr_torrent const*, double* ratio);

/****
*****  Idle Time Limits
****/

enum tr_idlelimit
{
    /* follow the global settings */
    TR_IDLELIMIT_GLOBAL = 0,
    /* override the global settings, seeding until a certain idle time */
    TR_IDLELIMIT_SINGLE = 1,
    /* override the global settings, seeding regardless of activity */
    TR_IDLELIMIT_UNLIMITED = 2
};

void tr_torrentSetIdleMode(tr_torrent* tor, tr_idlelimit mode);

tr_idlelimit tr_torrentGetIdleMode(tr_torrent const* tor);

void tr_torrentSetIdleLimit(tr_torrent* tor, uint16_t idleMinutes);

uint16_t tr_torrentGetIdleLimit(tr_torrent const* tor);

bool tr_torrentGetSeedIdle(tr_torrent const*, uint16_t* minutes);

/****
*****  Peer Limits
****/

void tr_torrentSetPeerLimit(tr_torrent* tor, uint16_t peerLimit);

uint16_t tr_torrentGetPeerLimit(tr_torrent const* tor);

/****
*****  File Priorities
****/

enum
{
    TR_PRI_LOW = -1,
    TR_PRI_NORMAL = 0, /* since Normal is 0, memset initializes nicely */
    TR_PRI_HIGH = 1
};

/**
 * @brief Set a batch of files to a particular priority.
 *
 * @param priority must be one of TR_PRI_NORMAL, _HIGH, or _LOW
 */
void tr_torrentSetFilePriorities(
    tr_torrent* torrent,
    tr_file_index_t const* files,
    tr_file_index_t fileCount,
    tr_priority_t priority);

/** @brief Set a batch of files to be downloaded or not. */
void tr_torrentSetFileDLs(tr_torrent* torrent, tr_file_index_t const* files, tr_file_index_t fileCount, bool do_download);

/* Raw function to change the torrent's downloadDir field.
   This should only be used by libtransmission or to bootstrap
   a newly-instantiated tr_torrent object. */
void tr_torrentSetDownloadDir(tr_torrent* torrent, char const* path);

char const* tr_torrentGetDownloadDir(tr_torrent const* torrent);

/**
 * Returns the root directory of where the torrent is.
 *
 * This will usually be the downloadDir. However if the torrent
 * has an incompleteDir enabled and hasn't finished downloading
 * yet, that will be returned instead.
 */
char const* tr_torrentGetCurrentDir(tr_torrent const* tor);

/**
 * Returns a newly-allocated string with a magnet link of the torrent.
 * Use tr_free() to free the string when done.
 */
char* tr_torrentGetMagnetLink(tr_torrent const* tor);

/**
***
**/

/**
 * Returns a newly-allocated string listing its tracker's announce URLs.
 * One URL per line, with a blank line between tiers.
 *
 * NOTE: this only includes the trackers included in the torrent and,
 * along with tr_torrentSetTrackerList(), is intended for import/export
 * and user editing. It does *not* include the "default trackers" that
 * are applied to all public torrents. If you want a full display of all
 * trackers, use tr_torrentTracker() and tr_torrentTrackerCount()
 */
char* tr_torrentGetTrackerList(tr_torrent const* tor);

/**
 * Sets a torrent's tracker list from a list of announce URLs with one
 * URL per line and a blank line between tiers.
 *
 * This updates both the `torrent' object's tracker list
 * and the metainfo file in tr_sessionGetConfigDir()'s torrent subdirectory.
 */
bool tr_torrentSetTrackerList(tr_torrent* tor, char const* text);

/**
***
**/

enum tr_completeness
{
    TR_LEECH, /* doesn't have all the desired pieces */
    TR_SEED, /* has the entire torrent */
    TR_PARTIAL_SEED /* has the desired pieces, but not the entire torrent */
};

/**
 * @param wasRunning whether or not the torrent was running when
 *                   it changed its completeness state
 */
using tr_torrent_completeness_func = void (*)( //
    tr_torrent* torrent,
    tr_completeness completeness,
    bool wasRunning,
    void* user_data);

using tr_torrent_ratio_limit_hit_func = void (*)(tr_torrent* torrent, void* user_data);

using tr_torrent_idle_limit_hit_func = void (*)(tr_torrent* torrent, void* user_data);

/**
 * Register to be notified whenever a torrent's "completeness"
 * changes. This will be called, for example, when a torrent
 * finishes downloading and changes from TR_LEECH to
 * either TR_SEED or TR_PARTIAL_SEED.
 *
 * func is invoked FROM LIBTRANSMISSION'S THREAD!
 * This means func must be fast (to avoid blocking peers),
 * shouldn't call libtransmission functions (to avoid deadlock),
 * and shouldn't modify client-level memory without using a mutex!
 *
 * @see tr_completeness
 */
void tr_torrentSetCompletenessCallback(tr_torrent* torrent, tr_torrent_completeness_func func, void* user_data);

void tr_torrentClearCompletenessCallback(tr_torrent* torrent);

using tr_torrent_metadata_func = void (*)(tr_torrent* torrent, void* user_data);

/**
 * Register to be notified whenever a torrent changes from
 * having incomplete metadata to having complete metadata.
 * This happens when a magnet link finishes downloading
 * metadata from its peers.
 */
void tr_torrentSetMetadataCallback(tr_torrent* tor, tr_torrent_metadata_func func, void* user_data);

/**
 * Register to be notified whenever a torrent's ratio limit
 * has been hit. This will be called when the torrent's
 * ul/dl ratio has met or exceeded the designated ratio limit.
 *
 * Has the same restrictions as tr_torrentSetCompletenessCallback
 */
void tr_torrentSetRatioLimitHitCallback(tr_torrent* torrent, tr_torrent_ratio_limit_hit_func func, void* user_data);

/**
 * Register to be notified whenever a torrent's idle limit
 * has been hit. This will be called when the seeding torrent's
 * idle time has met or exceeded the designated idle limit.
 *
 * Has the same restrictions as tr_torrentSetCompletenessCallback
 */
void tr_torrentSetIdleLimitHitCallback(tr_torrent* torrent, tr_torrent_idle_limit_hit_func func, void* user_data);

/**
 * MANUAL ANNOUNCE
 *
 * Trackers usually set an announce interval of 15 or 30 minutes.
 * Users can send one-time announce requests that override this
 * interval by calling tr_torrentManualUpdate().
 *
 * The wait interval for tr_torrentManualUpdate() is much smaller.
 * You can test whether or not a manual update is possible
 * (for example, to desensitize the button) by calling
 * tr_torrentCanManualUpdate().
 */

void tr_torrentManualUpdate(tr_torrent* torrent);

bool tr_torrentCanManualUpdate(tr_torrent const* torrent);

/***
****  tr_peer_stat
***/

struct tr_peer_stat
{
    bool isUTP;

    bool isEncrypted;
    bool isDownloadingFrom;
    bool isUploadingTo;
    bool isSeed;

    bool peerIsChoked;
    bool peerIsInterested;
    bool clientIsChoked;
    bool clientIsInterested;
    bool isIncoming;

    uint8_t from;
    uint16_t port;

    char addr[TR_INET6_ADDRSTRLEN];
    char flagStr[32];
    char const* client;

    float progress;
    double rateToPeer_KBps;
    double rateToClient_KBps;

    /***
    ****  THESE NEXT FOUR FIELDS ARE EXPERIMENTAL.
    ****  Don't rely on them; they'll probably go away
    ***/
    /* how many blocks we've sent to this peer in the last 120 seconds */
    uint32_t blocksToPeer;
    /* how many blocks this client's sent to us in the last 120 seconds */
    uint32_t blocksToClient;
    /* how many requests to this peer that we've cancelled in the last 120 seconds */
    uint32_t cancelsToPeer;
    /* how many requests this peer made of us, then cancelled, in the last 120 seconds */
    uint32_t cancelsToClient;

    /* how many requests the peer has made that we haven't responded to yet */
    int activeReqsToClient;

    /* how many requests we've made and are currently awaiting a response for */
    int activeReqsToPeer;
};

tr_peer_stat* tr_torrentPeers(tr_torrent const* torrent, int* peerCount);

void tr_torrentPeersFree(tr_peer_stat* peerStats, int peerCount);

/***
****  tr_tracker_stat
***/

enum tr_tracker_state
{
    /* we won't (announce,scrape) this torrent to this tracker because
     * the torrent is stopped, or because of an error, or whatever */
    TR_TRACKER_INACTIVE = 0,
    /* we will (announce,scrape) this torrent to this tracker, and are
     * waiting for enough time to pass to satisfy the tracker's interval */
    TR_TRACKER_WAITING = 1,
    /* it's time to (announce,scrape) this torrent, and we're waiting on a
     * free slot to open up in the announce manager */
    TR_TRACKER_QUEUED = 2,
    /* we're (announcing,scraping) this torrent right now */
    TR_TRACKER_ACTIVE = 3
};

/*
 * Unlike other _view structs, it is safe to keep a tr_tracker_view copy.
 * The announce, scrape, and host strings are interned & never go out-of-scope.
 */
struct tr_tracker_view
{
    char const* announce; // full announce URL
    char const* scrape; // full scrape URL
    char const* host; // uniquely-identifying tracker name (`${host}:${port}`)

    // The tracker site's name. Uses the first label before the public suffix
    // (https://publicsuffix.org/) in the announce URL's host.
    // e.g. "https://www.example.co.uk/announce/"'s sitename is "example"
    // RFC 1034 says labels must be less than 64 chars
    char sitename[64];

    char lastAnnounceResult[128]; // if hasAnnounced, the human-readable result of latest announce
    char lastScrapeResult[128]; // if hasScraped, the human-readable result of the latest scrape

    time_t lastAnnounceStartTime; // if hasAnnounced, when the latest announce request was sent
    time_t lastAnnounceTime; // if hasAnnounced, when the latest announce reply was received
    time_t nextAnnounceTime; // if announceState == TR_TRACKER_WAITING, time of next announce

    time_t lastScrapeStartTime; // if hasScraped, when the latest scrape request was sent
    time_t lastScrapeTime; // if hasScraped, when the latest scrape reply was received
    time_t nextScrapeTime; // if scrapeState == TR_TRACKER_WAITING, time of next scrape

    int downloadCount; // number of times this torrent's been downloaded, or -1 if unknown
    int lastAnnouncePeerCount; // if hasAnnounced, the number of peers the tracker gave us
    int leecherCount; // number of leechers the tracker knows of, or -1 if unknown
    int seederCount; // number of seeders the tracker knows of, or -1 if unknown

    int tier; // which tier this tracker is in
    tr_torrent_id_t id; // unique transmission-generated ID for use in libtransmission API

    tr_tracker_state announceState; // whether we're announcing, waiting to announce, etc.
    tr_tracker_state scrapeState; // whether we're scraping, waiting to scrape, etc.

    bool hasAnnounced; // true iff we've announced to this tracker during this session
    bool hasScraped; // true iff we've scraped this tracker during this session
    bool isBackup; // only one tracker per tier is used; the others are kept as backups
    bool lastAnnounceSucceeded; // if hasAnnounced, whether or not the latest announce succeeded
    bool lastAnnounceTimedOut; // true iff the latest announce request timed out
    bool lastScrapeSucceeded; // if hasScraped, whether or not the latest scrape succeeded
    bool lastScrapeTimedOut; // true iff the latest scrape request timed out
};

struct tr_tracker_view tr_torrentTracker(tr_torrent const* torrent, size_t i);

/**
 * Count all the trackers (both active and backup) this torrent is using.
 *
 * NOTE: this is for a status display only and may include trackers from
 * the default tracker list if this is a public torrent. If you want a
 * list of trackers the  user can edit, see tr_torrentGetTrackerList().
 */
size_t tr_torrentTrackerCount(tr_torrent const* torrent);

/*
 * This view structure is intended for short-term use. Its pointers are owned
 * by the torrent and may be invalidated if the torrent is edited or removed.
 */
struct tr_file_view
{
    char const* name; // This file's name. Includes the full subpath in the torrent.
    uint64_t have; // the current size of the file, i.e. how much we've downloaded
    uint64_t length; // the total size of the file
    double progress; // have / length
    tr_priority_t priority; // the file's priority
    bool wanted; // do we want to download this file?
};
tr_file_view tr_torrentFile(tr_torrent const* torrent, tr_file_index_t file);

size_t tr_torrentFileCount(tr_torrent const* torrent);

/*
 * This view structure is intended for short-term use. Its pointers are owned
 * by the torrent and may be invalidated if the torrent is edited or removed.
 */
struct tr_webseed_view
{
    char const* url; // the url to download from
    bool is_downloading; // can be true even if speed is 0, e.g. slow download
    unsigned download_bytes_per_second; // current download speed
};

struct tr_webseed_view tr_torrentWebseed(tr_torrent const* torrent, size_t nth);

size_t tr_torrentWebseedCount(tr_torrent const* torrent);

/*
 * This view structure is intended for short-term use. Its pointers are owned
 * by the torrent and may be invalidated if the torrent is edited or removed.
 */
struct tr_torrent_view
{
    char const* name;
    char const* hash_string;

    char const* comment; // optional; may be nullptr
    char const* creator; // optional; may be nullptr
    char const* source; // optional; may be nullptr

    uint64_t total_size; // total size of the torrent, in bytes

    time_t date_created;

    uint32_t piece_size;
    tr_piece_index_t n_pieces;

    bool is_private;
    bool is_folder;
};

struct tr_torrent_view tr_torrentView(tr_torrent const* tor);

/*
 * Get the filename of Transmission's internal copy of the torrent file.
 * This is a duplicate that must be freed with tr_free() when done.
 */
char* tr_torrentFilename(tr_torrent const* tor);

/***********************************************************************
 * tr_torrentAvailability
 ***********************************************************************
 * Use this to draw an advanced progress bar which is 'size' pixels
 * wide. Fills 'tab' which you must have allocated: each byte is set
 * to either -1 if we have the piece, otherwise it is set to the number
 * of connected peers who have the piece.
 **********************************************************************/
void tr_torrentAvailability(tr_torrent const* torrent, int8_t* tab, int size);

void tr_torrentAmountFinished(tr_torrent const* torrent, float* tab, int size);

/**
 * Queue a torrent for verification.
 */
void tr_torrentVerify(tr_torrent* torrent);

bool tr_torrentHasMetadata(tr_torrent const* tor);

/**
 * What the torrent is doing right now.
 *
 * Note: these values will become a straight enum at some point in the future.
 * Do not rely on their current `bitfield' implementation
 */
enum tr_torrent_activity
{
    TR_STATUS_STOPPED = 0, /* Torrent is stopped */
    TR_STATUS_CHECK_WAIT = 1, /* Queued to check files */
    TR_STATUS_CHECK = 2, /* Checking files */
    TR_STATUS_DOWNLOAD_WAIT = 3, /* Queued to download */
    TR_STATUS_DOWNLOAD = 4, /* Downloading */
    TR_STATUS_SEED_WAIT = 5, /* Queued to seed */
    TR_STATUS_SEED = 6 /* Seeding */
};

enum
{
    TR_PEER_FROM_INCOMING = 0, /* connections made to the listening port */
    TR_PEER_FROM_LPD, /* peers found by local announcements */
    TR_PEER_FROM_TRACKER, /* peers found from a tracker */
    TR_PEER_FROM_DHT, /* peers found from the DHT */
    TR_PEER_FROM_PEX, /* peers found from PEX */
    TR_PEER_FROM_RESUME, /* peers found in the .resume file */
    TR_PEER_FROM_LTEP, /* peer address provided in an LTEP handshake */
    TR_PEER_FROM__MAX
};

enum tr_stat_errtype
{
    /* everything's fine */
    TR_STAT_OK = 0,
    /* when we announced to the tracker, we got a warning in the response */
    TR_STAT_TRACKER_WARNING = 1,
    /* when we announced to the tracker, we got an error in the response */
    TR_STAT_TRACKER_ERROR = 2,
    /* local trouble, such as disk full or permissions error */
    TR_STAT_LOCAL_ERROR = 3
};

/** @brief Used by tr_torrentStat() to tell clients about a torrent's state and statistics */
struct tr_stat
{
    /** A warning or error message regarding the torrent.
        @see error */
    char const* errorString;

    /** Byte count of all the piece data we'll have downloaded when we're done,
        whether or not we have it yet. This may be less than tr_torrentTotalSize()
        if only some of the torrent's files are wanted.
        [0...tr_torrentTotalSize()] */
    uint64_t sizeWhenDone;

    /** Byte count of how much data is left to be downloaded until we've got
        all the pieces that we want. [0...tr_stat.sizeWhenDone] */
    uint64_t leftUntilDone;

    /** Byte count of all the piece data we want and don't have yet,
        but that a connected peer does have. [0...leftUntilDone] */
    uint64_t desiredAvailable;

    /** Byte count of all the corrupt data you've ever downloaded for
        this torrent. If you're on a poisoned torrent, this number can
        grow very large. */
    uint64_t corruptEver;

    /** Byte count of all data you've ever uploaded for this torrent. */
    uint64_t uploadedEver;

    /** Byte count of all the non-corrupt data you've ever downloaded
        for this torrent. If you deleted the files and downloaded a second
        time, this will be 2*totalSize.. */
    uint64_t downloadedEver;

    /** Byte count of all the checksum-verified data we have for this torrent.
      */
    uint64_t haveValid;

    /** Byte count of all the partial piece data we have for this torrent.
        As pieces become complete, this value may decrease as portions of it
        are moved to `corrupt' or `haveValid'. */
    uint64_t haveUnchecked;

    /** time when one or more of the torrent's trackers will
        allow you to manually ask for more peers,
        or 0 if you can't */
    time_t manualAnnounceTime;

    /** When the torrent was first added. */
    time_t addedDate;

    /** When the torrent finished downloading. */
    time_t doneDate;

    /** When the torrent was last started. */
    time_t startDate;

    /** The last time we uploaded or downloaded piece data on this torrent. */
    time_t activityDate;

    /** The last time during this session that a rarely-changing field
        changed -- e.g. any tr_torrent_metainfo field (trackers, filenames, name)
        or download directory. RPC clients can monitor this to know when
        to reload fields that rarely change. */
    time_t editDate;

    /** When tr_stat.activity is TR_STATUS_CHECK or TR_STATUS_CHECK_WAIT,
        this is the percentage of how much of the files has been
        verified. When it gets to 1, the verify process is done.
        Range is [0..1]
        @see tr_stat.activity */
    float recheckProgress;

    /** How much has been downloaded of the entire torrent.
        Range is [0..1] */
    float percentComplete;

    /** How much of the metadata the torrent has.
        For torrents added from a torrent this will always be 1.
        For magnet links, this number will from from 0 to 1 as the metadata is downloaded.
        Range is [0..1] */
    float metadataPercentComplete;

    /** How much has been downloaded of the files the user wants. This differs
        from percentComplete if the user wants only some of the torrent's files.
        Range is [0..1]
        @see tr_stat.leftUntilDone */
    float percentDone;

    /** How much has been uploaded to satisfy the seed ratio.
        This is 1 if the ratio is reached or the torrent is set to seed forever.
        Range is [0..1] */
    float seedRatioPercentDone;

    /** Speed all piece being sent for this torrent.
        This ONLY counts piece data. */
    float pieceUploadSpeed_KBps;

    /** Speed all piece being received for this torrent.
        This ONLY counts piece data. */
    float pieceDownloadSpeed_KBps;

    /** Total uploaded bytes / sizeWhenDone.
        NB: In Transmission 3.00 and earlier, this was total upload / download,
        which caused edge cases when total download was less than sizeWhenDone. */
    float ratio;

    /** The torrent's unique Id.
        @see tr_torrentId() */
    tr_torrent_id_t id;

    /** Number of seconds since the last activity (or since started).
        -1 if activity is not seeding or downloading. */
    int idleSecs;

    /** Cumulative seconds the torrent's ever spent downloading */
    int secondsDownloading;

    /** Cumulative seconds the torrent's ever spent seeding */
    int secondsSeeding;

    /** This torrent's queue position.
        All torrents have a queue position, even if it's not queued. */
    int queuePosition;

#define TR_ETA_NOT_AVAIL (-1)
#define TR_ETA_UNKNOWN (-2)
    /** If downloading, estimated number of seconds left until the torrent is done.
        If seeding, estimated number of seconds left until seed ratio is reached. */
    int eta;

    /** If seeding, number of seconds left until the idle time limit is reached. */
    int etaIdle;

    /** What is this torrent doing right now? */
    tr_torrent_activity activity;

    /** Defines what kind of text is in errorString.
        @see errorString */
    tr_stat_errtype error;

    /** Number of peers that we're connected to */
    uint16_t peersConnected;

    /** How many peers we found out about from the tracker, or from pex,
        or from incoming connections, or from our resume file. */
    uint16_t peersFrom[TR_PEER_FROM__MAX];

    /** Number of peers that are sending data to us. */
    uint16_t peersSendingToUs;

    /** Number of peers that we're sending data to */
    uint16_t peersGettingFromUs;

    /** Number of webseeds that are sending data to us. */
    uint16_t webseedsSendingToUs;

    /** A torrent is considered finished if it has met its seed ratio.
        As a result, only paused torrents can be finished. */
    bool finished;

    /** True if the torrent is running, but has been idle for long enough
        to be considered stalled.  @see tr_sessionGetQueueStalledMinutes() */
    bool isStalled;
};

/** Return a pointer to an tr_stat structure with updated information
    on the torrent. This is typically called by the GUI clients every
    second or so to get a new snapshot of the torrent's status. */
tr_stat const* tr_torrentStat(tr_torrent* torrent);

/** Like tr_torrentStat(), but only recalculates the statistics if it's
    been longer than a second since they were last calculated. This can
    reduce the CPU load if you're calling tr_torrentStat() frequently. */
tr_stat const* tr_torrentStatCached(tr_torrent* torrent);

/** @} */

/** @brief Sanity checker to test that the direction is TR_UP or TR_DOWN */
constexpr bool tr_isDirection(tr_direction d)
{
    return d == TR_UP || d == TR_DOWN;
}
