#include "ff.h"
#include "diskio.h"

extern int      ide_read_sectors (uint32_t lba, uint32_t count, void *buf);
extern int      ide_write_sectors(uint32_t lba, uint32_t count, const void *buf);
extern uint32_t ide_sector_count (void);

DSTATUS disk_initialize(BYTE pdrv) { return (pdrv == 0) ? 0 : STA_NOINIT; }
DSTATUS disk_status    (BYTE pdrv) { return (pdrv == 0) ? 0 : STA_NOINIT; }

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || count == 0) return RES_PARERR;
    return (ide_read_sectors((uint32_t)sector, count, buff) == 0)
           ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || count == 0) return RES_PARERR;
    return (ide_write_sectors((uint32_t)sector, count, buff) == 0)
           ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0) return RES_PARERR;
    switch (cmd) {
        case CTRL_SYNC:        return RES_OK;
        case GET_SECTOR_COUNT: *(DWORD *)buff = ide_sector_count(); return RES_OK;
        case GET_SECTOR_SIZE:  *(WORD  *)buff = 512;                return RES_OK;
        case GET_BLOCK_SIZE:   *(DWORD *)buff = 1;                  return RES_OK;
    }
    return RES_PARERR;
}