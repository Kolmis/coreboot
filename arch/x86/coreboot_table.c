/*
 * table management code for coreboot tables
 *
 * Copyright (C) 2002 Eric Biederman, Linux NetworX
 * Copyright (C) 2007 coresystems GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA, 02110-1301 USA
 *
 */

#include <console.h>
#include <ip_checksum.h>
#include <string.h>
#include <device/device.h>
#include <tables.h>
#include <mc146818rtc.h>
#include <uart8250.h>
#include <lib.h>
//#include <cpu/cpu.h>
//#include <pirq_routing.h>
//#include <smp/mpspec.h>
//#include <acpi.h>
#include <build.h>

struct lb_header *lb_table_init(unsigned long addr)
{
	struct lb_header *header;

	/* 16 byte align the address */
	addr += 15;
	addr &= ~15;

	header = (void *)addr;
	header->signature[0] = 'L';
	header->signature[1] = 'B';
	header->signature[2] = 'I';
	header->signature[3] = 'O';
	header->header_bytes = sizeof(*header);
	header->header_checksum = 0;
	header->table_bytes = 0;
	header->table_checksum = 0;
	header->table_entries = 0;
	return header;
}

struct lb_record *lb_first_record(struct lb_header *header)
{
	struct lb_record *rec;
	rec = (void *)(((char *)header) + sizeof(*header));
	return rec;
}

struct lb_record *lb_last_record(struct lb_header *header)
{
	struct lb_record *rec;
	rec = (void *)(((char *)header) + sizeof(*header) + header->table_bytes);
	return rec;
}

struct lb_record *lb_next_record(struct lb_record *rec)
{
	rec = (void *)(((char *)rec) + rec->size);	
	return rec;
}

struct lb_record *lb_new_record(struct lb_header *header)
{
	struct lb_record *rec;
	rec = lb_last_record(header);
	if (header->table_entries) {
		header->table_bytes += rec->size;
	}
	rec = lb_last_record(header);
	header->table_entries++;
	rec->tag = LB_TAG_UNUSED;
	rec->size = sizeof(*rec);
	return rec;
}


struct lb_memory *lb_memory(struct lb_header *header)
{
	struct lb_record *rec;
	struct lb_memory *mem;
	rec = lb_new_record(header);
	mem = (struct lb_memory *)rec;
	mem->tag = LB_TAG_MEMORY;
	mem->size = sizeof(*mem);
	return mem;
}

struct lb_serial *lb_serial(struct lb_header *header)
{
#if defined(TTYSx_BASE)
	struct lb_record *rec;
	struct lb_serial *serial;
	rec = lb_new_record(header);
	serial = (struct lb_serial *)rec;
	serial->tag = LB_TAG_SERIAL;
	serial->size = sizeof(*serial);
	serial->ioport = TTYSx_BASE;
	serial->baud = TTYSx_BAUD;
	return serial;
#else
	return header;
#endif
}

void add_console(struct lb_header *header, u16 consoletype)
{
	struct lb_record *rec;
	struct lb_console *console;
	rec = lb_new_record(header);
	console = (struct lb_console *)lb_new_record(header);
	console->tag = LB_TAG_CONSOLE;
	console->size = sizeof(*console);
	console->type = consoletype;
}

void lb_console(struct lb_header *header)
{
#ifdef CONFIG_CONSOLE_SERIAL
	add_console(header, LB_TAG_CONSOLE_SERIAL8250);
#endif
/* FIXME when we have a proper vga console ourself */
#ifdef CONFIG_PCI_OPTION_ROM_RUN
	add_console(header, LB_TAG_CONSOLE_VGA);
#endif
}

struct lb_mainboard *lb_mainboard(struct lb_header *header)
{
	struct lb_record *rec;
	struct lb_mainboard *mainboard;
	extern const char *mainboard_vendor, *mainboard_name;
	rec = lb_new_record(header);
	mainboard = (struct lb_mainboard *)rec;
	mainboard->tag = LB_TAG_MAINBOARD;

	mainboard->size = (sizeof(*mainboard) +
		strlen(mainboard_vendor) + 1 + 
		strlen(mainboard_name) + 1 +
		3) & ~3;

	mainboard->vendor_idx = 0;
	mainboard->part_number_idx = strlen(mainboard_vendor) + 1;

	memcpy(mainboard->strings + mainboard->vendor_idx,
		mainboard_vendor,      strlen(mainboard_vendor) + 1);
	memcpy(mainboard->strings + mainboard->part_number_idx,
		mainboard_name, strlen(mainboard_name) + 1);

	return mainboard;
}

struct cmos_checksum *lb_cmos_checksum(struct lb_header *header)
{
	struct lb_record *rec;
	struct cmos_checksum *cmos_checksum;
	rec = lb_new_record(header);
	cmos_checksum = (struct cmos_checksum *)rec;
	cmos_checksum->tag = LB_TAG_OPTION_CHECKSUM;

	cmos_checksum->size = (sizeof(*cmos_checksum));

	cmos_checksum->range_start = CB_CKS_RANGE_START * 8;
	cmos_checksum->range_end = ( CB_CKS_RANGE_END * 8 ) + 7;
	cmos_checksum->location = CB_CKS_LOC * 8;

	cmos_checksum->type = CHECKSUM_PCBIOS;

	return cmos_checksum;
}

void lb_strings(struct lb_header *header)
{
	static const struct {
		u32 tag;
		const u8 *string;
	} strings[] = {
		{ LB_TAG_VERSION,        (const u8 *)COREBOOT_VERSION        },
		{ LB_TAG_EXTRA_VERSION,  (const u8 *)COREBOOT_EXTRA_VERSION  },
		{ LB_TAG_BUILD,          (const u8 *)COREBOOT_BUILD          },
		{ LB_TAG_COMPILE_TIME,   (const u8 *)COREBOOT_COMPILE_TIME   }, // duplicate?
		{ LB_TAG_COMPILE_BY,     (const u8 *)COREBOOT_COMPILE_BY     },
		{ LB_TAG_COMPILE_HOST,   (const u8 *)COREBOOT_COMPILE_HOST   },
		{ LB_TAG_COMPILE_DOMAIN, (const u8 *)COREBOOT_COMPILE_DOMAIN },
		{ LB_TAG_COMPILER,       (const u8 *)COREBOOT_COMPILER       },
		{ LB_TAG_LINKER,         (const u8 *)COREBOOT_LINKER         },
		{ LB_TAG_ASSEMBLER,      (const u8 *)COREBOOT_ASSEMBLER      },
	};
	unsigned int i;
	for(i = 0; i < ARRAY_SIZE(strings); i++) {
		struct lb_string *rec;
		size_t len;
		rec = (struct lb_string *)lb_new_record(header);
		len = strlen((char *)strings[i].string);
		rec->tag = strings[i].tag;
		rec->size = (sizeof(*rec) + len + 1 + 3) & ~3;
		memcpy(rec->string, strings[i].string, len+1);
	}

}

static void lb_memory_range(struct lb_memory *mem,
	u32 type, u64 start, u64 size)
{
	int entries;

	printk(BIOS_DEBUG, "%s: start 0x%llx size 0x%llx\n", 
			__func__, start, size);

	entries = (mem->size - sizeof(*mem))/sizeof(mem->map[0]);
	mem->map[entries].start = pack_lb64(start);
	mem->map[entries].size = pack_lb64(size);
	mem->map[entries].type = type;
	mem->size += sizeof(mem->map[0]);
}

static void lb_reserve_table_memory(struct lb_header *head)
{
	struct lb_record *last_rec;
	struct lb_memory *mem;
	u64 start;
	u64 end;
	int i, entries;

	last_rec = lb_last_record(head);
	mem = get_lb_mem();
	start = (unsigned long)head;
	end = (unsigned long)last_rec;
	entries = (mem->size - sizeof(*mem))/sizeof(mem->map[0]);
	/* Resize the right two memory areas so this table is in
	 * a reserved area of memory.  Everything has been carefully
	 * setup so that is all we need to do.
	 */
	for(i = 0; i < entries; i++ ) {
		u64 map_start = unpack_lb64(mem->map[i].start);
		u64 map_end = map_start + unpack_lb64(mem->map[i].size);
		/* Does this area need to be expanded? */
		if (map_end == start) {
			mem->map[i].size = pack_lb64(end - map_start);
		}
		/* Does this area need to be contracted? */
		else if (map_start == start) {
			mem->map[i].start = pack_lb64(end);
			mem->map[i].size = pack_lb64(map_end - end);
		}
	}
}

unsigned long lb_table_fini(struct lb_header *head)
{
	struct lb_record *rec, *first_rec;
	rec = lb_last_record(head);
	if (head->table_entries) {
		head->table_bytes += rec->size;
	}
	lb_reserve_table_memory(head);
	first_rec = lb_first_record(head);
	head->table_checksum = compute_ip_checksum(first_rec, head->table_bytes);
	head->header_checksum = 0;
	head->header_checksum = compute_ip_checksum(head, sizeof(*head));
	printk(BIOS_DEBUG,"Wrote coreboot table at: %p - %p  checksum %x\n",
		head, rec, head->table_checksum);
	return (unsigned long)rec;
}

static void lb_cleanup_memory_ranges(struct lb_memory *mem)
{
	int entries;
	int i, j;
	entries = (mem->size - sizeof(*mem))/sizeof(mem->map[0]);
	printk(BIOS_DEBUG, "%s: # entries %d\n", __func__, entries);
	for(i = 0; i < entries; i++)
		printk(BIOS_INFO, "  #%d: base 0x%08x size 0x%x\n", 
				i, mem->map[i].start.lo, mem->map[i].size.lo);
	
	/* Sort the lb memory ranges */
	for(i = 0; i < entries; i++) {
		u64 entry_start = unpack_lb64(mem->map[i].start);
		for(j = i; j < entries; j++) {
			u64 temp_start = unpack_lb64(mem->map[j].start);
			if (temp_start < entry_start) {
				struct lb_memory_range tmp;
				tmp = mem->map[i];
				mem->map[i] = mem->map[j];
				mem->map[j] = tmp;
			}
		}
	}

	/* Merge adjacent entries */
	for(i = 0; (i + 1) < entries; i++) {
		u64 start, end, nstart, nend;
		if (mem->map[i].type != mem->map[i + 1].type) {
			continue;
		}
		start  = unpack_lb64(mem->map[i].start);
		end    = start + unpack_lb64(mem->map[i].size);
		nstart = unpack_lb64(mem->map[i + 1].start);
		nend   = nstart + unpack_lb64(mem->map[i + 1].size);
		if ((start <= nstart) && (end > nstart)) {
			if (start > nstart) {
				start = nstart;
			}
			if (end < nend) {
				end = nend;
			}
			/* Record the new region size */
			mem->map[i].start = pack_lb64(start);
			mem->map[i].size  = pack_lb64(end - start);

			/* Delete the entry I have merged with */
			memmove(&mem->map[i + 1], &mem->map[i + 2], 
				((entries - i - 2) * sizeof(mem->map[0])));
			mem->size -= sizeof(mem->map[0]);
			entries -= 1;
			/* See if I can merge with the next entry as well */
			i -= 1; 
		}
	}
}

static void lb_remove_memory_range(struct lb_memory *mem, 
	u64 start, u64 size)
{
	u64 end;
	int entries;
	int i;

	end = start + size;
	entries = (mem->size - sizeof(*mem))/sizeof(mem->map[0]);

	/* Remove a reserved area from the memory map */
	for(i = 0; i < entries; i++) {
		u64 map_start = unpack_lb64(mem->map[i].start);
		u64 map_end   = map_start + unpack_lb64(mem->map[i].size);
		if ((start <= map_start) && (end >= map_end)) {
			/* Remove the completely covered range */
			memmove(&mem->map[i], &mem->map[i + 1], 
				((entries - i - 1) * sizeof(mem->map[0])));
			mem->size -= sizeof(mem->map[0]);
			entries -= 1;

			/* Since the index will disappear 
			 * revisit what will appear here 
			 */
			i -= 1; 
		}
		else if ((start > map_start) && (end < map_end)) {
			/* Split the memory range */
			memmove(&mem->map[i + 1], &mem->map[i], 
				((entries - i) * sizeof(mem->map[0])));
			mem->size += sizeof(mem->map[0]);
			entries += 1;
			/* Update the first map entry */
			mem->map[i].size = pack_lb64(start - map_start);
			/* Update the second map entry */
			mem->map[i + 1].start = pack_lb64(end);
			mem->map[i + 1].size  = pack_lb64(map_end - end);
			/* Don't bother with this map entry again */
			i += 1;
		}
		else if ((start <= map_start) && (end > map_start)) {
			/* Shrink the start of the memory range */
			mem->map[i].start = pack_lb64(end);
			mem->map[i].size  = pack_lb64(map_end - end);
		}
		else if ((start < map_end) && (start > map_start)) {
			/* Shrink the end of the memory range */
			mem->map[i].size = pack_lb64(start - map_start);
		}
	}
}

static void lb_add_memory_range(struct lb_memory *mem,
	u32 type, u64 start, u64 size)
{
	lb_remove_memory_range(mem, start, size);
	lb_memory_range(mem, type, start, size);
	lb_cleanup_memory_ranges(mem);
}

/* Routines to extract part so the coreboot table or 
 * information from the coreboot table after we have written it.
 * Currently get_lb_mem relies on a global we can change the
 * implementaiton.
 */
static struct lb_memory *mem_ranges = 0;
struct lb_memory *get_lb_mem(void)
{
	return mem_ranges;
}

static void build_lb_mem_range(void *gp, struct device *dev, struct resource *res)
{
	struct lb_memory *mem = gp;
	lb_memory_range(mem, LB_MEM_RAM, res->base, res->size);
}

static struct lb_memory *build_lb_mem(struct lb_header *head)
{
	struct lb_memory *mem;

	/* Record where the lb memory ranges will live */
	mem = lb_memory(head);
	mem_ranges = mem;

	/* Build the raw table of memory */
	search_global_resources( IORESOURCE_MEM | IORESOURCE_CACHEABLE, 
		IORESOURCE_MEM | IORESOURCE_CACHEABLE, build_lb_mem_range, mem);

	lb_cleanup_memory_ranges(mem);
	return mem;
}

/**
 * Add pointer to device tree to coreboot table.
 *
 * @param head Pointer to lbtable header.
 * @return TODO
 */
struct lb_devtree *lb_devtree(struct lb_header *head)
{
	struct lb_devtree *lbdev = NULL;

	lbdev = (struct lb_devtree *)lb_new_record(head);
	lbdev->tag = LB_TAG_DEVTREE_PTR;
	lbdev->size = sizeof(*lbdev);
	lbdev->dev_root_ptr = (u32)&dev_root; // XXX ouch.

	// FIXME we should either copy the device tree completely
	// or at least mark it reserved. 
	return lbdev;
}

unsigned long write_coreboot_table( 
	unsigned long low_table_start, unsigned long low_table_end, 
	unsigned long rom_table_start, unsigned long rom_table_end)
{
	struct lb_header *head;
	struct lb_memory *mem;
	struct lb_record *rec_dest, *rec_src;

	if(low_table_end > (0x1000 - sizeof(struct lb_header))) { /* after 4K */
		/* We need to put lbtable on  to [0xf0000,0x100000) */
		head = lb_table_init(rom_table_end);
		rom_table_end = (unsigned long)head;
	} else {
		head = lb_table_init(low_table_end);
		low_table_end = (unsigned long)head;
	}

	/* Write the option config table... */

	rec_src = (struct lb_record *)(void *)get_option_table();
	if (rec_src != NULL) {
		rec_dest = lb_new_record(head);

		memcpy(rec_dest,  rec_src, rec_src->size);
		/* Create cmos checksum entry in coreboot table */
		lb_cmos_checksum(head);
	}

	/* Record where RAM is located */
	mem = build_lb_mem(head);
	
	/* Record the mptable and the the lb_table (will be adjusted later) */
	lb_add_memory_range(mem, LB_MEM_TABLE, 
		low_table_start, low_table_end - low_table_start);

	/* Record the pirq table, acpi tables, and maybe the mptable */
	lb_add_memory_range(mem, LB_MEM_TABLE, 
		rom_table_start, rom_table_end - rom_table_start);

	/* Note:
	 * I assume that there is always memory at immediately after
	 * the low_table_end.  This means that after I setup the coreboot table.
	 * I can trivially fixup the reserved memory ranges to hold the correct
	 * size of the coreboot table.
	 */

	/* Record our motherboard */
	lb_mainboard(head);

	/* Record the serial port, if present */
	lb_serial(head);

	/* Record our console setup */
	lb_console(head);

	/* Record our various random string information */
	lb_strings(head);

	/* Record a pointer to the coreboot device tree */
	lb_devtree(head);

	/* Remember where my valid memory ranges are */
	return lb_table_fini(head);
	
}
