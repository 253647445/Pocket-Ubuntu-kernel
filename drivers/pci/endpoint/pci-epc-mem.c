/**
 * PCI Endpoint *Controller* Address Space Management
 *
 * Copyright (C) 2017 Texas Instruments
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/pci-epc.h>

/**
 * pci_epc_mem_init() - initialize the pci_epc_mem structure
 * @epc: the EPC device that invoked pci_epc_mem_init
 * @phys_base: the physical address of the base
 * @size: the size of the address space
 *
 * Invoke to initialize the pci_epc_mem structure used by the
 * endpoint functions to allocate mapped PCI address.
 */
int pci_epc_mem_init(struct pci_epc *epc, phys_addr_t phys_base, size_t size)
{
	int ret;
	struct pci_epc_mem *mem;
	unsigned long *bitmap;
	int pages = size >> PAGE_SHIFT;
	int bitmap_size = BITS_TO_LONGS(pages) * sizeof(long);

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		ret = -ENOMEM;
		goto err;
	}

	bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!bitmap) {
		ret = -ENOMEM;
		goto err_mem;
	}

	mem->bitmap = bitmap;
	mem->phys_base = phys_base;
	mem->pages = pages;
	mem->size = size;

	epc->mem = mem;

	return 0;

err_mem:
	kfree(mem);

err:
return ret;
}
EXPORT_SYMBOL_GPL(pci_epc_mem_init);

/**
 * pci_epc_mem_exit() - cleanup the pci_epc_mem structure
 * @epc: the EPC device that invoked pci_epc_mem_exit
 *
 * Invoke to cleanup the pci_epc_mem structure allocated in
 * pci_epc_mem_init().
 */
void pci_epc_mem_exit(struct pci_epc *epc)
{
	struct pci_epc_mem *mem = epc->mem;

	epc->mem = NULL;
	kfree(mem->bitmap);
	kfree(mem);
}
EXPORT_SYMBOL_GPL(pci_epc_mem_exit);

/**
 * pci_epc_mem_alloc_addr() - allocate memory address from EPC addr space
 * @epc: the EPC device on which memory has to be allocated
 * @phys_addr: populate the allocated physical address here
 * @size: the size of the address space that has to be allocated
 *
 * Invoke to allocate memory address from the EPC address space. This
 * is usually done to map the remote RC address into the local system.
 */
void __iomem *pci_epc_mem_alloc_addr(struct pci_epc *epc,
				     phys_addr_t *phys_addr, size_t size)
{
	int pageno;
	void __iomem *virt_addr;
	struct pci_epc_mem *mem = epc->mem;
	int order = get_order(size);

	pageno = bitmap_find_free_region(mem->bitmap, mem->pages, order);
	if (pageno < 0)
		return NULL;

	*phys_addr = mem->phys_base + (pageno << PAGE_SHIFT);
	virt_addr = ioremap(*phys_addr, size);
	if (!virt_addr)
		bitmap_release_region(mem->bitmap, pageno, order);

	return virt_addr;
}
EXPORT_SYMBOL_GPL(pci_epc_mem_alloc_addr);

/**
 * pci_epc_mem_free_addr() - free the allocated memory address
 * @epc: the EPC device on which memory was allocated
 * @phys_addr: the allocated physical address
 * @virt_addr: virtual address of the allocated mem space
 * @size: the size of the allocated address space
 *
 * Invoke to free the memory allocated using pci_epc_mem_alloc_addr.
 */
void pci_epc_mem_free_addr(struct pci_epc *epc, phys_addr_t phys_addr,
			   void __iomem *virt_addr, size_t size)
{
	int pageno;
	int order = get_order(size);
	struct pci_epc_mem *mem = epc->mem;

	iounmap(virt_addr);
	pageno = (phys_addr - mem->phys_base) >> PAGE_SHIFT;
	bitmap_release_region(mem->bitmap, pageno, order);
}
EXPORT_SYMBOL_GPL(pci_epc_mem_free_addr);

MODULE_DESCRIPTION("PCI EPC Address Space Management");
MODULE_AUTHOR("Kishon Vijay Abraham I <kishon@ti.com>");
MODULE_LICENSE("GPL v2");
