#include "nvme_passthru.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/nvme_ioctl.h>
#include <fcntl.h>
#include <cstring>
#include <cstdint>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <cstdio>
#include <inttypes.h>

using namespace std;

const unsigned int PAGE_SIZE = 4096;
const unsigned int MAX_BUFLEN = 16*1024*1024; /* Maximum transfer size (can be adjusted if needed) */
const unsigned int NSID = 1; /* NSID can be checked using 'sudo nvme list' */

int Embedded::Proj1::Open(const std::string &dev) {
    int err;
    err = open(dev.c_str(), O_RDONLY);
    if (err < 0)
        return -1;
    fd_ = err;

    struct stat nvme_stat;
    err = fstat(fd_, &nvme_stat);
    if (err < 0)
        return -1;
    if (!S_ISCHR(nvme_stat.st_mode) && !S_ISBLK(nvme_stat.st_mode))
        return -1;

    return 0;
}

// firmware 코드에서 dword[10], dword[11], dowrd[12] 를 필요
// call nvme_passthru
int Embedded::Proj1::ImageWrite(const std::vector<uint8_t> &buf) {
    if (buf.empty()) return -EINVAL;
    if (buf.size() > MAX_BUFLEN) {
        cerr << "[ERROR] Image size exceeds the maximum transfer size limit." << endl;
        return -EINVAL;
    }
    if (fd_ < 0) {
        cerr << "[ERROR] ImageWrite - fd_ < 0\n";
        return -EBADF;
    }

    /* ------------------------------------------------------------------
     * TODO: Implement this function.
     * 
     * Return 0 on success, or a negative error code on failure.
     * ------------------------------------------------------------------ */

    size_t num_blocks = (buf.size() + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t lba = 0;
    uint32_t cdw10 = 0;
    uint32_t cdw11 = 0;

    for (size_t i = 0; i < num_blocks; ++i) {
        uint32_t data_len = PAGE_SIZE;
        void* data_addr = (void*)(buf.data() + i * PAGE_SIZE);

        if (i == num_blocks - 1 && (buf.size() % PAGE_SIZE != 0)) { // 마지막 블럭인 경우
            data_len = buf.size() % PAGE_SIZE;
        }

        cdw10 = lba & 0xFFFFFFF;
        cdw11 = (lba >> 32) & 0xFFFFFFFF;
        
        int ret = nvme_passthru(NVME_CMD_WRITE, NSID, cdw10, cdw11, data_addr, data_len, NULL);

        if (ret < 0) {
            cerr << "[ERROR]" << strerror(errno) << endl;
            return ret;
        }

        lba += 1;
    }

    return 0; // placeholder
}

int Embedded::Proj1::ImageRead(std::vector<uint8_t> &buf, size_t size) {
    if (size == 0) return -EINVAL;
    if (size > MAX_BUFLEN) {
        cerr << "[ERROR] Requested read size exceeds the maximum transfer size limit." << endl;
        return -EINVAL;
    }

    /* ------------------------------------------------------------------
     * TODO: Implement this function.
     * 
     * Return 0 on success, or a negative error code on failure.
     * ------------------------------------------------------------------ */

    if (fd_ < 0) {
        cerr << "[ERROR] ImageRead - fd_ < 0\n";
        return -EBADF;
    }

    buf.resize(size);
    size_t num_blocks = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t lba = 0;
    uint32_t cdw10 = 0;
    uint32_t cdw11 = 0;

    for (size_t i = 0; i < num_blocks; ++i) {
        uint32_t data_len = PAGE_SIZE;
        void* data_addr = (void*)(buf.data() + i * PAGE_SIZE);

        if (i == num_blocks - 1 && size % PAGE_SIZE != 0) {
            data_len = size % PAGE_SIZE;
        }

        cdw10 = lba & 0xFFFFFFF;
        cdw11 = (lba >> 32) & 0xFFFFFFFF;

        int ret = nvme_passthru(NVME_CMD_READ, NSID, cdw10, cdw11, data_addr, data_len, NULL);
        
        if (ret < 0) {
            cerr << "[ERROR]" << strerror(errno) << endl;
            return ret;
        }

        lba += 1;
    }
    return 0; // placeholder
}

int Embedded::Proj1::Hello() {
    /* ------------------------------------------------------------------
     * TODO: Implement this function.
     * 
     * Return 0 on success, or a negative error code on failure.
     * ------------------------------------------------------------------ */

    if (fd_ < 0) {
        cerr << "[ERROR] Hello - fd_ < 0\n";
        return -EBADF;
    }

    uint32_t cdw10 = 0;
    uint32_t cdw11 = 0;

    int ret = nvme_passthru(NVME_CMD_HELLO, NSID, cdw10, cdw11, 0, 0, NULL);

    if (ret < 0) {
        cerr << "[ERROR] Hello command failed with error code: " << ret << endl;
        return ret;
    }

    cout << "Hello command sent successfully." << endl;

    return 0; // placeholder
}

int Embedded::Proj1::nvme_passthru(uint8_t opcode, uint32_t nsid, uint32_t cdw10, uint32_t cdw11, \
    void* data_addr, uint32_t data_len, uint32_t* res)
{
    /* ------------------------------------------------------------------
     * TODO: Implement this function.
     * This function should serve as the low-level interface for issuing
     * passthru NVMe commands. Make sure to include appropriate arguments
     * (e.g., opcode, namespace ID, command dwords, buffer pointer, length,
     * and result field) so that higher-level methods (ImageWrite, ImageRead,
     * Hello) can be implemented using this helper.
     *
     * Hint: refer to the Linux nvme_ioctl.h header and the struct nvme_passthru_cmd definition.
     * - Link: https://elixir.bootlin.com/linux/v5.15/source/include/uapi/linux/nvme_ioctl.h
     * ------------------------------------------------------------------ */
    if (fd_ < 0) {
        cerr << "[ERROR] passthru fd_ < 0\n";
        return -EBADF;
    }

    struct nvme_passthru_cmd cmd = {0,};
    cmd.opcode = opcode;
    cmd.nsid = NSID;
    cmd.cdw10 = cdw10;
    cmd.cdw11 = cdw11;
    cmd.data_len = data_len;
    cmd.addr = (uint64_t)data_addr;
    cmd.cdw12 = (data_len + PAGE_SIZE - 1) / PAGE_SIZE - 1;

    int ret = ioctl(fd_, NVME_IOCTL_IO_CMD, &cmd);
    
    if (ret < 0) {
        cerr << "[ERROR] ioctl failed with error code: " << ret << endl;
        return ret;
    }

    if (res) {
        *res = cmd.result;
    }

    return 0; // success
}
