// 동시 접근 시 발생하는 문제를 해결하기 위한 세마포어 사용법  뒤에 // 표시한 부분을 3-8 rw.c 파일에서 복붙해옴


// shmput.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>	//
#include <sys/time.h>

#include "sem.h"	//	cp명령으로 3-8에서 sem.h 파일을 3-10으로 가져와야 한다
#include "shm.h"

pid_t pid;

int main(int argc, char **argv)
{
	int ret;
	int id_shm;
	struct shm_buf *shmbuf;
	int i;
	struct timeval tv;
	time_t start_time;
	int first = 1;
	unsigned long cs;
	int id_sem;				// 3줄
	struct sembuf sops_dec[1] = { {0, -1, SEM_UNDO} };
	struct sembuf sops_inc[1] = { {0, 1, SEM_UNDO} };

	if (argc != 1) {
		printf("usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	printf("[%d] running %s\n", pid = getpid(), argv[0]);

	id_sem = semget((key_t)KEY_SEM, 1, 0666 | IPC_CREAT);		//	
	if (id_sem == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	id_shm = shmget((key_t)KEY_SHM, sizeof(struct shm_buf), 0666 | IPC_CREAT);
	if (id_shm == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	shmbuf = shmat(id_shm, (void *)0, 0);
	if (shmbuf == (struct shm_buf *) - 1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	for (;;) {
		if (gettimeofday(&tv, NULL) == -1) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}

		ret = semop(id_sem, sops_dec, 1);		// 공통 접근할 수 있는 부분 앞에서 semop
		if (ret == -1) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}

		if (first) {
			first = 0;
			start_time = tv.tv_sec;
		}
		else {
			if (tv.tv_sec - start_time > TIMEOUT) {
				shmbuf->status = STATUS_INVALID;
				break;		// <-
			}
		}

		sprintf(shmbuf->buf, "%lu.%06lu", tv.tv_sec, tv.tv_usec);
		shmbuf->status = STATUS_VALID;
		for (i = 0, cs = 0; i < sizeof(struct shm_buf) - sizeof(unsigned long); i++) {
			cs += ((unsigned char *)shmbuf)[i];
		}
		shmbuf->cs = cs;


		ret = semop(id_sem, sops_inc, 1);		// 나가는 부분에서 semop
		if (ret == -1) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}

		//usleep(100000);
	}

	ret = semop(id_sem, sops_inc, 1);		// break로 빠져나온 경우 semop이 안될 것이다 빼먹지 않고 semop해준다
	if (ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	ret = shmdt(shmbuf);
	if (ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	printf("[%d] terminted\n", pid);

	return EXIT_SUCCESS;
}

// shmget.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>		//

#include "shm.h"
#include "sem.h"		//

pid_t pid;

int main(int argc, char **argv)
{
	int ret, i;
	int id_shm;
	struct shm_buf *shmbuf;
	unsigned long cs = 0;
	int id_sem;					//
	struct sembuf sops_dec[1] = { {0, -1, SEM_UNDO} };
	struct sembuf sops_inc[1] = { {0, 1, SEM_UNDO} };

	if (argc != 1) {
		printf("usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}
	printf("[%d] running %s\n", pid = getpid(), argv[0]);

	id_sem = semget((key_t)KEY_SEM, 1, 0666 | IPC_CREAT);			//
	if (id_sem == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	id_shm = shmget((key_t)KEY_SHM, sizeof(struct shm_buf), 0666 | IPC_CREAT);
	if (id_shm == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	shmbuf = shmat(id_shm, (void *)0, 0);
	if (shmbuf == (struct shm_buf *) - 1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	for (;;) {
		ret = semop(id_sem, sops_dec, 1);			//
		if (ret == -1) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}

		if (shmbuf->status != STATUS_VALID) {
			break;
		}

		for (i = 0, cs = 0; i < sizeof(struct shm_buf) - sizeof(unsigned long); i++) {
			cs += ((unsigned char *)shmbuf)[i];
		}
		if (shmbuf->cs != cs) {
			printf("[%d] error: invalid checksum (checksum=%ld, calculated=%ld)\n", pid, shmbuf->cs, cs);
			break;
		}

		printf("[%d] time=%s, checksum=%lu\n", pid, shmbuf->buf, shmbuf->cs);

		ret = semop(id_sem, sops_inc, 1);			//
		if (ret == -1) {
			printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
			return EXIT_FAILURE;
		}

		usleep(1000000);
	}

	ret = semop(id_sem, sops_inc, 1);			//
	if (ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	ret = shmdt(shmbuf);
	if (ret == -1) {
		printf("[%d] error: %s (%d)\n", pid, strerror(errno), __LINE__);
		return EXIT_FAILURE;
	}

	printf("[%d] terminted\n", pid);

	return EXIT_SUCCESS;
}