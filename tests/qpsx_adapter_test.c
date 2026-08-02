#include <stdint.h>
#include <stdio.h>
#include <string.h>

int fs_opendir(const char *path);
int fs_readdir(int handle, void *buffer);
int fs_closedir(int handle);

int main(void)
{
	struct {
		struct {
			uint32_t d_ino;
			char d_name[256];
		} entry;
		uint8_t guard[64];
	} record;
	unsigned count = 0;
	int handle;
	int result;

	memset(&record, 0xa5, sizeof(record));
	handle = fs_opendir(".");
	if (handle < 0)
		return 1;
	do {
		result = fs_readdir(handle, &record.entry);
		if (result < 0 || ++count > 10000u)
			return 2;
		for (unsigned i = 0; i < sizeof(record.guard); i++)
			if (record.guard[i] != 0xa5)
				return 3;
	} while (result > 0);
	if (fs_closedir(handle) != 0)
		return 4;
	puts("QPSX directory adapter test passed");
	return 0;
}
