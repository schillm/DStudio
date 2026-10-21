/* Production installer/model guards with real temporary files. No network,
 * compilation or model inference is claimed by this local regression suite. */
#define _GNU_SOURCE
#include <assert.h>
#define main dstudio_embedded_main_for_tests
#include "../../src/dstudio.c"
#undef main

/* Parse the prompt actually produced for the runtime, not the C source. The
 * legacy path still needs every inline function; the structured path sends
 * those definitions in request.tools (exercised by q36_agent_host_test). */
static void check_prompt_catalog(int mode, int structured) {
    const char *names[] = {"skill", "design_system", "pack_file", "skills_search",
                          "read_pdf", "question", "gsa_submit_phase"};
    char *prompt = build_piped_skill_sys(mode, structured), *save = NULL;
    assert(prompt);
    unsigned seen = 0;
    for (char *line = strtok_r(prompt, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (*line != '{') continue;
        dtg_json_token tokens[128]; char error[128], name[64];
        assert(dtg_json_validate_complete(line, '{', error, sizeof error));
        int count = dtg_json_tokenize(line, strlen(line), tokens, 128);
        assert(count > 0);
        int type = dtg_json_object_field(line, tokens, count, 0, "type");
        assert(type >= 0 && dtg_json_token_eq(line, &tokens[type], "function"));
        int fn = dtg_json_object_field(line, tokens, count, 0, "function");
        int id = dtg_json_object_field(line, tokens, count, fn, "name");
        int parameters = dtg_json_object_field(line, tokens, count, fn, "parameters");
        assert(id >= 0 && dtg_json_token_string(line, &tokens[id], name, sizeof name));
        assert(parameters >= 0 && tokens[parameters].type == DTG_JSON_OBJECT);
        size_t i;
        for (i = 0; i < sizeof names / sizeof names[0] && strcmp(name, names[i]); i++) {}
        assert(i < sizeof names / sizeof names[0] && !(seen & (1u << i)));
        seen |= 1u << i;
    }
    unsigned expected = structured || mode == ENGINE_DESIGN ? 0 : mode == ENGINE_AGENT ? 127 : 63;
    assert(seen == expected);
    free(prompt);
}

static void m3_lock_test(const char *root) {
    {   char lock[1300]; snprintf(lock, sizeof lock, "%s/ds4.lock", root);
            const char *old_lock = getenv("DS4_LOCK_FILE"); char saved_lock[1300] = ""; if (old_lock) cstr_copy(saved_lock, sizeof saved_lock, old_lock);
            assert(setenv("DS4_LOCK_FILE", lock, 1) == 0);
            assert(ds4_instance_lock_owner() == 0);                                     /* free */
            for (int stock_port = 1; stock_port >= 0; stock_port--) {
                pid_t kid = fork(); assert(kid >= 0);
                if (kid == 0) {
                    char port[16]; snprintf(port, sizeof port, "%d", stock_port ? 28000 : 28017);
                    execlp("python3", "python3", "-c", "import time; time.sleep(30)", "--port", port, (char *)NULL);
                    _exit(127);
                }
                int lfd = open(lock, O_RDWR | O_CREAT | O_TRUNC, 0600); assert(lfd >= 0);
                assert(flock(lfd, LOCK_EX | LOCK_NB) == 0);
                char pid[32]; int pn = snprintf(pid, sizeof pid, "%ld", (long)kid); assert(write(lfd, pid, pn) == pn);
                usleep(300000);                                                         /* let exec happen */
                pid_t owner = ds4_instance_lock_owner(); assert(owner == kid);
                assert(ds4_lock_owner_foreign(owner, 28017) == stock_port);
                kill(kid, SIGKILL); waitpid(kid, NULL, 0); flock(lfd, LOCK_UN); close(lfd);
            }
            assert(unlink(lock) == 0);
            if (old_lock) setenv("DS4_LOCK_FILE", saved_lock, 1); else unsetenv("DS4_LOCK_FILE");
        }
}

int main(void) {
    assert(model_file_is_auxiliary("DeepSeek-V4.1-Flash-Vision.gguf"));
    assert(!model_file_is_supported(MODEL_DS41_VISION));
    /* 1017 community checkpoint: sidecar is auxiliary on every platform, both
     * files are V4.1 for launch rules, and never pairs with stock files. */
    assert(model_file_is_auxiliary("DeepSeek-V4.1-Flash-1017-Vision.gguf"));
    assert(!model_file_is_supported(MODEL_DS41_1017_VISION));
    assert(model_file_is_deepseek41(MODEL_DS41_1017_Q2) && model_file_is_deepseek41(MODEL_DS41_1017_VISION));
    assert(!model_file_is_supported("DeepSeek-V4.1-Flash-1017-Q4.gguf"));
    assert(!model_file_is_supported("DeepSeek-V4.1-Flash-1017-Q2.gguf.part"));
#ifdef __APPLE__
    assert(model_file_is_supported(MODEL_DS41_1017_Q2));
    assert(!strcmp(native_vision_encoder_rel_for_model(MODEL_DS41_1017_Q2), MODEL_DS41_1017_VISION));
    assert(!strcmp(native_vision_encoder_rel_for_model("/x/gguf/DeepSeek-V4.1-Flash-1017-Q2.gguf"), MODEL_DS41_1017_VISION));
    assert(!strcmp(native_vision_encoder_rel_for_model(MODEL_DS41_Q4), MODEL_DS41_VISION));
#else
    assert(!model_file_is_supported(MODEL_DS41_1017_Q2));
#endif
#ifdef __APPLE__
    assert(model_file_is_supported(MODEL_DS41_Q2));
    assert(model_file_is_supported(MODEL_DS41_Q4));
    assert(!strcmp(native_vision_encoder_rel_for_model(MODEL_DS41_Q2), MODEL_DS41_VISION));
#else
    assert(!model_file_is_supported(MODEL_DS41_Q2));
    assert(!model_file_is_supported(MODEL_DS41_Q4));
#endif
    assert(!model_file_is_supported("DeepSeek-V4.1-Flash-Unknown.gguf"));
    assert(!model_file_is_deepseek41(MODEL_FLASH));
    cstr_copy(g_model_override, sizeof g_model_override, MODEL_DS41_Q2);
    assert(!model_is_flash()); /* Never reuse V4's memory or DSpark estimates. */
    assert(model_file_is_auxiliary("Qwen3.8-Flash-Next-PLE-Q4_1.gguf"));
    assert(model_file_is_auxiliary("Qwen3.8-Flash-Next-Q4KImatrix-MTP.gguf"));
    assert(!model_file_is_supported("gguf/Qwen3.8-Flash-Next-PLE-Q4_1.gguf"));
    assert(model_file_is_supported(MODEL_QWEN));
    assert(model_file_is_supported(MODEL_QWEN_Q2));
    assert(setup_engine_source("main") && !setup_engine_source("qwen"));
    assert(model_file_is_legacy_qwen38(MODEL_LEGACY_QWEN));
    assert(!model_file_is_legacy_qwen38(MODEL_QWEN));
    /* The dense checkpoint has its own route; the projector is never a Chat
     * model. Other quantizations still require their own qualification. */
    assert(model_file_is_auxiliary("Qwen3.8-27B-mmproj-F16.gguf"));
    assert(!model_file_is_supported("Qwen3.8-27B-mmproj-F16.gguf"));
    assert(model_file_is_supported("Qwen3.8-27B-UD-Q6_K_XL.gguf"));
    assert(!model_file_is_supported("Qwen3.8-27B-UD-Q4_K_XL.gguf"));
    assert(!model_file_is_qwen38("Qwen3.8-27B-UD-Q6_K_XL.gguf"));
    cstr_copy(g_model_override, sizeof g_model_override, MODEL_QWEN);
    assert(model_is_qwen() && !model_is_flash() && !model_is_laguna());
    engine_cfg cfg = ENGINE_DEFAULTS;
    char err[8600] = "", reason[256] = "";
#ifdef __APPLE__
    cfg.power = 50;
    assert(!strcmp(native_launch_preflight(&cfg, ENGINE_SERVER, MODEL_DS41_Q2, 0, 0, err, sizeof err), "unsupported_power"));
    assert(cfg.power == 50 && g_child <= 0);
    cfg.power = 100;
    assert(!strcmp(native_launch_preflight(&cfg, ENGINE_SERVER, MODEL_DS41_Q2, 0, 1, err, sizeof err), "unsupported_speculation"));
    cfg.ssd_streaming = SSD_STREAMING_ON;
    assert(model_ssd_streaming(&cfg, 0, MODEL_DS41_Q2, 0, reason, sizeof reason, err, sizeof err) == 1);
    assert(strstr(reason, "Engram") && strstr(reason, "separately"));
    cfg.ssd_streaming = SSD_STREAMING_OFF;
    assert(model_ssd_streaming(&cfg, 0, MODEL_DS41_Q2, 0, reason, sizeof reason, err, sizeof err) == 0);
    assert(strstr(reason, "Engram") && strstr(reason, "disk-backed"));
    /* Main's Qwen3.8 path refuses GPU throttling when the engine opens. The
     * default 90% must be rejected before a working model is stopped. */
    cfg.power = ENGINE_DEFAULTS.power;
    assert(cfg.power != 100);
    const char *throttled = native_launch_preflight(&cfg, ENGINE_SERVER, MODEL_QWEN, 0, 0, err, sizeof err);
    assert(throttled && !strcmp(throttled, "unsupported_power"));
    assert(strstr(err, "Qwen Next") && strstr(err, "100%") && cfg.power == ENGINE_DEFAULTS.power && g_child <= 0);
    throttled = native_launch_preflight(&cfg, ENGINE_AGENT, MODEL_QWEN, 0, 0, err, sizeof err);
    assert(throttled && !strcmp(throttled, "unsupported_power"));
    cfg.power = 100;
    const char *full_power = native_launch_preflight(&cfg, ENGINE_SERVER, MODEL_QWEN, 0, 0, err, sizeof err);
    assert(!full_power || strcmp(full_power, "unsupported_power"));
#endif
    // Execute the native preflight for every model named by the UI notice.
    const char *streaming_models[] = { MODEL_FLASH, MODEL_DSVISION_Q2, MODEL_GLM53_Q2, MODEL_PRO };
    cfg.ssd_streaming = SSD_STREAMING_ON;
    for (size_t i = 0; i < sizeof streaming_models / sizeof streaming_models[0]; i++) {
        cstr_copy(g_model_override, sizeof g_model_override, streaming_models[i]);
        int expected = 1;
#ifdef _WIN32
        expected = -1;
#endif
        assert(engine_effective_ssd_streaming(&cfg, 0, reason, sizeof reason, err, sizeof err) == expected);
        assert(engine_effective_ssd_streaming(&cfg, 1, reason, sizeof reason, err, sizeof err) == -1);
        g_dspark_enabled = 1;
        assert(engine_effective_ssd_streaming(&cfg, 0, reason, sizeof reason, err, sizeof err) == -1);
        g_dspark_enabled = 0;
    }
    cstr_copy(g_model_override, sizeof g_model_override, MODEL_LAGUNA);
    assert(engine_effective_ssd_streaming(&cfg, 0, reason, sizeof reason, err, sizeof err) == -1);
    cstr_copy(g_model_override, sizeof g_model_override, MODEL_QWEN);
    cfg.ssd_streaming = SSD_STREAMING_ON;
    assert(engine_effective_ssd_streaming(&cfg, 0, reason, sizeof reason, err, sizeof err) == -1);
    cfg.ssd_streaming = SSD_STREAMING_AUTO;
    assert(engine_effective_ssd_streaming(&cfg, 0, reason, sizeof reason, err, sizeof err) == 0);
    assert(!err[0]);
    cfg.ssd_streaming = SSD_STREAMING_OFF;
    assert(cfg_ssd_streaming(&cfg, 0, err, sizeof err));
    assert(!g_ssd_streaming_effective && !err[0]);
    assert(strstr(g_ssd_streaming_reason, "BF16 n-grams stay on SSD inside the GGUF"));
    g_dspark_enabled = 1;
    int requested_dspark = 1;
    assert(normalize_flash_memory_request(&cfg, 0, MODEL_QWEN, &requested_dspark, 0, reason, sizeof reason, NULL, NULL));
    assert(!requested_dspark && g_dspark_enabled == 1);
    g_dspark_enabled = 0;
#ifndef _WIN32
    char temp[] = "/tmp/dstudio-engine-setup-unit.XXXXXX";
    assert(mkdtemp(temp));
    /* Exercise actual persistence, not just a path-string contract: an empty
     * explicit profile must not hydrate the user's legacy conversations. */
    char legacy_store[2048], profile_store[2048], restored_store[2048];
    const char *prior_data = getenv("DS4UI_DATA_DIR");
    char *saved_data = prior_data ? strdup(prior_data) : NULL;
    unsetenv("DS4UI_DATA_DIR");
    store_file_path(legacy_store, sizeof legacy_store);
    assert(setenv("DS4UI_DATA_DIR", temp, 1) == 0);
    store_file_path(profile_store, sizeof profile_store);
    assert(strcmp(profile_store, legacy_store));
    assert(g_store == NULL);
    store_load();
    assert(g_store == NULL && g_store_rev == 0);
    g_store = strdup("{\"profile\":\"isolated-first-launch\"}");
    assert(g_store);
    g_store_len = strlen(g_store);
    store_save();
    free(g_store); g_store = NULL; g_store_len = 0;
    store_load();
    assert(g_store && !strcmp(g_store, "{\"profile\":\"isolated-first-launch\"}"));
    assert(g_store_rev == 1);
    free(g_store); g_store = NULL; g_store_len = 0; g_store_rev = 0;
    assert(unlink(profile_store) == 0);
    unsetenv("DS4UI_DATA_DIR");
    store_file_path(restored_store, sizeof restored_store);
    assert(!strcmp(restored_store, legacy_store));
    if (saved_data) { setenv("DS4UI_DATA_DIR", saved_data, 1); free(saved_data); }
    /* Signal shutdown may nudge an owned child, never a shared process. */
    pid_t supervised = fork();
    assert(supervised >= 0);
    if (supervised == 0) { for (;;) pause(); }
    g_child = supervised; g_external_server = 1;
    on_term(SIGTERM);
    usleep(20000);
    assert(kill(supervised, 0) == 0);
    g_external_server = 0;
    on_term(SIGTERM);
    int stopped_status = 0;
    assert(waitpid(supervised, &stopped_status, 0) == supervised);
    assert(WIFSIGNALED(stopped_status) && WTERMSIG(stopped_status) == SIGTERM);
    g_child = -1; g_stop = 0;
    char primary[512], shared[512], side[512], link[512], other[512], marker[512];
    snprintf(primary, sizeof primary, "%s/ds4", temp);
    snprintf(shared, sizeof shared, "%s/ds4/gguf", temp);
    snprintf(side, sizeof side, "%s/ds4-qwen38", temp);
    snprintf(link, sizeof link, "%s/ds4-qwen38/gguf", temp);
    snprintf(other, sizeof other, "%s/other-model-store", temp);
    snprintf(marker, sizeof marker, "%s/ds4/user-data.txt", temp);
    assert(mkdir(primary, 0755) == 0);
    assert(mkdir(side, 0755) == 0);
    assert(mkdir(other, 0755) == 0);
    cstr_copy(g_ds4_dir, sizeof g_ds4_dir, side);
    assert(selected_checkout_is_qwen());
    assert(!spawn_server(&cfg, err, sizeof err)); /* Required PLE is absent. */
    assert(!spawn_agent(&cfg, temp, 0, err, sizeof err));
    assert(!spawn_agent(&cfg, temp, 1, err, sizeof err));
    assert(!spawn_design(&cfg, temp, err, sizeof err));
    cstr_copy(g_model_override, sizeof g_model_override, MODEL_FLASH);
    assert(!spawn_server(&cfg, err, sizeof err)); /* Never patch Qwen as DeepSeek at boot. */
    assert(g_child <= 0);
    assert(setenv("DSTUDIO_KV_DIR", temp, 1) == 0);
    char kv[512]; kv_root(kv, sizeof kv); assert(!strcmp(kv, temp));
    unsetenv("DSTUDIO_KV_DIR");
    assert(jsonl_write_file(marker, "user data", 9));
    char target[DSTUDIO_PATH_MAX]; int downloaded = 0;
    assert(!setup_install_engine("main", temp, target, sizeof target, &downloaded, err, sizeof err));
    assert(!downloaded);
    FILE *f = fopen(marker, "rb"); char content[10] = "";
    assert(f && fread(content, 1, 9, f) == 9 && !strcmp(content, "user data"));
    fclose(f);
    assert(!setup_install_engine("unknown", temp, target, sizeof target, &downloaded, err, sizeof err));
    assert(!setup_install_engine("main", marker, target, sizeof target, &downloaded, err, sizeof err));

    cstr_copy(g_web_dir, sizeof g_web_dir, temp);
    const char *prompt_models[] = {MODEL_FLASH, MODEL_GLM53_Q2, MODEL_LAGUNA,
                                  MODEL_QWEN, MODEL_QWEN35, MODEL_QWEN27};
    for (size_t i = 0; i < sizeof prompt_models / sizeof prompt_models[0]; i++) {
        cstr_copy(g_model_override, sizeof g_model_override, prompt_models[i]);
        /* Both native legacy and actual remote/DSML retain their catalogs. */
        for (int remote = 0; remote < 2; remote++) {
            cstr_copy(g_remote_base_url, sizeof g_remote_base_url, remote ? "prepare-only" : "");
            check_prompt_catalog(ENGINE_AGENT, 0);
            check_prompt_catalog(ENGINE_COWORK, 0);
            check_prompt_catalog(ENGINE_DESIGN, 0);
        }
    }
    /* q36's private builder uses remote compilation without changing the
     * admitted structured protocol. The explicit selection must survive it. */
    for (int remote_build = 0; remote_build < 2; remote_build++) {
        cstr_copy(g_remote_base_url, sizeof g_remote_base_url, remote_build ? "prepare-only" : "");
        check_prompt_catalog(ENGINE_AGENT, 1);
        check_prompt_catalog(ENGINE_COWORK, 1);
        check_prompt_catalog(ENGINE_DESIGN, 1);
    }
    g_remote_base_url[0] = '\0';
    cstr_copy(g_model_override, sizeof g_model_override, MODEL_FLASH);
    assert(jsonl_write_file(shared, "not a directory", 15));
    assert(!setup_link_shared_gguf(side, err, sizeof err));
    assert(access(link, F_OK) != 0);
    assert(unlink(shared) == 0);
    assert(mkdir(shared, 0755) == 0);
    assert(mkdir(link, 0755) == 0); /* Empty optional folder is replaced. */
    assert(setup_link_shared_gguf(side, err, sizeof err));
    assert(setup_link_shared_gguf(side, err, sizeof err)); /* Idempotence. */
    struct stat a, b;
    assert(stat(shared, &a) == 0 && stat(link, &b) == 0);
    assert(a.st_dev == b.st_dev && a.st_ino == b.st_ino);
    assert(unlink(link) == 0);
    assert(symlink(other, link) == 0);
    assert(!setup_link_shared_gguf(side, err, sizeof err));
    assert(stat(other, &a) == 0 && stat(link, &b) == 0 && a.st_ino == b.st_ino);
    assert(unlink(link) == 0);
    assert(symlink("../missing-model-store", link) == 0);
    assert(!setup_link_shared_gguf(side, err, sizeof err));
    assert(lstat(link, &a) == 0 && S_ISLNK(a.st_mode));
    assert(unlink(link) == 0);
    assert(unlink(marker) == 0);
    assert(rmdir(shared) == 0 && rmdir(primary) == 0 && rmdir(side) == 0 && rmdir(other) == 0);
    {   /* DStudio 1017: <checkout>/gguf is a symlink to the stock weights folder. The stock path is injected
         * (production derives it from DSTUDIO_SHARED_GGUF_SUBPATH under $HOME on macOS). */
        char root[1024], stock[1100], fork[1100], fgguf[1200], side2[1100], sgguf[1200], f1[1300], tmp[1300], buf[1200], e2[512] = "";
        snprintf(root, sizeof root, "%s/shared-gguf", temp);
        snprintf(stock, sizeof stock, "%s/stock/ds4/gguf", root);
        snprintf(fork, sizeof fork, "%s/fork/ds4", root);
        snprintf(fgguf, sizeof fgguf, "%s/gguf", fork);
        snprintf(side2, sizeof side2, "%s/fork/ds4-qwen38", root);
        snprintf(sgguf, sizeof sgguf, "%s/gguf", side2);
        snprintf(f1, sizeof f1, "%s/DeepSeek-V4.1-Flash-1017-Q2.gguf", stock);
        snprintf(tmp, sizeof tmp, "%s/stock/ds4", root);
        assert(mkdir(root, 0755) == 0);
        { char d[1200]; snprintf(d, sizeof d, "%s/stock", root); assert(mkdir(d, 0755) == 0);
          snprintf(d, sizeof d, "%s/fork", root); assert(mkdir(d, 0755) == 0); }
        assert(mkdir(fork, 0755) == 0 && mkdir(side2, 0755) == 0);
        struct stat a, b;
        /* No stock folder (fresh machine, Linux, Windows): caller keeps the plain mkdir behaviour. */
        assert(setup_shared_gguf_link_at(fork, stock, 1, e2, sizeof e2) == SHARED_GGUF_NO_STOCK);
        assert(lstat(fgguf, &a) != 0);
        assert(setup_shared_gguf_link_at(fork, "relative/stock", 1, e2, sizeof e2) == SHARED_GGUF_NO_STOCK);
        assert(mkdir(tmp, 0755) == 0 && mkdir(stock, 0755) == 0);
        assert(jsonl_write_file(f1, "weights", 7));
        /* Startup repair never creates a missing folder; install does. */
        assert(setup_shared_gguf_link_at(fork, stock, 0, e2, sizeof e2) == SHARED_GGUF_KEPT && lstat(fgguf, &a) != 0);
        assert(setup_shared_gguf_link_at(fork, stock, 1, e2, sizeof e2) == SHARED_GGUF_LINKED);
        assert(lstat(fgguf, &a) == 0 && S_ISLNK(a.st_mode));
        { ssize_t n = readlink(fgguf, buf, sizeof buf - 1); assert(n > 0); buf[n] = '\0'; assert(!strcmp(buf, stock) && buf[0] == '/'); }
        assert(setup_shared_gguf_link_at(fork, stock, 1, e2, sizeof e2) == SHARED_GGUF_KEPT); /* reinstall/upgrade */
        /* The fork's own symlink is not "local data" for the installer's refusal check. */
        { int empty = 1; assert(setup_dir_empty(fork, &empty) && !empty); }
        assert(setup_dir_only_shared_gguf_link(fork, stock));
        assert(!setup_dir_only_shared_gguf_link(fork, "/elsewhere"));
        { char x[1300]; snprintf(x, sizeof x, "%s/Makefile", fork); assert(jsonl_write_file(x, "x", 1));
          assert(!setup_dir_only_shared_gguf_link(fork, stock)); assert(unlink(x) == 0); }
        /* Catalog-style lstat on files, and the Hugging Face partial-download scan, work through the link. */
        { char via[1400]; snprintf(via, sizeof via, "%s/DeepSeek-V4.1-Flash-1017-Q2.gguf", fgguf);
          assert(lstat(via, &a) == 0 && S_ISREG(a.st_mode) && a.st_size == 7); }
        { char c[1300], d[1500]; snprintf(c, sizeof c, "%s/.cache", stock); assert(mkdir(c, 0755) == 0);
          snprintf(c, sizeof c, "%s/.cache/huggingface", stock); assert(mkdir(c, 0755) == 0);
          snprintf(c, sizeof c, "%s/.cache/huggingface/download", stock); assert(mkdir(c, 0755) == 0);
          snprintf(d, sizeof d, "%s/gguf/.cache/huggingface/download/x.incomplete", fork); assert(jsonl_write_file(d, "p", 1));
          snprintf(d, sizeof d, "%s/x.incomplete", c); assert(lstat(d, &a) == 0 && S_ISREG(a.st_mode));
          assert(unlink(d) == 0);
          snprintf(d, sizeof d, "%s/.cache/huggingface/download", stock); assert(rmdir(d) == 0);
          snprintf(d, sizeof d, "%s/.cache/huggingface", stock); assert(rmdir(d) == 0);
          snprintf(d, sizeof d, "%s/.cache", stock); assert(rmdir(d) == 0); }
        /* Side engines still get their relative ../ds4/gguf link, resolved through the symlink chain. */
        { char saved[DSTUDIO_PATH_MAX]; char web[1200]; cstr_copy(saved, sizeof saved, g_web_dir);
          snprintf(web, sizeof web, "%s/fork", root); cstr_copy(g_web_dir, sizeof g_web_dir, web);
          assert(setup_link_shared_gguf(side2, e2, sizeof e2));
          assert(setup_link_shared_gguf(side2, e2, sizeof e2));
          char via[1400]; snprintf(via, sizeof via, "%s/DeepSeek-V4.1-Flash-1017-Q2.gguf", sgguf);
          assert(stat(via, &a) == 0 && S_ISREG(a.st_mode));
          assert(stat(sgguf, &a) == 0 && stat(stock, &b) == 0 && a.st_ino == b.st_ino && a.st_dev == b.st_dev);
          cstr_copy(g_web_dir, sizeof g_web_dir, saved);
          assert(unlink(sgguf) == 0); }
        /* Startup repair: an EMPTY real folder becomes the link; nothing is removed but that empty folder. */
        assert(unlink(fgguf) == 0 && mkdir(fgguf, 0755) == 0);
        assert(setup_shared_gguf_link_at(fork, stock, 0, e2, sizeof e2) == SHARED_GGUF_REPLACED);
        assert(lstat(fgguf, &a) == 0 && S_ISLNK(a.st_mode) && access(f1, R_OK) == 0);
        /* A real folder with any content (even a dotfile) is never touched; files survive. */
        { char own[1300]; assert(unlink(fgguf) == 0 && mkdir(fgguf, 0755) == 0);
          snprintf(own, sizeof own, "%s/.keep", fgguf); assert(jsonl_write_file(own, "k", 1));
          assert(setup_shared_gguf_link_at(fork, stock, 1, e2, sizeof e2) == SHARED_GGUF_KEPT);
          assert(lstat(fgguf, &a) == 0 && S_ISDIR(a.st_mode) && access(own, R_OK) == 0);
          assert(unlink(own) == 0 && rmdir(fgguf) == 0); }
        /* A foreign symlink and a plain file are left exactly as they are / reported. */
        assert(symlink("/nonexistent-model-store", fgguf) == 0);
        assert(setup_shared_gguf_link_at(fork, stock, 1, e2, sizeof e2) == SHARED_GGUF_KEPT);
        { ssize_t n = readlink(fgguf, buf, sizeof buf - 1); assert(n > 0); buf[n] = '\0'; assert(!strcmp(buf, "/nonexistent-model-store")); }
        assert(unlink(fgguf) == 0);
        assert(jsonl_write_file(fgguf, "f", 1));
        assert(setup_shared_gguf_link_at(fork, stock, 1, e2, sizeof e2) == SHARED_GGUF_ERROR && e2[0]);
        assert(unlink(fgguf) == 0);
        /* A checkout whose gguf IS the stock folder (fork pointed at the stock install) is left alone. */
        assert(setup_shared_gguf_link_at(tmp, stock, 1, e2, sizeof e2) == SHARED_GGUF_KEPT);
        assert(lstat(stock, &a) == 0 && S_ISDIR(a.st_mode) && !S_ISLNK(a.st_mode) && access(f1, R_OK) == 0);
        /* Onboarding (api_setup_ds4) and --install-engine share setup_prepare_checkout_gguf: with a stock
         * folder the result is the symlink (never a real folder that would later become unshareable). */
        assert(setup_prepare_checkout_gguf(fork, stock, e2, sizeof e2));
        assert(lstat(fgguf, &a) == 0 && S_ISLNK(a.st_mode));
        { ssize_t n = readlink(fgguf, buf, sizeof buf - 1); assert(n > 0); buf[n] = '\0'; assert(!strcmp(buf, stock)); }
        assert(setup_prepare_checkout_gguf(fork, stock, e2, sizeof e2));              /* idempotent */
        assert(unlink(fgguf) == 0);
        assert(setup_prepare_checkout_gguf(fork, NULL, e2, sizeof e2));               /* no stock folder: plain mkdir */
        assert(lstat(fgguf, &a) == 0 && S_ISDIR(a.st_mode) && !S_ISLNK(a.st_mode));
        assert(rmdir(fgguf) == 0);
        /* The stock support folder is off limits as an engine checkout (H2). HOME is pointed at a temp tree. */
        {   char home[1200], sup[1300], stk[1400], mine[1400], sl[1400], inside[1500], old_home[1200] = "";
            const char *oh = getenv("HOME"); if (oh) cstr_copy(old_home, sizeof old_home, oh);
            snprintf(home, sizeof home, "%s/home", root); snprintf(sup, sizeof sup, "%s/Library", home);
            assert(mkdir(home, 0755) == 0 && mkdir(sup, 0755) == 0);
            snprintf(sup, sizeof sup, "%s/Library/Application Support", home); assert(mkdir(sup, 0755) == 0);
            snprintf(stk, sizeof stk, "%s/DStudio", sup); snprintf(mine, sizeof mine, "%s/DStudio 1017", sup);
            snprintf(sl, sizeof sl, "%s/link-to-stock", home);
            snprintf(inside, sizeof inside, "%s/ds4", stk);
            assert(mkdir(stk, 0755) == 0 && mkdir(mine, 0755) == 0 && mkdir(inside, 0755) == 0);
            assert(symlink(stk, sl) == 0);
            assert(setenv("HOME", home, 1) == 0);
            assert(dstudio_checkout_is_stock(stk));
            assert(dstudio_checkout_is_stock(inside));
            { char p[1600]; snprintf(p, sizeof p, "%s/", stk); assert(dstudio_checkout_is_stock(p)); }
            { char p[1600]; snprintf(p, sizeof p, "%s/ds4/../ds4/", stk); assert(dstudio_checkout_is_stock(p)); }
            { char p[1600]; snprintf(p, sizeof p, "%s/not-yet/ds4", stk); assert(dstudio_checkout_is_stock(p)); }   /* does not exist yet */
            { char p[1600]; snprintf(p, sizeof p, "%s/ds4", sl); assert(dstudio_checkout_is_stock(p)); }          /* via a symlink */
            assert(dstudio_checkout_is_stock(sl));
            assert(!dstudio_checkout_is_stock(mine));                                   /* "DStudio 1017" is not under "DStudio" */
            { char p[1600]; snprintf(p, sizeof p, "%s/ds4", mine); assert(!dstudio_checkout_is_stock(p)); }
            { char p[1600]; snprintf(p, sizeof p, "%s/DStudioX/ds4", sup); assert(!dstudio_checkout_is_stock(p)); }
            assert(!dstudio_checkout_is_stock(sup) && !dstudio_checkout_is_stock(home) && !dstudio_checkout_is_stock(""));
            assert(dstudio_path_is_at_or_under("/a/DStudio/x", "/a/DStudio") && dstudio_path_is_at_or_under("/a/DStudio", "/a/DStudio/"));
            assert(!dstudio_path_is_at_or_under("/a/DStudio 1017", "/a/DStudio") && !dstudio_path_is_at_or_under("/a/DStudioX", "/a/DStudio"));
            if (old_home[0]) setenv("HOME", old_home, 1); else unsetenv("HOME");
            assert(unlink(sl) == 0 && rmdir(inside) == 0 && rmdir(stk) == 0 && rmdir(mine) == 0 && rmdir(sup) == 0);
            snprintf(sup, sizeof sup, "%s/Library", home); assert(rmdir(sup) == 0 && rmdir(home) == 0);
        }
        /* The fork's KV cache root is its own (api_wipe empties it); DSTUDIO_KV_DIR still overrides it. */
        {   char kv[1700], kd[1800]; const char *okv = getenv("DSTUDIO_KV_DIR"); char saved_kv[1700] = "";
            if (okv) cstr_copy(saved_kv, sizeof saved_kv, okv);
            unsetenv("DSTUDIO_KV_DIR");
            kv_root(kv, sizeof kv);
            assert(strstr(kv, "/.local/share/flashcards/ds4-kv-1017") && !strstr(kv, "flashcards/ds4-kv/") &&
                   strcmp(kv + strlen(kv) - 12, "/ds4-kv-1017") == 0);
            kv_dir_for_model("gguf/DeepSeek-V4.1-Flash-Q2.gguf", kd, sizeof kd);
            assert(!strncmp(kd, kv, strlen(kv)) && kd[strlen(kv)] == '/');
            setenv("DSTUDIO_KV_DIR", "/tmp/kv-override", 1);
            kv_root(kv, sizeof kv); assert(!strcmp(kv, "/tmp/kv-override"));
            if (okv) setenv("DSTUDIO_KV_DIR", saved_kv, 1); else unsetenv("DSTUDIO_KV_DIR");
        }
        /* M3: a lock held by another DStudio's engine (different --port) is reported, not waited on. */
        assert(ds4_args_foreign_port("ds4-server --host 127.0.0.1 --port 28000 -m x.gguf", 28017));
        assert(!ds4_args_foreign_port("ds4-server --host 127.0.0.1 --port 28017 -m x.gguf", 28017));
        assert(ds4_args_foreign_port("ds4-server --port=28000", 28017));
        assert(!ds4_args_foreign_port("ds4-agent-jsonl --model x", 28017));           /* unknown owner: keep waiting */
        assert(!ds4_args_foreign_port("ds4-server --portable 28000", 28017) && !ds4_args_foreign_port("", 28017) && !ds4_args_foreign_port(NULL, 28017));
        m3_lock_test(root);
        /* Cleanup. */
        { char d[1300]; assert(unlink(f1) == 0 && rmdir(stock) == 0 && rmdir(tmp) == 0);
          snprintf(d, sizeof d, "%s/stock", root); assert(rmdir(d) == 0);
          assert(rmdir(fork) == 0 && rmdir(side2) == 0);
          snprintf(d, sizeof d, "%s/fork", root); assert(rmdir(d) == 0); assert(rmdir(root) == 0); }
    }
    assert(rmdir(temp) == 0);
#endif
    puts("engine_setup_unit: model/component selection, prompt catalogs, residency, existing-data preservation and shared-store identity passed (no model)");
    return 0;
}
