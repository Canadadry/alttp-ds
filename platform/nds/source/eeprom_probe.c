#include <nds.h>
#include <calico/nds/env.h>
#include <string.h>

#include "eeprom_probe.h"

enum { kProbeAddr = 0x1FE0, kPage = 32 };

static void put16(u8 *p, u16 v)
{
	p[0] = v;
	p[1] = v >> 8;
}

static void put32(u8 *p, u32 v)
{
	put16(p, v);
	put16(p + 2, v >> 16);
}

static void spi_send(u8 v)
{
	REG_AUXSPIDATA = v;
	eepromWaitBusy();
}

static void write_page(u16 addr, const u8 *data)
{
	REG_AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD;
	spi_send(SPI_EEPROM_WREN);
	REG_AUXSPICNT = CARD_SPI_HOLD;

	REG_AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE | CARD_SPI_HOLD;
	spi_send(SPI_EEPROM_PP);
	spi_send(addr >> 8);
	spi_send(addr);
	for (int i = 0; i < kPage - 1; i++)
		spi_send(data[i]);
	REG_AUXSPICNT = CARD_ENABLE | CARD_SPI_ENABLE;
	spi_send(data[kPage - 1]);
	REG_AUXSPICNT = CARD_SPI_HOLD;

	for (int i = 0; i < 100000 && (cardEepromCommand(SPI_EEPROM_RDSR) & 1); i++)
		;
}

void EepromProbe(void)
{
	u8 rd[kPage], page[kPage], back[kPage];
	u16 exm_before = REG_EXMEMCNT;
	sysSetCardOwner(BUS_OWNER_ARM9);
	u16 exm_held = REG_EXMEMCNT;

	u8 sr0 = cardEepromCommand(SPI_EEPROM_RDSR);
	cardEepromCommand(SPI_EEPROM_WREN);
	u8 sr_wren = cardEepromCommand(SPI_EEPROM_RDSR);
	cardEepromCommand(SPI_EEPROM_WRDI);
	u8 sr_wrdi = cardEepromCommand(SPI_EEPROM_RDSR);

	cardReadEeprom(kProbeAddr, rd, kPage, 2);
	u8 rd_and = 0xFF, rd_or = 0;
	for (int i = 0; i < kPage; i++) {
		rd_and &= rd[i];
		rd_or |= rd[i];
	}

	memset(page, 0xA5, sizeof page);
	memcpy(page, "PRB1", 4);
	page[4] = sr0;
	page[5] = sr_wren;
	page[6] = sr_wrdi;
	page[7] = rd[0];
	page[8] = rd_and;
	page[9] = rd_or;
	put16(page + 10, exm_before);
	put16(page + 12, exm_held);
	put32(page + 14, g_envExtraInfo->dldi_io_type);
	put16(page + 18, g_envExtraInfo->dldi_features);
	put16(page + 20, g_envBootParam->boot_src);

	write_page(kProbeAddr, page);
	cardReadEeprom(kProbeAddr, back, kPage, 2);

	sysSetCardOwner(BUS_OWNER_ARM7);
	(void)REG_EXMEMCNT;
}
