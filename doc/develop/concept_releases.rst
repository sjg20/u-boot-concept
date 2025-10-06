.. SPDX-License-Identifier: GPL-2.0+

U-Boot Concept Releases
=======================

This document tracks all concept releases of U-Boot, including both final
releases and release candidates.

About Concept Releases
----------------------

Concept releases are automated builds of U-Boot that are:

* **Automatically generated** when changes are pushed to the master branch
* **Published as snap packages** for easy installation and testing  
* **Available in two variants**: QEMU-based and EFI-based builds
* **Distributed through Ubuntu Snap Store** on the edge channel

These releases allow developers and users to easily test the latest U-Boot
features without building from source.

Installation
~~~~~~~~~~~~

Install concept releases using snap::

    snap install u-boot-concept-qemu --edge
    snap install u-boot-concept-efi --edge

Next Release
------------

The next final release is scheduled for **2025.12**
on Monday, December 01, 2025.

Release candidate schedule:

* **2025.12-rc3**: Mon 20-Oct-2025
* **2025.12-rc2**: Mon 03-Nov-2025
* **2025.12-rc1**: Mon 17-Nov-2025
* **2025.12** (Final): Mon 01-Dec-2025

Release History
---------------

**2025.10** - Final Release
   :Date: 2025-10-06
   :Commit: e4c38498ad8c5027e80a4bbf3c20dcee7f3bc9b1
   :Subject: N/A
**2025.10-rc3** - Release Candidate
   :Date: 2025-09-22
   :Commit: 238eb7c10261a6ffdd7e5c7f28dec83409bbd79f
   :Subject: N/A
**2025.10-rc2** - Release Candidate
   :Date: 2025-09-11
   :Commit: 0447d5f5f59c7f3ab3a158ae25ad484aad24b75e
   :Subject: Version 2025.10-rc2 release

*Releases will be automatically added here by the CI system when version*
*bumps occur.*


