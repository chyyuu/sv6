/**
 * Nanobenchmark: META
 *   MU. PROCESS = {overwrite a non-overlapping region of /test/test.file}
 *       - TEST: concurrent inode.mtime  update
 */	      
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include "fxmark.h"
#include "util.h"

#define PRIVATE_REGION_SIZE (1024 * 1024 * 8)
#define PRIVATE_REGION_PAGE_NUM (PRIVATE_REGION_SIZE/PAGE_SIZE)

static void set_shared_test_root(struct worker *worker, char *test_root)
{
	struct fx_opt *fx_opt = fx_opt_worker(worker);
	sprintf(test_root, "%s", fx_opt->root);
}

static void set_test_file(struct worker *worker, char *test_root)
{
	struct fx_opt *fx_opt = fx_opt_worker(worker);
	sprintf(test_root, "%s/n_mtime_upt.dat", fx_opt->root);
}

static int pre_work(struct worker *worker)
{
	struct bench *bench = worker->bench;
	char path[PATH_MAX];
	int fd, max_id = -1, rc;
	int i, j;

	/* a leader takes over all pre_work() */
	if (worker->id != 0)
		return 0;

	/* find the largest worker id */
	for (i = 0; i < bench->ncpu; ++i) {
		struct worker *w = &bench->workers[i];
		if (w->id > max_id)
			max_id = w->id;
	}

	/* create a test file */
	set_shared_test_root(worker, path);
	rc = mkdir_p(path);
	if (rc) return rc;

	set_test_file(worker, path);
	if ((fd = open(path, O_CREAT | O_RDWR, S_IRWXU)) == -1)
		goto err_out;

	for (i = 0; i <= max_id; ++i) {
		char page[PAGE_SIZE];
		for (j = 0; j < PRIVATE_REGION_PAGE_NUM; ++j) {
			if (write(fd, page, sizeof(page)) == -1)
				goto err_out;
		}
	}
	fsync(fd);
	close(fd);
out:
	return rc;
err_out:
	rc = errno;
	goto out;
}

static int main_work(struct worker *worker)
{
	struct bench *bench = worker->bench;
	char path[PATH_MAX];
	char page[PAGE_SIZE];
	int fd, rc = 0;
	off_t pos;
	uint64_t iter = 0;

	set_test_file(worker, path);
	if ((fd = open(path, O_CREAT | O_RDWR, S_IRWXU)) == -1)
		goto err_out;
	
	pos = PRIVATE_REGION_SIZE * worker->id;
	for (iter = 0; !bench->stop; ++iter) {
	        if (pwrite(fd, page, sizeof(page), pos) == -1)
			goto err_out;
	}
	close(fd);
out:
	worker->works = (double)iter;
	return rc;
err_out:
	bench->stop = 1;
	rc = errno;
	goto out;
}

struct bench_operations n_mtime_upt_ops = {
	.pre_work  = pre_work, 
	.main_work = main_work,
};