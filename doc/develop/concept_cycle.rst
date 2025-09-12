.. SPDX-License-Identifier: GPL-2.0+

Concept Release Mechanism
=========================

This document describes the concept release mechanism for U-Boot, which provides
automated builds and distribution through Ubuntu Snap packages.

Overview
--------

The concept release mechanism automatically triggers builds of concept packages
when changes are pushed to the master branch. This system uses two main components:

1. **GitLab CI trigger job** - Automatically runs when master is updated
2. **Launchpad snap builds** - Creates distributable packages

Architecture
------------

The release workflow consists of:

**Source Repository (GitLab)**
   - Main U-Boot development happens here
   - Contains `.gitlab-ci.yml` with `trigger_snap_builds` job
   - Triggers downstream builds when master branch is updated

**Concept Repositories (Launchpad)**
   - `u-boot-concept-qemu` - QEMU-based U-Boot builds
   - `u-boot-concept-efi` - EFI-based U-Boot builds
   - Contain snap packaging configuration
   - Build and publish to Ubuntu Snap Store

Release Schedule
----------------

Concept releases follow an automated schedule based on calendar dates:

**Final Releases**
   - **When**: First Monday of even-numbered months (February, April, June, August, October, December)
   - **Frequency**: 6 times per year
   - **Version Format**: `YYYY.MM` (e.g., `2025.02`, `2025.04`)
   - **Makefile**: VERSION and PATCHLEVEL updated, SUBLEVEL and EXTRAVERSION cleared

**Release Candidates (RC)**
   - **When**: All other scheduled pipeline runs before final releases
   - **Frequency**: Up to 3 times per release cycle
   - **Version Format**: `YYYY.MM-rcN` (e.g., `2025.02-rc1`, `2025.02-rc2`)
   - **RC Numbering**: Counts backwards from final release date
     
     - **rc1**: 2 weeks before final release
     - **rc2**: 4 weeks before final release  
     - **rc3**: 6 weeks before final release
     - **Maximum**: rc3 (never rc4 or higher)

**Trigger Mechanism**
   - Releases are triggered by **scheduled pipelines** only
   - Must target the `master` branch
   - Automatic version bumping updates the `Makefile`
   - Git tags created: standard tag (`2025.02`) + c-prefixed tag (`c2025.02`)

**Pipeline Stages**
   1. `version_bump` stage: Updates Makefile and commits version
   2. `release` stage: Creates GitLab release and tags when version commit detected

Concept Snap Build Process
---------------------------

The concept snap builds are separate from the scheduled releases and trigger
automatically when changes are pushed to the master branch:

1. **GitLab CI Pipeline**
   
   The `trigger_snap_builds` job executes in the `release` stage:
   
   * Authenticates with Launchpad using SSH key
   * Clones both concept repositories
   * Creates empty commits with trigger messages
   * Pushes commits to trigger Launchpad builds

2. **Launchpad Build Process**
   
   Each triggered repository:
   
   * Detects the new commit
   * Starts automated snap build process  
   * Pulls latest U-Boot source code
   * Builds snap packages for multiple architectures
   * Publishes successful builds to Ubuntu Snap Store

Configuration
-------------

GitLab CI Setup
~~~~~~~~~~~~~~~~

The trigger job requires these GitLab CI/CD variables:

* `LAUNCHPAD_SSH_KEY` - SSH private key for Launchpad authentication
  
  - Must be set as a **Protected** variable
  - Should contain the full SSH private key including headers
  - Format: Standard OpenSSH private key with newlines

SSH Key Configuration
~~~~~~~~~~~~~~~~~~~~~

The SSH key must be:

* Associated with a Launchpad account that has push access to concept repos
* Added to GitLab as a protected CI/CD variable
* Properly formatted with actual newlines (not literal \\n strings)

Repository Access
~~~~~~~~~~~~~~~~~

The trigger job accesses these Launchpad repositories:

* `git+ssh://sjg1@git.launchpad.net/~sjg1/u-boot/+git/u-boot-concept-qemu`
* `git+ssh://sjg1@git.launchpad.net/~sjg1/u-boot/+git/u-boot-concept-efi`

Snap Store Distribution
-----------------------

Built packages are automatically published to Ubuntu Snap Store:

* **u-boot-concept-qemu** - QEMU development builds
* **u-boot-concept-efi** - EFI development builds

Users can install these with::

    snap install u-boot-concept-qemu --edge
    snap install u-boot-concept-efi --edge

Monitoring and Troubleshooting
-------------------------------

Build Status
~~~~~~~~~~~~~

* **GitLab CI**: Check pipeline status in GitLab interface
* **Launchpad**: Monitor build progress at Launchpad project pages

Common Issues
~~~~~~~~~~~~~

**SSH Authentication Failures**
   - Verify `LAUNCHPAD_SSH_KEY` variable is set and protected
   - Check SSH key has proper format with actual newlines
   - Ensure key is associated with correct Launchpad account

**Empty Commit Failures**
   - Repository may have restrictions on empty commits
   - Check Launchpad repository access permissions
   - Verify SSH user matches repository owner

**Build Failures**
   - Check Launchpad build logs for specific errors
   - Verify snap packaging configuration is correct
   - Ensure source dependencies are available

Security Considerations
-----------------------

* SSH private keys should always be stored as protected GitLab variables
* Only protected branches (master) should trigger production builds
* Access to concept repositories should be limited to authorized maintainers
* Regular rotation of SSH keys is recommended

Release Types Summary
---------------------

The concept release mechanism involves two types of releases:

**1. Scheduled Version Releases**
   - Formal version releases with tags (e.g., `2025.02`, `2025.04-rc1`)
   - Triggered by scheduled pipelines only
   - Updates official version numbers in Makefile
   - Creates GitLab releases with tags

**2. Concept Snap Builds**
   - Development builds for testing and evaluation
   - Triggered by any push to master branch
   - Published to Ubuntu Snap Store as edge channel
   - Independent of formal version releases

Development and Testing
-----------------------

For testing the trigger mechanism:

1. Change trigger condition to test branch in `.gitlab-ci.yml`
2. Temporarily remove "Protected" flag from `LAUNCHPAD_SSH_KEY` 
3. Push to test branch with CI variable overrides to skip other stages
4. Restore production configuration after testing

Example test push::

    git push -o ci.variable=TEST_SUITES=0 -o ci.variable=WORLD_BUILD=0 \\
             -o ci.variable=TEST_PY=0 -o ci.variable=SJG_LAB=0 origin HEAD:try