#include "csv-split.h"
#include "csv-buf.h"
#include "csv.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

/**
 * Trigger a command when a job is done
 */
static void exec_trigger(const char *trigger_cmd, const char *job_file, unsigned long row_count) {
	// The full trigger command we'll execute
    char row_count_str[40];

    // Get the row count in the form of a string
    snprintf(row_count_str, sizeof(row_count_str), "%lu", row_count);

    // Payload file
    if(setenv(ENV_PAYLOAD_VAR, job_file, 1) != 0) {
        fprintf(stderr, "Couldn't set environment variable: %d\n", errno);
    }
    
    // Row count
    if(setenv(ENV_ROWCOUNT_VAR, row_count_str, 1) != 0) {
        fprintf(stderr, "Couldn't set environment variable: %d\n", errno);
    }

	// Trigger our command
    if(system(trigger_cmd) != 0) {
        fprintf(stderr, "Error:  Couldn't execute system command \"%s\"\n", trigger_cmd);
    }
}

/**
 * Our IO worker thread, where we wait on our IO queue (files to be written)
 * and write them as we get them.  Once the queue is flagged done, we'll finish
 */
void *io_worker(void *arg) {
    // Grab our queue
    fqueue *queue = (fqueue*)arg;

    // Queue item
    struct q_flush_item *item;

    // Block until we have work, or we're done
    while(!fq_get(queue, (void**)&item)) {
        FILE *fp = fopen(item->out_file, "w");
        if(!fp) {
            fprintf(stderr, "Error: Unable to open output file '%s'\n", item->out_file);
            exit(EXIT_FAILURE);
        }

        if(fwrite(item->str, 1, item->len, fp) != item->len) {
            fprintf(stderr, "Error: Unable to write all data to file '%s'\n", item->out_file);
            exit(EXIT_FAILURE);
        }

        // Close our file
        fclose(fp);

        // Execute our trigger if one is set
        if(item->trigger_cmd) {
            exec_trigger(item->trigger_cmd, item->out_file, item->row_count);
        }

        // Now free our memory as this was a copy
        free(item->str);
        free(item);
    }

    return NULL;
}

// We're ready to split this file off, so package up information for our queue, 
// add it, and send it to one of our IO threads
void flush_file(struct csv_context *ctx, unsigned int use_ovr) {
    // Create a queue item
    struct q_flush_item *q_item = malloc(sizeof(struct q_flush_item));

    // If we've got an overflow position and we're supposed to use it, do so
    size_t flush_len = use_ovr && ctx->opos ? ctx->opos : CBUF_POS(ctx->csv_buf);

    // Copy in our filename
    sprintf(q_item->out_file, "%s%s.%d", ctx->out_path, ctx->in_prefix, ++ctx->on_file);

    // If we've got a non empty trigger command, set it in our item
    if(*ctx->trigger_cmd) {
    	q_item->trigger_cmd = (const char *)ctx->trigger_cmd;
    } else {
    	q_item->trigger_cmd = NULL;
    }

    // Store the number of rows we're going to write
    q_item->row_count = ctx->row;

    // Make a copy of our buffer, and store our length
    q_item->str = cbuf_duplen(ctx->csv_buf, &flush_len);
    q_item->len = flush_len;

    // Chop our output buffer
    CBUF_SETPOS(ctx->csv_buf, 0);

    // Reset our row counter and overflow
    ctx->row = ctx->opos = 0;

    // Add to our blocking/limited queue
    fq_add(&ctx->io_queue, (void*)q_item);
}

/**
 * Column callback
 */
static inline void cb_col(void *s, size_t len, void *data) {
    struct csv_context *ctx = (struct csv_context *)data;
    size_t cnt;

    // Put a comma if we should
    if(ctx->put_comma) {
        ctx->csv_buf = cbuf_putc(ctx->csv_buf, ',');
    }
    ctx->put_comma = 1;

    // If we are keeping same columns together see if we're on one
    if(ctx->gcol > -1 && ctx->col == ctx->gcol) {
        // If we have a last column value and we're in overflow, check
		// the new row's value against the last one
        if(ctx->gcol_buf && ctx->opos && memcmp(ctx->gcol_buf, s, len) != 0) {
            // Flush the data we have!
            flush_file(ctx, 1);
        } else if(!ctx->gcol_buf) {
            //ctx->gcol_buf = (char*)malloc(100);
            ctx->gcol_buf = cbuf_init(len);
        }

        // Update our last group column value
        ctx->gcol_buf = cbuf_setlen(ctx->gcol_buf, (const char*)s, len);
    }

    // Make sure we can write all the data
    while((cnt = csv_write(CBUF_PTR(ctx->csv_buf), CBUF_REM(ctx->csv_buf), s, len)) > CBUF_REM(ctx->csv_buf)) {
        // We didn't have room, reallocate
        ctx->csv_buf = cbuf_double(ctx->csv_buf);
    }

    // Increment where we are in our buffer
    CBUF_POS(ctx->csv_buf)+=cnt;

    // Increment our column
    ctx->col++;
}

/**
 * Row parsing callback
 */
void cb_row(int c, void *data) {
    // Type cast to our context structure
    struct csv_context *ctx = (struct csv_context*)data;

    // Put a newline
    ctx->csv_buf = cbuf_putc(ctx->csv_buf, '\n');

    // Have we hit our row count limit
    if(ctx->row++ == ctx->max_rows - 1) {
        // If we're keeping a given column together mark
        // our overflow position.  Otherwise we flip
        if(ctx->gcol >= 0) {
            ctx->opos = CBUF_POS(ctx->csv_buf);
        } else {
            flush_file(ctx, 0);
        }
    }

    // Back on column zero
    ctx->col=0;

    // We don't need a comma for the next column
    ctx->put_comma = 0;
}

/**
 * Usage function
 */
void print_usage(char *exec) {
    printf("Usage:  %s [options] FILE\n", exec);
}

/**
 * Parse arguments
 */
int parse_args(struct csv_context *ctx, int argc, char **argv) {
    int opt, opt_idx, intval;
    char *ptr;

    // While we've got arguments to parse
    while((opt = getopt_long(argc, argv, "g:n:", g_long_opts, &opt_idx)) != -1) {
        switch(opt) {
            case 't':
                strncpy(ctx->trigger_cmd, optarg, sizeof(ctx->trigger_cmd));
        		break;
            case 'g':
                ctx->gcol = atoi(optarg);
                break;
            case 'n':
                intval = atoi(optarg);
                if(!intval || intval < 0) {
                    fprintf(stderr, "Number of lines per file must be a positive integer!\n");
                    exit(EXIT_FAILURE);
                }
                ctx->max_rows = intval;
                break;
            case 'h':
            case '?':
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            case 0:
                // Parse from STDIN
                if(!strcmp("stdin", g_long_opts[opt_idx].name)) {
                    ctx->from_stdin = 1;
                }
                break;
        }
    }

    // Get the filename we're reading or the prefix to use if reading from STDIN
    if(!argv[optind] || !*argv[optind]) {
        fprintf(stderr, "Must specify a file to process or a prefix to use if reading from STDIN!\n");
        exit(EXIT_FAILURE);
    }

    // Copy in our input file, move to next argument
    strcpy(ctx->in_file, argv[optind++]);

    // If we find that there are path parts in the file, keep track of just the basename
    if((ptr = strrchr(ctx->in_file, '/'))) {
        ctx->in_prefix = ptr+1;
    } else {
        ctx->in_prefix = ctx->in_file;
    }

    // Set our output path if it's not set
    if(argv[optind] && *argv[optind]) {
    	// Copy in our output path
    	strcpy(ctx->out_path, argv[optind]);

    	// Terminate with a '/' if it's not already terminated
    	if(ctx->out_path[strlen(ctx->out_path)]-1 != '/') {
    		strcat(ctx->out_path, "/");
    	}
    }

    // Success
    return 0;
}

/**
 * Spin up our threads
 */
void spool_threads(struct csv_context *ctx) {
    int i;

    // Iterate up to our thread count
    for(i=0;i<BG_FLUSH_THREADS;i++) {
        // We have to fail if our background threads fail to initialize
        if(pthread_create(&ctx->io_threads[i], NULL, io_worker, (void*)&ctx->io_queue) != 0) {
            fprintf(stderr, "Couldn't start background IO threads!\n");
            exit(EXIT_FAILURE);
        }
    }
}

/**
 * Wait for threads to exit
 */
void join_threads(struct csv_context *ctx) {
    int i=0;

    // Iterate, joining on threads
    for(i=0;i<BG_FLUSH_THREADS;i++) {
        pthread_join(ctx->io_threads[i], NULL);
    }
}

/**
 * Initialize context pointers
 */
void context_init(struct csv_context *ctx) {
    // Init our passthrough buffer
    ctx->csv_buf = cbuf_init(BUFFER_SIZE);

    // Initialize our blocking queue
    fq_init(&ctx->io_queue, BG_QUEUE_MAX);

    // Initialize our CSV parser
    if(csv_init(&ctx->parser, 0) != 0) {
        fprintf(stderr, "Couldn't initialize CSV parser!\n");
        exit(EXIT_FAILURE);
    }

    // Set our csv block realloc size
    csv_set_blk_size(&ctx->parser, CSV_BLK_SIZE);

    // Default to no group column
    ctx->gcol = -1;
}

/**
 * Free dynamically allocated stuff in our context
 */
void context_free(struct csv_context *ctx) {
    // Free our pass through buffer
    cbuf_free(ctx->csv_buf);

    // Free group column buffer
    if(ctx->gcol_buf) {
        cbuf_free(ctx->gcol_buf);
    }

    // Free memory stored in our IO queue
    fq_free(&ctx->io_queue);

    // Free our CSV parser
    csv_free(&ctx->parser);
}

/**
 * Main processing loop
 */
void process_csv(struct csv_context *ctx) {
    FILE *fp;
    char buf[READ_BUF_SIZE];
    size_t bytes_read;

    // Read from a file or STDIN
    if(!ctx->from_stdin) {
        // Attempt to open our file
        if(!(fp = fopen(ctx->in_file, "r"))) {
            fprintf(stderr, "Couldn't open input file '%s'\n", ctx->in_file);
            exit(EXIT_FAILURE);
        }
    } else {
        // Just read from STDIN
        fp = stdin;
    }

    // Process the file
    while((bytes_read = fread(buf, 1, sizeof(buf), fp)) > 0) {
        // Parse our CSV
        if(csv_parse(&ctx->parser, buf, bytes_read, cb_col, cb_row, (void*)ctx) != bytes_read) {
            fprintf(stderr, "Error while parsing file!\n");
            exit(EXIT_FAILURE);
        }
    }

    // Flush any additional data we have
    if(CBUF_POS(ctx->csv_buf)) flush_file(ctx, 0);

    // Close our file
    fclose(fp);
}

/**
 * Main entry point for processing arguments and starting the split process
 */
int main(int argc, char **argv) {
	// Create our context object, null it out
	struct csv_context ctx;
    memset(&ctx, 0, sizeof(struct csv_context));

    // Initialize buffers
    context_init(&ctx);

    // Attempt to parse our arguments
    parse_args(&ctx, argc, argv);

    // Initialize our IO threads
    spool_threads(&ctx);

    // Process our input
    process_csv(&ctx);

    // Signal that we're done inside our queue
    fq_fin(&ctx.io_queue);

    // Join our threads
    join_threads(&ctx);

    // One last trigger showing we're done
    exec_trigger(ctx.trigger_cmd, "", 0);

    // Free memory from our context
    context_free(&ctx);

    // Success
    return 0;
}
