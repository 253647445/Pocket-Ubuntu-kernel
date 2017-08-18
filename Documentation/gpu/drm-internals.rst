=============
DRM Internals
=============

This chapter documents DRM internals relevant to driver authors and
developers working to add support for the latest features to existing
drivers.

First, we go over some typical driver initialization requirements, like
setting up command buffers, creating an initial output configuration,
and initializing core services. Subsequent sections cover core internals
in more detail, providing implementation notes and examples.

The DRM layer provides several services to graphics drivers, many of
them driven by the application interfaces it provides through libdrm,
the library that wraps most of the DRM ioctls. These include vblank
event handling, memory management, output management, framebuffer
management, command submission & fencing, suspend/resume support, and
DMA services.

Driver Initialization
=====================

At the core of every DRM driver is a :c:type:`struct drm_driver
<drm_driver>` structure. Drivers typically statically initialize
a drm_driver structure, and then pass it to
:c:func:`drm_dev_alloc()` to allocate a device instance. After the
device instance is fully initialized it can be registered (which makes
it accessible from userspace) using :c:func:`drm_dev_register()`.

The :c:type:`struct drm_driver <drm_driver>` structure
contains static information that describes the driver and features it
supports, and pointers to methods that the DRM core will call to
implement the DRM API. We will first go through the :c:type:`struct
drm_driver <drm_driver>` static information fields, and will
then describe individual operations in details as they get used in later
sections.

Driver Information
------------------

Driver Features
~~~~~~~~~~~~~~~

Drivers inform the DRM core about their requirements and supported
features by setting appropriate flags in the driver_features field.
Since those flags influence the DRM core behaviour since registration
time, most of them must be set to registering the :c:type:`struct
drm_driver <drm_driver>` instance.

u32 driver_features;

DRIVER_USE_AGP
    Driver uses AGP interface, the DRM core will manage AGP resources.

DRIVER_LEGACY
    Denote a legacy driver using shadow attach. Don't use.

DRIVER_KMS_LEGACY_CONTEXT
    Used only by nouveau for backwards compatibility with existing userspace.
    Don't use.

DRIVER_PCI_DMA
    Driver is capable of PCI DMA, mapping of PCI DMA buffers to
    userspace will be enabled. Deprecated.

DRIVER_SG
    Driver can perform scatter/gather DMA, allocation and mapping of
    scatter/gather buffers will be enabled. Deprecated.

DRIVER_HAVE_DMA
    Driver supports DMA, the userspace DMA API will be supported.
    Deprecated.

DRIVER_HAVE_IRQ; DRIVER_IRQ_SHARED
    DRIVER_HAVE_IRQ indicates whether the driver has an IRQ handler
    managed by the DRM Core. The core will support simple IRQ handler
    installation when the flag is set. The installation process is
    described in ?.

    DRIVER_IRQ_SHARED indicates whether the device & handler support
    shared IRQs (note that this is required of PCI drivers).

DRIVER_GEM
    Driver use the GEM memory manager.

DRIVER_MODESET
    Driver supports mode setting interfaces (KMS).

DRIVER_PRIME
    Driver implements DRM PRIME buffer sharing.

DRIVER_RENDER
    Driver supports dedicated render nodes.

DRIVER_ATOMIC
    Driver supports atomic properties. In this case the driver must
    implement appropriate obj->atomic_get_property() vfuncs for any
    modeset objects with driver specific properties.

Major, Minor and Patchlevel
~~~~~~~~~~~~~~~~~~~~~~~~~~~

int major; int minor; int patchlevel;
The DRM core identifies driver versions by a major, minor and patch
level triplet. The information is printed to the kernel log at
initialization time and passed to userspace through the
DRM_IOCTL_VERSION ioctl.

The major and minor numbers are also used to verify the requested driver
API version passed to DRM_IOCTL_SET_VERSION. When the driver API
changes between minor versions, applications can call
DRM_IOCTL_SET_VERSION to select a specific version of the API. If the
requested major isn't equal to the driver major, or the requested minor
is larger than the driver minor, the DRM_IOCTL_SET_VERSION call will
return an error. Otherwise the driver's set_version() method will be
called with the requested version.

Name, Description and Date
~~~~~~~~~~~~~~~~~~~~~~~~~~

char \*name; char \*desc; char \*date;
The driver name is printed to the kernel log at initialization time,
used for IRQ registration and passed to userspace through
DRM_IOCTL_VERSION.

The driver description is a purely informative string passed to
userspace through the DRM_IOCTL_VERSION ioctl and otherwise unused by
the kernel.

The driver date, formatted as YYYYMMDD, is meant to identify the date of
the latest modification to the driver. However, as most drivers fail to
update it, its value is mostly useless. The DRM core prints it to the
kernel log at initialization time and passes it to userspace through the
DRM_IOCTL_VERSION ioctl.

Device Instance and Driver Handling
-----------------------------------

.. kernel-doc:: drivers/gpu/drm/drm_drv.c
   :doc: driver instance overview

.. kernel-doc:: include/drm/drm_drv.h
   :internal:

.. kernel-doc:: drivers/gpu/drm/drm_drv.c
   :export:

Driver Load
-----------

IRQ Registration
~~~~~~~~~~~~~~~~

The DRM core tries to facilitate IRQ handler registration and
unregistration by providing :c:func:`drm_irq_install()` and
:c:func:`drm_irq_uninstall()` functions. Those functions only
support a single interrupt per device, devices that use more than one
IRQs need to be handled manually.

Managed IRQ Registration
''''''''''''''''''''''''

:c:func:`drm_irq_install()` starts by calling the irq_preinstall
driver operation. The operation is optional and must make sure that the
interrupt will not get fired by clearing all pending interrupt flags or
disabling the interrupt.

The passed-in IRQ will then be requested by a call to
:c:func:`request_irq()`. If the DRIVER_IRQ_SHARED driver feature
flag is set, a shared (IRQF_SHARED) IRQ handler will be requested.

The IRQ handler function must be provided as the mandatory irq_handler
driver operation. It will get passed directly to
:c:func:`request_irq()` and thus has the same prototype as all IRQ
handlers. It will get called with a pointer to the DRM device as the
second argument.

Finally the function calls the optional irq_postinstall driver
operation. The operation usually enables interrupts (excluding the
vblank interrupt, which is enabled separately), but drivers may choose
to enable/disable interrupts at a different time.

:c:func:`drm_irq_uninstall()` is similarly used to uninstall an
IRQ handler. It starts by waking up all processes waiting on a vblank
interrupt to make sure they don't hang, and then calls the optional
irq_uninstall driver operation. The operation must disable all hardware
interrupts. Finally the function frees the IRQ by calling
:c:func:`free_irq()`.

Manual IRQ Registration
'''''''''''''''''''''''

Drivers that require multiple interrupt handlers can't use the managed
IRQ registration functions. In that case IRQs must be registered and
unregistered manually (usually with the :c:func:`request_irq()` and
:c:func:`free_irq()` functions, or their :c:func:`devm_request_irq()` and
:c:func:`devm_free_irq()` equivalents).

When manually registering IRQs, drivers must not set the
DRIVER_HAVE_IRQ driver feature flag, and must not provide the
irq_handler driver operation. They must set the :c:type:`struct
drm_device <drm_device>` irq_enabled field to 1 upon
registration of the IRQs, and clear it to 0 after unregistering the
IRQs.

Memory Manager Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Every DRM driver requires a memory manager which must be initialized at
load time. DRM currently contains two memory managers, the Translation
Table Manager (TTM) and the Graphics Execution Manager (GEM). This
document describes the use of the GEM memory manager only. See ? for
details.

Miscellaneous Device Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Another task that may be necessary for PCI devices during configuration
is mapping the video BIOS. On many devices, the VBIOS describes device
configuration, LCD panel timings (if any), and contains flags indicating
device state. Mapping the BIOS can be done using the pci_map_rom()
call, a convenience function that takes care of mapping the actual ROM,
whether it has been shadowed into memory (typically at address 0xc0000)
or exists on the PCI device in the ROM BAR. Note that after the ROM has
been mapped and any necessary information has been extracted, it should
be unmapped; on many devices, the ROM address decoder is shared with
other BARs, so leaving it mapped could cause undesired behaviour like
hangs or memory corruption.

Bus-specific Device Registration and PCI Support
------------------------------------------------

A number of functions are provided to help with device registration. The
functions deal with PCI and platform devices respectively and are only
provided for historical reasons. These are all deprecated and shouldn't
be used in new drivers. Besides that there's a few helpers for pci
drivers.

.. kernel-doc:: drivers/gpu/drm/drm_pci.c
   :export:

Open/Close, File Operations and IOCTLs
======================================

File Operations
---------------

.. kernel-doc:: drivers/gpu/drm/drm_file.c
   :doc: file operations

.. kernel-doc:: include/drm/drm_file.h
   :internal:

.. kernel-doc:: drivers/gpu/drm/drm_file.c
   :export:

Misc Utilities
==============

Printer
-------

.. kernel-doc:: include/drm/drm_print.h
   :doc: print

.. kernel-doc:: include/drm/drm_print.h
   :internal:

.. kernel-doc:: drivers/gpu/drm/drm_print.c
   :export:


Legacy Support Code
===================

The section very briefly covers some of the old legacy support code
which is only used by old DRM drivers which have done a so-called
shadow-attach to the underlying device instead of registering as a real
driver. This also includes some of the old generic buffer management and
command submission code. Do not use any of this in new and modern
drivers.

Legacy Suspend/Resume
---------------------

The DRM core provides some suspend/resume code, but drivers wanting full
suspend/resume support should provide save() and restore() functions.
These are called at suspend, hibernate, or resume time, and should
perform any state save or restore required by your device across suspend
or hibernate states.

int (\*suspend) (struct drm_device \*, pm_message_t state); int
(\*resume) (struct drm_device \*);
Those are legacy suspend and resume methods which *only* work with the
legacy shadow-attach driver registration functions. New driver should
use the power management interface provided by their bus type (usually
through the :c:type:`struct device_driver <device_driver>`
dev_pm_ops) and set these methods to NULL.

Legacy DMA Services
-------------------

This should cover how DMA mapping etc. is supported by the core. These
functions are deprecated and should not be used.
