#include "jsonsl.h"
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <assert.h>

static int WantFail = 0;
static jsonsl_error_t WantError = 0;

void fmt_level(const char *buf, size_t nbuf, int levels)
{
    const char *c = buf;
    int ii;
    for (ii = 0; ii < levels; ii++) {
        putchar('\t');
    }

    while (nbuf && *c) {
        putchar(*c);
        if (*c == '\n') {
            for (ii = 0; ii < levels; ii++) {
                putchar(' ');
            }
        }
        c++;
        nbuf--;
    }
    putchar('\n');
}

void state_callback(jsonsl_t jsn,
                   jsonsl_action_t action,
                   struct jsonsl_state_st *state,
                   const char *buf)
{
    /* We are called here with the jsn object, the state (PUSH or POP),
     * the 'state' object, which contains information about the level of
     * nesting we are descending into/ascending from, and a pointer to the
     * start position of the detectin of this nesting
     */
    /*
    printf("@%-5lu L%d %c%s\n",
            jsn->pos,
            state->level,
            action,
            jsonsl_strtype(state->type));
            */

    if (action == JSONSL_ACTION_POP) {
        size_t state_len = state->pos_cur - state->pos_begin;

        const char *buf_begin = buf - state_len;
    }
}

int error_callback(jsonsl_t jsn,
                   jsonsl_error_t err,
                   struct jsonsl_state_st *state,
                   char *errat)
{
    /* Error callback. In theory, this can return a true value
     * and maybe 'correct' and seek ahead of the buffer, and try to
     * do some correction.
     */
    if (WantFail) {
        printf("Got error %s (PASS)\n", jsonsl_strerror(err));
        WantError = err;
        return 0;
    }

    fprintf(stderr, "Got parse error at '%c', pos %lu\n", *errat, jsn->pos);
    fprintf(stderr, "Error is %s\n", jsonsl_strerror(err));
    fprintf(stderr, "Remaining text: %s\n", errat);
    abort();
    return 0;
}


void parse_single_file(const char *path)
{
    char *buf, *bufp;
    struct stat sb;
    int status;
    size_t nread = 0;
    int fd;
    jsonsl_t jsn;
    WantError = 0;

    /* open our file */
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror(path);
        return;
    }

    status = fstat(fd, &sb);
    if (S_ISDIR(sb.st_mode)) {
        fprintf(stderr, "\tS_ISDIR..\n");
        close(fd);
        return;
    }
    assert(status == 0);
    assert(sb.st_size);
    assert(sb.st_size < 0x1000000);
    buf = malloc(sb.st_size);
    bufp = buf;


    /* Declare that we will support up to 512 nesting levels.
     * Each level of nesting requires about ~40 bytes (allocated at initialization)
     * to maintain state information.
     */
    jsn = jsonsl_new(0x2000);

    /* Set up our error callbacks (to be called when an error occurs)
     * and a nest callback (when a level changes in 'nesting')
     */
    jsn->error_callback = error_callback;
    jsn->action_callback = state_callback;

    /* Declare that we're intertested in receiving callbacks about
     * json 'Object' and 'List' types.
     */
    jsonsl_enable_all_callbacks(jsn);
    /* read into the buffer */

    /**
     * To avoid recomputing offsets and relative positioning,
     * we will maintain the buffer, but this is not strictly required.
     */

    while ( (nread = read(fd, bufp, 4096)) > 0) {
        jsonsl_feed(jsn, bufp, nread);
        bufp += nread;
    }
    if (WantFail && WantError == 0) {
        fprintf(stderr, "Expected error but didn't find any!\n");
        abort();
    }
    jsonsl_destroy(jsn);
    free(buf);
}

int main(int argc, char **argv)
{
    int ii;
    if (getenv("JSONSL_QUIET_TESTS")) {
        freopen("/dev/null", "w", stdout);
    }
    if (getenv("JSONSL_FAIL_TESTS")) {
        printf("Want Fail..\n");
        WantFail = 1;
    }
    if (argc < 2) {
        fprintf(stderr, "Usage: %s FILES..\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    for (ii = 1; ii < argc && argv[ii]; ii++) {
        fprintf(stderr, "==== %-40s ====\n", argv[ii]);
        parse_single_file(argv[ii]);
    }

    return 0;
}
