/* App identity of the "DStudio 1017" fork. Everything that would otherwise make
 * this build share state with, or collide with, a running stock DStudio lives
 * here so the separation is auditable in one place.
 *
 * Intentionally still shared with stock DStudio (see NOTES): /tmp/ds4.lock
 * (one heavy engine at a time), user skills, ~/.dstudio
 * helpers (embedding server on :28101, image/pdf caches, heavy-model lock), the
 * iogpu LaunchDaemon, and the model weights (via the gguf symlink below). */
#ifndef DSTUDIO_IDENTITY_H
#define DSTUDIO_IDENTITY_H

/* Folder name under ~/Library/Application Support (macOS) / %LOCALAPPDATA% (Windows). */
#define DSTUDIO_SUPPORT_NAME "DStudio 1017"
/* Folder name under $XDG_CONFIG_HOME or ~/.config (Linux). */
#define DSTUDIO_SUPPORT_NAME_LINUX "dstudio1017"
/* Window / application title. */
#define DSTUDIO_APP_TITLE "DStudio 1017"
/* Single-instance lock: $TMPDIR/<DSTUDIO_LOCK_BASENAME>-<uid>.lock; Windows mutex name. */
#define DSTUDIO_LOCK_BASENAME "dstudio1017"
#define DSTUDIO_WIN_MUTEX_NAME "Local\\DStudio1017SingleInstance"
/* Default ports (stock DStudio uses 5500 / 28000). */
#define DSTUDIO_DEFAULT_HTTP_PORT 5517
#define DSTUDIO_DEFAULT_ENGINE_PORT 28017
/* Background model-download log (stock DStudio writes /tmp/ds4-model-dl.log and truncates it on open). */
#define DSTUDIO_DOWNLOAD_LOG "/tmp/ds4-model-dl-1017.log"
/* KV prompt-cache root (default; DSTUDIO_KV_DIR still overrides). Stock DStudio uses "ds4-kv" next to it,
 * so "Clear all conversations" here can never empty the stock app's caches. Every user of the path goes
 * through kv_root()/kv_dir_for_model() in dstudio.c. */
#define DSTUDIO_KV_DIRNAME "ds4-kv-1017"
/* Stock weights folder, relative to $HOME. The fork's <ds4 checkout>/gguf is made a
 * symlink to <HOME>/<this> so both apps share one copy of the model files. */
#define DSTUDIO_STOCK_SUPPORT_SUBPATH "Library/Application Support/DStudio"
#define DSTUDIO_SHARED_GGUF_SUBPATH DSTUDIO_STOCK_SUPPORT_SUBPATH "/ds4/gguf"

#endif
